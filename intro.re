= はじめに
本書をお手に取っていただきありがとうございます。

本書は64ビットのx86 CPU(x86_64)を対象にフルスクラッチで"OS"を作ってみる本です。本書では主に"カーネル"と呼ばれるハードウェアの機能を上位のアプリケーションに向けて抽象化する部分を作ってみます。

#@# 本書では以下をフルスクラッチで扱う方法を紹介します。

#@#  * フレームバッファ(画面描画)
#@#  * キーボードコントローラ(キーボード入力)
#@#  * 割り込みコントローラ(割り込み制御)
#@#  * ファイルシステム(メモリ上に作る簡易ファイルシステム)

そして、上記を使用したアプリケーション(的なもの)として、本書の最後に「画像ビューア」を作ってみます。本書ではカーネルとアプリのバイナリレベルでの分離などは行わないため、アプリケーションはカーネルのバイナリに含まれたものとなります(そのため、「アプリケーション(的なもの)」です)。

本書で作成するカーネルとアプリケーションの構成は@<img>{intro}の通りです。

//image[intro][本書で作成するソフトウェア構成]{
//}

== 動作確認しているハードウェアとブートローダーについて
本書ではカーネルとそれより上位のソフトウェアレイヤーをすべてフルスクラッチで自作します。

カーネルを起動させるためのブートローダーと、それらを動作させるハードウェア環境は筆者は以下で動作確認しています。

 * ハードウェア
 ** CPUエミュレータ: QEMU
 ** 実機: Lenovo製 Thinkpad E450
 * ブートローダー
 ** poiboot

ハードウェアについて、本書の内容を試す際は、まずはQEMUで試してみることをおすすめします。実機での動作確認は筆者の所持している64ビットPCの都合上、Lenovo製Thinkpad E450のみです。実機で試す際におそらく問題となるのは3章で、いわゆる「PS/2マウス・キーボード」を制御するためのIC@<intro_jissai_kbc>であるキーボードコントローラ(KBC)を使用します。ノートPCの場合、内蔵されているキーボードが内部的にはKBCに接続されていたりしますが、デスクトップPCの場合、PS/2コネクタが搭載されていない場合はそもそもKBCも搭載されていないかと思われます@<fn>{moshikashitara_kbc}。
//footnote[intro_jissai_kbc][今はSoCとしてCPUに集積されています。]
//footnote[moshikashitara_kbc][KBCと言っても今やCPU(SoC)集積されている「KBC互換の何か」なので、もしかするとPS/2コネクタが無い様なマザーボードにはUSB対応のKBC互換の何かが搭載されているのかも知れませんが試したことも無く、わかりません。]

ブートローダーは"poiboot"というものを使用します。実はこれは本書のシリーズである「フルスクラッチで作る!UEFIベアメタルプログラミング」のパート1(無印)とパート2で説明した内容に少し手を加えて作ったものです。ブートローダーは本書の範囲を超えるため、手を加えた内容については本書の「付録」で簡単に説明するにとどめています。

== 開発環境について
本書で行う開発は、以下の環境で行うものとして説明します。

 * CPU: x86_64
 * OS: Debian GNU/Linux 8.0(Jessie)

基本的に使用するツールはエディタ(お好きなものを)、コンパイラ(GCC)、make程度なので、それらのツールのWindows版を使用する等すれば他のOSでも開発を行うことができるかと思います。

ただし、5章でシェルスクリプトを作成する箇所や、6章で"ImageMagick"という画像処理系のツールセットを使用する箇所があります。Linux以外のOSで開発する場合は適宜読み替えや、Windows版の導入等を行ってください。

== 本書のPDF版やサンプルコードの公開場所について
これまでの当サークルの同人誌同様、本書もPDF版は以下のページで無料で公開しています。

 * @<href>{http://yuma.ohgami.jp}

また、本書で扱うサンプルコードとコンパイル済みバイナリは以下のGitHubで公開しています。

 * ソースコード
 ** @<href>{https://github.com/cupnes/x86_64_jisaku_os_samples}
 * コンパイル済みバイナリ
 ** @<href>{https://github.com/cupnes/x86_64_jisaku_os_samples/releases}

サンプルコードは節ごとにディレクトリが分かれています。本書の各節冒頭で「この節のサンプルコードは"～"のディレクトリです」のように説明していますので、適宜参照してください。
