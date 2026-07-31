#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <site-directory>" >&2
  exit 2
fi

site_directory=$1
worktree_directory=$(mktemp -d)
cleanup() {
  git worktree remove --force "$worktree_directory" 2>/dev/null || true
  rm -rf "$worktree_directory"
}
trap cleanup EXIT

if git fetch origin gh-pages; then
  git worktree add "$worktree_directory" origin/gh-pages
  git -C "$worktree_directory" switch --force-create gh-pages
else
  git worktree add --detach "$worktree_directory" HEAD
  git -C "$worktree_directory" switch --orphan gh-pages
  git -C "$worktree_directory" rm --force --recursive . 2>/dev/null || true
fi

find "$worktree_directory" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
cp -a "$site_directory"/. "$worktree_directory"/

git -C "$worktree_directory" add --all
if git -C "$worktree_directory" diff --cached --quiet; then
  echo "GitHub Pages source is unchanged."
  exit 0
fi

git -C "$worktree_directory" \
  -c user.name="github-actions[bot]" \
  -c user.email="41898282+github-actions[bot]@users.noreply.github.com" \
  commit -m "docs: publish API documentation"
git -C "$worktree_directory" push origin gh-pages
