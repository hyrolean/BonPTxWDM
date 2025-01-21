# BonPTxWDM BonDriver_PTxWDM.dll
BonDriver optimized to [pt2wdm](https://www.vector.co.jp/soft/winnt/hardware/se507005.html) for EARTHSOFT PT1/PT2/PT3.

## 構築と動作確認

  ソリューションを構築する前に以下のファイルを配置する必要があります。

  - Common/inc に以下のファイルを配置
    - PtDrvIfLib.h  (pt2wdmドライバの中に入っているもの)

  - Common/lib に以下のファイルを配置
    - PtDrvIfLib.lib  (pt2wdmドライバの中に入っているもの)
    - PtDrvIfLib64.lib  (pt2wdmドライバの中に入っているもの)

  ソリューションBonPTxWDM.slnをコンパイルして出来上がったBonDriver_PTxWDM.dllと以下のファイル群をアプリ側のBonDriverフォルダに配置して動作確認…
  ```
  BonDriver_PTxWDM-S.dll ← BonPTxWDM BS/CS110 複合チューナー S側ID自動判別 (BonDriver_PTxWDM.dllをリネームしたもの)
  BonDriver_PTxWDM-T.dll ← BonPTxWDM 地デジ 複合チューナー T側ID自動判別 (BonDriver_PTxWDM.dllをリネームしたもの)
  BonDriver_PTxWDM.ini ← BonPTxWDM 用設定ファイル
  BonDriver_PTxWDM.ch.txt ← BonPTxWDM 用チャンネルファイル (配置しなくてもおｋ)
  PTxWDMCtrl.exe ← １プロセス多チューナー構成用制御プログラム (Spinelなどのプロクシソフト配下で使用する場合に配置)
  ```

  自動判別ではなく、IDで指定する場合は、次のように連番で記述してください。
  PTxWDMで連番を利用する場合は以下の通りです。
  ```
  BonDriver_PTxWDM-S0.dll ← BS/CS110 複合チューナーの最初のチューナーのS1端子のチューナー (BonDriver_PTxWDM.dllをリネームしたもの)
  BonDriver_PTxWDM-T0.dll ← 地デジ 複合チューナーの最初のチューナーのT1端子のチューナー (BonDriver_PTxWDM.dllをリネームしたもの)
  BonDriver_PTxWDM-S1.dll ← BS/CS110 複合チューナーの最初のチューナーのS2端子のチューナー (BonDriver_PTxWDM.dllをリネームしたもの)
  BonDriver_PTxWDM-T1.dll ← 地デジ 複合チューナーの最初のチューナーのT2端子のチューナー (BonDriver_PTxWDM.dllをリネームしたもの)
  BonDriver_PTxWDM-S2.dll ← BS/CS110 複合チューナーの二枚目のチューナーのS1端子のチューナー (BonDriver_PTxWDM.dllをリネームしたもの)
  BonDriver_PTxWDM-T2.dll ← 地デジ 複合チューナーの二枚目のチューナーのT1端子のチューナー (BonDriver_PTxWDM.dllをリネームしたもの)
  BonDriver_PTxWDM-S3.dll ← BS/CS110 複合チューナーの二枚目のチューナーのS2端子のチューナー (BonDriver_PTxWDM.dllをリネームしたもの)
  BonDriver_PTxWDM-T3.dll ← 地デジ 複合チューナーの二枚目のチューナーのT2端子のチューナー (BonDriver_PTxWDM.dllをリネームしたもの)
  BonDriver_PTxWDM-S4.dll ← BS/CS110 複合チューナーの三枚目のチューナーのS1端子のチューナー (BonDriver_PTxWDM.dllをリネームしたもの)
  BonDriver_PTxWDM-T4.dll ← 地デジ 複合チューナーの三枚目のチューナーのT1端子のチューナー (BonDriver_PTxWDM.dllをリネームしたもの)
  BonDriver_PTxWDM-S5.dll ← BS/CS110 複合チューナーの三枚目のチューナーのS2端子のチューナー (BonDriver_PTxWDM.dllをリネームしたもの)
  BonDriver_PTxWDM-T5.dll ← 地デジ 複合チューナーの三枚目のチューナーのT2端子のチューナー (BonDriver_PTxWDM.dllをリネームしたもの)
  ```

  **※β版です。無保証( NO WARRANTY )です。とりあえず、一連の動作に支障なく動く程度に仕上がっているとは思いますが、テスト期間が短い為、潜在的なバグについては未知数です。**

  P.S. 2025/1/19 更新版より、BonDriver_PTx-ST_mod に付属の[PTxScanS](https://github.com/hyrolean/BonDriver_PTx-ST_mod/tree/master/PTxScanS)を使用することにより、トランスポンダを利用したチャンネルスキャンを行うことができるようになりました。
  使い方は下記のように +df ｵﾌﾟｼｮﾝを用いると、.CSV.txt という拡張子のファイルが生成されますので、.ch.txt に変更するとチャンネルファイルとしてそのまま取り扱うことが可能です。 是非トライしてみてください。
  ```
  PTxScanS.exe +df BonDriver_PTxWDM-S0.dll
  ```

## TODO
- [x] EDCB/TVTest配下での動作(１プロセス１チューナー構成)
- [x] Spinelなどのプロクシソフト配下での動作(１プロセス多チューナー構成)
- [x] PTxWDMCtrl.exeとのバッファ通信の最適化
- [x] LNB供給の不具合を修正 

