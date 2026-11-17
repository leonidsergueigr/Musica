# Linux packaging

Musica ships a CMake install with CPack, so the release workflow produces a
`.tar.gz` archive, a `.deb` package and an `.rpm` package. The files in this
directory describe the distribution channels that are maintained separately.

All system builds require FTXUI 7 or newer and TagLib 2.0 or newer, because the
player uses `FileRef::complexProperties()`. Configure with
`-DMUSICA_SYSTEM_DEPS=ON`; without it CMake downloads both libraries, which is
not acceptable for distribution packaging.

## Fedora and COPR

`packaging/fedora/musica.spec` builds the package from the GitHub tag archive
with the system FTXUI and TagLib.

Building locally:

```sh
rpmbuild -ba packaging/fedora/musica.spec
```

Publishing to COPR is a manual step and needs a COPR account:

```sh
copr-cli build <project> packaging/fedora/musica.spec
```

The spec is not submitted to Fedora yet. Before doing so, check that the target
release provides FTXUI 7 and TagLib 2, and update the spec if the package names
differ.

## Arch Linux and AUR

`packaging/arch/PKGBUILD` builds from the same tag archive. The checksum is a
placeholder: on the release machine run

```sh
cd packaging/arch
updpkgsums
```

then build with `makepkg -si`. Publishing to the AUR is a manual step and needs
an AUR account; push the PKGBUILD and its `.SRCINFO` (`makepkg --printsrcinfo`).

The `makedepends` assume that the repositories or the AUR provide FTXUI 7 and
TagLib 2. Adjust the names if the available packages differ.

## Debian and Ubuntu

The release workflow publishes a `.deb` built on Ubuntu 22.04. There is no
official APT repository; installing the downloaded package with
`sudo apt install ./musica-<version>-Linux.deb` is the supported path.

An APT repository hosted on GitHub Pages is possible, but it is not set up here
and would require a signing key and a published `Packages` index.

## Manual release checklist

1. Update `CHANGELOG.md` and the version in `CMakeLists.txt`.
2. Push the `v<version>` tag. The release workflow checks that the tag matches
   the project version, runs the tests, and publishes the assets.
3. Download the new source archive and fill in `sha256sums` in the PKGBUILD.
4. Build the spec in a clean mock or COPR chroot before announcing the package.
