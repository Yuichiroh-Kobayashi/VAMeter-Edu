Status: DESIGN REVIEW ONLY
Implementation authority: NO
Physical qualification: NOT RUN
Base commit: 3348bc86d5d911e131315ad35d6cec022ab01a59

# Issue #27 Wi-Fi AP QR / Viewer QR 視覚識別設計

対象 Issue: [#27 Make Wi-Fi AP QR and Viewer QR screens visually distinguishable at a glance](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/27)

本書は設計レビュー用である。実装は行っていない。ビルド・test・実機確認はいずれも実行していない。
本 branch の作業対象は本file 1点のみで、implementation source は変更していない。

---

## 1. Decision

### 結論

`LiveShareView` のみを変更範囲とし、共通 QR generator (`QRCODE::GetQrcodeBitmap`) と
module renderer (`QRCODE::RenderQRCodeBitmap`) は変更しない方針を推奨する。

推奨案は **Alternative A1「state色背景 + 大型badge + frame形状 + quiet-zone plate」** である。
独立した視覚channelを5本持ち、色覚特性に依存しない識別が可能で、新規asset・新規memory確保を
伴わず、AssetPool layout guard に触れない。

### 判断根拠の区分

| 区分 | 内容 |
| --- | --- |
| FACT | 両画面は背景色 `0xFF6161`、title文字列 `Live View`、QR描画位置 `(45, 34)`、QR領域 `150px` がすべて同一であり、差分は14ptの2行テキストのみ。source上で確認済み。 |
| FACT | `RenderQRCodeBitmap` は module矩形のみを描画し、quiet zone を描画しない。現行の Live View QR は quiet zone 0 module で彩度の高い背景に直接置かれている。 |
| HYP | quiet zone 0 が実機 scan成功率に与える影響は未測定。多くの consumer scanner は 0〜2 module でも復号するが、ISO/IEC 18004 の推奨は 4 module。確度: 中。 |
| VALUE | 教室運用では「間違えない」ことを優先する。色差は補助channelに降格し、形状・文字サイズ・輝度差を主channelに置く。QR scan性能を下げる変更は採用しない。 |

### やらないこと

- QR payload の変更（`WIFI:T:nopass;S:<ssid>;;` / `http://<ipv4>/viewer/` は不変）。
- QR module 色の変更（前景 black / 背景 white を維持）。
- QR module 上への装飾・overlay。
- 装飾のための QR 縮小。
- `app/apps/utils/qrcode/` の共通 generator / renderer 変更。
- 他の QR 画面（Settings Network, Download, Meter Help）の同時変更。

### 撤退条件

- 実機で Wi-Fi QR または Viewer QR の scan が現行より劣化した場合、plate/frame を廃し
  Alternative A2（配色・geometry不変、badgeとcorner markerのみ）へ後退する。
- 教室照明下で暗色背景の反射が読み取りを阻害する場合、配色候補を `0x1B3A6B` 系へ差し替える。

---

## 2. Current QR architecture

### 2.1 実測した flow

```
LIVE_SHARE_SESSION::State (WifiQr | ViewerQr)
  → LiveShareView::prepareQr()
      State==WifiQr  → LIVE_SHARE_SESSION::BuildWifiQrPayload(activeApSsid)
      State==ViewerQr → trustedViewerUrl (HAL::GetSystemLiveViewerUrl())
      (state, payload) が前回と同一なら再生成しない cache
      → QRCODE::GetQrcodeBitmap(_qrBitmap, payload)   [qrcodegen, Ecc::LOW]
  → LiveShareView::render()
      canvas->fillScreen(themeColor)                   themeColor = AppWaveform.primary
      title "Live View" (Font24, top_center, y=4)
      QRCODE::RenderQRCodeBitmap(_qrBitmap, 45, 34, 150, TFT_BLACK, TFT_WHITE)
      Font14 hint 3行 (y=194 / 211 / 227)
      HAL::CanvasUpdate()
```

### 2.2 確認した source 位置

| 役割 | 位置 |
| --- | --- |
| state機械 | `app/libs/live_share_session/live_share_session.h:10-19` (`State::WifiQr`, `State::ViewerQr`) |
| payload生成 | `app/libs/live_share_session/live_share_session.cpp:166-203` (`BuildWifiQrPayload`, `BuildViewerUrl`) |
| view本体 | `app/apps/app_power_monitor/view/live_share_view.cpp:40-123` |
| QR bitmap生成 | `app/apps/utils/qrcode/qrcode.cpp:48-65` |
| QR module描画 | `app/apps/utils/qrcode/qrcode.cpp:67-87` |
| 呼び出し元 | `app/apps/app_waveform/app_waveform.cpp:153-161` (`_render_live_share_view`) |
| 背景色定義 | `app/assets/theme/types.h:61` `AppWaveform_t::primary = 0xFF6161` |

構造上の注意: `LiveShareView` は `app/apps/app_power_monitor/view/` に置かれているが、実際の
利用は `app_waveform` のみである（grep で他の call site なし）。移動は本 Issue の scope 外とする。

### 2.3 他 QR 画面の inventory

| 画面 | 位置 | 背景 | QR領域 | quiet zone |
| --- | --- | --- | --- | --- |
| Live View Wi-Fi QR | `live_share_view.cpp:88` | `0xFF6161` | 150 (x=45,y=34) | なし |
| Live View Viewer QR | 同上（同一 code path） | `0xFF6161` | 150 (x=45,y=34) | なし |
| Meter Help QR | `app/apps/utils/system/ui/misc/meter_help_qr_view.cpp:42-54` | theme色 + 白plate 185 | 可変 (`HelpQrGeometry`) | 4 module |
| Settings Network Wi-Fi QR | `app/apps/app_settings/view/network.cpp:136-141` | `AppSettings.background` (白) | 120 (y=50) | なし（背景が白） |
| Settings Network URL QR | `app/apps/app_settings/view/network.cpp:176-181` | `AppSettings.background` (白) | 120 (y=40) | なし（背景が白） |
| Record Download QR | `app/apps/utils/system/ui/misc/download_qr_page.cpp:122-127` | `AppSettings.background` (白) | 150 (y=30) | なし（背景が白） |

`meter_help_qr` のみが quiet zone を明示的に確保しており、`app/libs/meter_help_qr/meter_help_qr_urls.cpp:14-24`
の `GeometryForModuleCount()` が再利用可能な先例になる。

Settings Network も「Wi-Fi QR → URL QR」の2段構成で同種の取り違えリスクを持つが、背景が白で
運用文脈が異なるため、本 Issue では変更対象に含めない（Section 12 参照）。

---

## 3. Root cause of poor visual distinction

`live_share_view.cpp:85-95` の分岐は、`WifiQr` と `ViewerQr` で **描画される要素のうち2行の14pt文字列
だけ** を変えている。

両 state で完全に同一なもの:

- 背景色 `themeColor`（呼び出し側は常に `AppWaveform.primary = 0xFF6161`、Issue本文の「pink」に一致）
- title 文字列 `"Live View"`（Font24）
- QR矩形の原点 `(45, 34)` と指定size `150`
- QR module色（black / white）
- 3行目の hint `"Side: Stop"`
- 文字色（すべて `TFT_WHITE`）

異なるもの:

- y=194: `"Join device Wi-Fi"` / `"Open Viewer"`（Font14 ≒ 高さ14px）
- y=211: `"Encoder: Next"` / `"Encoder: Wi-Fi QR"`（Font14）

したがって「小さい文字が唯一の識別子」という Issue の記述は source と一致する。
QR 図柄自体も payload が異なるだけで、人間の視覚は module pattern を識別子として使えない。

副次的に確認した既存の不整合:

- `RenderQRCodeBitmap` の scale は `round(size / moduleCount)` であるため、実描画幅は指定の150px
  と一致しない。AP suffix 未設定時の SSID `M5-VAMeter-WiFi` は 29 module → scale 5 → 実幅 145px、
  suffix 設定時の `M5-VAMeter-07` および Viewer URL は 25 module → scale 6 → 実幅 150px。
- 原点 x は 145px の場合も 45 固定のため、左余白45px / 右余白50px で中央からずれる。
- 25 module（実幅150px, y=34..184）のとき、y=194 の Font14 行との間隔は約3pxしかない。

---

## 4. Scanability constraints

### 4.1 実測した module数と scale

`app/apps/utils/qrcode/qrcodegen/` を host で直接 link して算出した（本repo変更なし、scratchpad上で実行）。

| payload | bytes | module数 | 現行 scale (150基準) | 現行 実描画 |
| --- | --- | --- | --- | --- |
| `WIFI:T:nopass;S:M5-VAMeter-WiFi;;` | 33 | 29 | 5 | 145px |
| `WIFI:T:nopass;S:M5-VAMeter-07;;` | 31 | 25 | 6 | 150px |
| `http://192.168.4.1/viewer/` | 26 | 25 | 6 | 150px |
| `http://192.168.100.200/viewer/` | 30 | 25 | 6 | 150px |

`GetQrcodeBitmap` は `Ecc::LOW` 固定（`qrcode.cpp:51`）。誤り訂正 約7%。

### 4.2 quiet zone

- `RenderQRCodeBitmap` は module矩形のみを `fillRect` する。matrix 外は直前の `fillScreen(themeColor)`
  の色が残る。つまり **現行 Live View QR の quiet zone は 0 module**。
- 比較: `MeterHelpQrView` は `fillRect(..., TFT_WHITE)` で 185px の白領域を先に塗り、4 module の
  quiet zone を確保してから module を描画している。
- FACT として、現行実装は ISO/IEC 18004 の推奨 quiet zone を満たしていない。
- HYP: これが実際の scan失敗を起こしているかは未測定（確度: 不明）。ただし本 Issue の改修で
  quiet zone を確保すれば、少なくとも現行より悪化しないことは幾何的に保証できる。

### 4.3 QR 外側に使える領域（240x240）

現行 leftover:

| 領域 | 範囲 | 現在の用途 |
| --- | --- | --- |
| 上帯 | y 0..33 (34px) | Font24 title |
| 下帯 | y 184..239 (56px) | Font14 hint 3行 |
| 左余白 | x 0..44 (45px) | 空 |
| 右余白 | x 195..239 (45px) | 空 |

frame / header / badge は **すべてこの外側領域に収まる**。module 上には何も描かない。

### 4.4 quiet zone を確保しても module scale を落とさない構成

`meter_help_qr` と同じ `area / (moduleCount + 2*quietModules)` 方式を、area=174px, quietModules=2 で
適用した場合:

| payload | module数 | scale | 描画総幅 (module+quiet) | 内側QR実幅 |
| --- | --- | --- | --- | --- |
| Wi-Fi (default SSID) | 29 | 5 | 165px | 145px |
| Wi-Fi (suffix SSID) | 25 | 6 | 174px | 150px |
| Viewer URL | 25 | 6 | 174px | 150px |

**module scale は現行と同一（5 または 6）のまま、2 module 分の quiet zone が新たに得られる。**
scan性能を落とさずに quiet zone 欠落を是正できる、という点が本設計の中核である。

4 module の完全な quiet zone を同 scale で確保するには 198px 必要で、header と hint行を両立できない。
2 module は妥協であり、「現行0より改善」以上の主張はしない（実機確認は Section 10）。

### 4.5 採用しない選択肢

- module 上の装飾・overlay: 復号を直接壊す。
- QR 前景/背景の低コントラスト化: black/white を維持する。
- 装飾のための QR 縮小: 上記 4.4 により不要。
- 装飾都合での payload 変更: `BuildWifiQrPayload` / `BuildViewerUrl` は既存 host test
  (`tests/live_share_session/live_share_session_test.cpp:205-213`) で完全一致固定されており、
  変更すれば契約違反になる。
- ECC を LOW から上げること: Wi-Fi suffix SSID が 25→29 module になり scale が 6→5 に落ちる。
  本 Issue の目的と無関係な劣化なので採らない。

---

## 5. Design alternatives

すべての案は独立した視覚channelを2本以上組み合わせる。色相のみ、赤緑対比のみには依存しない。

### 配色候補（design candidate のみ。実機qualification未実施）

WCAG相対輝度で算出した候補。

| 候補 | 輝度 L | 白文字 CR | 黒文字 CR |
| --- | --- | --- | --- |
| 現行 `0xFF6161` | 0.307 | 2.94 | 7.13 |
| Wi-Fi 濃紺 `0x14213D` | 0.016 | 15.97 | 1.31 |
| Wi-Fi 紺 `0x1B3A6B` | 0.043 | 11.27 | 1.86 |
| Viewer 琥珀 `0xFFC233` | 0.601 | 1.61 | 13.02 |
| Viewer 琥珀 `0xFFB000` | 0.523 | 1.83 | 11.46 |

| 対 | 背景間 CR |
| --- | --- |
| `0x14213D` / `0xFFC233` | **9.90** |
| `0x14213D` / `0xFFB000` | 8.72 |
| `0x1B3A6B` / `0xFFC233` | 6.98 |
| （参考）現行同士 | 1.00 |

輝度差が大きいため、1型・2型・3型色覚および全色盲でも「暗い画面 / 明るい画面」として区別できる。
文字色の極性（白文字 / 黒文字）も同時に反転するため、色相を知覚しなくても差が残る。

---

### Alternative A1 — state色背景 + 大型badge + frame形状 + quiet-zone plate（推奨）

視覚channel: 色相 / 輝度 / 大型短ラベル / frame形状 / 文字色極性 の5本。

提案 layout（240x240、数値は desktop simulator で検証すべき candidate）:

```
y 0..38    背景 = state色。badge を Font36 で middle_center 描画
             WifiQr : "WI-FI"   (白文字 on 濃紺)
             ViewerQr: "VIEWER" (黒文字 on 琥珀)
y 40..222  frame（plate の 4px 外側、x 29..211）
             WifiQr : 3px 連続矩形（実線・閉じた枠）
             ViewerQr: 3px L字 corner bracket ×4（辺の中央が開いた枠）
y 44..218  白 plate 174x174 (x 33..207)
             内側に quiet 2 module を確保して QR module を中央配置
y 222..240 Font14 hint 1行
             WifiQr : "Encoder: Next    Side: Stop"
             ViewerQr: "Encoder: Back    Side: Stop"
```

- 小LCDでの視認性: 背景全面が変わるため、最も遠距離で効く。
- 色覚 accessibility: 背景CR 9.90 + 文字色極性反転 + frame形状 + badge語長差。色相を失っても4本残る。
- 通常運用距離からの識別: Font36 の "WI-FI" / "VIEWER" は語長も大きく異なる。
- QR scan性: module scale 現行同一、quiet zone 0→2 module で改善方向。
- 既存 font/asset: `Font.montserrat_semibold_36` は既に `StaticAsset_t` に存在し、ASCII全域103 glyph を
  含むことを .vlw 解析で確認済み（AssetPool 再生成不要）。
- code複雑度: `LiveShareView::render()` に分岐と描画呼び出しを追加。primitive は
  `fillRect` / `drawRect` / `drawString` / `loadFont` のみで、いずれも既に repo内で使用実績がある。
- AssetPool / static memory: 変更なし。`ImagePool_t` / `FontPool_t` の layout に触れないため
  `ASSET_POOL_LAYOUT::kStaticAssetLayoutVersion` の bump も不要。
- testability: 後述の pure selector を切り出せば host test 可能。
- UI整合: 白plate + quiet zone は `MeterHelpQrView` と同じ表現になり、むしろ整合性が上がる。
- 懸念: hint行が3行→1行に減る。情報量は badge が1行分を吸収するが、文言はレビュー対象。

### Alternative A2 — 配色・geometry不変、badge と corner marker のみ（最小資源案）

視覚channel: 大型短ラベル / corner marker形状 / step dot の3本。色は一切変更しない。

```
背景 0xFF6161 のまま、QR は現行の (45,34,150) のまま
y 0..38    Font36 badge "WI-FI" / "VIEWER"（白文字）
           右上に step dot: ●○ / ○●（fillCircle 2個）
QR外周     QR矩形の 6px 外側に L字 corner bracket
             WifiQr : 4隅すべて
             ViewerQr: 左上/右下の2隅のみ（配置自体が cue）
y 194..    既存 hint 3行を2行に整理
```

- 小LCDでの視認性: 背景が変わらないため A1 より弱い。距離が離れると badge 頼み。
- 色覚 accessibility: 色に全く依存しないため最も安全。
- QR scan性: QR 描画は完全に不変。**改善もしないが劣化もしない。** quiet zone 0 の既存問題は残る。
- 資源: 追加 asset なし、追加 memory なし。描画呼び出し追加は10回未満。
- 位置付け: A1 が実機で問題を起こした場合の後退先。撤退条件の受け皿。

### Alternative A3 — icon asset による識別（新規 ImagePool 追加）

Wi-Fi 電波弧 icon と browser window icon を `ImagePool_t` に追加し、色と併用する。

- 視認性・直感性は高い。
- **却下理由（資源）**: `app/assets/images/types.h` の `ImagePool_t` を変更すると
  `sizeof(StaticAsset_t)` が変わり、`app/libs/asset_pool_layout/asset_pool_layout.h` の
  fail-closed guard が `StaticAssetSizeMismatch` で起動を止める。`kStaticAssetLayoutVersion`
  の bump、AssetPool 再生成、firmware と AssetPool の matched deployment、実機書込みが必須になる。
- 参考容量: 40x40 の `uint16_t` icon 2個で 6,400 bytes。stable `v2.0.0` の AssetPool partition
  空きは 441,180 bytes（`docs/architecture/resource-budget.md`）なので容量自体は足りるが、
  「QR2画面の識別」という目的に対して deployment gate が重すぎる。
- 判定: 本 Issue では採らない。A1 が実機で不足と判定された場合のみ、別 Issue として再検討する。

### Alternative A4 — header band のみ state色（中間案）

背景 `0xFF6161` を維持し、上部 44px だけを state色の帯にして badge を極性反転文字で描く。
plate keyline の太さで形状 cue を足す。

- A1 より変更範囲が小さく、waveform app の色調から外れない。
- 遠距離での面積が A1 の 1/5 以下なので識別力は劣る。
- 帯色と本体 `0xFF6161` のコントラストも管理対象になり、実質2色管理が3色管理になる。
- 判定: A1 が「変化が大きすぎる」と判断された場合の代替として保持する。

---

## 6. Recommended design

**Alternative A1 を推奨する。** 次点は A2（撤退先）。

### 6.1 なぜ A1 か

1. Issue の受入条件「小さい文字を読まずに区別できる」「通常運用距離で見える」に対し、
   背景全面の輝度差が最も直接的に効く。
2. 「色覚が限定的でも理解できる」に対し、色相以外に4channel残る。
3. 「QR可読性を劣化させない」に対し、module scale 不変かつ quiet zone 0→2 という
   **幾何的に説明可能な非劣化** を示せる。
4. 新規 asset・新規 heap 確保がなく、AssetPool layout guard に触れない。
5. 白 plate は `MeterHelpQrView` の既存表現の再利用であり、新しい UI 語彙を増やさない。

### 6.2 Reuse → Buy → Make の整理

- **Reuse**: LovyanGFX の既存 primitive（`fillRect` / `drawRect` / `fillCircle` / `drawString`）、
  既存 `Font.montserrat_semibold_36`、`meter_help_qr` の quiet-zone geometry 算式、
  `QRCODE::GetQrcodeBitmap` / `RenderQRCodeBitmap`。
- **Buy**: 該当なし。外部 library / asset の購入・導入は不要。
- **Make**: `LIVE_SHARE_SESSION::State` → presentation style を返す純粋 selector 1個と、
  plate geometry 算出関数1個のみ。いずれも HAL / LGFX 非依存で host test 可能。

`meter_help_qr` の `GeometryForModuleCount()` を直接改変して共用化する案も検討したが、
`tests/educational_interaction_contract/educational_interaction_contract_test.cpp:129-132` が
Help QR の geometry を完全一致で凍結しているため、blast radius が広がる。
本 Issue では新 lib に同型の算式を置き、統合は別 Issue の cleanup 候補として残す。

### 6.3 提案する最小 interface

```cpp
// app/libs/live_share_qr_presentation/live_share_qr_presentation.h  (案)
namespace LIVE_SHARE_QR_PRESENTATION
{
    static const int kPlatePixels      = 174;
    static const int kQuietZoneModules = 2;
    static const int kPlateX           = 33;
    static const int kPlateY           = 44;

    enum class Frame : std::uint8_t { None, SolidRect, CornerBrackets };

    struct Style
    {
        bool          showsQr;      // QR state のみ true
        std::uint32_t background;
        std::uint32_t foreground;   // 背景輝度に追従する文字色
        Frame         frame;
        const char*   badge;        // "WI-FI" / "VIEWER" / nullptr
        const char*   hint;
    };

    struct PlateGeometry
    {
        bool valid;
        int  moduleCount;
        int  moduleScale;
        int  renderPixels;   // (moduleCount + 2*quiet) * scale
    };

    Style         SelectStyle(LIVE_SHARE_SESSION::State state);
    PlateGeometry EvaluatePlateGeometry(int moduleCount);
}
```

`SelectStyle` は `switch` による compile-time bounded な写像とし、default で
非QR state の style（`showsQr=false`, `background=AppWaveform.primary`, `frame=None`,
`badge=nullptr`）を返す。動的確保・static 可変状態を持たない。

色値そのものは `ColorPool_t::AppWaveform_t` に追加する案と、lib 内 constant にする案がある。
`ColorPool_t` は `StaticAsset_t` の一部なので **前者は AssetPool layout 変更を伴う**。
本 Issue では後者（lib内 constant）を推奨する。この点は Section 14 の open question に残す。

---

## 7. Accessibility rationale

| 要求 | 対応 channel | 根拠 |
| --- | --- | --- |
| 色相を知覚できない | 背景輝度 CR 9.90 | WCAG相対輝度で算出。全色盲でも明暗差として残る |
| 赤緑弁別が困難（1型/2型） | 紺 vs 琥珀は青黄軸の差 | 赤緑軸に依存しない対を選択 |
| 青黄弁別が困難（3型） | 輝度差 + 形状 + badge | 色相を落としても4channel残る |
| 小文字が読めない距離 | Font36 badge / 語長差 | 現行の唯一の cue は Font14 |
| 色も文字も使えない条件 | frame形状（閉じた矩形 / 開いた bracket） | 形状は輪郭で判別でき、色・文字に非依存 |
| 画面写真・白黒印刷 | 輝度 + 形状 + badge | 色相以外がすべて保存される |

赤緑対比は使用しない。色は5本の channel のうち2本（色相・輝度）にすぎず、
色を完全に除いても Alternative A2 相当の識別性が残る設計になっている。

明示的な限界: 本節は source と WCAG算式に基づく設計上の主張であり、
実機LCD・教室照明・実際の色覚特性を持つ利用者による確認は行っていない。

---

## 8. Expected file-impact map

実装時に触れる想定。本 branch では **いずれも変更していない**。

| file | 種別 | 想定内容 |
| --- | --- | --- |
| `app/libs/live_share_qr_presentation/live_share_qr_presentation.h` | 新規 | style / geometry の宣言 |
| `app/libs/live_share_qr_presentation/live_share_qr_presentation.cpp` | 新規 | `SelectStyle` / `EvaluatePlateGeometry` |
| `app/apps/app_power_monitor/view/live_share_view.cpp` | 変更 | `render()` の背景・badge・plate・frame・hint 描画 |
| `app/apps/app_power_monitor/view/live_share_view.h` | 変更（小） | 必要なら geometry の保持member追加 |
| `tests/live_share_qr_presentation/CMakeLists.txt` | 新規 | host test target |
| `tests/live_share_qr_presentation/live_share_qr_presentation_test.cpp` | 新規 | Section 9 の test |
| `CMakeLists.txt` | 変更（1行） | `add_subdirectory(tests/live_share_qr_presentation)` |

変更しない:

- `app/apps/utils/qrcode/qrcode.{h,cpp}` および `qrcodegen/`
- `app/libs/live_share_session/`（payload・state機械）
- `app/libs/live_share_controller/`
- `app/libs/meter_help_qr/` および `app/apps/utils/system/ui/misc/meter_help_qr_view.*`
- `app/apps/app_settings/view/network.cpp`
- `app/apps/utils/system/ui/misc/download_qr_page.cpp`
- `app/assets/` 配下すべて（theme/images/fonts/static_asset_types）
- `app/libs/asset_pool_layout/`
- `platforms/vameter/main/CMakeLists.txt`、`platforms/desktop/CMakeLists.txt`

build system への影響: device 側 `platforms/vameter/main/CMakeLists.txt` と desktop 側
`platforms/desktop/CMakeLists.txt` はいずれも `app/*.cpp` を `GLOB_RECURSE` しているため、
新規 lib の source は再 configure だけで両 build に取り込まれる。両 file の編集は不要。

---

## 9. Host/static validation plan

本 branch では実行しない。WSL 環境で実装時に実行する設計。

### 9.1 実行手順（実装後に実行する想定）

```bash
git diff --check
git status -sb
cmake -S . -B build/desktop
cmake --build build/desktop -j 2
ctest --test-dir build/desktop --output-on-failure
```

### 9.2 新規 host test（`tests/live_share_qr_presentation/`）

| ID | 検証内容 |
| --- | --- |
| S01 | `SelectStyle(WifiQr)` と `SelectStyle(ViewerQr)` の `background` が異なる |
| S02 | 同2者の `frame` が異なる（`SolidRect` vs `CornerBrackets`） |
| S03 | 同2者の `badge` が非 null かつ文字列として異なる |
| S04 | 同2者の `foreground` が異なる（文字色極性の反転） |
| S05 | `SelectStyle` は同一入力に対し常に同一出力（決定性）。100回反復で一致 |
| S06 | `Starting` / `Stopping` / `StopRecovery` / `StartError` / `Inactive` の `showsQr` が false、`frame == None`、`badge == nullptr` |
| S07 | 非QR state の `background` が `WifiQr` / `ViewerQr` のいずれとも一致しない（QR専用 styling を継承しない） |
| S08 | `State` の全列挙値を走査して `SelectStyle` が未定義値を返さない（bounded写像） |
| S09 | `EvaluatePlateGeometry(25)` が `scale==6`, `renderPixels==174` |
| S10 | `EvaluatePlateGeometry(29)` が `scale==5`, `renderPixels==165` |
| S11 | 全 `moduleCount` 21..177 について `renderPixels <= kPlatePixels` かつ `moduleScale >= 2` のとき `valid` |
| S12 | `moduleCount < 21` / `> 177` で `valid == false` |
| S13 | 背景色の WCAG 相対輝度比が 4.5 以上（定数 table に対する静的検査） |

### 9.3 既存 test への追加（`tests/live_share_session/live_share_session_test.cpp`）

| ID | 検証内容 |
| --- | --- |
| P01 | `BuildWifiQrPayload("M5-VAMeter-07") == "WIFI:T:nopass;S:M5-VAMeter-07;;"` が不変（既存 L209 を維持） |
| P02 | `BuildViewerUrl("192.168.4.1") == "http://192.168.4.1/viewer/"` が不変（既存 L210 を維持） |
| P03 | escape 規則が不変（既存 L207） |

### 9.4 QR bitmap 非依存性（`tests/live_share_qr_presentation/` 側で qrcodegen を link）

| ID | 検証内容 |
| --- | --- |
| B01 | 各 payload の `GetQrcodeBitmap` 結果が style 選択の前後で bit単位一致 |
| B02 | Wi-Fi payload の module数が 25（suffix SSID）/ 29（default SSID）で凍結 |
| B03 | Viewer URL の module数が 25 で凍結 |
| B04 | `EvaluatePlateGeometry` の `moduleScale` が現行 `round(150/n)` と同値（5 または 6）であることを明示比較 |

### 9.5 資源・確保に関する静的確認

| ID | 検証内容 |
| --- | --- |
| R01 | `Style` / `PlateGeometry` が trivially copyable かつ `sizeof` が compile-time上限内（`static_assert`） |
| R02 | `SelectStyle` / `EvaluatePlateGeometry` が `std::string` / `std::vector` / `new` を使わない（source grep と review で確認） |
| R03 | `sizeof(StaticAsset_t)` が base commit と同一（`tests/asset_pool_layout/` の既存 test が検出） |
| R04 | `-Wall -Wextra -Wpedantic -Werror` で警告ゼロ（既存 test target の設定を踏襲） |

### 9.6 desktop simulator 目視確認

`app_desktop_build` で Live View の QR 2画面を表示し、layout 数値（plate 174、frame 位置、
badge baseline、hint行）が 240x240 内で重ならないことを確認する。
これは layout の sanity check であり、**scan性能・実機視認性の確認にはならない**。

### 9.7 本設計で「小さい純粋 selector を作る」判断

`LiveShareView::render()` は `HAL::GetCanvas()` / `AssetPool` / LGFX に依存するため host test できない。
既存 host test（`tests/live_share_session`, `tests/live_share_controller`,
`tests/educational_interaction_contract`）はいずれも HAL 非依存の pure lib のみを link している。
したがって S01〜S13 を成立させるには style 決定を pure lib に出す必要がある。
これは testability のための必要最小限の分離であり、描画そのものは `LiveShareView` に残す。
不要な abstraction layer（描画 backend の抽象化、style の runtime 差し替え機構など）は導入しない。

---

## 10. Deferred physical validation

以下はすべて **DEFERRED — HARDWARE UNAVAILABLE**。本 session では実機に一切触れていない。

| ID | 項目 | 状態 |
| --- | --- | --- |
| H01 | Wi-Fi QR の実機 scan（suffix 設定あり / なしの両 SSID） | DEFERRED — HARDWARE UNAVAILABLE |
| H02 | Viewer QR の実機 scan | DEFERRED — HARDWARE UNAVAILABLE |
| H03 | quiet zone 2 module 適用前後の scan 成功率比較 | DEFERRED — HARDWARE UNAVAILABLE |
| H04 | 通常運用距離（教卓〜生徒机、約0.5〜2m）からの2画面識別 | DEFERRED — HARDWARE UNAVAILABLE |
| H05 | 教室照明下（蛍光灯 / LED / 窓際逆光）での視認性 | DEFERRED — HARDWARE UNAVAILABLE |
| H06 | 暗色背景 `0x14213D` の画面反射・映り込み評価 | DEFERRED — HARDWARE UNAVAILABLE |
| H07 | 色覚特性を持つ利用者、または色覚 simulation 下での識別確認 | DEFERRED — HARDWARE UNAVAILABLE |
| H08 | 白黒撮影・グレースケール印刷での識別確認 | DEFERRED — HARDWARE UNAVAILABLE |
| H09 | encoder / side button の遷移 regression（Wi-Fi QR ⇄ Viewer QR ⇄ Stop） | DEFERRED — HARDWARE UNAVAILABLE |
| H10 | station join による自動 ViewerQr 遷移の regression | DEFERRED — HARDWARE UNAVAILABLE |
| H11 | 対応端末での scan 確認（iPad 7th / iPadOS 18.7.9 Safari、Windows Edge 151、Chromebook、Android / iOS 標準camera） | DEFERRED — HARDWARE UNAVAILABLE |
| H12 | Live View 描画追加後の D2B streaming 継続性・frame timing | DEFERRED — HARDWARE UNAVAILABLE |
| H13 | Starting / Stopping / StopRecovery / StartError 画面の非回帰 | DEFERRED — HARDWARE UNAVAILABLE |
| H14 | firmware image size / partition 空きの実測（`docs/architecture/resource-budget.md` 更新用） | DEFERRED — HARDWARE UNAVAILABLE |

H11 の対象端末は `docs/validation/browser-physical-qualification.md` に記録された stable `v2.0.0`
の実機検証構成（Windows Edge 151 / iPad 7th generation iPadOS 18.7.9 Safari）を基準とする。
実機書込み・readback・rollback は `docs/ai/physical-validation-and-rollback.md` の手順に従う。

---

## 11. Resource-risk assessment

stable `v2.0.0` の実測値（`docs/architecture/resource-budget.md`）を基準とする。

| 項目 | `v2.0.0` 実測 | A1 の想定影響 |
| --- | --- | --- |
| Application partition 空き | 322,944 bytes | `.text` 微増（描画分岐と定数 table）。数百 bytes 級と推定、要実測 |
| AssetPool partition 空き | 441,180 bytes | **変化なし**（asset 追加なし） |
| Dedicated static IRAM 残 | **1 byte** | **変化なし**（IRAM 配置 attribute を付けない） |
| DRAM `.bss` | 56,824 bytes | `Style` を値返しにするため static 可変状態なし。**増加なし** |
| heap | 未測定 | 動的確保なし。`_qrBitmap` の既存 `std::vector` 以外に追加しない |
| `sizeof(StaticAsset_t)` | — | **不変**。layout guard を trip させない |

主要リスクと対応:

- **IRAM 残 1 byte**: 新 lib に `IRAM_ATTR` を付けない。付けた瞬間 link が壊れる。実装時の明示禁止事項。
- **AssetPool layout guard**: `ColorPool_t` に色を追加すると `sizeof(StaticAsset_t)` が変わり
  `StaticAssetSizeMismatch` で fail-closed になる。色値は lib 内 constant に置く。
- **描画 frame cost**: 現行1 frameあたり QR module の `fillRect` が 625（25²）〜841（29²）回。
  A1 の追加は plate 1 + frame 4〜8 + badge 1 + hint 1 の 10回未満で、相対増は 2% 未満。
  ただし Live View 中は D2B streaming が並行するため、実測は H12 に委ねる。
- **`_qrBitmap`**: `std::vector<std::vector<bool>>` は既存。style 変更では再生成しない
  （`prepareQr` の cache key は `(state, payload)` のままで、style は含めない）。

`.text` 増分と runtime heap / task stack は本 session では未測定。
実装後の device build と実機測定で確定させる。

---

## 12. Cross-Issue conflict assessment

### Issue #15（reverse-current safety）

- 重複可能性: **低**。
- 対象 file: `app/apps/app_waveform/view/waveform.cpp`、`app/apps/app_waveform/view/recorder.cpp`、
  `app/libs/waveform_scale/`、`app/libs/reverse_current_detector/`、`app/libs/signed_current_observation/`、
  `platforms/vameter/main/hal_vameter/components/`（relay authority）。
- 接点: #15 は Training mode で専用異常画面を出し、`app_waveform` の描画経路に入る。
  Issue #27 は同じ `app_waveform` から呼ばれる `LiveShareView` を変更する。
  **`app_waveform.cpp` の `onRunning()` 周辺で conflict が起きうる**が、#27 側は
  `_render_live_share_view()` の引数を変えない限り `app_waveform.cpp` を触らない設計にできる。
- 色の衝突: #15 は reverse-current 警告表示を導入する。警告色が Issue #27 の Viewer 琥珀
  `0xFFC233` と近いと意味が混線する。色語彙の割り当ては両 Issue で調整すべき。
  これは実装順に依存しない **設計レベルの調整項目**であり、Section 14 に残す。
- 推奨: 独立実装可能。ただし色語彙は先にレビューで確定させる。

### Issue #23（Node / Viewer intake）

- 重複可能性: **ほぼなし**。
- 対象 file: `app/assets/web/`、`app/libs/viewer_asset_contract/`、
  `app/libs/asset_pool_layout/`、`docs/architecture/viewer-assetpool-integration.md`、
  `docs/development/node-viewer-intake.md`、`tests/viewer_asset_contract/`、`tests/asset_pool_layout/`。
- 接点: #23 は `StaticAsset_t` の `WebPage` 領域と AssetPool layout version を扱う。
  Issue #27 は A1 を採る限り `StaticAsset_t` に触れないため衝突しない。
- **ただし A3（icon asset 追加）を採用すると `ImagePool_t` が変わり、#23 の layout version
  管理と正面衝突する。** A3 を採らない判断の実務的な根拠のひとつ。
- `docs/development/` に本 file を追加する点のみ #23 の `node-viewer-intake.md` と同ディレクトリだが、
  file 単位で独立している。

### 将来の実装 branch

設計レビュー完了後、その時点の `main` から新規に作成することを推奨する。

```
feat/issue-27-distinguish-live-view-qr
```

本 session では作成しない。`design/issue-27-qr-distinction` は本設計 file のみを保持する。

---

## 13. Future implementation/PR plan

1. 本書をレビューし、Section 14 の open question を確定する（特に配色・badge文言・hint行数）。
2. レビュー確定後、その時点の `main` から `feat/issue-27-distinguish-live-view-qr` を作成する。
3. commit 分割案:
   1. `feat(live-view): add QR presentation style selector` — 新 lib と host test のみ。描画未変更。
   2. `feat(live-view): distinguish Wi-Fi and Viewer QR screens` — `live_share_view.cpp` の描画変更。
   3. `docs(issue-27): record host validation result` — host test 結果の記録。
4. 各 commit で `git diff --check` / `git status -sb` / desktop build / `ctest` を実行する。
5. ESP-IDF v5.1.6 環境で device build を行い、partition overflow と
   `sdkconfig` / `dependencies.lock` の意図しない差分がないことを確認する。
6. 実機検証（Section 10）は `docs/ai/physical-validation-and-rollback.md` の gate に従い、
   frozen build・readback・rollback 手順を経てから release qualification 証跡とする。
7. PR は実装 branch で作成する。本 design branch からは PR を作成しない。

---

## 14. Open questions

1. **配色の最終決定**: `0x14213D`（CR 9.90、最大輝度差）か `0x1B3A6B`（CR 6.98、反射に強い想定）か。
   H06 の実機確認なしには決められない。暫定は `0x14213D` + `0xFFC233`。
2. **色定数の置き場所**: `ColorPool_t::AppWaveform_t` に追加すると AssetPool layout 変更になる。
   lib 内 constant を推奨するが、theme 一元管理の方針と衝突しないか要判断。
3. **badge 文言**: `"WI-FI"` / `"VIEWER"` でよいか。`"1/2"` / `"2/2"` の順序 cue を併記するか。
   Font36 は ASCII のみで、日本語 badge は efont 36pt が存在しないため現状不可。
4. **hint 行の削減可否**: 現行3行（`Join device Wi-Fi` / `Encoder: Next` / `Side: Stop`）を
   1行へ集約する案で運用上の情報が足りるか。
5. **quiet zone 2 module の妥当性**: 4 module を確保するには QR を縮小するか header/hint を削る必要がある。
   2 module で実機 scan が安定するか（H03）。
6. **非QR state の扱い**: Starting / Stopping / StopRecovery / StartError の背景を `0xFF6161` のまま
   据え置く前提だが、Wi-Fi QR が濃紺になると遷移時の色変化が大きい。据え置きでよいか。
7. **Issue #15 との色語彙調整**: reverse-current 警告色と Viewer 琥珀の識別をどう担保するか。
8. **Settings Network の2段 QR**: 同種の取り違えリスクがあるが本 Issue の scope 外とした。
   別 Issue として起票すべきか。
9. **`LiveShareView` の配置**: `app_power_monitor/view/` にあるが利用は `app_waveform` のみ。
   移動は別 Issue とすべきか。
10. **`meter_help_qr` との geometry 算式重複**: 本 Issue では重複を許容する判断だが、
    将来的に共通 `qr_plate` lib へ統合するか。

---

## 15. Claim boundary

本書が **確立していること**:

- base commit `3348bc86d5d911e131315ad35d6cec022ab01a59` における現行 QR 描画経路の source 上の事実。
- 両 QR 画面の差分が Font14 の2行のみであること。
- `RenderQRCodeBitmap` が quiet zone を描画しないこと。
- 各 payload の QR module 数（qrcodegen を host で直接実行して算出）。
- `Font.montserrat_semibold_36` が既存で ASCII 全域を含むこと（.vlw の glyph table 解析）。
- 配色候補の WCAG 相対輝度・コントラスト比（算式による計算値）。
- `ImagePool_t` / `ColorPool_t` 変更が `asset_pool_layout` guard を trip させること（source 上の構造）。

本書が **確立していないこと**:

- 実機 LCD 上の見え方、識別距離、教室照明下の視認性。
- 現行 quiet zone 0 が実際の scan 失敗を起こしているか。
- 提案 layout 数値（plate 174 / y=44 / badge baseline 等）の実画面上の妥当性。
  desktop simulator でも未確認。
- 提案配色が実機 panel（`cfg.invert = true` 設定の ST77xx 系）でどう再現されるか。
- 描画追加後の `.text` 増分、runtime heap、task stack、D2B streaming への影響。
- 色覚特性を持つ実利用者による識別可否。
- host test / desktop build / device build のいずれの実行結果（本 session では未実行）。

本書のレビュー完了は、設計方針の合意を意味する。
実装完了、host 検証完了、実機 qualification のいずれも意味しない。
Issue #27 の close 条件は実機での受入条件確認を含み、本書だけでは満たされない。

---

ISSUE27_DESIGN_READY_FOR_REVIEW
IMPLEMENTATION_NOT_STARTED
