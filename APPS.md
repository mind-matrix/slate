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

## Windows and overlays

Slate's shell is the Wayland compositor. An app with `gui` or `overlay` finds it through `WAYLAND_DISPLAY`
(`/run/slate-display/wayland-0`) and gets its own `XDG_RUNTIME_DIR`, so any toolkit with a Wayland backend works
(GTK, Qt, SDL, Flutter, WPE, LVGL). Apps without either permission can't connect. The GPU's render node is
available for OpenGL ES.

Slate only offers an app the Wayland globals its permissions allow: `xdg_wm_base` needs `gui`, the layer shell
(`zwlr_layer_shell_v1`) needs `overlay`, and the core globals (`wl_compositor`, `wl_shm`, `wl_seat`, `wl_output`,
`linux-dmabuf`, `xdg_activation_v1`, `wp_viewporter`) need one of the two. Input is touch (`wl_touch`), plus a
keyboard when one is attached.

**Full-screen windows (`gui`).**

- Every `xdg_toplevel` is sized 1280 × 672: the whole screen below Slate's status bar. Draw at the size you're
  configured with.
- A window stays hidden until the app is opened: from its desktop tile, from Open in Settings › Apps, or with an
  activation token from Slate. Mapping a window, or asking `xdg_activation_v1` to activate it with a token the app
  made itself, never brings it to the front.
- While an app is open, the status bar shows Slate's **Back** and **Home**.
  - **Back** sends your toplevel `close`. Treat it as "go back": if you have a previous screen, go to it and keep
    the window. At your first screen, unmap the window (commit a null buffer) and keep running. Slate then shows
    the desktop. Then commit once more without a buffer: under xdg-shell an unmapped window must start
    over, and Slate can only ask a window that has.
  - **Home** hides your window and shows the desktop. Your app keeps running and isn't told.
  - When the user opens your app again, Slate sends that window a configure with the `activated` state.
    Map the window again when you get it.
  - Don't destroy your toplevel on `close` and keep running without a window: Slate has no way to ask for a new
    one. If your toolkit quits when its last window closes, that is fine too; opening the app starts it again.
- Slate may hand an app an activation token, for example with Setup or Logout on the Voice assistant screen. Pass
  it to `xdg_activation_v1.activate` with your window's surface to come to the front. Each token works once, for
  the app it was issued to, and expires after 30 seconds.

**Overlays (`overlay`).**

- Create layer surfaces with `zwlr_layer_shell_v1`. Whatever layer you ask for, they are drawn above apps and the
  desktop and below the status bar, the drawer and system alerts. Anchors, size and margins work as usual on the
  1280 × 720 screen.
- Touches inside an overlay's input region go to it. A tap anywhere outside every overlay **closes all of them**:
  each gets `closed`, and that tap goes nowhere else. Destroy the surface when you get `closed`, and create a new one
  when you need it again.
- Slate's microphone badge doesn't depend on overlays: it shows whenever an assistant has the mic open.

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
