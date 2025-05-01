/*
ディスクイメージ用のヘッダ

このファイルは CC0 パブリック・ドメイン提供です。
https://creativecommons.org/publicdomain/zero/1.0/deed.ja

2015年6月5日 2016年5月19日 佐藤恭一 kyoutan.jpn.org
*/

struct st_d88_header
{
	char name[17];						/* +0x00 ディスクイメージの名前 終端の\0を含む */
	char reserve[9];					/* +0x11 予約 */
	unsigned char write_protect;		/* +0x1A 0x10:ライトプロテクト有り 0x00:無し */
	unsigned char type;					/* +0x1B 0x00:2D 0x10:2DD 0x20:2HD */
	unsigned long size;					/* +0x1C イメージサイズのサイズ+1（バイト数 リトルエンディアン） */
										/*       次のイメージへのオフセット */
	unsigned long track_offset[164];	/* +0x20 イメージファイル先頭からのオフセット */
										/*       存在しないトラックは 0を書く */
};
#define D88_IDXSIZE (17 + 9 + 1 + 1 + 4 + (164 * 4))
#define D88_2D  0x00
#define D88_2DD 0x10
#define D88_2HD 0x20

struct st_d88_sect_header
{
	unsigned char c;			/* +0x00 シリンダ番号 */
	unsigned char h;			/* +0x01 ヘッド（SIDE）番号 */
	unsigned char r;			/* +0x02 セクタ番号 (先頭セクタは1) */
	unsigned char n;			/* +0x03 セクタ長コード　0x00:128 */
								/*                       0x01:256 */
								/*                       0x02:512 */
								/*                       0x03:1024 */
								/*                       0x04:2048 */
								/*                       0x05:4096 */
								/*                       0x06:8192 */
	unsigned short sect;		/* +0x04 このトラックのセクタ数（リトルエンディアン） */
	unsigned char density;		/* +0x06 記録密度 0x00:MFM 0x40:FM */
	unsigned char deleted;		/* +0x07 0x00:DAM (NORMAL) 0x10:DDAM (DELETED) */
	unsigned char status;		/* +0x08 0x00:エラー無し */
								/*       0x10:エラー無し DDAM検出 */
								/*       0xA0:ID CRCエラー */
								/*       0xB0:DATA CRCエラー */
								/*       0xE0:IDAMなし */
								/*       0xF0:DAMなし */
	unsigned char reserve[5];	/* +0x09 予約 */
	unsigned short size;		/* +0x0E セクタデータのサイズ（このヘッダは含まない）（リトルエンディアン） */
								/* +0x10 からセクタデータ */
};
#define D88_SECHSIZE (1 + 1 + 1 + 1 + 2 + 1 + 1 + 1 + 5 + 2)
#define D88_MFM		0x00
#define D88_FM		0x40
#define D88_DAM		0x00
#define D88_DDAM	0x10
#define D88_IDCRC	0xA0
#define D88_DATCRC	0xB0
#define D88_NOIDAM	0xE0
#define D88_NODAM	0xF0

struct st_fdb
{
	char id[4];							/* +0x00 ID “FDBI” あまり意味は無い */
	char name[17];						/* +0x04 ディスクイメージの名前 終端の\0を含む */
	char reserve[9];					/* +0x15 予約 */
	unsigned char write_protect;		/* +0x1E 0x10:ライトプロテクト ON 0x00:OFF */
	unsigned char type;					/* +0x1F 0x00:2D 0x10:2DD 0x20:2HD (D88と同じ) */
	unsigned long track_offset[164];	/* +0x20 イメージファイル先頭からのオフセット（リトルエンディアン） */
										/*       164トラック （82シリンダ） 分 */
										/*       存在しないトラックは 0を書く */
										/*       （片面ディスクならSIDE1のトラックがゼロ） */
										/*   [0] TRACK 0 SIDE0 */
										/*   [1] TRACK 0 SIDE1 */
										/*   [2] TRACK 1 SIDE0 */
										/*   [3] TRACK 1 SIDE1 */
										/*    : */
										/* [163] TRACK82 SIDE1 */
	/*
		各トラック
		unsigned char rate;		このトラックの転送レート 0x00:125kbps(?) 0x10:250kbps(2D/2DD) 0x20:300kbps(PC98 2DD) 0x30:500kbps(2HD)
		unsigned short size;	トラックデータのサイズ
		unsigned char trackdata[size];	MFMまたはFMエンコード済のクロックビットを含むビット列
		CRCの値やギャップもすべて含む
	*/
};
