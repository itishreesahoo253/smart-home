#!/usr/bin/env bash
# Stage 3: initialise repo + branching strategy (GitHub-flow with stage tags).
set -e
git init -b main
git add . && git commit -m "chore: initial project skeleton (stage 1-3 docs, driver, daemon)"
git tag -a stage-3 -m "Stage 3: design & architecture complete"
for b in develop feature/driver feature/controller feature/logger docs/report; do git branch "$b"; done
git checkout develop
echo "Branches: main (stable, tagged per stage) <- develop <- feature/* , docs/*"
echo "Next: git remote add origin <your-github-url> && git push -u origin --all --tags"
