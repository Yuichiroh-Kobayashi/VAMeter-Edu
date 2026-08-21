#!/usr/bin/env python3
import importlib.util
import json
import pathlib
import sys


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: validate_capabilities.py ORACLE_REPOSITORY")
    oracle = pathlib.Path(sys.argv[1]).resolve()
    validator_path = oracle / "tools" / "validate_test_vectors.py"
    spec = importlib.util.spec_from_file_location("d2b_oracle_validator", validator_path)
    validator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(validator)
    document = json.load(sys.stdin)
    validator.validate_capabilities(document)
    print("PASS: product capabilities accepted by oracle validator")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
