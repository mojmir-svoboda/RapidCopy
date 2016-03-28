# RapidCopy for Linux

##### 概要

Windows系最速(?) のファイルコピー＆削除ツールFastCopy v2.11のLinux移植/独自発展版です。

RapidCopy is portable version of FastCopy(v2.11) that runs GNU/Linux.

Linux版はFastCopyV2.11をベースとして作成した
Mac移植版[RapidCopy]v1.1.1をベースとして作成されています。

##### ライセンス
全ソースを二条項BSDライセンスで公開しています。

##### 仕様／使用方法

[help]フォルダ内のhtmlファイルを参照してください。
基本的な仕様は[Mac版]と変わりません。

##### ビルドについて
以下の２種類のディストリビューションでコンパイル、起動確認をしてあります。
OS:CentOS7(x64)
必須ライブラリ:libacl,libbsd,libattr

OS:Ubuntu14.04(x64)
必須ライブラリ:libattr1-dev,libbsd-dev,libacl1-dev,libgl1-mesa-dev

いずれの環境でもコンパイルにはQt5.4.1以降必須です。
環境によってはg++などのインストールが必要かもしれません。

##### TODO
- 未使用変数及びインタフェースのリファクタリング,コンパイラ警告潰し,未使用コード整理
- ご本家FastCopy3.xxの各種新機能の取り込み
- GNOME,KDEなどの各種デスクトップ環境へのインストール対応
- Ubuntu Software Centerへの対応
- etc....

   [help]: <https://github.com/KengoSawa2/RapidCopy/tree/master/help>
   [RapidCopy]: <https://itunes.apple.com/jp/app/rapidcopy/id975974524>
   [Mac版]: <http://www.lespace.co.jp/file_bl/rapidcopy/manual/index.html>
