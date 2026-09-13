"""Exercise real AssetPool generation/loading with independent whole-image CRC checks.

Inputs are deterministic synthetic Viewer representations unless --viewer-inputs supplies
four exact stable files (index, manifest, CSS gzip, JS gzip). Outputs stay under the
source build directory for inspection; they are development artifacts, never releases.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import tempfile
import zlib

PARTITION = 2097152
TRAILER = PARTITION - 256
NAMES = ("INDEX", "MANIFEST", "CSS_GZIP", "JS_GZIP")
SIZES = (573, 1364, 2385, 25809)


def validate(data, layout):
    assert len(data) == PARTITION
    t = data[TRAILER:]
    assert t[:8] == b"VAMEAPL1"
    assert struct.unpack_from("<4H", t, 8) == (1, 76, 1, 1)
    assert struct.unpack_from("<3I", t, 16) == (layout["static_asset_size"], layout["webpage_offset"], 5)
    assert [list(struct.unpack_from("<2I", t, 28 + 8*i)) for i in range(5)] == layout["members"]
    assert not any(data[layout["static_asset_size"]:TRAILER])
    assert not any(t[76:])
    assert struct.unpack_from("<I", t, 68)[0] == zlib.crc32(data[:layout["static_asset_size"]])
    assert struct.unpack_from("<I", t, 72)[0] == zlib.crc32(t[:72])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--driver", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--viewer-inputs", type=Path, nargs=4)
    args = parser.parse_args()
    build = args.source_root.resolve() / "build"
    build.mkdir(exist_ok=True)
    dirs = [Path(tempfile.mkdtemp(prefix="asset-pool-" + label + "-", dir=build)) for label in ("A", "B", "cases")]
    driver = str(args.driver.resolve())
    layout = json.loads(subprocess.check_output([driver, "describe"], text=True))
    assert layout["static_asset_size"] == 1655972
    assert [m[1] for m in layout["members"]] == list(SIZES) + [65]
    env = os.environ.copy()
    inputs = []
    for i, (name, size) in enumerate(zip(NAMES, SIZES)):
        path = args.viewer_inputs[i].resolve() if args.viewer_inputs else dirs[2] / (name + ".input")
        if not args.viewer_inputs:
            path.write_bytes(bytes((j * 17 + i) & 255 for j in range(size)))
        assert len(path.read_bytes()) == size
        env["VAMETER_VIEWER_" + name + "_PATH"] = str(path)
        inputs.append(path.read_bytes())

    def run(mode, cwd, expected=0, extra=(), preexec=None, perturb="85"):
        result = subprocess.run([driver, mode, *extra], cwd=cwd, env={**env, "MALLOC_PERTURB_": perturb},
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, preexec_fn=preexec)
        (cwd / (mode + ".log")).write_text(result.stdout)
        assert result.returncode == expected, result.stdout
        return result.stdout

    images = []
    for directory, perturb in zip(dirs[:2], ("85", "170")):
        assert not (directory / "AssetPool-VAMeter.bin").exists()
        output = run("generate", directory, perturb=perturb)
        assert "HOST_CONTAINER_GENERATED_DEFAULTS_PRESERVED" in output
        data = (directory / "AssetPool-VAMeter.bin").read_bytes()
        validate(data, layout)
        for contents, (offset, size) in zip(inputs, layout["members"]):
            assert data[offset:offset+size] == contents
        assert "HOST_CONTAINER_LOADED" in run("load", directory)
        images.append(data)
        print(f"PASS generation {directory} bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()}")
    assert images[0] == images[1]
    subprocess.run(["cmp", str(dirs[0] / "AssetPool-VAMeter.bin"), str(dirs[1] / "AssetPool-VAMeter.bin")], check=True)
    print("PASS independent_generation_A_B_byte_identical_with_distinct_allocator_poison")
    good = images[0]
    cases = {"legacy_short": good[:layout["static_asset_size"]], "short": good[:75], "oversize": good + b"\0"}
    for label, offset in (("bad_magic", TRAILER), ("bad_trailer_crc", TRAILER+72), ("bad_static_crc", 0), ("reserved", PARTITION-1)):
        bad = bytearray(good)
        bad[offset] ^= 1
        cases[label] = bad
    bad = bytearray(good)
    struct.pack_into("<I", bad, TRAILER+16, 0xFFFFFFFF)
    struct.pack_into("<I", bad, TRAILER+72, zlib.crc32(bad[TRAILER:TRAILER+72]))
    cases["declared_size_ffffffff"] = bad
    for label, data in cases.items():
        directory = dirs[2] / label
        directory.mkdir()
        path = directory / "AssetPool-VAMeter.bin"
        path.write_bytes(data)
        output = run("load", directory, expected=1)
        assert "ASSETPOOL_CONTAINER_REJECTED" in output and "missing=0" in output
        assert path.read_bytes() == data and not (directory / "AssetPool-VAMeter.bin.tmp").exists()
        print("PASS desktop_reject_" + label)
    directory = dirs[2] / "not_regular"
    directory.mkdir()
    (directory / "AssetPool-VAMeter.bin").mkdir()
    assert "missing=0" in run("load", directory, expected=1)
    print("PASS desktop_reject_non_regular")
    if os.name == "posix":
        for label, target in (("symlink", dirs[0] / "AssetPool-VAMeter.bin"), ("dangling_symlink", dirs[2] / "absent.bin")):
            directory = dirs[2] / label
            directory.mkdir()
            path = directory / "AssetPool-VAMeter.bin"
            path.symlink_to(target)
            assert "missing=0" in run("load", directory, expected=1)
            assert path.is_symlink()
            print("PASS desktop_reject_" + label)
    directory = dirs[2] / "missing"
    directory.mkdir()
    assert "missing=1" in run("load", directory, expected=1)
    print("PASS desktop_distinguishes_missing")
    directory = dirs[2] / "rename_failure"
    directory.mkdir()
    target = directory / "existing-directory"
    target.mkdir()
    run("dump", directory, expected=1, extra=(str(target),))
    assert target.is_dir() and not Path(str(target)+".tmp").exists()
    print("PASS atomic_rename_failure_cleans_temp")
    if os.name == "posix":
        import resource
        directory = dirs[2] / "write_failure"
        directory.mkdir()
        target = directory / "preserved.bin"
        target.write_bytes(b"existing final must survive")
        def limit_file_size():
            signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE, (1024, 1024))
        run("dump", directory, expected=1, extra=(str(target),), preexec=limit_file_size)
        assert target.read_bytes() == b"existing final must survive"
        assert not Path(str(target)+".tmp").exists()
        print("PASS atomic_write_failure_preserves_final_and_cleans_temp")
    print("PASS whole_container_contract_and_desktop_failure_tests")
    print(json.dumps({"A": str(dirs[0]), "B": str(dirs[1]), "cases": str(dirs[2]),
                      "bytes": len(good), "sha256": hashlib.sha256(good).hexdigest(),
                      "static_asset_sha256": hashlib.sha256(good[:layout['static_asset_size']]).hexdigest()}))


if __name__ == "__main__":
    main()
