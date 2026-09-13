#!/usr/bin/env bash

set -euo pipefail

# Install the macOS build and packaging dependencies used by CI.

if (( $# != 0 )); then
  echo "Usage: $0" >&2
  exit 2
fi

brew install \
  autoconf \
  autoconf-archive \
  automake \
  libtool \
  mono \
  nasm \
  pkg-config
