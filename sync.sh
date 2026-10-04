#!/bin/bash
# Rebuilds this snapshot from the live source directories.
#
# Run this, review `git status` / `git diff`, then commit and push. The only
# thing ever excluded is each project's own data/ folder -- every personal
# or generated thing (databases, generated resumes/cover letters, one-time
# personal scripts) lives there and nowhere else, so this never needs a
# growing list of exclude flags remembered by hand. That's the actual fix:
# before this, the exclude list was retyped from memory each sync and
# missed a file twice (a personal seed script, a generated-output file).
set -euo pipefail
cd "$(dirname "$0")"

SRC_PLATFORM=/Users/sourav/Documents/cpp/platform
SRC_JOBSEARCH=/Users/sourav/Documents/jobsearch

rsync -a --delete \
  --exclude='__pycache__/' --exclude='*.pyc' --exclude='.venv/' --exclude='.DS_Store' \
  --exclude='data/' \
  "$SRC_PLATFORM"/ ./platform/

rsync -a --delete \
  --exclude='__pycache__/' --exclude='*.pyc' --exclude='.venv/' --exclude='.DS_Store' \
  --exclude='data/' \
  "$SRC_JOBSEARCH"/ ./jobsearch/

echo "Synced from:"
echo "  $SRC_PLATFORM"
echo "  $SRC_JOBSEARCH"
echo
echo "Review with: git status"
