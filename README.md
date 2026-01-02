# NIFSLOTSNIPER
This tool modifies binary .nif files and launches external patchers like Synthesis, which may cause some antivirus software to trigger a 'False Positive' alert.
Unzip glm.zip in the include folder
# NIF Slot Sniper

**NIF Slot Sniper** は、Skyrim 等のゲームで使用される `.nif` ファイルのパーティション（スロット）割り当てを視覚的に確認・編集するための軽量なツールです。

A lightweight tool to visually inspect and edit partition (slot) assignments for `.nif` files used in games like Skyrim.
---

## 🛠 Features (主な機能)

- **Visual Inspection**: OpenGL 3.3 を使用した 3D プレビュー機能。
- **Slot Management**: NIF ファイル内の各メッシュのスロット番号を直接書き換え。
- **Batch Processing**: 指定したフォルダ内のメッシュを一括スキャン。
- **Integration**: Synthesis.exe との連携機能。

## ⚠️ Security Note (セキュリティについて)

### Important: False Positive Alerts
Some antivirus software (including Windows Defender) may flag this tool as a "Trojan" or "Malicious." This is a **False Positive** caused by the following technical behaviors:
- **File Manipulation**: Directly reading/writing binary `.nif` files.
- **Process Launching**: Launching external tools like `Synthesis.exe` via `ShellExecute`.
- **Lack of Digital Signature**: As an independent open-source tool, the executable is not digitally signed.

Since the source code is fully open here, you can verify that it is safe and contains no malicious code.

### 誤検知に関する注意
Windows Defender 等のセキュリティソフトにより「トロイの木馬」等の警告が出る場合がありますが、これは以下の挙動による**誤検知**です。
- バイナリファイル（.nif）の直接的な読み書き。
- `ShellExecute` による外部ツール（Synthesis.exe）の起動。
- デジタル署名がない実行ファイルであること。

ソースコードをすべて公開していますので、安全性を確認した上でご利用いただけます。

---

## 🚀 How to Use (使い方)

1. **Select Data Folder**: `Data/meshes` フォルダを選択します。
2. **Scan**: 「NIF Scan」ボタンでファイルをリストアップします。
3. **Edit**: プレビューで確認しながら、スロット番号を変更します。
4. **Save**: 「Save as New NIF」で変更を保存します。

---

## 🏗 Requirements (動作環境)

- Windows 10/11 (64-bit)
- OpenGL 3.3 support
- Required Libraries: `nifly`, `ImGui`, `GLFW`, `glad`, `glm`

---

## 📜 License

This project is licensed under the MIT License.
