# D2B runtime evidence 運用メモ

この文書は、D2B V/I integration に追加した diagnostic-only breadcrumb の読み方と
物理試験前の確認範囲を示す。ログから測定精度、一定周期、空き容量の安全性を主張しては
ならない。heap と stack は instrumented but not measured である。

## 行形式

各行のメッセージは `D2B_DIAG` で始まり、次の順序で固定 key を持つ。

```text
D2B_DIAG event=<token> reason=<token> result=<token> owner=<token> generation=<n> server_generation=<n> socket=<n> stream_id=<n> heap_free=<n> heap_min=<n> heap_largest=<n> encoder_stack_min=<n> tx_stack_min=<n> acquisition_depth=<n> output_depth=<n> producer_drops=<n> output_drops=<n>
```

該当しない resource は `0`、socket は `-1` とする。boot 行は固定 resource fields の後に
`reset_reason_code`、`reset_reason_raw`、`boot_identity`、`rtc_boot_counter` を追加する。
SSID、password、IP、MAC、payload、測定値、個人情報は記録しない。

## 観測対象

- boot/reset: `event=boot`。RTC no-init 領域は magic guard 付きの揮発 counter だけに使う。
- Network settings 起点: `network_settings_start` と
  `network_settings_intentional_stop` を server lifecycle の reason として記録する。
- server owner: successful acquire のみ non-zero `server_generation` を進める。失敗した acquire は
  `server_generation` を変えず、release 後も直前 generation を相関用に保持する。
- WebSocket owner: `generation` は stale socket/action 防止のため accepted connection ごとに進む
  独立した non-zero owner key であり、server lifecycle の `server_generation` と置き換えない。
- server lifecycle: start/stop の request と result はそれぞれ `event=server_start` / `event=server_stop`
  として記録し、request/result の両方に heap snapshot を含める。
- WebSocket: accepted connect と通常の disconnect を記録する。intentional server stop は
  `event=websocket_disconnect reason=server_stop` として peer disconnect と区別し、active stream は
  `reason=server_stop` の `stream_abrupt` として扱う。
- send failure: 既存 TX task から `event=send_failure reason=send_failure` の snapshot と、続けて
  `event=stream_abrupt reason=send_failure` の二行を記録する。internal task failure は少なくとも
  `event=stream_abrupt reason=internal_failure` を記録する。
- stream: accepted start、orderly stop accepted/completed、abrupt termination を記録する。
- resource trend: internal 8-bit heap の free/minimum-ever/largest と D2B encoder/TX task の
  byte 単位 stack high-water を記録する。
- queue/drop: producer/output drop counter の変化を既存 TX task が snapshot として記録する。
  acquisition/encoder hot path ではログを出さない。

## rate limit と試験手順

active stream の trend は 5,000,000 us 未満では再出力しない。初回は直ちに出力し、時計が
後退した場合は新しい時刻を基準に gate を回復する。物理試験では、boot、Network settings
start/intentional stop、server stop、WebSocket connect/disconnect、start/stop、drop/send failure の
breadcrumb を同じ WebSocket `generation` と `stream_id` で時系列に並べ、必要に応じて
server lifecycle の `server_generation` と照合する。

この層は既存の D2B protocol v0.1、32-byte envelope、sequence/timestamp、backpressure、
public endpoint、測定表示、CSV、UI navigation を変更しない。HTTPD queue-work 修正や
transactional stream start はこの phase の対象外である。
