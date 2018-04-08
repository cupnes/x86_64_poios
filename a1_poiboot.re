= poibootについて
本書で使用している"poiboot"は前著までの「フルスクラッチで作る!UEFIベアメタルプログラミング」シリーズの知見+αで作成した簡単なブートローダーです。

ここではpoibootの構成と、前著からの+αの部分について説明します。

== poibootの構成
poibootはサンプルディレクトリ内の"A01_poiboot"ディレクトリに格納しています。

ソースディレクトリの構成は@<list>{a01_tree}の通りです。

//list[a01_tree][poibootのディレクトリ構成]{
A01_poiboot/
├─ Makefile
├─ poiboot.c
├─ libuefi/
└─ include/
//}

前著までで紹介したUEFIファームウェアを直接扱うためのコードは"libuefi"という名前でライブラリ化し、ライブラリ本体のソースコードを"libuefi"ディレクトリに、ヘッダを"include"ディレクトリに配置しています。そして、libuefiを使用してブートローダーを作っているpoibootの本体がpoiboot.cです。

#@# refactoring poiboot.c

poiboot.cでは以下の処理を行っています。

 1. libuefi初期化
 2. 設定ファイル(poiboot.conf)読み込み
 3. kernel.binをRAMへロード
 4. fs.imgが存在すればRAMへロード
 5. カーネルへ渡す情報の準備
 6. UEFIのboot servicesを終了させる
 7. カーネルへジャンプ

「1. libuefi初期化」は、前著で説明したefi_init関数です。

「2. 設定ファイル(poiboot.conf)読み込み」では設定ファイル"poiboot.conf"をパースし、カーネル先頭アドレス(kernel.binを配置するアドレス)とファイルシステム先頭アドレス(apps.imgを配置するアドレス)を設定します

「3. kernel.binをRAMへロード」では、2.2節で説明したカーネルのバイナリフォーマットを解釈し、RAMへロードします。といっても、2.2節で説明した通り、やることは、1) カーネル本体をpoiboot.conf設定(本書では0x00110000を想定)のアドレスへロードし、2) ヘッダの情報に従いbssセクションをゼロクリアの2つです。ファイルを扱う過程でUEFIファームウェアのファイル操作を使用しますが、UEFIでのファイル操作は前著までで説明した通りです。

「4. fs.imgが存在すればRAMへロード」は、"fs.img"という名前のバイナリ("app"という名前ですがアプリだけでなくカーネルから参照されるデータ全般)が存在すれば、poiboot.conf設定(本書では0x0000000100000000を想定)のアドレスへロードします。

「5. カーネルへ渡す情報の準備」について、ブートローダーからカーネルへ渡す情報はSystemTableとfbの先頭アドレスです。SystemTableはefi_main関数の第2引数で得られるものです。fbはカーネルへフレームバッファの情報を渡すために用意したstruct fb構造体の変数で、init_fb関数で作成しています。

「6. UEFIのboot servicesを終了させる」では、BootServicesのExitBootServices関数を呼び出し、UEFIファームウェアが様々なリソースを管理しているモードを終了します。ExitBootServicesを呼び出す処理は前著まででは説明していない+αの内容で、次節で説明します。

「7. カーネルへジャンプ」では、SystemTableとstruct fbの先頭アドレスを関数の引数として受け渡すためにそれぞれをレジスタへ格納します。関数呼び出しの第1・第2引数はそれぞれ、RDIレジスタ・RSIレジスタで受け渡しますので、それぞれのレジスタにSystemTableとfbの先頭アドレスを格納します。そして、スタックポインタを設定し、カーネルへジャンプします。

== 前著の+α(ExitBootServices)
BootServicesのExitBootServices関数の呼び出し方は少々特殊です。といっても仕様書に書いてあるとおりで、複雑なものではないです。

ExitBootServices関数を呼び出すために必要な手続きとExitBootServices関数の呼び出しはexit_boot_services関数にまとめています。exit_boot_services関数の実装は@<list>{a1_exit_boot_services}の通りです。

//list[a1_exit_boot_services][A01_poiboot/libuefi/mem.c][c]{
void exit_boot_services(void *IH)
{
	unsigned long long status;
	unsigned long long mmap_size;
	unsigned int desc_ver;

	do {
		mmap_size = MEM_DESC_SIZE;
		status = ST->BootServices->GetMemoryMap(
			&mmap_size, (struct EFI_MEMORY_DESCRIPTOR *)mem_desc,
			&map_key, &mem_desc_unit_size, &desc_ver);
	} while (!check_warn_error(status, L"GetMemoryMap"));

	status = ST->BootServices->ExitBootServices(IH, map_key);
	assert(status, L"ExitBootServices");
}
//}

ExitBootServices関数はGetMemoryMap関数で取得したメモリマップのキー(map_key)を取得した後、他のUEFIの機能を呼び出したりすること無くExitBootServices関数を呼び出す必要があります。そのため、GetMemoryMap関数とExitBootServices関数の呼び出しをひとつの関数にまとめています。

なお、ExitBootServicesを呼び出すと、それ以降、BootServices内の関数群や各種プロトコルは使用不可となりますが、RuntimeServices内の関数群は引き続き使用可能です。

ブートローダーはUEFIファームウェアから物理アドレスのメモリマップを取得できるので、カーネルが動作し始める時のスタックポインタ設定もブートローダーの役割としています。

#@# カーネルは実際に自分自身が動くためにブートローダーから受け取ったメモリマップを参考にスタックポインタを再設定しても良いですが、初期状態のメモリマップで今の所問題ないですし、それゆえにまだブートローダーはメモリマップをカーネルに渡す実装にもなっていないです。

== UEFIのファイル読み出しが正しく動作しないケースがありました
ブートローダーがUEFIの機能を利用してファイル読み出しを行う際は、"EFI_FILE_PROTOCOL"というプロトコル@<fn>{about_uefi_protocol}が持つ"Read"関数を使用します。
//footnote[about_uefi_protocol][実体は、関数ポインタをメンバに持つ構造体]

Read関数には、引数で「読みだして欲しいバイト数」を変数のポインタとして渡し、Read関数は「実際に読み出したバイト数」を渡されたポインタが指す変数へ格納して返します。

問題となったのが、Read関数が返す「実際に読み出したバイト数」です。Lenovo製ThinkPad E450にて、640x480のBGRA画像2枚を格納したfs.img(2.4MB程)をReadさせたとき、「実際に読み出したバイト数」としては指定したfs.imgのサイズを返すのですが、読み出し先を見てみると途中からでたらめなデータが格納されていることがありました。

100%読み出しに失敗するわけではなく、成功する場合もあり、因果は不明ですが筆者が試す限りは100%失敗に陥らせる手順を見つけました(下記)。

 1. PCを立ち上げ、GRUBメニューが表示されるまで待つ
 2. USBフラッシュメモリ挿入
 3. GRUBメニューで'c'キーを押下しGRUBのシェルを立ち上げる
 4. GRUBシェル内で"reboot"コマンドにより再起動
 5. USBフラッシュメモリから起動したpoibootでReadに失敗(筆者が試す限り100%)

USBフラッシュメモリの挿入タイミングはいつでも良いです。UEFIファームウェアの機能を利用するプログラム(本書の場合poiboot)を「PCの再起動で立ち上げる」事がきっかけのようで、Debianが起動している状態でUSBフラッシュメモリを挿入し、再起動したときに発生することもありましたが、一度シャットダウンしたPCにUSBフラッシュメモリを挿入しPCの電源を入れた場合に発生したことは無かったように思います。

上記の再現手順においてもkernel.bin(16KB程度)のReadはちゃんと最後まで読み出せていたので、16KBずつReadするようにワークアラウンドを作成しました。それがlibuefi内file.cのsafety_file_read関数です(Readする単位はfile.cに"SAFETY_READ_UNIT"という定数で設定)。

筆者の環境の場合、16KBまでは1回のReadで正しく読み出せていたので「16KBずつ」としていますが、その他のUEFIファームウェアではより小さいサイズしか正しく読み出せない(なおかつ、読み出したサイズは指定された通りのサイズを返す)場合もあるかもしれません。16KBは"063_iv_image_viewer"のkernel.binのサイズでもあるので、その場合はkernel.bin自体正しく読み出せていない可能性があります。そのため、挙動がおかしいがkernel.binやfs.img自体には問題が無いと思われる場合は、poibootでkernel.binとfs.imgを正しくメモリへロードできているか確認してみてください。

poibootが画面へ出すログを見る場合は、main.cで無限ループのみを行うkernel.binを用意すれば良いです。poibootはカーネル起動時に画面のクリアをしないため、カーネル側でフレームバッファをクリアしない限りはpoibootが画面へ出力したログを見ることができます。

poibootが画面へ出力するログの例は@<img>{a01_poiboot_log}の通りです。

//image[a01_poiboot_log][poibootが画面へ出力するログの例]{
//}

"kernel body"と"apps"のそれぞれでロードした最初の16バイト("first 16 bytes")と最後の16バイト("last 16 bytes")を出力しています。それらが実際のkernel.binとfs.imgと合っているか確認してください。なお、"kernel body"はkernel.binのヘッダ(BSSの先頭アドレスとサイズ)を除いた部分です。kernel.binの先頭32バイト(ヘッダ)を除いた以降(33バイト目以降)が"kernel body first 16 bytes"に並びますので注意してください。

なお、上記に「Lenovo製ThinkPad E450にて」と書いている通り、QEMUではこの問題は発生しませんでした。E450以外の実機は筆者の所持しているPCの都合上確認できていません。
