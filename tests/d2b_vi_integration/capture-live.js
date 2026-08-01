// SPDX-License-Identifier: Apache-2.0
// Paste this whole file into Chrome or Edge DevTools while
// http://<device>/d2b/v0/ is the active page.

(async () => {
  "use strict";

  const captureSeconds = Number.isFinite(window.D2B_CAPTURE_SECONDS)
    ? Number(window.D2B_CAPTURE_SECONDS)
    : 30;
  const maximumFrames = Number.isSafeInteger(window.D2B_CAPTURE_MAX_FRAMES)
    ? Number(window.D2B_CAPTURE_MAX_FRAMES)
    : 100000;
  if (captureSeconds < 5 || captureSeconds > 1800) {
    throw new Error("D2B_CAPTURE_SECONDS must be between 5 and 1800");
  }
  if (maximumFrames < 1 || maximumFrames > 100000) {
    throw new Error("D2B_CAPTURE_MAX_FRAMES must be between 1 and 100000");
  }
  if (location.protocol !== "http:" || !location.pathname.startsWith("/d2b/v0/")) {
    throw new Error("open the device http://<device>/d2b/v0/ page before running this capture");
  }

  const base = new URL("/d2b/v0/", location.origin);
  const started = performance.now();
  let eventIndex = 0;
  let stopRequested = false;
  let finished = false;
  let stopTimer;
  let watchdog;

  const capture = {
    format: "vameter-d2b-live-capture/0.1",
    captured_at: new Date().toISOString(),
    user_agent: navigator.userAgent,
    device_base_url: base.href,
    duration_seconds: captureSeconds,
    capabilities_text: null,
    status_before_text: null,
    controls: [],
    frames: [],
    status_after_text: null,
  };

  const elapsed = () => performance.now() - started;
  const recordControl = (direction, text) => {
    capture.controls.push({
      event_index: eventIndex++,
      received_ms: elapsed(),
      direction,
      text,
    });
  };
  const fetchText = async (name) => {
    const response = await fetch(new URL(name, base), { cache: "no-store" });
    if (!response.ok) throw new Error(`${name}: HTTP ${response.status}`);
    return response.text();
  };
  const hex = (buffer) => Array.from(new Uint8Array(buffer), (value) =>
    value.toString(16).padStart(2, "0")).join("");
  const save = () => {
    window.__d2bCapture = capture;
    const blob = new Blob([`${JSON.stringify(capture, null, 2)}\n`], {
      type: "application/json",
    });
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = `vameter-d2b-${capture.captured_at.replaceAll(":", "-")}.json`;
    document.body.append(link);
    link.click();
    link.remove();
    const blobUrl = link.href;
    setTimeout(() => URL.revokeObjectURL(blobUrl), 1000);
  };

  capture.capabilities_text = await fetchText("capabilities");
  capture.status_before_text = await fetchText("status");

  const socketUrl = new URL("stream", base);
  socketUrl.protocol = "ws:";
  const socket = new WebSocket(socketUrl);
  socket.binaryType = "arraybuffer";

  const sendControl = (value) => {
    const text = JSON.stringify(value);
    recordControl("client_to_server", text);
    socket.send(text);
  };
  const requestStop = (streamId, reason) => {
    if (stopRequested || finished) return;
    stopRequested = true;
    sendControl({ type: "stop_stream", stream_id: streamId, reason });
  };

  await new Promise((resolve, reject) => {
    const fail = (error) => {
      if (finished) return;
      finished = true;
      clearTimeout(stopTimer);
      clearTimeout(watchdog);
      try { socket.close(); } catch (_) { /* best effort */ }
      reject(error instanceof Error ? error : new Error(String(error)));
    };

    watchdog = setTimeout(() => fail(new Error("capture watchdog expired")),
      (captureSeconds + 20) * 1000);

    socket.addEventListener("open", () => {
      sendControl({
        type: "hello",
        protocol: "d2b-stream",
        versions: ["0.1"],
        client_name: "VAMeter-Edu integration capture",
      });
    });

    socket.addEventListener("message", async (event) => {
      try {
        if (typeof event.data === "string") {
          recordControl("server_to_client", event.data);
          const message = JSON.parse(event.data);
          if (message.type === "welcome") {
            sendControl({
              type: "start_stream",
              stream: "live-vi",
              profile: "vi-measurement",
              parameters: {
                sample_format: "vi-f32le",
                channel_count: 2,
                channel_mask: 3,
                sample_rate: { numerator: 0, denominator: 0 },
              },
            });
          } else if (message.type === "stream_started") {
            stopTimer = setTimeout(() => requestStop(message.stream_id, "capture complete"),
              captureSeconds * 1000);
          } else if (message.type === "stream_stopped") {
            capture.status_after_text = await fetchText("status");
            finished = true;
            clearTimeout(stopTimer);
            clearTimeout(watchdog);
            socket.close(1000, "capture complete");
            resolve();
          } else if (message.type === "error") {
            fail(new Error(`device error ${message.code}: ${message.message}`));
          }
          return;
        }

        if (!(event.data instanceof ArrayBuffer)) {
          fail(new Error("unexpected WebSocket payload type"));
          return;
        }
        if (capture.frames.length >= maximumFrames) {
          fail(new Error("capture frame limit reached before an orderly stop"));
          return;
        }
        capture.frames.push({
          event_index: eventIndex++,
          received_ms: elapsed(),
          hex: hex(event.data),
        });
      } catch (error) {
        fail(error);
      }
    });

    socket.addEventListener("error", () => fail(new Error("WebSocket error")));
    socket.addEventListener("close", (event) => {
      if (!finished) fail(new Error(`WebSocket closed early: ${event.code} ${event.reason}`));
    });
  });

  save();
  console.log("D2B capture complete", capture);
  return capture;
})();
