= 割り込みを使う
次は割り込みでキーボード入力を取得してみます。割り込みも割り込みコントローラ(PIC、Programmable Interrupt Controller)という外部IC@<fn>{about_pic_external_ic}の機能を利用します。ただし、割り込みの場合、CPU側でも設定が必要です。
//footnote[about_pic_external_ic][といっても今やKBCと同じくCPUに集積されています。]

この章では@<img>{4_pic}の構成で割り込み関連の機能の実装を行います。

//image[4_pic][この章で作る機能の構成]{
//}

この章ではCPU側の設定とPICの使い方を説明し、前章のキー入力を割り込みを使用して行うようにしてみます。章全体を通してサンプルディレクトリ"040_intr"の内容を説明します。

== CPU: セグメンテーション設定
セグメンテーションとは、メモリ空間を分割して管理する方法の一つで、「セグメント」と呼ぶ単位で管理します。各セグメントは「開始アドレス(Base Address)」と「セグメントサイズ(Segment Limit)」を持ち、セグメント毎に「コード(機械語)用である」とか「データ用である」とか、「カーネルモードでしかアクセスを許さない」といった「属性」を設定できます。

ただし、メモリ管理方法は今や「ページング」と呼ばれる方法が主流な様です。x86 CPUには、命令実行時のコード・データ・スタックについて、それぞれどのセグメントを参照するのかを指定するレジスタとして"CS(Code Segment)"・"DS(Data Segment)"・"SS(Stack Segment)"があるのですが、「IA-32eモード(64ビットのモード)では一般的には無効@<fn>{intel_sdm_324}」とのことです。ただ、「完全には無効ではない@<fn>{intel_sdm_324_2}」旨の括弧書きもあるため、一応、初期化を行いますが、内容について詳しい説明は行いません(あまり実りは無いので)。
//footnote[intel_sdm_324][3.2.4 Segmentation in IA-32e Mode - Intel 64 and IA-32 Architectures Software Developer's Manual]
//footnote[intel_sdm_324_2][同じく、3.2.4 Segmentation in IA-32e Mode - Intel 64 and IA-32 Architectures Software Developer's Manual]

セグメンテーションの初期化に当たって行うことは以下の通りです。

 1. GDTの定義
 2. レジスタGDTRの設定(lgdt命令)
 3. レジスタCS・DS・SSの設定

=== GDTの定義
GDT(Global Descriptor Table)とは、セグメントを定義する"セグメントデスクリプタ(セグメント記述子)"の表です。メモリ上に用意しておき、GDTRというレジスタへ用意したGDTの先頭アドレスを設定する事でCPUはGDTの場所を知ります。

本書ではGDTを実行時に変化させる事は無いので、@<list>{040_gdt_def}の様にconstの配列で定義してしまいます。

//list[040_gdt_def][GDTの定義][c]{
const unsigned long long gdt[] = {
	0x0000000000000000,	/* NULL descriptor */
	0x00af9a000000ffff,	/* base=0, limit=4GB, mode=code(r-x),kernel */
	0x00cf93000000ffff	/* base=0, limit=4GB, mode=data(rw-),kernel */
};
//}

@<list>{040_gdt_def}では3つのセグメントデスクリプタを定義しています。GDTの一番最初のデスクリプタは0(NULL)で定義することが決められているのでその様にしています。2つ目はCode Segmentを、開始アドレス0、セグメントサイズ4GBのカーネルモード用として定義しています。3つ目はData Segmentで、開始アドレスとセグメントサイズとモードはCode Segmentと同じです@<fn>{segment_overlap}。セグメントサイズの4GBは設定できる最大値です@<fn>{segmentation_noproblem}。
//footnote[segment_overlap][セグメント同士は領域を被って定義することもできます]
//footnote[segmentation_noproblem][上述の通りそもそも64ビットのモードではセグメンテーションは一般的に無効な挙動となり、ここでは一応の安全のために設定しているだけです。この"4GB"を気にする必要は無いかと思います。]

=== レジスタGDTRの設定(lgdt命令)
定義したGDTの先頭アドレスを"GDTR(GDT Register)"へ設定することでCPUへGDTの情報を伝えます。そのために使用する命令が"lgdt(Load GDT)"です。

GDTRにはGDTの先頭アドレス(Base Address)とサイズ(Limit)を設定します。GDTRへ格納できるフォーマットでデータ列を用意しておき、作成したデータ列の先頭アドレスを指定してlgdt命令を実行すると、データ列をGDTRへ設定してくれます(@<list>{040_set_gdtr})。

//list[040_set_gdtr][GDTRの設定例][c]{
unsigned long long gdtr[2];
gdtr[0] = ((unsigned long long)gdt << 16) | (sizeof(gdt) - 1);
gdtr[1] = ((unsigned long long)gdt >> 48);
asm volatile ("lgdt gdtr");
//}

=== レジスタCS・DS・SSの設定
CS・DS・SS等のセグメントレジスタには、「セグメントセレクタ」と呼ばれる値を設定するのですが、「セグメントセレクタ」は少なくとも本書で扱う範囲においては「GDT先頭からのオフセット」だと考えて問題ありません@<fn>{about_ss}。
//footnote[about_ss][厳密には下位3ビットに属性値を格納し、それより上位にGDTのインデックスを指定するのですが、少なくとも本書の範囲内では下位3ビットは常に0のためセグメントセレクタはGDT先頭からのオフセットと同じです。]

DS(データセグメント)・SS(スタックセグメント)のレジスタは、単にmov命令を使用してレジスタ値を設定するだけです。ただしCS(コードセグメント)は、現在実行中のCSを切り替えることになるため、例外へ陥ることが無いよう、「予めスタックに設定値をpushしret(関数リターン命令)で設定」という方法で行います(@<list>{040_set_cs_ds_ss})。

//list[040_set_cs_ds_ss][CS・DS・SSの設定][c]{
/* DS・SSの設定 */
asm volatile ("movw $2*8, %ax\n"
	      "movw %ax, %ds\n"
	      "movw %ax, %ss\n");

/* CSの設定 */
unsigned short selector = 8;
unsigned long long dummy;
asm volatile ("pushq %[selector];"
	      "leaq ret_label(%%rip), %[dummy];"
	      "pushq %[dummy];"
	      "lretq;"
	      "ret_label:"
	      : [dummy]"=r"(dummy)
	      : [selector]"m"(selector));
//}

=== まとめ: gdt_init関数の定義
以上をまとめ、x86.cへgdt_init関数を@<list>{040_x86_c}の様に追加します@<fn>{about_040_x86_c}。
//footnote[about_040_x86_c][インラインアセンブラ内のマジックナンバーの定数化はサボりました。。]

//list[040_x86_c][040_intr/x86.c][c]{
#include <x86.h>

/* 追加(ここから) */
/* GDTの定義 */
const unsigned long long gdt[] = {
	0x0000000000000000,  /* NULL descriptor */
	0x00af9a000000ffff,  /* base=0, limit=4GB, mode=code(r-x),kernel */
	0x00cf93000000ffff   /* base=0, limit=4GB, mode=data(rw-),kernel */
};
unsigned long long gdtr[2];
/* 追加(ここまで) */

inline unsigned char io_read(unsigned short addr)
{
	/* ・・・ 省略 ・・・ */
}

/* 追加(ここから) */
void gdt_init(void)
{
	/* GDTRの設定 */
	gdtr[0] = ((unsigned long long)gdt << 16) | (sizeof(gdt) - 1);
	gdtr[1] = ((unsigned long long)gdt >> 48);
	asm volatile ("lgdt gdtr");

	/* DS・SSの設定 */
	asm volatile ("movw $2*8, %ax\n"
		      "movw %ax, %ds\n"
		      "movw %ax, %ss\n");

	/* CSの設定 */
	unsigned short selector = SS_KERNEL_CODE;
	unsigned long long dummy;
	asm volatile ("pushq %[selector];"
		      "leaq ret_label(%%rip), %[dummy];"
		      "pushq %[dummy];"
		      "lretq;"
		      "ret_label:"
		      : [dummy]"=r"(dummy)
		      : [selector]"m"(selector));
}
/* 追加(ここまで) */
//}

併せて、x86.hへgdt_init関数の宣言とセグメントセレクタの定数(SS_KERNEL_CODE,SS_KERNEL_DATA)を追加します(@<list>{040_x86_h})。

//list[040_x86_h][040_intr/include/x86.h][c]{
#ifndef _X86_H_
#define _X86_H_

#define SS_KERNEL_CODE	0x0008	/* 追加 */
#define SS_KERNEL_DATA	0x0010	/* 追加 */

inline unsigned char io_read(unsigned short addr);
void gdt_init(void);	/* 追加 */

#endif
//}

#@# CPUの機能として「セグメンテーション無効ビット」等は無いので、ここでは事実上セグメンテーションを無効化するように、コードとデータそれぞれのセグメントでメモリの全領域をカバーするように定義してしまいます@<fn>{segment_2}。ただし、セグメントデスクリプタ自体が64ビットのモードになって拡張されていないためセグメントサイズは最大の4GBで設定しています。
#@# //footnote[segment_2][それは結局、セグメンテーションでメモリを分割していないことと同じです。]

#@# == CPU設定: GDTとIDT
#@# CPU側はGDT(Global Descriptor Table)とIDT(Interrupt Descriptor Table)の設定を行います。

#@# これらは、「セグメンテーション」と呼ばれるメモリ空間の分割管理方法でメモリ空間を分割した際の各領域(セグメント)について記述する記述子(descriptor)の表(table)です。大域的に何でも登録する表(ただし割り込みは除く)がGlobal Descriptor Tableで、割り込みに関して登録する表がInterrupt Descriptor Tableです。そして、表の各エントリはセグメントに関する記述子であるため「セグメント記述子(Segment Descriptor)」と呼ばれたりします。

#@# GDTにしろIDTにしろ、それぞれの表はメモリ上に作成し、CPUへはそれぞれの表の先頭アドレスを設定します。GDTには"GDTR"というレジスタが、IDTには"IDTR"というレジスタがあり、それぞれのレジスタへアドレスを設定するための命令"lgdt(Load GDT)"、"lidt(Load IDT)"があります。

#@# また、GDT・IDT共に表の各エントリは以下の3つの要素を持つGDTは8バイト、IDTは16バイトのデータ列です。

#@#  * セグメントの開始アドレス(Base Address)
#@#  * セグメントサイズ(Segment Limit)
#@#  * 属性

#@# 以降ではGDTとIDTの初期化を行っていきます。

#@# === GDTの初期化
#@# x86 CPUは現在実行している「コード(機械語命令)」と「データ」、そして「スタック」それぞれのセグメントを設定するレジスタがあります。それが"CS(Code Segment)"・"DS(Data Segment)"・"SS(Stack Segment)"と呼ばれるレジスタです。これらのレジスタにはGDTのエントリをGDT先頭からのオフセット値として設定します。そのため、予めGDTを用意し、lgdt命令でCPUへGDTを伝えておく必要があります@<fn>{why_now_running}。
#@# //footnote[why_now_running][これまではUEFIが設定したGDTで動作していました。UEFIがどのような値を設定するかは不定なため、ここで自ら設定します。]

#@# なお、実は64ビットのモード(IA-32e Mode)ではCS・DS・SSは一般的には無効なのですが、完全に無効な訳ではない様です@<fn>{intel_sdm_324}。そのため、一応、GDTとGDTR、CS・DS・SSには安全な値を設定しておきます。
#@# //footnote[intel_sdm_324][3.2.4 Segmentation in IA-32e Mode - Intel 64 and IA-32 Architectures Software Developer's Manual]

== CPU: IDT設定
IDT(Interrupt Descriptor Table)は、割り込みと例外のハンドラを管理する全256エントリ・各エントリ16バイトのテーブルです@<fn>{about_idt_name}。割り込みも例外も同じテーブルで扱う都合上、x86では割り込み番号0〜20番(IDTのインデックス0〜20)を例外、割り込み番号32〜255番(IDTのインデックス32〜255)を割り込みとしています(@<fn>{intel_sdm_vol3_615})。
//footnote[about_idt_name]["IDT"という名前ですが割り込み(Interrupt)も例外(Exception)も管理します。]
//footnote[intel_sdm_vol3_615][6.15 EXCEPTION AND INTERRUPT REFERENCE - Intel 64 and IA-32 Architectures Software Developer's Manual Volume 3 : System Programming Guide]

各エントリにはハンドラのアドレスといくつかのパラメータを設定し、GDT同様にメモリ上に決まったデータ構造で配置します。lidt命令を使用してIDTRというレジスタへIDTを設定することで、CPUは当該の例外や割り込みが発生した際にIDTに設定されたハンドラを呼び出してくれます。

まとめると、IDT設定で行うことは以下の通りです。

 1. ハンドラ作成
 2. IDTの定義
 3. レジスタIDTRの設定(lidt命令)

=== ハンドラ作成
今ハンドリングしたいものはキーボード割り込み(正確にはKBCからの割り込み)だけなので、それ以外の割り込み/例外は余計な処理を行わないよう無限ループに陥るように設定することにします。そのため、作成する割り込みハンドラは以下の2つです。

 * キーボードハンドラ(kbc_handler): キーボード(KBC)割り込み時の処理を行う
 * それ以外のハンドラ(default_handler): 無限ループ

割り込みハンドラは"handler.s"というファイルへアセンブラで記述します@<fn>{why_write_handler_by_asm}。まず、default_handlerをhandler.sへ記述します(@<list>{040_default_handler})。
//footnote[why_write_handler_by_asm][関数定義時に"__attribute__ ((interrupt))"を付けておくと、後述する割り込みハンドラに対応した関数をコンパイラが生成してくれるらしいです。しかし、私の使用しているGCCバイナリでは無効になっているようで"'interrupt' attribute directive ignored"と言われてしまうので、本書ではオーソドックスにアセンブラで記述しています。]

//list[040_default_handler][040_intr/handler.s(default_handler)]{
	.global default_handler
default_handler:
	jmp	default_handler
//}

@<list>{040_default_handler}について、".global"はシンボルを他のファイルからも参照できるようにするための指定です。その後は、"default_handler"シンボルへジャンプすることで、"default_handler"へ来ると無限ループするようにしています。

続いて、kbc_handlerを定義します(@<list>{040_kbc_handler})。

//list[040_kbc_handler][040_intr/handler.s(kbc_handler)]{
/* ・・・ 省略 ・・・ */

/* 追加(ここから) */
	.global	kbc_handler
kbc_handler:
	push	%rax
	push	%rcx
	push	%rdx
	push	%rbx
	push	%rbp
	push	%rsi
	push	%rdi
	call	do_kbc_interrupt
	pop	%rdi
	pop	%rsi
	pop	%rbp
	pop	%rbx
	pop	%rdx
	pop	%rcx
	pop	%rax
	iretq
/* 追加(ここまで) */
//}

default_handlerは、呼び出されると無限ループに陥るためハンドラからリターンすることが無いので、@<list>{040_default_handler}の様な記述で良いのですが、kbc_handlerは必要な処理を終えたらユーザ空間へリターンする必要があります。@<list>{040_kbc_handler}では、割り込みハンドラとして必要な入口/出口処理のみアセンブラで記述し、主要な処理はC言語の関数(do_kbc_interrupt(後述))を呼び出す(call命令)ようにしています。

kbc_handlerは以下を行っています。

 * push/pop命令
 ** 汎用レジスタのスタックへの退避(push)と復帰(pop)@<fn>{why_save_load_gpreg}
 * call命令
 ** do_kbc_interrupt関数呼び出し
 * iretq命令
 ** 割り込みハンドラからのリターン処理

//footnote[why_save_load_gpreg][割り込みは、汎用レジスタを使用した何らかの処理の最中でも発生する可能性があります。そのため、割り込みハンドラ内で汎用レジスタを使用した結果、元の処理に戻ってきた時に汎用レジスタの内容が割り込み直前と異なってしまう事が無いよう、割り込みハンドラの入口で汎用レジスタをスタックへ退避し、出口でスタックから復帰しています。]

そして、KBC割り込みの処理の本体であるdo_kbc_interrupt関数は、kbc.cへ@<list>{040_do_kbc_interrupt}の様に追加します。

//list[040_do_kbc_interrupt][040_intr/kbc.c(do_kbc_interrupt())][c]{
#include <kbc.h>
#include <x86.h>
#include <fbcon.h>	/* 追加 */

/* ・・・省略・・・ */

char getc(void)
{
	return keymap[get_keycode()];
}

/* 追加(ここから) */
void do_kbc_interrupt(void)
{
	/* ステータスレジスタのOBFがセットされていなければreturn */
	if (!(io_read(KBC_STATUS_ADDR) & KBC_STATUS_BIT_OBF))
		return;

	/* make状態でなければreturn */
	unsigned char keycode = io_read(KBC_DATA_ADDR);
	if (keycode & KBC_DATA_BIT_IS_BRAKE)
		return;

	/* エコーバック処理 */
	char c = keymap[keycode];
	if (('a' <= c) && (c <= 'z'))
		c = c - 'a' + 'A';
	else if (c == '\n')
		putc('\r');
	putc(c);
}
/* 追加(ここまで) */
//}

KBC割り込み時、押下されたキーの情報を取得し、エコーバック処理を行うよう、前章で扱ったエコーバック処理をdo_kbc_interrupt関数へ持ってきています。@<fn>{kbc_interrupt_not_better}
//footnote[kbc_interrupt_not_better][割り込みハンドラ長い処理を行うのは良く無いとの考えもありますが、ここでは気にせず、割り込み契機で行いたい事は全てハンドラに実装してみることにします。(今はまだスケジューラも無く、ハンドラの処理が長引くことで被害を受ける人も居ないので。)]

===[column] call命令実行時、スタックポインタは16バイトアライメントされていること
　64ビットモードでの動作時、一部のCPU命令にはスタックポインタ(RSP)が16の倍数になっていること(16バイトアライメント)を前提にコンパイラが機械語コードを生成するものがあります。そのため、64ビットモードにおいて、コンパイラは、関数実行開始時のRSPが16バイトアライメントされるように機械語コードを生成します。

　C言語の中で関数を呼び出したり呼びだされたりしている間はコンパイラがよしなに調節してくれるので16バイトアライメントを気にする必要はありません。しかし、今回の割り込みハンドラの様に自力でアセンブラを書いて関数呼び出しを行う場合は注意が必要です。

　ただし、割り込み発生直後は、CPUがRSPを16バイトアライメント調節してから割り込みハンドラを呼び出してくれるので気にする必要はありません。

　注意しなければならないのは割り込みハンドラ等のように自身で書いたアセンブラからcall命令で別の関数を呼び出す場合です。

　call命令は、1)戻り先アドレス(8バイト)をスタックに積む(RSPは-8される)、2)呼び出し先の関数へジャンプ、という振る舞いをします。そのため、割り込みハンドラ先頭では、CPUが調整してくれる事もあり、16バイトアライメントであったとしても、call命令の挙動により戻り先アドレスをスタックに積むため、呼び出し先の関数先頭時点でRSPは戻り先アドレス分の8を引いた値となり、16バイトアライメントは崩れてしまいます。

　本書では汎用レジスタ退避も兼ねて7個のレジスタ値(各8バイト、計56バイト)をスタックに積んでからcall命令を実行しています。そのため、呼び出し先のdo_kbc_interrupt関数の先頭でのRSPは、割り込みハンドラ先頭時点から合計64(56バイト + 8バイト)引いた値となり16バイトアライメントは保たれています。

　なお、このコラムは以下の記事で勉強させてもらったものの受け売りです。詳しくは以下をご覧ください。

 * x86-64 モードのプログラミングではスタックのアライメントに気を付けよう - uchan note
 ** @<href>{http://uchan.hateblo.jp/entry/2018/02/16/232029}

=== IDTの定義
まず、IDTの各デスクリプタのデータ構造を"interrupt_descriptor"という名前の構造体としてx86.hに用意しておきます(@<list>{040_struct_intr_desc})。

//list[040_struct_intr_desc][040_intr/include/x86.h(interrupt_descriptor構造体)][c]{
/* ・・・ 省略 ・・・ */
#define SS_KERNEL_DATA	0x0010

/* 追加(ここから) */
#define MAX_INTR_NO	256

struct interrupt_descriptor {
	unsigned short offset_00_15;
	unsigned short segment_selector;
	unsigned short ist	: 3;
	unsigned short _zero1	: 5;
	unsigned short type	: 4;
	unsigned short _zero2	: 1;
	unsigned short dpl	: 2;
	unsigned short p	: 1;
	unsigned short offset_31_16;
	unsigned int   offset_63_32;
	unsigned int   _reserved;
};
/* 追加(ここまで) */

inline unsigned char io_read(unsigned short addr);
/* ・・・ 省略 ・・・ */
//}

次に、IDT初期化関数(intr_init)とIDTの各デスクリプタを設定する関数(set_intr_desc)を"intr.c"というソースファイルへ実装します。それぞれの関数では以下を行うことにします。

 * intr_init関数: IDTの全エントリをdefault_handlerで初期化
 * set_intr_desc関数: 引数で指定されたハンドラと割り込み番号で当該デスクリプタを設定する

intr.cは@<list>{040_intr_c}の通りです。

//list[040_intr_c][040_intr/intr.c(intr_init()とset_intr_desc())][c]{
#include <intr.h>
#include <x86.h>

#define DESC_TYPE_INTR	14

struct interrupt_descriptor idt[MAX_INTR_NO];

void default_handler(void);

void set_intr_desc(unsigned char intr_no, void *handler)
{
	idt[intr_no].offset_00_15 = (unsigned long long)handler;
	idt[intr_no].segment_selector = SS_KERNEL_CODE;
	idt[intr_no].type = DESC_TYPE_INTR;
	idt[intr_no].p = 1;
	idt[intr_no].offset_31_16 = (unsigned long long)handler >> 16;
	idt[intr_no].offset_63_32 = (unsigned long long)handler >> 32;
}

void intr_init(void)
{
	int i;
	for (i = 0; i < MAX_INTR_NO; i++)
		set_intr_desc(i, default_handler);
}
//}

@<list>{040_intr_c}について、set_intr_desc関数では、(見た通りですが、)interrupt_descriptor構造体の配列としてintr.c冒頭で定義したidtへ設定を行っています。どういったパラメータを設定する必要があるかは見たとおりですが、補足として、"type"はデスクリプタタイプで割り込み/例外の場合は14番を指定します。また、"p"は"Present"で、デスクリプタの存在フラグです@<fn>{descriptor_p_flag}。
//footnote[descriptor_p_flag][pフラグを0に設定しておくと、そのデスクリプタへのアクセスを例外(割り込み番号11番、Segment Not Present)で検出できます。]

併せて、intr.hを作成し、set_intr_desc関数とintr_init関数の宣言を追加します(@<list>{040_intr_h_set_intr_desc})。

//list[040_intr_h_set_intr_desc][040_intr/include/intr.h(set_intr_desc()とintr_init())][c]{
#ifndef _INTR_H_
#define _INTR_H_

void set_intr_desc(unsigned char intr_no, void *handler);
void intr_init(void);

#endif
//}

また、kbc.cへIDT設定処理をkbc_init関数として追加します(@<list>{040_kbc_set_intr_desc})。

//list[040_kbc_set_intr_desc][040_intr/kbc.c(kbc_init())][c]{
#include <kbc.h>
#include <x86.h>
#include <fbcon.h>
#include <intr.h>	/* 追加 */

#define KBC_DATA_ADDR		0x0060
#define KBC_DATA_BIT_IS_BRAKE	0x80
#define KBC_STATUS_ADDR		0x0064
#define KBC_STATUS_BIT_OBF	0x01
#define KBC_INTR_NO		33	/* 追加 */

/* ・・・ 省略 ・・・ */

const char keymap[] = {
	/* ・・・ 省略 ・・・ */
};

void kbc_handler(void);	/* 追加 */

/* ・・・ 省略 ・・・ */

void do_kbc_interrupt(void)
{
	/* ・・・ 省略 ・・・ */
}

/* 追加(ここから) */
void kbc_init(void)
{
	set_intr_desc(KBC_INTR_NO, kbc_handler);
}
/* 追加(ここまで) */
//}

併せて、kbc.hへプロトタイプ宣言を追加します(@<list>{040_kbc_h_kbc_init})。

//list[040_kbc_h_kbc_init][040_intr/include/kbc.h][c]{
/* ・・・ 省略 ・・・ */

char getc(void);
void kbc_init(void);	/* 追加 */

#endif
//}

=== レジスタIDTRの設定(lidt命令)
続いて、lidt命令を使用してIDTの先頭アドレスとサイズをレジスタ"IDTR(IDT Register)"へ設定します。

これらの処理はintr.cのintr_init関数へ追加することにします(@<list>{040_intr_c_2})。

//list[040_intr_c_2][040_intr/intr.c(IDTRの設定)][c]{
/* ・・・ 省略 ・・・ */

struct interrupt_descriptor idt[MAX_INTR_NO];
unsigned long long idtr[2];	/* 追加 */

/* ・・・ 省略 ・・・ */

void intr_init(void)
{
	int i;
	for (i = 0; i < MAX_INTR_NO; i++)
		set_intr_desc(i, default_handler);

	/* 追加(ここから) */
	idtr[0] = ((unsigned long long)idt << 16) | (sizeof(idt) - 1);
	idtr[1] = ((unsigned long long)idt >> 48);
	__asm__ ("lidt idtr");
	/* 追加(ここまで) */
}
//}

ここまででIDTの設定も完了で、セグメンテーション含めてCPUの割り込み設定が完了です。

== CPU: 割り込み有効化(sti命令)
CPU側の設定の最後に、sti(Set Interrupt Flag)命令で割り込みの有効化を行います。

"enable_cpu_intr"関数としてx86.cへ追加します(@<list>{040_enable_cpu_intr})。

//list[040_enable_cpu_intr][040_intr/x86.c][c]{
/* ・・・ 省略 ・・・ */

/* 追加(ここから) */
inline void enable_cpu_intr(void)
{
	asm volatile ("sti");
}
/* 追加(ここまで) */

inline unsigned char io_read(unsigned short addr)
/* ・・・ 省略 ・・・ */
//}

併せて、x86.hへプロトタイプ宣言を追加します(@<list>{040_x86_h_enable_cpu_intr})。

//list[040_x86_h_enable_cpu_intr][040_intr/include/x86.h][c]{
/* ・・・ 省略 ・・・ */

inline void enable_cpu_intr(void);	/* 追加 */
inline unsigned char io_read(unsigned short addr);
/* ・・・ 省略 ・・・ */
//}

enable_cpu_intr関数はmain.cから呼び出しますが、main.cの変更はこの章の最後で他の変更と合わせて行います。

== PIC: 割り込みコントローラ(PIC)設定
最後はPICの設定です。in/outの命令でPICのレジスタへアクセスし、設定を行います。

やることは以下の2つです。

 1. PICの初期化処理を実施
 2. 使用する割り込みのみ有効化

=== PIC初期化処理の流れと使用するレジスタについて
PICにはマスタPICとスレーブPICの2つがあり、初期化の際はマスタ/スレーブそれぞれに対して以下の初期化処理を実施します。

 1. ICW(Initialize Control Word)1へICW4設定の要不要を設定
 2. ICW2へ割り込み番号のベース値設定
 3. ICW3へマスター時は0x04を、スレーブ時は0x02を設定
 4. ICW4へ各種パラメータ設定
 5. OCW(Operation Control Word)1へ割り込みマスク設定

また、上記レジスタのIOアドレスと書き込む値の意味は@<table>{pic_ioaddr}の通りです。

//table[pic_ioaddr][PICのレジスタのIOアドレスと書き込む値の意味]{
IOアドレス(マスタ/スレーブ)	レジスタ名と書き込む値の意味
------------
0x0020/0x00a0	ICW1
　	bit7-4: 0x1
	bit3-1: 0b000
　	bit0  : ICW4要不要(必要=1 / 不要=0)
0x0021/0x00a1	ICW2
　	bit7-3: 割り込み番号[7:3]の値
　	bit2-0: 0
0x0021/0x00a1	ICW3
　	bit7-0: マスタは0x04を、スレーブは0x02を設定
0x0021/0x00a1	ICW4
　	bit7-5: 0b000
　	bit4  : SFNM(特殊完全ネストモード=1 / ノーマル完全ネストモード=0)
　	bit3-2: 0b00
　	bit1  : AEOI(自動EOIモード=1 / ノーマルEOIモード=0)
　	bit0  : 1
0x0021/0x00a1	OCW1
　	bit7-0: 割り込みマスク(禁止=1 / 許可=0)
//}

@<table>{pic_ioaddr}について、bit4が1の書き込みが0x0020(スレーブの場合0x00a0)に対して行われると、それはICW1への書き込みとして扱われます。ICW1へ書き込みが行われるとPICは初期化モードへ入ります。初期化モード後、0x0021(スレーブの場合0x00a1)へ書き込みを行うと、書き込んだ値はICW2、ICW3、ICW4(ICW1のbit0へ1を書き込んだ場合)の順で設定されます。最後のICW(3あるいは4)への書き込みで初期化モードを抜け、以降の0x0021(スレーブの場合0x00a1)への書き込みはOCW1への書き込みとして扱われます。

@<table>{pic_ioaddr}で重要な設定は、ICW2で設定する割り込み番号のbit7〜bit3の値です。マスタ・スレーブのPICはそれぞれ、8つずつ割り込みを受け付けることができます。それぞれのPICではIR0〜IR7というIR番号に割り込みが対応しており、IR番号は3ビットで表現できます。CPUへ割り込み番号として8ビットの値を通知する際に余った上位5ビットに好きな値を設定できるのがこのICW2です。

x86 CPUでは外部割り込みは32番以降のため、ICW2に32(0x20)を設定しておけば、PICは32番以降の値で割り込みを通知してくれます。なお、スレーブ側は8個分ずらして40(0x28)をICW2へ設定します。

@<table>{pic_ioaddr}のICW4のAEOI(自動EOI)やSFNM(特殊完全ネストモード)等の機能は簡単のため本書では使用しません@<fn>{why_dont_use_aeoi_sfnm}。それぞれが何であるかについては後述のコラムで説明していますので興味があれば見てみてください。ただし、"EOI"については別途割り込みハンドラを対応させる必要があるため、後述します。
//footnote[why_dont_use_aeoi_sfnm][高度な制御を行うなら今やAPICを勉強スべきかと思うので、本書では詳しくPICに突っ込んでは行きません。]

なお、@<table>{pic_ioaddr}では現代のPCでは固定値となるパラメータ等の説明は省略しています。PICは十分に枯れたデバイスですので、詳しくはインターネット上で調べてみるか、参考情報に挙げている書籍等を見てみてください。

=== PIC初期化処理の実装
それでは、初期化処理を実装していきます。

IOアドレスへの書き込みの関数が無いので、まずは書き込みの関数を"io_write"という名前でx86.cへ追加します(@<list>{040_io_write})。

//list[040_io_write][040_intr/x86.c(io_write())][c]{
/* ・・・ 省略 ・・・ */
inline unsigned char io_read(unsigned short addr)
{
	/* ・・・ 省略 ・・・ */
}

/* 追加(ここから) */
inline void io_write(unsigned short addr, unsigned char value)
{
	asm volatile ("outb %[value], %[addr]"
		      :: [value]"a"(value), [addr]"d"(addr));
}
/* 追加(ここまで) */
/* ・・・ 省略 ・・・ */
//}

併せて、x86.hへio_write関数の宣言を追加します(@<list>{040_x86_h_io_write})。

//list[040_x86_h_io_write][040_intr/include/x86.h][c]{
/* ・・・ 省略 ・・・ */
inline unsigned char io_read(unsigned short addr);
inline void io_write(unsigned short addr, unsigned char value);	/* 追加 */
/* ・・・ 省略 ・・・ */
//}

PICの初期化処理はpic.cというソースファイルを作成し、そこへpic_init関数として実装することにします(@<list>{040_pic_init})。

//list[040_pic_init][040_intr/pic.c(pic_init())][c]{
#include <pic.h>
#include <x86.h>

#define MPIC_ICW1_ADDR	0x0020
#define MPIC_ICW2_ADDR	0x0021
#define MPIC_ICW3_ADDR	0x0021
#define MPIC_ICW4_ADDR	0x0021
#define MPIC_OCW1_ADDR	0x0021
#define SPIC_ICW1_ADDR	0x00a0
#define SPIC_ICW2_ADDR	0x00a1
#define SPIC_ICW3_ADDR	0x00a1
#define SPIC_ICW4_ADDR	0x00a1
#define SPIC_OCW1_ADDR	0x00a1

#define INTR_NO_BASE_MASTER	32
#define INTR_NO_BASE_SLAVE	40

void pic_init(void)
{
	/* マスタPICの初期化 */
	io_write(MPIC_ICW1_ADDR, 0x11);
	io_write(MPIC_ICW2_ADDR, INTR_NO_BASE_MASTER);
	io_write(MPIC_ICW3_ADDR, 0x04);
	io_write(MPIC_ICW4_ADDR, 0x01);
	io_write(MPIC_OCW1_ADDR, 0xff);

	/* スレーブPICの初期化 */
	io_write(SPIC_ICW1_ADDR, 0x11);
	io_write(SPIC_ICW2_ADDR, INTR_NO_BASE_SLAVE);
	io_write(SPIC_ICW3_ADDR, 0x02);
	io_write(SPIC_ICW4_ADDR, 0x01);
	io_write(SPIC_OCW1_ADDR, 0xff);
}
//}

@<list>{040_pic_init}で設定している内容は先述した通りです。初期化完了後、OCW1へマスタ・スレーブ共に全ての割り込みを無効にするようマスクしています。(次節で割り込み有効化関数を追加します。)

併せて、@<list>{040_pic_h_pic_init}の様にpic.hを作成します。

//list[040_pic_h_pic_init][040_intr/include/pic.h(pic_init())][c]{
#ifndef _PIC_H_
#define _PIC_H_

void pic_init(void);

#endif
//}

=== 割り込み有効化関数の実装
pic_init関数では全ての割り込みをマスクしましたので、pic.cは割り込み有効化関数を提供し、デバイス側から割り込みマスクの解除が行えるようにします。

マスクの特定のビットのみ操作するためには現在のマスク値を読み出せる必要があります。現在の割り込みマスク値はIMR(Interrupt Mask Register)を読み出すことで確認できます(@<table>{table_pic_imr})。

//table[table_pic_imr][IMRのアドレスと読み出す値の意味]{
IOアドレス(マスタ/スレーブ)	レジスタ名と読み出す値の意味
------------
0x0021/0x00a1	IMR
　	bit7-0: 割り込みマスク(禁止=1 / 許可=0)
//}

@<table>{table_pic_imr}について、IMRのアドレスはOCW1と同じです。読めばIMRとしてマスク値が読み出せ、書けばOCW1としてマスク値を設定できます。

それでは、pic.cへenable_pic_intr関数として割り込み有効化関数を追加します(@<list>{040_enable_pic_intr})。

//list[040_enable_pic_intr][040_intr/pic.c(enable_pic_intr())][c]{
/* ・・・ 省略 ・・・ */
#define MPIC_ICW4_ADDR	0x0021
#define MPIC_OCW1_ADDR	0x0021
#define MPIC_IMR_ADDR	0x0021	/* 追加 */
/* ・・・ 省略 ・・・ */

void pic_init(void)
{
	/* ・・・ 省略 ・・・ */
}

/* 追加(ここから) */
void enable_pic_intr(unsigned char intr_no) /* ※ スレーブ未対応 */
{
	/* intr_no番のビットのみ立っているビットフィールド(ir_bit)を作成 */
	unsigned char ir_no = intr_no - INTR_NO_BASE_MASTER;
	unsigned char ir_bit = 1U << ir_no;

	/* 現在のマスク値を取得 */
	unsigned char mask = io_read(MPIC_IMR_ADDR);

	/* 既にマスク解除済みならば何もせずreturn */
	if (!(ir_bit & mask))
		return;

	/* マスク解除 */
	io_write(MPIC_OCW1_ADDR, mask - ir_bit);
}
/* 追加(ここまで) */
//}

@<list>{040_enable_pic_intr}では、スレーブ側を設定することは本書では無いので、簡単のためマスタPICにのみ対応しています。

併せて、pic.hへプロトタイプ宣言を追加します(@<list>{040_pic_h_enable_pic_intr})。

//list[040_pic_h_enable_pic_intr][040_intr/include/pic.h(enable_pic_intr())][c]{
#ifndef _PIC_H_
#define _PIC_H_

void pic_init(void);
void enable_pic_intr(unsigned char intr_no);	/* 追加 */

#endif
//}

これで特定の割り込みの有効化が行えるようになったので、kbc.cのkbc_init関数へ割り込み有効化の処理を追加します(@<list>{040_kbc_enable_pic_intr})。

//list[040_kbc_enable_pic_intr][040_intr/kbc.c(KBC割り込み有効化)][c]{
#include <kbc.h>
#include <x86.h>
#include <fbcon.h>
#include <intr.h>
#include <pic.h>	/* 追加 */

/* ・・・ 省略 ・・・ */

void kbc_init(void)
{
	set_intr_desc(KBC_INTR_NO, kbc_handler);
	enable_pic_intr(KBC_INTR_NO);	/* 追加 */
}
//}

=== PICのEOI(End Of Interrupt)設定
"EOI"は"End Of Interrupt"の略で、CPUからPICに対して割り込み処理の終了を通知するものです。PICのOCW2レジスタへEOI設定を行うとPICはCPUが割り込み処理を終了したと判断し、次の割り込みを受け付けるようになります。OCW2レジスタについては@<table>{table_ocw2}の通りです@<fn>{about_ocw2}。
//footnote[about_ocw2][OCW2はICW1と同じアドレスですが、書き込まれた時のbit4-3の値によって内部で判別されています。]

//table[table_ocw2][PICのOCW2レジスタのIOアドレスと書き込む値の意味]{
IOアドレス(マスタ/スレーブ)	レジスタ名と書き込む値の意味
------------
0x0020/0x00a0	OCW2
　	bit7-5: EOI方法(本書では「指定EOI(0b011)」を使用)
　	bit4-3: 0
　	bit2-1: EOI対象のIR番号指定
//}

こちらも「指定された割り込みのEOIを設定する」関数を用意します。"set_pic_eoi"という名前でpic.cへ追加します(@<list>{040_set_pic_eoi})。

//list[040_set_pic_eoi][040_intr/pic.c(set_pic_eoi())][c]{
#define MPIC_ICW1_ADDR	0x0020
#define MPIC_OCW2_ADDR	0x0020	/* 追加 */
/* ・・・ 省略 ・・・ */

#define PIC_OCW2_BIT_MANUAL_EOI	0x60	/* 追加 */

/* ・・・ 省略 ・・・ */

void enable_pic_intr(unsigned char intr_no)
{
	/* ・・・ 省略 ・・・ */
}

/* 追加(ここから) */
void set_pic_eoi(unsigned char intr_no)	/* ※ スレーブ未対応 */
{
	/* IR番号計算 */
	unsigned char ir_no = intr_no - INTR_NO_BASE_MASTER;

	/* ir_no番に対してEOI設定 */
	io_write(MPIC_OCW2_ADDR, PIC_OCW2_BIT_MANUAL_EOI | ir_no);
}
/* 追加(ここまで) */
//}

併せて、pic.hへプロトタイプ宣言を追加します(@<list>{040_pic_h_set_pic_eoi})。

//list[040_pic_h_set_pic_eoi][040_intr/include/pic.h][c]{
#ifndef _PIC_H_
#define _PIC_H_

void pic_init(void);
void enable_pic_intr(unsigned char intr_no);
void set_pic_eoi(unsigned char intr_no);	/* 追加 */

#endif
//}

set_pic_eoi関数を使用して、kbc.cのdo_kbc_interrupt関数へEOI処理を追加します(@<list>{040_kbc_c_set_pic_eoi})。

//list[040_kbc_c_set_pic_eoi][040_intr/kbc.c(set_pic_eoi())][c]{
/* ・・・ 省略 ・・・ */
void do_kbc_interrupt(void)
{
	/* ステータスレジスタのOBFがセットされていなければreturn */
	if (!(io_read(KBC_STATUS_ADDR) & KBC_STATUS_BIT_OBF))
		goto kbc_exit;	/* 変更 */

	/* make状態でなければreturn */
	unsigned char keycode = io_read(KBC_DATA_ADDR);
	if (keycode & KBC_DATA_BIT_IS_BRAKE)
		goto kbc_exit;	/* 変更 */

	/* エコーバック処理 */
	char c = keymap[keycode];
	if (('a' <= c) && (c <= 'z'))
		c = c - 'a' + 'A';
	else if (c == '\n')
		putc('\r');
	putc(c);

/* 追加(ここから) */
kbc_exit:
	/* PICへ割り込み処理終了を通知(EOI) */
	set_pic_eoi(KBC_INTR_NO);
/* 追加(ここまで) */
}
/* ・・・ 省略 ・・・ */
//}

===[column] AEOI(自動EOI)モードとは
　@<table>{pic_ioaddr}のICW4のbit1で設定する"AEOI"は「自動EOI」と呼ばれるもので、設定されているとソフトウェア的な割り込みハンドラの処理とは非同期にPICが次の割り込みを受け付けるようになります。

　本書では簡単のため、割り込みハンドラでEOIを設定してから次の割り込みを受け付けるようにするので、AEOIは使用しません。

===[column] SFNM(特殊完全ネストモード)とは
　@<table>{pic_ioaddr}のICW4のbit4の"SFNM"は「特殊完全ネストモード」と呼ばれるスレーブから多重に割り込みを受け付けるモードを使用するか否かの設定です。

　PICのマスタ・スレーブの関係は、スレーブが割り込み要求を上げる信号線がマスターの割り込み要求を受け付けるピンに接続されている関係となっており、スレーブ側で受け付けた割り込みはCPUではなくマスターPICへ伝わります。

　"AEOI"で前述した通り、PICは割り込み処理中は次の割り込みを受け付けません。そのため、スレーブ側で優先度が低い割り込みが発生し、マスタPICを通してCPUが処理中の間、スレーブで優先度の高い割り込みが発生すると、マスタがそもそも割り込みを受け付けないため優先度が高いにも関わらず割り込みが受け付けられないという優先度の逆転現象が発生します。

　このような問題を解決するためにスレーブから多重に割り込みを受け付ける事ができるようにする設定がSFNM(特殊完全ネストモード)です。

　ただし、SFNMが有効な場合、スレーブから多重に割り込みを受け付けることになるため、CPUからEOIを発行する際、スレーブにペンディング中の割り込みがないかチェックする必要があります。

== 仕上げ: main.cとMakefileへ反映する
=== main.cとMakefileへこれまでの変更を盛り込む
以上でCPU側とPIC側それぞれの周辺のソースコードの追加・変更は完了です。

最後にmain.cとMakefileを変更します。

main.cの変更は@<list>{040_main_c}の通りです。

//list[040_main_c][040_intr/main.c][c]{
#include <x86.h>	/* 追加 */
#include <intr.h>	/* 追加 */
#include <pic.h>	/* 追加 */
#include <fb.h>
#include <kbc.h>

void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *fb,
		  void *_fs_start __attribute__ ((unused)))
{
	/* フレームバッファ周りの初期化 */
	fb_init(fb);
	set_fg(255, 255, 255);
	set_bg(0, 70, 250);
	clear_screen();

	/* 変更(ここから) */
	/* CPU周りの初期化 */
	gdt_init();
	intr_init();

	/* 周辺ICの初期化 */
	pic_init();
	kbc_init();

	/* CPUの割り込み有効化 */
	enable_cpu_intr();

	/* 何もせず待つ */
	while (1);
	/* 変更(ここまで) */
}
//}

エコーバック処理をKBCのハンドラへ持っていったので、start_kernel関数は各種の初期化処理を終えると何もせず待つようになりました。

Makefileの変更は@<list>{040_makefile}の通りです。

//list[040_makefile][040_intr/Makefile]{
/* ・・・ 省略 ・・・ */
$(TARGET): main.o fbcon.o fb.o font.o kbc.o x86.o intr.o pic.o handler.o  # 変更
	ld $(LDFLAGS) -o $@ $+
/* ・・・ 省略 ・・・ */
%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<
%.o: %.s				# 追加
	gcc $(CFLAGS) -c -o $@ $<	# 追加
//}

@<list>{040_makefile}の「変更」の箇所で"intr.o"と"pic.o"と"handler.o"を追加しています。

=== haltで待つようにする
これでも良いのですが、「何もせず待つ」の無限ループがあまりエコではないです。

x86 CPUにはhlt(halt)命令があります。この命令を実行すると、命令実行を止めてCPUをHALT状態に遷移させ、割り込み等で復帰します。「何もせず待つ」の無限ループではhlt命令を実行するようにしてみます。

まずは、x86.cへ"cpu_halt"という名前でhlt命令を実行する関数を追加します(@<list>{040_cpu_halt})。

//list[040_cpu_halt][040_intr/x86.c][c]{
/* ・・・ 省略 ・・・ */

inline void enable_cpu_intr(void)
{
	asm volatile ("sti");
}

/* 追加(ここから) */
inline void cpu_halt(void)
{
	asm volatile ("hlt");
}
/* 追加(ここまで) */
/* ・・・ 省略 ・・・ */
//}

併せてx86.hへプロトタイプ宣言を追加します(@<list>{040_x86_h_cpu_halt})。

//list[040_x86_h_cpu_halt][040_intr/include/x86.h(cpu_halt())][c]{
/* ・・・ 省略 ・・・ */
inline void enable_cpu_intr(void);
inline void cpu_halt(void);	/* 追加 */
/* ・・・ 省略 ・・・ */
//}

そして、main.cを修正します(@<list>{040_main_c_cpu_halt})。

//list[040_main_c_cpu_halt][040_intr/main.c(cpu_halt())][c]{
/* ・・・ 省略 ・・・ */
void start_kernel(void *_t __attribute__ ((unused)), struct framebuffer *fb,
		  void *_fs_start __attribute__ ((unused)))
{
/* ・・・ 省略 ・・・ */

	/* haltして待つ */
	while (1)		/* 変更 */
		cpu_halt();	/* 追加 */
}
//}

以上でキーボード入力の割り込み対応の追加・変更は終わりです。

ポーリング版と今回の割り込み版は、見た目はどちらも同じですが、それぞれ実際に試してみると、キー入力を行っていない時はhaltによってCPUが休んでいる様子がCPUファンの音などで確認できるかと思います。(見た目変わらないのでスクリーンショットは省略します。)
