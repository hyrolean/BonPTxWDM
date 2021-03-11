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
  PTxWDMCtrl.exe ← Spinelなどのプロクシソフト配下で使用する場合に配置 (１プロセス多チューナー構成用制御プログラム)
  ```

  **※β版です。無保証( NO WARRANTY )です。とりあえず、一連の動作には支障なく動く程度には仕上がっているとは思いますが、テスト期間が短い為、潜在的なバグについては未知数です。**


## TODO
- [x] EDCB/TVTest配下での動作(１プロセス１チューナー構成)
- [x] Spinelなどのプロクシソフト配下での動作(１プロセス多チューナー構成)
- [x] PTxWDMCtrl.exeとのバッファ通信の最適化
