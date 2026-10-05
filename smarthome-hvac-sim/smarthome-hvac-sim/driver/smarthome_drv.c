// SPDX-License-Identifier: GPL-2.0
/*
 * smarthome_drv - virtual PIR + temperature sensor and HVAC actuator.
 *
 * A kernel timer advances a simple thermal model every tick_ms and pushes an
 * sh_event into a ring buffer. User space reads events (blocking / poll),
 * and drives the HVAC actuator and simulation knobs through ioctl().
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include "smarthome_ioctl.h"

#define DRV_NAME "smarthome"
#define RING_SZ  64            /* power of two */

static unsigned int tick_ms = 500;
module_param(tick_ms, uint, 0444);
MODULE_PARM_DESC(tick_ms, "Simulation tick in milliseconds (default 500)");

struct sh_dev {
	dev_t devno;
	struct cdev cdev;
	struct class *cls;
	struct timer_list timer;

	spinlock_t lock;                 /* protects everything below */
	wait_queue_head_t wq;
	struct sh_event ring[RING_SZ];
	unsigned int head, tail;         /* free-running counters */
	s32 temp_mc, outdoor_mc, target_mc;
	u8 hvac;
	bool occupied;
	bool manual_occ;                 /* random occupancy disabled */
	bool stopping;
	u32 dropped;
};

static struct sh_dev sh;

static inline bool ring_empty(void)
{
	return READ_ONCE(sh.head) == READ_ONCE(sh.tail);
}

/* Timer callback (softirq context): thermal model + sensor sample. */
static void sh_timer_fn(struct timer_list *t)
{
	struct sh_dev *d = from_timer(d, t, timer);
	struct sh_event ev;
	unsigned long flags;
	s32 delta;
	bool motion;

	spin_lock_irqsave(&d->lock, flags);

	if (!d->manual_occ) {
		u32 r = get_random_u32() % 100;

		if (d->occupied && r < 5)
			d->occupied = false;
		else if (!d->occupied && r < 4)
			d->occupied = true;
	}
	motion = d->occupied && (get_random_u32() % 100) < 70;

	/* Heat leak toward outdoor (2%/tick) +/- actuator, +20 mC body heat. */
	delta = (d->outdoor_mc - d->temp_mc) / 50;
	if (d->hvac == SH_HVAC_HEAT)
		delta += 300;
	else if (d->hvac == SH_HVAC_COOL)
		delta -= 300;
	if (d->occupied)
		delta += 20;
	d->temp_mc += delta;

	ev.ts_ns = ktime_get_ns();
	ev.temp_mc = d->temp_mc;
	ev.motion = motion;
	ev.hvac = d->hvac;
	ev.reserved = 0;

	if (d->head - d->tail == RING_SZ) {   /* full: drop oldest */
		d->tail++;
		d->dropped++;
	}
	d->ring[d->head % RING_SZ] = ev;
	d->head++;

	spin_unlock_irqrestore(&d->lock, flags);
	wake_up_interruptible(&d->wq);

	if (!READ_ONCE(d->stopping))
		mod_timer(&d->timer, jiffies + msecs_to_jiffies(tick_ms));
}

static int sh_open(struct inode *inode, struct file *f)
{
	return 0;
}

static ssize_t sh_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
	struct sh_event ev;
	unsigned long flags;
	int ret;

	if (len < sizeof(ev))
		return -EINVAL;

	for (;;) {
		spin_lock_irqsave(&sh.lock, flags);
		if (sh.head != sh.tail) {
			ev = sh.ring[sh.tail % RING_SZ];
			sh.tail++;
			spin_unlock_irqrestore(&sh.lock, flags);
			break;
		}
		spin_unlock_irqrestore(&sh.lock, flags);

		if (f->f_flags & O_NONBLOCK)
			return -EAGAIN;
		ret = wait_event_interruptible(sh.wq, !ring_empty());
		if (ret)
			return ret;      /* -ERESTARTSYS on signal */
	}

	if (copy_to_user(buf, &ev, sizeof(ev)))
		return -EFAULT;
	return sizeof(ev);
}

static __poll_t sh_poll(struct file *f, poll_table *wait)
{
	poll_wait(f, &sh.wq, wait);
	return ring_empty() ? 0 : (EPOLLIN | EPOLLRDNORM);
}

static long sh_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	unsigned long flags;
	int v;

	switch (cmd) {
	case SH_IOC_SET_HVAC:
		if (get_user(v, (int __user *)arg))
			return -EFAULT;
		if (v < SH_HVAC_OFF || v > SH_HVAC_COOL)
			return -EINVAL;
		spin_lock_irqsave(&sh.lock, flags);
		sh.hvac = v;
		spin_unlock_irqrestore(&sh.lock, flags);
		return 0;
	case SH_IOC_SET_TARGET:
		if (get_user(v, (int __user *)arg))
			return -EFAULT;
		if (v < 10000 || v > 35000)
			return -EINVAL;
		spin_lock_irqsave(&sh.lock, flags);
		sh.target_mc = v;
		spin_unlock_irqrestore(&sh.lock, flags);
		return 0;
	case SH_IOC_SET_OUTDOOR:
		if (get_user(v, (int __user *)arg))
			return -EFAULT;
		if (v < -30000 || v > 50000)
			return -EINVAL;
		spin_lock_irqsave(&sh.lock, flags);
		sh.outdoor_mc = v;
		spin_unlock_irqrestore(&sh.lock, flags);
		return 0;
	case SH_IOC_SET_OCCUPIED:
		if (get_user(v, (int __user *)arg))
			return -EFAULT;
		if (v < 0 || v > SH_OCC_AUTO)
			return -EINVAL;
		spin_lock_irqsave(&sh.lock, flags);
		sh.manual_occ = (v != SH_OCC_AUTO);
		if (sh.manual_occ)
			sh.occupied = v;
		spin_unlock_irqrestore(&sh.lock, flags);
		return 0;
	case SH_IOC_GET_STATUS: {
		struct sh_status st = { 0 };

		spin_lock_irqsave(&sh.lock, flags);
		st.temp_mc = sh.temp_mc;
		st.outdoor_mc = sh.outdoor_mc;
		st.target_mc = sh.target_mc;
		st.hvac = sh.hvac;
		st.occupied = sh.occupied;
		st.dropped = sh.dropped;
		spin_unlock_irqrestore(&sh.lock, flags);
		return copy_to_user((void __user *)arg, &st, sizeof(st)) ? -EFAULT : 0;
	}
	default:
		return -ENOTTY;
	}
}

static const struct file_operations sh_fops = {
	.owner          = THIS_MODULE,
	.open           = sh_open,
	.read           = sh_read,
	.poll           = sh_poll,
	.unlocked_ioctl = sh_ioctl,
	.llseek         = no_llseek,
};

static int __init sh_init(void)
{
	struct device *dev;
	int ret;

	if (!tick_ms)
		return -EINVAL;

	spin_lock_init(&sh.lock);
	init_waitqueue_head(&sh.wq);
	sh.temp_mc = 24000;
	sh.outdoor_mc = 32000;
	sh.target_mc = 24000;
	sh.hvac = SH_HVAC_OFF;

	ret = alloc_chrdev_region(&sh.devno, 0, 1, DRV_NAME);
	if (ret)
		return ret;

	cdev_init(&sh.cdev, &sh_fops);
	sh.cdev.owner = THIS_MODULE;
	ret = cdev_add(&sh.cdev, sh.devno, 1);
	if (ret)
		goto err_region;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	sh.cls = class_create(DRV_NAME);
#else
	sh.cls = class_create(THIS_MODULE, DRV_NAME);
#endif
	if (IS_ERR(sh.cls)) {
		ret = PTR_ERR(sh.cls);
		goto err_cdev;
	}

	dev = device_create(sh.cls, NULL, sh.devno, NULL, DRV_NAME);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		goto err_class;
	}

	timer_setup(&sh.timer, sh_timer_fn, 0);
	mod_timer(&sh.timer, jiffies + msecs_to_jiffies(tick_ms));
	pr_info(DRV_NAME ": loaded, major=%d tick=%ums\n", MAJOR(sh.devno), tick_ms);
	return 0;

err_class:
	class_destroy(sh.cls);
err_cdev:
	cdev_del(&sh.cdev);
err_region:
	unregister_chrdev_region(sh.devno, 1);
	return ret;
}

static void __exit sh_exit(void)
{
	WRITE_ONCE(sh.stopping, true);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 15, 0)
	timer_delete_sync(&sh.timer);
#else
	del_timer_sync(&sh.timer);
#endif
	device_destroy(sh.cls, sh.devno);
	class_destroy(sh.cls);
	cdev_del(&sh.cdev);
	unregister_chrdev_region(sh.devno, 1);
	pr_info(DRV_NAME ": unloaded\n");
}

module_init(sh_init);
module_exit(sh_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Smart home occupancy sensor + HVAC simulator");
