/*
DOS/V機(IBM PC / OADG PC) のFDCを直接操作する
PC9801と違ってBIOSで用意されていないFDCの機能が多いので、直接FDCを操作する

This code is provided under a CC0 Public Domain License.
http://creativecommons.org/publicdomain/zero/1.0/

2019年9月19日 佐藤恭一 kyoutan.jpn.org
2019年9月19日 システムタイマー動作確認
2022年7月6日 再開 RECALIBRATEやっと動いた。SENCE INTERRUPT STATUSコマンドの番号間違えていた。
2022年7月7日 SEEKとREAD ID動いた。FDCのハードウェアリセットができない
2022年7月9日 READ DATA動いたコマンド番号間違えていた。resultおかしい セクタサイズがおかしのでresultにデータが入っているみたい
2022年7月9日 READ DATAがおかしい原因がわかった
FDCのTC端子でデータ転送完了を通知しなければ、次のセクタを読み出してしまう。
TC端子はソフトウェアで制御出来ないのでDMAコントローラを操作する必要がある。
DMAコントローラの使い方を調べなきゃならない
2022年7月14日 DMA動いて READ DATA動作確認できた 実アドレスの計算間違えていた
2022年7月14日 PC98互換関数用意できた
2022年8月21日 sectsenseで 2HDの 300/360rpm両対応
2022年8月23日 FDCコマンド発行のリトライ廃止 / リードコマンドでリトライするようにした
2023年2月18日 2HDフォーマット時の 300rpmと360rpmの切り替え
2023年10月22日 readidでリトライしないようにした
2023年10月26日 2DDを読めなかったのを修正
2023年10月29日 2HDの 300RPM/360RPM切り替えで 2PIN MODE SELECT信号が出力されていることを確認
2023年10月29日 シークのタイムアウト判断が短すぎて、シーク完了前にシークエラーとしていたのを直した
2023年10月29日 リキャリブレートでエラーが出ることがあったのでリトライするようにした
2023年11月1日 書き込み系コマンドで変数名間違えてデータバッファを壊したせいでエラーになっていたのを直した。

参考にした資料
・トランジスタ技術 SPECIAL No.11 特集 フロッピ・ディスク・インターフェースのすべて
　　uPD765A FDCの詳しい使い方が載っている。必要な情報がコンパクトにまとまっている。
・SMSC FDC37C669 data sheet
　　PCAT用SUPER I/Oのデータシート。uPD765から増えているレジスタ詳細はコレを見る必要がある
・OADGテクニカル・リファレンス ハードウェア編
・inside X68000 （FDCの使い方が載っている）
・PC-9800シリーズ テクニカルデータブック 1986年版
　　割り込みコントローラとDMAコントローラの使い方が載っている
・LSI C-86 Ver.3.30 試食版 ユーザーズマニュアル
*/

/*
#define DEBUG
*/

#include <machine.h> /* imp, outp */
#include <dos.h>     /* int86 */
#include <stdio.h>
#include "diskbios.h"
#include "diskPCAT.h"
#include "timePCAT.h"

struct DISK_RESULT DISKSTAT;
struct PCATWORK DISKWORK;

/* error code */
enum errorcode
{
    ENoError = 0x00,
    ECorrecteddata = 0x08,
    EIllegalAddres = 0x38,
    EDirectAccessAltTrack = 0x88,
    EDataErr = 0xB8,
    ESeekErr = 0xC8,
    ENotReadAltTrack = 0xD8,
    EDMABoundary = 0x20,
    EEndCylinder = 0x30,
    EOverRun = 0x40,
    ENotReady = 0x60,
    ENotWritable = 0x70,
    EErr0x80 = 0x80,
    ETimeOut = 0x90,
    EIDCRCErr = 0xA0,
    EDataCRCErr = 0xB0,
    ENoData = 0xC0,
    EBadCylinder = 0xD0,
    EMissingID = 0xE0,
    EMissingData = 0xF0
};

/* FDC Register */
/* FDC37C669 data sheet より                     */
/* 0x3f0  R  Status Register A               SRA (PS/2 mode only) */
/* 0x3f1  R  Status Register B               SRB (PS/2 mode only) */
/* 0x3f2 R/W Digital Output Register         DOR */
/* 0x3f3 R/W Tape Drive Register             TSR */
/* 0x3f4  R  Main Status Register            MSR (uPD765) */
/* 0x3f4  W  Data Rate Select Register       DSR */
/* 0x3f5 R/W Data (FIFO)                    FIFO (uPD765) */
/* 0x3f6     Reserved                            */
/* 0x3f7  R  Digital Input Register          DIR */
/* 0x3f7  W  Configuration Contorol Register CCR */

/* 0x03f2 R/W:Digital Output Register DOR */
/*          bit7   : Motor Enable 3 */
/*          bit6   : Motor Enable 2 */
/*          bit5   : Motor Enable 1 */
/*          bit4   : Motor Enable 0 (1:Motor on) */
/*          bit3   : DMA and Interrupu Enable (1:DMA Enable 0:DMA,FINTR出力ハイインピーダンス TC入力無効) */
/*          bit2   : ~Controller Reset (0:Reset / min 100ns) */
/*          bit1,0 : Drive Select (00=Drive 0, 01=Drive 1) */
/* reset : 0x00 */
#define FDCDORP 0x03f2 /* 0x03f2 W:Digital Output Register DOR */
#define fdc_DOR(data) outp(FDCDORP, data)
#define fdc_DORread() inp(FDCDORP)
#if 0
#define fdc_ds0() fdc_DOR(0x14 + 0) /* DMA Disable */
#define fdc_ds1() fdc_DOR(0x24 + 1) /* DMA Disable */
#endif
/* drive select drive:0~3
ドライブを選択してモーターON */
void fdc_drivesel(unsigned char device)
{
    unsigned char motor;
    device &= 0x03; /* 0b00000011 */
    motor = 1 << device;
    motor = motor << 4;
    motor = (1 << 3) | motor; /* bit3=1 DMA Enable */
    motor = motor | (1 << 2); /* bit2=1 resetしない */
    fdc_DOR(motor | device);
}

/* 全ドライブモーターOFF DMA無し */
void fdc_motoroff()
{
    fdc_DOR(1 << 2); /* bit2=1 resetはしない */
}

/* DOR bit3を0にする*/
void fdc_nonDMA()
{
    unsigned char dor;

    dor = fdc_DORread();
    dor &= 0xf7; /* 0b11110111 bit3だけ0 */
    fdc_DOR(dor);
}

#if 0
/* 0x03f3 R/W:Tape Drive Register TDR */
/* このレジスタは、82077ソフトウェアとの互換性のために含まれています。 */
/* このレジスタは このレジスタの内容はデバイス内部では使用されません。 */
#define FDCTDRP 0x03f3 /* 0x03f3 R/W:Tape Drive Register TSR */
#define fdc_TSR_R() inp(FDCTDRP)
#define fdc_TSR_W(data) outp(FDCTDRP, data)
#endif

/* 0x03f4 R:Main Status Register MSR uPD765 */
/*          bit7 : Request for Master  RQM  1:データ転送要求 */
/*          bit6 : Data Input/Output   DIO  1:Read / 0:Write */
/*          bit5 : Non-DMA Mode        NDM  1:Non-DMA */
/*          bit4 : Diskette Controller Busy 1:Busy */
/*          bit3 : Drive 3 Busy */
/*          bit2 : Drive 2 Busy */
/*          bit1 : Drive 1 Busy */
/*          bit0 : Drive 0 Busy (シーク中) */
#define FDCMSRP 0x03f4 /* 0x03f4 R:Main Status Register MSR */
#define fdc_status() inp(FDCMSRP)

/* 0x03f4 W:Data Rate Select Register  DSR */
/*          bit7   : SOFTWARE RESET (DORレジスタのリセットと同じ) 自動クリア */
/*          bit6   : POWER DOWN (1:low power mode ソフトリセット、データレジスタ、メインステータスレジスタにアクセスで復帰) */
/*          bit5   : UNDEFINED (0) */

/*          bit4-2 : PRE-COMP 書き込み補償 */
/* PRECOMP */
/*     111   0.00ns - Disabled */
/*     001  41.67ns */
/*     010  83.34ns */
/*     011 125.00ns */
/*     100 166.67ns */
/*     101 208.33ns */
/*     110 250.00ns */
/*     000 Default 2Mbps:125ns 1Mbps:41.67ns 500/300/250kbps:125ns */
/* OADGテクニカルリファレンス 3-105によると、
全てのシリーンダーに対し125nsの書き込み補償が行われる。 */

/*          bit1-0 : Data Rate Select (00=500Kbps, 10=250Kbps) */
/* |DRT |DRS |DATARATE | */
/* | 00 | 00 | 500kbps | 2HD 300RPM */
/* | 00 | 01 | 300kbps | 2D/2DD 300RPMのメディアを 360rpmで読み書きする時 */
/* | 00 | 10 | 250kbps | 2D/2DD 普通360RPM */
/* | 00 | 11 |   1Mbps | */
/* | 01 | 00 | 500kbps | 3-Mode Drive 2HD 360RPM ? */
/* | 01 | 01 | 500kbps | 3-Mode Drive 2HD 360RPM ? */
/* | 01 | 10 | 250kbps | 3-Mode Drive 2D/2DD ? */
/* | 01 | 11 |   1Mbps | 3-Mode Drive */
/* | 10 | 00 | 500kbps | 2M TAPE */
/* | 10 | 01 |   2Mbps | 2M TAPE */
/* | 10 | 10 | 250kbps | 2M TAPE */
/* | 10 | 11 |   1Mbps | 2M TAPE */
/* reset後 : 0x02 (250kbps) */
#define FDCDSRP 0x3f4 /* W:Data Rate Select Register  DSR */
#define fdc_DSR(data) outp(FDCDSRP, data)

/* 0x04f5 R/W:Data Register uPD765 */
/* uPD765には無い 16byteのFIFOバッファがある */
/* リセット後は FIFO無効 Configure command (FDCコマンド)で変更可能 */
#define FDCRWRP 0x03f5 /* 0x04f5 R/W:Data Register */
#define fdc_data_R() inp(FDCRWRP)
#define fdc_data_W(data) outp(FDCRWRP, data)

/* 0x04f7 R:Digital Input Register  DIR */
/*          bit7 : ~Diskette Change */
#define FDCDIRP 0x04f7 /* 0x04f7 R:Digital Input Register  DIR */
#define fdc_DIR_dchg (1 << 7)
#define fdc_DIR() inp(FDCDIRP)
/* 0x04f7 W:Configuration Control Register CCR */
/*          bit 1,0 : DRATE SEL DSRと組み合わせてデータレートを設定 */
/*                    3モードドライブで 2HDの300/360rpm切り替え */
/*                    DENSELピンは、ハードウェアリセット後にHighに設定される */
#define fdc_CCR(data) outp(FDCDIRP, data)
#define fdc_DRT(data) outp(FDCDIRP, data)

/* fdc_setdatarate
引数 0b0000****
           ||++- DSR
           ++--- DRT
0000 0:500Kbps 2PIN MODE SELECT H
0001 1:300Kbps 2PIN MODE SELECT L
0010 2:250Kbps 2PIN MODE SELECT L
0011 3:1Mbps ?
0100 4:3mode 500kbps 2PIN MODE SELECT H
0101 5:3mode 500Kbps 2PIN MODE SELECT L (VIA C3のマザーボードは300Kbpsだった。他のマザーボードでもチェックする)
0110 6:3mode 250Kbps 2PIN MODE SELECT L
0111 7:3mode 1Mbps ? */
void fdc_setdatarate(unsigned char drtdsr)
{
    drtdsr &= 0x0f; /* 下位4ビットだけにする */
    fdc_DRT(drtdsr >> 2);
    fdc_DSR(0x03 & drtdsr);
}

/* 500Kbps 2HD 1.44MB */
/* 2PIN MODE SELECT H */
void fdc_500kbps()
{
    fdc_setdatarate(0);
    DISKWORK.F2HD360 = MODE300;
}

/* 300Kbps 300rpmのメディアを 360rpmで読み書きする時 */
/* 2PIN MODE SELECT L */
void fdc_300kbps()
{
    fdc_setdatarate(1);
}

/* 250Kbps 2DD/2D */
/* 2PIN MODE SELECT L */
void fdc_250kbps()
{
    fdc_setdatarate(2);
}

/* 1000Kbps 2ED */
/* 2PIN MODE SELECT H */
void fdc_1000kbps()
{
    fdc_setdatarate(3);
}

/* 3mode 500Kbps 2HD */
/* 2PIN MODE SELECT H */
void fdc_3mode500kbps()
{
    fdc_setdatarate(4);
    DISKWORK.F2HD360 = MODE300;
}

/* 3mode 360RPMドライブで2DD読めるのでたぶん300kbps */
/* 2PIN MODE SELECT L  */
void fdc_3mode500kbps_360rpm()
{
    fdc_setdatarate(5);
    DISKWORK.F2HD360 = MODE360;
}

/* 3mode 250Kbps 2DD/2D */
/* 2PIN MODE SELECT L */
void fdc_3mode250kbps()
{
    fdc_setdatarate(6);
}

/* 3mode 1000Kbps 2ED */
/* 2PIN MODE SELECT H */
void fdc_3mode1000kbps()
{
    fdc_setdatarate(7);
}
/*
元々 8インチが 360rpm
2D/2DD が300rpm（PC98は360rpmで300bps ディスク面上では互換）
5インチ 2HDがPC98もPCATも 8インチと互換で 360rpm 1.2MB
3.5インチ 2HD PCAT 300rpm 1.4MB
3.5インチ 2HD PC98 360rpm 1.4MB 5インチと同じ
*/

/*ディスクチェンジ信号の取得*/
unsigned char fdc_diskchange()
{
    if (0 == (fdc_DIR() & fdc_DIR_dchg))
        return TRUE; /* ~Diskette Change 0:TRUE */
    else
        return FALSE;
}

void fdc_st0(unsigned char device, unsigned char value)
{
    switch (device & 0x03)
    {
    case 0:
        DISKSTAT.D0.ST0 = value;
        break;
    case 1:
        DISKSTAT.D1.ST0 = value;
        break;
    case 2:
        DISKSTAT.D2.ST0 = value;
        break;
    case 3:
        DISKSTAT.D3.ST0 = value;
        break;
    }
}

void fdc_st1(unsigned char device, unsigned char value)
{
    switch (device & 0x03)
    {
    case 0:
        DISKSTAT.D0.ST1 = value;
        break;
    case 1:
        DISKSTAT.D1.ST1 = value;
        break;
    case 2:
        DISKSTAT.D2.ST1 = value;
        break;
    case 3:
        DISKSTAT.D3.ST1 = value;
        break;
    }
}

void fdc_st2(unsigned char device, unsigned char value)
{
    switch (device & 0x03)
    {
    case 0:
        DISKSTAT.D0.ST2 = value;
        break;
    case 1:
        DISKSTAT.D1.ST2 = value;
        break;
    case 2:
        DISKSTAT.D2.ST2 = value;
        break;
    case 3:
        DISKSTAT.D3.ST2 = value;
        break;
    }
}

void fdc_stC(unsigned char device, unsigned char value)
{
    switch (device & 0x03)
    {
    case 0:
        DISKSTAT.D0.C = value;
        break;
    case 1:
        DISKSTAT.D1.C = value;
        break;
    case 2:
        DISKSTAT.D2.C = value;
        break;
    case 3:
        DISKSTAT.D3.C = value;
        break;
    }
}

void fdc_stH(unsigned char device, unsigned char value)
{
    switch (device & 0x03)
    {
    case 0:
        DISKSTAT.D0.H = value;
        break;
    case 1:
        DISKSTAT.D1.H = value;
        break;
    case 2:
        DISKSTAT.D2.H = value;
        break;
    case 3:
        DISKSTAT.D3.H = value;
        break;
    }
}

void fdc_stR(unsigned char device, unsigned char value)
{
    switch (device & 0x03)
    {
    case 0:
        DISKSTAT.D0.R = value;
        break;
    case 1:
        DISKSTAT.D1.R = value;
        break;
    case 2:
        DISKSTAT.D2.R = value;
        break;
    case 3:
        DISKSTAT.D3.R = value;
        break;
    }
}

void fdc_stN(unsigned char device, unsigned char value)
{
    switch (device & 0x03)
    {
    case 0:
        DISKSTAT.D0.N = value;
        break;
    case 1:
        DISKSTAT.D1.N = value;
        break;
    case 2:
        DISKSTAT.D2.N = value;
        break;
    case 3:
        DISKSTAT.D3.N = value;
        break;
    }
}

void fdc_stNCN(unsigned char device, unsigned char value)
{
    switch (device & 0x03)
    {
    case 0:
        DISKSTAT.D0.NCN = value;
        break;
    case 1:
        DISKSTAT.D1.NCN = value;
        break;
    case 2:
        DISKSTAT.D2.NCN = value;
        break;
    case 3:
        DISKSTAT.D3.NCN = value;
        break;
    }
}

/* ワークエリア初期化 */
void fdc_workinit(unsigned char unit)
{
    fdc_st0(unit, 0);
    fdc_st1(unit, 0);
    fdc_st2(unit, 0);
    fdc_stC(unit, 0);
    fdc_stH(unit, 0);
    fdc_stR(unit, 1);
    fdc_stN(unit, fdc_N_default);
    fdc_stNCN(unit, 0);
    DISKWORK.SEEK2D = MODE2DD;
    DISKWORK.F2HD360 = MODE300;
    DISKWORK.TIMEOUT = FDCTIMEOUT;
    DISKWORK.SEEKTIMEOUT = FDCSEEKTIMEOUT;
    DISKWORK.CMDRETRY = FDCCMDRETRY;
    DISKWORK.RETRY = FDCRETRY;
}

/* fdc読み込み可能か */
unsigned char fdc_rRQM()
{
    unsigned char status;
    status = fdc_status();
    if ((0 != (status & (1 << 7)))     /* 0b10000000 転送要求 */
        && (0 != (status & (1 << 6)))  /* 0b01000000 bit6=1 read */
        /* && (0 == (status & 0x1f))*/ /* 0b00011111 not bysy */
        /* busyビット見ない コマンド実行中はbusyビット1 */
    )
    {
        return TRUE;
    }
    return FALSE;
}

/* fdc読み込み可能になるまで待つ タイムアウトあり
TRUE:データあり / FALSE:time out */
unsigned char fdc_readRQM()
{
    unsigned short starttime;
    starttime = timer_get16();
    while (FALSE == fdc_rRQM()) /* 読み込み可能になるまで待つ */
    {
        if (timeout_ms(DISKWORK.TIMEOUT, starttime))
            return FALSE; /* time out */
    }
    return TRUE;
}

/* FDCがデータ読み取り要求を出していれば書き込み可能になるまで読み捨てる
TRUE=データが無かった / FALSE=データが有った */
unsigned char fdc_writable()
{
    unsigned short starttime;

    /*fdc_nonDMA();*/ /* DMA無効 */
    starttime = timer_get16();

    if (FALSE == fdc_rRQM()) /* 読み込み可能か タイムアウトなし */
        return TRUE;         /* データなし */
    while (fdc_rRQM())       /*データが有る間*/
    {
        if (timeout_ms(DISKWORK.TIMEOUT, starttime))
            break;    /* time out データが無くならない */
        fdc_data_R(); /* データを読み捨てる */
    }
#ifdef DEBUG
    printf(" RDx "); /* 読もうとしたけどFDCがデータを出していた */
#endif
    return FALSE; /* データが残っていた */
}

/* fdc書き込み可能か
bysychk 0:busyチェックしない 1:busyチェックする
fdcにコマンド最初の1バイトを書き込むと FDC Busy ビットが 1になるので
Busyビットは最初の 1バイトを書き込む時以外はチェックしない */
unsigned char fdc_wRQM(unsigned char BusyChk)
{
    unsigned char status;
    status = fdc_status();
    if ((0 != (status & (1 << 7)))     /* 0b10000000 転送要求 */
        && (0 == (status & (1 << 6)))) /* 0b01000000 bit6=0 write */
    {
        if (0 != BusyChk) /* busyチェックする */
        {
            if (0 == (status & 0x1f)) /* 0b00011111 not bysy */
                return TRUE;
            else
                return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

/* fdc書き込み可能まで待つ タイムアウトあり
TRUE:転送要求あり / FALSE:time out
bysychk 0:busyチェックしない 1:busyチェックする
fdcにコマンド最初の1バイトを書き込むと FDC Busy ビットが 1になるので
Busyビットはコマンド最初の 1バイトを書き込む時以外はチェックしない */
unsigned char fdc_writeRQM(unsigned char BusyChk)
{
    unsigned short starttime;
    fdc_writable(); /* データが残っていれば読み捨てる */
    starttime = timer_get16();
    while (FALSE == fdc_wRQM(BusyChk)) /* 書き込み可能になるまで待つ */
    {
        if (timeout_ms(DISKWORK.TIMEOUT, starttime))
            return FALSE; /* time out */
    }
    return TRUE;
}

/* ステータスをチェックしながらデータを書き込む リトライなし busyチェックなし */
unsigned char fdc_write(unsigned char *buff, unsigned short size)
{
    /*fdc_nonDMA();*/ /* DMA無効 */

    for (; 0 != size; size--)
    {
        if (FALSE == fdc_writeRQM(0)) /* 書き込み可能になるまで待つ busyチェックなし */
        {
#ifdef DEBUG
            printf(" WTOUT:%d ", size);
#endif
            return FALSE; /* time out */
        }
        fdc_data_W(*buff);
        buff++;
    }
    return TRUE;
}

/* ステータスをチェックしながらデータを書き込む リトライあり
戻り値 0:異常終了 */
unsigned char fdc_datawrite(unsigned char *buff, unsigned short size)
{
    unsigned char count;
    for (count = DISKWORK.CMDRETRY + 1; 0 != count; count--) /*RETRY +1しないと 0の時一度も実行されない*/
    {
        if (fdc_write(buff, size))
            return TRUE;
#ifdef DEBUG
        printf(" WRT "); /* 書き込みでリトライした*/
#endif
    }
    return FALSE;
}

/* ステータスをチェックしながらデータを読み込む リトライなし */
unsigned char fdc_read(unsigned char *buff, unsigned short size)
{
    /*fdc_nonDMA();*/ /* DMA無効 */

    for (; 0 != size; size--)
    {
        if (FALSE == fdc_readRQM())
            return FALSE; /* time out */
        *buff = fdc_data_R();
        buff++;
    }
    return TRUE;
}

/* ステータスをチェックしながらデータを読み込む リトライあり
戻り値 0:異常終了 */
unsigned char fdc_dataread(unsigned char *buff, unsigned short size)
{
    unsigned char count;
    for (count = DISKWORK.CMDRETRY + 1; 0 != count; count--) /*RETRY +1しないと 0の時一度も実行されない*/
    {
        if (fdc_read(buff, size))
            return TRUE;
#ifdef DEBUG
        printf(" RRT "); /* 読み込みでリトライした */
#endif
    }
    return FALSE;
}

/* FDC CONFIGUREコマンド
戻り値 0:異常終了 */
/*
Configureコマンドは、FDCの特別な機能を選択するために発行する。
FDCのデフォルト値がシステム要件を満たしている場合、Configureコマンドを発行する必要は無い。

Configureデフォルト値：
EIS     - "0" インプライドシークなし
EFIFO   - "1" FIFO無効
POLL    - "0" ポーリング有効
FIFOTHR - "0" FIFOしきい値を1バイトに設定
PRETRK  - "" プリコンペンセーションをトラック0に設定

EIS - インプライド・シークを有効にする。
"1"に設定すると、FDCは読み取りまたは書き込みコマンドを実行する前にシーク動作を実行する。
デフォルトはシークなし。

EFIFO - "1"はFIFOを無効にする（デフォルト）。
これは、データ転送がバイト単位で要求されることを意味する。デフォルトは "1"で、FIFOは無効である。

POLL - ドライブのポーリングを無効にする。
デフォルトは "0"で、ポーリングは有効。有効にすると、リセット後に1つの割り込みが生成される。
ヘッドがロードされ、ヘッド・アンロード遅延が経過していない間は、ポーリングは実行されない。

FIFOTHR - リードまたはライト・コマンドの実行フェーズにおけるFIFOスレッショルド。
1～16バイトの範囲でプログラム可能。
デフォルトは"0"に設定されている。"0"で1バイト、"F"で16バイトが選択される。

PRETRK - プリコンペンセーション（書き込み補償？）開始トラック番号。
トラック0から255までプログラム可能。
デフォルトはトラック0。"00"でトラック0、"FF"でトラック255を選択。
*/
unsigned char fdc_configure()
{
    unsigned char buff[4];
    buff[0] = 0x13;        /* 0b00010011 */
    buff[1] = 0x00;        /* 0b00000000 */
    buff[2] = FDC_CONFIG1; /* 0b00001111 */
                           /*    |||++++- FITOTHR */
                           /*    ||+----- POLL    */
                           /*    |+------ EFIFO   */
                           /*    +------- EIS     */
    buff[3] = FDC_CONFIG2; /* PRETRK  */

    return fdc_datawrite(buff, 4);
}

/* FDC SPECIFYコマンド
戻り値 0:異常終了 */
unsigned char fdc_specify()
{
    unsigned char buff[3];
    buff[0] = 0x03; /* 0b00000011 */
    buff[1] = FDC_SPECIFY1;
    buff[2] = FDC_SPECIFY2;

    return fdc_datawrite(buff, 3);
}

/* SENCE INTERRUPT STATUS
シークコマンド後は SENCE INTERRUPT STATUSコマンド実行まで
ステータスレジスタのseek bitが 1になっているので注意
 */
unsigned char fdc_sensint(unsigned char device)
{
    unsigned char buff[2];
    buff[0] = 0x08; /* 0b00001000 SENSE INTERRUPU STATUS */
    if (FALSE == fdc_datawrite(buff, 1))
        return FALSE;
    if (FALSE == fdc_dataread(buff, 2)) /* ST0, PCN */
        return FALSE;
    fdc_st0(device, buff[0]);
    fdc_stNCN(device, buff[1]);

#ifdef DEBUG
    printf(" sensint ST0 %0x", buff[0]);
    printf(" NCN %0x\r\n", buff[1]);
#endif

    if (0x00 != (0xc0 & buff[0]))
        return FALSE; /*異常終了*/
    return TRUE;
}

/* ST0の seek endビットが立つまで待つ */
unsigned char fdc_waitseekend(unsigned char device)
{
    unsigned short starttime;
    unsigned char st0;

    st0 = 0;
    starttime = timer_get16();

    while (0 == ((1 << 5) & st0))
    {
        if (FALSE == fdc_sensint(device))
            return FALSE; /* st0読めなかった */

        switch (0x03 & device)
        {
        case 0:
            st0 = DISKSTAT.D0.ST0;
            break;
        case 1:
            st0 = DISKSTAT.D1.ST0;
            break;
        case 2:
            st0 = DISKSTAT.D2.ST0;
            break;
        case 3:
            st0 = DISKSTAT.D3.ST0;
            break;
        }

        if (timeout_ms(DISKWORK.SEEKTIMEOUT, starttime))
            return FALSE; /* 時間が経過しても seek endビットが立たない */
    }
    return TRUE; /* seek endビットが立った */
}

/* FDCメインステータスレジスタのシークビットが立つまで待つ */
void fdc_seekwait()
{
    unsigned short starttime;

    starttime = timer_get16();
    while (0 == (0x0f & fdc_status())) /* ビットが立つまで待つ */
    {
        if (timeout_ms(DISKWORK.TIMEOUT, starttime))
            break; /* time out */
    }
}

/* 転送レートを設定してドライブパラメータのインデックスを返す
引数
0b*00000**
  |     ++- drive
  +-------- 0:2DD 1:2HD
戻り値
0b00000***
       ||+- 0:2DD 1:2HD
       |+-- 0:360rpm 1:300rpm
       +--- 0:R/W 1:format
 */
unsigned char fdc_getfmt(unsigned char device)
{
    /*2023年10月26日 条件間違えていて2DDに到達不能だったので2DD読めなかった。直した。*/
    if (0 != (0x80 & device))
    {
        /* 2HD */
        if (MODE300 == DISKWORK.F2HD360)
        {
            /* 300rpm 1.44MB */
            /*puts("1.44MB");*/
            fdc_500kbps();
            /*fdc_3mode500kbps();*/        /*1.44MB読める*/
            /*fdc_3mode500kbps_360rpm();*/ /*1.44MBも1.2MBも2DDも読めない。FDCの設定は変わっているようだ*/
            return 0x03;                   /* 0b00000011 */
        }
        else
        {
            /* 360rpm 1.2MB */
            /*puts("1.2MB");*/
            fdc_3mode500kbps_360rpm();
            /*fdc_3mode500kbps();*/
            return 0x01; /* 0b00000001 */
        }
    }
    /* 2DD */
    if (MODE300 == DISKWORK.F2HD360)
    {
        /*puts("2DD");*/
        fdc_250kbps();
        return 0x02; /* 0b00000010 */
    }
    fdc_300kbps();
    return 0x02; /* 0b00000010 */
}

/*
トラックあたりのセクタ数
N 0~7

format
0:2DD 360rpm data R/W
1:2HD 360rpm data R/W
2:2DD 300rpm data R/W
3:2HD 300rpm data R/W
0b00000000
        |+- 0:2DD 1:2HD
        +-- 0:360rpm 1:300rpm */
unsigned char fdc_getEOT(unsigned char MF, unsigned char N, unsigned char format)
{
    /*
    FDC37C669 PC98/99 SUPER I/O Floppy Disk Controler DATA SHEET
    Format A Track コマンドより
             sector N SC GPL1 GPL2
    5.25  FM    128 0 12   07   09   5インチ2HDは360rpm 2D/2DDは300rpm
                128 0 10   10   19
                512 2 08   18   30
               1024 3 04   46   87
               2048 4 02   C8   FF
               4096 5 01   C8   FF
         MFM    256 1 12   0A   0C
                256 1 10   20   32
                512 2 09   2A   50
               1024 3 04   80   F0
               2048 4 02   C8   FF
               4096 5 01   C8   FF
    3.5   FM    128 0 0F   07   1B   3.5インチは300rpm
                256 1 09   0F   2A
                512 2 05   1B   3A
         MFM    256 1 0F   0E   36
                512 2 09   1B   54
               1024 3 05   35   74

    PC9801/X68000 2HD 360rpm 2D/2DD 300rpm 5インチも3.5インチも回転数同じ
         MFM    512 2 0F
               1024 3 08

    TRACK FORMAT
        INDEX                 SECTOR x SC
        GAP4a SYNK IAM GAP1 | SYNK IDAM C H R N CRC GAP2 SYNK DAM DATA CRC GAP3 | GAP4b
    FM
    MFM
    */
    unsigned char ret[4][9] =
        /*   0     1     2     3     4     5     6     7     8   N */
        {{0x12, 0x0f, 0x08, 0x04, 0x02, 0x01, 0x01, 0x01, 0x01},  /* FM 360rpm */
         {0x12, 0x12, 0x09, 0x04, 0x02, 0x01, 0x01, 0x01, 0x01},  /* MFM360rpm */
         {0x0f, 0x09, 0x05, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01},  /* FM 300rpm */
         {0x0f, 0x0f, 0x09, 0x05, 0x03, 0x01, 0x01, 0x01, 0x01}}; /* MFM300rpm */

    if (N > 7)
        N = 7;

    format &= 0x03; /* 0b00000011 */

    /* 2DD/2HD */
    if (FD2HD != (format & FD2HD))
        N++; /* 2D/2DD は 容量半分 */

    /* FM/MFM */
    if (FDFM == MF)
        format &= ~0x01; /* bit0=0 */
    else
        format |= 0x01; /* bit0=1 */

    return ret[format][N];
}

/* GAP3 length
N 0~7
format
0:2DD 360rpm data R/W
1:2HD 360rpm data R/W
2:2DD 300rpm data R/W
3:2HD 300rpm data R/W
4:2DD 360rpm format
5:2HD 360rpm format
6:2DD 300rpm format
7:2HD 300rpm format
0b00000000
       ||+- 0:2DD 1:2HD
       |+-- 0:360rpm 1:300rpm
       +--- 0:R/W 1:format
2Dと2DDはシリンダ数が違うだけでパラメータは同じ */
unsigned char fdc_getGPL(unsigned char MF, unsigned char N, unsigned char format)
{
    unsigned char ret[8][9] =
        {
            {0x07, 0x07, 0x10, 0x18, 0x46, 0xC8, 0xC8, 0xC8, 0xC8},  /* 2DD360rpm data R/W */
            {0x0a, 0x0a, 0x20, 0x2a, 0x80, 0xC8, 0xC8, 0xC8, 0xC8},  /* 2HD360rpm data R/W */
            {0x07, 0x07, 0x0f, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b},  /* 2DD300rpm data R/W */
            {0x0e, 0x0e, 0x0e, 0x1b, 0x35, 0x35, 0x35, 0x35, 0x35},  /* 2HD300rpm data R/W */
            {0x09, 0x09, 0x19, 0x30, 0x87, 0xff, 0xff, 0xff, 0xff},  /* 2DD360rpm format */
            {0x0c, 0x0c, 0x32, 0x50, 0xf0, 0xff, 0xff, 0xff, 0xff},  /* 2HD360rpm format */
            {0x1b, 0x1b, 0x2a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a},  /* 2DD300rpm format */
            {0x36, 0x36, 0x36, 0x57, 0x74, 0x74, 0x74, 0x74, 0x74}}; /* 2HD300rpm format */
    /* 2DD 300rpm 640/720KB */
    /* 2HD PCAT 3.5inch 300rpm 1.4MB */
    /* 2HD PCAT   5inch 360rpm 1.2MB */
    /* 2HD PC98 3.5inch 360rpm 1.2MB */
    /* 2HD PC98   5inch 360rpm 1.2MB */
    /* 元々 8inch が360rpm */

    /* PC98 BIOSのテーブル / テクニカルハンドブック BIOS編 P236 にあり*/

    if (N > 7)
        N = 7;
    if (FDFM == MF) /* FM */
        N++;

    format &= 0x07; /* 0b00000111 */
    return ret[format][N];
}

/* ステータスレジスタからエラーコードを得る
戻り値 エラーコード */
/* result[0]=st0 result[1]=st1 result[2]=st2 */
unsigned char fdc_errcode(unsigned char *result)
{
    if (0 == (0xc0 & *result))
        return ENoError;
    if (0 != ((1 << 3) & *result))
        return ENotReady;

    result++;
    if (0 != ((1 << 0) & *result))
        return EMissingID;
    if (0 != ((1 << 1) & *result))
        return ENotWritable;
    if (0 != ((1 << 2) & *result))
        return ENoData;
    if (0 != ((1 << 4) & *result))
        return EOverRun;
    if (0 != ((1 << 5) & *result))
        return EDataErr;
    if (0 != ((1 << 7) & *result))
        return EEndCylinder;

    result++;
    if (0 != ((1 << 0) & *result))
        return ENoData;
    if (0 != ((1 << 1) & *result))
        return EBadCylinder;
    if (0 != ((1 << 4) & *result))
        return EBadCylinder;
    if (0 != ((1 << 5) & *result))
        return EDataCRCErr;

    return EErr0x80;
}

/*  BIOSでFDC初期化 */
unsigned char fdc_biosreset(char drive)
{
    union REGS reg;
    unsigned char ret;

    drive = drive & 0x03;
    reg.h.ah = 0x00;
    reg.h.dl = drive; /* DRIVE 0=A: 1=B: 2=C: 3=D: ~ */
    int86(0x13, &reg, &reg);
    ret = reg.h.ah;

    fdc_workinit(0);
    fdc_workinit(1);
    fdc_workinit(2);
    fdc_workinit(3);
    /*   fdc_configure();
       fdc_specify();*/
    return ret;
}

/* soft reset して設定 DSR(0x3f4) bit7
FIFO有効 16byte ポーリング有効
ステップレート／ヘッドロード・アンロード／ポーリングモード
ワークエリアはそのまま */
void fdc_softreset()
{
    fdc_DSR(0x80); /* reset リセット後自動でクリアされる*/
                   /* リセットできた
                   コマンド途中でリセットかけて、ステータスレジスタ 0x80になった */

    wait_ms(100);
    DMAfdcmask();    /* DMAコントローラ ch2 (FDC) マスク */
    PICfdcmask(1);   /* FDC割り込みをマスク */
    fdc_configure(); /* FIFO有効 16byte */
    fdc_specify();   /* ステップレート／ヘッドロード・アンロード／ポーリングモード */
}

/* fdc reset して設定 (DOR reset)
FIFO有効 16byte ポーリング有効
ステップレート／ヘッドロード・アンロード／ポーリングモード
ワークエリアはそのまま */
void fdc_reset()
{
    unsigned char dor;

    dor = fdc_DORread();
    dor &= 0xfb;  /* reset bitだけゼロ 0b11111011 */
    fdc_DOR(dor); /* bit2   : ~Controller Reset (0:Reset) */

    wait_ms(100);
    dor |= (1 << 2); /* reset bitを1 */
    fdc_DOR(dor);
    wait_ms(100);
    /* リセットできた
    コマンド途中でリセットかけて、ステータスレジスタ 0x80になった */
    DMAfdcmask();    /* DMAコントローラ ch2 (FDC) マスク */
    PICfdcmask(1);   /* FDC割り込みをマスク */
    fdc_configure(); /* FIFO有効 16byte */
    fdc_specify();   /* ステップレート／ヘッドロード・アンロード／ポーリングモード */
}

/* PC-9801 DISK BIOS 互換関数 */
/* INITIALIZE / FDCとBIOSのワークエリアの初期化
device bit7 0:2DD 1:2HD
       bit1 unit
       bit0 unit
戻り値 0:異常終了 0以外:正常終了 */
unsigned char diskinit(unsigned char device)
{
    fdc_workinit(0);
    fdc_workinit(1);
    fdc_workinit(2);
    fdc_workinit(3);
    /*fdc_biosreset(0);*/
    /*fdc_softreset();*/
    fdc_reset();
    return TRUE;
}

/* DISKMODE 動作モードの設定 デバイスタイプユニットが2DDの時のみ有効
48tpi(2D)/96tpi(2DD)の切り替え 2HDの時は無意味
戻り値 0:異常終了 0以外:正常終了
###PC/ATでは機能無し### */
unsigned char drvmode(unsigned char device, /* デバイス番号 0～3 */
                      unsigned char mode)   /* モード MODE2D/MODE2DD */
{
    /* 2Dの時はシーク2倍にするようにワークエリアセット ワークエリアはドライブごとに確保するのやめた */
    DISKWORK.SEEK2D = mode;
    return TRUE;
}

/* DISKMODE 動作モードの設定 デバイスタイプユニットが2DDの時のみ有効
48tpi(2D)/96tpi(2DD)の切り替え 2HDの時は無意味
mode ----1111  0:2D 1:2DD
         |||+- DRIVE0
         ||+-- DRIVE1
         |+--- DRIVE2
         +---- DRIVE3
戻り値 0:異常終了 0以外:正常終了
###PC/ATでは機能無し### */
unsigned char diskmode(unsigned char mode)
{
    if (0 == ((1 << 0) & mode))
        drvmode(0, MODE2D);
    else
        drvmode(0, MODE2DD);

    if (0 == ((1 << 1) & mode))
        drvmode(1, MODE2D);
    else
        drvmode(1, MODE2DD);

    if (0 == ((1 << 2) & mode))
        drvmode(2, MODE2D);
    else
        drvmode(2, MODE2DD);

    if (0 == ((1 << 2) & mode))
        drvmode(2, MODE2D);
    else
        drvmode(2, MODE2DD);

    return TRUE;
}

/* SENSE デバイスの状態を調べる
戻り値 0:異常終了 0以外:正常終了
device 0b10010000
         ||||  ++- Unit Address 0~3
         |+++----- I/F選択 000:HDD 001:2HD/2DD両用 111:2DD
         +-------- 0:2DD 1:2HD
*errcode ST3 ドライブの状態を得られる
 0b00000000
   ||||||++- US1,0
   |||||+--- HD
   ||||+---- TS Tow Side
   |||+----- T0 Track 0
   ||+------ RY READY
   |+------- WP Write Protect
   +-------- FT Fault */
unsigned char disksense(unsigned char device, unsigned char *errcode)
{
    unsigned char buff[2];
    buff[0] = 0x04;          /* 0b00000100 SENSE DEVICE STATUS */
    buff[1] = device & 0x07; /* 0b-----000 */
                             /*        |++- US1 US0 */
                             /*        +--- HD HEAD */
                             /* ドライブの端子状態を調べるだけなので、2DD/2HD・ヘッド番号関係無い */
    fdc_drivesel(device);
    if (FALSE == fdc_writeRQM(1)) /* FDC busy check */
        return FALSE;
    if (FALSE == fdc_datawrite(buff, 2))
    {
        *errcode = ETimeOut;
        return FALSE;
    }
    if (FALSE == fdc_dataread(buff, 1)) /* ST3 */
    {
        *errcode = ETimeOut;
        return FALSE;
    }
    /**errcode = buff[0];*/ /* ST3 ドライブの状態を得られる */
    /* 0b00000000 */
    /*   ||||||++- US1,0 */
    /*   |||||+--- HD */
    /*   ||||+---- TS Tow Side */
    /*   |||+----- T0 Track 0 /
    /*   ||+------ RY READY */
    /*   |+------- WP Write Protect */
    /*   +-------- FT Fault*/
    *errcode = buff[0];
    return TRUE;
}

/* RECALIBRATE ヘッドをトラック0に移動する リトライ無し
戻り値 0:異常終了 0以外:正常終了 */
unsigned char recalibrate(unsigned char device)
{
    unsigned char buff[2];

    fdc_drivesel(device);
    buff[0] = 0x07;               /* 0b00000111 RECALIBRATE */
    buff[1] = device & 0x03;      /* 0b------00 */
                                  /*         ++- US1 US0 */
    if (FALSE == fdc_writeRQM(1)) /* FDC busy check */
        return FALSE;
    if (FALSE == fdc_datawrite(buff, 2))
        return FALSE;
#ifdef DEBUG
    printf("fdc status reg %0x\r\n", fdc_status());
#endif
    wait_ms(500);
    if (!PICfdcwait()) /* FDC割り込み要求があるまで待つ */
    {
#ifdef DEBUG
        printf(" #NO IRQ# ");
#endif
    }
    /*wait_ms(FDCRWAIT);*/
    fdc_seekwait(); /*メインステータスレジスタのシークビットが立つまで待つ*/
#ifdef DEBUG
    printf("fdc status reg %0x\r\n", fdc_status());
#endif
    if (FALSE == fdc_waitseekend(device)) /* sensintコマンドを発行してシークが終了するまで待つ*/
        return FALSE;                     /* st0の seek endビットが立たない */
    return TRUE;
}

/* RECALIBRATE ヘッドをトラック0に移動する リトライ有り
戻り値 0:異常終了 0以外:正常終了 */
unsigned char track00(unsigned char device)
{
    unsigned char count;
    for (count = DISKWORK.RETRY + 1; 0 != count; count--)
    {
        if (recalibrate(device))
            return TRUE;
    }
    return FALSE;
}

/* SEEK 指定シリンダにヘッドを移動する
戻り値 0:異常終了 0以外:正常終了
C:シリンダ番号 */
unsigned char seek(unsigned char device, unsigned char C)
{
    unsigned char buff[3];

    /* 2DDドライブで2Dメディアを読む時に2倍シークする処理を書く */

    fdc_drivesel(device);
    device &= 0x07;
    buff[0] = 0x0f; /* 0b00001111 SEEK */
    buff[1] = device;
    buff[2] = C;
    if (FALSE == fdc_writeRQM(1)) /* FDC busy check */
        return FALSE;
    if (FALSE == fdc_datawrite(buff, 3))
        return FALSE;
#ifdef DEBUG
    printf("seek.. fdc status reg %0x\r\n", fdc_status());
#endif
    wait_ms(500);
    if (!PICfdcwait()) /* FDC割り込み要求があるまで待つ */
    {
#ifdef DEBUG
        printf(" #NO IRQ# ");
#endif
    }
    /*wait_ms(FDCSEEKWAIT);*/
    fdc_seekwait();
#ifdef DEBUG
    printf("seek end: fdc status reg %0x\r\n", fdc_status());
#endif
    if (FALSE == fdc_waitseekend(device)) /* sensintコマンドを発行してシークが終了するまで待つ*/
        return FALSE;                     /* st0の seek endビットが立たない */
    return TRUE;
}

/* READ ID 現在のトラックからIDを読む リトライ無し
buffに C H R N の順に4バイト書き込む
戻り値 0:異常終了 0以外:正常終了
device bit7 0:2DD 1:2HD / bit1 bit0 drive
MF:FDFM/FDMFM H:ヘッド番号 */
unsigned char readid(unsigned char device, unsigned char MF,
                     unsigned char H,
                     unsigned char *buff)
/*unsigned char rreadid(unsigned char device, unsigned char MF,
                      unsigned char H,
                      unsigned char *buff)*/
{
    unsigned char cmd[7];

    fdc_drivesel(device);
    fdc_getfmt(device); /* 転送レート指定 */
    cmd[0] = MF | 0x0a; /* 0b0x001010 */
                        /*    +------- MF 0:FM 1:MFM */
    cmd[1] = (device & 0x03) | (H << 2);

    buff[0] = ETimeOut;
    if (FALSE == fdc_writeRQM(1)) /* FDC busy check */
        return FALSE;
    buff[0] = EMissingID;
    if (FALSE == fdc_datawrite(cmd, 2)) /* C-PHASE (E-PHASE 無し) */
        return FALSE;
    if (FALSE == fdc_dataread(cmd, 7)) /* R-PHASE */
        return FALSE;

    fdc_st0(device, cmd[0]);
    fdc_st1(device, cmd[1]);
    fdc_st2(device, cmd[2]);
    fdc_stC(device, cmd[3]);
    fdc_stH(device, cmd[4]);
    fdc_stR(device, cmd[5]);
    fdc_stN(device, cmd[6]);
    buff[0] = cmd[3];         /* C */
    buff[1] = cmd[4];         /* H */
    buff[2] = cmd[5];         /* R */
    buff[3] = cmd[6];         /* N */
    if (0 != (0xc0 & cmd[0])) /* ST0 bit7,6がゼロなら正常終了 */
    {
        /*エラーだったら アドレスbuffにエラーコードを書く*/
        *buff = fdc_errcode(cmd);
        return FALSE;
    }
    return TRUE;
}

#if 0
/* READ ID 現在のトラックからIDを読む
buffに C H R N の順に4バイト書き込む
戻り値 0:異常終了 0以外:正常終了
device bit7 0:2DD 1:2HD / bit1 bit0 drive
MF:FDFM/FDMFM H:ヘッド番号 */
unsigned char readid(unsigned char device, unsigned char MF,
                     unsigned char H,
                     unsigned char *buff)
{
    unsigned char count;
    for (count = DISKWORK.RETRY + 1; 0 != count; count--)
    {
        if (rreadid(device, MF, H, buff))
            return TRUE;
    }
    return FALSE;
}
#endif

/* READ DATA 指定した1セクタからデータを読む リトライ無し
戻り値はエラーコード 0:正常終了
device bit7 0:2DD 1:2HD / bit1 bit0 drive
MF FDFM/FDMFM */
unsigned char rreaddata(unsigned char device, unsigned char MF,
                        unsigned char C,
                        unsigned char H,
                        unsigned char R,
                        unsigned char N,
                        unsigned char *buff)
{
    unsigned char result[7];
    unsigned char format;

    /* E-PHASE用 DMA セットアップ I/O -> MEM */
    if (FALSE == DMAset_toMEM(buff, sectlength(N)))
    {
        puts(" DMA - ILLEGAL ADDRESS");
        return EDMABoundary; /* バッファのアドレスが64KB境界を跨いでいる */
    }

#ifdef DEBUG
    printf("   PAGE:%02x ADDR:%04x COUNT:%04x\r\n", DMAgetpage(buff), DMAgetaddress(buff), sectlength(N));
    printf("DMA REG READ / PAGE:%02x ADDR:%04x COUNT:%04x\r\n", DMAreadpagereg(), DMAreadaddrreg(), DMAreadcountreg());
#endif
    fdc_drivesel(device);
    format = fdc_getfmt(device);
#ifdef DEBUG
    printf(" / track param %0x", format);
    printf(" EOT:%d GPL:%d\r\n", fdc_getEOT(MF, N, format), fdc_getGPL(MF, N, format));
#endif
    buff[0] = MF | 0x06; /* 0b0x000110 */
                         /*   ||+------ SK DDAM/DAMスキップ */
                         /*   |+------- MF 0:FM 1:MFM */
                         /*   +-------- MT マルチトラック */
    buff[1] = (device & 0x03) | (H << 2);
    buff[2] = C;
    buff[3] = H;
    buff[4] = R;
    buff[5] = N;
    buff[6] = fdc_getEOT(MF, N, format); /* EOT トラック内の最終セクタ */
    buff[7] = fdc_getGPL(MF, N, format); /* GSL Gap3サイズ */
    buff[8] = 0xff;                      /* DTL Nが 0のときのデータ長
    DTLは0xFFにしておく（トラ技SPECIAL No.11 P48） */

#ifdef DEBUG
    printf("CMD %02x %02x %02x %02x %02x %02x %02x %02x %02x\r\n", buff[0], buff[1], buff[2], buff[3], buff[4], buff[5], buff[6], buff[7], buff[8]);
#endif
    if (FALSE == fdc_writeRQM(1)) /* FDC busy check */
        return ETimeOut;
#ifdef DEBUG
    printf(" start C-PHASE .. ");
#endif
    if (FALSE == fdc_datawrite(buff, 9)) /* C-PHASE */
        return ETimeOut;
#ifdef DEBUG
    printf(" C-PHASE OK ");
#endif
#if 0
    /* FDCのTC端子でデータ転送完了を通知しなければ、次のセクタを読み出してしまう。
    TC端子はソフトウェアで制御出来ないのでDMAコントローラを操作する必要がある。
    DMAコントローラの使い方を調べなきゃならない */
    if (FALSE == fdc_dataread(buff, sectlength(N))) /* E-PHASE */
    {
        /* セクタが見つからないと E-PHASEもR-PHASEも読み出し可能にならない */
        return ENoData;
    }
#endif

    /* E-PHASE */
    /* DMA転送開始前はTCビットが1になっているのでTCビットだけ見ても
    転送終了の判断ができない。割り込み要求を見る */
    /*wait_ms(500);*/

    if (!PICfdcwait()) /* FDC割り込み要求があるまで待つ */
    {
        /* time out */
        puts("FDC:TIMEOUT(READDATA)");
        return ENoData;
    }
#if 0
    starttime = timer_get16();
    while (FALSE == DMAbusy()) /* DMA転送が終了するまで待つ */
    {
#ifdef DEBUG
        printf("@");
#endif
        if (timeout_ms(DISKWORK.TIMEOUT, starttime))
        {
            puts("FDC:TIMEOUT");
            return ENoData;
        }
    }
#endif
#ifdef DEBUG
    printf(" E-PHASE OK %dbyte\r\n", sectlength(N));
    printf("DMA REG READ / PAGE:%02x ADDR:%04x COUNT:%04x\r\n", DMAreadpagereg(), DMAreadaddrreg(), DMAreadcountreg());
#endif

    if (FALSE == fdc_dataread(result, 7)) /* R-PHASE */
    {
        return ENoData;
    }
#ifdef DEBUG
    printf(" R-PHASE OK ");
#endif

    fdc_st0(device, result[0]);
    fdc_st1(device, result[1]);
    fdc_st2(device, result[2]);
    fdc_stC(device, result[3]);
    fdc_stH(device, result[4]);
    fdc_stR(device, result[5]);
    fdc_stN(device, result[6]);
#ifdef DEBUG
    printf("\r\nresult 0:%0x 1:%0x 2:%0x C:%0x H:%0x R:%0x N:%0x\r\n", result[0], result[1], result[2], result[3], result[4], result[5], result[6]);
#endif
    return fdc_errcode(result); /*エラーコード*/
}

/* READ DATA 指定した1セクタからデータを読む
戻り値はエラーコード 0:正常終了
device bit7 0:2DD 1:2HD / bit1 bit0 drive
MF FDFM/FDMFM */
unsigned char readdata(unsigned char device, unsigned char MF,
                       unsigned char C,
                       unsigned char H,
                       unsigned char R,
                       unsigned char N,
                       unsigned char *buff)
{
    unsigned char count, ret;
    for (count = DISKWORK.RETRY + 1; 0 != count; count--)
    {
        ret = rreaddata(device, MF, C, H, R, N, buff);
        if (ENoError == ret)
            break; /* 正常終了なら終了、エラーならリトライ */
    }
    return ret;
}

/* READ DELETED DATA 指定した1セクタからデリーテッドデータを読む リトライ無し
戻り値はエラーコード！ 0:正常終了
device bit7 0:2DD 1:2HD / bit1 bit0 drive
MF FDFM/FDMFM  */
unsigned char rreaddeleted(unsigned char device,
                           unsigned char MF,
                           unsigned char C,
                           unsigned char H,
                           unsigned char R,
                           unsigned char N,
                           unsigned char *buff)
{
    unsigned char result[7];
    unsigned char format;

    /* E-PHASE用 DMA セットアップ I/O -> MEM */
    if (FALSE == DMAset_toMEM(buff, sectlength(N)))
    {
        puts(" DMA - ILLEGAL ADDRESS");
        return EDMABoundary; /* バッファのアドレスが良くない。あふれる */
    }

    fdc_drivesel(device);
    format = fdc_getfmt(device);
    buff[0] = MF | 0x0c; /* 0b0x001100 */
                         /*   ||+------ SK DDAM/DAMスキップ */
                         /*   |+------- MF 0:FM 1:MFM */
                         /*   +-------- MT マルチトラック */
    buff[1] = (device & 0x03) | (H << 2);
    buff[2] = C;
    buff[3] = H;
    buff[4] = R;
    buff[5] = N;
    buff[6] = fdc_getEOT(MF, N, format); /* EOT トラック内の最終セクタ */
    buff[7] = fdc_getGPL(MF, N, format); /* GSL Gap3サイズ */
    buff[8] = 0xff;                      /* DTL Nが 0のときのデータ長 */

    /* C-PHASE */
    if (FALSE == fdc_writeRQM(1)) /* FDC busy check */
        return ETimeOut;
    if (FALSE == fdc_datawrite(buff, 9))
        return ETimeOut;

    /* E-PHASE */
    if (!PICfdcwait()) /* FDC割り込み要求があるまで待つ */
    {
        /* time out */
        puts("FDC:TIMEOUT(READDEL)");
        return ENoData;
    }

    /* R-PHASE */
    if (FALSE == fdc_dataread(result, 7))
        return ENoData;
    fdc_st0(device, result[0]);
    fdc_st1(device, result[1]);
    fdc_st2(device, result[2]);
    fdc_stC(device, result[3]);
    fdc_stH(device, result[4]);
    fdc_stR(device, result[5]);
    fdc_stN(device, result[6]);

    return fdc_errcode(result); /*エラーコード*/
}

/* READ DELETED DATA 指定した1セクタからデリーテッドデータを読む
戻り値はエラーコード！ 0:正常終了
device bit7 0:2DD 1:2HD / bit1 bit0 drive
MF FDFM/FDMFM  */
unsigned char readdeleted(unsigned char device,
                          unsigned char MF,
                          unsigned char C,
                          unsigned char H,
                          unsigned char R,
                          unsigned char N,
                          unsigned char *buff)
{
    unsigned char count, ret;
    for (count = DISKWORK.RETRY + 1; 0 != count; count--)
    {
        ret = rreaddeleted(device, MF, C, H, R, N, buff);
        if (0 == ret)
            break; /* 正常終了なら終了、エラーならリトライ */
    }
    return ret;
}

/* VERIFY 1セクタからデータを読むが、バッファへの転送は行わない。 リトライ無し
（セクタが存在するか、データCRCエラーが無いかのチェック）
戻り値 0:異常終了 0以外:正常終了 */
unsigned char vverify(unsigned char device, unsigned char MF,
                      unsigned char C,
                      unsigned char H,
                      unsigned char R,
                      unsigned char N,
                      unsigned char *buff)
{
    unsigned char cmd[9];
    unsigned char format;

    fdc_drivesel(device);
    format = fdc_getfmt(device);         /* 転送レート指定 */
    cmd[0] = MF | 0x16;                  /* 0b0*01 0110 */
                                         /*    +------- MF 0:FM 1:MFM */
    cmd[1] = (device & 0x03) | (H << 2); /* 0x*0000*** */
                                         /*   |0000|++- DS1,0 */
                                         /*   |    +--- H */
                                         /*   +-------- EC 1:DTLパラメータはSC（トラックあたりのセクタ数）になる */
    cmd[2] = C;
    cmd[3] = H;
    cmd[4] = R;
    cmd[5] = N;
    cmd[6] = fdc_getEOT(MF, N, format); /* EOT トラック内の最終セクタ */
    cmd[7] = fdc_getGPL(MF, N, format); /* GSL Gap3サイズ */
    cmd[8] = 0xff;                      /* DTL Nが 0のときのデータ長 / SC */

    *buff = EMissingID;
    if (FALSE == fdc_writeRQM(1)) /* FDC busy check */
        return FALSE;
    if (FALSE == fdc_datawrite(cmd, 9)) /* C-PHASE (E-PHASE 無し) */
        return FALSE;
    if (FALSE == fdc_dataread(cmd, 7)) /* R-PHASE */
        return FALSE;

    fdc_st0(device, cmd[0]);
    fdc_st1(device, cmd[1]);
    fdc_st2(device, cmd[2]);
    buff[0] = cmd[3]; /* C */
    buff[1] = cmd[4]; /* H */
    buff[2] = cmd[5]; /* R */
    buff[3] = cmd[6]; /* N */

    if (0 != (0xc0 & cmd[0])) /* ST0 bit7,6がゼロなら正常終了 */
    {
        /*エラーだったら アドレスoutにエラーコードを書く*/
        *buff = fdc_errcode(cmd);
        return FALSE; /* 0:異常終了*/
    }
    return TRUE; /* 0以外:正常終了*/
}

/* VERIFY 1セクタからデータを読むが、バッファへの転送は行わない。
（セクタが存在するか、データCRCエラーが無いかのチェック）
戻り値 0:異常終了 0以外:正常終了 */
unsigned char verify(unsigned char device, unsigned char MF,
                     unsigned char C,
                     unsigned char H,
                     unsigned char R,
                     unsigned char N,
                     unsigned char *buff)
{
    unsigned char count;
    for (count = DISKWORK.RETRY + 1; 0 != count; count--)
    {
        if (vverify(device, MF, C, H, R, N, buff))
            return TRUE; /* 正常終了なら終了、エラーならリトライ */
    }
    return FALSE;
}

/* WRITE DATA 指定した1セクタにデータを書く シークはしない
戻り値 0:異常終了 0以外:正常終了 */
unsigned char writedata(unsigned char device,
                        unsigned char MF,
                        unsigned char C,
                        unsigned char H,
                        unsigned char R,
                        unsigned char N,
                        unsigned char *buff)
{
    unsigned char cmd[9];
    unsigned char format;

    /* E-PHASE用 DMA セットアップ MEM -> I/O */
    if (FALSE == DMAset_toIO(buff, sectlength(N)))
    {
        puts(" DMA - ILLEGAL ADDRESS");
        return FALSE; /* バッファのアドレスが良くない。あふれる */
    }

    fdc_drivesel(device);
    format = fdc_getfmt(device);
    cmd[0] = MF | 0x05; /* 0b0x00 0101 */
                        /*   ||+------ SK DDAM/DAMスキップ */
                        /*   |+------- MF 0:FM 1:MFM */
                        /*   +-------- MT マルチトラック */
    cmd[1] = (device & 0x03) | (H << 2);
    cmd[2] = C;
    cmd[3] = H;
    cmd[4] = R;
    cmd[5] = N;
    cmd[6] = fdc_getEOT(MF, N, format); /* EOT トラック内の最終セクタ */
    cmd[7] = fdc_getGPL(MF, N, format); /* GSL Gap3サイズ */
    cmd[8] = 0xff;                      /* DTL Nが 0のときのデータ長 */

    /* C-PHASE */
    if (FALSE == fdc_writeRQM(1)) /* FDC busy check */
        return FALSE;
    if (FALSE == fdc_datawrite(cmd, 9))
        return FALSE;

    /* E-PHASE */
    if (!PICfdcwait()) /* FDC割り込み要求があるまで待つ */
    {
        /* time out */
        puts("FDC:TIMEOUT(WRITEDATA)");
        return FALSE;
    }

    /* R-PHASE */
    if (FALSE == fdc_dataread(cmd, 7))
        return FALSE;
    fdc_st0(device, cmd[0]);
    fdc_st1(device, cmd[1]);
    fdc_st2(device, cmd[2]);
    fdc_stC(device, cmd[3]);
    fdc_stH(device, cmd[4]);
    fdc_stR(device, cmd[5]);
    fdc_stN(device, cmd[6]);

    if (0 != (0xc0 & cmd[0])) /* ST0 bit7,6がゼロなら正常終了 */
    {
        return FALSE; /* 0:異常終了*/
        /*エラーが発生するのはIDが見つからない時*/
    }
    return TRUE; /* 0以外:正常終了*/
}

/* WRITE DELETED DATA 指定した1セクタにデリーデッドデータを書く シークはしない
戻り値 0:異常終了 0以外:正常終了 */
unsigned char writedeleted(unsigned char device,
                           unsigned char MF,
                           unsigned char C,
                           unsigned char H,
                           unsigned char R,
                           unsigned char N,
                           unsigned char *buff)
{
    unsigned char cmd[9];
    unsigned char format;

    /* E-PHASE用 DMA セットアップ MEM -> I/O */
    if (FALSE == DMAset_toIO(buff, sectlength(N)))
    {
        puts(" DMA - ILLEGAL ADDRESS");
        return FALSE; /* バッファのアドレスが良くない。あふれる */
    }

    fdc_drivesel(device);
    format = fdc_getfmt(device);
    cmd[0] = MF | 0x09; /* 0b0x00 1001 */
                        /*   ||+------ SK DDAM/DAMスキップ */
                        /*   |+------- MF 0:FM 1:MFM */
                        /*   +-------- MT マルチトラック */
    cmd[1] = (device & 0x03) | (H << 2);
    cmd[2] = C;
    cmd[3] = H;
    cmd[4] = R;
    cmd[5] = N;
    cmd[6] = fdc_getEOT(MF, N, format); /* EOT トラック内の最終セクタ */
    cmd[7] = fdc_getGPL(MF, N, format); /* GSL Gap3サイズ */
    cmd[8] = 0xff;                      /* DTL Nが 0のときのデータ長 */

    /* C-PHASE */
    if (FALSE == fdc_writeRQM(1)) /* FDC busy check */
        return FALSE;
    if (FALSE == fdc_datawrite(cmd, 9))
        return FALSE;

    /* E-PHASE */
    if (!PICfdcwait()) /* FDC割り込み要求があるまで待つ */
    {
        /* time out */
        puts("FDC:TIMEOUT(WRITEDEL)");
        return FALSE;
    }

    /* R-PHASE */
    if (FALSE == fdc_dataread(cmd, 7))
        return FALSE;
    fdc_st0(device, cmd[0]);
    fdc_st1(device, cmd[1]);
    fdc_st2(device, cmd[2]);
    fdc_stC(device, cmd[3]);
    fdc_stH(device, cmd[4]);
    fdc_stR(device, cmd[5]);
    fdc_stN(device, cmd[6]);

    if (0 != (0xc0 & cmd[0])) /* ST0 bit7,6がゼロなら正常終了 */
        return FALSE;
    return TRUE;
}

/* FORMAT TRACK シークしてから 1トラックをフォーマットする
戻り値 0:異常終了 0以外:正常終了

MF     : MFM / FM
C      : シリンダ番号
H      : ヘッド番号
N      : セクタ長コード
SC     : トラックあたりのセクタ数
DATA   : セクタに書き込むデータパターン
buff[] : C H R N の4バイト*セクタ数 をバッファに用意しておく
*/
unsigned char trackfmt(unsigned char device,
                       unsigned char MF,
                       unsigned char C,
                       unsigned char H,
                       unsigned char N,
                       unsigned char SC,
                       unsigned char DATA,
                       unsigned char *buff)
{
    unsigned char cmd[7];
    unsigned char format;

    /* E-PHASE用 DMA セットアップ MEM -> I/O */
    if (FALSE == DMAset_toIO(buff, SC * 4)) /* データ長は セクタ数 * CHRNの 4byte */
    {
        puts(" DMA - ILLEGAL ADDRESS");
        return FALSE; /* バッファのアドレスが良くない。あふれる */
    }

    fdc_drivesel(device); /* drive select */
    seek(device, C);

    /* 2HDの時回転数指定 */
    if (FD2HD == (FD2HD & device))
    {
        /* 2HD */
        /* 1トラック 8192byteより大きかったら 300RPM */
        if (8200 < (sectlength(N) * SC))
        {
#ifdef DEBUG
            puts("2HD 300RPM");
#endif
            DISKWORK.F2HD360 = MODE300;
        }
        else
        {
#ifdef DEBUG
            puts("2HD 360RPM");
#endif
            DISKWORK.F2HD360 = MODE360;
        }
    }
    else
    {
        /* 2DD 250Kbps/300Mbpsの指定 */
        DISKWORK.F2HD360 = MODE300;
    }

    format = fdc_getfmt(device); /* データレート指定 */
    format |= (1 << 2);          /* bit2:formatを1 */
    cmd[0] = MF | 0x0d;          /* 0b0*00 1101 */
                                 /*    +------- MF 0:FM 1:MFM */
    cmd[1] = (device & 0x03) | (H << 2);
    cmd[2] = N;
    cmd[3] = SC;
    cmd[4] = fdc_getGPL(MF, N, format); /* GSL Gap3サイズ */
    cmd[5] = DATA;

#ifdef DEBUG
    printf("N:%d SC:%d GPL:%2x %d\r\n", cmd[2], cmd[3], cmd[4], cmd[4]);
#endif

    /* C-PHASE */
    if (FALSE == fdc_writeRQM(1)) /* FDC busy check */
    {
#ifdef DEBUG
        puts("C-PHASE:FDC BUSY (TRACKFMT)");
#endif
        return FALSE;
    }

    if (FALSE == fdc_datawrite(cmd, 6))
    {
#ifdef DEBUG
        puts("C-PHASE:COMMAND WRITE ERROR (TRACKFMT)");
#endif
        return FALSE;
    }

    /* E-PHASE */
    if (!PICfdcwait()) /* FDC割り込み要求があるまで待つ */
    {
        /* time out */
#ifdef DEBUG
        puts("FDC:TIMEOUT (TRACKFMT)");
#endif
        return FALSE;
    }

    /* R-PHASE */
    if (FALSE == fdc_dataread(cmd, 7))
    {
#ifdef DEBUG
        puts("R-PHASE:READ ERROR (TRACKFMT)");
#endif
        return FALSE;
    }
    fdc_st0(device, cmd[0]);
    fdc_st1(device, cmd[1]);
    fdc_st2(device, cmd[2]);
    fdc_stC(device, cmd[3]);
    fdc_stH(device, cmd[4]);
    fdc_stR(device, cmd[5]);
    fdc_stN(device, cmd[6]);

    if (0 != (0xc0 & cmd[0])) /* ST0 bit7,6がゼロなら正常終了 */
    {
#ifdef DEBUG
        puts("FDC:ERROR SEE ST0 (TRACKFMT)");
#endif
        return FALSE;
    }
    return TRUE;
}

/* READ DIAGNOSTIC
インデックス信号直後のセクタからデータを読む。
戻り値 0:異常終了 0以外:正常終了
MF   : MFM / FM
H    : ヘッド番号
N    : セクタ長コード
buff : データバッファのアドレス

READ DATAと違い、IDのエラー、データのエラーがあっても処理を続行する。
リザルトステータスはコマンド中に起きたすべてのエラーの論理和になる。
エラー時はデータバッファ先頭にエラコードを書く
エラー時リトライは行わない */
unsigned char readdiag(unsigned char device, unsigned char MF,
                       unsigned char H, unsigned char N, unsigned char *buff)
{
    unsigned char result[7];
    unsigned char format, c;

    readid(device, MF, H, buff);
    c = buff[0]; /* 現在のシリンダ番号を得る */

    /* E-PHASE用 DMA セットアップ I/O -> MEM */
    if (FALSE == DMAset_toMEM(buff, sectlength(N)))
    {
        /* バッファのアドレスが良くない。あふれる */
        puts(" DMA - ILLEGAL ADDRESS");
        *buff = EDMABoundary; /* バッファ先頭アドレスにエラーコードを書く */
        return FALSE;
    }

    fdc_drivesel(device);
    format = fdc_getfmt(device);
    buff[0] = MF | 0x02; /* 0b0*00 0010 */
                         /*   ||+------ SK DDAM/DAMスキップ */
                         /*   |+------- MF 0:FM 1:MFM */
                         /*   +-------- MT マルチトラック */
    buff[1] = (device & 0x03) | (H << 2);
    buff[2] = c;                         /* C ここで与えたシリンダ番号が実行結果のシリンダ番号になる */
    buff[3] = H;                         /* H */
    buff[4] = 1;                         /* R */
    buff[5] = N;                         /* N  IDが一致しなくてもインデックス信号の直後のセクタからデータを読む */
    buff[6] = fdc_getEOT(MF, N, format); /* EOT トラック内の最終セクタ */
    buff[7] = fdc_getGPL(MF, N, format); /* GSL Gap3サイズ */
    buff[8] = 0xff;                      /* DTL Nが 0のときのデータ長 */

    /* C-PHASE */
    if (FALSE == fdc_writeRQM(1)) /* FDC busy check */
    {
        *buff = ETimeOut;
        return FALSE;
    }
    if (FALSE == fdc_datawrite(buff, 9))
    {
        *buff = ETimeOut;
        return FALSE;
    }

    /* E-PHASE */
    if (!PICfdcwait()) /* FDC割り込み要求があるまで待つ */
    {
        /* time out */
        puts("FDC:TIMEOUT(READDIAG)");
        *buff = ETimeOut;
        return FALSE;
    }

    /* R-PHASE */
    if (FALSE == fdc_dataread(result, 7))
    {
        *buff = ETimeOut;
        return FALSE;
    }
    fdc_st0(device, result[0]);
    fdc_st1(device, result[1]);
    fdc_st2(device, result[2]);
    fdc_stC(device, result[3]);
    fdc_stH(device, result[4]);
    fdc_stR(device, result[5]);
    fdc_stN(device, result[6]);

    if (0 != (0xc0 & result[0])) /* ST0 bit7,6がゼロなら正常終了 */
    {
        /*エラーだったら アドレスbuffにエラーコードを書く*/
        *buff = fdc_errcode(result);
        return FALSE;
    }
    return TRUE;
}

/* 指定したトラックのセクタのタイプを調べる。シークもする
引数 density
0b--**----
    ++----- 0:2D 1:2DD 2:2HD 3:自動判別

戻り値 0:異常終了 0以外:セクタのタイプ
0b*0010*00
  |    +--- 1:MFM 0:FM
  +-------- 1:2HD 0:2DD
 */
unsigned char sectsense(unsigned char device, unsigned char C, unsigned char H, unsigned char density)
{
    unsigned char buff[4];

    device &= 0x03;  /*ユニット番号だけにする*/
    density &= 0x30; /* bit4,5っだけにする*/
    /* リード／ライト／トラックフォーマット関数冒頭で転送レート設定している (fdc_getfmt) */

    if ((0x20 == density) || (0x30 == density))
    {
        /*2HD*/
        /* 500kbps 300rpm MFM 1.44MB */
        diskinit(device); /*モードを変えるたびにFDCリセットしてみたけど2DDや1.2MB読めない*/
        track00(device);
        seek(device, C);
        DISKWORK.F2HD360 = MODE300;
        if (readid(FD2HD | device, FDMFM, H, buff))
            return FD2HD | FDMFM;

        /*IDを読めなかった場合、その後しばらくFDCの動作がおかしくなるのでFDCをリセットする。
        FDCをリセットするとFDCのレジスタがクリアされるのでリキャブレートしてシークし直す必要がある。*/
        /* 500kbps 300rpm FM */
        diskinit(device);
        track00(device);
        seek(device, C);
        DISKWORK.F2HD360 = MODE300;
        if (readid(FD2HD | device, FDFM, H, buff))
            return FD2HD | FDFM;
#if 0
VIA C3のマザーボードだと 500Kbps 2pin:Lにならない
DOSでも1.2MB読めないので別のマザーボードでテストする
    /* 500kbps 360rpm MFM 1.2MB */
    diskinit(device);
    track00(device);
    seek(device, C);
    DISKWORK.F2HD360 = MODE360;
    if (readid(FD2HD | device, FDMFM, H, buff))
        return FD2HD | FDMFM;

    /* 500kbps 360rpm FM */
    diskinit(device);
    track00(device);
    seek(device, C);
    DISKWORK.F2HD360 = MODE360;
    if (readid(FD2HD | device, FDFM, H, buff))
        return FD2HD | FDFM;
#endif
        if (0x30 != density)
            return FALSE; /*自動判別じゃないときはここで異常終了*/
    }

    /* 2D/2DDは自動判別できないので共通 */

    /* 250kbps MFM */
    diskinit(device);
    track00(device);
    seek(device, C);
    DISKWORK.F2HD360 = MODE300;
    if (readid(FD2DD | device, FDMFM, H, buff))
        return FD2DD | FDMFM;

    /* 250kbps FM */
    diskinit(device);
    track00(device);
    seek(device, C);
    DISKWORK.F2HD360 = MODE300;
    if (readid(FD2DD | device, FDFM, H, buff))
        return FD2DD | FDFM;

    /* 300kbps MFM (2DD 360RPM PC9801) */
    diskinit(device);
    track00(device);
    seek(device, C);
    DISKWORK.F2HD360 = MODE360;
    if (readid(FD2DD | device, FDMFM, H, buff))
        return FD2DD | FDMFM;

    /* 300kbps FM (2DD 360RPM PC9801) */
    diskinit(device);
    track00(device);
    seek(device, C);
    DISKWORK.F2HD360 = MODE360;
    if (readid(FD2DD | device, FDFM, H, buff))
        return FD2DD | FDFM;

    return FALSE;
}

/* 指定したトラックのセクタの変調方式を調べる
シークもする
戻り値 FDMFM/FDFM
0b00000*00
       +--- 1:MFM 0:FM
*/
unsigned char modsense(unsigned char device, unsigned char C, unsigned char H)
{
    unsigned char buff[4], temp;

    temp = DISKWORK.F2HD360;
    /*
    diskinit(device);
    track00(device);
    seek(device, C);
    DISKWORK.F2HD360 = temp;
    */
    seek(device, C);
    if (readid(device, FDMFM, H, buff))
    {
        /* ID読めたときはリセットいらない
        diskinit(device);
        DISKWORK.F2HD360 = temp;
        */
        return FDMFM;
    }

    /*ID読めなかったとき、その後しばらくFDCの動きがおかしくなるのでFDCをリセットする*/
    diskinit(device);
    track00(device);
    seek(device, C);
    DISKWORK.F2HD360 = temp;
    if (readid(device, FDFM, H, buff))
    {
        return FDFM;
    }

    /* MFM/FM どちらでも読めない */
    diskinit(device);
    track00(device);
    seek(device, C);
    DISKWORK.F2HD360 = temp;
    return FDMFM; /*FDFM=0なので FALSEは使えない*/
}

/* セクタ長コードからセクタ長を得る
戻り値 0:異常終了 0以外:セクタのバイト数 */
unsigned short sectlength(unsigned char N) /* セクタ長コード */
{
    unsigned short sectsize[8] =
        {128, 256, 512, 1024, 2048, 4096, 8192, 16384};

    if (7 < N)
    {
#ifdef DEBUG
        printf(" sectlength err ");
#endif
        return 0;
    }
    return sectsize[N];
}

/* READDIAG で先頭セクタの番号を調べる
バッファサイズはセクタ長の分必要
正常終了ならバッファ先頭にセクタ番号
異常終了ならバッファ先頭にエラーコード
### PC98のまま変更なし ### */
unsigned char secthead(unsigned char drive, unsigned char MF, /* MFM / FM */
                       unsigned char H,                       /* ヘッド番号 */
                       unsigned char *buff)                   /* 出力を書き込むアドレス */
{
    unsigned char n;

    /*セクタ長コードを調べる*/
    if (!readid(drive, MF, H, buff))
    {
        /*printf("readidERR");*/
        return FALSE;
    }
    n = buff[3];

    if (!readdiag(drive, MF, H, n, buff))
    {
        /*printf("readdiagERR");*/
        /*エラーならエラーコードがバッファの先頭アドレスに入る*/
        /*puts(errmsg(buff[0]));*/
        return FALSE;
    }
    drive &= 0x0F; /*ドライブ番号だけにする*/
    switch (drive)
    {
    case 0:
        buff[0] = DISKSTAT.D0.R - 1; /*読み込んだ次のセクタ番号を示しているので引き算*/
        return TRUE;
    case 1:
        buff[0] = DISKSTAT.D1.R - 1;
        return TRUE;
    case 2:
        buff[0] = DISKSTAT.D2.R - 1;
        return TRUE;
    case 3:
        buff[0] = DISKSTAT.D3.R - 1;
        return TRUE;
    }
    return FALSE;
}

/* エラーコードからエラーメッセージを得る */
char *errmsg(unsigned char ah)
{
    if (ENoError == ah)
        return "No error";

    switch (ah & 0xF8)
    {
    case ECorrecteddata:
        return "Corrected data";
    case EIllegalAddres:
        return "Illegal disk Addres";
    case EDirectAccessAltTrack:
        return "Direct access an alternate track";
    case EDataErr:
        return "Data error";
    case ESeekErr:
        return "Seek error";
    case ENotReadAltTrack:
        return "Not read the alternate track";
    }

    switch (ah & 0xF0)
    {
    case EDMABoundary:
        return "DMA Boundary";
    case EEndCylinder:
        return "End of cylinder";
    case EOverRun:
        return "Equipment check over run";
    case ENotReady:
        return "Not ready";
    case ENotWritable:
        return "Not writable";
    case EErr0x80:
        return "Error 0x80";
    case ETimeOut:
        return "Time out";
    case EIDCRCErr:
        return "ID CRC error";
    case EDataCRCErr:
        return "DATA CRC error";
    case ENoData:
        return "No data (ID not found)";
    case EBadCylinder:
        return "Bad cylinder";
    case EMissingID:
        return "Missing address mark (ID)";
    case EMissingData:
        return "Missing address mark (DATA)";
    }
    return "Unknown error";
}

/* FDCを操作し終わった後の処理
モーターOFF FDC割り込み許可
DOSに戻る前に実行しないと、フロッピーの読み書きができない */
void diskexit(void)
{
    fdc_motoroff();
    PICfdcmask(0); /* FDC割り込み許可
     割込み禁止のままだと biosリセットしても DOSでフロッピーにアクセスできない */
    fdc_biosreset(0);
}

/*FDCにアクセスできるかチェックする （機種判別）
戻り値 0:異常終了*/
unsigned char fdcheck(void)
{
    /*DORレジスタ(0x3f2)に値を書いてみて、書けているかをチャックする*/
    /*0x3f2はPC9801のI/Oポート一覧でCPUに使われている様子*/
    fdc_drivesel(0);
    if (0 != (0x03 & fdc_DORread()))
        return FALSE;
    fdc_drivesel(1);
    if (1 != (0x03 & fdc_DORread()))
        return FALSE;
    return TRUE;
}
