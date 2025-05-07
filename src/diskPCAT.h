/*
PC-9801 DISK BIOS を使う関数をPCATに移植
LSI C-86 Ver.3.30 試食版用ですが、DOS用コンパイラならどれもだいたい同じ。

This code is provided under a CC0 Public Domain License.
https://creativecommons.org/publicdomain/zero/1.0/deed.ja

2016年2月1日作成 佐藤恭一 kyoutan.jpn.org
2023年3月23日 関数の定義を分離して、PC98と共通化した
*/

/* PC-9801用
#define DISKSTAT  (*(volatile struct DISK_RESULT far *)0x00000564)
*/
/* PCAT */
#if 0
struct FDC_PCATWORK
{
    unsigned char SEEK2D;  /* SEEK2D MODE2D/MODE2DD */
    unsigned char F2HD360; /* 1:2HD360rpm 0:300rpm */
};
struct PCATWORK
{
    struct FDC_PCATWORK D0, D1, D2, D3;
    unsigned char RETRY;
    unsigned short TIMEOUT;
};
#endif
struct PCATWORK
{
    /*struct FDC_PCATWORK D0, D1, D2, D3;*/
    unsigned char SEEK2D;  /* SEEK2D MODE2D/MODE2DD */
    unsigned char F2HD360; /* 1:2HD360rpm 0:300rpm */
    unsigned char CMDRETRY;
    unsigned short TIMEOUT;
    unsigned short SEEKTIMEOUT;
    unsigned char RETRY;
};

/*システム共通域の DISK_RESULT (PC98互換)*/
struct FDC_RESULT
{
    unsigned char ST0, ST1, ST2, C, H, R, N, NCN; /* NCN:現在のシリンダ番号 */
};

struct DISK_RESULT
{
    struct FDC_RESULT D0, D1, D2, D3;
};

extern struct DISK_RESULT DISKSTAT;
extern struct PCATWORK DISKWORK;

#define MODE300 0           /* 2HD 300rpm */
#define MODE360 1           /* 2HD 360rpm */
#define FDCRETRY 5          /* FDCリードコマンドのリトライ回数 */
#define FDCCMDRETRY 0       /* FDCコマンドリトライ回数 初期値 */
#define FDCTIMEOUT 1000     /* FDCタイムアウト[ms] 初期値 (最大10000) */
#define FDCSEEKTIMEOUT 4000 /* シーク動作のタイムアウト[ms] (最大10000) */
/* FDC CONFIGUREコマンド
EIS     - 1:リードまたはライト・コマンドを実行する前にシークを実行する (Enable Implied Seek)
EFIFO   - 1:disables the FIFO
POLL    - 0:ポーリングが有効
FIFOTHR - バッファ容量 0x00:1byte ~ 0x0f:16byte
PRETRK   :どのトラックから書き込み補償を実行するか 0x00 ~ 0xff
初期値
EIS        0:No Implied Seek
EFIFO      1:FIFO Disabled
POLL       0:Polling Enabled
FITOTHR 0x00:FIFO 1byte
PRETRK  0x00:Pre-Compensation Set to Track 0
読み取りフィルタによる補正を行うドライブが多いため書き込み補償機能は使用しない（トランジスタ技術SPECIAL No.11 P20） */

/*#define FDC_PCATCONFIG 0x0f*/ /* 0b00001111 CONFIGUREコマンドに与える値 NON DMA*/
#define FDC_CONFIG1 0x1f        /* 0b00011111 CONFIGUREコマンドに与える値 DMA*/
                                /*    |||++++- FITOTHR 16byte FIFO */
                                /*    ||+----- POLL    割り込み */
                                /*    |+------ EFIFO   FIFO有効 */
                                /*    +------- EIS     No Implied Seek */
#define FDC_CONFIG2 0xff        /* PRETRK 書き込み補償開始トラック 0xff:書き込み補償使用しない */

/* FDC SPECIFコマンド
SRT - STEP RATE TIME
HUT - HEAD UNLOAD TIME ヘッドアンロードするまでの待ち時間
HLT - HEAD LOAD TIME ヘッドロードしてからの待ち時間
ND  - 1:ポーリング 0:DMA

ヘッドアンロードタイム[ms] = HUT * 16[ms] (0:設定禁止) 2DD/2Dは2倍
ヘッドロードタイム[ms] = HLT * 2[ms] (0:設定禁止) 2DD/2Dは2倍
ステップレートタイム[ms] = 16 - SRT[ms] 2DD/2Dは2倍 */
#define FDC_SPECIFY1 0xaf            /* 0b00000000 */
                                     /*   ||||++++- HUT 0xf:240ms/480ms */
                                     /*   ++++----- SRT 0xa:6ms/12ms */
#define FDC_SPECIFY2 ((50 << 1) | 0) /* 0b00000000 */
                                     /*   |||||||+- ND 0:DMA */
                                     /*   +++++++-- HLT 50:100ms/200ms */

/* セクタサイズ
#define fdc_N_128 0x00
#define fdc_N_256 0x01
#define fdc_N_512 0x02
#define fdc_N_1024 0x03
#define fdc_N_2048 0x04
#define fdc_N_4096 0x05
#define fdc_N_8192 0x06
#define fdc_N_16384 0x07*/
#define fdc_N_default 0x02 /* 512 */
