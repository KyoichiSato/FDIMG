/*
PCAT (IBM PC / DOS/V / OADG PC) の
タイマーBIOS / 割り込みコントローラ / DMAコントローラを使う （FDC用）
LSI C-86 Ver.3.30 試食版用

This code is provided under a CC0 Public Domain License.
http://creativecommons.org/publicdomain/zero/1.0/

2022年7月6日 佐藤恭一 kyoutan.jpn.org

参考にした資料
OADGテクニカル・リファレンス ハードウェア編
PC-9800シリーズ テクニカルデータブック 1986年版 （割り込みコントローラとDMAコントローラの使い方が載っている）
LSI C-86 Ver.3.30 試食版 ユーザーズマニュアル
*/
#include <dos.h> /* int86, segread */
#include "timePCAT.h"
#include "diskbios.h"
#include "diskPCAT.h"

extern struct PCATWORK DISKWORK;

/*#define DEBUG*/
#ifdef DEBUG
#include <stdio.h>
#endif

/* timer */

/* BIOSでシステムタイマ値の下位16bitを取得 値はタイマー割り込みで55ms間隔で増加 */
/* 60分で16bitオーバーフロー */
unsigned short timer_get16()
{
    union REGS reg;
    reg.h.ah = 0x00;
    int86(0x1a, &reg, &reg);
    /* CX上位 DX下位 タイマ値 */
    return reg.x.dx;
}

/* 16bitカウンタ値の差を求める ラップアラウンド対応 */
/* current - start */
unsigned short UshortSub(unsigned short current, unsigned short start)
{
    if (start > current) /* ラップアラウンドしている */
    {
        return (0xffff - start) + current + 1;
    }
    return current - start;
}

/* 指定時間経過したか調べる 経過したらTRUE */
char timeout_tick(unsigned short tick, unsigned short start)
{
    unsigned short current;
    current = timer_get16();
    if (tick < UshortSub(current, start))
    {
        return TRUE;
    }
    return FALSE;
}

/* 指定時間経過したか調べる ms  経過したらTRUE */
char timeout_ms(unsigned short time, unsigned short start)
{
    time = time / 55;
    return timeout_tick(time, start);
}

/* tick数分待つ 2~65535 1tick=55ms*/
void wait_tick(unsigned short tick)
{
    unsigned short start;
    start = timer_get16();
    if (2 > tick)
        tick++; /*最小2tick*/

    while (UshortSub(timer_get16(), start) < tick)
        ; /* 時間まで待つ */
}

/* ms単位のウエイト 110~65500ms 分解能55ms*/
void wait_ms(unsigned short time)
{
    time = time / 55; /* tick値に変換 1tick 55ms */
    wait_tick(time);
}

/* 割り込みコントローラ FDC:IRQ6 */
#define PIC1REG1 0x020
#define PIC1REG2 0x021
#define pic_IMRr() inp(PIC1REG2)
#define pic_IMRw(data) outp(PIC1REG2, data)

/* FDC割り込みを禁止／許可
1:割り込みをマスク 0:割り込みを許可 */
void PICfdcmask(unsigned char mask)
{
    if (0 == mask)
    {
        pic_IMRw(~(1 << 6) & pic_IMRr()); /* マスクレジスタの bit6をリセット */
        return;
    }
    pic_IMRw((1 << 6) | pic_IMRr()); /* マスクレジスタの bit6をセット */
}

/* 割り込みリクエストレジスタをリード */
unsigned char PICrIRR(void)
{
    outp(PIC1REG1, 0x0a); /* OCW3 IRRリード*/
    return inp(PIC1REG1);
}

/* DMAコントローラ */
#define DMAaddr2 0x004     /* Memory Address Register / Channel 2 */
#define DMAcount2 0x005    /* Transfer Count Register / Channel 2 */
#define DMAstatus 0x008    /* R Status Register */
#define DMAcommand 0x008   /* W Command Register */
#define DMAsmask 0x00A     /* W Single Mask Register Bit */
#define DMAmode 0x00B      /* W Channel 0-3, Mode Register */
#define DMAbyteclear 0x00C /* W Channel 0-3, Clear Byte Pointer */
#define DMAmasterclr 0x00D /* W Channel 0-3, Master Clear */
#define DMAtemp 0x00D      /* R Temp */
#define DMAclearmask 0x00E /* W Channel 0 - 3, Clear Mask Register */
#define DMAwritemask 0x00F /* Channel 0 - 3, Write All Mask Register Bits */
#define DMApage2 0x081     /* Channel 2, Page Table Address Register */

/* DMA ch2(FDC) アドレスレジスタリード */
unsigned short DMAreadaddrreg(void)
{
    unsigned short ret;
    outp(DMAsmask, 0x06);     /* DMA ch2 mask 1 */
    outp(DMAbyteclear, 0x00); /* Clear Byte Pointer */
    ret = inp(DMAaddr2);      /* 下位・上位の順 */
    ret |= (inp(DMAaddr2) << 8);
    outp(DMAsmask, 0x02); /* DMA ch2 mask 0 */
    return ret;
}

/* DMA ch2(FDC) カウントレジスタリード */
unsigned short DMAreadcountreg(void)
{
    unsigned short ret;
    outp(DMAsmask, 0x06);     /* DMA ch2 mask 1 */
    outp(DMAbyteclear, 0x00); /* Clear Byte Pointer */
    ret = inp(DMAcount2);     /* 下位・上位の順 */
    ret |= (inp(DMAcount2) << 8);
    outp(DMAsmask, 0x02); /* DMA ch2 mask 0 */
    return ret;
}

/* DMA ch2(FDC) ページレジスタリード */
unsigned char DMAreadpagereg(void)
{
    return inp(DMApage2);
}

/* 変数のアドレスからDMAにセットするページアドレスを得る */
unsigned char DMAgetpage(unsigned char *buff)
{
    struct SREGS seg;
    unsigned long address;

    segread(&seg); /* セグメントレジスタの値を取得 */

    /* データセグメントとオフセットから物理アドレスを求める */
    address = ((unsigned long)seg.ds << 4);
#ifdef DEBUG
    printf(" ### DMA GETPAGE/DS:%04x <<4:%06x ", seg.ds, address);
    printf("ADDRESS:%04x ", (unsigned short)buff);
#endif
    address += (unsigned short)buff;
#ifdef DEBUG
    printf("PADDRESS:%06x ", address);
    printf("PAGE:%02x ", address >> 16);
#endif
    return (address >> 16); /*物理アドレスの上位 4bitを返す */
}

/* 変数のアドレスからDMAにセットするアドレスを得る */
unsigned short DMAgetaddress(unsigned char *buff)
{
    struct SREGS seg;
    unsigned long address;

    segread(&seg); /* セグメントレジスタの値を取得 */

    /* データセグメントとオフセットから物理アドレスを求める */
    address = ((unsigned long)seg.ds << 4);
    address += (unsigned short)buff;
    return address; /*物理アドレスの下位 16bitを返す */
}

/* アドレスがあふれる位置にあるか検査する
FALSE=あふれる TRUE=あふれない */
unsigned char DMAchackaddress(unsigned short start, unsigned short size)
{
    /* DMAの転送回数は size + 1回
     0xffff, 0 は 0xffffから 1バイト書くので許される */
    if ((start + size) < start)
        return FALSE; /* ラップアラウンドする */
    return TRUE;      /* ラップアラウンドしない */
}

/* DMA Channel 2 マスク (FDC) */
void DMAfdcmask(void)
{
    outp(DMAsmask, 0x06); /* Set Channel Mask Bit      (000Ah) 0b00000110 x6h ch2マスク*/
}

/* DMA set FDC:Channel 2 I/O -> メモリ
戻り値 0:異常終了 バッファのアドレスが良くない DMA転送であふれる */
unsigned char DMAset_toMEM(unsigned char *buff, unsigned short count)
{
    unsigned char page;
    unsigned short address;

    if (FALSE == DMAchackaddress(DMAgetaddress(buff), count))
    {
        return FALSE; /* バッファのアドレスが良くない。DMA転送であふれる */
    }
    page = DMAgetpage(buff);
    address = DMAgetaddress(buff);
    count--;                         /* 0で1回転送するので -1 */
    outp(DMAsmask, (1 << 3) | 0x02); /* Set Channel Mask Bit      (000Ah) 0b00000110 x6h ch2マスク*/
    outp(DMAbyteclear, 0x00);        /* Clear Byte Pointer        (000Ch) xxh 何かを書き込むと、カウンタレジスタのファースト／ラストフリップフロップの初期化 */
    outp(DMAaddr2, address);         /* Write Memory Address      (0004h) xxh 下位 */
    outp(DMAaddr2, address >> 8);    /* Write Memory Address      (0004h) xxh 上位 */
    outp(DMApage2, page);            /* Write Page Table Address  (0081h) xxh */
    outp(DMAbyteclear, 0x00);        /* Clear Byte Pointer        (000Ch) xxh 何かを書き込むと、カウンタレジスタのファースト／ラストフリップフロップの初期化 */
    outp(DMAcount2, count);          /* Write Register Count      (0005h) xxh 下位 */
    outp(DMAcount2, count >> 8);     /* Write Register Count      (0005h) xxh 上位 */
    outp(DMAmode, 0x46);             /* Write Mode Register       (000Bh) 0b0100 0110 46h シングルモード／インクリメント／I/O->メモリ／チャンネル2 */
    outp(DMAsmask, 0x02);            /* Clear Channel 2 Mask Bit  (000Ah) 0b00000010 x2h ch2マスククリア*/
    return TRUE;
}

/* DMA set FDC:Channel 2 メモリ -> I/O
戻り値 0:異常終了 */
unsigned char DMAset_toIO(unsigned char *buff, unsigned short count)
{
    unsigned char page;
    unsigned short address;

    if (FALSE == DMAchackaddress(DMAgetaddress(buff), count))
    {
        return FALSE; /* バッファのアドレスが良くない。DMA転送であふれる */
    }
    page = DMAgetpage(buff);
    address = DMAgetaddress(buff);
    count--;                         /* 0で1回転送するので -1 */
    outp(DMAsmask, (1 << 3) | 0x02); /* Set Channel Mask Bit      (000Ah) 0b00000110 x6h ch2マスク*/
    outp(DMAbyteclear, 0x00);        /* Clear Byte Pointer        (000Ch) xxh 何かを書き込むと、カウンタレジスタのファースト／ラストフリップフロップの初期化 */
    outp(DMAaddr2, address);         /* Write Memory Address      (0004h) xxh 下位 */
    outp(DMAaddr2, address >> 8);    /* Write Memory Address      (0004h) xxh 上位 */
    outp(DMApage2, page);            /* Write Page Table Address  (0081h) xxh */
    outp(DMAbyteclear, 0x00);        /* Clear Byte Pointer        (000Ch) xxh 何かを書き込むと、カウンタレジスタのファースト／ラストフリップフロップの初期化 */
    outp(DMAcount2, count);          /* Write Register Count      (0005h) xxh 下位 */
    outp(DMAcount2, count >> 8);     /* Write Register Count      (0005h) xxh 上位 */
    outp(DMAmode, 0x4a);             /* Write Mode Register       (000Bh) 0b0100 1010 4ah シングルモード／インクリメント／I/O->メモリ／チャンネル2 */
    outp(DMAsmask, 0x02);            /* Clear Channel 2 Mask Bit  (000Ah) 0b00000010 x2h ch2マスククリア*/
    return TRUE;
}

#if 0
/*   DMA転送が終了したか調べる (DMA ch2 FDC)
TRUE=終了 FALSE=実行中 */
unsigned char DMAbusy(void)
{
#if 0
    /* FDCにコマンドを出した直後は TCビットが1になっている */
    if (0 == ((1 << 2) & inp(DMAstatus))) /* ch2 Terminal count ビットが0 */
        return FALSE;
    else
        return TRUE;
#endif
#if 0
    if (0xffff == DMAreadcountreg()) /* DMA残りカウントが0xffffになったら転送終了している */
        return TRUE;
    else
        return FALSE;
#endif
}
#endif

/* FDC(IRQ6) 割り込み要求
TRUE=要求あり FALSE=要求なし */
unsigned char PICfdcIRQ(void)
{
    if (0 == ((1 << 6) & PICrIRR())) /* IRQ6(FDC) 割り込み要求無し*/
        return FALSE;
    else
        return TRUE;
}

/*割り込み要求があるまで待つ
戻り値 FALSE=time out */
unsigned char PICfdcwait(void)
{
    unsigned short starttime;
    starttime = timer_get16();
    while (FALSE == PICfdcIRQ()) /* 割り込み要求が発生するまで待つ */
    {
        if (timeout_ms(DISKWORK.TIMEOUT, starttime))
            return FALSE; /* time out */
    }
#ifdef DEBUG
    printf(" IRQ6 ");
#endif
    return TRUE;
}