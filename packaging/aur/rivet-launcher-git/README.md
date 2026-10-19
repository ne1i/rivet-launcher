# AUR packaging

This directory contains the `rivet-launcher-git` AUR package.

## Why `-git` and not a per-commit AUR push

A VCS package builds from the current `master` branch. The `pkgver()` function
derives a version from the number of commits and the short hash, so every AUR
build picks up the latest Rivet commit without a maintainer pushing a new
PKGBUILD on each commit. This is the AUR convention for development packages.

A `-bin` package updated on every GitHub release would require the maintainer's
AUR SSH key to be stored in GitHub Actions secrets, and a registered
`rivet-launcher-bin` AUR repository. That is possible, but not needed for
automatic tracking.

## Publish (one-time)

The AUR requires an account and an SSH public key on
<https://aur.archlinux.org/account/>.

```bash
# 1. Add ~/.ssh/id_ed25519.pub to your AUR account.

# 2. Clone the empty AUR repository (the name must not exist yet).
git clone ssh://aur@aur.archlinux.org/rivet-launcher-git.git
cd rivet-launcher-git

# 3. Copy the package files.
cp /path/to/rivet-launcher/packaging/aur/rivet-launcher-git/PKGBUILD .
cp /path/to/rivet-launcher/packaging/aur/rivet-launcher-git/.SRCINFO .

git add PKGBUILD .SRCINFO
git commit -m "Initial import: rivet-launcher-git 1.0.0"
git push
```

After that, users install it with:

```bash
paru -S rivet-launcher-git
# or
yay -S rivet-launcher-git
```

## Local validation

```bash
cd packaging/aur/rivet-launcher-git
makepkg --printsrcinfo > .SRCINFO
makepkg -s
```
