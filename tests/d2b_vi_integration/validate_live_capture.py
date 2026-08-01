#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Validate VAMeter-Edu live captures with the d2b-stream oracle."""

from __future__ import annotations

import argparse
import importlib.util
import json
import math
import pathlib
import struct
import tempfile
from typing import Any


FORMAT = "vameter-d2b-live-capture/0.1"
PARAMETERS = {
    "sample_format": "vi-f32le",
    "channel_count": 2,
    "channel_mask": 3,
    "sample_rate": {"numerator": 0, "denominator": 0},
}
CAPTURE_FIELDS = {
    "format",
    "captured_at",
    "user_agent",
    "device_base_url",
    "duration_seconds",
    "capabilities_text",
    "status_before_text",
    "controls",
    "frames",
    "status_after_text",
}
PUBLIC_STATUS_FIELDS = {
    "protocol",
    "version",
    "state",
    "connected_client_count",
    "producer_drop_count",
    "output_queue_drop_count",
    "queued_sample_count",
    "uptime_us",
}


class CaptureError(Exception):
    pass


def require(condition: bool, detail: str) -> None:
    if not condition:
        raise CaptureError(detail)


def is_uint(value: Any, maximum: int = 2**53 - 1) -> bool:
    return isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= maximum


def load_oracle(repository: pathlib.Path):
    validator_path = repository.resolve() / "tools" / "validate_test_vectors.py"
    require(validator_path.is_file(), f"oracle validator not found: {validator_path}")
    spec = importlib.util.spec_from_file_location("d2b_oracle_validator", validator_path)
    require(spec is not None and spec.loader is not None, "cannot load oracle module")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_capture(path: pathlib.Path, oracle) -> dict[str, Any]:
    try:
        text = path.read_bytes().decode("utf-8")
        value = oracle.strict_json_loads(text)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, ValueError) as error:
        raise CaptureError(f"{path}: invalid UTF-8 JSON: {error}") from error
    require(isinstance(value, dict), f"{path}: capture is not an object")
    return value


def validate_public_status(value: Any, name: str) -> dict[str, Any]:
    require(isinstance(value, dict) and set(value) == PUBLIC_STATUS_FIELDS,
            f"{name}: wrong public status fields")
    require(value["protocol"] == "d2b-stream" and value["version"] == "0.1",
            f"{name}: wrong protocol/version")
    require(value["state"] in {"idle", "streaming"}, f"{name}: invalid state")
    for field in PUBLIC_STATUS_FIELDS - {"protocol", "version", "state"}:
        require(is_uint(value[field]), f"{name}: invalid {field}")
    return value


def parse_wire_json(text: Any, name: str, oracle) -> Any:
    require(isinstance(text, str) and len(text.encode("utf-8")) <= 2048,
            f"{name}: response is not bounded UTF-8 text")
    try:
        return oracle.strict_json_loads(text)
    except (json.JSONDecodeError, ValueError) as error:
        raise CaptureError(f"{name}: invalid JSON: {error}") from error


def validate_event(event: Any, fields: set[str], name: str) -> None:
    require(isinstance(event, dict) and set(event) == fields, f"{name}: wrong fields")
    require(is_uint(event["event_index"]), f"{name}: invalid event_index")
    received = event["received_ms"]
    require(isinstance(received, (int, float)) and not isinstance(received, bool) and
            math.isfinite(received) and received >= 0, f"{name}: invalid received_ms")


def binary_context(stream_id: int, maximum_size: int, previous: dict[str, int] | None) -> dict[str, Any]:
    context = {
        "negotiated_version": "0.1",
        "session_state": "STREAMING",
        "maximum_binary_frame_size": maximum_size,
        "stream_id": stream_id,
        "profile": "vi-measurement",
        "parameters": PARAMETERS,
    }
    if previous is not None:
        context.update(previous)
    return context


def validate_one(path: pathlib.Path, oracle) -> dict[str, Any]:
    capture = load_capture(path, oracle)
    require(set(capture) == CAPTURE_FIELDS, f"{path}: wrong capture fields")
    require(capture["format"] == FORMAT, f"{path}: wrong capture format")
    for field in ("captured_at", "user_agent", "device_base_url"):
        require(isinstance(capture[field], str) and capture[field], f"{path}: invalid {field}")
    require(capture["device_base_url"].startswith("http://") and
            capture["device_base_url"].endswith("/d2b/v0/"),
            f"{path}: capture was not made at the device same-origin page")
    duration = capture["duration_seconds"]
    require(isinstance(duration, (int, float)) and not isinstance(duration, bool) and
            math.isfinite(duration) and 5 <= duration <= 1800,
            f"{path}: invalid duration_seconds")

    capabilities = oracle.validate_capabilities(
        parse_wire_json(capture["capabilities_text"], f"{path}: capabilities", oracle))
    require(capabilities["maximum_binary_frame_size"] == 48,
            f"{path}: product did not advertise 48-byte V/I frames")
    before = validate_public_status(
        parse_wire_json(capture["status_before_text"], f"{path}: status_before", oracle),
        f"{path}: status_before")
    after = validate_public_status(
        parse_wire_json(capture["status_after_text"], f"{path}: status_after", oracle),
        f"{path}: status_after")
    require(before["state"] == "idle" and before["connected_client_count"] == 0,
            f"{path}: capture did not begin without an owner")
    require(after["state"] == "idle" and after["connected_client_count"] == 1 and
            after["queued_sample_count"] == 0,
            f"{path}: orderly stop did not drain to idle while the capture owner remained connected")
    require(after["uptime_us"] >= before["uptime_us"], f"{path}: uptime regressed")
    for counter in ("producer_drop_count", "output_queue_drop_count"):
        require(after[counter] >= before[counter], f"{path}: {counter} regressed")

    controls = capture["controls"]
    frames = capture["frames"]
    require(isinstance(controls, list) and isinstance(frames, list) and frames,
            f"{path}: controls/frames are missing")
    events: list[tuple[int, float, str, Any]] = []
    parsed_controls = []
    for index, event in enumerate(controls):
        validate_event(event, {"event_index", "received_ms", "direction", "text"},
                       f"{path}: controls[{index}]")
        require(event["direction"] in {"client_to_server", "server_to_client"},
                f"{path}: controls[{index}] invalid direction")
        require(isinstance(event["text"], str), f"{path}: controls[{index}] text is not a string")
        parsed = oracle.validate_control_message(event["text"].encode("utf-8"), event["direction"])
        parsed_controls.append((event["direction"], parsed))
        events.append((event["event_index"], event["received_ms"], "control", parsed))

    expected_control_types = [
        ("client_to_server", "hello"),
        ("server_to_client", "welcome"),
        ("client_to_server", "start_stream"),
        ("server_to_client", "stream_started"),
        ("client_to_server", "stop_stream"),
        ("server_to_client", "stream_stopped"),
    ]
    require([(direction, message["type"]) for direction, message in parsed_controls] ==
            expected_control_types, f"{path}: unexpected control sequence")
    start_request = parsed_controls[2][1]
    started = parsed_controls[3][1]
    stop_request = parsed_controls[4][1]
    stopped = parsed_controls[5][1]
    require(start_request["stream"] == "live-vi" and
            start_request["profile"] == "vi-measurement" and
            start_request["parameters"] == PARAMETERS,
            f"{path}: unexpected start request")
    require(started["stream"] == "live-vi" and started["profile"] == "vi-measurement" and
            started["parameters"] == PARAMETERS,
            f"{path}: unexpected stream_started")
    stream_id = started["stream_id"]
    require(stop_request.get("stream_id") == stream_id and stopped["stream_id"] == stream_id,
            f"{path}: stop stream_id mismatch")

    previous = None
    decoded_frames = []
    data_frames = 0
    sample_count = 0
    valid_voltage = 0
    valid_current = 0
    gap_samples = 0
    producer_gap_frames = 0
    output_gap_frames = 0
    for index, event in enumerate(frames):
        validate_event(event, {"event_index", "received_ms", "hex"}, f"{path}: frames[{index}]")
        hex_value = event["hex"]
        require(isinstance(hex_value, str) and len(hex_value) % 2 == 0,
                f"{path}: frames[{index}] invalid hex")
        try:
            raw = bytes.fromhex(hex_value)
        except ValueError as error:
            raise CaptureError(f"{path}: frames[{index}] invalid hex: {error}") from error
        require(len(raw) <= capabilities["maximum_binary_frame_size"],
                f"{path}: frames[{index}] exceeds advertised maximum")
        decoded = oracle.decode_binary_frame(raw, "vi-measurement",
                                             binary_context(stream_id, 48, previous))
        decoded_frames.append(decoded)
        events.append((event["event_index"], event["received_ms"], "frame", decoded))
        if decoded["stream_end"]:
            require(len(raw) == 32, f"{path}: STREAM_END is not 32 bytes")
            continue
        require(len(raw) == 48 and decoded["sample_count"] == 1,
                f"{path}: data frame is not one 48-byte V/I record")
        valid_mask = decoded["first_valid_mask"]
        voltage_bits, current_bits = struct.unpack_from("<II", raw, 40)
        if valid_mask & 1:
            valid_voltage += 1
        else:
            require(voltage_bits == 0, f"{path}: invalid voltage is not canonical +0.0f")
        if valid_mask & 2:
            valid_current += 1
        else:
            require(current_bits == 0, f"{path}: invalid current is not canonical +0.0f")
        data_frames += 1
        sample_count += decoded["sample_count"]
        gap_samples += decoded["gap_samples"]
        producer_gap_frames += int(decoded["producer_overflow"])
        output_gap_frames += int(decoded["output_queue_drop"])
        previous = {
            "previous_first_sample_sequence": decoded["first_sample_sequence"],
            "previous_sample_count": decoded["sample_count"],
            "previous_first_timestamp_us": decoded["first_timestamp_us"],
            "previous_last_timestamp_us": decoded["first_timestamp_us"] + decoded["last_delta_us"],
        }

    require(data_frames > 0, f"{path}: no data frame")
    require(decoded_frames[0]["stream_start"], f"{path}: first binary frame lacks STREAM_START")
    require(decoded_frames[-1]["stream_end"], f"{path}: final binary frame is not STREAM_END")
    require(sum(int(frame["stream_end"]) for frame in decoded_frames) == 1,
            f"{path}: STREAM_END count is not one")

    events.sort(key=lambda item: item[0])
    require([item[0] for item in events] == list(range(len(events))),
            f"{path}: event_index is not contiguous")
    require(all(events[index][1] <= events[index + 1][1] for index in range(len(events) - 1)),
            f"{path}: event time regressed")
    event_kinds = [(kind, value["type"] if kind == "control" else
                    ("stream_end" if value["stream_end"] else "data"))
                   for _, _, kind, value in events]
    require(event_kinds[:4] == [("control", "hello"), ("control", "welcome"),
                                ("control", "start_stream"), ("control", "stream_started")],
            f"{path}: stream setup event order is wrong")
    require(event_kinds[-2:] == [("frame", "stream_end"), ("control", "stream_stopped")],
            f"{path}: STREAM_END/stream_stopped event order is wrong")
    stop_position = event_kinds.index(("control", "stop_stream"))
    end_position = event_kinds.index(("frame", "stream_end"))
    require(stop_position < end_position,
            f"{path}: STREAM_END was not produced after the stop request")
    require(all(kind == "frame" and frame_type == "data"
                for kind, frame_type in event_kinds[stop_position + 1:end_position]),
            f"{path}: unexpected event while draining after stop request")

    return {
        "path": str(path),
        "stream_id": stream_id,
        "status_before_uptime_us": before["uptime_us"],
        "status_after_uptime_us": after["uptime_us"],
        "data_frames": data_frames,
        "samples": sample_count,
        "valid_voltage_samples": valid_voltage,
        "valid_current_samples": valid_current,
        "gap_samples": gap_samples,
        "producer_gap_frames": producer_gap_frames,
        "output_gap_frames": output_gap_frames,
        "producer_drop_delta": after["producer_drop_count"] - before["producer_drop_count"],
        "output_drop_delta": after["output_queue_drop_count"] - before["output_queue_drop_count"],
    }


def synthetic_capture() -> dict[str, Any]:
    capabilities = {
        "protocol": "d2b-stream", "version": "0.1", "maximum_binary_frame_size": 48,
        "maximum_control_message_size": 2048, "maximum_active_stream_sessions": 1,
        "maximum_control_connections": 1, "persistent_capture_supported": False,
        "security_mode": "unauthenticated-read-only",
        "streams": [{"id": "live-vi", "label": "Voltage and current", "profiles": [{
            "profile": "vi-measurement", "parameter_sets": [PARAMETERS]}]}],
    }
    messages = [
        ("client_to_server", {"type": "hello", "protocol": "d2b-stream", "versions": ["0.1"]}),
        ("server_to_client", {"type": "welcome", "protocol": "d2b-stream", "version": "0.1",
                              "max_control_message_size": 2048, "max_binary_frame_size": 48,
                              "session_state": "ready"}),
        ("client_to_server", {"type": "start_stream", "stream": "live-vi",
                              "profile": "vi-measurement", "parameters": PARAMETERS}),
        ("server_to_client", {"type": "stream_started", "stream": "live-vi",
                              "profile": "vi-measurement", "parameters": PARAMETERS, "stream_id": 7}),
        ("client_to_server", {"type": "stop_stream", "stream_id": 7,
                              "reason": "capture complete"}),
        ("server_to_client", {"type": "stream_stopped", "stream_id": 7,
                              "reason": "client request"}),
    ]
    data = struct.pack("<4sBBBBIIQQIIff", b"D2BS", 0, 1, 2, 1, 7, 1, 10, 1000,
                       0, 3, 1.5, 0.25)
    end = struct.pack("<4sBBBBIIQQ", b"D2BS", 0, 1, 0x10, 2, 7, 0, 11, 1040)
    order = [0, 1, 2, 3, "data", 4, "end", 5]
    controls = []
    frames = []
    for event_index, source in enumerate(order):
        if source in {"data", "end"}:
            frames.append({"event_index": event_index, "received_ms": event_index * 10,
                           "hex": (data if source == "data" else end).hex()})
        else:
            direction, message = messages[source]
            controls.append({"event_index": event_index, "received_ms": event_index * 10,
                             "direction": direction,
                             "text": json.dumps(message, separators=(",", ":"))})
    status = {"protocol": "d2b-stream", "version": "0.1", "state": "idle",
              "connected_client_count": 0, "producer_drop_count": 0,
              "output_queue_drop_count": 0, "queued_sample_count": 0, "uptime_us": 500}
    after = dict(status, connected_client_count=1, uptime_us=2000)
    return {"format": FORMAT, "captured_at": "2026-08-01T00:00:00.000Z",
            "user_agent": "synthetic-self-test", "device_base_url": "http://device/d2b/v0/",
            "duration_seconds": 5,
            "capabilities_text": json.dumps(capabilities, separators=(",", ":")),
            "status_before_text": json.dumps(status, separators=(",", ":")),
            "controls": controls, "frames": frames,
            "status_after_text": json.dumps(after, separators=(",", ":"))}


def run_self_test(oracle) -> None:
    with tempfile.TemporaryDirectory(prefix="d2b-live-capture-") as directory:
        path = pathlib.Path(directory) / "valid.json"
        path.write_text(json.dumps(synthetic_capture()), encoding="utf-8")
        result = validate_one(path, oracle)
        require(result["samples"] == 1 and result["stream_id"] == 7,
                "self-test valid capture decoded unexpectedly")

        invalid = synthetic_capture()
        invalid["frames"][0]["hex"] = invalid["frames"][0]["hex"].replace("44324253", "58324253", 1)
        bad_path = pathlib.Path(directory) / "bad-magic.json"
        bad_path.write_text(json.dumps(invalid), encoding="utf-8")
        try:
            validate_one(bad_path, oracle)
        except oracle.ValidationError as error:
            require(error.code == "bad_magic", f"self-test expected bad_magic, got {error.code}")
        else:
            raise CaptureError("self-test accepted bad binary magic")

        noncanonical = synthetic_capture()
        data = bytearray.fromhex(noncanonical["frames"][0]["hex"])
        struct.pack_into("<I", data, 36, 1)
        struct.pack_into("<I", data, 44, 0x80000000)
        noncanonical["frames"][0]["hex"] = data.hex()
        zero_path = pathlib.Path(directory) / "noncanonical-zero.json"
        zero_path.write_text(json.dumps(noncanonical), encoding="utf-8")
        try:
            validate_one(zero_path, oracle)
        except CaptureError as error:
            require("canonical +0.0f" in str(error),
                    "self-test rejected noncanonical zero for an unexpected reason")
        else:
            raise CaptureError("self-test accepted noncanonical invalid-channel zero")

        duplicate = synthetic_capture()
        duplicate["capabilities_text"] = duplicate["capabilities_text"].replace(
            '{"protocol":"d2b-stream",',
            '{"protocol":"d2b-stream","protocol":"d2b-stream",', 1)
        duplicate_path = pathlib.Path(directory) / "duplicate-capability-key.json"
        duplicate_path.write_text(json.dumps(duplicate), encoding="utf-8")
        try:
            validate_one(duplicate_path, oracle)
        except CaptureError as error:
            require("duplicate object key" in str(error),
                    "self-test rejected duplicate key for an unexpected reason")
        else:
            raise CaptureError("self-test accepted duplicate capability key")
    print("PASS: live-capture validator self-test")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--oracle", required=True, type=pathlib.Path,
                        help="Device-to-Browser-Data-Streaming repository")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("captures", nargs="*", type=pathlib.Path)
    args = parser.parse_args()
    oracle = load_oracle(args.oracle)
    if args.self_test:
        require(not args.captures, "--self-test does not accept capture files")
        run_self_test(oracle)
        return 0
    require(bool(args.captures), "at least one capture is required")
    results = [validate_one(path, oracle) for path in args.captures]
    if len(results) > 1:
        require(len({result["stream_id"] for result in results}) == len(results),
                "reconnect captures reused a stream_id")
        for previous, current in zip(results, results[1:]):
            require(current["status_before_uptime_us"] >= previous["status_after_uptime_us"],
                    "reconnect captures do not prove the same boot")
    print(json.dumps({"result": "PASS", "captures": results}, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except CaptureError as error:
        raise SystemExit(f"FAIL: {error}") from error
