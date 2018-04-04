= フレームバッファを制御する
この章ではフレームバッファへ書き込むプログラムの作成を通して本書でのOS自作の流れを紹介します。

この章では最終的に@<img>{2_fb}の構成でフレームバッファ関連の機能を実装します。

//image[2_fb][この章で作る機能の構成]{
//}

@<img>{2_fb}の角丸の四角が作成するソースコードです。フレームバッファドライバ(fb.cとfb.h)、フレームバッファコンソール(fbcon.cとfbcon.h)、フォント(font.cとfont.h)を作成します。

== 画面を単色で塗りつぶす
最初に行う"Hello world"的プログラムとして、画面を単色で塗りつぶしてみます。本書で使用するブートローダー"poiboot"はメモリへロードしたカーネルを実行する際、C言語の引数としてフレームバッファの先頭アドレスを渡しますので、それを使用することで画面描画が行えます。

ここで紹介するプログラムはサンプルディレクトリ内の"020_fill"ディレクトリに格納しています。

== ソースコードを書く
まずはソースコードを書きます。ソースコードは"main.c"の一つだけで、内容は@<list>{020_fill_main}の通りです。

//list[020_fill_main][020_fill/main.c][c]{
#define BG_RED		0
#define BG_GREEN	255
#define BG_BLUE		0

struct pixelformat {
	unsigned char b;
	unsigned char g;
	unsigned char r;
	unsigned char _reserved;
};

struct framebuffer {
	struct pixelformat *base;
	unsigned long long size;
	unsigned int hr;
	unsigned int vr;
};

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *fb,
		  void *_fs_start __attribute__ ((unused)))
{
	unsigned int x, y;
	struct pixelformat *p;

	for (y = 0; y < fb->vr; y++) {
		for (x = 0; x < fb->hr; x++) {
			p = fb->base + (fb->hr * y) + x;
			p->r = BG_RED;
			p->g = BG_GREEN;
			p->b = BG_BLUE;
		}
	}

	while (1);
}
//}

start_kernel関数がブートローダーからジャンプしてくる先の関数で、第2引数に"framebuffer"という構造体のポインタとしてフレームバッファの情報を渡してくれます。第1引数にもブートローダーから何か情報を渡しているのですが、本書では使わないので気にしなくて構いません@<fn>{about_start_kernel_1st_arg}。第3引数はブートローダーが配置したファイルシステムの先頭アドレスです。これについては後の章で使用します。
//footnote[about_start_kernel_1st_arg][UEFIのSystemTableというものの先頭アドレスです。]

framebuffer構造体の"base"メンバはフレームバッファの先頭アドレスです。ピクセルフォーマットは"pixelformat"構造体の通りで、「B(青)・G(緑)・R(赤)・Reserved(予約)」の順に各1バイトで並んでいます。加えて、framebufferの"hr"メンバには画面の水平解像度、"vr"には垂直解像度が格納されていますので、start_kernel関数のforの二重ループのコードで画面を単色で塗りつぶすことができます。ここでは緑一色に塗りつぶしています。

start_kernel関数の戻り先は無いので、最後は無限ループで止めています。

== リンカスクリプトを書く
ソースコードは作成できたので、poibootがロードし実行できるフォーマットでカーネルバイナリ"kernel.bin"を生成するようリンカスクリプトを書きます。

まず、kernel.binはヘッダとボディで構成されます。ヘッダとボディそれぞれの内容を使用してpoibootがkernel.binを実行するまでの流れは以下の通りです。

 1. ボディの内容を設定ファイル"poiboot.conf"で指定されたkernel.binロード先アドレスへロード
 2. ヘッダに記述されたbssセクション@<fn>{about_section}の先頭アドレスとサイズに従ってbssセクションをゼロクリア
 3. "poiboot.conf"で指定されたkernel.binロード先アドレスへジャンプ

//footnote[about_section][「セクション」とは、コンパイル過程のリンク時にバイナリデータを並べる単位です。リンカは「機械語コード」や「グローバル変数の初期値」といった領域(セクション)単位でバイナリを並べます。「機械語コード」を「textセクション」、「0以外の初期値を持つグローバル変数の初期値」を「dataセクション」等と呼び、「bssセクション」は「初期値無しあるいは0のグローバル変数」です。そのため、実行バイナリロード後にロード先のbssセクションはゼロクリアする必要があります。なお、厳密には「初期値無しグローバル変数」はCOMMONセクションですが、bssとまとめてゼロクリアできるよう、MakefileでCFLAGSへ"-fno-common"を指定しています。]

図示すると@<img>{linkerscript}の通りです。

//image[linkerscript][kernel.binのフォーマットとpoibootのロードの挙動]{
//}

poibootが実行できるバイナリを生成するためにリンカスクリプトで考慮しなければならないことをまとめると以下の通りです。

 * ヘッダへロード先のbssセクションの先頭アドレスとサイズを格納
 * ボディの先頭へtextセクションを配置
 * ボディの内容は、poiboot.confで指定されたkernel.binロード先アドレスに従ってアドレス解決する

ボディ先頭のtextセクションについて、厳密には、加えて「textセクション先頭にカーネルのエントリ関数が配置されていること」も必要です。これについてはmain.cにエントリ関数のみ記載(別の関数を記載するとしてもエントリ関数の後に関数定義する)とし、Makefileでldへ指定するオブジェクトファイル順でmain.oを1番目に指定することにします。

また、ボディの内容がpoiboot.confの設定に従ってアドレス解決する点については、ここでは簡単のためpoiboot.confに記載したkernel.binロード先アドレスと同じアドレスをリンカスクリプトにも記載することにします(本書では0x00110000を想定)。

以上の内容をリンカスクリプトとして記述すると@<list>{020_fill_ld}の通りです。なお、リンカスクリプトは"kernel.ld"というファイル名で作成することとします。

//list[020_fill_ld][020_fill/kernel.ld]{
OUTPUT_FORMAT("binary");

MEMORY
{
	exec_file(r) : ORIGIN = 0x00000000, LENGTH = 1m
	ram(wx)      : ORIGIN = 0x00110000, LENGTH = 960k
}

SECTIONS
{
	.header : {
		QUAD(__bss_start)
		QUAD(__bss_end - __bss_start)
	} > exec_file

	.body : {
		*(.text)

		*(.rodata)

		*(.data)

		__bss_start = .;
		*(.bss)
		__bss_end = .;

		*(.eh_frame)
	} > ram AT> exec_file
}
//}

kernel.ldの内容を説明すると、まずOUTPUT_FORMAT()でいかなる実行ファイルフォーマットでもない単なるバイナリであることを示しています。

そして、"MEMORY{}"と"SECTIONS{}"では、上述したフォーマットとなるようにヘッダ(header)とボディ(body)の各種セクションの配置ルールを決めています。この時、headerは実行ファイル(kernel.bin)にのみ配置され、メモリへはロードされないのに対し、bodyは実行ファイルに存在し、かつメモリへもロードされる、という違いがあります。

そのため、実行ファイルへの配置とアドレス解決の都合上、"MEMORY{}"内で"exec_file"(実行ファイル)と"ram"(メモリ)という2つの定義を用意しています。そして、"SECTIONS{}"内で、headerは単純にexec_fileに配置し(「> exec_file」の指定)、続くbodyはアドレス解決はramに配置した想定で行いバイナリデータはexec_fileに続いて配置を行っています(「> ram AT> exec_file」の指定)。

== Makefileを書く
後はgccコマンドでソースコードからオブジェクトファイルを生成し、ldコマンドでkernel.ldを使用してリンクを行うだけです。

gccやldコマンドは指定するオプションも多く、何度も手動でコマンドを実行するのは面倒なので、Makefileを作成します。内容は@<list>{020_fill_make}の通りです。

//list[020_fill_make][020_fill/Makefile]{
TARGET = kernel.bin
CFLAGS = -Wall -Wextra -nostdinc -nostdlib -fno-builtin -fno-common
LDFLAGS = -Map kernel.map -s -x -T kernel.ld

$(TARGET): main.o
	ld $(LDFLAGS) -o $@ $+

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

clean:
	rm -f *~ *.o *.map $(TARGET)

.PHONY: clean
//}

Makefileでは特別なことはしていません。

デバッグ用にリンク時に"kernel.map"というファイル名でリンクマップ@<fn>{linkmap}を生成しています。
//footnote[linkmap][ldによってマップされたシンボルの位置情報やセクションの位置情報などが書かれたテキストファイル]

== コンパイルする
Makefileを作成しているため、コンパイルは"make"コマンドを実行するだけです。

ソースコード、リンカスクリプト、Makefileを配置したディレクトリへ移動し、以下のように"make"コマンドを実行してください。

//cmd{
$ @<b>{make}
//}

makeが成功すると"kernel.bin"が生成されています。

//cmd{
$ @<b>{ls kernel.bin}
kernel.bin
//}

== QEMUで実行する
QEMUには特定のディレクトリ以下をディスクと見なす機能があります。

"fs"というディレクトリを作成し、1章でダウンロード・展開したブートローダー(poiboot.efi)とkernel.binを@<list>{020_fill_file_placement}のように配置してください。

//list[020_fill_file_placement][QEMU実行時のファイル配置]{
fs/
├─ EFI/
│   └─ BOOT/
│        └─ BOOTX64.EFI  ← poiboot.efiをリネームして配置
└─ kernel.bin
//}

UEFIファームウェアは起動ディスク第1パーティションに"EFI/BOOT/BOOTX64.EFI"というファイル名でファイルが置いてあるとそれを見つけて実行してくれます。その後、poibootがkernel.binを見つける流れは先述の通りです。

ファイル配置後は以下のコマンドで実行できます。

//cmd{
$ @<b>{qemu-system-x86_64 -bios OVMF.fd -hda fat:fs}
//}

"-hda"オプションに"fat:fs"のように指定すると、「fsディレクトリ以下をFATフォーマットされたディスク」とみなして実行してくれます。

また、"-bios"の"OVMF.fd"は、OVMF.fdを手動でダウンロードした場合は、"OVMF.fd"を配置したパスを指定してください。

QEMUを起動すると@<img>{020_fill_qemu}のように緑一色の画面が表示されます(白黒印刷なので印刷上は分かりませんが。。。)。

//image[020_fill_qemu][020_fillのQEMU実行結果]{
//}

終了する際は、QEMUのウィンドウごとXボタンで終了してしまって構いません(特にストレージ等をマウントしている訳では無いので)。

kernel.bin更新の都度、fsディレクトリへファイルを配置してqemuコマンドを打つのも面倒なので、これらも@<list>{020_fill_make2}の様にMakefileへ追記します。

//list[020_fill_make2][QEMU実行手順もMakefileへ追記]{
TARGET = kernel.bin
CFLAGS = -Wall -Wextra -nostdinc -nostdlib -fno-builtin -fno-common
LDFLAGS = -Map kernel.map -s -x -T kernel.ld

$(TARGET): main.o
	ld $(LDFLAGS) -o $@ $+

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

# 追加(ここから)
run: $(TARGET)
	cp $(TARGET) ../fs/
	qemu-system-x86_64 -bios OVMF.fd -hda fat:../fs
# 追加(ここまで)

clean:
	rm -f *~ *.o *.map $(TARGET)

.PHONY: run clean  # runを追加
//}

@<list>{020_fill_make2}は、サンプルのディレクトリの一つ上の階層にfsディレクトリがブートローダー配置済みの状態で存在していることを前提としています。

予め、020_fillの一つ上の階層に@<list>{020_fill_file_placement2}のようにディレクトリ構造を作っておいてください。

//list[020_fill_file_placement2][Makefileのrunターゲット実行時のディレクトリ構造]{
020_fill/
├─ Makefile
├─ kernel.ld
└─ main.c
fs/
└─ EFI/
     └─ BOOT/
          └─ BOOTX64.EFI
//}

== 実機で試す
実機で試す際は、USBフラッシュメモリ等のPCの起動ディスクとして使用できるストレージへfsディレクトリ内のファイルをコピーし、コピーしたストレージから起動します。

なお、ストレージの第1パーティションはGPTあるいはFATでフォーマットしておいてください。ただし、たいていのUSBフラッシュメモリは購入時、FATフォーマットされたパーティション1つの状態なので特に再フォーマットの必要は無いです。フォーマットする場合、GPTフォーマットの方法は参考までに本書末尾の付録に記載しています。

=== USBフラッシュメモリへファイル配置・起動
USBフラッシュメモリ第1パーティションへfsディレクトリ内のディレクトリ構造をまるごとコピーします。

コマンドとしては以下の通りです(USBフラッシュメモリ第1パーティションが"/dev/sdb1"として認識されているとします)。

//cmd{
$ sudo mount /dev/sdb1 /mnt
$ sudo cp -r fs/* /mnt/
$ sudo umount /mnt
//}

その後、BIOS設定で起動デバイスの優先順位設定を変更し、USBフラッシュメモリから起動してください。すると@<img>{020_fill_lenovo}の様に動作確認できます(といってもこちらも白黒印刷なので印刷上は分かりませんが。。)。

//image[020_fill_lenovo][020_fillの実機実行結果]{
//}

シャットダウンなどの処理は無いので、電源ボタンで終了させてください(ディスク書き込み等はしていないので、何も害はありません)。

== ソースコードを整理
最後に、ソースコードの見通しが悪くならないよう、ソースコードを整理します。

整理後のソースコードはサンプルディレクトリの"021_fill"ディレクトリに格納しています。

ここでは、fb.hとfb.cというソースファイルを作成し、フレームバッファに関する処理はそこへ移動します。fb.hの内容は@<list>{021_fill_fb_h}、fb.cの内容は@<list>{021_fill_fb_c}の通りです。

//list[021_fill_fb_h][021_fill/include/fb.h][c]{
#ifndef _FB_H_
#define _FB_H_

struct pixelformat {
	unsigned char b;
	unsigned char g;
	unsigned char r;
	unsigned char _reserved;
};

struct framebuffer {
	struct pixelformat *base;
	unsigned long long size;
	unsigned int hr;
	unsigned int vr;
};

void fb_init(struct framebuffer *_fb);
void set_bg(unsigned char r, unsigned char g, unsigned char b);
inline void draw_px(unsigned int x, unsigned int y,
		    unsigned char r, unsigned char g, unsigned char b);
inline void fill_rect(unsigned int x, unsigned int y,
		      unsigned int w, unsigned int h,
		      unsigned char r, unsigned char g, unsigned char b);
void clear_screen(void);

#endif
//}

//list[021_fill_fb_c][021_fill/fb.c][c]{
#include <fb.h>

struct framebuffer fb;
struct pixelformat color_bg;

void fb_init(struct framebuffer *_fb)
{
	fb.base = _fb->base;
	fb.size = _fb->size;
	fb.hr = _fb->hr;
	fb.vr = _fb->vr;
}

void set_bg(unsigned char r, unsigned char g, unsigned char b)
{
	color_bg.b = b;
	color_bg.g = g;
	color_bg.r = r;
}

inline void draw_px(unsigned int x, unsigned int y,
		    unsigned char r, unsigned char g, unsigned char b)
{
	struct pixelformat *p = fb.base;
	p += y * fb.hr + x;

	p->b = b;
	p->g = g;
	p->r = r;
}

inline void fill_rect(unsigned int x, unsigned int y,
		      unsigned int w, unsigned int h,
		      unsigned char r, unsigned char g, unsigned char b)
{
	unsigned int i, j;
	for (i = y; i < (y + h); i++)
		for (j = x; j < (x + w); j++)
			draw_px(j, i, r, g, b);
}

void clear_screen(void)
{
	fill_rect(0, 0, fb.hr, fb.vr, color_bg.r, color_bg.g, color_bg.b);
}
//}

一気にコード行数が増えた感じがありますが、先ほどのフレームバッファへの画面塗りつぶし処理を機能ごとに分けて関数化しているだけです。用意した関数は以下の5つです。

 * fb_init
 ** フレームバッファ情報を登録
 * set_bg
 ** 背景色を設定
 * draw_px
 ** 指定した座標1ピクセルを指定した色で描画する
 * fill_rect
 ** 座標・幅・高さで指定した領域を指定した色で塗りつぶす
 * clear_screen
 ** 背景色で画面を塗りつぶす

draw_px関数やfill_rect関数は他の関数からも何度も呼び出される関数なので、インライン関数化しています@<fn>{021_fill_inline}。
//footnote[021_fill_inline][GCCは頭が良いのかinline指定しなくてもdraw_pxとfill_rect関数はインライン展開されていました。また、以降の章でもinline指定の関数はいくつか登場しますが、どうもinline指定していてもインライン展開されない場合もあるようです。]

また、ヘッダファイルはincludeディレクトリに配置し、"#include <>"でincludeできるようにコンパイルオプションへ追加します(後述)。

以上のfb.h・fb.cを使用すると、main.cは@<list>{021_fill_main}のように書けます。

//list[021_fill_main][021_fill/main.c][c]{
#include <fb.h>

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *_fb,
		  void *_fs_start __attribute__ ((unused)))
{
	fb_init(_fb);
	set_bg(0, 255, 0);
	clear_screen();

	while (1);
}
//}

start_kernel関数内が初期化処理っぽくなりました。また、start_kernelの第2引数はfb_init関数へ渡すだけの一時変数になったため"_fb"という変数名へ変更しました。

最後にMakefileへfb.oとincludeディレクトリの指定を追加します(@<list>{021_fill_makefile})。

//list[021_fill_makefile][021_fill/Makefile]{
TARGET = kernel.bin
CFLAGS = -Wall -Wextra -nostdinc -nostdlib -fno-builtin -fno-common -Iinclude
#                                                                   ↑追加
LDFLAGS = -Map kernel.map -s -x -T kernel.ld

$(TARGET): main.o fb.o  # fb.oを追加
	ld $(LDFLAGS) -o $@ $+

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

run: $(TARGET)
	cp $(TARGET) ../fs/
	qemu-system-x86_64 -bios OVMF.fd -hda fat:../fs

clean:
	rm -f *~ *.o *.map $(TARGET) include/*~  # "include/*~"を追加

.PHONY: run clean
//}

これでコード整理は終了です。

ここで追加したfb.hとfb.cがフレームバッファのドライバに当たります。今後、このドライバを使用して、画面描画を行います。

また、以降はここで作成したソースコード構成を拡張していく形で機能拡張や機能追加を行っていきます。

== 画面に文字を表示する(フォント)
この章最後に、画面に文字を出してみます。

前著やそれ以前にQEMU上で動く「BIOS+x86_32」を対象に書いた「Ohgami's Commentary on OS5」ではファームウェアやCRTコントローラのテキストモードを使用し、カーネル側でフォントを持つこと無く画面に文字を表示していました。

しかし、グラフィックモードをいずれ扱う際にはいずれフォントが必要になることと、64ビットのアーキテクチャでCRTコントローラのテキストモード時のVRAMがアドレス空間のどこにマップされているかが分からなかったので、本著では早速フォントを用意してみます。

ソースコードはサンプルディレクトリの"022_font"に格納しています。

フォントの実装方法は色々あるかと思いますが、ここでは簡単に配列で実装してみます。今のGCCでは連想配列の初期化のように配列を定義できます。フォントの定義はfont.cというソースコードへ行うことにします。font.cの内容は@<list>{022_font_font_c}の通りです。

//list[022_font_font_c][022_font/font.c(一部)][c]{
#include <font.h>

const unsigned char font_bitmap[][FONT_HEIGHT][FONT_WIDTH] = {
	[' '] = {
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0}
	},
	['!'] = {
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,1,0,0,0},
		{0,0,0,0,1,0,0,0},
		{0,0,0,0,1,0,0,0},
		{0,0,0,0,1,0,0,0},
		{0,0,0,0,1,0,0,0},
		{0,0,0,0,1,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,1,0,0,0},
		{0,0,0,0,0,0,0,0}
	},
	/* ・・・ 省略 ・・・ */
		['0'] = {
		{0,0,0,0,0,0,0,0},
		{0,0,1,1,1,1,0,0},
		{0,1,0,0,0,1,1,0},
		{0,1,0,0,1,0,1,0},
		{0,1,0,0,1,0,1,0},
		{0,1,0,1,0,0,1,0},
		{0,1,0,1,0,0,1,0},
		{0,1,1,0,0,0,1,0},
		{0,0,1,1,1,1,0,0},
		{0,0,0,0,0,0,0,0}
	},
	['1'] = {
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,1,0,0,0},
		{0,0,0,1,1,0,0,0},
		{0,0,0,0,1,0,0,0},
		{0,0,0,0,1,0,0,0},
		{0,0,0,0,1,0,0,0},
		{0,0,0,0,1,0,0,0},
		{0,0,0,0,1,0,0,0},
		{0,0,1,1,1,1,0,0},
		{0,0,0,0,0,0,0,0}
	},
	/* ・・・ 省略 ・・・ */
		['A'] = {
		{0,0,0,0,0,0,0,0},
		{0,0,0,1,1,0,0,0},
		{0,0,0,1,1,0,0,0},
		{0,0,1,0,0,1,0,0},
		{0,0,1,0,0,1,0,0},
		{0,1,0,0,0,0,1,0},
		{0,1,1,1,1,1,1,0},
		{0,1,0,0,0,0,1,0},
		{0,1,0,0,0,0,1,0},
		{0,0,0,0,0,0,0,0}
	},
	['B'] = {
		{0,0,0,0,0,0,0,0},
		{0,1,1,1,1,1,0,0},
		{0,1,0,0,0,0,1,0},
		{0,1,0,0,0,0,1,0},
		{0,1,1,1,1,1,0,0},
		{0,1,0,0,0,0,1,0},
		{0,1,0,0,0,0,1,0},
		{0,1,0,0,0,0,1,0},
		{0,1,1,1,1,1,0,0},
		{0,0,0,0,0,0,0,0}
	},
	/* ・・・ 省略 ・・・ */
};
//}

@<list>{022_font_font_c}で定義しているfont_bitmap配列の宣言や、1文字の幅(FONT_WIDTH)と高さ(FONT_HEIGHT)はinclude/font.hに書いておきます。内容は@<list>{022_font_font_h}の通りです。

//list[022_font_font_h][022_font/include/font.h][c]{
#ifndef _FONT_H_
#define _FONT_H_

#define FONT_WIDTH	8
#define FONT_HEIGHT	10

extern const unsigned char font_bitmap[][FONT_HEIGHT][FONT_WIDTH];

#endif
//}

このようにフォントのビットマップを定義しておくことで、先ほど実装したdraw_px関数を使用して画面に文字を出すことができるようになります。

現状のdraw_px関数は引数に色を指定していますが、1つの文字を描画している最中に色を変えることは(それはそれで面白そうですが、)考えないので、背景色同様に、描画色も予め設定しておくことにします。

フレームバッファドライバ(fb.c)へ描画色(color_fg)と設定関数(set_fg関数)、描画色で1ピクセルを描画する関数(draw_px_fg)の定義を追加します。追加後のfb.cは@<list>{022_font_fb_c}の通りです。

//list[022_font_fb_c][022_font/fb.c][c]{
#include <fb.h>

struct framebuffer fb;
struct pixelformat color_fg;	/* 追加 */
struct pixelformat color_bg;

/* ・・・ 省略 ・・・ */

/* 追加(ここから) */
void set_fg(unsigned char r, unsigned char g, unsigned char b)
{
	color_fg.b = b;
	color_fg.g = g;
	color_fg.r = r;
}
/* 追加(ここまで) */

void set_bg(unsigned char r, unsigned char g, unsigned char b)
{
/* ・・・ 省略 ・・・ */

/* 追加(ここから) */
inline void draw_px_fg(unsigned int x, unsigned int y)
{
	draw_px(x, y, color_fg.r, color_fg.g, color_fg.b);
}
/* 追加(ここまで) */
//}

また、include/fb.hへset_fg関数のプロトタイプ宣言追加と、fbのextern宣言追加@<fn>{022_why_extern_fb}も行います(@<list>{022_font_include_fb_h})。
//footnote[022_why_extern_fb][改行処理のためにフレームバッファの垂直解像度(struct fbのvrメンバ)を参照するため]

//list[022_font_include_fb_h][022_font/include/fb.h][c]{
/* ・・・ 省略 ・・・ */

extern struct framebuffer fb;	/* 追加 */

void fb_init(struct framebuffer *_fb);
void set_fg(unsigned char r, unsigned char g, unsigned char b);	/* 追加 */
void set_bg(unsigned char r, unsigned char g, unsigned char b);
inline void draw_px(unsigned int x, unsigned int y,
		    unsigned char r, unsigned char g, unsigned char b);
inline void draw_px_fg(unsigned int x, unsigned int y);		/* 追加 */
/* ・・・ 省略 ・・・ */
//}

font_bitmap配列とdraw_px_fg関数を使用すると、指定された一文字を描画するputc関数は@<list>{022_font_putc}の様に定義できます。

//list[022_font_putc][putc関数][c]{
unsigned int cursor_x = 0, cursor_y = 0;

void putc(char c)
{
	unsigned int x, y;

	/* カーソル座標(cursor_x,cursor_y)へ文字を描画 */
	for (y = 0; y < FONT_HEIGHT; y++)
		for (x = 0; x < FONT_WIDTH; x++)
			if (font_bitmap[(unsigned int)c][y][x])
				draw_px_fg(cursor_x + x, cursor_y + y);

	/* カーソル座標の更新 */
	cursor_x += FONT_WIDTH;
	if ((cursor_x + FONT_WIDTH) >= fb.hr) {
		cursor_x = 0;
		cursor_y += FONT_HEIGHT;
		if ((cursor_y + FONT_HEIGHT) >= fb.vr) {
			cursor_x = cursor_y = 0;
			clear_screen();
		}
	}
}
//}

@<list>{022_font_putc}ではカーソル座標をグローバル変数として用意し、putc関数ではカーソル座標へ文字を描画後、カーソル座標の更新を行っています。フォントのビットマップをASCII文字の値を添え字とした配列として用意しているため、引数で指定された文字(char c)をそのままフォント配列の添字に使用しています。

あと改行文字の処理を加え、putc関数は完成とします。putc関数はフレームバッファコンソールとしてfbcon.cというソースコードを追加し、そちらへ追加することにします。fbcon.cの内容は@<list>{022_font_fbcon_c}の通りです。

//list[022_font_fbcon_c][022_font/fbcon.c][c]{
#include <fbcon.h>
#include <fb.h>
#include <font.h>

unsigned int cursor_x = 0, cursor_y = 0;

void putc(char c)
{
	unsigned int x, y;

	switch(c) {
	case '\r':
		cursor_x = 0;
		break;

	case '\n':
		cursor_y += FONT_HEIGHT;
		if ((cursor_y + FONT_HEIGHT) >= fb.vr) {
			cursor_x = cursor_y = 0;
			clear_screen();
		}
		break;

	default:
		/* カーソル座標(cursor_x,cursor_y)へ文字を描画 */
		for (y = 0; y < FONT_HEIGHT; y++)
			for (x = 0; x < FONT_WIDTH; x++)
				if (font_bitmap[(unsigned int)c][y][x])
					draw_px_fg(cursor_x + x, cursor_y + y);

		/* カーソル座標の更新 */
		cursor_x += FONT_WIDTH;
		if ((cursor_x + FONT_WIDTH) >= fb.hr) {
			cursor_x = 0;
			cursor_y += FONT_HEIGHT;
			if ((cursor_y + FONT_HEIGHT) >= fb.vr) {
				cursor_x = cursor_y = 0;
				clear_screen();
			}
		}
	}
}
//}

ついでに文字列を出力するputs関数もfbcon.cへ追加しておきます(@<list>{022_font_fbcon_c_2})。

//list[022_font_fbcon_c_2][022_font/fbcon.c(puts関数)][c]{
/* ・・・ 省略 ・・・ */

void putc(char c)
{
	/* ・・・ 省略 ・・・ */
}

/* 追加(ここから) */
void puts(char *s)
{
	while (*s != '\0')
		putc(*s++);
}
/* 追加(ここまで) */
//}

putc関数とputs関数を呼び出せるよう、include/fbcon.hを@<list>{022_font_include_fbcon_h}の様に作成します。

//list[022_font_include_fbcon_h][022_font/include/fbcon.h][c]{
#ifndef _FBCON_H_
#define _FBCON_H_

void putc(char c);
void puts(char *s);

#endif
//}

それでは、set_fg関数とputs関数を使用して"HELLO WORLD!"を出力するようmain.cを書き替えてみます(@<list>{022_font_main_c})。

//list[022_font_main_c][022_font/main.c][c]{
#include <fb.h>
#include <fbcon.h>

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *fb,
		  void *_fs_start __attribute__ ((unused)))
{
	fb_init(fb);
	set_fg(255, 255, 255);	/* 追加 */
	set_bg(0, 70, 250);	/* 変更 */
	clear_screen();

	puts("HELLO WORLD!");	/* 追加*/

	while (1);
}
//}

最後に、Makefileへfont.cとfbcon.cをコンパイルの対象として追加します(@<list>{022_font_makefile})。

//list[022_font_makefile][022_font/Makefile]{
TARGET = kernel.bin
CFLAGS = -Wall -Wextra -nostdinc -nostdlib -fno-builtin -fno-common -Iinclude
LDFLAGS = -Map kernel.map -s -x -T kernel.ld

$(TARGET): main.o fbcon.o fb.o font.o	# fbcon.oとfont.oを追加
	ld $(LDFLAGS) -o $@ $+

# ・・・ 省略 ・・・
//}

//note[GNU ldへ渡すオブジェクトファイル順について]{
　ldへ渡すオブジェクトファイルの順序としてGNU ldの場合、「オブジェクトファイルのシンボル名解決を最初の方(左側)から順に行い、解決できなかったシンボルを後続のオブジェクトファイルから探す」という挙動となります。そのため、色々なオブジェクトファイルを参照するmain.oを一番最初のオブジェクトファイルとして渡し、参照されるだけであるfont.oは一番最後にldへ渡しています。
//}

実行すると@<img>{022_font}の様に"HELLO WORLD!"と表示されます。

//image[022_font][022_fontの実行結果]{
//}

OSのシステムコンソールっぽく(?)、青背景に白文字にしてみました@<fn>{022_mono_ry}。色はお好きなように変えてみてください。
//footnote[022_mono_ry][白黒印刷なのでこちらも印刷上は分かりませんが。。]

また、本書で用意しているfont.cにはアルファベットは大文字しか用意していないので、Hello worldも全て大文字で出しています。小文字の追加や、あるいはフォントサイズ等も変更して独自のフォントを実装してみたりすると自作OSに愛着がでてきて良いです。

ここまでで、フレームバッファ出力に関する実装は完了です。
