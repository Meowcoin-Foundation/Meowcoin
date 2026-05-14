23.2 Release Notes
==================

Meowcoin Core version 23.2 is now available from:

  <https://meowcoincore.org/bin/meowcoin-core-23.2/>

This release includes various bug fixes and performance
improvements, as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/meowcoin/meowcoin/issues>

To receive security and update notifications, please subscribe to:

  <https://meowcoincore.org/en/list/announcements/join/>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes in some cases), then run the
installer (on Windows) or just copy over `/Applications/Meowcoin-Qt` (on macOS)
or `meowcoind`/`meowcoin-qt` (on Linux).

Upgrading directly from a version of Meowcoin Core that has reached its EOL is
possible, but it might take some time if the data directory needs to be migrated. Old
wallet versions of Meowcoin Core are generally supported.

Compatibility
==============

Meowcoin Core is supported and extensively tested on operating systems
using the Linux kernel, macOS 10.15+, and Windows 7 and newer.  Meowcoin
Core should also work on most other Unix-like systems but is not as
frequently tested on them.  It is not recommended to use Meowcoin Core on
unsupported systems.

### P2P

- #26909 net: prevent peers.dat corruptions by only serializing once
- #27608 p2p: Avoid prematurely clearing download state for other peers
- #27610 Improve performance of p2p inv to send queues

### Build system

- #25436 build: suppress array-bounds errors in libxkbcommon
- #25763 bdb: disable Werror for format-security
- #26944 depends: fix systemtap download URL
- #27462 depends: fix compiling bdb with clang-16 on aarch64

### Miscellaneous

- #25444 ci: macOS task imrovements
- #26388 ci: Use macos-ventura-xcode:14.1 image for "macOS native" task
- #26924 refactor: Add missing includes to fix gcc-13 compile error

Credits
=======

Thanks to everyone who directly contributed to this release:

- Anthony Towns
- Hennadii Stepanov
- MacroFake
- Martin Zumsande
- Michael Ford
- Suhas Daftuar

As well as to everyone that helped with translations on
[Transifex](https://www.transifex.com/meowcoin/meowcoin/).