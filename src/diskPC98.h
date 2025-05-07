/*
PC-9801 DISK BIOS を使う関数
FDCの動作を正確に再現するPC-9801エミュレータは少ないので、エミュレータで動作確認をする場合は注意を要する。
LSI C-86 Ver.3.30 試食版用ですが、DOS用コンパイラならどれもだいたい同じ。

このソースコードは CC0 パブリック・ドメイン提供です。
https://creativecommons.org/publicdomain/zero/1.0/deed.ja

2016年2月1日 2016年5月18日 佐藤恭一 kyoutan.jpn.org
2023年3月23日 関数の定義を分離してPCAT用と共通化した
*/

/*システム共通域の DISK_RESULT*/
struct FDC_RESULT
{
    unsigned char ST0, ST1, ST2, C, H, R, N,NCN;
};
struct DISK_RESULT
{
    struct FDC_RESULT D0, D1, D2, D3;
};
#define DISKSTAT  (*(volatile struct DISK_RESULT far *)0x00000564)
#define DISKBIOS  0x1B
