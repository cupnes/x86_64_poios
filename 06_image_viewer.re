= 画像ビューアアプリを作ってみる
「アプリ」といってもバイナリレベルでカーネルと分けたり、動作時の権限を変えたりは本書ではしません。これまでプラットフォームとして実装してきた「フレームバッファ」・「キーボードコントローラ」・「割り込み」・「ファイルシステム」等の道具を使用して何らかの用途を実現するサンプルとして、本書では画像のビューアを作ってみます。作成する画像ビューアの構成は@<img>{6_iv}の通りです。

//image[6_iv][画像ビューアの構成]{
//}

== 仕様
仕様は以下の通りとします。

 * 起動するとファイルシステム内の順序で1番目の画像を表示する
 * 'j'キー押下で次の画像、'k'キー押下で前の画像を表示する
 * 表示する順序はファイルシステム内の並び順
 * ファイルシステム内には画像ファイルしか無いものとする
 * 画像ファイルのフォーマットはBGRA各8ビット(フレームバッファのピクセルフォーマット)
 * 画像ファイルのサイズ(幅・高さ)はフレームバッファ全画面表示の解像度

== 画像ファイルを用意する
画像ファイルの仕様は前述の通り以下です。

 * フォーマット: BGRA(各色8ビット)
 * サイズ(幅・高さ): フレームバッファ全画面表示の解像度

なお、この中で、「サイズ(幅・高さ)」は具体的な値がまだわかっていませんので見てみます。

=== putd関数を追加し、フレームバッファ解像度を画面表示して確認する
フレームバッファの解像度はstruct framebuffer構造体中に存在します。この値を表示させて確認したいところですが、そういえばこれまで、数値を画面へ表示する機能は実装していませんでした。

ここでは数値を表示する関数をputd(put decimal)という名前で追加し、フレームバッファの解像度を画面へ表示してみます。

この節のサンプルディレクトリは"060_iv_show_fb_resol"です。前章の"051_fs_read_fs"を元に作ります。

それでは、putd関数をfbcon.cへ追加してみます(@<list>{060_fbcon_c})。

//list[060_fbcon_c][060_iv_show_fb_resol/fbcon.c][c]{
#include <fbcon.h>
#include <fb.h>
#include <font.h>

/* 追加(ここから) */
/* 64bit unsignedの最大値0xffffffffffffffffは
 * 10進で18446744073709551615(20桁)なので'\0'含め21文字分のバッファで足りる */
#define MAX_STR_BUF	21
/* 追加(ここまで) */

/* ・・・ 省略 ・・・ */

void puts(char *s)
{
	while (*s != '\0')
		putc(*s++);
}

/* 追加(ここから) */
void putd(unsigned long long val, unsigned char num_digits)
{
	char str[MAX_STR_BUF];

	int i;
	for (i = num_digits - 1; i >= 0; i--) {
		int digit = val % 10;
		str[i] = '0' + digit;
		val /= 10;
	}
	str[num_digits] = '\0';

	puts(str);
}
/* 追加(ここまで) */
//}

併せて、fbcon.hへputd関数のプロトタイプ宣言を追加します。

//list[060_fbcon_h][060_iv_show_fb_resol/include/fbcon.h][c]{
/* ・・・ 省略 ・・・ */
void puts(char *s);
void putd(unsigned long long val, unsigned char num_digits);	/* 追加 */

#endif
//}

putd関数は第1引数に表示したい値を、第2引数に表示桁数を指定します。

それでは、追加したputd関数を使用してフレームバッファの解像度情報を表示してみます(@<list>{060_main_c})。

//list[060_main_c][060_iv_show_fb_resol][c]{
/* ・・・ 省略 ・・・ */

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *_fb)
{
	/* フレームバッファ周りの初期化 */
	fb_init(_fb);
	/* ・・・ 省略 ・・・ */

	/* CPUの割り込み有効化 */
	enable_cpu_intr();

	/* 変更(ここから) */
	/* 水平解像度(Horizontal Resolution)と
	 * 垂直解像度(Vertical Resolution)を表示 */
	puts("HR ");
	putd(fb.hr, 4);
	puts("\r\n");
	puts("VR ");
	putd(fb.vr, 4);
	puts("\r\n");
	/* 変更(ここまで) */

	/* haltして待つ */
	while (1)
		cpu_halt();
}
//}

struct framebufferの値の参照にはstart_kernel関数の仮引数の"_fb"ではなく、fb_init関数内で設定され、fb.hでexternされているstruct framebuffer fbを使用しています@<fn>{about_060_fb}。
//footnote[about_060_fb][start_kernel関数内においてはどちらを用いても良いのですが、カーネル内の処理はfb.hのものを使うように統一しています。]

QEMUで実行すると@<img>{060_qemu}のように表示されました。

//image[060_qemu][060_iv_show_fb_resolの実行結果(QEMU)]{
size 1920000
hr   800
vr   600
(/ 1920000.0 (* 800 600))4.0 bytes/pixel
//}

解像度は800x600のようです。

ただし、実機(Lenovo)で試した所、@<img>{060_lenovo}のように表示されました。

//image[060_lenovo][060_iv_show_fb_resolの実行結果(実機)]{
size 1228800
hr   640
vr   480
(/ 1228800.0 (* 640 480))4.0 bytes/pixel
//}

poibootがカーネルへ渡すフレームバッファの解像度は起動時のUEFIファームウェアのデフォルトの解像度そのままです。

ちゃんと解像度設定を行うならばpoibootで行うのがUEFIの機能が使えるので楽かと思いますが@<fn>{about_uefi_change_graphic_mode}、ここでは単にQEMU側を採用することにします。
//footnote[about_uefi_change_graphic_mode][UEFIでの解像度変更(グラフィックモード変更)方法は、前著「フルスクラッチで作る!UEFIベアメタルプログラミング パート2」の「1.4 テキストモードを変更する」のコラム「グラフィックモードの情報を確認し、変更するには」に記載しています。本書同様、PDF版は無料でダウンロードできます。「参考情報」にリンクを記載しておりますので興味があればご覧ください]

そのため、以降、フレームバッファの解像度は800x600だとし、サンプルプログラムはQEMU上での動作を想定します。といっても解像度が違うだけなので、以降で紹介する画像表示プログラムを複数解像度対応させるか、ご自身のマシンの解像度を調べて800x600の箇所を書き替えていただければ実機でも動作させられるかと思います。

=== ImageMagick(convertコマンド)で画像を変換する
次に画像ファイルを変換します。

画像ファイルの変換にはImageMagickという画像編集のツール郡に含まれる"convert"コマンドを使用します。ImageMagickがインストールされていない場合は以下のコマンドでインストールを行ってください。

//cmd{
$ @<b>{sudo apt install imagemagick}
//}

それでは、convertコマンドを使用して、フレームバッファの解像度とピクセルフォーマットへ変換します。何らかの画像ファイルを用意してください。画像ファイルの解像度はフレームバッファの解像度とアスペクト比が同じである方が良いです(縦か横に伸びてしまうので)。

以下のコマンドで変換できます。

//cmd{
$ @<b>{convert 元画像ファイル -resize 800x600! -depth 8 変換後ファイル名.bgra}
//}

フレームバッファ解像度(800x600)以外の画像ファイルは考えないためアスペクト比を無視して800x600へ変換するよう"-resize"オプションの解像度指定に"!"を付けています。また、BGRA(各色8ビット)へピクセルフォーマットを変換するため"-depth 8"オプションと変換後の画像ファイル名に".bgra"の拡張子を付けています。なお、ヘッダ等のメタデータが無い形式へ変換していますので、ファイルの中身は先頭からピクセルデータ(BGRA)が並びます。

== 1枚の画像を表示してみる
まずは1枚の画像表示だけを行ってみます。

サンプルディレクトリは"061_iv_view_a_image"です。

前節で用意した画像ファイルがフレームバッファの解像度にもピクセルフォーマットにも既に合わせてあって、かつファイル先頭からピクセルデータが並ぶため、やることは単に以下の2点です。

 1. 画像ファイルを開く
 2. 画像ファイルのデータをフレームバッファへコピー

さっそくmain.cを書き変えて行きたい所ですが、2.のコピー処理は「特定のメモリアドレスから特定のメモリアドレスへ指定されたバイト数をコピーする」という汎用関数として用意しておくと便利そうです。

そのため、先にmemcpyという関数名でcommon.cとcommon.hへ関数を追加しておくことにします。

common.cの追加内容は@<list>{061_common_c}の通りです。

//list[061_common_c][061_iv_view_a_image/common.c][c]{
#include <common.h>

int strcmp(char *s1, char *s2)
{
	/* ・・・ 省略 ・・・ */
}

/* 追加(ここから) */
void memcpy(void *dst, void *src, unsigned long long size)
{
	unsigned char *d = (unsigned char *)dst;
	unsigned char *s = (unsigned char *)src;
	for (; size > 0; size--)
		*d++ = *s++;
}
/* 追加(ここまで) */
//}

common.hの追加内容は@<list>{061_common_h}の通りです。

//list[061_common_h][061_iv_view_a_image/include/common.h][c]{
/* ・・・ 省略 ・・・ */

int strcmp(char *s1, char *s2);
void memcpy(void *dst, void *src, unsigned long long size);	/* 追加 */

#endif
//}

これで必要な道具は用意できました。あとはmain.cを変更するだけです(@<list>{061_main_c})。

//list[061_main_c][061_iv_view_a_image/main.c][c]{
/* ・・・ 省略 ・・・ */
#include <fs.h>
#include <common.h>	/* 追加 */

void start_kernel(void)
{
	/* ・・・ 省略 ・・・ */

	/* CPUの割り込み有効化 */
	enable_cpu_intr();

	/* 変更(ここから) */
	/* "image.bgra"を開いて表示 */
	struct file *img = open("image.bgra");
	memcpy(fb.base, img->data, img->size);
	/* 変更(ここまで) */

	/* haltして待つ */
	while (1)
		cpu_halt();
}
//}

Makefileの変更点は無いです。makeしてkernel.binを生成してください。

fs.imgは、前節のconvertコマンドで"image.bgra"という名前で画像ファイルを用意し、前章で作成したcreate_fs.shを使用して生成します。

//cmd{
$ @<b>{convert 元画像ファイル -resize 800x600! -depth 8 image.bgra}
$ @<b>{./create_fs.sh image.bgra}
$ @<b>{ls fs.img}
fs.img	# "fs.img"が生成される
//}

実行すると@<img>{061_iv_view_a_image}のように画像が表示されます。ここでは表紙の元画像を表示してみています。

//image[061_iv_view_a_image][061_iv_view_a_imageの実行結果]{
//}

なお、main.c(@<list>{061_main_c})にてmemcpy関数へ指定するコピーサイズ(第3引数)はstruct fileのsizeメンバを使用していました。そのため、例えばフレームバッファサイズが640x480な環境で実行する際は上記のconvertコマンドとcreate_fs.shコマンドでfs.imgさえ作り直せば、kernel.binは変更なしで画面表示が行えます。

== ファイルシステムからファイルの一覧情報を取得する
これまではopen関数を使用して「このファイル名のファイルをくれ」とファイルシステムへ問い合わせていました。しかし、画像ビューアを作成するにはファイルシステムに存在するファイルのリストを問い合わせる必要があります。まだこのような問い合わせにはファイルシステム(fs.c)が対応していないので、そのための機能追加を行います。

この節のサンプルディレクトリは"062_iv_ls"です。

やり方はいろいろとあるかと思うのですが、本書では以下のようにfs.cへ実装することにします。

 * unsigned long long get_files(struct file *files[])
 * 引数: ファイルシステム内の各ファイルのstruct fileへのポインタを格納する領域を指定
 * 戻り値: ファイルシステム内のファイルの個数

このような実装方針にした思想は脚注@<fn>{about_implementation_policy}に記載します。これまでの実装も同じ思想に従っていたのですが、読者の方には関係ないので(お好きなように実装されれば良いので)脚注へ記載します。
//footnote[about_implementation_policy][自身が管理する機能の使い方を利用者が知らなくても良いように実装しています。例えばfs.cの場合、「各ファイルはアドレス上連続に並んでいること」や「ファイルシステムの最後は0x00で示すこと」はfs.cで隠ぺいすべきと考え、get_files()のような実装としています。]

それでは、fs.cへ実装してみます。内容は@<list>{062_fs_c}の通りです。

//list[062_fs_c][062_iv_ls/fs.c][c]{
/* ・・・ 省略 ・・・ */

struct file *open(char *name)
{
	/* ・・・ 省略 ・・・ */
}

/* 追加(ここから) */
unsigned long long get_files(struct file *files[])
{
	struct file *f = fs_start;
	unsigned int num;

	for (num = 0; f->name[0] != END_OF_FS; num++) {
		files[num] = f;
		f = (struct file *)((unsigned long long)f + sizeof(struct file)
				    + f->size);
	}

	return num;
}
/* 追加(ここまで) */
//}

fs.hへプロトタイプ宣言の追加も行います(@<list>{062_fs_h})。

//list[062_fs_h][062_iv_ls/include/fs.h][c]{
/* ・・・ 省略 ・・・ */

void fs_init(void *_fs_start);
struct file *open(char *name);
unsigned long long get_files(struct file *files[]);	/* 追加 */

#endif
//}

get_files関数を使用して、試しにmain.cでファイルシステム内のファイル名を表示してみます(@<list>{062_main_c})。

//list[062_main_c][062_iv_ls/main.c][c]{
/* ・・・ 省略 ・・・ */

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *_fb)
{
	/* ・・・ 省略 ・・・ */

	/* CPUの割り込み有効化 */
	enable_cpu_intr();

	/* 変更(ここから) */
	/* ファイルシステム内のファイル名を表示 */
	struct file *files[10];
	int num = get_files(files);
	int i;
	for (i = 0; i < num; i++) {
		puts((char *)files[i]->name);
		puts("\r\n");
	}
	/* 変更(ここまで) */

	/* haltして待つ */
	while (1)
		cpu_halt();
}
//}

試しに前章で"HELLO.TXT"と"FOO.TXT"を格納したfs.imgで実行すると@<img>{062_iv_ls}のように表示されます。

//image[062_iv_ls][062_iv_lsの実行結果]{
//}

== 画像ビューアを実装する
必要な道具はそろったので、画像ビューアを実装していきます。

この節のサンプルディレクトリは"063_iv_image_viewer"です。

=== 実装方針
画像ビューアはiv.cとiv.hへ実装していくこととします。

そして、他の機能とやり取りするインタフェースとなる関数としては以下の2つを作成します。

 * iv_init: 初期化処理。ファイルシステムからファイルリスト取得と、最初の1枚目の画像表示を行う
 ** start_kernel関数で各種初期化処理終了後、割り込み有効化直前に呼び出される
 * iv_kbc_handler: キー入力に応じた処理を行う
 ** KBCハンドラから呼び出される

=== 最初の画像表示処理を実装する(iv_init)
まず、iv_init関数を実装します(@<list>{063_iv_init})。

//list[063_iv_init][063_iv_image_viewer/iv.c(iv_init())][c]{
#include <iv.h>
#include <fb.h>
#include <fs.h>
#include <common.h>

struct file *iv_files[MAX_IV_FILES];
unsigned long long iv_num_files;
unsigned long long iv_idx = 0;

void view(unsigned long long idx)
{
	memcpy(fb.base, iv_files[idx]->data, iv_files[idx]->size);
}

void iv_init(void)
{
	iv_num_files = get_files(iv_files);
	view(iv_idx);
}
//}

iv_init関数はファイルシステム内の全てのファイルを画像ファイルとして扱います("ファイルシステム内には画像ファイルしか無いものとする"と仕様で決めた通りです)。また、iv.cの内部で使う関数としてファイルシステム上のidx番目(引数で指定)の画像表示を行うview関数も作成しました。

iv.hは@<list>{063_iv_h_iv_init}の通りです。

//list[063_iv_h_iv_init][063_iv_image_viewer/include/iv.h][c]{
#ifndef _IV_H_
#define _IV_H_

#define MAX_IV_FILES	100

void iv_init(void);

#endif
//}

@<list>{063_iv_h_iv_init}で"MAX_IV_FILES"を100と定義しているため、画像ビューアで扱えるファイル数の上限は100です。100という数に特に根拠はありませんが、増やす場合はpoiboot.confで設定したfs.imgのロード先アドレスの領域が十分であるかUEFI Shellのmemmapコマンドで確認してください@<fn>{about_max_iv_files}。
//footnote[about_max_iv_files][800x600の解像度の時、1枚のBGRA画像のサイズは2MB弱のため、100枚の時はおおよそ200MB弱の領域が必要です。搭載メモリ(RAM)が4GB以上の時、0x0000000100000000のアドレスには、そこより小さいアドレスで割り当てた3GB分を引いた残り1GBのメモリが割り当てられている様です。]

そして、iv_init関数を呼び出すようmain.cを書き変えます(@<list>{063_main_c})。

//list[063_main_c][063_iv_image_viewer/main.c][c]{
/* ・・・ 省略 ・・・ */
#include <common.h>
#include <iv.h>		/* 追加 */

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *_fb)
{
	/* フレームバッファ周りの初期化 */
	fb_init(_fb);
	set_fg(255, 255, 255);
	set_bg(0, 70, 250);
	/* clear_screen()削除 */

	/* CPU周りの初期化 */
	gdt_init();
	intr_init();

	/* 周辺ICの初期化 */
	pic_init();
	kbc_init();

	/* 画像ビューアの初期化 */	/* 追加 */
	iv_init();			/* 追加 */

	/* CPUの割り込み有効化 */
	enable_cpu_intr();

	/* 動作確認で書いていたコード削除 */

	/* haltして待つ */
	while (1)
		cpu_halt();
}
//}

起動後最初の画像はiv_init関数で表示するので、clear_screen関数呼び出しは削除しました。

また、実装方針でも書きましたが、画像ビューア初期化処理中は割り込みは入ってほしくないため、割り込み有効化処理の直前でiv_init関数呼び出しを行っています@<fn>{koremade_mo}。
//footnote[koremade_mo][これまでmain.cへ書いていた各種の動作確認コードも厳密にはあまり割り込んで欲しく無い処理ではありますが、キーを押しっぱなしで起動とかでも無い限り特に問題は無いかと思います。(割り込まれたとしても文字が画面に出るか出ないかだけなので。)]

iv.cを追加しましたので、Makefileへも"iv.o"をビルド対象に追加しておきます(@<list>{063_makefile})。

//list[063_makefile][063_iv_image_viewer/Makefile]{
TARGET = kernel.bin
CFLAGS = -Wall -Wextra -nostdinc -nostdlib -fno-builtin -fno-common -Iinclude
LDFLAGS = -Map kernel.map -s -x -T kernel.ld
OBJS = main.o iv.o fbcon.o fb.o font.o kbc.o x86.o intr.o pic.o	\  # iv.oを追加
	handler.o fs.o common.o

$(TARGET): $(OBJS)
# ・・・ 省略 ・・・
//}

この時点で一度、動作確認してみると良いかと思います。("image.bgra"だけを格納した"fs.img"などを使うと良いです。)

=== KBC割り込み時の処理を実装する(iv_kbc_handler)
#@# まずはkbc.cからiv_handlerを呼び出すようにして、
#@# カーネルにはコールバック関数登録機能を持たせる
#@# - iv -> call backへ登録
#@# - kbc -> call backの関数を実行
それでは、割り込み時の処理を"iv_kbc_handler"という関数名でiv.cへ実装します(@<list>{063_iv_kbc_handler})。

//list[063_iv_kbc_handler][063_iv_image_viewer/iv.c(iv_kbc_handler())][c]{
/* ・・・ 省略 ・・・ */

struct file *iv_files[MAX_IV_FILES];
unsigned long long iv_num_files;
unsigned long long iv_idx = 0;	/* 追加 */

/* ・・・ 省略 ・・・ */

void iv_init(void)
{
	iv_num_files = get_files(iv_files);
	view(iv_idx);
}

/* 追加(ここから) */
void iv_kbc_handler(char c)
{
	switch (c) {
	case 'j':
		if (iv_idx < iv_num_files - 1)
			view(++iv_idx);
		break;
	case 'k':
		if (iv_idx > 0)
			view (--iv_idx);
	}
}
/* 追加(ここまで) */
//}

@<list>{063_iv_kbc_handler}にて、iv_kbc_handler関数は押下されたキーは引数で渡される事を想定しています(後ほどそのようにKBCのハンドラから呼び出します)。

iv_kbc_handler関数内の処理としては、グローバル変数で用意したiv_idx(現在の画像のインデックス)を使用して、'j'キー押下で次の画像を表示し、'k'キー押下で前の画像を表示するようにしています。iv_idxが最後の画像を指していたり、最初の画像を指していた場合は何もしないようにしています。

iv_kbc_handler関数をKBC割り込みハンドラから呼べるようにiv.hへプロトタイプ宣言を追加します(@<list>{063_iv_kbc_handler})。

//list[063_iv_kbc_handler][063_iv_image_viewer/include/iv.h(iv_kbc_handler())][c]{
#ifndef _IV_H_
#define _IV_H_

#define MAX_IV_FILES	100

void iv_init(void);
void iv_kbc_handler(char c);	/* 追加 */

#endif
//}

そして、KBC割り込みハンドラ(do_kbc_interrupt関数)でiv_kbc_handler関数を呼び出すようkbc.cを変更します(@<list>{063_kbc_iv_call})。

//list[063_kbc_iv_call][063_iv_image_viewer/kbc.c(iv_kbc_handler()呼び出し)][c]{
/* ・・・ 省略 ・・・ */
#include <pic.h>
#include <iv.h>		/* 追加 */

/* ・・・ 省略 ・・・ */

void do_kbc_interrupt(void)
{
	/* ステータスレジスタのOBFがセットされていなければreturn */
	if (!(io_read(KBC_STATUS_ADDR) & KBC_STATUS_BIT_OBF))
		goto kbc_exit;

	/* make状態でなければreturn */
	unsigned char keycode = io_read(KBC_DATA_ADDR);
	if (keycode & KBC_DATA_BIT_IS_BRAKE)
		goto kbc_exit;

	/* KBC割り込み処理を呼び出す */	/* 変更 */
	char c = keymap[keycode];
	iv_kbc_handler(c);		/* 変更 */

kbc_exit:
	/* PICへ割り込み処理終了を通知(EOI) */
	set_pic_eoi(KBC_INTR_NO);
}

void kbc_init(void)
{
	set_intr_desc(KBC_INTR_NO, kbc_handler);
	enable_pic_intr(KBC_INTR_NO);
}
//}

これで追加・変更は終わりです。

convertコマンドでBGRA画像を何枚か用意し、create_fs.shスクリプトでfs.imgを生成してください。

#@# fs.imgのサイズは第1章で確認したアプリケーション領域のサイズまで大丈夫です。

実行すると、画像が表示され、'j'キー/'k'キーで画像送り/戻りが確認できます(@<img>{063_iv_image_viewer})。ここでは表紙・裏表紙の元画像を表示してみています。

//image[063_iv_image_viewer][画像ビューア実行の流れ]{
//}

===[column] 割り込みハンドラからアプリケーションの機能を呼び出す構成について
　趣味で書くOSは何をどうやるのも自由なので、設計についても自分なりに考えてみる事は面白いです。

　今回はやりたいことを実現する手っ取り早い方法としてkbc.c(KBCドライバ)内のKBC割り込みのハンドラから画像ビューアの割り込み処理を呼び出すようにしましたが、設計としては微妙な気がします。

　KBCドライバはKBCを管理・制御する事に従事すべきで、「画像ビューア」等という上位のアプリケーション(カーネルと一緒になってしまってはいますが)と直接お話しするべきでは無いかと思います。ドライバが直接やり取りする対象はカーネルなので、修正するとすれば、例えば、「割り込み時のコールバック関数を登録する機能をカーネル側に用意し、アプリケーションはカーネルへ『この関数を割り込み時に呼び出してくれ』とお願いする」等でしょうか。

　もっと考えると、ハードウェアの機能である「割り込み」をアプリケーションに扱わせるのも微妙かもしれません。アプリケーションは「割り込みを使いたい」のではなく単に「データが欲しい」だけな場合があります。今回の場合、「キー入力があった事とその入力値」を知ることができれば割り込みで無くとも何でも構わないのです。そのため、C言語でアプリケーションを作るときのgetc関数のように「アプリケーションへは当該データを得るためのインタフェースを提供し、割り込み等の処理はカーネル内へ隠蔽する」というのも良いかと思います。
