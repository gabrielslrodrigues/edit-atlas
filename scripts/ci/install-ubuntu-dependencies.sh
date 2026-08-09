#!/usr/bin/env bash

set -euo pipefail

sudo apt-get update
sudo apt-get install --yes --no-install-recommends \
  autoconf \
  autoconf-archive \
  automake \
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
  pkg-config \
  rpm \
  wayland-protocols
