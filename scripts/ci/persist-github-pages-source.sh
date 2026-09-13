#!/usr/bin/env bash

set -euo pipefail

# Publish assembled documentation through the gh-pages branch.

# Remove the temporary publishing worktree even if publication fails.
# Globals: worktree_directory (local to the active main invocation).
cleanup() {
  git worktree remove --force "$worktree_directory" 2>/dev/null || true
  rm -rf "$worktree_directory"
}

# Replace the published site with the assembled documentation.
# Arguments: site directory.
# Outputs: Git progress and publication status; failures to stderr.
# Returns: 2 for invalid arguments, otherwise the Git or filesystem status.
main() {
  local site_directory
  local worktree_directory

  if (( $# != 1 )); then
    echo "Usage: $0 <site-directory>" >&2
    exit 2
  fi

  site_directory=$1
  worktree_directory=$(mktemp -d)
  trap cleanup EXIT

  if git fetch origin gh-pages; then
    git worktree add "$worktree_directory" origin/gh-pages
    git -C "$worktree_directory" switch --force-create gh-pages
  else
    git worktree add --detach "$worktree_directory" HEAD
    git -C "$worktree_directory" switch --orphan gh-pages
    git -C "$worktree_directory" rm --force --recursive . 2>/dev/null || true
  fi

  find "$worktree_directory" -mindepth 1 -maxdepth 1 \
    ! -name .git -exec rm -rf {} +
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

  # Exit before main unwinds the local state used by EXIT callbacks.
  exit "$?"
}

main "$@"
