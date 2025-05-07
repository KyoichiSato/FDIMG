/*
DOS/V機(IBM PC / OADG PC) の
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

/* PC/AT BIOSでタイマーを使う */
unsigned short UshortSub(unsigned short current, unsigned short start);
unsigned short timer_get16();
char timeout_tick(unsigned short tick, unsigned short start);
char timeout_ms(unsigned short time, unsigned short start);
void wait_tick(unsigned short tick);
void wait_ms(unsigned short time);


/* 割り込みコントローラ */
void PICfdcmask(unsigned char mask);
unsigned char PICfdcIRQ(void);
unsigned char PICfdcwait(void);

/* DMAコントローラ */
void DMAfdcmask(void);
unsigned char DMAset_toMEM(unsigned char *buff, unsigned short count);
unsigned char DMAset_toIO(unsigned char *buff, unsigned short count);
unsigned short DMAreadaddrreg(void);
unsigned short DMAreadcountreg(void);
unsigned char DMAreadpagereg(void);
unsigned char DMAgetpage(unsigned char *buff);
unsigned short DMAgetaddress(unsigned char *buff);
unsigned char DMAchackaddress(unsigned short address, unsigned short size);
/*unsigned char DMAbusy(void);*/
