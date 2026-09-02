# VAMeter-Edu（日本語）

[M5Stack VAMeter](https://docs.m5stack.com/en/products/sku/K136) 用の教育向けファームウェアです。日本の中学校で、実際の回路操作、電圧・電流の観察、短時間の明示的な記録、ローカル CSV のグラフ化を結び付けることを目的としています。

目に見えない電気を、**波形表示(簡易オシロスコープ)** と **デジタル値** で可視化し、初学者の学習をサポートします。

> **English version**: [README.md](README.md)

> **最新の安定版:** [`v2.0.0`](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/releases/tag/v2.0.0)（2026-09-02）

> **M5Stack Global Innovation Contest 2026 Special Mentions - Educational Impact Award 受賞**
>
> VAMeter-Edu は [M5Stack Global Innovation Contest 2026](https://m5stack.com/global-innovation-contest-2026/results) で教育的な取り組みを評価されました。コンテストではブラウザで電圧・電流をリアルタイム観察するデモを実現しました。その後、device-hosted Viewer、接続動作の安定性、再現可能な検証、教室で扱いやすい操作フローを整え、コンテスト用デモから実際の教室運用を想定した安定版へ発展しました。

---

## 特徴

### 教育向け機能

- **メニューの簡略化**：電圧計／電流計／USB-C電力計／設定の4画面に整理
- **接続ガイド画面**：回路の正しい接続方法を図示
- **固定表示モード**：誤操作によるページ切り替えを防止
- **日本語UI**：教室での運用を想定した日本語表示

### Device-hosted Viewer（`v2.0.0`）

VAMeter 本体が Wi-Fi アクセスポイントと same-origin Web Viewer を提供します。生徒や教員は本体へ直接接続し、インターネット接続やクラウドアカウントなしで Viewer を開けます。

- **Student / Professional** の表示モード
- **Voltage / Current / Both** の測定プロファイル
- **10 / 30 / 60 秒**の表示時間幅
- device timestamp を使用し、欠落を補間せず invalid sample を 0 にしない波形表示
- OS／ブラウザ設定に追従する **Light / Dark** theme と、ページを閉じるまで有効な手動切り替え
- **Windows の Microsoft Edge 151** による実機ブラウザ検証
- **iPad 第7世代 / iPadOS 18.7.9 / Safari** による実機 smoke 検証

これらの browser test で確認したのは device-hosted streaming と lifecycle の動作です。電気測定精度を再認定したものでも、新しい校正証明でもありません。

### 教育用記録とローカルデータダウンロード

現在の fallback recorder は、手動開始した **5秒の教育用記録**を保存します。画面に表示される **QRコード** から、CSV データを端末へ直接ダウンロードできます。**インターネット接続やクラウドアカウントは不要** です。

- 学校ネットワークのアクセス制限を回避しやすい
- CSV は生徒端末側で表示・加工・共有が可能
- ※ v1.1.0 では、従来の **EzData へのアップロード機能を廃止**し、ローカルダウンロード方式に変更しました
- 同期 FAT 書き込みによる sampling stall は [Issue #3](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3) として継続中で、**gap-free CSV は保証しません**

また、複数台を同時に使用する教室運用を想定し、AP 名称の識別用に **AP サフィックス(01〜40)** を設定できます。

### 安全機能（教室運用向け）

- **OTA アップグレード無効化**：教室での誤ったファームウェア変更を防止
- **ファクトリーリセット無効化**：誤操作による設定初期化を防止

---

## ハードウェア

VAMeter-Edu は **M5Stack VAMeter** を対象としたファームウェアです。必要に応じて、専用ケースやプローブを追加して運用できます。

### Normal Probe（通常プローブ）

市販のテストリードを使用した通常の電圧・電流測定用です。4mm バナナジャック接続。

### Training Probe（トレーニングプローブ）

アナログメーターの読み取り練習用です。アナログメーターと VAMeter-Edu を同時に使用できます（RCA 端子）。

### 3D プリントケース・パーツ

Hackster.io プロジェクトページから以下の情報を参照できます：

- **Custom parts and enclosures**：STL ファイル（3D プリント用）
- **Schematics**：回路図

👉 [Hackster.io プロジェクトページ](https://www.hackster.io/Yuichiroh-Kobayashi/vameter-edu-easy-tester-for-everyone-learning-electricity-9d06c6)

---

## ファームウェア

### v2.0.0（2026-09-02）

[`v2.0.0`](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/releases/tag/v2.0.0) は、上記の device-hosted Viewer と5秒の教育用 CSV workflow を教室向けにまとめた安定版です。

Application と AssetPool のバイナリは、対応するリリースの組み合わせです。**異なる VAMeter-Edu バージョンのバイナリを混在させないでください。** 安定版 Release から各ファイルをダウンロードし、書き込み前に公開された `SHA256SUMS` で確認してください。

| Release identity | 値 |
|---|---|
| Release URL | [`v2.0.0`](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/releases/tag/v2.0.0) |
| Firmware tag | `v2.0.0` |
| Annotated tag object | `f862754cadd3ddf485b0ed5b9312507c7df15853` |
| Firmware commit | `ee4da1b5e5e238fbc66a9d9a49f4d051c1ca986b` |
| Firmware tree | `f80a0caaa213a965033f6773ea2d3f41af436807` |
| Viewer source commit | `e1ebdb1cde8585a37447a66f4c8183654f4c3cda` |
| Viewer source tree | `8f8426e9af1649f68e66e4f8f432d1b91452e38d` |
| D2B repository reference commit | `b30ad676922af73448952d5a9cac312467a944f9` |
| D2B repository commit tree | `22d644546a8ae76559bb7f1ec01fa737c7160886` |
| Viewer copied `reference/browser/src` subtree | `6e5b4844548c1355dea7e5cbbcb1200c9d2335fd` |
| Viewer bundle | `4422530b6e1ba9549dd4bef2e3bb2c183d8fced49ed2d8d695d2a04a4aa7c2af` |
| app SHA-256 | `4a54b7addb69c89497de585a200de5c3976b21c2a0a2e50ee4fb0d6d0e51198a` |
| AssetPool SHA-256 | `3a587a04127a5eab4df0d0714e37e214029bcedbbeb6a616e8426c6e9aa1c1fc` |
| `SHA256SUMS` SHA-256 | `40558a943ef194dd7931cc5fe8fe8a9517928dcf09c9443af5d05bbf4abf84e0` |

現在の制約：

- [Issue #3](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3) は OPEN で、gap-free CSV も均一 sampling も保証しません。
- static IRAM 使用量は `16383 / 16384` bytes です。リソース余裕は極めて小さく、技術的負債として残っています。
- 複数クライアントの製品方針は [Issue #8](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/8) で今後検討します。
- ブラウザ／ストリーミングの検証は、電気測定の校正証明ではありません。

### v2.0.0-beta.1（2026-08-21、過去のプレリリース）

この過去の beta では、device-hosted same-origin Viewer、Student / Professional、Voltage / Current / Both、10 / 30 / 60 秒の表示時間幅、5秒の教育用 CSV workflow を追加・整理しました。

公開した application と AssetPool は対応する release binary の組み合わせです。**異なる version の binary を混在させないでください。** 書き込み前にダウンロードしたファイルを確認してください。

| Release identity | 値 |
|---|---|
| Firmware released commit | `6f769485f5f1de119d7b7edcc38f724620dc2ac7` |
| Viewer commit | `105bca2616ef372fe23ac0797f58b5c7383ee20c` |
| D2B commit | `5411ba59a12882345d32218eda367bd6ba35ef5d` |
| Viewer bundle | `cbcbd7eab111b49c0c6119b22a7f50ae55981933fd799abfd98d92d0dc5d96e5` |
| app SHA-256 | `9b872ea5cc483b361bba9550e1878b9036d205ee830fd84a0e321d3f3a732423` |
| AssetPool SHA-256 | `1df4b81fba8b3f16baf1331f015cdb1fdc7214d66a215657ef752673b43c1c41` |

当時の注意事項：

- [Issue #3](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3) は OPEN で、gap-free CSV も均一 sampling も保証しません。
- static IRAM 使用量は `16383 / 16384` bytes です。resource headroom は極めて小さく、firmware feature を追加する前に review が必要です。
- この beta は安定版 `v2.0.0` ではありません。

### v1.1.0（2026-01-01）

- **ローカルダウンロード機能**：クラウドアップロードに代わり、QR コード経由でのローカルダウンロードに変更
- **AP サフィックス設定**：複数台使用時の識別用（01〜40）
- **バグ修正**：電圧計/電流計からの遷移後に波形記録が開始されない問題を修正

詳細は [CHANGELOG.md](CHANGELOG.md) を参照してください。

### ビルド方法

#### 前提条件

[ESP-IDF v5.1.6](https://docs.espressif.com/projects/esp-idf/en/v5.1.6/esp32s3/index.html)

#### 依存関係の取得

```bash
python ./fetch_repos.py
```

#### ビルド

```bash
cd platforms/vameter
idf.py build
```

#### 書き込み

```bash
idf.py -p <ポート名> flash -b 1500000
```

#### AssetPool の書き込み

```bash
parttool.py --port <ポート名> write_partition --partition-name=assetpool --input "path/to/AssetPool-VAMeter.bin"
```

---

## Roadmap

- [同期 FAT 書き込みによる recorder の sampling stall 改善](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/3)
- [逆方向電流の表示と Training mode の保護動作](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/15)
- [Multi-client product policy](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/8)
- [アナログ計器の答え合わせ用表示補正](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/9)
- [内部電源式 V-I logger の検討（hardware redesign 待ちで一時停止中）](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu/issues/11)

---

## コントリビューション

不具合報告、ドキュメントの改善、目的を絞った pull request を歓迎します。

Issue や PR を作成する前に [CONTRIBUTING_ja.md](CONTRIBUTING_ja.md) をご確認ください。ファームウェア、ブラウザ Viewer、汎用 D2B プロトコルのどこが担当する問題か分からない場合は、問題に気付いた場所と状況を記載してください。メンテナーが適切なリポジトリへの案内や相互リンクを行えます。

English: [CONTRIBUTING.md](CONTRIBUTING.md)

---

## クレジット

- **開発者**: Yuichiroh-Kobayashi
- **謝辞**: [@M5Stack](https://github.com/m5stack) - オリジナル VAMeter ファームウェア

---

## 関連リンク

- [Hackster.io プロジェクトページ](https://www.hackster.io/Yuichiroh-Kobayashi/vameter-edu-easy-tester-for-everyone-learning-electricity-9d06c6)
- [オリジナル VAMeter ファームウェア](https://github.com/m5stack/VAMeter-Firmware)
- [M5Stack VAMeter 製品ページ](https://docs.m5stack.com/en/products/sku/K136)

---

## ライセンス

MIT License - 詳細は [LICENSE](LICENSE) を参照してください。
