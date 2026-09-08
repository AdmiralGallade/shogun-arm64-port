# Credits

## Original game

**Shogun: Rise Of The Renegade** — © 2012 **int13**. All rights reserved.

Everything that makes the game a game — the engine, the artwork, the music,
the level design, the asset pack — is theirs. int13 is no longer trading and
the game was delisted; this project exists so a copy you already own does not
become unplayable, and it distributes none of their material.

## 64-bit port

**AdmiralGallade** — ARM32-on-arm64 runtime, GLES 1.x bridge, JNI bridge,
audio path, Android shell, build tooling.

## Third-party components

| Project | Role | License |
|---|---|---|
| [Unicorn Engine](https://github.com/unicorn-engine/unicorn) | ARM32 CPU emulation (QEMU TCG) | GPL-2.0 |
| [Capstone](https://github.com/capstone-engine/capstone) | Disassembly, used by the analysis tools | BSD-3-Clause |
| [Keystone](https://github.com/keystone-engine/keystone) | Assembly, used by the APK patcher | GPL-2.0 |

Unicorn is GPL-2.0. It is linked as a shared library and is not redistributed
here; you build it yourself as part of the setup.
