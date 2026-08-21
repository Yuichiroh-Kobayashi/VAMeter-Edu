#!/usr/bin/env python3
import importlib.util
import pathlib
import subprocess
import sys


PARAMETERS = {
    "sample_format": "vi-f32le",
    "channel_count": 2,
    "channel_mask": 3,
    "sample_rate": {"numerator": 0, "denominator": 0},
}


def context(previous=False):
    result = {
        "negotiated_version": "0.1",
        "session_state": "STREAMING",
        "maximum_binary_frame_size": 48,
        "stream_id": 11,
        "profile": "vi-measurement",
        "parameters": PARAMETERS,
    }
    if previous:
        result.update(
            previous_first_sample_sequence=100,
            previous_sample_count=1,
            previous_first_timestamp_us=1000000,
            previous_last_timestamp_us=1000000,
        )
    return result


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: validate_backpressure_frames.py PRODUCT_CLI ORACLE_REPOSITORY")
    frames = subprocess.run([sys.argv[1]], check=True, capture_output=True, text=True).stdout.splitlines()
    if len(frames) != 2:
        raise RuntimeError("product backpressure CLI returned wrong line count")

    oracle_path = pathlib.Path(sys.argv[2]).resolve() / "tools" / "validate_test_vectors.py"
    spec = importlib.util.spec_from_file_location("d2b_oracle_validator", oracle_path)
    oracle = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(oracle)

    first = oracle.decode_binary_frame(bytes.fromhex(frames[0]), "vi-measurement", context())
    gap = oracle.decode_binary_frame(bytes.fromhex(frames[1]), "vi-measurement", context(previous=True))
    if not first["stream_start"] or first["first_sample_sequence"] != 100:
        raise RuntimeError("prepared first frame decoded unexpectedly")
    if gap["gap_samples"] != 4 or not gap["discontinuity"] or not gap["producer_overflow"] or not gap["output_queue_drop"]:
        raise RuntimeError("prepared gap frame decoded unexpectedly")
    print("PASS: oracle decoder accepts product STREAM_START and combined backpressure gap")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
