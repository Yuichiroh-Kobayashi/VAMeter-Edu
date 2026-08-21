# D2B V/I AP mode reconciliation オフライン検証（2026-08-03）

## 対象、identity、検証境界

- branch: `fix/d2b-vi-ap-mode-reconciliation`
- base SHA / 検証時 HEAD: `95c5eda38da886e5cc3ae6ebfc0bb7d80d2becc2`
- normalized final uncommitted tree identity: `be88368dae9184992fb28ae10d2dee59d933d173e02b920fed65abd2ffdcd780`
- identity algorithm: artifact の `commit_manifest.txt` を bytewise sort 順に読み、各 repository-relative path、NUL、file bytes、NUL を連結して SHA-256 を計算する。本 validation report 内の上記64桁 digest 自体だけは64個の `0` に正規化する。Git index/tree object は生成していない。
- 実装6ファイルだけの補助 source-tree identity: `4d2944a2541633ce8620f6d29239b62c430c1a825ad528ee2c65cfd4620d24b4`。bytewise path 順の `sha256sum` record 群を再度 SHA-256 化し、自己参照する本 report は含めていない。
- ESP-IDF: `v5.1.6`、Git `7452b1cb1d22cd1b439a6a922548efacea98ee72`
- D2B oracle: `5411ba59a12882345d32218eda367bd6ba35ef5d`
- 対象は同期 Wi-Fi mode を authority とする AP start/stop reconciliation、System/Download start preflight、その host 回帰検証に限定した。
- status cross-component atomicity、full transactional stream-start、Download HTTPD stop UI recovery、protocol/schema/frame/measurement/queue、CSV/recorder/waveform、partition/sdkconfig/dependency、AssetPool は変更していない。

## mode と event-bit の契約

1. production の判断 authority は `esp_wifi_get_mode()` の同期結果である。`ESP_OK` の `WIFI_MODE_AP` / `WIFI_MODE_APSTA` は `Enabled`、`WIFI_MODE_NULL` / `WIFI_MODE_STA` は `Disabled`、`ESP_ERR_WIFI_NOT_INIT` は `Disabled`、その他の error または未知 mode は `Unknown` に写像する。
2. pinned arduino_lite の `WiFi.AP.started()` は event group の `STARTED_BIT` を読む。bit は非同期 `WIFI_EVENT_AP_START` / `WIFI_EVENT_AP_STOP` callback で set/clear されるため、同期の開始・停止判断には使用しない。本変更の production decision path から完全に除外した。
3. `softAP()` は pinned source 上 `AP.begin() && AP.create(...)` であり、AP mode が有効になった後に false を返せる。したがって start の戻り値だけではなく、直後の同期 mode を確認する。
4. start preflight の mode が `Enabled` または `Unknown` なら `StopRetryRequired` として fail-closed にし、STA query/disconnect と AP start を行わない。`Disabled` のみ stale internal state を `Inactive` に整合して進める。
5. start 後は `startAp == true` かつ post-mode `Enabled` の場合だけ `Started`。それ以外で mode が `Enabled` / `Unknown`、または true 後に `Disabled` の場合は、1 invocation あたり最大1回だけ `stopAp` を行い、stop true 後の mode が `Disabled` なら clean `StartFailed`、それ以外は `StopRetryRequired` とする。
6. stop pre-mode `Disabled` は `AlreadyStopped`。`Enabled` / `Unknown` は `stopAp` を1回だけ実行し、true 後の post-mode `Disabled` の場合だけ `Stopped`、false または post-mode `Enabled` / `Unknown` は `StopFailed` とする。retry、wait、delay、busy loop はない。
7. AP stop adapter は厳密に `WiFi.softAPdisconnect(true)` の bool を伝播し、AP stop 中に `WiFi.disconnect()` を呼ばない。STA 接続中の start だけが `WiFi.disconnect()` の bool を伝播する。

## System / Download と UI

- internal `Inactive` でも実 mode が `Enabled` の System start は、HTTPD 等の start side effect 前に `RetainedApNeedsStopRetry` となり、既存の System UI recovery mapping により明示 AP stop retry へ誘導される。
- 同じ状態の Download start は record validation、path、selection side effect 前に false を返す。
- HTTPD が `AlreadyStopped` でも AP mode が `Enabled` / `Unknown` なら、明示 stop invocation は AP stop を1回だけ試す。
- D2B-RR-13: reconciliation state と callback 群は既存どおり single application task から直列に呼ばれる前提である。この前提を超える atomicity redesign は deferred。
- D2B-RR-14: Download の HTTPD stop failure に対する専用 UI recovery は別 Issue として deferred。今回も Download start preflight の retained AP block のみを扱う。
- status atomicity と full transactional-start は今回の変更範囲外で、deferred のままである。

## 実行コマンドと結果

authoritative raw log は artifact directory の `raw/parent/` と `raw/final/` に保存した。

### Desktop、focused、全 CTest

- current worktree の `cmake -S . -B build/desktop` と `cmake --build build/desktop -j 2`: PASS。
- focused AP/recovery/owner/lifecycle/pump: 5/5 PASS。
- 全 CTest: 17/17 PASS。
- production helper を直接 link する `d2b_ap_operation`: 29/29 PASS（従来17 regression を含む）。mode と asynchronous event bit は fake 内で独立値として保持した。
- 必須ケースは delayed AP_START、partial start cleanup、delayed AP_STOP、Inactive+Enabled start reject/stop cleanup、Unknown pre-start/stop、start true+Disabled cleanup、stop true+Enabled failure、immediate restart と stale event、APSTA、100 cycles exact count、false-false-true retry、System/Download retained block を網羅する。
- seed worktree に対する別 artifact desktop configure は、root CMake が executable output を seed source 内 `build/desktop` に固定するため read-only source では link できなかった。この試行を PASS とは扱っていない。base の authoritative比較は fresh ESP-IDF build、final desktop gate は current worktree の上記成功 run である。

### Sanitizer

- GCC 13.3.0、ASan/UBSan configure/build: PASS。
- `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`、`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`: 12/12 PASS、finding なし。
- LeakSanitizer: 12/12 が test body 開始前に `LeakSanitizer does not work under ptrace` で拒否、CTest exit 8。環境制約による BLOCKED であり PASS には数えていない。
- TSan instrumented build: PASS。runtime は `FATAL: ThreadSanitizer: unexpected memory mapping` で、CTest 1 pass / 11 failed、exit 8、focused direct exit 66。環境制約による BLOCKED であり PASS には数えていない。

### Oracle と product validators

- oracle: PASS、3 schemas / 95 golden（control 34、capabilities 12、V/I 25、PCM 24）/ 2 negative / 7 mutation。
- product control parser: PASS、25 client-to-server golden。
- product capabilities: PASS。
- product V/I + STREAM_END: PASS。
- product STREAM_START + combined backpressure gap: PASS。

### ESP-IDF v5.1.6 fresh build/link

- parent/base と final を別 fresh artifact build tree で構成。いずれも ESP-IDF `v5.1.6` / `7452b1cb1d22cd1b439a6a922548efacea98ee72`、application 1653/1653、bootloader、partition table、ELF link、binary生成、partition check: PASS。
- final app binary SHA-256: `0207db37b81287c855d96e83042f315a00a6a3fc47fc03375b15c6b773208fd3`
- final ELF SHA-256: `a6cadebe2b2deaeee6a6b30f6aa6e3c366fe2f677d1432057a6a8a5be5ce4a46`
- final map SHA-256: `fe592e5c49d8c4a6fb115f37f905f73aa8abdafbbad68e93250898ed7170dad2`
- build tree、ELF、map、binary は artifact にのみ置き、repository へ追加していない。outer launcher が commit 後に clean commit build を別途実施する。

## Resource gate

| 指標 | parent | final | delta / 判定 |
|---|---:|---:|---:|
| app binary | `0x1ac580` (1,754,496 B) | `0x1ac620` (1,754,656 B) | +160 B |
| partition remaining | 342,656 B | 342,496 B | -160 B |
| partition use | 83.660889% | 83.668518% | 85% review gate未満、90% hard stop未満 |
| total image (`esp_idf_size`) | 1,754,381 B | 1,754,549 B | +168 B |
| Flash code | 1,228,883 B | 1,228,995 B | +112 B |
| Flash rodata | 417,908 B | 417,964 B | +56 B |
| shared D/IRAM used / remaining | 145,999 / 199,857 B | 145,999 / 199,857 B | 0 B |
| full `iram0_0_seg` remaining | 273,920 B (`0x42e00`) | 273,920 B (`0x42e00`) | 0 B |
| dedicated static IRAM used / remaining | 16,383 / 1 B | 16,383 / 1 B | 0 B、informational |

authoritative full `iram0_0_seg` は origin `0x40374000`、length `0x58700`、`_iram_end=0x40389900` であり、remaining は `0x42e00` = 273,920 B、parent/final delta は 0 B である。Dedicated static IRAM 16 KiB は informational であり、authoritative full IRAM gate ではない。link/partition/full IRAM overflow はない。full IRAM decrease >4,096 B、shared D/IRAM decrease >8,192 B、partition use >=85% の review gate は発火していない。partition use >=90% の hard stop もない。

## Dependency pins

- smooth_ui_toolkit: `db66713bf8e275595627e52ed83c3415cb451d84`
- LovyanGFX: `d0beeee9d680c6967926b7593e3f73a907064321`
- mooncake: `52ce99196438ba03706cbb6aca33049520ccba3e`
- ArduinoJson: `36e1eecc7d246d829bc51e61d6ac541893c646a3`
- arduino_lite / arduino-esp32: `b698796ae3e7d9c16843208f6259b5a66b8747e3`
- ESP32Encoder: `ba7f6c6253666ec18b6f35744da1e773038bbe72`
- PsychicHttp: `44948e612ef50730ed0338baba6dc36c42e4768d`

全7件は expected path、real directory、symlink 0、Git-readable、canonical origin、exact HEAD/ref、clean working treeを再監査し 7/7 PASS。

## Static audit と forbidden scope

- decision path に `WiFi.AP.started()` / `isApStarted` は0件。
- `esp_wifi_get_mode()` の `esp_err_t` を検査し、`AP` / `APSTA` を `Enabled`、`NULL` / `STA` と `ESP_ERR_WIFI_NOT_INIT` を `Disabled`、その他を `Unknown` に写像する。
- production の実 AP stop call は `return WiFi.softAPdisconnect(true);` の厳密な1箇所。AP stop adapter 内に `WiFi.disconnect()` はない。
- host test は start/stop callback count を各ケースで検査し、100 cycles で start 100、stop 100、STA query 100、STA disconnect 0、mode query 400 を確認した。
- System/Download start preflight、System UI recovery、HTTPD `AlreadyStopped` 後の AP stop retry の call site を確認した。
- `git diff --check`: PASS、出力なし。
- `sdkconfig`、`dependencies.lock`、`repos.json`、partition、D2B schema/frame/measurement/queue、CSV/recorder/waveform、AssetPool、dependency tree の変更なし。

## 未実施、deferred、Git状態

- physical VAMeter、flash、serial、Edge、browser、Wi-Fi、SSID scan、softAP disappearance はユーザー指定禁止と hardware boundary のため未実施。
- 従って physical AP または元の Edge symptom が修正済みとは主張しない。offline gate は outer commit readiness のみを示す。
- stage、commit、push、PR、Issue、merge、branch/tag/ref/index/worktree mutation は実施していない。HEAD は base SHA のまま。
- outer launcher が指定 message `fix: reconcile VAMeter AP mode state` で commit した後、clean commit build を実施する予定である。
