[![Join the chat at https://gitter.im/Wargus](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/Wargus?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

[![Discord](https://img.shields.io/discord/780082494447288340?style=flat-square&logo=discord&label=discord)](https://discord.gg/dQGxaw3QfB)

### Nightly builds are available:

- Windows Installer: https://github.com/Wargus/stargus/releases/tag/master-builds
- Ubuntu/Debian Packages: https://code.launchpad.net/~timfelgentreff/+archive/ubuntu/stratagus
- OS X App Bundle: https://github.com/Wargus/stratagus/wiki/osx/Stargus.app.tar.gz

## Installation Instructions

Download the installer for your platform. Upon first launch of Stargus, it will ask you for
your Starcraft installation to extract the data to work with Stargus.

## Build Instructions

Stratagus is pinned as a submodule, so the game and engine are built together
from matching revisions.

### Prerequisites (macOS)

```
brew install pkg-config stormlib sdl2 libpng zlib
```

You also need a C++17 compiler and GNU Make. Xcode Command Line Tools provide
both on macOS.

On Linux, install the equivalent development packages for StormLib, SDL2,
libpng, and zlib, together with a C++17 compiler, Make, and pkg-config.

### Build Stargus and Stratagus

```
git clone --recurse-submodules https://github.com/corepunch/stargus.git
cd stargus
make
```

For an existing checkout, initialize or refresh all nested submodules before
building:

```
git submodule update --init --recursive
make
```

The build produces:

- `startool` — StarCraft data extractor
- `stargus` — game launcher
- `build/bin/stratagus` — Stratagus engine

Lua 5.1, tolua++, and Guisan are built from the pinned Stratagus third-party
sources. Audio is handled directly through SDL2 and supports Stargus's WAV
assets.

Individual build targets are available when you do not need the complete game:

```
make third-party  # initialize nested engine dependencies
make lua          # build Lua 5.1
make tolua++      # build the generator and runtime
make engine       # build build/bin/stratagus
```

Run `./stargus` after extracting or selecting a StarCraft installation through
the launcher. Build variables you may override:

- `PREFIX` - install prefix used for runtime data/scripts paths (default `/usr/local`)
- `CC`, `CFLAGS`, `CXX`, `CXXFLAGS`, `AR`, `PKG_CONFIG`, `STORM_PREFIX`

![image](https://cloud.githubusercontent.com/assets/46235/11292960/499a7d3c-8f55-11e5-9356-62c190c57467.png)
![image](https://cloud.githubusercontent.com/assets/46235/11292993/9198675c-8f55-11e5-9f74-2f23fb207498.png)
![image](https://cloud.githubusercontent.com/assets/46235/11293018/cef6e970-8f55-11e5-8625-8bd13082b041.png)
