= 読み取り専用の簡易ファイルシステムを用意する
次の章で画像ビューアを作ってみるに当たり、読み取り専用の簡易的なファイルシステムを用意してみます。

== poibootの第2のロード機能
poibootはカーネル(kernel.bin)だけでなく、もうひとつイメージをロードしてくれる機能があり、"apps.img"というファイル名でkernel.binと同じ階層に配置しておくと(@<list>{apps_img_same_floor})、poibootは0x00200000に配置します。

//list[apps_img_same_floor][apps.imgはkernel.imgと同じ階層へ配置]{
fsディレクトリ、あるいはUSBフラッシュメモリ等
├─ EFI/
│   └─ BOOT/
│        └─ BOOTX64.EFI
├─ kernel.bin
└─ apps.img  ← kernel.binと同じ階層に配置
//}

この章では、poibootが配置する0x00200000以降をファイルシステムの領域とし、ファイルシステム領域上のバイナリ列をファイルと見立てるルールを決めることで、簡易的なファイルシステムとします。

== 単一のテキストファイルを読んでみる
まずはpoibootの機能確認として、単一のテキストファイルをapps.imgというファイル名で配置し、0x00200000へファイルの内容がロードされていることを確認してみます。

この節のサンプルディレクトリは"050_fs_read_text"です。

まずはapps.img(テキストファイル)を用意します。

//cmd{
$ @<b>{echo -n 'HELLO\0' > apps.img}
//}

できあがった"apps.img"をkernel.binと同じディレクトリに配置してください。

次にapps.imgのロード先である0x00200000を読むようにkernel.binのプログラムを改造します。

前章の"040_intr"ディレクトリのmain.cへ@<list>{050_main_c}のように追記します。

//list[050_main_c][050_fs_read_text/main.c][c]{
#include <x86.h>
#include <intr.h>
#include <pic.h>
#include <fb.h>
#include <kbc.h>
#include <fbcon.h>	/* 追加 */

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *fb,
		  void *_fs_start)
{
	/* ・・・ 省略 ・・・ */

	/* CPUの割り込み有効化 */
	enable_cpu_intr();

	/* apps.img(テキストファイル)を読む */	/* 追加 */
	char *hello_str = (char *)_fs_start;	/* 追加 */
	puts(hello_str);			/* 追加 */

	/* haltして待つ */
	while (1)
		cpu_halt();
//}

apps.imgが0x00200000へロードされることで、"HELLO\0"が0x00200000から並ぶ事になるため、それをchar *として参照し、puts関数で出力するだけです。

実行すると@<img>{050_fs_read_text}の様に表示されます。

//image[050_fs_read_text][050_fs_read_textの実行結果]{
//}

"040_intr"のKBC割り込み時のエコーバックは残っているのでキー入力すると"HELLO"に続いて表示されます。

また、"font.c"にアルファベットは大文字しか用意していないのでapps.imgとして小文字のテキストを用意すると化けます。

== ファイルシステムを考える
前節ではapps.imgというファイル名でkernel.binと同じ階層へ配置しておけば、poibootが0x00200000へロードしてくれることを確認しました。

次は、単なるバイト列である"apps.img"の中に複数の「ファイル」を表現する「ルール」を考えます。本書で「ファイルシステム」として考える機能はこのような「ルール」です。シンプルに以下を考えてみました@<fn>{about_rofs_spec}。
//footnote[about_rofs_spec][ファイル名がASCIIで28文字までであったり、ファイルサイズが4GBまでだったりしますが、今必要な事以上のことは考えないことにします。]

1. 各「ファイル」は「ヘッダ」と「データ」で構成される
2. 「ヘッダ」は「ファイル名(28バイト)」と「ファイルサイズ(4バイト)」で構成される
3. apps.imgには、ファイルが先頭から隙間なく並べられている
4. ファイルシステム領域の終わりは1バイトの0x00で示す

これらのルールを図示すると@<img>{050_rofs_img}の通りとなります。

//image[050_rofs_img][バイト列をファイルへ見立てるルール]{
//}

== ファイルシステムを実装する
それでは、前節で説明したファイルシステムを解釈できるよう機能を追加していきます。

この節のサンプルディレクトリは"051_fs_read_fs"です。この節も一つ前のサンプル"050_fs_read_text"を元に作ります。

ファイルシステムに関しては"fs.h"と"fs.c"へ記述することにします。

"fs.h"へは「ファイル構造体(struct file)の定義」と「ファイル名を渡すとファイルシステム上の対応するstruct file *を返す関数(open)のプロトタイプ宣言」を行います(@<list>{051_fs_h})。

//list[051_fs_h][051_fs_read_fs/include/fs.h][c]{
#ifndef _FS_H_
#define _FS_H_

#define FILE_NAME_LEN	28

struct file {
	char name[FILE_NAME_LEN];
	unsigned int size;
	unsigned char data[0];
};

struct file *open(char *name);

#endif
//}

次に、"fs.c"へopen関数の実装を行っていきたいところですが、先に"common.c"と"common.h"というソースファイルを用意して「文字列比較関数(strcmp)」と「ヌルポインタ(NULL)」を定義しておきます。

"common.c"の内容は@<list>{051_common_c}の通りです。

//list[051_common_c][051_fs_read_fs/common.c][c]{
#include <common.h>

int strcmp(char *s1, char *s2)
{
	char is_equal = 1;

	for (; (*s1 != '\0') && (*s2 != '\0'); s1++, s2++) {
		if (*s1 != *s2) {
			is_equal = 0;
			break;
		}
	}

	if (is_equal) {
		if (*s1 != '\0') {
			return 1;
		} else if (*s2 != '\0') {
			return -1;
		} else {
			return 0;
		}
	} else {
		return (int)(*s1 - *s2);
	}
}
//}

strcmp関数は、文字列比較ができればどのような実装でも構いません。

"common.h"の内容は@<list>{051_common_h}の通りです。

//list[051_common_h][051_fs_read_fs/include/common.h][c]{
#ifndef _COMMON_H_
#define _COMMON_H_

#define NULL	(void *)0

int strcmp(char *s1, char *s2);

#endif
//}

これらを使用して、"open"関数をfs.cに作成します(@<list>{051_fs_c})。

#@# todo: fs_init()を追加する

//list[051_fs_c][051_fs_read_fs/fs.c][c]{
#include <fs.h>
#include <common.h>

#define FS_START_ADDR	0x0000000000200000
#define END_OF_FS	0x00

struct file *open(char *name)
{
	struct file *f = (struct file *)FS_START_ADDR;
	while (f->name[0] != END_OF_FS) {
		if (!strcmp(f->name, name))
			return f;

		f = (struct file *)((unsigned long long)f + sizeof(struct file)
				    + f->size);
	}

	return NULL;
}
//}

@<list>{051_fs_c}では、ファイルシステム先頭アドレスの定数名を"FS_START_ADDR"へ変更しました。open関数内はファイルシステムの終わりまでwhileループを回し、引数で与えたファイル名と同じものが見つかればstruct fileのポインタを返しています(見つからなければNULLを返します)。

そして、追加した関数を使用して複数のテキストファイル"HELLO.TXT"と"FOO.TXT"を読む処理をmain.cへ追加してみます(@<list>{051_main_c})。

//list[051_main_c][051_fs_read_fs/main.c][c]{
/* ・・・ 省略 ・・・ */
#include <fbcon.h>
#include <fs.h>	/* 追加 */

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *fb)
{
	/* ・・・ 省略 ・・・ */

	/* CPUの割り込み有効化 */
	enable_cpu_intr();

	/* 変更(ここから) */
	/* HELLO.TXTとFOO.TXTを1行目のみ読む */
	struct file *hello = open("HELLO.TXT");
	if (hello) {
		puts((char *)hello->name);
		putc(' ');
		puts((char *)hello->data);
	} else {
		puts("HELLO.TXT IS NOT FOUND.");
	}
	puts("\r\n");

	struct file *foo = open("FOO.TXT");
	if (foo) {
		puts((char *)foo->name);
		putc(' ');
		puts((char *)foo->data);
	} else {
		puts("FOO.TXT IS NOT FOUND.");
	}
	puts("\r\n");
	/* 変更(ここまで) */

	/* haltして待つ */
	while (1)
		cpu_halt();
}
//}

@<list>{051_main_c}では、open関数を使用して、ファイルシステムからファイル名に対応したstruct file *を取得し、ファイル名とデータの表示を行っています。データは先頭アドレスをそのままputs関数へ渡しているだけなので、1行目だけ表示されます。

最後にMakefileを変更します(@<list>{051_makefile})。

//list[051_makefile][051_fs_read_fs/Makefile][c]{
TARGET = kernel.bin
CFLAGS = -Wall -Wextra -nostdinc -nostdlib -fno-builtin -fno-common -Iinclude
LDFLAGS = -Map kernel.map -s -x -T kernel.ld
OBJS = main.o fbcon.o fb.o font.o kbc.o x86.o intr.o pic.o handler.o	\ # 追加
	fs.o common.o							  # 追加

$(TARGET): $(OBJS)	# 変更
	ld $(LDFLAGS) -o $@ $+

# ・・・ 省略 ・・・
//}

"$(TARGET)"の依存リストが長くなってきたので、$(OBJS)という変数へ分けました。

実行してみるには、あとファイルシステムイメージ(apps.img)の生成が必要です。次節で作成します。

== ファイルシステムイメージ(apps.img)出力スクリプトを作る
カーネル側へのファイルシステム機能の追加は終わりましたので、最後にファイルシステムイメージ(apps.img)を生成します。

サンプルディレクトリは"052_fs_create_fs"です。

「ファイルシステムを考える」の節で説明したルールを満たしたバイナリを"apps.img"として生成できれば何でも良いです。

ここではシェルスクリプトで実装してみました。内容は@<list>{052_sh}の通りです。

//list[052_sh][052_fs_create_fs/create_fs.sh][sh]{
#!/bin/bash

FILE_NAME_LEN=28
FS_IMG_NAME='apps.img'

while [ -n "$1" ]; do
	# ヘッダ作成
	## ファイル名を出力
	name=$(basename $1)
	echo -ne "$name\0"
	dd if=/dev/zero count=$((${FILE_NAME_LEN} - ${#name} - 1)) bs=1 \
	   status=none

	## ファイルサイズを出力
	### ファイルサイズを16進8桁へ変換
	size=$(stat -c '%s' $1)
	size_hex=$(printf "%08X" $size)
	### リトルエンディアンへ変換
	size_hex_le=''
	for b in $(echo $size_hex | fold -w2 -b); do
		size_hex_le="$b$size_hex_le"
	done
	### 8ビットずつ出力
	for b in $(echo $size_hex_le | fold -w2 -b); do
		printf '%b' "\x$b"
	done

	# データ出力
	dd if=$1 status=none

	# 位置変数をシフト
	shift
done > ${FS_IMG_NAME}	# whileループ内の出力はapps.imgへ出力

# ファイルシステムの終わり(0x00)をapps.imgへ追記
echo -ne "\x00" >> ${FS_IMG_NAME}
//}

@<list>{052_sh}の内容についてはコメントに記載のとおりです。

スクリプトは以下のように引数にファイルシステムへ格納したいファイルを指定して実行します。

//cmd{
$ @<b>{./create_fs.sh ファイル1 ファイル2 ...}
$ @<b>{ls apps.img}
apps.img	# apps.imgが生成される
//}

ここでは、以下のように2つのファイルを作成し、それらのファイルを含むapps.imgを作成してみます。

//cmd{
$ @<b>{echo -n "HELLO\0" > HELLO.TXT}		# HELLO.TXTを作成
$ @<b>{hexdump -C HELLO.TXT}			# 中身を確認
00000000  48 45 4c 4c 4f 00                                 |HELLO.|
00000006
$ @<b>{echo -n "FOO\0" > FOO.TXT}		# FOO.TXTを作成
$ @<b>{hexdump -C FOO.TXT}			# 中身を確認
00000000  46 4f 4f 00                                       |FOO.|
00000004
$ @<b>{./create_fs.sh HELLO.TXT FOO.TXT}	# apps.imgを作成
$ @<b>{ls apps.img}				# 作成されたことを確認
apps.img
//}

前節の変更を適用して作成したkernel.binとここで作成したapps.imgを実行すると@<img>{052_fs_create_fs}の様に2つのファイルのファイル名とファイルの内容が表示されます。

//image[052_fs_create_fs][052_fs_create_fsの実行結果]{
//}
