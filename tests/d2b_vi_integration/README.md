# D2B V/I live integration capture

This directory contains a same-origin browser capture helper and an offline
validator. The capture helper records raw control text and binary frame bytes;
it is not a second protocol parser. The offline validator loads the tracked
`d2b-stream/0.1` Python validator from the separately cloned oracle repository.

Run the validator self-test from the VAMeter-Edu repository root:

```sh
python3 tests/d2b_vi_integration/validate_live_capture.py \
  --oracle "$HOME/Dev/Device-to-Browser-Data-Streaming" --self-test
```

For a real capture, open `http://<device>/d2b/v0/` in Chrome or Edge and paste
all of `capture-live.js` into DevTools Console. The script defaults to 30
seconds, requests an orderly stop, downloads JSON, and also leaves the result
in `window.__d2bCapture`. To select a duration before pasting the script:

```js
window.D2B_CAPTURE_SECONDS = 300;
```

Validate one capture or two same-boot reconnect captures:

```sh
python3 tests/d2b_vi_integration/validate_live_capture.py \
  --oracle "$HOME/Dev/Device-to-Browser-Data-Streaming" first.json second.json
```

The complete physical-device and browser procedure, evidence fields, gates,
and interpretation rules are in
`docs/vi-logger/operations/d2b_vi_live_validation_plan.md`.
