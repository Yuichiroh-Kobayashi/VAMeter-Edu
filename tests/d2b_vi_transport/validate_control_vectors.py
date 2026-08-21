#!/usr/bin/env python3
import json
import pathlib
import subprocess
import sys


def wire_bytes(vector):
    if "message_hex" in vector:
        return bytes.fromhex(vector["message_hex"])
    message = vector["message"]
    if isinstance(message, str):
        return message.encode("utf-8")
    return json.dumps(message, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: validate_control_vectors.py PRODUCT_CLI ORACLE_REPOSITORY")
    cli = pathlib.Path(sys.argv[1]).resolve()
    vectors_path = pathlib.Path(sys.argv[2]).resolve() / "test-vectors" / "control-messages.json"
    document = json.loads(vectors_path.read_text(encoding="utf-8"))
    checked = 0
    failures = []
    for vector in document["vectors"]:
        if vector["direction"] != "client_to_server":
            continue
        command = [str(cli), wire_bytes(vector).hex()]
        context = vector.get("context")
        if context is not None:
            command += [context["state"], "1" if context["owns_stream"] else "0"]
        actual = subprocess.run(command, check=True, capture_output=True, text=True).stdout.strip()
        expected = "none" if vector["expected_valid"] else vector["expected_error"]
        checked += 1
        if actual != expected:
            failures.append(f"{vector['name']}: expected {expected}, actual {actual}")
    if failures:
        print("FAIL: product parser differs from control golden vectors")
        print("\n".join(failures))
        return 1
    print(f"PASS: product parser matches {checked} client-to-server control golden vectors")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
