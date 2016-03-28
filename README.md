# RapidCopy for GNU/Linux

#### 概要(About)

Windows系最速(?) のファイルコピー＆削除ツールFastCopy v2.11のGNU/Linux移植 独自発展版です。  
RapidCopy is portable version of FastCopy(v2.11) that runs GNU/Linux.  

Linux版はFastCopyv2.11のMac移植版である[RapidCopy]v1.1.0をベースとして作成されています。  
Linux ver is made of RapidCopy(for Mac)v1.1.0  

#### ライセンス(License)
全ソースを二条項BSDライセンスで公開しています。  
Source Code license is [BSD 2-Clause License]  

#### 仕様／使用方法(Specification/Usage)

[help]フォルダ内のhtmlファイルを参照してください。  
Plz read [help] file  
基本的な仕様は[Mac ver]と共通です。  
Basically, usage and specification is same as the Mac version.  

#### ビルドについて(to Build)
以下のディストリビューションでコンパイル、起動確認をしています。  
We checked next Linux distribution.  

OS:CentOS7.1(x64)  
必須ライブラリ(Required Library):libacl,libbsd,libattr  

OS:Ubuntu14.04 LTS(x64)  
必須ライブラリ(Required Library):libattr1-dev,libbsd-dev,libacl1-dev,libgl1-mesa-dev  

いずれの環境でもコンパイルにはQt5.4.1以降必須です。  
環境によってはg++などのインストールが必要かもしれません。  
In either environment Qt5.4.1 has to be always installed.  
You might need to be installed other library, such as g++,depending on your environment.  

デフォルトではubuntu14.04をターゲットとしています。  
CentOS7でコンパイルする場合はrapidcopy_main.proのLIBS行を変更してください。  
Default target is ubuntu 14.04(LTS)  
Please change the LIBS line of rapidcopy_main.pro If you want to compile in CentOS7.  

#### TODO
- 未使用変数及びインタフェースのリファクタリング,コンパイラ警告潰し,未使用コード整理(code refactoring)
- ご本家FastCopy3.xxの各種新機能の取り込み(The great original FastCopyv3.xx function import)
- GNOME,KDEなどの各種デスクトップ環境へのインストール対応(support GNOME,KDE install/uninstall)
- Ubuntu Software Center対応(support Ubuntu Software Center)
- etc....

   [help]: <https://github.com/KengoSawa2/RapidCopy/tree/master/help>
   [RapidCopy]: <https://itunes.apple.com/jp/app/rapidcopy/id975974524>
   [Mac ver]: <http://www.lespace.co.jp/file_bl/rapidcopy/manual/index.html>
