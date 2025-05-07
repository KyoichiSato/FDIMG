/*
D88ファイルの情報を表示する

このソースコードは CC0 パブリック・ドメイン提供です。
https://creativecommons.org/publicdomain/zero/1.0/deed.ja

2023年11月1日 作成 佐藤恭一 kyoutan.jpn.org
*/

#include <stdio.h>
#include <string.h>
#include "fdimage.h"
#include "diskbios.h"
#include "fdimg.h"

#define DEFAULTEXT "D88" /*デフォルト拡張子*/
/*#define BUFFSIZE (16 * 1024)*/
#define SECTMAX 32

/* D88イメージファイルの情報を表示する
引数 mode
0b0*000000
   +------- 1:詳細表示
filename[]:D88イメージのファイル名 d88name[]:空でなければD88ヘッダ内の名前をd88nameに変更する

戻り値 0:異常終了 */
int d88info(char *filename, unsigned char mode, char *d88name)
{
    unsigned char tr_max, track, count, previousN;
    unsigned long datasize;

    addext(filename, DEFAULTEXT); /*ファイル名に拡張子が無ければ拡張子追加*/

    /*ファイルを開く*/
    D88FILE = fopen(filename, "r+b"); /*バイナリファイル読み書き*/
    if (NULL == D88FILE)
    {
        printf("\r\nFile open error : %s\r\n", filename);
        return FALSE;
    }

    memset(&D88IDX, 0, sizeof D88IDX);   /*ゼロクリア*/
    memset(&D88SECT, 0, sizeof D88SECT); /*ゼロクリア*/

    /* D88のヘッダを読む */
    if (!d88idx_read())
    {
        fclose(D88FILE);
        puts("\r\nD88IDX READ ERROR ");
        return FALSE;
    }

    printf("FILE NAME  : %s \r\n", filename);
    printf("IMAGE NAME : %s ", D88IDX.name);

    /*d88nameに文字が入っていたらD88ヘッダのイメージ名を変更*/
    if (0 != strlen(d88name))
    {
        memset(D88IDX.name, '\0', sizeof D88IDX.name);
        strncpy(D88IDX.name, d88name, (sizeof D88IDX.name) - 1);
        /*ファイル先頭にシーク*/
        if (0 != fseek(D88FILE, 0, SEEK_SET))
        {
            /*seekでエラー （0位外はエラー）*/
            fclose(D88FILE);
            printf("\r\nFile seek error : %s\r\n", filename);
            return FALSE;
        }

        /*変更した名前でD88ヘッダを書き込む*/
        if (!d88idx_write())
        {
            fclose(D88FILE);
            printf("\r\nFile write error : %s\r\n", filename);
            return FALSE;
        }
        printf("--> %s ", D88IDX.name);
    }
    puts("");

    printf("MEDIA TYPE : ");
    switch (D88IDX.type)
    {
    case D88_2HD:
        puts("2HD ");
        break;
    case D88_2DD:
        puts("2DD ");
        break;
    case D88_2D:
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

    /*トラックの情報を表示する*/
    datasize = 0;
    previousN = 0xff;
    for (track = 0; tr_max > track; track++)
    {
        if (0 != D88IDX.track_offset[track]) /*トラックのデータが無ければスキップ*/
        {
            printf("              %3d / %3d track \r", track, tr_max - 1);
            if (0 != fseek(D88FILE, D88IDX.track_offset[track], SEEK_SET)) /*トラックの先頭にシーク*/
            {
                /*fseekでエラー (0位外はエラー)*/
                fclose(D88FILE);
                puts("\nD88 FILE READ ERROR (TRACK READ) ");
                return FALSE;
            }

            /*セクタをスキャンする*/
            for (count = 0; SECTMAX > count; count++)
            {
                if (!d88sect_read())
                {
                    fclose(D88FILE);
                    puts("\nD88SECT READ ERROR (SECT READ) ");
                    return FALSE;
                }

                datasize += D88SECT.size;
                IDBUFF[count] = D88SECT.r;
                if (0 == count)
                {
                    /*先頭セクタ*/
                    printf("%3d / %3d track ", track, tr_max - 1);
                    if (D88_MFM == D88SECT.density)
                    {
                        printf("MFM ");
                    }
                    else
                    {
                        printf(" FM ");
                    }
                    printf("%4d byte/sect  ", sectlength(D88SECT.n));
                    printf("%2d sect/track ", D88SECT.sect);
                }

                printf("\r");

                if (0 != D88SECT.status)
                {
                    /*セクタにエラー有り*/
                    printf("\nsect %d:%s", D88SECT.r, errmsg(D88SECT.status));
                }

                if (D88SECT.sect == (count + 1))
                {
                    break;
                }
            }
            /*1トラック終了*/
            /*詳細表示指示ならエラーがなくても改行*/
            if (0 != (0x40 & mode))
                previousN = 0xff;

            /*セクタ長コードが前のトラックと違ったら改行してセクタシーケンス表示*/
            if (previousN != D88SECT.n)
            {
                printf("\n");
                printf("sect sequence ");
                for (count = 0; count < D88SECT.sect; count++)
                {
                    printf("%d ", IDBUFF[count]);
                }
                printf("\r\n");
            }
        }
        previousN = D88SECT.n;
    }
    printf("\r\nDATA SIZE : %ld byte / %d Kbyte\r\n", datasize, datasize / 1024);

    fclose(D88FILE);
    return TRUE;
}