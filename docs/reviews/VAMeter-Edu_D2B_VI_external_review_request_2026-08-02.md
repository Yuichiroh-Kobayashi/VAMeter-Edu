# VAMeter-Edu D2B V/I 外部レビュー依頼

作成日: 2026-08-02 JST  
対象リポジトリ: `Yuichiroh-Kobayashi/VAMeter-Edu`  
対象base branch: `dev/vi-logger`  
base commit: `43cda3462df91af2e78e4fbc67a2bf242ae25ff5`  
実装最上位commit: `124b6c7aad4739024d22a58e1e6698dd37196ca6`

> この変更は外部レビュー用です。物理統合試験、iPad Safari試験、IRAM余裕の確保が未完了であり、merge/releaseを依頼するものではありません。

## 1. レビュー依頼の目的

VAMeter-Eduへ、Device-to-Browser Data Streaming protocol `d2b-stream/0.1` のV/I measurement vertical sliceを追加しました。

現時点では、protocol oracleによる検証、host build／unit test、ESP-IDF v5.1.6 build、authorized firmwareの実機flash、boot smoke test、VAMeter softAPへの実接続、D2B HTTP root／capabilities／statusの実機確認まで完了しています。

Chromeは試験harness上の問題が続いたため、試験計画上SKIPPEDとし、PASS／FAILのいずれにも扱っていない。

Microsoft Edgeでは、専用user-data-dir、CDP endpoint、device page targetの確認、tracked `capture-live.js`の実行まで到達した。

collector開始後、強いRF条件（signal 100%、RSSI -30～-32 dBm）にもかかわらず、HTTPがtimeoutへ移行し、WebSocketがclose code 1006で終了した。
その後、Windows Wi-Fiは切断され、VAMeter softAPはWLAN scanから消失した。

ユーザーはVAMeter本体が再起動したように見える表示遷移を目視した。
ただし、serial evidenceにはboot banner、reset reason、panic、watchdog等が残っておらず、device resetが確定したとは扱わない。

したがって、現在のblockerは単なるbrowser harness問題ではない。
VI0、VI1、VI4を中心に、WebSocket開始を契機としたHTTP server、softAP、task、socket、owner、resource、reset lifecycleの静的レビューが必要である。

本レビューでは、物理試験の未完了とは分離して、VI0～VI4の製品実装とVI5の検証設計を静的・批判的に確認してください。

## 2. Protocol oracle

- Repository: `Device-to-Browser-Data-Streaming`
- Branch: `main`
- Commit: `5411ba59a12882345d32218eda367bd6ba35ef5`
- Implementation commit: `1d45da017b1b834f49be51d870691ac298e507a0`

確認済みoracle結果:

- JSON schemas: 3 PASS
- Golden vectors: 95 PASS
- Negative self-tests: 2 PASS
- Mutation tests: 7 PASS

仕様、schemas、golden vectors、browser reference parserをoracleとし、製品実装との不一致がある場合は、まず製品実装を疑う方針です。

## 3. Stacked branch構成

| Slice | Branch | Base | Head | 主なscope |
|---|---|---|---|---|
| VI0 | `refactor/d2b-web-server-owner` | `43cda3462df91af2e78e4fbc67a2bf242ae25ff5` | `0b4e3e6c8b2c9c232834c755b67270ac9f314b92` | port 80 server ownerの排他化 |
| VI1 | `feat/d2b-vi-transport` | `0b4e3e6c8b2c9c232834c755b67270ac9f314b92` | `13639df1988b0490a37d79b178e54c31c7c539e1` | HTTP routes、bounded WebSocket、strict control、owner/state |
| VI2 | `feat/d2b-vi-frame-writer` | `13639df1988b0490a37d79b178e54c31c7c539e1` | `eb78407a5c0dc080b9550d5dd036df81c7f02d43` | explicit little-endian V/I frame writer、oracle bridge |
| VI3 | `feat/d2b-vi-producer` | `eb78407a5c0dc080b9550d5dd036df81c7f02d43` | `fa7bf37feb8003b46dca22f9b00fa657b97697c5` | checked measurement validity、bounded PM tap |
| VI4 | `feat/d2b-vi-backpressure` | `fa7bf37feb8003b46dca22f9b00fa657b97697c5` | `4861d5d2e2878260f40c78d603a8b87386a3994b` | output ring、TX task、drop counters、stop/reconnect |
| VI5 | `test/d2b-vi-integration` | `4861d5d2e2878260f40c78d603a8b87386a3994b` | `124b6c7aad4739024d22a58e1e6698dd37196ca6` | live capture／offline replay tooling、物理試験手順 |

各sliceは独立checkpoint commitです。可能であれば、branch間差分をVI0から順にレビューしてください。

## 4. 実装方針

### 4.1 HTTP／WebSocket transport

- 第三のHTTP serverは追加せず、port 80 ownerを明確化
- stock `PsychicWebSocketHandler`は不使用
  - frame長に比例したallocation前に2048-byte制限を適用できないため
- ESP-IDF WebSocket APIを用いたproduct-local bounded handler
- control message raw上限: 2048 bytes
- fragmented messageはbounded reassembly
- clientからのbinary messageは拒否
- invalid control messageでstateを変更しない
- one active stream owner
- disconnect時にstale ownerを残さない

### 4.2 Strict JSON control

- strict UTF-8
- duplicate decoded key拒否
- trailing JSON拒否
- lexical integer検証
- non-finite拒否
- field数、depth、string長の上限
- schema、semantic、state、owner validationを分離
- acceptance後のみtransactionalにstate commit

### 4.3 V/I binary frame

- Envelope: 32 bytes
- V/I record: 16 bytes
- Frame: 48 bytes
- 1 record per frame
- advertised `sample_rate`: unknownとして`0/0`
- explicit little-endian store
- invalid channelはvalidity bitをclearし、payloadをcanonical `+0.0f`
- orderly stop時に`STREAM_END`を1回送信
- `STREAM_END`後にdata frameを送らない

### 4.4 Producer／backpressure

```text
existing PM daemon
  -> nonblocking acquisition ring
  -> V/I encoder
  -> output ring
  -> dedicated WebSocket TX task
```

- acquisition queue: 64 samples
- output queue: 32 frames
- producerはnetwork sendを行わない
- queue overflow時はoldestをdrop
- sequenceはdrop後も詰め直さない
- producer/output drop counterはsample数のcumulative monotonic counter
- reconnect時は新しいstream ID

### 4.5 Measurement validity

既存UI／CSVのcalibration・scaling・LC/HC switchingは変更していません。

D2B用に、INA226 readのI2C成功、finite、overflow statusを確認するchecked pathを追加しました。

重点確認事項:

- I2C errorが有限値の0として誤ってvalidにならないか
- LC/HC range switch後に採用したpathのvalidityが正しいか
- invalid fieldがbit-levelでcanonical `+0.0f`か
- 既存measurement semanticsを変更していないか

## 5. 完了した検証

### Offline／build

- Host CMake configure/build: PASS
- CTest: 12/12 PASS
- Live-capture validator positive/negative self-test: PASS
- ESP-IDF v5.1.6 build: PASS
- `git diff --check`: PASS
- 7 dependencies: commit、origin、clean、symlink条件 PASS
- Artifact checksum verification: PASS

### Resource

- Application: `0x1a9480 / 0x200000`
- 約83.1%使用、17% free
- Static D/IRAM: 42.1%使用
- Static IRAM: 16,383 bytes使用、残り1 byte

Static IRAM残り1 byteは主要なrelease blockerです。実機試験用のexact binaryはlink済みですが、今後の変更やmergeに対する余裕がありません。

### Physical

- Authorized firmware flash: PASS
- Application SHA-256: `f9492b72a1fd14addf88aa35ff8de34eae6d3f1d561edf4a2fe8f9373809b6bf`
- ELF SHA-256: `20cd18188332d6de8903b950d05091a9f96c067d7c22600abe38b78cb74ab1e4`
- Boot、FAT mount、INA226 2台、Power Monitor init: PASS
- boot evidence上のpanic／watchdog／allocation failure: 0
- softAP接続: PASS
- `/d2b/v0/`: HTTP 200
- `/d2b/v0/capabilities`: HTTP 200、oracle PASS
- `/d2b/v0/status`: HTTP 200、oracle PASS

## 6. Browser試験の現状

### Chrome

```text
SKIPPED BY TEST-PLAN DECISION
NOT PASS
NOT FAIL
```

最初の試行ではWebSocket close `1006`が発生しましたが、直後にhost Wi-Fiも切断し、当時のsignalが5%だったため原因を分離できませんでした。

Wi-Fi driver更新後の再試行は、Chrome CDP harnessのprofile／PowerShell quoting問題でcollectorへ到達していません。firmware結果として扱っていません。

### Edge

Windows Edge 150.0.4078.105を専用user-data-dirおよびCDP endpointで起動し、tracked `capture-live.js`を1回実行した。

試験開始条件:

- VAMeter softAP signal: 100%
- RSSI: -30～-32 dBm
- Windows IPv4: 192.168.4.2/24
- Device address: 192.168.4.1
- `/d2b/v0/status`: HTTP 200
- D2B state: idle
- connected client count: 0
- Edge CDP target: `http://192.168.4.1/d2b/v0/`

時系列:

1. 01:01:42 tracked collector開始
2. 01:01:43 HTTP 200、Wi-Fi connectedを確認
3. 01:01:45 HTTPがtimeoutへ移行
4. 約01:01:57 WebSocket close code 1006
5. postflightでWindows Wi-Fi disconnected
6. VAMeter softAPはWLAN scanから消失
7. ユーザーがVAMeter本体の再起動と見える挙動を目視した。ただしreset reasonは未取得であり、confirmed device resetとは扱わない。

serial raw captureにはboot banner、panic、watchdog、assert、abort等は残らなかったが、これはresetがなかったことの証明ではない。

この結果により、browser harnessだけでは説明できない可能性が生じた。
VI0、VI1、VI4を中心に、WebSocket開始を契機としたHTTP server、softAP、task、socket、owner、watchdog、reset lifecycleのレビューを依頼する。

現時点の判定:

- Edge desktop interoperability: No-Go
- Chrome: skipped / not pass
- iPad Safari: not tested
- VI5: No-Go
- merge／release: No-Go

### iPad Safari

未実施です。最終製品目標として、少なくとも以下を独立確認する必要があります。

- softAP接続
- HTTP root表示
- WebSocket接続
- binary frame受信
- V/I更新
- orderly stop
- reconnect
- UI操作性
- 継続動作

## 7. Security上の現状

現在のsoftAPはopen／unencryptedです。

```text
D2B security interpretation:
  unauthenticated-read-only candidate
```

`isolated`またはsecureとは判定しません。

レビュー用branch／reportへ、SSID、BSSID、client IP、credential、個人情報を含むruntime artifactは追加していません。

## 8. 重点レビュー依頼

### VI0: server ownership

- system serverとdownload serverのport 80 ownerが一意か
- app遷移時にserver／route／clientが残存しないか
- 既存download flowのregressionがないか

### VI1: transport／control

- allocation前2048-byte上限
- fragmented message reassemblyの境界
- strict UTF-8
- duplicate decoded key
- trailing data
- integer lexical validation
- state transaction
- owner generation
- disconnect race
- stale socket／owner
- second ownerへのbusy response
- information disclosure

### VI2: binary writer

- envelope field layout
- checked arithmetic
- explicit little endian
- `STREAM_START`
- data frame
- `STREAM_END`
- payload length
- flags
- sequence／timestamp

### VI3: producer／validity

- PM daemonへの非blocking性
- mutex lifetime
- INA226 I2C error propagation
- finite／overflow判定
- LC/HC switching
- canonical zero
- existing UI／CSV semanticsの非変更

### VI4: backpressure／lifecycle

- acquisition ringとoutput ringのoverflow
- oldest-drop semantics
- sequence gap
- cumulative sample counters
- saturating counter
- TX blocking隔離
- orderly drain
- `STREAM_END`と`stream_stopped`の順序
- abrupt disconnect
- reconnect stream ID
- task／queue／socket cleanup

### VI5: integration tooling

- collectorが独自parserになっていないか
- wire itemをoracleへ渡してからproduct assertionを行っているか
- raw evidenceの欠落／変形がないか
- orderly drainの判定
- synthetic self-testが物理PASSへ誤用されていないか
- Safari試験へ再利用可能な構成か

### Resource／security

- IRAM残り1 byteの原因と削減候補
- task stack／heap計画
- open softAPでのsecurity mode表現
- read-onlyであっても公開してはいけないstatus fieldがないか

## 9. 明示的に未完了の項目

- Edge live capture
- iPad Safari live capture
- 実V/Iとの比較
- intentional gap／backpressure physical test
- abrupt disconnect／same-boot reconnect
- runtime task stack high-water
- heap trend
- PM daemon timing independence
- 30分以上のsoak
- waveform／recorderの最終実機回帰
- IRAM余裕の確保

## 10. レビュー結果に求める形式

```text
Overall:
  Go / Conditional Go / No-Go

Blocking findings:
  - file:line
  - severity
  - failure scenario
  - proposed correction

Major findings:

Minor findings:

Protocol compliance:

Concurrency／lifecycle:

Measurement semantics:

Security:

Resource risk:

Test adequacy:

Recommended PR split／merge order:

Required physical tests before merge:
```

レビュー段階では、直接のmerge、force-push、branch deletion、dependency更新、protocol oracle変更は行わないでください。
