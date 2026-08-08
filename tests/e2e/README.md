# End-to-end tests

This directory is reserved for black-box pytest scenarios that exercise
installed Edit Atlas packages. The shared harness and packaged CLI coverage are
tracked by issue #88.

E2E tests are not registered with CTest. They must use the public GUI or command
line, keep user state isolated, use bounded polling instead of fixed sleeps, and
provide reproducible entry points that place generated environments and output
under `build/`.
