/*
フロッピーディスクの読み取りテストを行って、エラーがあれば表示する。
イメージファイルの作成は行わない。
全セクタを読んでエラーの有無を調べるが、セクタシーケンスは調べない。

このソースコードは CC0 パブリック・ドメイン提供です。
https://creativecommons.org/publicdomain/zero/1.0/deed.ja

2023年11月1日 作成 佐藤恭一 kyoutan.jpn.org
*/
#include <stdio.h>
#include "diskbios.h"
#include "fdimage.h"
#include "fdimg.h"

int diskinfo(unsigned char drive, unsigned char mode)
{
    if (!fdcheck())
    {
        puts("");
        puts("ERROR : Could not access the floppy disk.");
        return FALSE;
    }

    puts("diskinfo");
    return TRUE;
}