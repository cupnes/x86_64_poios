= 開発環境とブートローダーの準備
== 開発環境の準備
本書ではC言語と一部アセンブラを使用して開発を行います。そのため、最低限、「コンパイラ/アセンブラ(GCC)」と「エディタ(好きなものを)」があれば良いです。加えて、作成したバイナリを試すための「エミュレータ(QEMU)」や、ビルドルールを管理するために「make」等を適宜インストールします。

筆者の作業環境の都合上、開発環境構築はDebian(バージョン8 Jessie)で説明しますが、同じくパッケージ管理にAPTを使用するUbuntuでも同様に構築できると思います。

インストールするパッケージは以下の通りです。

 * build-essential
 ** GCCやmake等が含まれるパッケージ
 * qemu-system-x86
 ** 64ビットのx86CPUのエミュレータ
 * ovmf
 ** 仮想マシン向けUEFIファームウェア(後述)

以下のコマンドでインストールできます。

//cmd{
$ @<b>{sudo apt install build-essential qemu-system-x86 ovmf}
//}

Ubuntuにはovmfパッケージが無い様です。結局は"OVMF.fd"というファームウェアバイナリが欲しいだけなので、ovmfパッケージが無ければ以下からバイナリをダウンロードしてください。

 * @<href>{https://sourceforge.net/projects/edk2/files/OVMF/}
 ** 2018/01現在、"OVMF-X64-r15214.zip"がOVMF.fdを含む最新のアーカイブです

//note[OVMF(Open Virtual Machine Firmware)について]{
"ovmf"パッケージはQEMUに含まれていないUEFIファームウェアを追加するためにインストールしています。最近のPCではレガシーBIOSからUEFIへBIOSのファームウェア実装が変わっているのですが、QEMUにはレガシーBIOSしか含まれていないため、"OVMF(Open Virtual Machine Firmware)"という仮想マシン向けのUEFI仕様準拠のファームウェアバイナリをインストールしています。

UEFIについてはGoogle検索でもいろいろと情報が出てきます。本シリーズの前著でも「UEFIファームウェアを直接叩くプログラムを作る」本を出しており、PDF版は無料で公開していますので興味があれば見てもらえると嬉しいです(本書の「参考情報」にURLを記載しています)。
//}

== ブートローダー(poiboot)の準備
=== ブートローダーの役割
ブートローダーについて少し説明します。

PCは電源を入れるとレガシーBIOSやUEFIといったファームウェアが動作します。ファームウェアは、レガシーBIOSであれば起動ディスクの先頭512バイト(マスタブートレコード)を、UEFIであれば起動ディスク第1パーティションの特定のパスにある実行バイナリをメモリへロードし実行します。

ここで実行されるプログラムが一般的にブートローダーと呼ばれるものです。ブートローダーはファームウェアとやり取りして、その次に実行するプログラム(一般的には「カーネル」)をメモリへロードし、ファームウェアとのやり取りを終え@<fn>{why_exit_firmware}、メモリへロードしたプログラムの実行を開始します。
//footnote[why_exit_firmware][ファームウェア動作中はファームウェアが各種ハードウェアを管理しており、特定のメモリ領域が使えなかったりします。カーネル起動後はカーネル側でこれらのリソースを管理したいため、ファームウェアが動作できないモードへ移行します。]

=== 本書で使用するブートローダー"poiboot"について
本書では前著までの「フルスクラッチで作る!UEFIベアメタルプログラミング」シリーズ@<fn>{01_about_ubmp}で紹介した内容とプラスアルファ(付録で簡単に紹介しています)の内容を元に作ったブートローダー"poiboot"を使用します。
//footnote[01_about_ubmp][PDF版は無料で公開していますので興味のある方は「参考情報」の章に記載のURLからダウンロードして見てみてください。]

poibootの動作は@<img>{1_poiboot}の通りです。

//image[1_poiboot][poibootの動作]{
//}

poibootは設定ファイル(poiboot.conf)に記載されているアドレスへカーネルバイナリ(kernel.bin)とファイルシステムイメージ(fs.img)をロードします。

=== poibootをダウンロード
それでは、poibootを準備していきます。

まず、以下のURLからpoibootのアーカイブ"poiboot_20180422.zip"をダウンロードし、展開しておいてください。

 * @<href>{http://yuma.ohgami.jp}

展開すると中には"poiboot.efi"と"poiboot.conf"が入っています。poiboot.efiがブートローダーの実行バイナリです。

poiboot.efiを何処に配置して使用するかは次章で最初のプログラムを作っていく中で紹介します。

=== 設定ファイル(poiboot.conf)について
poiboot準備の最後として、設定ファイル(poiboot.conf)を用意します。設定内容は、poibootがロードするkernel.binとfs.imgそれぞれのロード先アドレスだけです。

なお、poibootのアーカイブに入っているデフォルト値記載済のpoiboot.confをまずは使用してみて、ダメだったらメモリマップ確認(後述)を行う、でも良いかと思います@<fn>{01_sekkaku}。QEMUは仕様が変わらない限りメモリマップは変わらないと思いますし、実機の場合も後述のメモリマップ確認方法で確認した結果が公開されているのをいくつか見る限りは問題無いかと思います。
//footnote[01_sekkaku][ただ、メモリマップ確認にはUEFIシェルというものを使うのですが、自身のPCのハードウェア情報を知る便利なツールなので興味があればぜひ試してみて欲しいです。]

=== メモリマップを見てみる
それでは、自作OSを実行する環境のメモリマップ@<fn>{about_pa_memorymap}を見てみましょう。
//footnote[about_pa_memorymap][厳密には、ここでは「物理アドレス」に対応したメモリマップを確認します。CPUはMMU(メモリ管理ユニット)により変換された「仮想アドレス」で動作し、一般的なカーネルは仮想アドレスのメモリマップを何パターンも持っています。ただしUEFIの場合、ファームウェア側で「物理アドレス=仮想アドレス」となるようにMMUの設定を行ってくれます。本書では「物理アドレス=仮想アドレス」設定のまま使用するため特に仮想アドレスのメモリマップは意識しません。]

メモリマップを見るのに本書では「UEFIシェル」を使います。UEFIシェルはUEFIファームウェアが持つ機能へアクセスする為のシェルです。このシェル上で"memmap"というコマンドを実行してメモリマップを確認します。

==== UEFIシェルを起動する
まずはUEFIシェルを起動します。「QEMUの場合」・「実機の場合」それぞれで紹介します。

■ QEMUの場合

QEMUで使用するUEFIファームウェア"OVMF"にはUEFIシェルが内蔵されています。そのため、QEMUに"-bios"オプションでOVMFファームウェアを指定して起動し、しばらく待っていると"Shell> "というプロンプトが表示されます。

起動するコマンドとしては以下の通りです。

//cmd{
■ APTでovmfパッケージをインストールした場合
$ @<b>{qemu-system-x86_64 -m 4G -bios OVMF.fd}
■ OVMF.fdをダウンロードしてきた場合
$ @<b>{qemu-system-x86_64 -m 4G -bios OVMF.fdへのパス指定}
//}

"-m 4G"はQEMU上の仮想マシンのメモリ設定です。4GBに設定している理由は本章最後のコラムで説明します。

■ 実機の場合

UEFIシェル内蔵の場合はPC起動時にF2キー等で表示されるBIOS(UEFI)設定画面からUEFIシェルを起動できるようです。

ここではUEFIシェルを内蔵していない場合に、公開されているUEFIシェル実行バイナリを使用する方法を紹介します。UEFIシェル内蔵の場合も使える方法なので、「内蔵されているか分からない」・「内蔵されているが上手く起動できない」場合はこの方法を試してみてください。

UEFIシェルの実行バイナリ("Shell_Full.efi")は以下からダウンロードできます。

 * @<href>{https://github.com/tianocore/edk2/tree/master/EdkShellBinPkg/FullShell/X64}

ダウンロード後、UEFIファームウェアが見つけられるように、起動用ストレージ(USBフラッシュメモリ等)へ配置します。"Shell_Full.efi"を起動用ストレージの@<list>{place_uefi_shell}の場所へ配置してください。

なお、UEFIファームウェアはFATあるいはGPT@<fn>{about_gpt}の第1パーティションを認識します。USBフラッシュメモリ等は購入時、FATパーティションが一つのみの状態であるため特に問題は無いかと思います。UEFI向けのGPTフォーマットの仕方は付録に記載しておりますので、必要であれば参照してください。
//footnote[about_gpt]["GUID Partition Table"と呼ばれるもので、UEFI向けに新たに作られたものです。]

//list[place_uefi_shell][UEFIシェルバイナリの配置]{
起動用ストレージ/
└── EFI/
    └─BOOT/
         └─ BOOTX64.EFI  ← Shell_Full.efiをリネームして配置
//}

そして、PCのBIOS(UEFI)設定を変更し、作成した起動用ストレージからブートするとUEFIシェルが立ち上がり、以下のようにプロンプトが表示されます。

//cmd{
Shell>
//}

==== memmapコマンドでメモリマップを確認する
"memmap"というコマンドでメモリマップを確認できます。

実行例(QEMU)

//cmd{
Shell> @<b>{memmap}
Type      Start            End              #pages             Attributes
Available 0000000000000000-000000000009FFFF 00000000000000A0 000000000000000F
Available 0000000000100000-000000000081FFFF 0000000000000720 000000000000000F
BS_Data   0000000000820000-0000000000FFFFFF 00000000000007E0 000000000000000F
Available 0000000001000000-00000000BBFFFFFF 00000000000BB000 000000000000000F
BS_Data   00000000BC000000-00000000BC01FFFF 0000000000000020 000000000000000F
Available 00000000BC020000-00000000BE968FFF 0000000000002949 000000000000000F
BS_Data   00000000BE969000-00000000BE98DFFF 0000000000000025 000000000000000F
Available 00000000BE98E000-00000000BE9B2FFF 0000000000000025 000000000000000F
BS_Data   00000000BE9B3000-00000000BE9BFFFF 000000000000000D 000000000000000F
Available 00000000BE9C0000-00000000BE9C0FFF 0000000000000001 000000000000000F
BS_Data   00000000BE9C1000-00000000BEB17FFF 0000000000000157 000000000000000F
LoaderCode 00000000BEB18000-00000000BEC2EFFF 0000000000000117 000000000000000F
BS_Data   00000000BEC2F000-00000000BECE7FFF 00000000000000B9 000000000000000F
BS_Code   00000000BECE8000-00000000BEE3BFFF 0000000000000154 000000000000000F
RT_Data   00000000BEE3C000-00000000BEE4DFFF 0000000000000012 800000000000000F
BS_Data   00000000BEE4E000-00000000BFD4DFFF 0000000000000F00 000000000000000F
BS_Code   00000000BFD4E000-00000000BFECDFFF 0000000000000180 000000000000000F
RT_Code   00000000BFECE000-00000000BFEFDFFF 0000000000000030 800000000000000F
RT_Data   00000000BFEFE000-00000000BFF21FFF 0000000000000024 800000000000000F
Reserved  00000000BFF22000-00000000BFF25FFF 0000000000000004 000000000000000F
ACPIRec   00000000BFF26000-00000000BFF2DFFF 0000000000000008 000000000000000F
ACPI_NVS  00000000BFF2E000-00000000BFF31FFF 0000000000000004 000000000000000F
BS_Data   00000000BFF32000-00000000BFFCFFFF 000000000000009E 000000000000000F
RT_Data   00000000BFFD0000-00000000BFFFFFFF 0000000000000030 800000000000000F
Available 0000000100000000-000000013FFFFFFF 0000000000040000 000000000000000F
  Reserved  :          4 Pages (16,384)
  LoaderCode:        279 Pages (1,142,784)
  LoaderData:          0 Pages (0)
  BS_Code   :        724 Pages (2,965,504)
  BS_Data   :      6,624 Pages (27,131,904)
  RT_Code   :         48 Pages (196,608)
  RT_Data   :        102 Pages (417,792)
  ACPI Recl :          8 Pages (32,768)
  ACPI NVS  :          4 Pages (16,384)
  MMIO      :          0 Pages (0)
  Available :  1,040,687 Pages (4,262,653,952)
Total Memory: 4095 MB (4,294,574,080 Bytes)
//}

結果が1画面に収まらない場合、勝手にスクロールしてしまいますが、"PgUp"/"PgDn"キーで上下にスクロールできます。

"Available"と記載されている領域が使用可能なメモリ領域で、"#pages"列に各領域のサイズが記載されています。サイズはページ数なので、ページサイズ(4KB)を掛けた値がバイト数になります。

=== メモリマップの内容からpoiboot.conf作成
memmapコマンドで表示されるメモリマップの中からkernel.binとfs.imgそれぞれの配置先として2つのAvailable領域を選び、以下のようにpoiboot.confへ記載します。

 * 1行目: kernel.binのロード先アドレス(16進数16文字)
 * 2行目: fs.imgのロード先アドレス(16進数16文字)

kernel.binとfs.imgそれぞれのAvailable領域としては以下を満たすものを選んでください。

 * 1行目: kernel.binのロード先
 ** 先頭アドレスから1MB先をスタックのベースアドレスにしているため、サイズが1MB以上であること
 ** デフォルトの設定ファイルの値: 0000000000110000
 * 2行目: fs.imgのロード先
 ** 本書最後の方で1枚2MB弱の画像ファイルをいくつか配置するため、20MB以上はあると良い
 ** デフォルトの設定ファイルの値: 0000000100000000

以降、本書ではデフォルトの設定ファイルの値でpoibootが設定されているものとして説明します。

===[column] QEMUの"-m 4G"オプションの理由
-mが4GB以上の時、1GB以上のAvailableな領域がアドレス"0x0000000100000000"に対応付けられるようになります。実機(lenovo)でも同様であったので「fs.imgのロード先」として0x0000000100000000が使えるようQEMUには"-m 4G"のオプションを付けています。

ちなみに、ネット上でmemmapの実機での実行結果をいくつか見てみると、"0x0000000100000000"に1GB以上のAvailable領域が対応付けられるのはどのマシンでも共通な様です。

また、"0x0000000100000000"には「搭載メモリ - 3GB」のメモリが対応付けられているようです。QEMUや実機でmemmapを試すと分かりますが、"0x0000000100000000"より上位(小さいアドレス)に散り散りに対応付けられているAvailable領域が3GB弱程度で、それらを引いた残りが"0x0000000100000000"に対応付けられているようです。(厳密にはUEFIファームウェアが使用する分も引いた残りですが、それらは1GBもありません。)

そのため、4GB以上であればQEMUのメモリ設定はいくつでも構わないのですが、今の所fs.imgは1GBあれば十分です。

===[column] メモリマップ取得をブートローダーで行うには
本書では簡単のためにメモリマップをUEFIシェルから確認しましたが、ブートローダーからUEFIファームウェアへ問い合わせてメモリマップ取得を行うこともできます。

本書の範囲から外れるため説明しませんが、実装としてはサンプルソースのpoibootのディレクトリ"A01_poiboot"内の"libuefi/mem.c"に"dump_memmap"というメモリマップを画面にダンプする関数がありますので興味があれば見てみてください。
