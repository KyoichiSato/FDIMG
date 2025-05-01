/* Encoding of this file is a shift-JIS.
フロッピーディスクを読んでD88形式のディスクイメージを作ったり、
D88形式のディスクイメージをフロッピーディスクに書き込みます。

FDCの動作を正確に再現するPC-9801エミュレータは少ないので、エミュレータで動作確認をする場合は注意を要する。
LSI C-86 Ver.3.30 試食版でコンパイルできます。
make でコンパイルできます。
lcc -o fdimg.exe fdimg.c diskbios.c のようにしてコンパイルしてもよい

MS-DOSで漢字のメッセージを表示したいので、このファイルは shift-JIS で保存されていなければなりません。

このソースコードは CC0 パブリック・ドメイン提供です。
https://creativecommons.org/publicdomain/zero/1.0/deed.ja

2016年5月18日 佐藤恭一 kyoutan.jpn.org
2020年2月13日 セクタリードエラー時、リトライしてそれでもだめならエラーメッセージを表示するようにした
2016年6月16日 D88イメージの作製が正常に動作した。2HD/2DD/2DそれぞれOK。すんなり動いてうれしい
2016年6月17日 FDへの書込みできた2HD
2016年6月19日 一通り動作正常 R001

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
*/

/*#include <stdlib.h>*/
#include <stdio.h>
#include <string.h>
#include "diskbios.h"
#include "fdimage.h"

/*自分の決まり グローバル変数と定数は大文字にする*/

/* d88 */
struct st_d88_header		D88IDX;
struct st_d88_sect_header	D88SECT;
FILE *D88FILE;

#define DEFAULTEXT "D88"	/*デフォルト拡張子*/
#define TRYCOUNT	5	/*セクタリード時リトライカウント*/
#define TRYFCOUNT	20	/*フォーマット判別時別のトラックを読むリトライカウント*/
#define CY2D	41		/*2D のシリンダ数 普通は40です*/
#define CY2DD	82		/*2DDのシリンダ数 普通は80です*/
#define CY2HD	82		/*2HDのシリンダ数 PC98等は77、PC/ATは80です*/
#define SECTMAX   32	/*最大セクタ数 1セクタ256バイトの時26なので余裕を見て32くらい*/
#define BUFFSIZE  (16*1024)
unsigned char BUFF[BUFFSIZE];

char FILENAME[256];
unsigned char DRIVE;
unsigned char MODE;
#define READ		0
#define WRITE		1
#define M2D			(1 << 4)
/*#define MMFM		(1 << 5)
#define MDATAERR	(1 << 6)*/
unsigned char IDBUFF[SECTMAX * 4];

void usage(void)
{
/*
	puts("FLOPPY DISK IMAGE FILE READER / WRITER");
	puts("Create a disk image of the \"D88 format\" from the floppy disk, and writing from a disk image to a floppy disk of \"D88 format\".");
    puts("To specify the drive using the \"drive number\". Caution is not a \"drive letter\".");
	puts("");
	puts("FDIMG src dst [-2D]");
	puts("");
	puts("READ FD");
	puts("    FDIMG 0 filename.d88");
	puts("WRITE FD");
	puts("    FDIMG filename.d88 0");
    return;
*/    
	puts("FLOPPY DISK IMAGE FILE READER / WRITER  R002");
	puts("");
	puts("フロッピーディスクを読んでD88形式のディスクイメージを作ったり、D88形式のディスクイメージをフロッピーディスクに書き込みます。");
    puts("フロッピーディスクドライブの指定には、");
	puts("「ドライブレター」ではなく「ドライブ番号」 (0-3) を使用します。");
	puts("");
	puts("使い方");
	puts("FDIMG <読み取り元> <書き込み先> [-2D]");
	puts("");
	puts("ドライブ0のフロッピーの読み出し");
	puts("    FDIMG 0 filename.d88");
	puts("ドライブ0のフロッピーへ書き出し");
	puts("    FDIMG filename.d88 0");
    return;
}

/*改行しない単純文字出力*/
void print(char *str)
{
    while(*str) /* 0じゃなかったら1文字表示 */
    {
        putch(*str);
        str++;
    }
}

/* ドライブ番号かどうか調べる */
int setdrive(char str[])
{
	if(1 == strlen(str))
	{
		/*長さが1文字だったからドライブ番号かも*/
		switch(str[0])
		{
			case '0':
				DRIVE=0;
				return TRUE;
			case '1':
				DRIVE=1;
				return TRUE;
			case '2':
				DRIVE=2;
				return TRUE;
			case '3':
				DRIVE=3;
				return TRUE;
		}
	}
	/* ドライブ番号じゃないのでファイル名をセット */
	strncpy(FILENAME, str, sizeof FILENAME);
	FILENAME[(sizeof FILENAME) - 1] = '\0';
	return FALSE;
}

/* コマンドラインオプションを読む */
int option(int argc, char* argv[])
{
	if(3 > argc)
	{
		/*引数がない時*/
		usage();
		return FALSE;
	}

	DRIVE=0;

	/*1つ目の引数*/
	if(setdrive(argv[1])) MODE = READ;	/*ドライブ番号だった時*/
	else MODE = WRITE;					/*ドライブ番号じゃない時*/
	
	/*2つ目の引数*/
	if(setdrive(argv[2]))
	{
		/*ドライブ番号だった時*/
		if(READ == MODE)
		{
			/*1つ目2つ目両方ドライブ番号ならエラー*/
			usage();
			return FALSE;
		}
	}
	else
	{
		/*ドライブ番号じゃない時*/
		if(WRITE == MODE)
		{
			/*1つ目2つ目両方ファイル名ならエラー*/
			usage();
			return FALSE;
		}
	}

	if(3 < argc)
	{
		/*3つ目の引数*/
		if(0 == strnicmp(argv[3], "-2D", 3)) /* 大文字小文字を区別しない */
		{
			/*等しい時*/
			MODE |= M2D;
		}
	}

	return TRUE;
}

/*ファイル名に拡張子が無かったら追加する*/
int addext(char filename[], char exte[])
{
	unsigned int pos, len;

	/*後端に移動*/
	pos = strlen(filename) - 1;
	len = strlen(exte);
	if((5 < len) || (pos > (0xfe - len))) return FALSE;	/*文字数が多すぎ*/

	/*拡張子があるか後端から調べる*/
	for(; 0 != pos; pos--)
	{
		if('.' == filename[pos]) return TRUE;	/*拡張子があった*/
		if('\\' == filename[pos]) break; 	/*拡張子が無い*/
	}
	/*文字列連結*/
	strncat(filename, ".", 1);
	strncat(filename, exte, len);

	return TRUE;
}

/* ファイル名からパスと拡張子を取り除く */
int retname(char in[], char out[], int size)
{
	unsigned int pos;
	unsigned char top, end, len;

	end = strlen(in) - 1;	/*後端*/
	if(0xfe < end) return FALSE; /*文字数が多すぎ*/
	/*ファイル名の先頭を調べる*/
	top = 0;
	pos = end;
	while(TRUE)
	{
		if('\\' == in[pos])
		{
			if(pos != end) top = pos + 1;
			break;
		}
		if(0 == pos--) break;
	}
	/*拡張子を除いたファイル名の終端を調べる*/
	for(pos = end; 0 != pos; pos--) /*（endから1まで）*/
	{
		if('\\' == in[pos])
		{
			/*拡張子が無い*/
			break;
		}
		if('.' == in[end])
		{
			/*拡張子があった*/
			end = pos - 1;
			break;
		}
	}
	
	/*出力用配列に文字列コピー*/
	len = end - top + 1;
	/*printf("top %d end %d len %d size %d\n", top, end, len, size);*/
	/*if((len + 1) > size) return FALSE;*/ /*出力先のサイズが足りない*/
	if((len + 1) > size) end = top + size -1; /*出力先のサイズが足りない*/
	memset(&D88IDX, '\0', size);		/*終端文字でクリア*/
	memcpy(out, &in[top], len);

	return TRUE;
}

void printop(void)
{
	printf("FILENAME : %s\n",FILENAME);
	printf("DRIVE : %d\n",DRIVE);
	printf("D88NAME : %s\n",D88IDX.name);
	if(WRITE == (MODE & WRITE)) puts("WRITE");
	else puts("READ");
	if(M2D == (MODE & M2D)) puts("2D");
}

/*D88のヘッダ部分を書き込む*/
int d88idx_write(void)
{
	unsigned char track;
	unsigned int wcount;

	/*パディングの影響が出ないようにfwriteを並べているけれど、不要かも*/
	/*fwriteの戻り値は書き込んだバイト数じゃなくて　size のデータを書き込んだ個数*/
	wcount = 0;
	wcount += fwrite( D88IDX.name,         17, 1, D88FILE);
	wcount += fwrite( D88IDX.reserve,       9, 1, D88FILE);
	wcount += fwrite(&D88IDX.write_protect, 1, 1, D88FILE);
	wcount += fwrite(&D88IDX.type,          1, 1, D88FILE);
	wcount += fwrite(&D88IDX.size,          4, 1, D88FILE);
	for (track = 0; 164 != track; track++)	/*0-163*/
	{
		wcount += fwrite(&D88IDX.track_offset[track], 4, 1, D88FILE);
	}
	if((5 + 164) == wcount) return TRUE;	/* 5 + 164回書けていれば正常 */

	return FALSE;	/*異常終了*/
}

/*D88の1セクタ分書き込む*/
int d88sect_write(void)
{
	unsigned int wcount;

	wcount = 0;
	wcount += fwrite(&D88SECT.c,       1, 1, D88FILE);
	wcount += fwrite(&D88SECT.h,       1, 1, D88FILE);
	wcount += fwrite(&D88SECT.r,       1, 1, D88FILE);
	wcount += fwrite(&D88SECT.n,       1, 1, D88FILE);
	wcount += fwrite(&D88SECT.sect,    2, 1, D88FILE);
	wcount += fwrite(&D88SECT.density, 1, 1, D88FILE);
	wcount += fwrite(&D88SECT.deleted, 1, 1, D88FILE);
	wcount += fwrite(&D88SECT.status,  1, 1, D88FILE);
	wcount += fwrite( D88SECT.reserve, 5, 1, D88FILE);
	wcount += fwrite(&D88SECT.size,    2, 1, D88FILE);
	wcount += fwrite( BUFF, D88SECT.size, 1, D88FILE);
	
	if(11 == wcount) return TRUE;	/*正常終了*/
	return FALSE;	/*異常終了*/
}

/*D88のヘッダ部分を読み込む*/
int d88idx_read(void)
{
	unsigned char track;
	unsigned int rcount;

	/*fwriteの戻り値は書き込んだバイト数じゃなくて　size のデータを書き込んだ個数*/
	rcount = 0;
	rcount += fread( D88IDX.name,         17, 1, D88FILE);
	rcount += fread( D88IDX.reserve,       9, 1, D88FILE);
	rcount += fread(&D88IDX.write_protect, 1, 1, D88FILE);
	rcount += fread(&D88IDX.type,          1, 1, D88FILE);
	rcount += fread(&D88IDX.size,          4, 1, D88FILE);
	for (track = 0; 164 != track; track++)	/*0-163*/
	{
		rcount += fread(&D88IDX.track_offset[track], 4, 1, D88FILE);
	}
	D88IDX.name[16] = 0;	/*念のため終端文字を書いておく*/
	if((5 + 164) == rcount) return TRUE;	/* 5 + 164回読めていれば正常 */

	return FALSE;	/*異常終了*/
}

/*D88の1セクタ分のヘッダとデータを読み込む*/
int d88sect_read(void)
{
	unsigned int rcount;

	rcount = 0;
	rcount += fread(&D88SECT.c,       1, 1, D88FILE);
	rcount += fread(&D88SECT.h,       1, 1, D88FILE);
	rcount += fread(&D88SECT.r,       1, 1, D88FILE);
	rcount += fread(&D88SECT.n,       1, 1, D88FILE);
	rcount += fread(&D88SECT.sect,    2, 1, D88FILE);
	rcount += fread(&D88SECT.density, 1, 1, D88FILE);
	rcount += fread(&D88SECT.deleted, 1, 1, D88FILE);
	rcount += fread(&D88SECT.status,  1, 1, D88FILE);
	rcount += fread( D88SECT.reserve, 5, 1, D88FILE);
	rcount += fread(&D88SECT.size,    2, 1, D88FILE);
	rcount += fread( BUFF, D88SECT.size, 1, D88FILE);
	
	if(11 == rcount) return TRUE;	/*正常終了*/
	return FALSE;	/*異常終了*/
}

/*セクタシーケンスを読み込む
戻り値 0:異常終了 それ以外:セクタ数*/
int sectseq(unsigned char device, unsigned char mod, unsigned char h)
{
	unsigned char sechead,seccount,count,startpos;

    /*READDIAG で先頭セクタの番号を調べる*/
    if(! secthead(device, mod, h, BUFF))
    {
		return FALSE; /*エラー*/
	}
    sechead = BUFF[0];    /*先頭のセクタ番号*/
    	
	/*READID で1トラック分のセクタのIDを調べる*/
    for(count = 0; count != (SECTMAX *2 ); count++)
    {
        if(! readid(device, mod, h, &BUFF[count * 4]))
        {
			return FALSE;
		}
    }
	
    /*1トラックに何セクタあるか数える*/
    for(seccount = 1; seccount != (SECTMAX - 1); seccount++)
    {
        if(BUFF[2] /*R*/ == BUFF[(seccount * 4) + 2]) break;
    }

    /*先頭から順にバッファを整頓*/
    /*先頭位置を調べる*/
    for(count=0; count != (SECTMAX - 1); count++)
    {
        if(sechead == BUFF[(count * 4) + 2]) break;
    }
    startpos = count * 4;

	/*コピー*/
	memcpy(IDBUFF, &BUFF[startpos], seccount * 4);

	/* IDが正常か検査する */
	for(count=0; count != seccount; count++)
	{
		/*if(           90 < IDBUFF[count * 4 + 0]) return FALSE;*/ /*C 極端に大きかったらおかしい */
		if(            1 < IDBUFF[count * 4 + 1]) return FALSE; /*H ヘッドは0か1 */
		if((SECTMAX - 1) < IDBUFF[count * 4 + 2]) return FALSE; /*R 極端に大きかったらおかしい 本当は0でもおかしい */
		if(            5 < IDBUFF[count * 4 + 3]) return FALSE; /*H セクタ長コードは、普通 0 - 3 */
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
	unsigned char sectcount;

	printf("DRIVE %d のフロッピーディスクを読み取っています ... ", device);

	memset(&D88IDX, 0, sizeof D88IDX);	/*ゼロクリア*/
	retname(FILENAME, D88IDX.name, sizeof D88IDX.name);	/*イメージ名セット*/
	addext(FILENAME, DEFAULTEXT);		/*ファイル名に拡張子が無ければ拡張子追加*/

	/* ディスクのタイプを調べる FM/MFM 2DD/2HD */
	/* IDを読んで2DDなのか2HDなのか調べる */
	drvmode(device, MODE2DD);	/*2DDモード*/
	track00(FD2HD | device);
	count=0;
	while(TRUE)
	{
		if(mod = sectsense(device, count/2, count & 1)) /*シリンダ0ヘッド0から順番に*/
		{
			break; /*正常終了*/
		}
		/*異常なら繰り返し別のトラックを読む*/
		if(TRYFCOUNT == count++)	/*リトライしても読めない*/
		{
			puts("ディスクのフォーマットを判別できませんでした");
			return FALSE;
		}
	}
	
	if(FD2HD == (FD2HD & mod))
	{
		print("2HD ");
		D88IDX.type = D88_2HD;
		tr_max = CY2HD * 2; /*トラック数*/
	}
	else
	{
		if(M2D == (MODE & M2D))
		{
			/*オプションで2D指定なら 2D*/
			print("2D  ");
			D88IDX.type = D88_2D;
			drvmode(device, MODE2D);	/*2Dモード*/
			tr_max = CY2D * 2;
		}
		else
		{
			print("2DD ");
			D88IDX.type = D88_2DD;
			tr_max = CY2DD * 2;
		}
	}
	device = device | (FD2HD & mod);	/*デバイス番号に2HD/2DDの指定を追加*/
	mod = FDMFM & mod;	/*modの変調方式だけ残す*/

	D88FILE = fopen(FILENAME, "wb");
	if(NULL == D88FILE)
	{
		puts("ファイルを開けません");
		fclose(D88FILE);
		return FALSE;
	}

	if(! d88idx_write())	/*D88ヘッダ仮書き込み*/
	{
		/*エラー*/
		fclose(D88FILE);
		puts("\nWRITE ERROR ");
		return FALSE;
	}
	D88IDX.size = D88_IDXSIZE;
	
	puts("\n  C  H  R  N ");
	track00(device);
	for(track = 0; track < tr_max; track++)
	{
		printf("  -  -  -  -    READ SECSEQ         %3d / %3d track \r", track, tr_max - 1);
		/* シリンダ番号=track/2 ヘッド番号=track & 0x01*/
		mod = modsense(device, track/2, track & 0x01); /*トラックごとに変調方式が違うかもしれない*/
		/*modsenseでseekもする*/
		if(sectcount = sectseq(device, mod, track & 0x01))	/*セクタシーケンスを読み込む*/
		{													/*ヘッド番号は track & 0x01 (trackの下位1ビット)*/
			/*セクタシーケンスを正常に読めた*/
			printf("               %d secter/track ",sectcount);
			if(FDMFM == mod) printf("MFM \r");
			else printf("FM \r");
			D88IDX.track_offset[track] = ftell(D88FILE);	/*トラックオフセット保存*/
			/*セクタを読む*/
			for(count = 0; count != sectcount; count++)
			{
				for(try = 0; TRYCOUNT != try; try++)
				{
					printf(" %2d %2d %2d %2d \r",IDBUFF[count * 4 + 0] /* C */ /* \r キャリッジリターン \n ラインフィード */
					                            ,IDBUFF[count * 4 + 1] /* H */
											    ,IDBUFF[count * 4 + 2] /* R */
											    ,IDBUFF[count * 4 + 3] /* N */);

					memset(&D88SECT, 0, sizeof D88SECT);	/*セクタヘッダゼロクリア*/

					/*D88のエラーコードはPC98のBIOSのエラーコード*/
					D88SECT.status = readdata(device
					                         ,mod
											 ,IDBUFF[count * 4 + 0] /* C */
											 ,IDBUFF[count * 4 + 1] /* H */
											 ,IDBUFF[count * 4 + 2] /* R */
											 ,IDBUFF[count * 4 + 3] /* N */
											 ,BUFF);
				
					/* リードエラーがあったら表示する */
					if(0 != D88SECT.status)
					{
						/*エラーがあった*/
						printf("                                                                           \r"); /* 現在行消去 */
						printf(" %2d %2d %2d %2d  :%2d:",IDBUFF[count * 4 + 0] /* C */
					                            	,IDBUFF[count * 4 + 1] /* H */
										  	    	,IDBUFF[count * 4 + 2] /* R */
											    	,IDBUFF[count * 4 + 3] /* N */
													,1 + try ); /* リトライ数 */
						printf( errmsg(D88SECT.status));
						printf("\r");
					}
					else
					{
						/*エラーがなかった*/
						break;
					}				
				}

				if(0 != D88SECT.status)
				{
					printf("\n");	/* エラーがあったとき改行してエラーメッセージ残す */
				}

				D88SECT.c = IDBUFF[count * 4 + 0]; /* C */
				D88SECT.h = IDBUFF[count * 4 + 1]; /* H */
				D88SECT.r = IDBUFF[count * 4 + 2]; /* R */
				D88SECT.n = IDBUFF[count * 4 + 3]; /* N */
				D88SECT.sect = sectcount;	/* このトラック内のセクタ数 */
				
				/*PC98のBIOSとD88のFMとMFMの符号反転しているみたい*/
				if(FDMFM & mod) D88SECT.density = D88_MFM;
				else D88SECT.density = D88_FM;
				
				/* DELETED DATA かどうか */
				if(D88_DDAM == D88SECT.status) D88SECT.deleted = D88_DDAM;
				else D88SECT.deleted = D88_DAM;
				
				D88SECT.size = sectlength(D88SECT.n);	/* セクタのサイズ (byte) */
				
				/*1セクタずつ書くと遅い。1トラック分バッファを確保してまとめて書いたら速いよな*/
				if(! d88sect_write())	/*1セクタ分ファイルに書き出す*/
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
			/*ID読み取りをBIOSで8回リトライしたけどエラー このトラックはスキップ*/
			printf(" %2d %2d  -  -    ID READ ERROR \n",track / 2		/* C */
                                                 ,track & 0x01);	/* H */

			D88IDX.track_offset[track] = 0;	/*トラックオフセット保存（空）*/
		}
	}

	/*全トラック終了*/
	D88IDX.size = ftell(D88FILE); /*イメージ全体のサイズ （連結した場合次のイメージへのオフセット）*/
	if(0 != fseek(D88FILE, 0, SEEK_SET))	/*ファイル先頭にシーク*/
	{
		/*seekでエラー （0位外はエラー）*/
		fclose(D88FILE);
		puts("\nD88 FILE WRITE ERROR");
		return FALSE;		
	}

	if(! d88idx_write())	/*D88ヘッダ本書き込み*/
	{
		/*エラー*/
		fclose(D88FILE);
		puts("\nD88IDX WRITE ERROR ");
		return FALSE;
	}

	fclose(D88FILE);
	return TRUE;
}

/*ファイルから読んでフロッピーへ書き込むモード*/
int writemode(void)
{
	unsigned char device, track, tr_max, mod, sectcount, count;

	addext(FILENAME, DEFAULTEXT);		/*ファイル名に拡張子が無ければ拡張子追加*/
	/*ファイルを開く*/
	D88FILE = fopen(FILENAME, "rb");
	if(NULL == D88FILE)
	{
		printf("File open error : %s", FILENAME);
		return FALSE;
	}

	memset(&D88IDX, 0, sizeof D88IDX);		/*ゼロクリア*/
	memset(&D88SECT, 0, sizeof D88SECT);	/*ゼロクリア*/
	
	if(! d88idx_read())
	{
		fclose(D88FILE);
		puts("\nD88IDX READ ERROR ");
		return FALSE;
	}

	printf("NAME : %s \n", D88IDX.name);
	printf("DRIVE %d のフロッピーディスクに書き込んでいます ...  ", DRIVE);

	switch(D88IDX.type)
	{
		case D88_2HD:
			device = DRIVE | FD2HD;
			puts("2HD ");
			break;
		case D88_2DD:
			device = DRIVE | FD2DD;
			drvmode(device, MODE2DD);	/*2DDモード*/
			puts("2DD ");
			break;
		case D88_2D:
			device = DRIVE | FD2DD;
			drvmode(device, MODE2D);	/*2Dモード*/
			puts("2D  ");
			break;
	}


	/*何トラックまであるか終端から調べる*/
	for(tr_max = 163; 0 < tr_max ; tr_max--)
	{
		if(0 != D88IDX.track_offset[tr_max]) break;
	}
	tr_max++; /*トラックの数*/

	track00(device);
	for(track = 0; tr_max > track; track++)
	{
		printf("              %3d / %3d track \r", track, tr_max - 1);
		if(0 != fseek(D88FILE, D88IDX.track_offset[track], SEEK_SET))	/*トラックの先頭にシーク*/
		{
			/*seekでエラー (0位外はエラー)*/
			/*fclose(D88FILE);
			puts("\nD88 FILE READ ERROR (SEEK 1) ");
			return FALSE;*/
			/*シークエラー無視*/
		}
		/*セクタの並びを調べる*/
		for(count = 0; SECTMAX > count; count++)
		{
			if(! d88sect_read())
			{
				puts("\nD88SECT READ ERROR (SECT SEQ) ");
				return FALSE;
			}
			IDBUFF[count * 4 + 0] = D88SECT.c;
			IDBUFF[count * 4 + 1] = D88SECT.h;
			IDBUFF[count * 4 + 2] = D88SECT.r;
			IDBUFF[count * 4 + 3] = D88SECT.n;

			if(D88SECT.sect == (count + 1)) break; /* 1トラック分終了 */
		}
		/*printf("count:%d D88SECT.sect:%d\n", count, D88SECT.sect);*/

		if(D88_MFM == D88SECT.density) mod = FDMFM;
		else mod = FDFM;
		sectcount = D88SECT.sect;
		printf(" FORMAT     \r");
		/*トラックフォーマット trackfmt()でシークもします
		シークのないタイミングでもガチョンガチョンいう。ヘッドのアンロード/ロードをしているみたい*/
		if(! trackfmt(device, mod, track / 2, track & 0x01, D88SECT.n, sectcount, 0, IDBUFF))
		{
			/*エラー*/
			fclose(D88FILE);
			puts("\nTRACK FORMAT WRITE ERROR ");
			return FALSE;
		}

		printf(" DATA WRITE \r");
		/*セクタのデータを書き込む*/
		if(0 != fseek(D88FILE, D88IDX.track_offset[track], SEEK_SET))	/*トラックの先頭にシーク*/
		{
			/*seekでエラー (0位外はエラー)*/
			/*fclose(D88FILE);
			puts("\nD88 FILE READ ERROR (SEEK 2)");
			return FALSE;*/
			/*シークエラー無視*/
		}
		for(count = 0; sectcount > count; count++)
		{
			if(! d88sect_read())
			{
				puts("\nD88SECT READ ERROR (DATA) ");
				return FALSE;
			}
			if(D88_DAM == D88SECT.deleted)
			{
				/*DAM*/
				if(! writedata(device, mod, D88SECT.c, D88SECT.h, D88SECT.r, D88SECT.n, BUFF))
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
				if(! writedeleted(device, mod, D88SECT.c, D88SECT.h, D88SECT.r, D88SECT.n, BUFF))
				{
					/*エラー*/
					fclose(D88FILE);
					puts("\nID READ ERROR (DDAM) ");
					return FALSE;
				}
			}
		}
	}

	return TRUE;
}

int main(int argc, char* argv[])
{
	
	if(!option(argc, argv)) return FALSE;	/*コマンドらいのプションを読んでグローバル変数をセット*/
	
	/*printop();*/

	if(WRITE == (MODE & WRITE))
	{
		if(! writemode()) return FALSE;		/*ファイルから読んでフロッピーへ書き込むモード*/
	}
	else
	{
		if(! readmode(DRIVE)) return FALSE;		/*フロッピーから読んでファイルに書き込むモード*/
	}

    return TRUE;
}