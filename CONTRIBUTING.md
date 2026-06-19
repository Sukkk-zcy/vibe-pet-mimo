# Contributing to Vibe Pet

Thanks for helping improve Vibe Pet. This project connects desktop AI coding
agent activity to animated desktop pets and BLE/Wi-Fi hardware displays, so
clear changes, careful testing, and good protocol notes all matter.

## Ways to Contribute

- Report bugs with reproduction steps, logs, platform details, and device model.
- Improve agent integrations, hooks, plugin support, or session monitoring.
- Add or improve hardware firmware profiles.
- Improve desktop UI, overlay behavior, translations, or documentation.
- Fix packaging, installer, or setup issues across macOS, Windows, and Linux.

For larger changes, please open an issue or discussion first so the design and
scope can be agreed before implementation.

## Development Setup

Requirements:

- Node.js 18 or newer.
- npm.
- PlatformIO, only if you are building or flashing firmware.
- Supported BLE/Wi-Fi hardware, only if you are testing hardware sync.

Install dependencies:

```bash
npm install
```

Start the desktop app:

```bash
npm start
```

Run in development mode:

```bash
npm run dev
```

Run the local web bridge directly:

```bash
npm run bridge:web:dev
```

Run project checks:

```bash
npm run check
```

`npm install`, `npm start`, and `npm run dev` automatically sync supported
hooks and plugins. To manage hooks manually:

```bash
npm run install:hooks
npm run uninstall:hooks
```

## Project Areas

- `src/desktop/` contains the Electron desktop app, overlay, preload code, and
  desktop assets.
- `src/host/` contains the local bridge service, public web UI, agent state
  aggregation, and protocol handling.
- `src/hooks/` contains integrations for supported AI coding agents.
- `src/firmware/` contains firmware profiles for supported devices.
- `docs/` contains protocol and integration documentation.

## Coding Guidelines

- Follow the existing CommonJS style and nearby file conventions.
- Keep changes focused. Avoid unrelated formatting or large refactors in the
  same pull request.
- Preserve backwards compatibility for agent state payloads and hardware
  protocols unless the change is explicitly a breaking change.
- Update protocol docs when changing BLE, Wi-Fi, hook, or agent state formats.
- Update translations when changing user-facing text.
- Do not commit `node_modules`, local secrets, generated build outputs, or
  machine-specific files.

## Testing

Before opening a pull request, run:

```bash
npm run check
```

Also test the area you changed:

- Desktop changes: run `npm start` or `npm run dev` and verify the main window,
  overlay, and relevant pet states.
- Hook or agent changes: verify the target agent reports expected state changes.
- Protocol changes: verify the bridge still accepts old and new expected payloads
  where compatibility is intended.
- Firmware changes: build with PlatformIO for the changed environment, and flash
  a real device when possible.
- Packaging changes: run the relevant `npm run package:*`, `npm run build:*`, or
  `npm run installer:*` command for the target platform when feasible.

## Pull Request Checklist

- The change is focused and explained clearly.
- `npm run check` passes, or the reason it could not be run is documented.
- UI changes include screenshots or a short visual description.
- Hardware, firmware, or agent integration changes list the tested devices,
  platforms, or agent versions.
- Documentation and translations are updated when behavior or user-facing text
  changes.
- No secrets, local paths, or generated dependency folders are included.

## License

By contributing to this project, you agree that your contributions are licensed
under the MIT License.
