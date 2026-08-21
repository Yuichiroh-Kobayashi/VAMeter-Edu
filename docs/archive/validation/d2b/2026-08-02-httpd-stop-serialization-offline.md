# D2B HTTPD stop serialization オフライン検証（2026-08-02）

## 対象、identity、境界

- branch: `fix/d2b-vi-httpd-stop-serialization`
- parent SHA: `fe8292502e10efae2bef9f899ca5e23347b58c2e`
- final uncommitted tree identity (normalized): `009214e6b7d292a74915c14a263ff3c80d8412fd1b02ccd2435cda36015b3582`。
- identity algorithm: `commit_manifest.txt` の bytewise sort 順に、repository-relative path、NUL、file bytes、NUL を連結して SHA-256 を計算する。ただし本 validation report の上記64桁 digest 自体は64個の `0` に正規化する。
- ESP-IDF: `v5.1.6`、Git `7452b1cb1d22cd1b439a6a922548efacea98ee72`
- oracle: `5411ba59a12882345d32218eda367bd6ba35ef5d`
- 対象は D2B V/I HTTPD shutdown lifecycle のオフライン修正に限定した。
- protocol/schema、32-byte envelope、V/I record、sequence/timestamp/gap、queue depth/drop-oldest、measurement、CSV/recorder、UI、calibration、partition/sdkconfig、dependency revision は変更していない。

## 実装と停止順序

1. allocation-free/C++11 の `D2B_HTTPD_LIFECYCLE::State` を production pipeline と host test の両方で使用する。
2. `onClose` を `listen()` より前、すなわち HTTPD task 起動前に設定する。全 D2B route 登録後だけ lifecycle を running へ commit し、commit failure は登録済み route を rollback する。
3. active server lifetime 中の Transport `_owner`、`_session`、`_buffer`、`_violations`、`_streamIdCounter`、`_generationCounter` の変更を HTTPD task に限定する。application/UI の pre-stop は `Transport::close/stop` を呼ばず、pipeline の lifecycle gate へ直接入る。
4. stop は wrapper が有効な間に raw HTTPD handle を captureし、共通 send mutex を取得後、新規 activation 禁止、current pump invalidate、Producer abort、pipeline owner/stream/output clear を完了してから mutex を解放する。Close、server-stop、send failure は従来どおり connection pump を invalidate する。
5. `StartStream` は同じ mutex の保持中に pipeline activation、`Producer::Start`、Transport session/stream-id publication callback を完了する。publication failure は Producer abort と pipeline rollback を同じ区間で行うが、owner と connection-lifetime pump は active/idle のまま retry に再利用する。
6. orderly stream completion は実際の `d2b_httpd_send_pump_state::finishOrderlyStream` で executing token を Idle に遷移させ、connection pump を invalidate しない。従って同一 WebSocket/generation の `STREAMING → READY → STREAMING` を許し、次の queue_work を Accepted にする。
7. capture 済み handle で `httpd_stop` を呼ぶ。HTTPD task の close callback は通常の Transport close を実行する。成功後は `_http_server` を直ちに null 化し、wrapper を参照せず、HTTPD API を使わない success-only sanity cleanup だけを行う。
8. `httpd_stop` failure は PsychicHttp wrapper、web server owner、lifecycle key を保持する。activation は禁止したまま `StopFailed` とし、次回 stop を deterministic retry として扱う。
9. PsychicHttp wrapper 自体は明示 delete しない。pinned PsychicHttp の `PsychicEndpoint` は handler を破棄しないため、既存派生 destructor の endpoint/default handler cleanup は保持する。

## 実行コマンドと結果

### Desktop と focused/all CTest

- `cmake -S . -B <artifact>/raw/final_r2/desktop`: PASS。
- `cmake --build <artifact>/raw/final_r2/desktop -j 2`: PASS。`app_desktop_build` を含む。
- `ctest --test-dir <artifact>/raw/final_r2/desktop --output-on-failure -R 'd2b_httpd_lifecycle|d2b_httpd_send_pump|d2b_vi_transport|d2b_vi_producer|d2b_vi_backpressure'`: 5/5 PASS。
- `ctest --test-dir <artifact>/raw/final_r2/desktop --output-on-failure`: 15/15 PASS。

focused lifecycle test は production と同一の lifecycle helper と send-pump helper をリンクし、application pre-stop、response 後 activation 前 stop、旧 Producer start 割込み点、pending/executing pump、同一 WebSocket の orderly stream 2本再利用、publication failure→retry start、disconnect/reconnect、fd ABA、反復 start/stop、shutdown queue reject、httpd_stop failure/retry、反復 cleanup、全 fault 後 invariant、全 branch mutex release を決定論的に検査した。send-pump test は `finishOrderlyStream` を直接検査する。fault matrix は queue_work accepted/reject/delayed、send success/failure/invalid-fd、close callback before/during/after、producer start/abort、停止後 send/queue_work 0、完全停止 snapshot を含む。

### Sanitizer

- GCC 13.3.0、`-fsanitize=address,undefined -fno-omit-frame-pointer` build: PASS。
- `ASAN_OPTIONS=detect_leaks=1`: 実行環境の ptrace 制約により LeakSanitizer が `LeakSanitizer does not work under ptrace` で拒否。PASS には数えていない。
- 同一 binary を `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`、`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` で実行: 1/1 PASS、ASan/UBSan finding なし。
- `-fsanitize=thread` build: PASS。
- TSan 実行: `FATAL: ThreadSanitizer: unexpected memory mapping ...` で環境拒否。PASS には数えていない。

### Oracle と product vectors

- `python3 <oracle>/tools/validate_test_vectors.py`: PASS、3 schemas、95 golden（control 34、capabilities 12、V/I 25、PCM 24）、2 negative self-tests、7 mutation tests。
- product control parser: 25 client-to-server golden PASS。
- product capabilities: oracle validation PASS。
- product V/I/STREAM_END frame: oracle decode PASS。
- product STREAM_START/backpressure gap frame: oracle decode PASS。

### ESP-IDF authoritative final build

- `CMAKE_BUILD_PARALLEL_LEVEL=2 idf.py -B <artifact>/raw/final_r2/idf-build build`: PASS。
- application、bootloader、partition生成、ELF link、binary生成、partition check: PASS。
- exact version: ESP-IDF `v5.1.6`、Git `7452b1cb1d22cd1b439a6a922548efacea98ee72`。
- app binary SHA-256: `791f7fad679e32459df26434c731d805a9a8270c642e2a194745522c408a261f`
- ELF SHA-256: `f44d980b2527deb5f99a570d351155b1f5aebef1e002a740af1996e831e80d71`
- map SHA-256: `0ce5b44fbcebde398039a6d54fa21caefb12cfc4c94a42e29568adfbf93fa2ff`
- build は artifact directory 内の新規 build tree を使用し、Git へ追加しない。
- 初回 fresh review 前の `raw/final/` build は round 1 履歴としてのみ保持し、blocking regression 修正後の最終 gate には使用しない。

## Resource gate

| 指標 | parent | final | delta / 判定 |
|---|---:|---:|---:|
| app binary | `0x1ab200` (1,749,504 B) | `0x1ab8b0` (1,751,216 B) | +1,712 B |
| partition remaining | `0x54e00` (347,648 B) | `0x54750` (345,936 B) | -1,712 B |
| partition use | 83.4229% | 83.5045% | 85% review gate未満、90% hard stop未満 |
| full `iram0_0_seg` remaining | `0x42e00` (273,920 B) | `0x42e00` (273,920 B) | 0 B |
| shared D/IRAM remaining | 199,873 B | 199,865 B | -8 B |
| dedicated 16 KiB static IRAM | 1 B remain | 1 B remain | informational only |

link/partition/full IRAM overflow はない。full IRAM decrease >4,096 B、shared D/IRAM decrease >8,192 B、partition use >=85% の review gate はいずれも発火していない。

## Dependency pins

- smooth_ui_toolkit: `db66713bf8e275595627e52ed83c3415cb451d84`
- LovyanGFX: `d0beeee9d680c6967926b7593e3f73a907064321`
- mooncake: `52ce99196438ba03706cbb6aca33049520ccba3e`
- ArduinoJson: `36e1eecc7d246d829bc51e61d6ac541893c646a3`
- arduino-esp32: `b698796ae3e7d9c16843208f6259b5a66b8747e3`
- ESP32Encoder: `ba7f6c6253666ec18b6f35744da1e773038bbe72`
- PsychicHttp: `44948e612ef50730ed0338baba6dc36c42e4768d`

全7件は real directory、symlink 0、Git-readable、canonical origin、exact HEAD、clean を再監査した。network DNS 制約で通常 fetch が失敗したため、preflight で監査済みの独立ローカル clone を使用した。authoritative pin log は `raw/final_r2/dependency_pins_corrected.log` であり、先行 `dependency_pins.log` の `arduino` path error は監査コマンドの path typo で、corrected log で全7件を再実行した。

## Static audit と Git gate

- Transport mutation-site audit: mutation は constructor、HTTPD receive/open/close、StartStream publication callback、successful `httpd_stop` 後の sanity cleanup に限定。application pre-stop から Transport field mutationなし。
- direct-send audit: synchronous control response は HTTPD receive 内。async V/I send と `queue_work` は send mutex/pump token/lifecycle gate 配下。pre-stop 後の新規 schedule と stopped handle send を許す別経路なし。
- stop-call graph audit: UI/HAL → `StopOwnedHttpServer` → raw handle capture → `PrepareServerStop` → `httpd_stop` → success-only `AfterServerStopped`、failure-only `ServerStopFailed`。application taskから Transport close/stop呼出なし。
- `git diff --check`: outputなし。
- `sdkconfig`、`dependencies.lock`、`repos.json`、partition の tracked changeなし。

## Fresh reviewer

- round 1 fresh reviewer: `sol_advisor_sol_reviewer` / GPT-5.6 Sol / High、thread `019fc1de-e4fe-7423-a843-c455e17ec6cd`。verdict `needs-fix` 相当。orderly stop が connection pump を invalidate し、同一 WebSocket の次 stream を送れない High finding を報告した。
- correction: actual production/test 共通 `finishOrderlyStream` を追加し、同一 generation の 2 stream 再利用と publication failure 後 retry を focused tests に追加した。全 offline/build evidence は `raw/final_r2/` で再取得した。
- final fresh reviewer: `sol_advisor_sol_reviewer` / GPT-5.6 Sol / High、thread `019fc1fd-607c-77e1-b249-b2e48d1ebcac`、verdict `ship`。complete diff、14 source、UI stop call graph、全 Transport mutation site、StartStream/Close 線形化、pump/server lifetime、pinned PsychicHttp/ESP-IDF、tests、`raw/final_r2` logs、dependency pins、resource maps を read-only で確認した。
- reviewer は sandbox `workspace-write` / permission profile `managed` で OS 強制 read-only ではなかったが、行動上 read-only を遵守し、開始/終了の14 source hash、Git state、complete diff SHA-256 `43f83339c4002aa70621e7661c753f55e4683aa0f03988717e8a359bdcb421b5` の一致を報告した。

## 未実施、残存リスク、状態変更

- physical test、flash、serial、Edge、browser、softAP、Wi-Fi は実施していない。停止済みの Edge 試験を acceptance evidence に使用していない。
- offline evidence から original Edge/softAP symptom が解決したとは主張しない。
- `stream_started` response 自体を activation transaction に取り込む full transactional redesign は仕様外として deferred。response 送信直後に server-stop/client-close が勝つと、client が開始応答を観測後、一件も data を受けず close され得る。この残存リスクは実 HTTPD/FreeRTOS scheduling の物理検証も未実施である。
- commit、stage、push、PR、Issue、merge、branch/ref/index mutation は実施していない。
