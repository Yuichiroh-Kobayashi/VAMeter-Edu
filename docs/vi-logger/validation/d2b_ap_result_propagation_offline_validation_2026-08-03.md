# D2B V/I AP operation result propagation オフライン検証（2026-08-03）

## 対象、identity、検証境界

- branch: `fix/d2b-vi-ap-result-propagation`
- base SHA / 検証時 HEAD: `b73d853ff80c2c3d4a8a795830b7006883570b5f`
- base tree: `eb6c0b25f8ee7fee659ac8dd1a9dfb364b82f7ac`
- normalized final uncommitted tree identity: `e39b9b8eda9e511f6d0b526d601226de266cda25e6a8e47c6092774452a12dab`
- identity algorithm: artifact の `commit_manifest.txt` を bytewise sort 順に読み、各 repository-relative path、NUL、file bytes、NUL を連結して SHA-256 を計算する。本 validation report 内の上記64桁 digest 自体だけは64個の `0` に正規化する。Git index/tree object は生成していない。
- 実装10ファイルだけの補助 source-tree identity: `7e53464d52af7bb7c25243f142495c846226e08747fc006d3ee4a89d06226a4f`。これは bytewise path 順の `sha256sum` record 群を再度 SHA-256 化した値で、自己参照する本 report は含めない。
- ESP-IDF: `v5.1.6`、Git `7452b1cb1d22cd1b439a6a922548efacea98ee72`
- D2B oracle: `5411ba59a12882345d32218eda367bd6ba35ef5d`
- 対象は AP/STA operation の実戻り値伝播、AP stop-retry state、System/Download start/stop rollback、System UI recovery mapping に限定した。
- status cross-component atomic redesign、Download UI recovery、full transactional stream-start、protocol/schema/vector、frame/measurement/queue、CSV/recorder/waveform、partition/sdkconfig/dependency、AssetPool は変更していない。

## 実装と AP 停止 postcondition

1. allocation-free C++11 の `WEB_SERVER_OWNER::ApOperation` を production HAL と host test が直接共有する。状態は `Inactive`、`Active`、`StopRetryRequired`。
2. AP start 前に STA 接続を確認する。接続中なら `WiFi.disconnect()` の `bool` が false の時点で AP start を行わず `ApStartFailed` とする。
3. `WiFi.softAP(...)` の false 後は `WiFi.AP.started()` を一度確認する。partial AP がなければ clean `ApStartFailed`、partial AP があれば `WiFi.softAPdisconnect(true)` を一度だけ試す。cleanup false は `RetainedApNeedsStopRetry` として保持する。
4. 通常 AP stop は owned state が Active/retry の時に `WiFi.AP.started()` を確認する。false なら `AlreadyStopped` として Inactive へ遷移し、STA disconnect は呼ばない。true なら厳密に `WiFi.softAPdisconnect(true)` を一度だけ呼ぶ。
5. `WiFi.softAPdisconnect(true) == true` が API 上の停止成功 postcondition である。pinned arduino_lite では `AP.clear()` 後に `AP.end()`、すなわち AP 設定消去と AP interface disable request を行う。false は `StopRetryRequired` を保持する。
6. System stop の low-level false は `StopResult::ApStopFailed` へ到達し、Stop Recovery を維持する。HTTPD が `AlreadyStopped` でも次の明示操作は AP stop だけを一度再試行する。
7. AP retry pending 中の新 System start は `RetainedApNeedsStopRetry`、Download start は false で fail-closed となり、AP/HTTPD/owner/Download selection に新しい副作用を加えない。
8. AP stop は `WiFi.disconnect()` を呼ばないため、無関係な有効 STA 接続を無条件には切断しない。

この postcondition は API 戻り値と内部状態に関するものである。物理 AP 消失は hardware acceptance gate のままであり、元の Edge/softAP 症状が解決したとは主張しない。

## 実行コマンドと結果

authoritative raw log は artifact directory の `raw/parent/` と `raw/final/` に保存した。主要コマンドは次のとおり。

```text
cmake -S . -B build/desktop
cmake --build build/desktop -j 2
ctest --test-dir build/desktop -R 'd2b_ap_operation|d2b_stop_failure_recovery|web_server_owner|d2b_httpd_lifecycle|d2b_httpd_send_pump' --output-on-failure
ctest --test-dir build/desktop --output-on-failure

cmake -S . -B <artifact>/raw/final/asan-ubsan -DPLATFORM_BUILD_DESKTOP=OFF -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build <artifact>/raw/final/asan-ubsan -j 2
env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir <artifact>/raw/final/asan-ubsan --output-on-failure
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir <artifact>/raw/final/asan-ubsan --output-on-failure

cmake -S . -B <artifact>/raw/final/tsan -DPLATFORM_BUILD_DESKTOP=OFF -DCMAKE_C_FLAGS='-fsanitize=thread -fno-omit-frame-pointer' -DCMAKE_CXX_FLAGS='-fsanitize=thread -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=thread'
cmake --build <artifact>/raw/final/tsan -j 2
env TSAN_OPTIONS=halt_on_error=1 ctest --test-dir <artifact>/raw/final/tsan --output-on-failure
env TSAN_OPTIONS=halt_on_error=1 <artifact>/raw/final/tsan/tests/d2b_ap_operation/d2b_ap_operation

python3 <oracle>/tools/validate_test_vectors.py
python3 tests/d2b_vi_transport/validate_control_vectors.py build/desktop/d2b_control_cli <oracle>
build/desktop/d2b_capabilities_cli | python3 tests/d2b_vi_transport/validate_capabilities.py <oracle>
python3 tests/d2b_vi_frame_writer/validate_product_frames.py build/desktop/d2b_vi_frame_cli <oracle>
python3 tests/d2b_vi_backpressure/validate_backpressure_frames.py build/desktop/d2b_vi_backpressure_cli <oracle>

source /home/yu-ichirou/esp/esp-idf/export.sh
idf.py --version
CMAKE_BUILD_PARALLEL_LEVEL=2 idf.py -B <artifact>/raw/final/idf-build build
idf.py -B <artifact>/raw/final/idf-build size --format json
idf.py -B <artifact>/raw/final/idf-build size-components
idf.py -B <artifact>/raw/final/idf-build size-files

git diff --check
```

### Desktop、focused、全 CTest

- desktop configure/build: PASS。新しい production helper は desktop `app_layer` と focused target の両方へ組み込まれた。
- focused AP/recovery/owner/lifecycle/pump: 5/5 PASS。
- 全 CTest: 17/17 PASS。
- `d2b_ap_operation` は production helper/results/owner/transaction/recovery source を直接 C++11 linkし、`-Wall -Wextra -Werror` で buildした。
- 指定17ケースは AP start/STA disconnect/start cleanup/stop retry/callback順序/UI mapping/route rollback/normal stop/HTTPD AlreadyStopped/System・Download gate を含む。case 3 は partial AP cleanup success、case 9 は `AP.started()==false` 相当を一度観測し STA callback 0 を直接検査する。

### Sanitizer

- GCC 13.3.0、ASan/UBSan configure/build: PASS。
- `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`、`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`: 12/12 PASS、finding なし。
- LeakSanitizer: 12/12 が test body 開始前に `LeakSanitizer does not work under ptrace (strace, gdb, etc)` で拒否、CTest exit 8。環境制約であり PASS には数えていない。
- TSan instrumented build: PASS。CTest は実行環境の `FATAL: ThreadSanitizer: unexpected memory mapping ...` により 1 pass / 11 failed、exit 8。focused executable の直接実行も同じ fatal、exit 66。環境制約であり PASS には数えていない。

### Oracle と product validators

- oracle: PASS、3 schemas / 95 golden（control 34、capabilities 12、V/I 25、PCM 24）/ 2 negative / 7 mutation。
- product control parser: PASS、25 client-to-server golden。
- product capabilities: PASS。
- product V/I + STREAM_END: PASS。
- product STREAM_START + combined backpressure gap: PASS。
- 初回 product 実行は CLI を誤って `build/desktop/tests/...` と指定し `FileNotFoundError` になった。raw `*_attempt1_path_error.log` を保持し、実在する上記 root build CLI で全4本を再実行して PASS を得た。初回は製品コードを実行しておらず gate には数えていない。

### ESP-IDF authoritative final build/link

- fresh artifact build tree、ESP-IDF `v5.1.6` / `7452b1cb1d22cd1b439a6a922548efacea98ee72`: PASS。
- application 1653/1653、bootloader、partition table、ELF link、binary生成、partition check: PASS。
- final case 3 test-only tightening 後にも同じ build treeで `idf.py build` を再実行し、production input無変更、app/bootloader/partition PASS を確認した。
- app binary SHA-256: `6a19d9fe473bbc2bd78a7c83e54cd519483428a7677a75931856f08c5caca23f`
- ELF SHA-256: `04764d27b0b89710f2094ed906c7f2b5e8211a15cf82e4cb6bffe1667de6b201`
- map SHA-256: `23a51b93572b9e26a1ce70ea5e0347b3b600f82b2239f08d06fd3defd85ee76a`
- build tree、ELF、map、binary は artifact にのみ置き、repository へ追加していない。outer launcher は commit 後に clean commit build を別途行う。

## Resource gate

| 指標 | parent | final | delta / 判定 |
|---|---:|---:|---:|
| app binary | `0x1ac010` (1,753,104 B) | `0x1ac580` (1,754,496 B) | +1,392 B |
| partition remaining | 344,048 B | 342,656 B | -1,392 B |
| partition use | 83.594513% | 83.660889% | 85% review gate未満、90% hard stop未満 |
| total image (`esp_idf_size`) | 1,753,001 B | 1,754,381 B | +1,380 B |
| full `iram0_0_seg` remaining | 273,920 B | 273,920 B | 0 B |
| shared D/IRAM remaining | 199,857 B | 199,857 B | 0 B |
| dedicated 16 KiB static IRAM | 1 B remain | 1 B remain | informational only |

link/partition/full IRAM overflow はない。full IRAM decrease >4,096 B、shared D/IRAM decrease >8,192 B、partition use >=85% の review gate はいずれも発火していない。hard stop 条件もない。

## Dependency pins

- smooth_ui_toolkit: `db66713bf8e275595627e52ed83c3415cb451d84`
- LovyanGFX: `d0beeee9d680c6967926b7593e3f73a907064321`
- mooncake: `52ce99196438ba03706cbb6aca33049520ccba3e`
- ArduinoJson: `36e1eecc7d246d829bc51e61d6ac541893c646a3`
- arduino_lite / arduino-esp32: `b698796ae3e7d9c16843208f6259b5a66b8747e3`
- ESP32Encoder: `ba7f6c6253666ec18b6f35744da1e773038bbe72`
- PsychicHttp: `44948e612ef50730ed0338baba6dc36c42e4768d`

全7件は expected path、real directory、symlink 0、Git-readable、canonical origin、exact HEAD/ref、clean working treeを再監査し 7/7 PASS。初回監査log生成は shell quoting errorで実行対象 path が空になったため exit 2 だった。raw `dependency_pins_attempt1_quoting_error.log` を保持し、quoting修正後に全7件を最初から再実行した。

## Static audit と forbidden scope

- production adapter は `WiFi.disconnect()`、`WiFi.softAP(...)`、`WiFi.AP.started()` の戻り値/状態を消費する。
- production の実 AP stop call は `return WiFi.softAPdisconnect(true);` の厳密な1箇所。`softAPdisconnect(false);` と引数省略 `softAPdisconnect();` は0件。
- 初回 exact-call count はログ文字列内の `softAPdisconnect(true)` も数えたため audit command が exit 1 になった。raw `softapdisconnect_true_audit_attempt1_string_match.log` を保持し、実 call の `return ...;` に絞った監査を再実行して PASS。
- System start/Download start の AP retry gate、System stop の `ApStopFailed`、route/server rollback の retained AP mapping、Download selection 境界を call-site 列挙で確認した。
- `git diff --check`: PASS、出力なし。
- `sdkconfig`、`dependencies.lock`、`repos.json`、partition、D2B schema/frame/measurement/queue、CSV/recorder/waveform、AssetPool、dependency tree の変更なし。

## 未実施、deferred、Git状態

- physical VAMeter、flash、serial、Edge、browser、Wi-Fi、softAP disappearance はタスク禁止および hardware boundary のため未実施。
- 従って original Edge/softAP symptom が修正済みとは主張しない。offline gate は outer commit readiness のみを示す。
- status cross-component atomicity、Download stop UI recovery、full transactional stream-start は nonblocking follow-up として deferred。
- stage、commit、push、PR、Issue、merge、branch/tag/ref/index/worktree mutation は実施していない。HEAD は base SHA のまま。
- outer launcher が指定 message `fix: propagate VAMeter AP operation failures` で commit した後、clean commit build を実施する予定である。
