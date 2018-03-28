= キーボード入力
文字を出力することはできたので、次は順当にキーボードから文字の入力を取得してみます。もはやレガシーなデバイスとなりますが、本書ではキーボードコントローラ(KBC)からキー入力を取得してみます(@<fn>{kbc_can_use_qemu_lenovo})。
//footnote[kbc_can_use_qemu_lenovo][レガシーデバイスではありUSBに置き替えられる噂も聞きますが、QEMUと実機(少なくともLenovo ThinkPad E450)で今のところ使用できます。]

== キーボードコントローラ(KBC)の使い方
キーボードコントローラは32ビット(そしてそれ以前)のアーキテクチャから存在するコントロールICで、その名の通りキーボードの制御を行ってくれます。昔はIntelの8042(i8042)というICがKBCとしてマザーボードに搭載されていましたが、集積度が上がり、現在は互換の機能がひとつのSoC@<fn>{about_soc}内に入っています。
//footnote[about_soc][System on Chipの略。一つのチップの中に複数のICに相当する機能が集積されている。]

どんな制御ICでもそうですが、KBCもレジスタ経由で操作します。各レジスタにはアドレスが割り当てられており、特定のアドレスへ読み書きすることで制御ICのレジスタへアクセスできます。ただし、Intelの場合、周辺の制御ICのレジスタは通常のメモリとは別のアドレス空間であり、io命令(in命令とout命令)を使用してアクセスします。

なお、i8042は枯れたICであり、インターネット上にも情報があります@<fn>{about_i8042}。32ビットのx86 CPUでQEMU向けの自作OS(OS5)の全コード+コメンタリー本"Ohgami's Commentary on OS5@<fn>{about_ocos5}"でもKBCを扱っています。筆者の他の同人誌同様、PDF版は無料で公開していますので、興味があれば見てみてください。
//footnote[about_i8042][むしろ枯れすぎてインターネット上から情報が消えてきています。]
//footnote[about_ocos5][@<href>{http://funlinux.org/os5/}]

== ステータスレジスタとデータレジスタ
KBCには現在の状態を取得するためのステータスレジスタと、入力されたキーのデータ(キーコード)を取得するためのデータレジスタがあります。

KBCのステータスレジスタとデータレジスタのIOアドレスと読み出せる値の意味は@<table>{kbc_ioaddr}の通りです。

//table[kbc_ioaddr][KBCのレジスタのIOアドレスと読み出せる値の意味]{
IOアドレス	レジスタ名と読み出した値の意味
------------
0x0060	データレジスタ
　	bit7  : 押下状態(make=0 / brake=1)
　	bit6-0: キーコード
0x0064	ステータスレジスタ
　	bit7-4: 予約
　	bit3  : KBCコマンド/パラメータ書込フラグ
　	bit2  : 予約
　	bit1  : IBF(入力バッファフル)
　	bit0  : OBF(出力バッファフル)
//}

@<table>{kbc_ioaddr}の2つのレジスタのビットフィールドの中で、ステータスレジスタ(0x0064)のbit0と、データレジスタ(0x0060)の全ビットを使用します。

ステータスレジスタのOBFとIBFは、少しわかりにくいのですが、KBC自身から見た入力/出力であるため、KBCというICに対して外部に存在するx86 CPUはKBCの出力バッファからデータを読み出す事になりますので、KBCのOBF(出力バッファフル)を確認してデータ読み出しを行います。

データレジスタの押下状態(bit7)は、指でキーを押している状態を"make"、キーから指を離した状態を"brake"と呼んでいます。そのため、キーを押しっぱなしにしている間はデータレジスタのbit7は0(make)で読み出せ、キーから指を離すと一度だけbit7が1(brake)のデータがデータレジスタから読み出せます。

== インラインアセンブラ
in命令はアセンブラで記述しなければなりませんが、本書では極力C言語で記述するためにインラインアセンブラを使用してみます。

まず、in命令を使用したIOアドレスのレジスタ読み出しはアセンブラで@<list>{030_kbc_read_reg_ex}のように記述します。

//list[030_kbc_read_reg_ex][in命令を使用したステータスレジスタ読み出し例(アセンブラ)]{
	movw	$0x0064, %dx
	inb	%dx, %al
//}

@<list>{030_kbc_read_reg_ex}について、アセンブラではmovbやinbといった命令に当たる部分を「オペコード」、続く"$0x0064, %dx"等を「オペランド」と呼びます。

オペコードにはmovやinといった命令に'w'や'b'といった一文字が付いていますが、これはデータサイズを示しています。'w'は2バイト、'b'は1バイトで、他にも'q'(8バイト)、'l'(4バイト)があります。

@<list>{030_kbc_read_reg_ex}では、ステータスレジスタのアドレス(0x0064)をmov命令でdxレジスタへ格納し、in命令でdxレジスタが指すIOアドレスの値(KBCステータスレジスタの値)をalレジスタへ格納します。

@<list>{030_kbc_read_reg_ex}をインラインアセンブラで記述すると@<list>{030_kbc_read_reg_inline_ex}の様になります。

#@# linux/arch/x86/boot/boot.h
#@# static inline u8 inb(u16 port)
#@# {
#@# 	u8 v;
#@# 	asm volatile("inb %1,%0" : "=a" (v) : "dN" (port));
#@# 	return v;
#@# }

//list[030_kbc_read_reg_inline_ex][インラインアセンブラでのステータスレジスタ読み出し例][c]{
unsigned short ioaddr = 0x0064;
unsigned char value;
asm volatile ("inb %[ioaddr], %[value]"
              : [value]"=a"(value) : [ioaddr]"d"(ioaddr));
//}

@<list>{030_kbc_read_reg_inline_ex}について、"asm"がインラインアセンブラであることを示す指定です。"asm"というシンボルを別の用途で使用したい場合等は"__asm__"と書くこともできます(そのように書いているコードも多いです)。そして、コンパイラの最適化の影響を受けない様、"volatile"を付けています。

@<list>{030_kbc_read_reg_inline_ex}の"inb %[ioaddr], %[value]"が元のアセンブラ(@<list>{030_kbc_read_reg_ex})の"inb %dx, %al"に当たる箇所です。なんとなく分かるかと思いますが、変数"ioaddr"のIOアドレスにあるレジスタの値を変数"value"へ読み出しています。

ちゃんと説明すると、インラインアセンブラ内の要素はコロン(:)区切りで@<list>{desc_inline_asm}のように並んでいます@<fn>{about_inline_asm_4th_operand}。
//footnote[about_inline_asm_4th_operand][入力オペランドの後ろにもう一つコロンで区切る事で「使用するレジスタのリスト」を指定することもできますが、本書では使用しないため特に説明しません。]

//list[desc_inline_asm][インラインアセンブラの構成要素]{
asm volatile (アセンブラテンプレート : 出力オペランド : 入力オペランド)
//}

そして、出力オペランドと入力オペランドのフォーマットは@<list>{inline_asm_ioformat}の通りです(入力オペランドと出力オペランドの違いは'='が付くかどうかだけです)。

//list[inline_asm_ioformat][入力/出力オペランドのフォーマット]{
■ 出力オペランド
=[アセンブラから参照する名前]"使用するレジスタ等の指定"(変数名)
■ 入力オペランド
[アセンブラから参照する名前]"使用するレジスタ等の指定"(変数名)
//}

@<list>{inline_asm_ioformat}の"使用するレジスタ等の指定"には、in命令の仕様の都合上、入力オペランドには"d"のレジスタを出力オペランドには"a"のレジスタを指定しています。

===[column] AX、EAX、RAX等のレジスタ名について
x86の汎用レジスタには、32ビットではEAXやEBX、64ビットではRAXやRBXのように頭に'E'、あるいは'R'が付き、最後に'X'で終わる名前のレジスタがいくつかあります。

この様な命名には歴史的な経緯があります。そもそも、初めはAやBといった8ビットのレジスタがあり、CPUの16ビットへの進化に併せて8ビットから16ビット拡張され"extended"の意味で'X'を付け、16ビットレジスタはAXレジスタ、BXレジスタといった名前となりました。その後、32ビットへのさらなる拡張に伴い、同様に"extended"の意味で'E'が頭に付けられ32ビットレジスタはEAX、EBXといった名前となりましたが、64ビットでは流石に"extended"から文字を取るのに限界を感じたのかRAX、RBXという名前になっています@<fn>{about_reg_name}@<fn>{about_64bit_reg_name}。
//footnote[about_reg_name][書籍"自作エミュレータで学ぶx86アーキテクチャ: コンピュータが動く仕組みを徹底理解！"より]
//footnote[about_64bit_reg_name][64ビットモードでは汎用レジスタ自体も16個へ増えており、増えた分は"R8"〜"R15"という名前になっており、RAX等の'R'は単に"Register"を指すものと思われます。]

同じアルファベットに対して'E'や'R'、'X'が付いたレジスタはアクセスするサイズが違うだけで同じレジスタを指していて、RAXの下位32ビットをEAXでアクセスでき、EAXの下位16ビットをAXでアクセスできます。AXの上位と下位8ビットはそれぞれ"AH"・"AL"という名前でアクセスできます('A'というレジスタ名は無くなりました)@<fn>{about_64bit_new_regs}。
//footnote[about_64bit_new_regs][R8〜R15の様な64ビットレジスタはR8D〜R15Dという名前で下位32ビットアクセスできます。(16ビット単位/8ビット単位のレジスタ名はありません)]

== キー入力を取得する
それでは、キーが入力されたかどうかをソフトウェア的にステータスレジスタを監視する方法(ポーリング)で実装してみます。(ハードウェアによる割り込みを使用する方法は次章で紹介します。)

ソースコードはサンプルディレクトリの"030_keyinput_polling"に格納しています。

まずは、「指定したIOアドレスのレジスタから値を取得する」処理をC言語から汎用的に呼べるように関数化したいため、インラインアセンブラを使用して"io_read"という関数を用意することにします。x86 CPUに依存した関数群は"x86.c"というソースファイルにまとめることにし、対応して"x86.h"というヘッダファイルも用意します(@<list>{030_x86_h}、@<list>{030_x86_c})。

//list[030_x86_h][030_keyinput_polling/include/x86.h][c]{
#ifndef _X86_H_
#define _X86_H_

inline unsigned char io_read(unsigned short addr);

#endif
//}

//list[030_x86_c][030_keyinput_polling/x86.c][c]{
#include <x86.h>

inline unsigned char io_read(unsigned short addr)
{
	unsigned char value;
	asm volatile ("inb %[addr], %[value]"
		      : [value]"=a"(value) : [addr]"d"(addr));
	return value;
}
//}

それでは、io_read関数を使用して、データレジスタの取得を行う"get_kbc_data"関数と、get_kbc_data関数を使用してキーコードの取得を行う"get_keycode"関数を作成します。KBCに依存した処理のため"kbc.c"で定義することにします。内容は@<list>{030_get_kbc_data_get_keycode}の通りです。

//list[030_get_kbc_data_get_keycode][030_keyinput_polling/kbc.c(get_kbc_data()とget_keycode())][c]{
#define KBC_DATA_ADDR		0x0060
#define KBC_DATA_BIT_IS_BRAKE	0x80
#define KBC_STATUS_ADDR		0x0064
#define KBC_STATUS_BIT_OBF	0x01

unsigned char get_kbc_data(void)
{
	/* ステータスレジスタのOBFがセットされるまで待つ */
	while (!(io_read(KBC_STATUS_ADDR) & KBC_STATUS_BIT_OBF));

	return io_read(KBC_DATA_ADDR);
}

unsigned char get_keycode(void)
{
	unsigned char keycode;

	/* make状態(brakeビットがセットされていない状態)まで待つ */
	while ((keycode = get_kbc_data()) & KBC_DATA_BIT_IS_BRAKE);

	return keycode;
}
//}

ポーリング(ソフトウェア的な待機処理)を行っているのが正に@<list>{030_get_kbc_data_get_keycode}のget_kbc_data関数とget_keycode関数それぞれのwhileです。

また、@<list>{030_get_kbc_data_get_keycode}のget_keycode関数ではmake状態のキーコードを待ちます。brake状態のキーコードを待つようにしたい場合はwhile内の条件を反転させてください。

キーコードを取得できたので、キーコードをASCIIへ変換し、画面へ表示してみます。キーコードをASCIIへ変換する配列"keymap"と、キー入力1文字を取得する"getc"関数、そして各種ヘッダのincludeを"kbc.c"へ追加します(@<list>{030_kbc_c})。

//list[030_kbc_c][030_keyinput_polling/kbc.c][c]{
#include <kbc.h>	/* 追加 */
#include <x86.h>	/* 追加 */

#define KBC_DATA_ADDR		0x0060
#define KBC_DATA_BIT_IS_BRAKE	0x80
#define KBC_STATUS_ADDR		0x0064
#define KBC_STATUS_BIT_OBF	0x01

/* 追加(ここから) */
const char keymap[] = {
	0x00, ASCII_ESC, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '^', ASCII_BS, ASCII_HT,
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '@', '[', '\n', 0x00, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	':', 0x00, 0x00, ']', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/', 0x00, '*',
	0x00, ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, '7',
	'8', '9', '-', '4', '5', '6', '+', '1',
	'2', '3', '0', '.', 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, '_', 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, '\\', 0x00, 0x00
};
/* 追加(ここまで) */

/* ・・・ 省略 ・・・ */

/* 追加(ここから) */
char getc(void)
{
	return keymap[get_keycode()];
}
/* 追加(ここまで) */
//}

そして、kbc.cのgetc関数と、keymap配列で使用しているASCII_ESC等のASCII制御文字は外部からも参照できるよう"kbc.h"に宣言と定義を用意します(@<list>{030_kbc_h})。

//list[030_kbc_h][030_keyinput_polling/include/kbc.h][c]{
#ifndef _KBC_H_
#define _KBC_H_

#define ASCII_ESC	0x1b
#define ASCII_BS	0x08
#define ASCII_HT	0x09

char getc(void);

#endif
//}

ここまでで、キー入力取得に必要な道具は揃いました。ここではmain.cを改造して、キー入力取得のサンプルとしてはよくある「エコーバック@<fn>{about_echoback}」を作ってみます。改造後のmain.cは@<list>{030_main_c}の通りです。
//footnote[about_echoback][入力された文字をそのまま画面へ表示するプログラム]

//list[030_main_c][030_keyinput_polling/main.c][c]{
#include <fb.h>
#include <fbcon.h>
#include <kbc.h>	/* 追加 */

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *fb,
		  void *_fs_start __attribute__ ((unused)))
{
	fb_init(fb);
	set_fg(255, 255, 255);
	set_bg(0, 70, 250);
	clear_screen();

	/* 変更箇所(ここから) */
	while (1) {
		char c = getc();
		if (('a' <= c) && (c <= 'z'))
			c = c - 'a' + 'A';
		else if (c == '\n')
			putc('\r');
		putc(c);
	}
	/* 変更箇所(ここまで) */
}
//}

現状のフォントの都合上、アルファベットは大文字しか表示できないので、@<list>{030_main_c}では入力された文字がアルファベットの場合、大文字に変換しています。また、Enterを押下すると'\n'(LF)を受け取るので、'\n'取得時は追加で'\r'(CR)も出力しています。

最後に今回追加したソースファイルをMakefileへ反映します(@<list>{030_makefile})。

//list[030_makefile][030_keyinput_polling/Makefile]{
TARGET = kernel.bin
CFLAGS = -Wall -Wextra -nostdinc -nostdlib -fno-builtin -fno-common -Iinclude
LDFLAGS = -Map kernel.map -s -x -T kernel.ld

$(TARGET): main.o fbcon.o fb.o font.o kbc.o x86.o  # kbc.oとx86.oを追加
	ld $(LDFLAGS) -o $@ $+

# ・・・ 省略 ・・・
//}

ここまででソースコードの追加・変更は終わりです。実行すると、@<img>{030_keyinput_polling}の様に入力した文字が画面に表示されます。

//image[030_keyinput_polling][030_keyinput_pollingの実行結果]{
//}
