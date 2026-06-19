# Tencent RUM analytics

Vibe Pet uses Tencent Cloud RUM/Aegis for lightweight product analytics in the Electron desktop window.

## Configuration

Create a Web application in Tencent Cloud RUM, then pass the SDK report ID from the SDK integration snippet when starting or packaging the app. In Tencent Cloud docs this field is called `report id`; it may be different from the console `business system ID`.

The default report ID in this app is `3oLdoCL4jZnGkgvoYp`.

```bash
CODE_PET_TENCENT_RUM_ID=your-rum-app-id npm start
```

You can also use launch arguments:

```bash
npm start -- --tencent-rum-id your-rum-app-id
```

By default, Vibe Pet probes the Tencent RUM hosts at startup and chooses the reachable endpoint with the lowest latency:

- `https://rumt-zh.com`
- `https://rumt-sg.com`
- `https://rumt-us.com`
- `https://aegis.qq.com`

You can still force a report host:

```bash
CODE_PET_TENCENT_RUM_HOST_URL=https://rumt-zh.com
```

Set the value to `auto` or leave it unset to use automatic host selection.

Analytics can be disabled with:

```bash
CODE_PET_DISABLE_ANALYTICS=1 npm start
```

To diagnose reporting failures, start with:

```bash
npm start -- --analytics-debug
```

Debug mode sends an `analytics_probe` event and prints Aegis `beforeRequest` / `afterRequest` details to the terminal. If Tencent returns 403, re-check that the configured ID is the SDK `report id`.

## Events

- `app_installed`: first launch on this machine.
- `app_started`: every app launch.
- `pet_selected`: character selection.
- `pet_active`: active character usage, deduped per app run.

Custom local and AI-generated characters are reported only as `custom_local` or `custom_ai`; local filenames and custom prompt text are not uploaded.

## Tencent dimensions

Events are sent through `aegis.reportEvent`.

- `name`: event name.
- `ext1`: character slug for pet events, otherwise platform.
- `ext2`: character source/kind for pet events, otherwise app version.
- `ext3`: compact JSON details, trimmed to 1024 characters.
