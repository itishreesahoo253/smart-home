/* Shared kernel/user ABI for the Smart Home simulator driver. */
#ifndef SMARTHOME_IOCTL_H
#define SMARTHOME_IOCTL_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define SH_HVAC_OFF  0
#define SH_HVAC_HEAT 1
#define SH_HVAC_COOL 2

#define SH_OCC_AUTO  2   /* SET_OCCUPIED arg: 0=vacant, 1=occupied, 2=random */

/* One sample produced every tick; returned by read(). 16 bytes. */
struct sh_event {
	__u64 ts_ns;      /* ktime_get_ns() */
	__s32 temp_mc;    /* room temperature, milli-degrees C */
	__u8  motion;     /* PIR sensor: 1 = motion detected */
	__u8  hvac;       /* current actuator state */
	__u16 reserved;
};

struct sh_status {
	__s32 temp_mc;
	__s32 outdoor_mc;
	__s32 target_mc;
	__u8  hvac;
	__u8  occupied;
	__u16 reserved;
	__u32 dropped;    /* events overwritten because reader was slow */
};

#define SH_IOC_MAGIC       'S'
#define SH_IOC_SET_HVAC    _IOW(SH_IOC_MAGIC, 1, int)
#define SH_IOC_SET_TARGET  _IOW(SH_IOC_MAGIC, 2, int)  /* milli-C */
#define SH_IOC_SET_OUTDOOR _IOW(SH_IOC_MAGIC, 3, int)  /* milli-C */
#define SH_IOC_SET_OCCUPIED _IOW(SH_IOC_MAGIC, 4, int)
#define SH_IOC_GET_STATUS  _IOR(SH_IOC_MAGIC, 5, struct sh_status)

#endif
