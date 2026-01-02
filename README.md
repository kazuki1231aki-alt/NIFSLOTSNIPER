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

重要】 現在のバージョン(v0.1)では、slotdata-*.txt の書き出し機能が未実装のため、スロット番号の変更はテキストファイルを直接手動で編集してください。（次回以降のアップデートでツール内編集に対応予定）

[Important] In version v0.1, Nif Slot Sniper cannot yet export changes to the text file. To change slot numbers, you must manually edit slotdata-*.txt directly. (In-tool editing is planned for a future update.)
### Workflow Diagram
```mermaid
graph TD
    %% 外部データ
    LO[Your Esps LoadOrder] --> SE[1st step>synthesis patch: slotExporter]

    %% エクスポート工程
    SE -- "1.1 Export Slot Data" --> EX_TXT((slotdata-*.txt))
    
    %% ★ここを修正：テキストを "" で囲みました
    Manual["Edit Rules Here<br/>(Define Slot Changes)"] <--> EX_TXT
    style Manual fill:#ffcccc,stroke:#ff0000,stroke-width:2px,color:#000
    
    %% Nif Slot Sniper側の工程
    subgraph "Nif Slot Sniper (Mesh Side)"
        EX_TXT -- "2.1 Import Text & Read Path" --> NSS[2nd step>Nif Slot Sniper]
        NSS -- "2.2 Locate & Load" --> NIF_IN[.NIF File]
        NIF_IN -- " 2.3 Data for Editing" --> NSS
        NSS -- "2.4 Save Updated NIF" --> NIF_OUT[.NIF File]
    end
    
    %% Synthesis側の工程
    subgraph "Synthesis (ESP Side)"
        EX_TXT -- "3.1 Read Text" --> SI[3rd step>synthesis patch: slotImporter]
        LO --> SI
        SI -- "3.2 Update ESP Data" --> ESP_OUT[.esp] ==> FESP[3.3 end of ESP side slot changing]
    end
    
    %% 最終結果
    NIF_OUT ==> FNIF[2.5 end of NIF side slot changing]
    
    %% スタイル設定
    style NSS fill:#d4edda,stroke:#28a745
    style SI fill:#fff3cd,stroke:#ffc107
    style EX_TXT fill:#f8d7da,stroke:#dc3545
