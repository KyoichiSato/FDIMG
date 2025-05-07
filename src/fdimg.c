/* Encoding of this file is a shift-JIS.
MS-DOSで漢字のメッセージを表示したいので、このファイルは shift-JIS で保存されていなければなりません。

フロッピーディスクを読んでD88形式のディスクイメージを作ったり、
D88形式のディスクイメージをフロッピーディスクに書き込みます。

FDCの動作を正確に再現するPC-9801エミュレータは少ないので、エミュレータで動作確認をする場合は注意を要する。
LSI C-86 Ver.3.30 試食版でコンパイルできます。
make でコンパイルできます。

このソースコードは CC0 パブリック・ドメイン提供です。
https://creativecommons.org/publicdomain/zero/1.0/deed.ja

2016年5月18日 作成 佐藤恭一 kyoutan.jpn.org
2016年6月16日 D88イメージの作製が正常に動作した。2HD/2DD/2DそれぞれOK。すんなり動いてうれしい
2016年6月17日 FDへの書込みできた2HD
2016年6月19日 R001 一通り動作正常
2020年2月13日 セクタリードエラー時、リトライしてそれでもだめならエラーメッセージを表示するようにした
2020年2月14日 R002 セクタリードエラーを表示するようにした
2023年7月18日 PC/ATでも動くようにした。終了ステータス（エラーレベル）0で異常終了としていたのを 0で正常終了（標準）に直した
2023年7月24日 FDリードでセクタ長とセクタ数を表示するようにした
2023年11月8日 D88ファイルの情報を表示できるようにした
・済 PC/ATでFDアクセスエラー多発で動作しなかったのは、ワークエリアの初期化をしていなかったせいだった。diskinit()を実行するようにしたら動くようになった
・済 エラーが出るのはドライブ（ハードウェア）の不良だった　書き込み補償0から0xffに変更した PCAT後ろのトラックでリードエラー出るFDCの設定でなにかなかったかな？補正？
・済 リードコマンドではDTLを0xFFにしておく（トラ技SPECIAL No.11 P48）
・済 fdc_getfmtの条件分岐ミス ***優先*** PCATで1.44MB以外が読めない。diskpcatの関数単体テストを行う
・済 セクタリードエラーの表示がおかしい
・済 テストFDの80シリンダフォーマットされていた 80シリンダで何かを読んでいてエラーになっていない ID READエラーで81シリンダ以外改行しない
・済 READIDでリトライしないようにした 2DD読み取り時、最初の2DD/2Hd判別にとても時間がかかるIDリードでリトライしすぎ、結果読めない
・済 READSSEQエラーになるのが遅い。リトライしすぎでは
・済 都度FDCリセットするようにした FMフォーマットをMFMモードでリードするとFDCハングアップすることがある。フォーマット不明時はFMで読んでからMFMで読む。「最新フロッピ・ディスク装置とその応用ノウハウ」 P160
・済 フォーマットで書き込むデータをトラ技の通りに変更する P49 FM:0xFF MFM:0xE5
・済 VIA C3のマザーボードで 500Kbps 360RPMの設定で300Kbpsになってしまうため、500Kbps 360RPM禁止にした ATに98用3.5FDDを接続すると 2DDメディアを2HDとして読み取ってしまう。データは読めている様子。何故なのか調べる
・済 前のトラックとフォーマットが違う時表示するようにする。セクタサイズ、セクタ数、変調方式
・済 書き込みコマンド全部で変数名間違えてデータバッファを壊していたのを直した
・済 変数名間違えてバッファを壊していたのを直した フォーマットコマンドのC-PHASEでエラー出ている 書き込みトラック0で TRACK FORMAT WRITE ERROR ID書けていない。たぶん300RPM/360RPM自動切り替えミス 360RPMになっているんだと思う。
・済 D88ファイルの情報を表示するオプションを追加する
・済 DISKBIOS/FDCアクセスが可能かどうかを調べる関数を作る。別機種のバイナリを実行したときなど使用不可なら異常終了させる。
・済 ファイル名と-Nオプションのときイメージ名を変更する
・済 直した 2DD/2HD切り替えオートにならなくなった
・済 直した retname終端一文字欠ようになった
・済 直した d88idx_write()のカッコつけ忘れ fdimfat file -n イメージ名の変更ができていない
・メディアが入っていないときはメディアが入っていないと表示して終了するようにする。間違ったドライブ番号を指定したときにメディアが入っていないだけなのに読み取りエラーになってわかりにくい。
・説明書きに追加する－異常終了時にフロッピーディスクのアクセスができなくなることがあるが、FDIMGをオプション無しで実行すればFDCが初期化されて多くな場合回復する。だめならコンピュータを再起動。
・AT用の5インチ2D(360KB)ドライブは34ピンがNCだったようで、BIOSで5インチ2Dを選べるPCならDISKCHANGE信号無しの88やX1の2Dドライブを接続して使用できるかも
・セクタリードエラーが出たらリキャリブレートしてからリトライしたほうが良いかも
・ATに98用データセパレータ内蔵FDDを接続する時、DRIVE SELECTを1にして 4PIN HEAD LOAD をGNDに接続しておけばそのまま接続して使えた。
・ATに98用FDDを接続すると2DDは300Kbpsで読めるけど、2HDはID読めてデータ読めない。ドライブ交換して動作確認する。
・2HD1.2MB読めない。2PIN信号出ているが2DD/2HDの切替信号になっている。別のマザーボードで動作を調べる
・トラックに FMとMFMのセクタが混在するときの処理 (D88ではMFM/FM混在できる) 読み書き共
・FDへの書き込みでS2000のイメージ 160/161トラックへ書き込まれない。書き込み処理は161まで行く 2048byte 1セクタ/trackだから？ 直す
・ドライブ番号何番にドライブが存在するか一覧表示するオプションつける

オプションじゃなく引数が1つだけのときは下記動作を行う
ディスクの検査だけを行うオプションを追加する
　セクタサイズ、セクタ数が前のトラックと違うとき表示を残す
　リードエラーがあるときエラー表示を残す

　

2HDのディスクを読むのに7分位かかる
22:15 - 22:25 2DD BASIC
22:26 - 22:35 2HD DOS
22:38 - 22:47 2HD DOS
DISKBASICのフロッピー（1トラック26セクタ）を読めるようにIDをたくさん読むようにしたら、2HD 1枚 10分掛かるようになった。
IDの並びを調べるのが遅い。
1トラックのセクタ数が多いととても遅い(PC-9801のDISK BASIC等)。1トラック分まとめて読めばきっと速い

FDIMG.EXEは同名のプログラムがあるみたい
D88IMG.EXEなら無いみたい

今後
データCRCエラーが出た時、そのトラックにMFMとFMが混在していないか検査する
FDがライトプロテクトならD88に反映させる しないほうがいいか
１トラックまとめて書き込み

ファイルリスト
makefile
fdimg.c		メイン関数
diskbios.h	DISKBIOSとFDC操作を行う関数のヘッダ。diskPC98とdiakPCAT共通
fdimgage.h	D88ディスクイメージの構造体
diskPC98.h
diskPC98.c	PC9801のDISKBIOSを使用する関数
diskPCAT.h
diskPCAT.c	PCATのFDCを操作する関数
timePCAT.h
timePCAT.c	PCATのタイマーとDMAを使用する関数
d88info.h
d88info.c	D88イメージの情報を表示する関数
diskinfo.h
diskinfo.c	FDの情報を表示する関数
fdseek.c	FDDのシークを繰り返すコマンド
test.c		FDD操作関数の動作テスト用
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fdimage.h"
#include "diskbios.h"
#include "d88info.h"
#include "diskinfo.h"

/* d88 */
struct st_d88_header D88IDX;
struct st_d88_sect_header D88SECT;
FILE *D88FILE;

#define RELEASE "R003"
#define SIGNATURE "2023 kyoutan.jpn.org"
#define DEFAULTEXT "D88" /*デフォルト拡張子*/
#define TRYCOUNT 5		 /*セクタリード時リトライカウント*/
#define TRYFCOUNT 10	 /*フォーマット判別時別のトラックを読むリトライカウント*/
#define CY2D 41			 /*2D のシリンダ数 普通は40です （最大シリンダ番号ではない）*/
#define CY2DD 82		 /*2DDのシリンダ数 普通は80です*/
#define CY2HD 82		 /*2HDのシリンダ数 PC98等は77、PC/ATは80です*/
#define SECTMAX 32		 /*最大セクタ数 1セクタ256バイトの時26なので余裕を見て32くらい*/
#define BUFFSIZE (16 * 1024)
unsigned char BUFF[BUFFSIZE];
unsigned char IDBUFF[SECTMAX * 4];
char FILENAME[256];
char D88NAME[17];
unsigned char DRIVE;	/*ドライブ番号*/
unsigned char CYLINDER; /*最終シリンダ*/
/* MODE */
/* 0b0******* */
/*    |||++++- 0:READ / 1:WRITE / 2:D88INFO / 3:DISKINFO */
/*    |++----- 0:2D / 1:2DD / 2:2HD */
/*    +------- 1:全トラックの情報を表示 */
unsigned char MODE;
#define OPREAD 0x00
#define OPWRITE 0x01
#define OPD88INFO 0x02
#define OPDISKINFO 0x03
#define OP2D 0x00
#define OP2DD 0x10
#define OP2HD 0x20
#define OPAUTO 0x30
#define OPDETAIL 0x40

void usage(void)
{
#ifdef JPN
	printf("FLOPPY DISK IMAGE FILE READER / WRITER");
	printf("  %s    %s\r\n", RELEASE, SIGNATURE);
	puts("");
	puts("フロッピーディスクを読んでD88形式のディスクイメージを作ったり、D88形式のディスクイメージをフロッピーディスクに書き込みます。");
	puts("フロッピーディスクドライブの指定には、");
	puts("「ドライブレター」ではなく「ドライブ番号」 (0-3) を使用します。");
	puts("");
	puts("使い方");
	puts("FDIMG <読み取り元> [<書き込み先>] [-2D|-2DD|-2HD][-Cxx][-A][-Nname]");
	puts("  -2D / -2DD / -2HD メディア指定 省略時は 2HD/2DD 自動判別");
	puts("  -C 読み取りシリンダ数 省略時 2D:41 2HD/2DD:82");
	puts("  -N D88ヘッダのイメージ名指定（16文字まで） 省略時はファイルネームと同一");
	puts("  -A 全トラックの情報を表示 省略時はエラーの時とセクタ長が変化した時に表示");
	puts("書き込み先を指定しない場合は情報表示のみ実行");
	puts("");
	puts("ドライブ0のフロッピーの読み出し");
	puts("    FDIMG 0 filename.d88");
	puts("ドライブ0のフロッピーへ書き出し");
	puts("    FDIMG filename.d88 0");
	puts("イメージ名の変更");
	puts("    FDIMG filename.d88 -Nimagename");
	puts("[ctrl]-[c]で中断した後など、FDが使用できなくなった場合は、FDIMGをオプション無しで実行してハードウェアを初期化してください。");
	return;
#else
	printf("FLOPPY DISK IMAGE FILE READER / WRITER");
	printf("  %s    %s\r\n", RELEASE, SIGNATURE);
	puts("Create a disk image of the \"D88 format\" from the floppy disk, and writing from a disk image to a floppy disk of \"D88 format\".");
	puts("To specify the drive using the \"drive number\". Caution is not a \"drive letter\".");
	puts("");
	puts("FDIMG <src> <dst>  [-2D|-2DD|-2HD][-Cxx][-A][-Nname]");
	puts("  -2D / -2DD / -2HD Specifies the media. If omitted, 2HD/2DD is automatically detected.");
	puts("  -C Specifies the number of cylinders to be read. Default 2D:41 2HD/2DD:82");
	puts("  -N Specifies the image name for the D88 header (up to 16 characters) If omitted, it will be the same as the filename.");
	puts("  -A Displays information on all tracks. If omitted, information is displayed when there is an error or when the sector length changes.");
	puts("If no write destination is specified, only information display is executed.");
	puts("");
	puts("READ FD");
	puts("    FDIMG 0 filename.d88");
	puts("WRITE FD");
	puts("    FDIMG filename.d88 0");
	puts("イメージ名の変更");
	puts("    FDIMG filename.d88 -Nimagename");
	puts("If FD is no longer available, such as after an interruption with [ctrl]-[c], run FDIMG without options to initialize the hardware.");
	return;
#endif
}

/* ドライブ番号かどうか調べる
ドライブ番号なら、ドライブ番号セットする。違ったらファイル名をセットする
戻り値 0:ドライブ番号じゃなかった */
int setdrive(char str[])
{
	if (1 == strlen(str))
	{
		/*長さが1文字だったからドライブ番号かも*/
		switch (str[0])
		{
		case '0':
			DRIVE = 0;
			return TRUE;
		case '1':
			DRIVE = 1;
			return TRUE;
		case '2':
			DRIVE = 2;
			return TRUE;
		case '3':
			DRIVE = 3;
			return TRUE;
		}
	}

	/*一文字目が '-'なら何もしない*/
	if ('-' == str[0])
		return FALSE;

	/* ドライブ番号じゃないのでファイル名をセット */
	strncpy(FILENAME, str, sizeof FILENAME);
	FILENAME[(sizeof FILENAME) - 1] = '\0';
	return FALSE;
}

/* コマンドラインオプションを読んでグローバル変数をセットする
戻り値 0:異常終了 */
int option(int argc, char *argv[])
{
	unsigned char pos;
	/* argv[0]:実行ファイル名 argv[1]:1つ目のオプション ～ */
	if (2 > argc)
	{
		/*引数がない時*/
		return FALSE;
	}

	DRIVE = 0xff;
	CYLINDER = 0;
	MODE = OPAUTO;
	memset(FILENAME, '\0', sizeof FILENAME);
	memset(D88NAME, '\0', sizeof D88NAME);

	for (pos = 1; pos < argc; pos++)
	{
		/*
		puts(argv[pos]);
		*/
		if (0 == strnicmp(argv[pos], "-2HD", 4)) /* 大文字小文字を区別しない */
		{
			/*等しい時*/
			MODE &= 0xcf; /*0b1100 1111 bit4,5クリア*/
			MODE |= OP2HD;
		}

		if (0 == strnicmp(argv[pos], "-2D", 3))
		{
			MODE &= 0xcf;
			MODE |= OP2D;
		}

		if (0 == strnicmp(argv[pos], "-2DD", 4))
		{
			MODE &= 0xcf;
			MODE |= OP2DD;
		}

		if (0 == strnicmp(argv[pos], "-A", 2)) /*詳細表示*/
		{
			MODE |= OPDETAIL;
		}

		if (0 == strnicmp(argv[pos], "-C", 2)) /*読み取りシリンダ数*/
		{
			if (2 < strlen(argv[pos]))
			{
				CYLINDER = atoi(&argv[pos][2]); /*2文字目より後ろを数値に変換*/
				if (82 < CYLINDER)
					CYLINDER = 82;
			}
		}

		if (0 == strnicmp(argv[pos], "-N", 2)) /*D88ヘッダ内のイメージ名*/
		{
			if (2 < strlen(argv[pos]))
			{
				memset(D88NAME, '\0', sizeof D88NAME);				   /*複数回 -Nオプションが付いたときのために毎回クリア*/
				strncpy(D88NAME, &argv[pos][2], (sizeof D88NAME) - 1); /*2文字目より後ろをコピー*/
			}
		}

		if (0 == strnicmp(argv[pos], "-H", 2)) /*ヘルプ*/
		{
			return FALSE;
		}
	}

	MODE &= 0xf0; /*0b1111 0000 下位4ビットクリア*/
	if (setdrive(argv[1]))
	{
		MODE |= OPREAD; /*1つ目の引数がドライブ番号だったらとりあえずFD読み取り*/
	}
	else
	{
		MODE |= OPWRITE;
	}

	if (2 < argc)
	{
		setdrive(argv[2]); /*2つ目の引数はドライブ番号かファイル名かオプション*/
	}

	if (0xff != DRIVE)
	{
		if (0 != strlen(FILENAME))
		{
			/*ドライブ番号とファイルネームがセットされている
			READかWRITEモード*/
			return TRUE;
		}
	}

	/*ドライブ番号かファイルネームどちらかしかセットされていない*/
	MODE &= 0xf0; /*下位4ビットクリア*/

	if (0xff != DRIVE)
	{
		/*ドライブ番号がセットされている
		FD読み取りテスト（ディスクイメージは作成しない）*/
		MODE |= OPDISKINFO;
		return TRUE;
	}

	if (0 != strlen(FILENAME))
	{
		/*ファイル名がセットされている
		D88ファイル情報表示*/
		MODE |= OPD88INFO;
		return TRUE;
	}

	return FALSE;
}

/*ファイル名に拡張子が無かったら追加する*/
int addext(char filename[], char exte[])
{
	unsigned int pos, len;

	/*後端に移動*/
	pos = strlen(filename) - 1;
	len = strlen(exte);
	if ((5 < len) || (pos > (0xfe - len)))
		return FALSE; /*文字数が多すぎ*/

	/*拡張子があるか後端から調べる*/
	for (; 0 != pos; pos--)
	{
		if ('.' == filename[pos])
			return TRUE; /*拡張子があった*/
		if ('\\' == filename[pos])
			break; /*拡張子が無い*/
	}
	/*文字列連結*/
	strncat(filename, ".", 1);
	strncat(filename, exte, len);

	return TRUE;
}

/* ファイル名からパスと拡張子を取り除く
戻り値 0:ファイル名がなかった */
int retname(char out[], char in[], unsigned char size)
{
	unsigned int pos;
	unsigned char top, end, len;

	memset(out, '\0', size); /*出力をクリア*/

	if (0 == (end = strlen(in)))
	{
		/*入力が空の時*/
		return FALSE;
	}
	--end; /*後端は文字数-1*/
	/*012345*/
	if (size <= (end + 1))
	{
		printf("retname:size%d end%d", size, end);
		return FALSE; /*文字数が多すぎ*/
	}
	/*ファイル名の先頭を調べる*/
	top = 0;
	pos = end;
	while (TRUE)
	{
		if ('\\' == in[pos])
		{
			if (pos != end)
				top = pos + 1;
			break;
		}
		if (0 == pos--)
			break;
	}
	/*拡張子を除いたファイル名の終端を調べる*/
	for (pos = end; 0 != pos; pos--) /*（endから1まで）*/
	{
		if ('\\' == in[pos])
		{
			/*拡張子が無い*/
			break;
		}
		if ('.' == in[pos])
		{
			/*拡張子があった*/
			end = pos - 1;
			break;
		}
	}

	if ((top < end) || (top == end)) /* topとend が同じなのは入力が1文字の時 */
	{
		/* 0123.45 */
		/*  | |   3-1=2 len=3 len=end-top+1  */
		len = end - top + 1; /*endとtopが同じ時は長さ1文字*/
	}
	else
	{
		printf("retname:top%d end%d", top, end);
		return FALSE;
	}
	/*出力用配列に文字列コピー*/
	/*printf("top %d end %d len %d size %d\n", top, end, len, size);*/
	/*if((len + 1) > size) return FALSE;*/ /*出力先のサイズが足りない*/
	if ((len + 1) > size)
		end = top + size - 1;	/*出力先のサイズが足りない*/
	memcpy(out, &in[top], len); /*終端文字無関係にコピーするのでstrncopyは使えない*/

	return TRUE;
}

void printop(void)
{
	unsigned char temp;

	printf("FILENAME : %s\r\n", FILENAME);
	printf("D88NAME : %s\r\n", D88NAME);
	printf("DRIVE : %d\r\n", DRIVE);
	printf("CYLINDER : %d\r\n", CYLINDER);

	temp = 0x30 & MODE; /*0b0011 0000 bit4,5だけ取り出す*/
	if (OP2D == temp)
		puts("-2D");
	if (OP2DD == temp)
		puts("-2DD");
	if (OP2HD == temp)
		puts("-2HD");
	if (OPAUTO == temp)
		puts("2DD/2HD");
	temp = OPDETAIL & MODE;
	if (OPDETAIL == temp)
		puts("-A");
	temp = 0x0f & MODE;
	if (OPREAD == temp)
		puts("READ");
	if (OPWRITE == temp)
		puts("WRITE");
	if (OPDISKINFO == temp)
		puts("DISKINFO");
	if (OPD88INFO == temp)
		puts("D88INFO");

	puts("");
}

/*D88のヘッダ部分を書き込む*/
int d88idx_write(void)
{
	unsigned char track;
	unsigned int wcount;

	/*パディングの影響が出ないようにfwriteを並べているけれど、不要かも*/
	/*fwriteの戻り値は書き込んだバイト数じゃなくて　size のデータを書き込んだ個数*/
	wcount = 0;
	wcount += fwrite(D88IDX.name, 17, 1, D88FILE);
	wcount += fwrite(D88IDX.reserve, 9, 1, D88FILE);
	wcount += fwrite(&D88IDX.write_protect, 1, 1, D88FILE);
	wcount += fwrite(&D88IDX.type, 1, 1, D88FILE);
	wcount += fwrite(&D88IDX.size, 4, 1, D88FILE);
	for (track = 0; 164 != track; track++) /*0-163*/
	{
		wcount += fwrite(&D88IDX.track_offset[track], 4, 1, D88FILE);
	}
	if ((5 + 164) == wcount)
		return TRUE; /* 5 + 164回書けていれば正常 */

	return FALSE; /*異常終了*/
}

/*D88の1セクタ分書き込む*/
int d88sect_write(void)
{
	unsigned int wcount;

	wcount = 0;
	wcount += fwrite(&D88SECT.c, 1, 1, D88FILE);
	wcount += fwrite(&D88SECT.h, 1, 1, D88FILE);
	wcount += fwrite(&D88SECT.r, 1, 1, D88FILE);
	wcount += fwrite(&D88SECT.n, 1, 1, D88FILE);
	wcount += fwrite(&D88SECT.sect, 2, 1, D88FILE);
	wcount += fwrite(&D88SECT.density, 1, 1, D88FILE);
	wcount += fwrite(&D88SECT.deleted, 1, 1, D88FILE);
	wcount += fwrite(&D88SECT.status, 1, 1, D88FILE);
	wcount += fwrite(D88SECT.reserve, 5, 1, D88FILE);
	wcount += fwrite(&D88SECT.size, 2, 1, D88FILE);
	wcount += fwrite(BUFF, D88SECT.size, 1, D88FILE);

	if (11 == wcount)
		return TRUE; /*正常終了*/
	return FALSE;	 /*異常終了*/
}

/*D88のヘッダ部分を読み込む*/
int d88idx_read(void)
{
	unsigned char track;
	unsigned int rcount;

	/*fwriteの戻り値は書き込んだバイト数じゃなくて　size のデータを書き込んだ個数*/
	rcount = 0;
	rcount += fread(D88IDX.name, 17, 1, D88FILE);
	rcount += fread(D88IDX.reserve, 9, 1, D88FILE);
	rcount += fread(&D88IDX.write_protect, 1, 1, D88FILE);
	rcount += fread(&D88IDX.type, 1, 1, D88FILE);
	rcount += fread(&D88IDX.size, 4, 1, D88FILE);
	for (track = 0; 164 != track; track++) /*0-163*/
	{
		rcount += fread(&D88IDX.track_offset[track], 4, 1, D88FILE);
	}
	D88IDX.name[16] = 0; /*念のため終端文字を書いておく*/
	if ((5 + 164) == rcount)
		return TRUE; /* 5 + 164回読めていれば正常 */

	return FALSE; /*異常終了*/
}

/*D88の1セクタ分のヘッダとデータを読み込む*/
int d88sect_read(void)
{
	unsigned int rcount;

	rcount = 0;
	rcount += fread(&D88SECT.c, 1, 1, D88FILE);
	rcount += fread(&D88SECT.h, 1, 1, D88FILE);
	rcount += fread(&D88SECT.r, 1, 1, D88FILE);
	rcount += fread(&D88SECT.n, 1, 1, D88FILE);
	rcount += fread(&D88SECT.sect, 2, 1, D88FILE);
	rcount += fread(&D88SECT.density, 1, 1, D88FILE);
	rcount += fread(&D88SECT.deleted, 1, 1, D88FILE);
	rcount += fread(&D88SECT.status, 1, 1, D88FILE);
	rcount += fread(D88SECT.reserve, 5, 1, D88FILE);
	rcount += fread(&D88SECT.size, 2, 1, D88FILE);
	rcount += fread(BUFF, D88SECT.size, 1, D88FILE);

	if (11 == rcount)
		return TRUE; /*正常終了*/
	return FALSE;	 /*異常終了*/
}

/*セクタシーケンスを読み込む
戻り値 0:異常終了 それ以外:セクタ数*/
int sectseq(unsigned char device, unsigned char mod, unsigned char h)
{
	unsigned char sechead, seccount, count, startpos;

	/*READDIAG で先頭セクタの番号を調べる*/
	if (!secthead(device, mod, h, BUFF))
	{
		/*printf("SectheadERR");*/
		return FALSE; /*エラー*/
	}
	sechead = BUFF[0]; /*先頭のセクタ番号*/

	/*READID で1トラック分のセクタのIDを調べる*/
	for (count = 0; count != (SECTMAX * 2); count++)
	{
		if (!readid(device, mod, h, &BUFF[count * 4]))
		{
			/*printf("ReadidERR");*/
			return FALSE;
		}
	}

	/*1トラックに何セクタあるか数える*/
	for (seccount = 1; seccount != (SECTMAX - 1); seccount++)
	{
		if (BUFF[2] /*R*/ == BUFF[(seccount * 4) + 2])
			break;
	}

	/*先頭から順にバッファを整頓*/
	/*先頭位置を調べる*/
	for (count = 0; count != (SECTMAX - 1); count++)
	{
		if (sechead == BUFF[(count * 4) + 2])
			break;
	}
	startpos = count * 4;

	/*コピー*/
	memcpy(IDBUFF, &BUFF[startpos], seccount * 4);

	/* IDが正常か検査する */
	for (count = 0; count != seccount; count++)
	{
		/*if(           90 < IDBUFF[count * 4 + 0]) return FALSE;*/ /*C 極端に大きかったらおかしい */
		if (1 < IDBUFF[count * 4 + 1])
			return FALSE; /*H ヘッドは0か1 */
		if ((SECTMAX - 1) < IDBUFF[count * 4 + 2])
			return FALSE; /*R 極端に大きかったらおかしい 本当は0でもおかしい */
		if (5 < IDBUFF[count * 4 + 3])
			return FALSE; /*H セクタ長コードは、普通 0 - 3 */
	}

	/*正常終了*/
	return seccount;
}

/*フロッピーから読み取ってD88形式で書き出す*/
/*device:0-3*/
int readmode(unsigned char device)
{
	unsigned char count, try;
	unsigned char mod, track, tr_max;
	unsigned char c, h, sectcount, previousN;
	unsigned char detail;	 /* TRUE:詳細表示 */
	unsigned char mediatype; /* 0:2D 1:2DD 2:2HD 3:auto */

	previousN = 0;
	mediatype = 0x30 & MODE;

	detail = TRUE;
	if (0 == (OPDETAIL & MODE))
		detail = FALSE;

	/*FDを操作できるかチェックする。
	PC98用をPCATで実行したり、PCAT用をPC98で実行した時に異常終了させる。*/
	if (!fdcheck())
	{
		puts("\r\nERROR : Could not access the floppy disk.");
		return FALSE;
	}

	printf("Reading floppy disk from drive %d...        ", device);

	memset(&D88IDX, 0, sizeof D88IDX); /*D88ヘッダをゼロクリア*/
	/*printf("元 %s\r\n", FILENAME);*/
	addext(FILENAME, DEFAULTEXT); /*ファイル名に拡張子が無ければ拡張子追加*/
	/*printf("拡張子追加 %s\r\n", FILENAME);*/

	/* D88イメージの名前をセット */
	if (0 == strlen(D88NAME))
	{
		/*オプションでイメージ名がセットされていない時はファイル名をセット*/
		retname(D88IDX.name, FILENAME, sizeof D88IDX.name);
		/*printf("filename %s\r\n", D88IDX.name);*/
	}
	else
	{
		strncpy(D88IDX.name, D88NAME, sizeof D88IDX.name); /*イメージ名をセット*/
														   /*printf("d88name %s\r\n", D88IDX.name);*/
	}

	diskinit(device);
	drvmode(device, MODE2DD); /*2Dじゃなく2DDモード*/
	if (!track00(FD2HD | device))
	{
		puts("\r\nRECALIBLATE ERROR");
		return FALSE;
	}

	/* ディスクのタイプを調べる FM/MFM 2DD/2HD */
	/* IDを読んで2DDなのか2HDなのか調べる */
	count = 0;
	while (TRUE)
	{
		printf("\b\b\b\b\b\b\bC%2d H%2d", count / 2, count & 1);
		if (mod = sectsense(device, count / 2, count & 1, mediatype)) /*シリンダ0ヘッド0から順番に*/
		{
			break; /*正常終了*/
		}
		/*異常なら繰り返し別のトラックを読む*/
		if (TRYFCOUNT == count++) /*リトライしても読めない*/
		{
			puts(" Could not determine disk format.");
			return FALSE;
		}
	}

	printf("\b\b\b\b\b\b\b       \b\b\b\b\b\b\b"); /* C H 消去 */

	/* 処理するトラック数をセット */
	if (FD2HD == (FD2HD & mod))
	{
		/*FDをsectsenseで読んでみて2HDだったとき*/
		printf("2HD ");
		D88IDX.type = D88_2HD;
		if (0 == CYLINDER)
		{
			/*オプションでシリンダ数を指定していない時*/
			tr_max = CY2HD * 2; /*トラック数*/
		}
		else
		{
			tr_max = CYLINDER * 2;
		}
	}
	else
	{
		/*FDをsectsenseで読んでみて2DDか2Dだったとき*/
		if (OP2D == mediatype)
		{
			/*オプションで2D指定なら 2D*/
			printf("2D  ");
			D88IDX.type = D88_2D;
			drvmode(device, MODE2D); /*2Dモード*/
			if ((0 == CYLINDER) || (CY2D < CYLINDER))
			{
				/*オプションでシリンダ数を指定していない時*/
				tr_max = CY2D * 2; /*トラック数*/
			}
			else
			{
				tr_max = CYLINDER * 2;
			}
		}
		else
		{
			printf("2DD ");
			D88IDX.type = D88_2DD;
			tr_max = CY2DD * 2;
			if ((0 == CYLINDER) || (CY2DD < CYLINDER))
			{
				/*オプションでシリンダ数を指定していない時*/
				tr_max = CY2DD * 2; /*トラック数*/
			}
			else
			{
				tr_max = CYLINDER * 2;
			}
		}
	}
	device = device | (FD2HD & mod); /*デバイス番号に2HD/2DDの指定を追加*/
	mod = FDMFM & mod;				 /*modの変調方式だけ残す*/

	D88FILE = fopen(FILENAME, "wb"); /*wb:新しいバイナリファイルを作り、出力モードでオープンします。すでに同じ名前のファイルがある場合はその内容は捨てられます*/
	if (NULL == D88FILE)
	{
		puts("\nCannot create D88 file.");
		fclose(D88FILE);
		return FALSE;
	}

	if (!d88idx_write()) /*D88ヘッダ仮書き込み*/
	{
		/*エラー*/
		fclose(D88FILE);
		puts("\nWRITE ERROR ");
		return FALSE;
	}
	D88IDX.size = D88_IDXSIZE; /*まだトラックのデータがないので、仮にインデックスのサイズをイメージファイルのサイズにセット*/

	puts("\n  C  H  R  N ");
	track00(device);
	for (track = 0; track < tr_max; track++)
	{
		/* シリンダ番号=track/2 ヘッド番号=track & 0x01*/
		c = track / 2;
		h = track & 0x01;
		printf(" %2d %2d  -  - READ SECSEQ                     %3d / %3d track \r", c, h, track, tr_max - 1);
		mod = modsense(device, c, h); /*トラックごとに変調方式が違うかもしれないので毎回調べる*/
		/*modsenseでseekもする*/
		if (sectcount = sectseq(device, mod, track & 0x01)) /*セクタシーケンスを読み込む*/
		{													/*ヘッド番号は track & 0x01 (trackの下位1ビット)*/
			/*PCAT ID リードエラーのときとても遅い*/
			D88IDX.track_offset[track] = ftell(D88FILE); /*トラックオフセット保存*/
			/*セクタを読む*/
			for (count = 0; count != sectcount; count++)
			{
#define ID_C IDBUFF[count * 4 + 0]
#define ID_H IDBUFF[count * 4 + 1]
#define ID_R IDBUFF[count * 4 + 2]
#define ID_N IDBUFF[count * 4 + 3]

				printf(" %2d %2d %2d %2d %4d byte  %2d secter/track ",
					   ID_C, ID_H, ID_R, ID_N, sectlength(ID_N), /*FMとMFMセクタ長コード同じでいいんだっけ？*/
					   sectcount);
				if (FDMFM == mod)
					printf("MFM\r");
				else
					printf("FM\r");

				memset(&D88SECT, 0, sizeof D88SECT); /*セクタヘッダゼロクリア*/

				for (try = 0; TRYCOUNT != try; try++)
				{
					/*D88のエラーコードはPC98のBIOSのエラーコード*/
					D88SECT.status = readdata(device, mod,
											  ID_C, ID_H, ID_R, ID_N,
											  BUFF);

					if (0 == D88SECT.status)
						break; /*エラーがなかった*/
				}

				if (0 != D88SECT.status)
				{
					/* リードエラーがあったら表示する */
					printf("                                                                           \r"); /* 現在行消去 */
					printf(" %2d %2d %2d %2d ", ID_C, ID_H, ID_R, ID_N);
					printf("%-31s ", errmsg(D88SECT.status));
					printf("%3d / %3d track \r", track, tr_max - 1);
					printf("\n");		  /* エラーがあったとき改行してエラーメッセージ残す */
					/*previousN = 0xff;*/ /*エラーがあった次の行はID表示したいのでありえない値にしておく*/
				}
				else
				{
					/* エラーがない時、セクタ長が前回と違ったら改行する */
					if (previousN != ID_N)
					{
						printf("\n");
					}
					else
					{
						if (detail)
						{
							/*オプションで詳細表示指定なら改行*/
							printf("\n");
						}
					}
					/*Nを保存する*/
					previousN = ID_N;
				}

				/**/
				D88SECT.c = ID_C;
				D88SECT.h = ID_H;
				D88SECT.r = ID_R;
				D88SECT.n = ID_N;
				D88SECT.sect = sectcount; /* このトラック内のセクタ数 */

				/*PC98のBIOSとD88ではFMとMFMの符号が反転しているみたい*/
				if (FDMFM & mod)
					D88SECT.density = D88_MFM;
				else
					D88SECT.density = D88_FM;

				/* DELETED DATA かどうか */
				if (D88_DDAM == D88SECT.status)
					D88SECT.deleted = D88_DDAM;
				else
					D88SECT.deleted = D88_DAM;

				D88SECT.size = sectlength(D88SECT.n); /* セクタのサイズ (byte) */

				/*1セクタずつ書くと遅い。1トラック分バッファを確保してまとめて書いたら速いよな*/
				if (!d88sect_write()) /*1セクタ分ファイルに書き出す*/
				{
					/*エラー*/
					fclose(D88FILE);
					printf("\nWRITE ERROR t %d \n", track);
					return FALSE;
				}
				D88IDX.size += D88_SECHSIZE;
				D88IDX.size += D88SECT.size;
			}
		}
		else
		{
			/*IDが見つからない。このトラックはスキップ*/
			printf(" %2d %2d  -  -    ID READ ERROR \r\n",
				   track / 2,	  /* C */
				   track & 0x01); /* H */

			D88IDX.track_offset[track] = 0; /*トラックオフセット保存（空）*/
		}
	}

	/*全トラック終了*/
	D88IDX.size = ftell(D88FILE);		  /*イメージ全体のサイズ （連結した場合次のイメージへのオフセット）*/
	if (0 != fseek(D88FILE, 0, SEEK_SET)) /*ファイル先頭にシーク*/
	{
		/*seekでエラー （0以外はエラー）*/
		fclose(D88FILE);
		puts("\nD88 FILE WRITE ERROR");
		return FALSE;
	}

	if (!d88idx_write()) /*D88ヘッダ本書き込み*/
	{
		/*エラー*/
		fclose(D88FILE);
		puts("\nD88IDX WRITE ERROR ");
		return FALSE;
	}

	fclose(D88FILE);
	return TRUE;
}

/*ファイルから読んでフロッピーへ書き込む*/
int writemode(void)
{
	unsigned char device, track, tr_max, mod, sectcount, count, data;
	unsigned char detail;

	detail = TRUE;
	if (0 == (OPDETAIL & MODE))
		detail = FALSE;

	/*FDを操作できるかチェックする。
	PC98用をPCATで実行したり、PCAT用をPC98で実行した時に異常終了させる。*/
	if (!fdcheck())
	{
		puts("");
		puts("ERROR : Could not access the floppy disk.");
		return FALSE;
	}

	addext(FILENAME, DEFAULTEXT); /*ファイル名に拡張子が無ければ拡張子追加*/
	/*ファイルを開く*/
	D88FILE = fopen(FILENAME, "rb");
	if (NULL == D88FILE)
	{
		printf("File open error : %s\r\n", FILENAME);
		return FALSE;
	}

	memset(&D88IDX, 0, sizeof D88IDX);	 /*ゼロクリア*/
	memset(&D88SECT, 0, sizeof D88SECT); /*ゼロクリア*/

	if (!d88idx_read())
	{
		fclose(D88FILE);
		puts("\nD88IDX READ ERROR ");
		return FALSE;
	}

	printf("NAME : %s \n", D88IDX.name);
	printf("Writing to floppy disk in drive %d...  ", DRIVE);

	switch (D88IDX.type)
	{
	case D88_2HD:
		device = DRIVE | FD2HD;
		puts("2HD ");
		break;
	case D88_2DD:
		device = DRIVE | FD2DD;
		drvmode(device, MODE2DD); /*2DDモード*/
		puts("2DD ");
		break;
	case D88_2D:
		device = DRIVE | FD2DD;
		drvmode(device, MODE2D); /*2Dモード*/
		puts("2D  ");
		break;
	}

	/*何トラックまであるか終端から調べる tr_max:添字じゃなくトラック数 0-164*/
	for (tr_max = 163; 0 < tr_max; tr_max--)
	{
		if (0 != D88IDX.track_offset[tr_max])
			break;
	}

	if (0 == tr_max)
	{
		if (0 == D88IDX.track_offset[0])
		{
			puts("Valid track not found.");
			return FALSE;
		}
	}
	tr_max++;

	diskinit(device);
	if (!track00(FD2HD | device))
	{
		return FALSE;
	}

	for (track = 0; tr_max > track; track++)
	{
		if (0 != D88IDX.track_offset[track]) /*トラックのデータが無ければスキップ*/
		{
			printf("                          %3d / %3d track \r", track, tr_max - 1);
			if (0 != fseek(D88FILE, D88IDX.track_offset[track], SEEK_SET)) /*トラックの先頭にシーク*/
			{
				/*fseekでエラー (0位外はエラー)*/
				fclose(D88FILE);
				puts("\nD88 FILE READ ERROR (SEEK 1) ");
				return FALSE;
			}
			/*フォーマットの準備でセクタの並びを調べる*/
			for (count = 0; SECTMAX > count; count++)
			{
#define ID_C IDBUFF[count * 4 + 0]
#define ID_H IDBUFF[count * 4 + 1]
#define ID_R IDBUFF[count * 4 + 2]
#define ID_N IDBUFF[count * 4 + 3]
				if (!d88sect_read())
				{
					puts("\nD88SECT READ ERROR (SECT SEQ) ");
					return FALSE;
				}
				ID_C = D88SECT.c;
				ID_H = D88SECT.h;
				ID_R = D88SECT.r;
				ID_N = D88SECT.n;

				if (D88SECT.sect == (count + 1))
					break; /* 1トラック分終了 */
			}
			/*printf("count:%d D88SECT.sect:%d\n", count, D88SECT.sect);*/

			if (D88_MFM == D88SECT.density)
			{
				mod = FDMFM;
				/* フォーマットで書き込むデータ FM:0xFF MFM:0xE5
				トランジスタ技術 SPECIAL No.11 P49 */
				data = 0x5e;
			}
			else
			{
				mod = FDFM;
				data = 0xff;
			}

			sectcount = D88SECT.sect;
			/*フォーマットでセクタ数とセクタサイズ表示したい*/
			printf(" FORMAT  %4dbyte %2dsect \r", D88SECT.size, D88SECT.sect);
			/*トラックフォーマット trackfmt()でシークもする*/
			if (!trackfmt(device, mod, track / 2, track & 0x01, D88SECT.n, sectcount,
						  data, IDBUFF))
			{
				/*エラー*/
				fclose(D88FILE);
				puts("\nTRACK FORMAT WRITE ERROR ");
				return FALSE;
			}
			/*詳細表示指定の時改行する*/
			if (detail)
				puts("");

			printf(" DATA WRITE              \r");
			/*セクタのデータを書き込む*/
			if (0 != fseek(D88FILE, D88IDX.track_offset[track], SEEK_SET)) /*トラックの先頭にシーク*/
			{
				/*seekでエラー (0位外はエラー)*/
				fclose(D88FILE);
				puts("\nD88 FILE READ ERROR (SEEK 2)");
				return FALSE;
			}
			for (count = 0; sectcount > count; count++)
			{
				if (!d88sect_read())
				{
					puts("\nD88SECT READ ERROR (DATA) ");
					return FALSE;
				}
				if (D88_DAM == D88SECT.deleted)
				{
					/*DAM*/
					if (!writedata(device, mod, D88SECT.c, D88SECT.h, D88SECT.r, D88SECT.n, BUFF))
					{
						/*エラー*/
						fclose(D88FILE);
						puts("\nID READ ERROR (DAM) ");
						return FALSE;
					}
				}
				else
				{
					/*DDAM*/
					if (!writedeleted(device, mod, D88SECT.c, D88SECT.h, D88SECT.r, D88SECT.n, BUFF))
					{
						/*エラー*/
						fclose(D88FILE);
						puts("\nID READ ERROR (DDAM) ");
						return FALSE;
					}
				}
			}
		}
	}

	return TRUE;
}

int main(int argc, char *argv[])
{
	unsigned char function;

	if (!option(argc, argv)) /*コマンドラインオプションを読んでグローバル変数をセット*/
	{
		usage();
		diskexit();
		return EXIT_FAILURE;
	}
	/*
		printop();
	*/
	function = 0x0f & MODE; /*下位4ビットだけにする*/
	switch (function)
	{
	case OPWRITE:
		if (!writemode()) /*D88ファイルから読んでフロッピーへ書き込むモード*/
		{
			diskexit();
			return EXIT_FAILURE;
		}
		break;

	case OPREAD:
		if (!readmode(DRIVE)) /*フロッピーから読んでD88ファイルに書き込むモード*/
		{
			diskexit();
			return EXIT_FAILURE;
		}
		break;

	case OPDISKINFO:
		if (!diskinfo(DRIVE, MODE)) /*フローッピーディスクの読み取りテスト*/
		{
			diskexit();
			return EXIT_FAILURE;
		}
		break;

	case OPD88INFO:
		if (!d88info(FILENAME, MODE, D88NAME)) /*D88イメージの情報表示*/
		{
			diskexit();
			return EXIT_FAILURE;
		}
		break;
	}

	diskexit();
	return EXIT_SUCCESS;
}