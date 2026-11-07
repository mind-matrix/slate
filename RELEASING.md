# Releasing Slate

Slate devices update from GitHub Releases. A release is only installed if its manifest is signed with the Slate
signing key, which lives offline on the maintainer's machine and never in this repository or GitHub.

## One-time setup

1. Install `minisign`, `jq` and the GitHub CLI (`gh auth login`) in the build environment.
2. Run `tools/new-signing-key.sh` and choose a passphrase. It writes the secret key to `~/.minisign/slate.key` and
   the public key to `packaging/update.pub`.
3. Back up `~/.minisign/slate.key` and its passphrase offline. Devices only trust this key; losing it means existing
   devices can no longer be updated and need a new image.
4. Commit `packaging/update.pub`. Images and packages built after this trust it.

## Each release

1. Set the new version in `VERSION` (`YYYY.M.D`, add `.N` for a second release on the same day). Versions must
   always increase.
2. Write `releases/<version>.md`. Lines starting with `- ` become the notes shown on the device.
3. If a release removes a package Slate used to depend on, list it in `packaging/obsolete-packages`.
4. If devices on an old version cannot update directly to this one, raise `packaging/min-version`.
5. Commit, tag `v<version>` and push the tag. The Release workflow builds, tests and creates a draft release with
   the package and an unsigned manifest.
6. Run `tools/sign-release.sh v<version>`. It checks the manifest against the package, signs it with your key,
   uploads the signature and publishes the release. Devices see it on their next check.

Draft releases are never offered to devices, so nothing reaches them until step 6.
