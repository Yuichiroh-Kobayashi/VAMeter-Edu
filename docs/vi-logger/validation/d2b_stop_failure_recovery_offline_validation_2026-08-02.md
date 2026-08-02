# D2B V/I stop-failure recovery オフライン検証 (2026-08-02)

この記録は、D2B V/I の停止失敗回復について、実際の production UI/HAL/server-owner 経路、公開 status 判定、publication failure の接続終了 semantics を検証した結果である。実機、flash、serial、Edge、Wi-Fi/softAP の物理試験は実施しておらず、元の Edge/softAP 症状が実機で解消したとは主張しない。

## 対象と同一性

- branch: `fix/d2b-vi-stop-failure-recovery`
- required initial/parent SHA: `ff6af1c9f194f8f31fd9ed91f9a2f0fc3f027719`
- parent branch: `fix/d2b-vi-httpd-stop-serialization`
- Oracle HEAD: `5411ba59a12882345d32218eda367bd6ba35ef5d`
- final normalized tree identity (SHA-256): `f9258eef73b230efd97c009d25569dbfd59df2f52fa8f878419c4887fee35fbb`
- identity algorithm: `commit_manifest.txt` の bytewise-sorted 各 pathについて `path + NUL + bytes + NUL` を連結して SHA-256。本文の identity 値だけは 64 個の `0` に正規化する。
- service tier observation: runtime evidence から確認不能のため `launcher requested fast`

最終 source は意図した未commit差分を含むため、ESP-IDF の application version は `-dirty` である。fresh artifact directory で build したが、clean-source reproducible build provenance とは表現しない。

## 実装した production contract

- `WEB_SERVER_OWNER::StartResult`: `Started`、`BusyOtherOwner`、`RetainedServerNeedsStopRetry`、`AllocationOrListenFailure`、`RouteOrRegistrationFailure`。
- `WEB_SERVER_OWNER::StopResult`: `Stopped`、`AlreadyStopped`、`RetryRequired`、`RejectedWrongOwner`、`ApStopFailed`。
- `WEB_SERVER_OWNER::CleanupPartial()` / `StopOwned()` の allocation-free C++11 transaction helper は、production HAL の partial-listen cleanup と normal/retry stop の双方から呼ばれる。fake callback の stop failure/success、lifecycle callback order、wrapper key、owner release、AP policy を同じ helperで検証する。
- System/Download の preflight は AP start より前、Download は selection publish より前に行う。
- System の retained wrapper/owner は明示 stop retry が成功するまで保持し、新規 System start と AP stop を抑止する。partial-listen/route registration cleanup の stop failure も同じ retained recovery に入る。
- successful `httpd_stop` 後は wrapper を dereference/delete せず、null 化、owner release、AP stop の順に進む。owner release failure は AP を残す fail-closed result とする。
- Network UI は stop result を必ず decision helper に渡す。失敗時は通常 menu へ成功扱いで戻さず、`Retry Stop` または power-cycle guidance をユーザー選択ごとに一度だけ表示する。自動 retry、busy loop、reboot はない。
- public `/status` の既存 field set/endpoint は維持し、Transport session/owner、pipeline、producer、server lifecycle の stream ID、handle、generation がすべて coherent な場合だけ `"state":"streaming"` とする。StopFailed/PreStopping/inactive component は streaming にならない。
- publication failure は rollback 後に production handler が `ESP_FAIL` を返して WebSocket を閉じる。再接続時は新 stream ID。同一 WebSocket の再試行とは扱わない。orderly stop 後の同一接続 2 stream は維持する。

## exact commands

raw log に command line、stdout/stderr、exit status を保存した。主要 command は次の通り。

```text
cmake -S . -B /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/parent/desktop -DBUILD_TESTING=ON -DPLATFORM_BUILD_DESKTOP=ON
cmake --build /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/parent/desktop -j 2
ctest --test-dir /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/parent/desktop --output-on-failure

cmake -S . -B /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/desktop -DBUILD_TESTING=ON -DPLATFORM_BUILD_DESKTOP=ON
cmake --build /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/desktop -j 2
ctest --test-dir /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/desktop --output-on-failure -R 'd2b_stop_failure_recovery|web_server_owner|d2b_httpd_lifecycle|d2b_httpd_send_pump|d2b_vi_transport'
ctest --test-dir /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/desktop --output-on-failure

cmake -S . -B /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/asan-ubsan -DPLATFORM_BUILD_DESKTOP=OFF -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/asan-ubsan -j 2
env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/asan-ubsan --output-on-failure
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --test-dir /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/asan-ubsan --output-on-failure

cmake -S . -B /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/tsan -DPLATFORM_BUILD_DESKTOP=OFF -DCMAKE_C_FLAGS='-fsanitize=thread -fno-omit-frame-pointer' -DCMAKE_CXX_FLAGS='-fsanitize=thread -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=thread'
cmake --build /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/tsan -j 2
env TSAN_OPTIONS=halt_on_error=1 ctest --test-dir /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/tsan --output-on-failure
env TSAN_OPTIONS=halt_on_error=1 ctest --test-dir /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/tsan --output-on-failure -R recorder_followup

python3 tools/validate_test_vectors.py
python3 tests/d2b_vi_transport/validate_control_vectors.py build/desktop/d2b_control_cli /home/yu-ichirou/Dev/Device-to-Browser-Data-Streaming
build/desktop/d2b_capabilities_cli | python3 tests/d2b_vi_transport/validate_capabilities.py /home/yu-ichirou/Dev/Device-to-Browser-Data-Streaming
python3 tests/d2b_vi_frame_writer/validate_product_frames.py build/desktop/d2b_vi_frame_cli /home/yu-ichirou/Dev/Device-to-Browser-Data-Streaming
python3 tests/d2b_vi_backpressure/validate_backpressure_frames.py build/desktop/d2b_vi_backpressure_cli /home/yu-ichirou/Dev/Device-to-Browser-Data-Streaming

source /home/yu-ichirou/esp/esp-idf/export.sh
idf.py --version
CMAKE_BUILD_PARALLEL_LEVEL=2 idf.py -B /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/idf-build build
idf.py -B /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/idf-build size
idf.py -B /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/idf-build size-components
idf.py -B /home/yu-ichirou/Dev/artifacts/VAMeter-Edu/d2b-vi-rev2/20260802T223303+0900-stop-failure-recovery/raw/final_rev3/idf-build size-files

git diff --check
```

Fresh review 1 補正で実際に実行した追加コマンド（共有 worktree、raw artifact ではない）:

```text
cmake -S . -B /tmp/vameter-d2b-build -DBUILD_TESTING=ON -DPLATFORM_BUILD_DESKTOP=OFF
cmake --build /tmp/vameter-d2b-build -j 2
ctest --test-dir /tmp/vameter-d2b-build --output-on-failure -R 'd2b_stop_failure_recovery|web_server_owner|d2b_httpd_lifecycle|d2b_vi_transport'
git diff --check

cmake -S . -B /tmp/vameter-desktop-build -DBUILD_TESTING=ON -DPLATFORM_BUILD_DESKTOP=ON
cmake --build /tmp/vameter-desktop-build -j 2
ctest --test-dir /tmp/vameter-desktop-build --output-on-failure
```

`clang-format` は環境に存在せず `command not found` だった。package install は行っていない。

## 検証結果

| Gate | 結果 | evidence |
|---|---|---|
| parent desktop | PASS | 15/15、`raw/parent/ctest_all.log` |
| parent ESP-IDF v5.1.6 | PASS | app/bootloader/partition/ELF link、`raw/parent/idf_build.log` |
| final desktop configure/build | PASS | `raw/final_rev3/desktop_configure.log`、`desktop_build.log` |
| focused recovery/lifecycle/pump/transport | PASS | 5/5、`raw/final_rev3/ctest_focused.log` |
| all CTest | PASS | 16/16、`raw/final_rev3/ctest_all.log` |
| ASan/UBSan | PASS | 11/11、finding なし、`detect_leaks=0` |
| LeakSanitizer | ENVIRONMENT BLOCKED | 11/11 が test 実行前に `LeakSanitizer does not work under ptrace`。PASS に数えない |
| ThreadSanitizer | ENVIRONMENT BLOCKED | 2 pass/9 fail。単独再実行を含め `FATAL: ThreadSanitizer: unexpected memory mapping`。PASS に数えない |
| Oracle | PASS | 3 schemas / 95 golden (34 control, 12 capabilities, 25 V/I, 24 PCM) / 2 negative / 7 mutation |
| product client parser | PASS | client-to-server 25 vectors、capabilities、V/I、STREAM_END、backpressure/gap |
| final ESP-IDF v5.1.6 | PASS | app/bootloader/partition/ELF link、`raw/final_rev3/idf_build.log` |
| dependency pins | PASS | 7/7 exact、real directory、symlink 0、clean |
| `git diff --check` | PASS | output なし |
| static production call-site audits | PASS | `raw/final_rev3/*audit.log` と `*callsites.log` |
| forbidden scope audit | PASS | config/dependency/partition/AssetPool/recorder/waveform diff なし |
| physical/flash/serial/Edge/Wi-Fi | NOT RUN | task の physical test 禁止と hardware boundary |

ASan/UBSan、LSan、TSan の raw stdout/stderr と exact environment error は artifact に保存した。環境で開始不能だった LSan/TSan は成功扱いにしていない。

## Acceptance A–F coverage

1. start success相当 → production `StopOwned()` callback failure → typed retry/UI decision → AP stop count 0: `d2b_stop_failure_recovery_test` fake callback。
2. explicit retry → callback success → wrapper key 0 → lifecycle callback → owner release → shared `IsStopSuccessful()` AP policyでAP stop count 1。
3. transaction success後の`StartPreflight`/acquire succeeds。
4. partial non-null handle → production `CleanupPartial()` callback failure → retained → production explicit-retry `StopOwned()` success → wrapper key 0/owner None/AP cleanup。
5. `BusyOtherOwner` does not stop AP: preflight/AP policy test。
6. no-retained allocation/listen failure cleans AP: start result/AP cleanup test。
7. wrong-owner stop rejection: owner lifecycle test。
8. repeated failure/retry cycles: deterministic recovery test。
9. StopFailed status is not streaming: public status helper test。
10. actual streaming requires Transport+pipeline+producer+lifecycle: coherent snapshot matrix test。
11. publication rollback → connection close: production disposition と Session test。
12. reconnect uses a new stream ID: production Session reconnect test。
13. same connection orderly two-stream: production Session reuse test。
14. `/syscfg` と `/download` ownership: route/owner regression tests と static call-site audit。
15. result branch lock/owner/AP invariants: owner/recovery result matrix と static audit。

Network UI の `StartWebServer`/`StopWebServer` 全 call site は static audit し、Network UI が stop result を無視しないことを確認した。transaction helperはallocation-free pure C++で、production HALのpartial/normal/retry stopから実際に呼ばれる。focused testはその `.cpp` を直接linkし、手動`markRetained()`/`release()`だけでAcceptance 1–4を成立させていない。

## dependency pins

| dependency | HEAD |
|---|---|
| smooth | `db66713bf8e275595627e52ed83c3415cb451d84` |
| LovyanGFX | `d0beeee9d680c6967926b7593e3f73a907064321` |
| mooncake | `52ce99196438ba03706cbb6aca33049520ccba3e` |
| ArduinoJson | `36e1eecc7d246d829bc51e61d6ac541893c646a3` |
| arduino-esp32 | `b698796ae3e7d9c16843208f6259b5a66b8747e3` |
| ESP32Encoder | `ba7f6c6253666ec18b6f35744da1e773038bbe72` |
| PsychicHttp | `44948e612ef50730ed0338baba6dc36c42e4768d` |

## ESP-IDF と resource evidence

- ESP-IDF: v5.1.6、Git `7452b1cb1d22cd1b439a6a922548efacea98ee72`
- parent app: 1,751,200 B (`0x1ab8a0`)、partition remaining 345,952 B、use 83.503723%
- final app: 1,753,104 B (`0x1ac010`)、partition remaining 344,048 B (`0x53ff0`)、use 83.594513%
- delta: app +1,904 B、remaining -1,904 B、use +0.090790 point
- full `iram0_0_seg` remaining: parent/final 273,920 B、delta 0 B
- shared D/IRAM remaining: 199,865 B → 199,857 B、delta -8 B
- dedicated static IRAM remaining: 1 B → 1 B、informational
- review-required threshold: full IRAM decrease >4,096 B、shared D/IRAM decrease >8,192 B、partition use >=85% のいずれも未発火
- hard stop: link/partition/full IRAM overflow なし、partition use <90%
- final app bin SHA-256: `70c92796b07858c8b62c1975933140d13a01c054bd479edd06977e25f369e520`
- final ELF SHA-256: `f2731e9725260e81dfe7979500152feeb24c4b751ef6436b33746bbe6241bd07`
- final map SHA-256: `1a5f1751b561e789fee9ca715163adca46c33d85fac9cdd6ab698b048590a1c5`

raw ELF/map/build tree/bin は artifact directory のみに置き、tracked source と commit manifest には含めていない。

## external findings disposition

- D2B-RR-03: production UI/HAL typed result propagation、retained state、明示 retry、power-cycle guidance を実装。オフライン gate は PASS。ただし物理症状の解消は未確認。
- D2B-RR-04: public streaming 判定を全 data-path/lifecycle coherence 条件へ狭めた。
- D2B-RR-05: same-connection retry の記述を除去し、rollback後の handler failure/connection close、reconnect/new stream ID、orderly same-connection two-stream を分離して検証した。
- D2B-RR-06: tracked report は raw ELF/map/build tree を含めず、hash と resource evidence のみ記録する。raw evidence は commit 対象外 artifact に保存した。

## fresh reviewer

Fresh review 1 は production code の静的 semantics を支持したが、最初の focused test が transaction callback を実行していない点と報告不一致を blocking とした。この指摘は production `CleanupPartial()` / `StopOwned()`、wrapper-clear callback、production-linked failure/retry test、rev3再検証で是正した。reviewer 1 はユーザー指定外の `fix-first` を返したため最終 verdict としては採用しない。before/after hash一致、mutationなしはartifact `raw/review1_*` に保存した。

Fresh reviewer 2 は是正後のactual diff、rev3 raw evidence、tracked reportを精査し、blocking findingなしで `ship` と判定した。以下にそのruntimeとmutation gateを記録する。この更新後の完全sourceは別のfresh Sol reviewer 3が再監査する。

- role: `sol_advisor_sol_reviewer`
- thread: `019fc301-9bab-70a1-b4f2-c50ee31ddfe3`
- model: `gpt-5.6-sol`
- reasoning effort: `high`
- service tier observation: runtimeから確認不能のため `launcher requested fast`
- sandbox: `workspace-write` / managed、behaviorally read-only
- behaviorally read-only before/after state: diff SHA-256 `cfc482a8d4c86c180ed5b232d6e4adf260c5d6ce5fad4fbfdeecc97bd7e67b43`、manifest path+bytes SHA-256 `47852ada4937af558c7a5798952b6ee942e8b4a7e0e0520f93f8868e87cc069a` が一致、mutationなし
- verdict: `ship`

## 未実施、変更していない状態、残存 risk

- 物理 VAMeter、flash、serial、Edge、Wi-Fi/softAP、実 HTTP client/browser は未実施。
- commit、stage、push、branch/ref/worktree、Issue、PR、merge、remote、実機状態は変更していない。
- full transactional stream start（response-after-send / before-activation stop window）は別 hardening issue として deferred。今回実装していない。
- Download owner と System owner は分離を維持した。Download UI の新しい recovery surface は scope 外で追加していない。
- 事前監査中に read-only identity を得る意図で `git write-tree` を一度入力したが、`.git/index.lock: Read-only file system` で即時失敗した。index/ref/object/worktree の変化はなく、その後は filesystem bytes と read-only Git command のみを同一性根拠にした。
