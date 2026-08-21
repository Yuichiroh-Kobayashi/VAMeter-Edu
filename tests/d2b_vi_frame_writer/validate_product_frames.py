#!/usr/bin/env python3
import importlib.util
import pathlib
import subprocess
import sys


VI_PARAMETERS = {
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
        "parameters": VI_PARAMETERS,
    }
    if previous:
        result.update(
            previous_first_sample_sequence=100,
            previous_sample_count=2,
            previous_first_timestamp_us=1000000,
            previous_last_timestamp_us=1001000,
        )
    return result


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: validate_product_frames.py PRODUCT_FRAME_CLI ORACLE_REPOSITORY")
    output = subprocess.run([sys.argv[1]], check=True, capture_output=True, text=True).stdout.splitlines()
    if len(output) != 3:
        raise RuntimeError("product frame CLI returned wrong line count")
    oracle_path = pathlib.Path(sys.argv[2]).resolve() / "tools" / "validate_test_vectors.py"
    spec = importlib.util.spec_from_file_location("d2b_oracle_validator", oracle_path)
    oracle = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(oracle)

    first = oracle.decode_binary_frame(bytes.fromhex(output[0]), "vi-measurement", context())
    if not first["stream_start"] or first["first_valid_mask"] != 3 or first["first_voltage"] != 3.25:
        raise RuntimeError("first product frame decoded unexpectedly")
    end = oracle.decode_binary_frame(bytes.fromhex(output[1]), "vi-measurement", context(previous=True))
    if not end["stream_end"] or end["sample_count"] != 0:
        raise RuntimeError("STREAM_END product frame decoded unexpectedly")
    canonical = oracle.decode_binary_frame(bytes.fromhex(output[2]), "vi-measurement", context())
    if canonical["first_valid_mask"] != 1 or canonical["first_current"] != 0.0:
        raise RuntimeError("invalid channel was not canonical zero")
    print("PASS: oracle decoder accepts product V/I and STREAM_END frames")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
