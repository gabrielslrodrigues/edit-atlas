# FFmpeg LGPL compliance policy

Edit Atlas uses FFmpeg under the GNU Lesser General Public License version 2.1
or later. This document records the project's technical policy; it is not legal
advice and does not resolve codec patent licensing.

## Build and linkage policy

All supported desktop targets dynamically link only the FFmpeg libraries
needed for video inspection and frame decoding: libavcodec, libavformat,
libavutil, and libswscale. The pinned vcpkg dependency disables default
features and does not select `gpl`, `nonfree`, `x264`, or `x265`.

The initial application codec policy accepts FFmpeg's native H.264, HEVC,
ProRes, DNxHD/DNxHR, MPEG-2 Video, and Motion JPEG decoders in MOV, MP4, and
MXF containers. The application backend exposes no encoding operation, and no
external encoder library is included. Codec patent rights are independent from
FFmpeg's copyright license and require separate review before commercial
distribution.

## Binary distribution checklist

Every binary release must:

1. Include `THIRD_PARTY_NOTICES.md` and the `ffmpeg` copyright file installed
   from the resolved vcpkg package.
2. Publish the exact FFmpeg corresponding source and vcpkg patch set as an
   Edit Atlas-controlled release asset.
3. Include `FFMPEG_SOURCE_OFFER.md` with rebuilding and replacement
   instructions.
4. Keep the FFmpeg shared libraries replaceable by interface-compatible
   modified builds.
5. Verify that the packaged runtime contains all four required shared
   libraries and that the linked configuration does not enable GPL, nonfree,
   x264, or x265 features.

CI enforces the runtime-library and notice checks on every package. Integration
tests inspect FFmpeg's linked license and configure strings. Tagged release
jobs verify and publish the corresponding-source archive before publishing
the draft release.

## References

- [FFmpeg legal information](https://ffmpeg.org/legal.html)
- [FFmpeg license checklist](https://ffmpeg.org/legal.html#License-Compliance-Checklist)
- [FFmpeg general documentation](https://ffmpeg.org/general.html)
