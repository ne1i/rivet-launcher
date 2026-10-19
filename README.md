# Rivet

[![Build release](https://github.com/ne1i/rivet-launcher/actions/workflows/release.yml/badge.svg?branch=master)](https://github.com/ne1i/rivet-launcher/actions/workflows/release.yml)

**A native Linux launcher for online-fixed games.** Rivet ties a game, its
multiplayer fix, Proton, Steam and the right Wine environment together, without
bundling Java, JPHP or a browser runtime.

Rivet is a from-scratch C++20 / Qt 6 rewrite of
[ZzEdovec/onlinefix-linux](https://github.com/ZzEdovec/onlinefix-linux). The UI
is deliberately animation-free: no fades, spinners, splash screens or delays.

> **Legal notice:** Rivet does not host or distribute games, cracks or fixes.
> You must own the games you run and comply with copyright, anti-circumvention,
> contract and local law. Read [`DISCLAIMER.md`](DISCLAIMER.md) before using it.
> It is not legal advice, and no disclaimer can make an unlawful act lawful.

## Features

- **Library** with covers, icons, play time, instant search and a detail page.
- **Add games** from an extracted folder, a RAR archive, or a FreeTP/OnlineFix
  installer.
- **Fix detection and patches** for OnlineFix, FreeTP and EOSFix:
  - `OnlineFix.ini` / `SteamFix.ini` AppIDs and `ExtraProtection`
  - FreeTP `SteamFix` section patch
  - Photon Launcher `Newtonsoft.Json.dll`
  - `winmm.txt` / `dlllist.txt` DLL overrides
  - FreeTP “bypass Steam not running” shim
- **Proton manager**: browse GE-Proton releases from GitHub, download, unpack,
  remove, choose a default, and detect incompatible CPU architectures.
- **Steam integration**: start Steam and wait for sign-in; inject the overlay;
  use Steam Linux Runtime; use native Wayland.
- **Per-game launch settings**: arguments before/after the executable,
  environment variables, DLL overrides, prefix path, cover and icon.
- **Wine tools**: `winecfg`, task manager, control panel, explorer, command
  prompt, registry editor, Winetricks, and run any `.exe` in the prefix.
- **Game downloads** from a user-supplied Hydra/OnlineFix JSON source through
  `aria2c`, with progress, cancel and resume.
- **Shortcuts**, single-instance launch mode, tray support, logs and play-time
  tracking.

## Configuration

Rivet uses its own versioned JSON document — not the old INI files:

```
$XDG_CONFIG_HOME/rivet/config.json      # usually ~/.config/rivet/config.json
$XDG_DATA_HOME/rivet/                   # usually ~/.local/share/rivet/
  ├── icons/
  ├── covers/
  ├── protons/                          # default for new setups
  └── prefixes/                         # default for new setups
```

The document is structured instead of section/key soup. Environment variables
are a real JSON object (no more four-backslash escaping), and optional data is
grouped by meaning:

```json
{
  "format": "rivet",
  "version": 1,
  "settings": {
    "default_proton": "GE-Proton Latest",
    "no_steam_request": false,
    "fullscreen": false,
    "downloads_source": "",
    "paths": {
      "downloads": "",
      "installs": "",
      "prefixes": "",
      "protons": ""
    },
    "new_games": {
      "native_wayland": true,
      "wined3d": false
    }
  },
  "games": [
    {
      "name": "Example Game",
      "executable": "/games/Example/Example.exe",
      "game_dir": "/games/Example",
      "prefix": "/games/Example/Rivet Prefix",
      "proton": "GE-Proton Latest",
      "cover": "/home/user/.local/share/rivet/covers/12345.jpg",
      "icon": "/home/user/.local/share/rivet/icons/AbCdEf.png",
      "dll_overrides": "onlinefix64=n;steam_api64=n",
      "fix_dir": "/games/Example",
      "args": { "before": "", "after": "" },
      "environment": { "LC_ALL": "fr_FR.UTF-8" },
      "steam": { "app_id": "12345", "overlay_app_id": "480" },
      "time_played": 3600,
      "features": {
        "steam_overlay": true,
        "steam_runtime": true,
        "wined3d": false,
        "native_wayland": true
      }
    }
  ]
}
```

### One-time migration

On first start, if `~/.config/rivet/config.json` does not exist, Rivet converts
`~/.config/OFME-Linux/Games.ini` and `Launcher.ini` automatically:

- all games, play time, DLL overrides, launch options and feature flags move to
  the JSON document;
- the legacy environment string is parsed into a proper object;
- covers and icons are copied into `~/.local/share/rivet/{covers,icons}`;
- existing Proton and prefix roots are reused **in place**, so multi-gigabyte
  prefixes and Proton builds are not duplicated;
- the old files are left untouched as a backup.

No further reads from the old format happen after migration.

## Dependencies

Required:

- `steam` — the native package, **not** Flatpak or Snap
- `unrar` — RAR archives
- `7z` — icon extraction fallback
- `tar` — GE-Proton archives
- `aria2` — game downloads
- Qt 6 (Core, Gui, Widgets, Network, Concurrent, Svg)

Recommended:

- `ffmpeg` — convert extracted `.ico` icons
- `icoextract` — better icon extraction than 7z
- `winetricks` — the Wine tools menu entry

Arch Linux:

```bash
sudo pacman -S --needed base-devel cmake qt6-base qt6-svg steam unrar 7zip \
    ffmpeg icoextract winetricks aria2
```

## Releases

Every push to `master` creates a release with:

- `rivet-linux-x86_64` and `rivet-linux-aarch64` — directly runnable Linux
  executables;
- `SHA256SUMS-*` — checksums.

```bash
chmod +x rivet-linux-x86_64
./rivet-linux-x86_64
```

The binaries are dynamically linked against Qt 6 and call the helper programs
listed under Dependencies.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/rivet
```

Install:

```bash
sudo cmake --install build
```

This installs `rivet` to `/usr/local/bin`, the desktop entry and the icon.

### Arch Linux (AUR)

AUR packaging for `rivet-launcher-git` is included under
`packaging/aur/rivet-launcher-git`. It is a VCS package, so it automatically
tracks the latest `master` commit; no per-commit AUR upload is needed. Once the
package is published:

```bash
paru -S rivet-launcher-git
# or
yay -S rivet-launcher-git
```

### Nix / NixOS

```bash
nix run github:ne1i/rivet-launcher
nix build github:ne1i/rivet-launcher
```

Flakes and the `nix` command are still marked experimental. Enable them once:

```bash
mkdir -p ~/.config/nix
printf 'experimental-features = nix-command flakes\n' >> ~/.config/nix/nix.conf
```

If you get `/nix/store: Permission denied` or `cannot write modified lock file`:

- Run Nix as your user, **never with `sudo`**. `sudo nix run` drops your
  Wayland/X11 session credentials and GUI apps then fail to open a display.
- Use a multi-user Nix install with the daemon running
  (`sudo systemctl enable --now nix-daemon`) and add yourself to the Nix users
  group (`sudo usermod -aG nix-users "$USER"`), then log out and back in.
- This repository ships `flake.lock`, so Nix does not need to modify the lock
  file in the store.
- On X11, install `xcb-util-cursor` if Qt reports that the xcb platform plugin
  is missing `xcb-cursor0`. On Wayland, launch from your normal user session
  (`WAYLAND_DISPLAY` set), or try `QT_QPA_PLATFORM=wayland`.

Or from the checkout:

```bash
nix develop
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The flake wraps the binary with the runtime helpers it needs (`aria2`, `ffmpeg`,
`icoextract`, `7z`, `winetricks`, `tar`, `xdg-utils`, `procps`). Steam is not
included by the flake; enable the native Steam package in your NixOS
configuration. `unrar` is unfree, so add it explicitly if you want RAR support:

```nix
nixpkgs.config.allowUnfree = true;
environment.systemPackages = [ pkgs.unrar ];
```

## Tests

```bash
cmake -S . -B build -DRIVET_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

The test covers legacy INI → JSON migration, structured environment values,
the add-game pipeline and a JSON round-trip.

## Command line

```text
rivet                              Open the launcher
rivet --launch "Game name"         Launch a game directly
rivet "Game name"                  Same as --launch
rivet --list                       List games and executables
rivet --print-command "Game name"  Print the Proton command and environment
rivet --version                    Show the version
```

`--print-command` is useful to debug environment/override problems without
starting a game.

## Notes

- Shortcuts created by Rivet point at the installed `rivet` binary. Old
  shortcuts from previous launchers can be removed and recreated from a game's
  page.
- Some game entries may point at executables on unmounted drives. Rivet reports
  this instead of failing silently.
- Steam must be the native package. Flatpak and Snap Steam installations do not
  work with Proton prefixes.
- Rivet is not affiliated with Valve, Steam, OnlineFix, FreeTP, EOSFix or any
  game publisher. See [`THIRD_PARTY.md`](THIRD_PARTY.md) and
  [`DISCLAIMER.md`](DISCLAIMER.md).

## License

GNU Affero General Public License v3.0. See [`LICENSE`](LICENSE).
