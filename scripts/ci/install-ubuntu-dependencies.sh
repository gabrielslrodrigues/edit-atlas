#!/usr/bin/env bash

set -euo pipefail

# Prevent apt/dpkg from blocking on an interactive prompt (e.g. needrestart's
# "which services should be restarted?" dialog) when no TTY is attached, and
# bound retries against a flaky mirror instead of hanging indefinitely.
export DEBIAN_FRONTEND=noninteractive
export NEEDRESTART_MODE=a

# Skip doc/man/locale unpacking: this is an ephemeral CI runner, not an
# installed system, so nothing reads them. Keep license text for compliance.
sudo tee /etc/dpkg/dpkg.cfg.d/01_nodoc >/dev/null <<'EOF'
path-exclude=/usr/share/doc/*
path-include=/usr/share/doc/*/copyright
path-exclude=/usr/share/man/*
path-exclude=/usr/share/groff/*
path-exclude=/usr/share/info/*
path-exclude=/usr/share/lintian/*
path-exclude=/usr/share/linda/*
EOF

apt_options=(
  -o Acquire::Retries=3
  -o Acquire::http::Timeout=30
  -o Acquire::https::Timeout=30
)

sudo --preserve-env=DEBIAN_FRONTEND,NEEDRESTART_MODE \
  apt-get "${apt_options[@]}" update
sudo --preserve-env=DEBIAN_FRONTEND,NEEDRESTART_MODE \
  apt-get "${apt_options[@]}" \
  -o Dpkg::Options::="--force-unsafe-io" \
  install --yes --no-install-recommends \
  autoconf \
  autoconf-archive \
  automake \
  bison \
  dpkg-dev \
  libc6-dev \
  libegl1-mesa-dev \
  libglu1-mesa-dev \
  libltdl-dev \
  libtool \
  libx11-dev \
  libx11-xcb-dev \
  libxext-dev \
  libxfixes-dev \
  libxi-dev \
  libxkbcommon-dev \
  libxkbcommon-x11-dev \
  libxrender-dev \
  libxtst-dev \
  libxcb-cursor-dev \
  libxcb-glx0-dev \
  libxcb-icccm4-dev \
  libxcb-image0-dev \
  libxcb-keysyms1-dev \
  libxcb-randr0-dev \
  libxcb-render-util0-dev \
  libxcb-render0-dev \
  libxcb-shape0-dev \
  libxcb-shm0-dev \
  libxcb-sync-dev \
  libxcb-util-dev \
  libxcb-xfixes0-dev \
  libxcb-xinerama0-dev \
  libxcb-xinput-dev \
  libxcb-xkb-dev \
  libxcb1-dev \
  libwayland-dev \
  mono-complete \
  nasm \
  perl \
  pkg-config \
  rpm \
  wayland-protocols
