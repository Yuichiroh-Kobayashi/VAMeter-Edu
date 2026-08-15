# Dependency baseline for PR #1 merge commit

This document records the dependency snapshot verified during the clean build of VAMeter-Edu merge commit `c9bb8ca310b879ca54e83bcea91ff941463d50d1`.

`repos.json` defines requested URLs, paths, and ref names. This table records the exact revisions observed for that successful build. It is reproducibility evidence, not an immutable dependency lock. If a mutable branch resolves to a different commit, do not automatically declare it correct or wrong and do not silently rewind or accept it; stop and request review.

| Dependency | Repository URL | Worktree-relative path | Requested ref | Ref kind | Verified commit |
|---|---|---|---|---|---|
| smooth_ui_toolkit | `https://github.com/Forairaaaaa/smooth_ui_toolkit.git` | `dependencies/smooth_ui_toolkit` | `v1.1.0` | tag | `db66713bf8e275595627e52ed83c3415cb451d84` |
| LovyanGFX | `https://github.com/lovyan03/LovyanGFX.git` | `dependencies/LovyanGFX` | `1.1.12` | tag | `d0beeee9d680c6967926b7593e3f73a907064321` |
| mooncake | `https://github.com/Forairaaaaa/mooncake.git` | `dependencies/mooncake` | `v1.2` | tag | `52ce99196438ba03706cbb6aca33049520ccba3e` |
| ArduinoJson | `https://github.com/bblanchon/ArduinoJson.git` | `platforms/vameter/components/ArduinoJson` | `v7.0.4` | tag | `36e1eecc7d246d829bc51e61d6ac541893c646a3` |
| arduino-esp32 | `https://github.com/Forairaaaaa/arduino_lite.git` | `platforms/vameter/components/arduino-esp32` | `v3.0.0-rc3` | branch | `b698796ae3e7d9c16843208f6259b5a66b8747e3` |
| ESP32Encoder | `https://github.com/Forairaaaaa/ESP32Encoder.git` | `platforms/vameter/components/ESP32Encoder` | `vameter-bk` | branch | `ba7f6c6253666ec18b6f35744da1e773038bbe72` |
| PsychicHttp | `https://github.com/Forairaaaaa/PsychicHttp.git` | `platforms/vameter/components/PsychicHttp` | `vameter-bk` | branch | `44948e612ef50730ed0338baba6dc36c42e4768d` |

## Validation use

Successful completion of `fetch_repos.py` is not dependency identity proof. After it, validate all seven paths independently:

- the path is a real directory and not a cross-worktree symlink;
- the origin URL matches `repos.json`;
- `HEAD` and the requested tag or branch are recorded;
- the dependency working tree is clean;
- any difference from this verified snapshot is reviewed explicitly.

`fetch_repos.py` is currently a simple clone helper. It does not validate existing repositories, is not idempotent, and does not reliably propagate individual clone failures because its subprocess calls do not use `check=True`. Making dependency acquisition idempotent or introducing an immutable lock belongs in a separate tooling PR.

## ESP-IDF managed components

ESP-IDF managed components are materialized per build-capable worktree from the tracked component manifests, including `idf_component.yml`, together with tracked `dependencies.lock` authority.

- Do not share, copy, symlink, bind-mount, or path-substitute `managed_components` across worktrees.
- Validate that the materialized content belongs to the exact source worktree and lock/manifest inputs used for the build.
- A change to a component manifest or `dependencies.lock` is a dependency-authority mutation and requires separate explicit authorization. Do not perform it as an incidental build repair.
