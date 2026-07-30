# Build and validation guidance

## Baselines

- Device target: M5Stack VAMeter / ESP32-S3.
- ESP-IDF: v5.1.6.
- The root default is C++11.
- A desktop dependency target requests C++17 compile features. Inspect the edited target and transitive compile features; do not infer one project-wide standard or unify all targets on C++17 without an explicit build-system change.
- Formatting: repository `.clang-format`.

Do not install packages or change the toolchain without explicit user approval.

## Dependency acquisition

From a clean worktree root:

```bash
mkdir -p platforms/vameter/components
python3 ./fetch_repos.py
```

Dependency URLs, paths, and requested ref names are defined by `repos.json`. The verified PR #1 merge-commit snapshot is recorded in [`dependency-baseline.md`](dependency-baseline.md); it is evidence, not an immutable lock.

The current script is a simple clone helper, not a package manager:

- existing destinations are not validated or updated;
- individual `git clone` failure may not produce a nonzero Python exit;
- it is not safe to treat exit status alone as success;
- it is not intended for repeated idempotent runs.

Validate every dependency after the script:

```bash
git -C <dependency-path> remote get-url origin
git -C <dependency-path> rev-parse HEAD
git -C <dependency-path> status --porcelain
```

Also confirm the expected tag/branch from `repos.json`, real directories, and no cross-worktree symlinks.

## Desktop build and host tests

Preferred reproducible commands:

```bash
cmake -S . -B build/desktop
cmake --build build/desktop -j 2
ctest --test-dir build/desktop --output-on-failure
```

Relevant host test directories:

- `tests/local_csv_download/`
- `tests/recorder_followup/`

Host tests should cover pure naming, parser, selection, lifecycle, and Auto-scale behavior where possible.

A desktop PASS does not prove:

- VAMeter filesystem timing;
- physical encoder/button input;
- display readability;
- real voltage/current values;
- USB MSC behavior;
- Wi-Fi AP behavior;
- ESP32 task timing.

## ESP-IDF build

Activate an ESP-IDF v5.1.6 environment first. Do not assume a user-specific installation path.

Verify:

```bash
idf.py --version
```

Then:

```bash
cd platforms/vameter
CMAKE_BUILD_PARALLEL_LEVEL=2 idf.py build
```

A clean-build claim requires:

- clean source worktree before build;
- expected ESP-IDF version;
- successful application, bootloader, and partition generation/link;
- no partition overflow;
- no unintended tracked changes in:
  - `CMakeLists.txt`;
  - `sdkconfig`;
  - `dependencies.lock`.

Do not run `idf.py fullclean`, delete user build outputs, or recreate dependencies unless the task explicitly authorizes cleanup.

## AssetPool

Localization or static asset structure changes can require a new AssetPool.

Desktop configure/build and desktop application execution are separate operations. A successful build alone does not prove that an AssetPool was generated. The desktop application first tries to load an existing `AssetPool-VAMeter.bin`; when it succeeds, it does not create a fresh pool. The usual execution-directory path is:

```text
build/desktop/AssetPool-VAMeter.bin
```

Rules:

- keep AssetPool out of Git;
- do not unconditionally delete an existing pool;
- before regeneration, record the existing file's size, modification time, SHA256, source/provenance if known, and preserve it under the appropriate Artifact or Archive location;
- use a generation directory where absence of `AssetPool-VAMeter.bin` has been explicitly confirmed, then run the desktop application from that directory;
- confirm the new file exists and record its size, modification time, SHA256, and source commit;
- confirm that the desktop application reports successful AssetPool output;
- if exact log strings are used in an automated check, verify them against the dependency revision recorded in `dependency-baseline.md`;
- do not assume a firmware build automatically proves the correct AssetPool was flashed;
- firmware flash and AssetPool partition write are separate device mutations;
- `platforms/vameter/upload_asset_pool.sh` is only a convenience script: it fixes the port to `/dev/ttyACM0` and uses a relative input path. A human must verify the current device port, resolved input path, partition table, and SHA256 before execution;
- firmware and AssetPool device flashing require explicit authorization;
- validate UI/localization on physical hardware after flashing.

Prefer host-independent commands and explicitly discovered paths in operation plans. Do not execute either firmware flash or partition write without exact user authorization.

## Physical-device validation matrix

### Measurement/UI changes

Validate on a physical VAMeter:

- normal boot;
- target screen rendering;
- voltage-only/current-only/both behavior as relevant;
- staged Auto scale;
- controls and exit behavior;
- range-specific HAL handling for negative current and confirmation that plot clipping does not further rewrite the processed HAL/CSV value.

### Recorder/CSV changes

Validate:

- manual 10-second record;
- mode-specific blank columns;
- first timestamp near zero;
- for the current manual-trigger, nominal 40 ms, no-pretrigger path,
  timestamps start near zero and are strictly increasing;
- for other sampling modes or pretrigger use, validate the documented
  duplicate-timestamp policy rather than assuming unique milliseconds;
- final elapsed time near 10 seconds;
- REC numbering;
- failure behavior;
- Files preview/delete;
- MSC transfer;
- QR/AP download where affected.

### Storage/filesystem changes

Validate:

- sufficient-space recording;
- insufficient-space rejection;
- no reboot/format/auto-delete;
- incomplete-file cleanup;
- power interruption or fault injection only with an explicit safety plan.

### Sampling architecture changes

For Issue #3, validate with physical records and analyze inter-sample gaps. Do not infer timing quality from row count or desktop tests alone.

## Minimum task report

State:

- exact branch/worktree and commit tested;
- dependency acquisition/verification result;
- desktop configure/build/test result;
- ESP-IDF version and build result;
- hardware and firmware/AssetPool provenance;
- tests not run;
- known limitations.
