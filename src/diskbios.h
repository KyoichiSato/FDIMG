/*
PC-9801 DISK BIOS を使う関数
LSI C-86 Ver.3.30 試食版用

This code is provided under a CC0 Public Domain License.
https://creativecommons.org/publicdomain/zero/1.0/deed.ja

2023年3月23日 佐藤恭一 kyoutan.jpn.org
*/

#define FALSE 0
#define TRUE (!FALSE)
#define FD2HD 0x90 /*デバイスタイプ指定*/
#define FD2DD 0x10 /*デバイスタイプ指定*/
#define FDMFM 0x40 /*コマンド識別*/
#define FDFM 0x00  /*コマンド識別*/
#define MODE2D 0   /*モード切替 48tpi*/
#define MODE2DD 1  /*モード切替 96tpi*/

unsigned char diskinit(unsigned char device);
unsigned char drvmode(unsigned char device, /* デバイス番号 0～3 */
                      unsigned char mode);  /* モード MODE2D/MODE2DD */
unsigned char diskmode(unsigned char mode);
unsigned char disksense(unsigned char device,
                        unsigned char *errcode);
unsigned char track00(unsigned char device);
unsigned char seek(unsigned char device,
                   unsigned char C); /* シリンダ番号 */
unsigned char readid(unsigned char device,
                     unsigned char MF,     /* MFM/FM */
                     unsigned char H,      /* ヘッド番号 */
                     unsigned char *buff); /* IDを書き込むアドレス */
unsigned char readdata(unsigned char device,
                       unsigned char MF,     /* MFM / FM */
                       unsigned char C,      /* シリンダ番号 */
                       unsigned char H,      /* ヘッド番号 */
                       unsigned char R,      /* セクタ番号 */
                       unsigned char N,      /* セクタ長コード */
                       unsigned char *buff); /* 出力を書き込むアドレス */
unsigned char readdeleted(unsigned char device,
                          unsigned char MF,     /* MFM / FM */
                          unsigned char C,      /* シリンダ番号 */
                          unsigned char H,      /* ヘッド番号 */
                          unsigned char R,      /* セクタ番号 */
                          unsigned char N,      /* セクタ長コード */
                          unsigned char *buff); /* 出力を書き込むアドレス */
unsigned char verify(unsigned char device,
                     unsigned char MF,     /* MFM / FM */
                     unsigned char C,      /* シリンダ番号 */
                     unsigned char H,      /* ヘッド番号 */
                     unsigned char R,      /* セクタ番号 */
                     unsigned char N,      /* セクタ長コード */
                     unsigned char *buff); /* 出力を書き込むアドレス */
unsigned char writedata(unsigned char device,
                        unsigned char MF,     /* MFM / FM */
                        unsigned char C,      /* シリンダ番号 */
                        unsigned char H,      /* ヘッド番号 */
                        unsigned char R,      /* セクタ番号 */
                        unsigned char N,      /* セクタ長コード */
                        unsigned char *buff); /* データバッファの先頭アドレス */
unsigned char writedeleted(unsigned char device,
                           unsigned char MF,     /* MFM / FM */
                           unsigned char C,      /* シリンダ番号 */
                           unsigned char H,      /* ヘッド番号 */
                           unsigned char R,      /* セクタ番号 */
                           unsigned char N,      /* セクタ長コード */
                           unsigned char *buff); /* データバッファの先頭アドレス */
unsigned char trackfmt(unsigned char device,
                       unsigned char MF,     /* MFM / FM */
                       unsigned char C,      /* シリンダ番号 */
                       unsigned char H,      /* ヘッド番号 */
                       unsigned char N,      /* セクタ長コード */
                       unsigned char SC,     /* トラックあたりのセクタ数*/
                       unsigned char DATA,   /* セクタに書き込むデータパターン */
                       unsigned char *buff); /* データバッファの先頭アドレス */
unsigned char readdiag(unsigned char device,
                       unsigned char MF,     /* MFM / FM */
                       unsigned char H,      /* ヘッド番号 */
                       unsigned char N,      /* セクタ長コード */
                       unsigned char *buff); /* 出力を書き込むアドレス */
unsigned char sectsense(unsigned char device,
                        unsigned char C,
                        unsigned char H,
                        unsigned char density);
unsigned char modsense(unsigned char device,
                       unsigned char C,
                       unsigned char H);
unsigned short sectlength(unsigned char N); /*セクタ長コード*/
unsigned char secthead(unsigned char drive,
                       unsigned char MF,     /* MFM / FM */
                       unsigned char H,      /* ヘッド番号 */
                       unsigned char *buff); /* 出力を書き込むアドレス */
char *errmsg(unsigned char ah);
void diskexit(void);
unsigned char fdcheck(void);
