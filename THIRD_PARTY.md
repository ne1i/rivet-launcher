# Third-party notices

Rivet is a rewrite of
[ZzEdovec/onlinefix-linux](https://github.com/ZzEdovec/onlinefix-linux) and is
distributed under the GNU Affero General Public License v3.0. The original
project's authors retain copyright over their work; see `LICENSE`.

## Bundled compatibility libraries

| File | Purpose | Provenance / licence |
| --- | --- | --- |
| `assets/Newtonsoft.Json.dll` | Required by some Photon Launcher fixes | Newtonsoft.Json, MIT licence. The licence text is in `third_party/Newtonsoft.Json.LICENSE.md`. |
| `assets/ftpPath32.dll` | FreeTP "bypass Steam not running" compatibility shim | Taken from the original onlinefix-linux repository. Copyright belongs to its original authors; included only to preserve compatibility with existing fix files. |
| `assets/ftpPath64.dll` | FreeTP "bypass Steam not running" compatibility shim | Same as above. |

No game assets, covers, icons, executables or fixes are bundled. Game covers
and icons are downloaded or extracted locally at runtime and are not part of
this repository.

If you own the rights to one of the bundled compatibility libraries and object
to its inclusion, open an issue on the repository with the file name and the
basis of your request. It will be reviewed and removed or replaced.

## Trademarks

Steam and Valve are trademarks or registered trademarks of Valve Corporation.
Epic Games, OnlineFix, FreeTP, EOSFix, Proton, GE-Proton, GitHub, Qt and all
other names are the property of their respective owners. Their use here is
descriptive only and does not imply affiliation or endorsement.

## Qt

Rivet links against Qt 6. Qt is available under the GNU Lesser General Public
License v3 (or GPLv3 for some modules) and is not redistributed as a bundled
binary by this repository. Your distribution's Qt package is used.

## Other tools

Rivet invokes external programs such as `aria2c`, `ffmpeg`, `unrar`, `7z`,
`icoextract`, `tar`, `winetricks`, Wine/Proton and Steam. Those programs are
not part of Rivet and are governed by their own licences and terms. No
third-party game or fix content is included.
