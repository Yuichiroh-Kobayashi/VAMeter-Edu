# VAMeter-Edu（日本語）

[M5Stack VAMeter](https://docs.m5stack.com/en/products/sku/K136) 用の教育向けファームウェアです。

目に見えない電気を、**波形表示(簡易オシロスコープ)** と **デジタル値** で可視化し、初学者の学習をサポートします。

> **English version**: [README.md](README.md)

---

## 特徴

### 教育向け機能

- **メニューの簡略化**：電圧計／電流計／USB-C電力計／設定の4画面に整理
- **接続ガイド画面**：回路の正しい接続方法を図示
- **固定表示モード**：誤操作によるページ切り替えを防止
- **日本語UI**：教室での運用を想定した日本語表示

### ローカルデータダウンロード（v1.1.0〜）

VAMeter 本体が **Wi-Fi アクセスポイント(AP)** として動作し、画面に表示される **QRコード** からアクセスすることで、**CSV データを端末へ直接ダウンロード** できます。**インターネット接続やクラウドアカウントは不要** です。

- 学校ネットワークのアクセス制限を回避しやすい
- CSV は生徒端末側で表示・加工・共有が可能
- ※ v1.1.0 では、従来の **EzData へのアップロード機能を廃止**し、ローカルダウンロード方式に変更しました

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

### v1.1.0（2026-01-01）

- **ローカルダウンロード機能**：クラウドアップロードに代わり、QR コード経由でのローカルダウンロードに変更
- **AP サフィックス設定**：複数台使用時の識別用（01〜40）
- **バグ修正**：電圧計/電流計からの遷移後に波形記録が開始されない問題を修正

詳細は [CHANGELOG.md](CHANGELOG.md) を参照してください。

### ビルド方法

#### 前提条件

[ESP-IDF v5.1.3](https://docs.espressif.com/projects/esp-idf/en/v5.1.3/esp32s3/index.html)

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

## 今後の開発予定

- アクセシビリティ機能（音声読み上げ）
- 多言語 UI
- コンテキストに応じたヘルプボタン
- 表示値補正（校正）ワークフローの改善・自動化

コントリビューション歓迎です！

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
