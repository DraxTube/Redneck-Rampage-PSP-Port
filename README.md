# Redneck Rampage PSP Port

A PSP homebrew port of **Redneck Rampage** (1997), built on a custom implementation of Ken Silverman's Build engine adapted for the PlayStation Portable.

## Features

- Full Build engine sector-based renderer (320×200 upscaled to 480×272)
- GRP archive, ART tile, and MAP file format support
- 10 weapon types from the original game
- Enemy AI with patrol/chase/attack behaviors
- Interactive sectors: doors, elevators, teleporters
- Software audio mixer with VOC format support
- Optimized for PSP's MIPS R4000 CPU at 333 MHz
- Built-in HUD with health, armor, ammo, and FPS display

## Requirements

- **REDNECK.GRP** — You MUST own the original game. Get it from [GOG.com](https://www.gog.com) or [Steam](https://store.steampowered.com/).
- PSP with Custom Firmware (CFW) or a PSP emulator (PPSSPP)

## Building

### Using GitHub Actions (recommended)

1. Push this repository to GitHub
2. GitHub Actions will automatically build the EBOOT.PBP
3. Download the artifact from the Actions tab

### Local Build (with pspdev toolchain)

```bash
# Install pspdev (https://github.com/pspdev/pspdev)
# Or use Docker:
docker run --rm -v $(pwd):/src -w /src ghcr.io/pspdev/psp:latest bash -c \
  "mkdir -p build && cd build && psp-cmake .. && make -j$(nproc)"
```

## Installation on PSP

1. Copy the `EBOOT.PBP` to: `ms0:/PSP/GAME/RedneckRampage/EBOOT.PBP`
2. Copy `REDNECK.GRP` to: `ms0:/PSP/GAME/RedneckRampage/REDNECK.GRP`
3. Launch from the PSP XMB menu under Game → Memory Stick

## Controls

| PSP Button | Action |
|---|---|
| D-Pad Up/Down | Move forward/backward |
| D-Pad Left/Right | Strafe |
| Analog Stick | Look/Turn |
| Cross (X) | Fire |
| Square | Use / Open |
| Triangle | Jump |
| Circle | Crouch |
| L Trigger | Previous weapon |
| R Trigger | Next weapon |
| L+R Triggers | Run |
| Start | Pause / Menu |
| Select | Toggle map |

## Architecture

```
src/
├── engine/      Build engine core (renderer, math, cache)
├── game/        Game logic (player, actors, weapons, sectors, menus)
├── platform/    PSP hardware abstraction (display, input, audio, timer)
└── util/        File format parsers (GRP, ART, MAP)
```

## Legal

This project contains NO copyrighted game data. You must provide your own `REDNECK.GRP` from a legitimate copy of the game. The Build engine source code was released by Ken Silverman under a source-available license.

## License

This port is provided for educational and personal use only. Redneck Rampage is a trademark of Interplay Entertainment.
