/*
FDSEEK.EXE
FDDのモーターを回した後、リキャブレイトしてからシーク動作を行います。
指定回数繰り返しシークを行うことができるので、クリーニングディスクを使うときやFDD注油後の馴染ませなどに使うことができます。

PC-9801ではDISK BIOSを使用して動作し、PC/ATではFDCを直接操作して動作します。
BIOSやFDCを直接操作するのでDOS上でしか動作しません。

FDSEEK <DRIVE> [-2D/-2DD/-2HD/-Cxx] [-Nxx]
  DRIVE:ドライブ番号 (0-3)
  -2D: 目標シリンダを39に設定
  -2DD:目標シリンダを79に設定
  -2HD:目標シリンダを79に設定
  -Cxx:目標シリンダ番号をxxに設定 (0-81)
  -Nxx:xx回繰り返す 省略時は1 (1-20)
オプションの大文字／小文字は区別しません。

このソースコードは CC0 パブリック・ドメイン提供です。
https://creativecommons.org/publicdomain/zero/1.0/deed.ja

2023年10月28日 作成 佐藤恭一 kyoutan.jpn.org
*/
#define RELEASE "R1"
#define SIGNATURE "2023 kyoutan.jpn.org"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fdimage.h"
#include "diskbios.h"

void usage(void)
{
    printf("FDSEEK  %s    %s\r\n", RELEASE, SIGNATURE);
    puts("After turning the motor of the FDD, recabulate it and then perform the seek operation.");
    puts("");
    puts("FDSEEK <DRIVE> [-2D/-2DD/-2HD/-Cxx] [-Nxx]");
    puts("  DRIVE: Specify the drive number. (0-3)");
    puts("  -2D:  Seek to cylinder 39.");
    puts("  -2DD: Seek to cylinder 79.");
    puts("  -2HD: Seek to cylinder 79.");
    puts("  -Cxx: Seek to cylinder xx. (0-81)");
    puts("  -Nxx: Repeat xx times. (1-20)");
    puts("All other drive numbers can be omitted. If omitted, seek once toward the 39 cylinder.");
}
unsigned char DRIVE, COUNT, CYLINDER;

/* コマンドラインオプションを読む */
int option(int argc, char *argv[])
{
    unsigned char pos;

    if (2 > argc)
    {
        /*引数がない時*/
        return FALSE;
    }

    DRIVE = 0;
    COUNT = 1;
    CYLINDER = 39;

    for (pos = 1; pos < argc; pos++)
    {
        /*
        puts(argv[pos]);
        */
        if (1 == strlen(argv[pos]))
        {
            /*1文字の時、ドライブ番号*/
            DRIVE = atoi(argv[pos]);
            if (3 < DRIVE)
                DRIVE = 3;
        }

        if (0 == strnicmp(argv[pos], "-2DD", 4)) /* 大文字小文字を区別しない */
        {
            /*等しい時*/
            CYLINDER = 79;
        }

        if (0 == strnicmp(argv[pos], "-2HD", 4))
        {
            CYLINDER = 79;
        }

        if (0 == strnicmp(argv[pos], "-C", 2))
        {
            if (2 < strlen(argv[pos]))
            {
                CYLINDER = atoi(&argv[pos][2]); /*3文字目以降を数値に変換*/
                if (81 < CYLINDER)
                    CYLINDER = 81;
            }
        }

        if (0 == strnicmp(argv[pos], "-N", 2))
        {
            if (2 < strlen(argv[pos]))
            {
                COUNT = atoi(&argv[pos][2]); /*3文字目以降を数値に変換*/

                if (20 < COUNT)
                    COUNT = 20;

                if (0 == COUNT)
                    COUNT = 1;
            }
        }

        if (0 == strnicmp(argv[pos], "-H", 2))
        {
            return FALSE;
        }
    }
    return TRUE;
}

unsigned char fdseek(void)
{
    /*FDを操作できるかチェックする。
      PC98用をPCATで実行したり、PCAT用をPC98で実行した時に異常終了させる。*/
    if (!fdcheck())
    {
        puts("\r\nERROR : Could not access the floppy disk.");
        return FALSE;
    }

    printf("DRIVE:%d CYLINDER:%d COUNT:%d\r\n", DRIVE, CYLINDER, COUNT);

    for (; 0 != COUNT; COUNT--)
    {
        diskinit(DRIVE);
        if (!track00(DRIVE))
            return FALSE;

        if (!seek(DRIVE, CYLINDER))
            return FALSE;

        printf("*");
    }
    puts("");
    return TRUE;
}

int main(int argc, char *argv[])
{
    if (!option(argc, argv)) /*コマンドラインオプションを読んでグローバル変数をセット*/
    {
        /*引数が無い時*/
        usage();
        diskexit();
        return EXIT_FAILURE;
    }

    if (!fdseek())
    {
        diskexit();
        return EXIT_FAILURE;
    }

    diskexit();
    return EXIT_SUCCESS;
}