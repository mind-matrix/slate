# Building an app for Slate

Slate runs second-party apps such as Marvin and Ava next to its own shell. Each app lives in its own repository,
publishes its own signed releases, and is updated separately from Slate. This document is the contract an app follows.

## Identity

Every app has an **id**: lowercase letters, digits and dashes, starting with a letter or digit (for example `ava`).
The id is internal. It names the app's package, files, Linux user and service, and is never shown to people.

## Package

An app ships as one Debian package for arm64:

- package name `slate-app-<id>`, file name `slate-app-<id>_<version>_arm64.deb`
- versions compare with `dpkg --compare-versions` and must always increase
- dependencies are declared in the package's `Depends:` as usual

The package installs:

| Path | Purpose |
|---|---|
| `/usr/lib/slate-apps/<id>/run` | Executable that starts the app. It runs in the foreground and exits on SIGTERM. |
| `/usr/share/slate/apps/<id>.app` | Metadata, described below. |
| `/usr/share/slate/apps/<id>.png` | Optional logo, 256 × 256 RGBA PNG. A missing or invalid logo is replaced by a generic one. |

The package must not install systemd units, polkit rules, sudoers entries or anything under `/etc`. Slate starts the
app and decides what it may do; files that try to grant rights are not used.

## The `.app` file

Plain `key=value` lines:

```
name=Ava
permissions=audio network discovery secrets assistant gui overlay
registration=required
```

| Key | Meaning |
|---|---|
| `name` | Display name, shown everywhere in Slate: desktop tile, Settings › Apps, Voice assistant, Software update and notifications. If it is missing, Slate shows the id. |
| `permissions` | Space-separated permissions the app asks for. It receives only those that Slate has also granted it. |
| `registration` | `required` if the app needs an account or setup before use, otherwise `none` (the default). Controls Setup and Logout on the Voice assistant screen. |

## Permissions

Slate defines each permission in `/usr/share/slate/permissions/<name>.conf`. An app gets a permission only when it
asks for it in its `.app` file **and** Slate grants it in the app's policy. Everything else is denied.

| Permission | Allows |
|---|---|
| `audio` | Microphone and speaker. Assistants only have them while they are the selected assistant. |
| `network` | Internet and local network sockets. |
| `discovery` | Registering and browsing services through Slate's shared mDNS service (DNS-SD API). Apps must not run their own mDNS daemon. |
| `secrets` | The app's own namespace in the Slate key store. |
| `assistant` | Being offered as a voice assistant, and the assistant API. |
| `wifi` | Joining and changing Wi-Fi networks through NetworkManager. |
| `bluetooth` | Pairing and connecting Bluetooth devices. |
| `power` | Restarting and shutting down the device. |
| `timezone` | Changing the time zone. |
| `notifications` | Posting to the notification list. |
| `gui` | Full-screen windows, with Slate's Back and Home controls in the status bar. Gives the app a desktop tile. |
| `overlay` | Surfaces drawn above other apps and the desktop, for example voice captions. |

## How the app runs

- The app runs as its own Linux user `app-<id>` under the service `slate-app@<id>`, started at boot.
- Its private, writable home is `/var/lib/slate-apps/<id>` (also `$HOME`). The rest of the system is read-only,
  home folders are hidden, and devices are not available directly.
- It is restarted if it crashes.
- When stopped, it gets SIGTERM and **60 seconds** to exit. After that, it and everything it started are killed.
  An app cannot extend this.

## Releases

Each GitHub release of the app's repository carries three files:

- `slate-app-<id>_<version>_arm64.deb`
- `manifest.json`
- `manifest.json.minisig`, the manifest signed with the app's own minisign key

The manifest uses the same format as Slate's (see `packaging/make-manifest.sh`):

```json
{
  "version": "1.2.0",
  "channel": "release",
  "min_version": "1.0.0",
  "remove": [],
  "package": { "file": "...deb", "url": "https://github.com/<owner>/<repo>/releases/download/<tag>/...deb",
               "sha256": "...", "size": 123456 },
  "notes": ["What changed, one line each."]
}
```

Devices fetch `releases/latest/download/manifest.json` and its signature, verify the signature with the public key
pinned in Slate, check the package's size, hash and package name, and install it. A manifest signed with any other
key, or a package whose name is not `slate-app-<id>`, is refused. Draft releases are never seen by devices.

## Becoming a built-in app

Slate bundles and updates an app only when Slate's repository contains its policy,
`packaging/root/usr/share/slate/app-policy/<id>.conf`:

```
repo=<owner>/<repo>
key=<the app's minisign public key, the base64 line>
permissions=<the permissions Slate grants>
```

Images include the latest signed release of every app with a policy. Apps are updated from Settings › About ›
Software update, one row per app.

## Building from a local checkout (dev images only)

The repository must provide `packaging/build-deb.sh <output dir>`. It builds the arm64 package into that directory
and prints the path of the `.deb` on its last line. To bundle a local checkout into a dev image, add to
`config/local.conf`:

```
export SLATE_APP_SOURCE_<id>=/path/to/checkout
```

A local build replaces the GitHub release of the same app. It is marked as a dev build, so it is never updated from
GitHub, and it receives the permissions it asks for even without a policy.
