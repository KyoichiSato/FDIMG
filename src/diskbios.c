/*
PC-9801 DISK BIOS を使う関数
FDCの動作を正確に再現するPC-9801エミュレータは少ないので、エミュレータで動作確認をする場合は注意を要する。
LSI C-86 Ver.3.30 試食版用

このソースコードは CC0 パブリック・ドメイン提供です。
https://creativecommons.org/publicdomain/zero/1.0/deed.ja

2016年2月1日 佐藤恭一 kyoutan.jpn.org
2016年6月19日 更新

「PC-9800シリーズ テクニカルデータブック」を見ながら書きました。
*/
/*#include <stdio.h>*/
#include "diskbios.h"

/* INITIALIZE FDCとBIOSのワークエリアの初期化
戻り値 0:異常終了 0以外:正常終了 */
unsigned char diskinit(unsigned char device)
{
  union REGS reg;

  reg.h.ah=0x03;  /* INITIALIZE */
  reg.h.al=device;
  int86(DISKBIOS, &reg, &reg);

  if(0!=reg.x.cflag) return FALSE; /* 0:異常終了*/

  return TRUE;  /* 0以外:正常終了*/
}

/* DISKMODE 動作モードの設定 デバイスタイプユニットが2DDの時のみ有効
48tpi(2D)/96tpi(2DD)の切り替え 2HDの時は無意味
戻り値 0:異常終了 0以外:正常終了 */
unsigned char drvmode(unsigned char device /* デバイス番号 0～3 */
                     ,unsigned char mode)  /* モード MODE2D/MODE2DD */
{
  unsigned char val;
  unsigned char ret;

  device&=0x03; /*0000 0011 下位2ビットのみ取り出す（0～3）*/
  val=0x0f; /* 初期値 00001111 */
  /*各ドライブの現在のモードを調べる 0:48tpi(2D) 1:96tpi(2DD*/
  if(disksense(FD2DD | 0, &ret)) if((0x04 & ret)==0) val &= ~(1<<0); /* DRIVE0 */
  if(disksense(FD2DD | 1, &ret)) if((0x04 & ret)==0) val &= ~(1<<1); /* DRIVE1 */
  if(disksense(FD2DD | 2, &ret)) if((0x04 & ret)==0) val &= ~(1<<2); /* DRIVE2 */
  if(disksense(FD2DD | 3, &ret)) if((0x04 & ret)==0) val &= ~(1<<3); /* DRIVE3 */

  if(MODE2D==mode) val &= ~(1<<device); /* 2D  ビットクリア */
  else             val |=  (1<<device); /* 2DD ビットセット */

  return diskmode(val);
}

/* DISKMODE 動作モードの設定 デバイスタイプユニットが2DDの時のみ有効
48tpi(2D)/96tpi(2DD)の切り替え 2HDの時は無意味
mode ----1111  0:2D 1:2DD
         |||+- DRIVE0
         ||+-- DRIVE1
         |+--- DRIVE2
         +---- DRIVE3
戻り値 0:異常終了 0以外:正常終了 */
unsigned char diskmode(unsigned char mode)
{
  union REGS reg;

  reg.h.ah=0x0E;  /* 全ドライブ両面モードにする */
  reg.h.al=0x1F;
  int86(DISKBIOS, &reg, &reg);

  mode &= 0x0f;
  mode |= 0x10;
  reg.h.ah=0x8E;  /* MODE設定 */
  reg.h.al=mode;
  int86(DISKBIOS, &reg, &reg);

  if(0!=reg.x.cflag) return FALSE; /* 0:異常終了*/

  return TRUE;  /* 0以外:正常終了*/
}

/* SENSE デバイスの状態を調べる
戻り値 0:異常終了 0以外:正常終了 */
unsigned char disksense(unsigned char device
                       ,unsigned char *errcode)
{
  union REGS reg;

  reg.h.ah=0x04;  /* 00000100 SENSE */
  reg.h.al=device;
  int86(DISKBIOS, &reg, &reg);

  *errcode=reg.h.ah;

  if(0!=reg.x.cflag) return FALSE; /* 0:異常終了*/

  return TRUE;  /* 0以外:正常終了*/
}

/* RECALIBRATE ヘッドをトラック0に移動する
戻り値 0:異常終了 0以外:正常終了 */
unsigned char track00(unsigned char device)
{
  union REGS reg;

  reg.h.ah=0x07;  /* RECALIBRATE */
  reg.h.al=device;
  int86(DISKBIOS, &reg, &reg);

  if(0!=reg.x.cflag) return FALSE; /* 0:異常終了*/
  return TRUE; /* 0以外:正常終了*/
}

/* SEEK 指定シリンダにヘッドを移動する
戻り値 0:異常終了 0以外:正常終了 */
unsigned char seek(unsigned char device
                  ,unsigned char C)     /* シリンダ番号 */
{
  union REGS reg;

  reg.h.ah= 0x10; /*AH 00010000*/
                  /*      +---- SEEK */
  reg.h.al= device;                /*AL デバイスタイプユニット番号*/
  reg.h.cl= C;                     /*CL シリンダ番号（ahのSEEKビットが1の時のみ有効）*/
  int86(DISKBIOS, &reg, &reg);

  if(0!=reg.x.cflag)
  {
    return FALSE; /* 0:異常終了*/
  }
  return TRUE; /* 0以外:正常終了*/
}

/* READ ID 現在のトラックからIDを読む
アドレスout から C H R N の順に4バイト書き込む
戻り値 0:異常終了 0以外:正常終了 */
unsigned char readid(unsigned char device
                    ,unsigned char MF     /* MFM/FM */
                    ,unsigned char H      /* ヘッド番号 */
                    ,unsigned char *buff) /* IDを書き込むアドレス */
{
  union REGS reg;

  reg.h.ah= MF | 0x0A; /* 01001010 ID READ */
                       /*  ||+---- SEEK */
                       /*  |+----- ~r */
                       /*  +------ MF */
  reg.h.al=device;     /*デバイスタイプユニット番号*/
  reg.h.dh=H;          /*ヘッド番号*/

  int86(DISKBIOS, &reg, &reg);

  if(0!=reg.x.cflag)
  {
      /*エラーだったら アドレスbuffにエラーコードを書く*/
      *buff = reg.h.ah;
      return FALSE; /* 0:異常終了*/
  }

  *buff++ = reg.h.cl;   /* C シリンダ */
  *buff++ = reg.h.dh;   /* H ヘッド */
  *buff++ = reg.h.dl;   /* R セクタ番号 */
  *buff   = reg.h.ch;   /* N セクタ長コード */

  return TRUE; /* 0以外:正常終了*/
}

/* READ DATA 指定した1セクタからデータを読む
戻り値はエラーコード！ 0:正常終了 ！変更しました！ */
unsigned char readdata(unsigned char device
                      ,unsigned char MF     /* MFM / FM */
                      ,unsigned char C      /* シリンダ番号 */
                      ,unsigned char H      /* ヘッド番号 */
                      ,unsigned char R      /* セクタ番号 */
                      ,unsigned char N      /* セクタ長コード */
                      ,unsigned char *buff) /* 出力を書き込むアドレス */
{
  union REGS reg;
  struct SREGS seg;

  reg.h.ah= MF | 0x16; /*AH 00010110 READ DATA  セクタからデータを読む。*/
                       /*   |||+---- SEEK */
                       /*   ||+----- ~r   */
                       /*   |+------ MF   */
                       /*   +------- MT   */
  reg.h.al= device;                /*AL デバイスタイプユニット番号*/
  reg.x.bx= sectlength(N);         /*BX DTL*/
  reg.h.cl= C;                     /*CL シリンダ番号（ahのSEEKビットが1の時のみ有効）*/
  reg.h.dh= H;                     /*DH ヘッド番号*/
  reg.h.dl= R;                     /*DL セクタ番号*/
  reg.h.ch= N;                     /*CH セクタ長コード （合わないセクタ長コードを指定するとエラー）*/
  segread(&seg);                   /* セグメントレジスタの値を取得 */
  seg.es= seg.ds;                  /*ES バッファの先頭アドレス[セグメント]*/
  reg.x.bp= (unsigned short)buff;  /*BP バッファの先頭アドレス[オフセット]*/
  int86x(DISKBIOS, &reg, &reg, &seg); /*セグメントレジスタも設定するのでint86x*/

/*  if(0!=reg.x.cflag)*/
/*  {*/
    /*エラーだったら アドレスoutにエラーコードを書く*/
/*    *buff = reg.h.ah;*/
/*    return FALSE;*/ /* 0:異常終了*/
/*  }*/
/*  return TRUE;*/ /* 0以外:正常終了*/

  return reg.h.ah;  /*エラーコード*/
}

/* READ DELETED DATA 指定した1セクタからデリーテッドデータを読む
戻り値はエラーコード！ 0:正常終了 ！変更しました！ */
unsigned char readdeleted(unsigned char device
                         ,unsigned char MF     /* MFM / FM */
                         ,unsigned char C      /* シリンダ番号 */
                         ,unsigned char H      /* ヘッド番号 */
                         ,unsigned char R      /* セクタ番号 */
                         ,unsigned char N      /* セクタ長コード */
                         ,unsigned char *buff) /* 出力を書き込むアドレス */
{
  union REGS reg;
  struct SREGS seg;

  reg.h.ah= MF | 0x16; /*AH 00011100 READ DELETED DATA*/
                       /*   |||+---- SEEK */
                       /*   ||+----- ~r   */
                       /*   |+------ MF   */
                       /*   +------- MT   */
  reg.h.al= device;                /*AL デバイスタイプユニット番号*/
  reg.x.bx= sectlength(N);         /*BX DTL*/
  reg.h.cl= C;                     /*CL シリンダ番号（ahのSEEKビットが1の時のみ有効）*/
  reg.h.dh= H;                     /*DH ヘッド番号*/
  reg.h.dl= R;                     /*DL セクタ番号*/
  reg.h.ch= N;                     /*CH セクタ長コード （合わないセクタ長コードを指定するとエラー）*/
  segread(&seg);                   /* セグメントレジスタの値を取得 */
  seg.es= seg.ds;                  /*ES バッファの先頭アドレス[セグメント]*/
  reg.x.bp= (unsigned short)buff;  /*BP バッファの先頭アドレス[オフセット]*/
  int86x(DISKBIOS, &reg, &reg, &seg); /*セグメントレジスタも設定するのでint86x*/

/*  if(0!=reg.x.cflag)*/
/*  {*/
    /*エラーだったら アドレスoutにエラーコードを書く*/
/*    *buff = reg.h.ah;*/
/*    return FALSE;*/ /* 0:異常終了*/
/*  }*/
/*  return TRUE;*/ /* 0以外:正常終了*/
  return reg.h.ah;
}

/* VERIFY 1セクタからデータを読むが、バッファへの転送は行わない。（セクタが存在するか、データCRCエラーが無いかのチェック）
戻り値 0:異常終了 0以外:正常終了 */
unsigned char verify(unsigned char device
                    ,unsigned char MF     /* MFM / FM */
                    ,unsigned char C      /* シリンダ番号 */
                    ,unsigned char H      /* ヘッド番号 */
                    ,unsigned char R      /* セクタ番号 */
                    ,unsigned char N      /* セクタ長コード */
                    ,unsigned char *buff) /* 出力を書き込むアドレス */
{
  union REGS reg;
  struct SREGS seg;

  reg.h.ah= MF | 0x11; /*AH 00010001 READ DATA  セクタからデータを読む。*/
                       /*   |||+---- SEEK */
                       /*   ||+----- ~r   */
                       /*   |+------ MF   */
                       /*   +------- MT   */
  reg.h.al= device;                /*AL デバイスタイプユニット番号*/
  reg.x.bx= sectlength(N);         /*BX DTL*/
  reg.h.cl= C;                     /*CL シリンダ番号（ahのSEEKビットが1の時のみ有効）*/
  reg.h.dh= H;                     /*DH ヘッド番号*/
  reg.h.dl= R;                     /*DL セクタ番号*/
  reg.h.ch= N;                     /*CH セクタ長コード （合わないセクタ長コードを指定するとエラー）*/
  segread(&seg);                   /* セグメントレジスタの値を取得 */
  seg.es= seg.ds;                  /*ES バッファの先頭アドレス[セグメント]*/
  reg.x.bp= (unsigned short)buff; /*BP バッファの先頭アドレス[オフセット]*/
  int86x(DISKBIOS, &reg, &reg, &seg); /*セグメントレジスタも設定するのでint86x*/

  if(0!=reg.x.cflag)
  {
    /*エラーだったら アドレスoutにエラーコードを書く*/
    *buff = reg.h.ah;
    return FALSE; /* 0:異常終了*/
  }
  return TRUE; /* 0以外:正常終了*/
}

/* WRITE DATA 指定した1セクタにデータを書く シークはしない
戻り値 0:異常終了 0以外:正常終了 */
unsigned char writedata(unsigned char device
                       ,unsigned char MF     /* MFM / FM */
                       ,unsigned char C      /* シリンダ番号 */
                       ,unsigned char H      /* ヘッド番号 */
                       ,unsigned char R      /* セクタ番号 */
                       ,unsigned char N      /* セクタ長コード */
                       ,unsigned char *buff)  /* データバッファの先頭アドレス */
{
  union REGS reg;
  struct SREGS seg;

  reg.h.ah= MF | 0x05; /*AH 00000101 WRITE DATA  セクタにデータを書く。*/
                       /*   |||+---- SEEK */
                       /*   ||+----- ~r   */
                       /*   |+------ MF   */
                       /*   +------- MT （マルチトラック指定は使用しないこと）*/
  reg.h.al= device;                /*AL デバイスタイプユニット番号*/
  reg.x.bx= sectlength(N);         /*BX DTL データ長（DTLで示す書き込みがセクタの途中で終わった場合、残りを0で埋めて正常終了する）*/
  reg.h.cl= C;                     /*CL シリンダ番号（ahのSEEKビットが1の時のみ有効）*/
  reg.h.dh= H;                     /*DH ヘッド番号*/
  reg.h.dl= R;                     /*DL セクタ番号*/
  reg.h.ch= N;                     /*CH セクタ長コード （合わないセクタ長コードを指定するとエラー）*/
  segread(&seg);                   /* セグメントレジスタの値を取得 */
  seg.es= seg.ds;                  /*ES バッファの先頭アドレス[セグメント]*/
  reg.x.bp= (unsigned short)buff;  /*BP バッファの先頭アドレス[オフセット]*/
  int86x(DISKBIOS, &reg, &reg, &seg); /*セグメントレジスタも設定するのでint86x*/

  if(0!=reg.x.cflag)
  {
    return FALSE; /* 0:異常終了*/
    /*エラーが発生するのはIDが見つからない時*/
  }
  return TRUE; /* 0以外:正常終了*/
}

/* WRITE DELETED DATA 指定した1セクタにデリーデッドデータを書く シークはしない
戻り値 0:異常終了 0以外:正常終了 */
unsigned char writedeleted(unsigned char device
                          ,unsigned char MF     /* MFM / FM */
                          ,unsigned char C      /* シリンダ番号 */
                          ,unsigned char H      /* ヘッド番号 */
                          ,unsigned char R      /* セクタ番号 */
                          ,unsigned char N      /* セクタ長コード */
                          ,unsigned char *buff)  /* データバッファの先頭アドレス */
{
  union REGS reg;
  struct SREGS seg;

  reg.h.ah= MF | 0x09; /*AH 00001001 WRITE DELETED DATA*/
                       /*   |||+---- SEEK */
                       /*   ||+----- ~r   */
                       /*   |+------ MF   */
                       /*   +------- MT （マルチトラック指定は使用しないこと）*/
  reg.h.al= device;                /*AL デバイスタイプユニット番号*/
  reg.x.bx= sectlength(N);         /*BX DTL データ長（DTLで示す書き込みがセクタの途中で終わった場合、残りを0で埋めて正常終了する）*/
  reg.h.cl= C;                     /*CL シリンダ番号（ahのSEEKビットが1の時のみ有効）*/
  reg.h.dh= H;                     /*DH ヘッド番号*/
  reg.h.dl= R;                     /*DL セクタ番号*/
  reg.h.ch= N;                     /*CH セクタ長コード （合わないセクタ長コードを指定するとエラー）*/
  segread(&seg);                   /* セグメントレジスタの値を取得 */
  seg.es= seg.ds;                  /*ES バッファの先頭アドレス[セグメント]*/
  reg.x.bp= (unsigned short)buff;  /*BP バッファの先頭アドレス[オフセット]*/
  int86x(DISKBIOS, &reg, &reg, &seg); /*セグメントレジスタも設定するのでint86x*/

  if(0!=reg.x.cflag)
  {
    return FALSE; /* 0:異常終了*/
    /*エラーが発生するのはIDが見つからない時*/
  }
  return TRUE; /* 0以外:正常終了*/
}

/* FORMAT TRACK 一つのトラックをフォーマットする
戻り値 0:異常終了 0以外:正常終了
与えるデータは
C H R N の4バイト*セクタ数 をバッファに用意しておく
ギャップ長はBIOSが決めてくれる */
unsigned char trackfmt(unsigned char device
                      ,unsigned char MF     /* MFM / FM */
                      ,unsigned char C      /* シリンダ番号 */
                      ,unsigned char H      /* ヘッド番号 */
                      ,unsigned char N      /* セクタ長コード */
                      ,unsigned char SC    /* トラックあたりのセクタ数*/
                      ,unsigned char DATA   /* セクタに書き込むデータパターン */
                      ,unsigned char *buff) /* データバッファの先頭アドレス */
{
  union REGS reg;
  struct SREGS seg;

  reg.h.ah= MF | 0x1D; /*AH 00011101 FORMAT TRACK*/
                       /*    ||+---- SEEK */
                       /*    |+----- ~r   */
                       /*    +------ MF   */
  reg.h.al= device;                /*AL デバイスタイプユニット番号*/
  reg.x.bx= SC * 4;               /*BX DTL データ長*/
  reg.h.cl= C;                     /*CL シリンダ番号（ahのSEEKビットが1の時のみ有効）*/
  reg.h.dh= H;                     /*DH ヘッド番号*/
  reg.h.ch= N;                     /*CH セクタ長コード （合わないセクタ長コードを指定するとエラー）*/
  reg.h.dl= DATA;                  /*DL セクタに書き込むデータパターン*/
  segread(&seg);                   /* セグメントレジスタの値を取得 */
  seg.es= seg.ds;                  /*ES バッファの先頭アドレス[セグメント]*/
  reg.x.bp= (unsigned short)buff;  /*BP バッファの先頭アドレス[オフセット]*/
  int86x(DISKBIOS, &reg, &reg, &seg); /*セグメントレジスタも設定するのでint86x*/

  if(0!=reg.x.cflag)
  {
    return FALSE; /* 0:異常終了*/
    /*エラーが発生するのはメディアが入っていない時くらい？*/
  }
  return TRUE; /* 0以外:正常終了*/
}

/* READ DIAGNOSTIC
インデックス信号直後のセクタからデータを読む。
戻り値 0:異常終了 0以外:正常終了
READ DATAと違い、IDのエラー、データのエラーがあっても処理を続行する。
リザルトステータスはコマンド中に起きたすべてのエラーの論理和になる。
*/
unsigned char readdiag(unsigned char device
                      ,unsigned char MF     /* MFM / FM */
                      ,unsigned char H      /* ヘッド番号 */
                      ,unsigned char N      /* セクタ長コード */
                      ,unsigned char *buff) /* 出力を書き込むアドレス */
{
  union REGS reg;
  struct SREGS seg;

  reg.h.ah= MF | 0x02; /*AH 01000010 READ DIAGNOSTIC インデックス信号直後のセクタからデータを読む。*/
                       /*    ||+---- SEEK            READ DATAと違い、IDのエラー、データのエラーがあっても処理を続行する。*/
                       /*    |+----- ~r              リザルトステータスはコマンド中に起きたすべてのエラーの論理和になる。*/
                       /*    +------ MF */
  reg.h.al= device;                /*AL デバイスタイプユニット番号*/
  reg.x.bx= sectlength(N);         /*BX DTL*/
  reg.h.cl= 0;                     /*CL シリンダ番号（ahのSEEKビットが1の時のみ有効）*/
  reg.h.dh= H;                     /*DH ヘッド番号*/
  reg.h.dl= 1;                     /*DL セクタ番号（無意味）*/
  reg.h.ch= N;                     /*CH セクタ長コード （合わないセクタ長コードを指定するとエラー）*/
  segread(&seg);                   /* セグメントレジスタの値を取得 */
  seg.es= seg.ds;                  /*ES バッファの先頭アドレス[セグメント]*/
  reg.x.bp= (unsigned short)buff;  /*BP バッファの先頭アドレス[オフセット]*/
  int86x(DISKBIOS, &reg, &reg, &seg); /*セグメントレジスタも設定するのでint86x*/

  if(0!=reg.x.cflag)
  {
    /*エラーだったら アドレスbuffにエラーコードを書く*/
    *buff = reg.h.ah;
    return FALSE; /* 0:異常終了*/
  }
  return TRUE; /* 0以外:正常終了*/
}

/* 指定したトラックのセクタのタイプを調べる
戻り値 0:異常終了 0以外:セクタのタイプ
2D/2HD MFM/FM */
unsigned char sectsense(unsigned char device
                       ,unsigned char C
                       ,unsigned char H)
{
  unsigned char buff[4];

  seek(device, C);
  device &= 0x03; /*ユニット番号だけにする*/

  if(readid(FD2HD | device, FDMFM, H, buff)) return FD2HD | FDMFM;
  if(readid(FD2DD | device, FDMFM, H, buff)) return FD2DD | FDMFM;
  if(readid(FD2HD | device, FDFM,  H, buff)) return FD2HD | FDFM;
  if(readid(FD2DD | device, FDFM,  H, buff)) return FD2DD | FDFM;
  /* 2Dと2DDの見分けはつかないか？
  シークしてみれば2Dと2DDの違いでシークエラー出るかな？*/
  return FALSE;
}

/* 指定したトラックのセクタの変調方式を調べる
MFM/FM */
unsigned char modsense(unsigned char device
                      ,unsigned char C
                      ,unsigned char H)
{
  unsigned char buff[4];

  seek(device, C);
  if(readid(device, FDMFM, H, buff)) return FDMFM;
  /*if(readid(device, FDFM,  H, buff)) return FDFM;*/
  return FDFM;  /*FDFM=0なので FALSEは使えない*/
}

/* セクタ長コードからセクタ長を得る
戻り値 0:異常終了 0以外:セクタのバイト数 */
unsigned short sectlength(unsigned char N)  /* セクタ長コード */
{
  switch(N)
  {
    case 0:
      return 128;
    case 1:
      return 256;
    case 2:
      return 512;
    case 3:
      return 1024;
    case 4:
      return 2048;
    case 5:
      return 4096;
  }
  return 0;
}

/*READDIAG で先頭セクタの番号を調べる
バッファサイズはセクタ長の分必要
正常終了ならバッファ先頭にセクタ番号
異常終了ならバッファ先頭にエラーコード*/
unsigned char secthead(unsigned char drive
                      ,unsigned char MF     /* MFM / FM */
                      ,unsigned char H      /* ヘッド番号 */
                      ,unsigned char *buff) /* 出力を書き込むアドレス */
{
  unsigned char n;

  /*セクタ長コードを調べる*/
  if(! readid(drive, MF, H, buff)) return FALSE;
  n=buff[3];

  if(! readdiag(drive, MF, H, n, buff))
  {
    /*エラーならエラーコードがバッファの先頭アドレスに入る*/
    /*puts(errmsg(buff[0]));*/
    return FALSE;
  }
  drive &= 0x0F; /*ドライブ番号だけにする*/
  switch(drive)
  {
    case 0:
      buff[0] = DISKSTAT.D0.R - 1;    /*読み込んだ次のセクタ番号を示しているので引き算*/
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
  switch(ah & 0xF8)
  {
    case 0x08:
      return "Corrected data";
    case 0x38:
      return "Illegal disk Addres";
    case 0x88:
      return "Direct access an alternate track";
    case 0xB8:
      return "Data error";
    case 0xC8:
      return "Seek error";
    case 0xD8:
      return "Not read the alternate track";
  }

  switch(ah & 0xF0)
  {
    case 0x20:
      return "DMA Boundary";
    case 0x30:
      return "End of cylinder";
    case 0x40:
      return "Equipment check over run";
    case 0x60:
      return "Not ready";
    case 0x70:
      return "Not writable";
    case 0x80:
      return "Error 0x80";
    case 0x90:
      return "Time out";
    case 0xA0:
      return "ID CRC error";
    case 0xB0:
      return "DATA CRC error";
    case 0xC0:
      return "No data (ID not found)";
    case 0xD0:
      return "Bad cylinder";
    case 0xE0:
      return "Missing address mark (ID)";
    case 0xF0:
      return "Missing address mark (DATA)";
    }
    return "Unknown error";
}
