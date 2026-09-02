#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: $0 [--build|--e2e]" >&2
}

if (( $# > 1 )); then
  usage
  exit 2
fi

dependency_profile="${1:---build}"
case "${dependency_profile}" in
  --build)
    packages=(
      autoconf
      autoconf-archive
      automake
      bison
      clang
      dpkg-dev
      libc6-dev
      libegl1-mesa-dev
      libglu1-mesa-dev
      libltdl-dev
      libtool
      libx11-dev
      libx11-xcb-dev
      libxext-dev
      libxfixes-dev
      libxi-dev
      libxkbcommon-dev
      libxkbcommon-x11-dev
      libxrender-dev
      libxtst-dev
      libxcb-cursor-dev
      libxcb-glx0-dev
      libxcb-icccm4-dev
      libxcb-image0-dev
      libxcb-keysyms1-dev
      libxcb-randr0-dev
      libxcb-render-util0-dev
      libxcb-render0-dev
      libxcb-shape0-dev
      libxcb-shm0-dev
      libxcb-sync-dev
      libxcb-util-dev
      libxcb-xfixes0-dev
      libxcb-xinerama0-dev
      libxcb-xinput-dev
      libxcb-xkb-dev
      libxcb1-dev
      libwayland-dev
      mono-complete
      nasm
      perl
      pkg-config
      rpm
      wayland-protocols
    )
    ;;
  --e2e)
    packages=(
      at-spi2-core
      build-essential
      dbus-x11
      gir1.2-atspi-2.0
      gir1.2-gtk-3.0
      libcairo2-dev
      libgirepository1.0-dev
      # Loaded by Qt's xcb platform plugin, which the packaged application
      # requires for a GUI session. A hosted CI runner image provides these
      # incidentally; an environment that installs only what this profile
      # declares must be told about them.
      libxcb-cursor0
      libxcb-icccm4
      libxcb-keysyms1
      libxcb-shape0
      libxcb-xinput0
      libxcb-xkb1
      libxkbcommon-x11-0
      locales
      pkg-config
      python3-dev
      util-linux
      x11-apps
      xauth
      xvfb
    )
    ;;
  *)
    usage
    exit 2
    ;;
esac

if (( EUID == 0 )); then
  privilege=()
elif command -v sudo >/dev/null 2>&1; then
  privilege=(sudo)
else
  echo "Root privileges or sudo are required to install dependencies" >&2
  exit 1
fi

# Prevent apt/dpkg from blocking on an interactive prompt (e.g. needrestart's
# "which services should be restarted?" dialog) when no TTY is attached, and
# bound retries against a flaky mirror instead of hanging indefinitely.
export DEBIAN_FRONTEND=noninteractive
export NEEDRESTART_MODE=a

# Skip doc/man/locale unpacking: this is an ephemeral CI runner, not an
# installed system, so nothing reads them. Keep license text for compliance.
"${privilege[@]}" tee /etc/dpkg/dpkg.cfg.d/01_nodoc >/dev/null <<'EOF'
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

"${privilege[@]}" env \
  DEBIAN_FRONTEND="${DEBIAN_FRONTEND}" \
  NEEDRESTART_MODE="${NEEDRESTART_MODE}" \
  apt-get "${apt_options[@]}" update
"${privilege[@]}" env \
  DEBIAN_FRONTEND="${DEBIAN_FRONTEND}" \
  NEEDRESTART_MODE="${NEEDRESTART_MODE}" \
  apt-get "${apt_options[@]}" \
  -o Dpkg::Options::="--force-unsafe-io" \
  install --yes --no-install-recommends "${packages[@]}"

if [[ "${dependency_profile}" == "--e2e" ]]; then
  "${privilege[@]}" locale-gen pt_BR.UTF-8
fi
