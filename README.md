# RapidCopy for Linux

##### 概要

Windows系最速(?) のファイルコピー＆削除ツールFastCopy v2.11のLinux移植版です。

RapidCopy is portable version of FastCopy that runs GNU/Linux.

Linux版はMac移植版である[RapidCopy]v1.1.1をベースとしています。

##### ライセンス
全ソースを二条項BSDライセンスで公開しています。

##### 仕様／使用方法

[help]のhtmlファイルを参照してください。
基本的な仕様は[Mac版]と変わりません。

##### ビルドについて
OS:CentOS7(x64)
コンパイラ:gcc4.8.5
必要ライブラリ:Qt5.4.2以降とlibacl,libbsd,libattrが必要です。

##### TODO
- 未使用変数及びインタフェースのリファクタリング,コンパイラ警告潰し,コード整理
- ご本家FastCopy3.xxの各種新機能の取り込み
- GNOME,KDEなどの各種デスクトップ環境へのインストール対応


   [help]: <https://github.com/KengoSawa2/RapidCopy/tree/master/help>
   [RapidCopy]: <https://itunes.apple.com/jp/app/rapidcopy/id975974524>
   [Mac版]: <http://www.lespace.co.jp/file_bl/rapidcopy/manual/index.html>
