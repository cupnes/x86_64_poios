= poibootについて
本書で使用している"poiboot"は前著までの「フルスクラッチで作る!UEFIベアメタルプログラミング」シリーズの知見+αで作成した簡単なブートローダーです。

ここではpoibootの構成と、前著からの+αの部分について説明します。

== poibootの構成
なお、poibootはサンプルディレクトリ内の"A01_poiboot"ディレクトリに格納しています。

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
 2. kernel.binをRAMへロード
 3. apps.imgが存在すればRAMへロード
 4. カーネルへ渡す情報の準備
 5. UEFIのboot servicesを終了させる
 6. カーネルへジャンプ

「1. libuefi初期化」は、前著で説明したefi_init関数です。

「2. kernel.binをRAMへロード」では、2.2節で説明したカーネルのバイナリフォーマットを解釈し、RAMへロードします。といっても、2.2節で説明した通り、やることは、1) カーネル本体を0x00110000へロードし、2) ヘッダの情報に従いbssセクションをゼロクリアの2つです。ファイルを扱う過程でUEFIファームウェアのファイル操作を使用しますが、UEFIでのファイル操作は前著までで説明した通りです。

「3. apps.imgが存在すればRAMへロード」は、"apps.img"という名前のバイナリ("app"という名前ですがアプリだけでなくカーネルから参照されるデータ全般)が存在すれば、予め決められたアドレス0x00200000へロードします。

「4. カーネルへ渡す情報の準備」について、ブートローダーからカーネルへ渡す情報はSystemTableとfbの先頭アドレスです。SystemTableはefi_main関数の第2引数で得られるものです。fbはカーネルへフレームバッファの情報を渡すために用意したstruct fb構造体の変数で、init_fb関数で作成しています。

「5. UEFIのboot servicesを終了させる」では、BootServicesのExitBootServices関数を呼び出し、UEFIファームウェアが様々なリソースを管理しているモードを終了します。ExitBootServicesを呼び出す処理は前著まででは説明していない+αの内容で、次節で説明します。

「6. カーネルへジャンプ」では、SystemTableとstruct fbの先頭アドレスを関数の引数として受け渡すためにそれぞれをレジスタへ格納します。関数呼び出しの第1・第2引数はそれぞれ、RDIレジスタ・RSIレジスタで受け渡しますので、それぞれのレジスタにSystemTableとfbの先頭アドレスを格納します。そして、スタックポインタを設定し、カーネルへジャンプします。

== 前著の+α(ExitBootServices)
BootServicesのExitBootServices関数の呼び出し方は少々特殊です。といっても仕様書に書いてあるとおりで、複雑なものではないです。

ExitBootServices関数を呼び出すために必要な手続きとExitBootServices関数の呼び出しはexit_boot_services関数にまとめています。exit_boot_services関数の実装は@<list>{a1_exit_boot_services}の通りです。

//listnum[a1_exit_boot_services][A01_poiboot/libuefi/mem.c][c]{
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
