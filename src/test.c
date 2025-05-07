/*
PCAT FDC 動作テスト

This code is provided under a CC0 Public Domain License.
http://creativecommons.org/publicdomain/zero/1.0/

2022年7月16日 佐藤恭一 kyoutan.jpn.org
*/

/*#include <DOS.H>*/
#include <stdio.h>
#include "diskPCAT.h"
#include "timePCAT.h"
#include "diskbios.h"

#define BUFFSIZE (1024 * 16)
unsigned char BUFF[BUFFSIZE];

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

/* 外に出していない関数 */
#define fdc_status() inp(0x03f4)
unsigned char fdc_wRQM(unsigned char BusyChk);
unsigned char fdc_rRQM();
void fdc_drivesel(unsigned char device);
void fdc_motoroff();
unsigned char fdc_biosreset(char drive);
void fdc_500kbps();
void fdc_300kbps();
void fdc_250kbps();
void fdc_3mode500kbps();
void fdc_3mode500kbps_360rpm();
void fdc_3mode250kbps();
void fdc_setdatarate(unsigned char drtdsr);

#define PIC1REG1 0x020
#define PIC1REG2 0x021
#define pic_IMRr() inp(PIC1REG2)
#define pic_IMRw(data) outp(PIC1REG2, data)

void dump(unsigned char *buff, unsigned short size, unsigned char col)
{
    unsigned short count;
    count = 1;
    for (; 0 != size; size--)
    {
        printf("%02x ", *buff);
        if (0 == (count % col))
            printf("\r\n");
        buff++;
        count++;
    }
    /*printf("\r\n");*/
}

/*ポインタ使い方忘れたのでテスト*/
void test(unsigned char *buff, unsigned short size)
{
    for (; 0 != size; size--)
    {
        printf("%d ", *buff);
        buff++;
    }
    printf("\r\n");
}

void testw(unsigned char *buff, unsigned short size)
{
    for (; 0 != size; size--)
    {
        *buff = size;
        buff++;
    }
}

void test_timeout(void)
{
    unsigned short start, count;
    count = 0;
    start = timer_get16();

    while (TRUE)
    {
        if (timeout_ms(1000, start))
            break;

        printf("%d ", count);
        wait_ms(500);
        count = count + 5;
    }
}

void test_irqmask(void)
{
    int count;
    /*割り込みマスクのテスト*/
    printf("IMR:%0x\r\n", pic_IMRr());
    pic_IMRw((1 << 0) | pic_IMRr()); /* bit0 をセット(timer割り込み禁止) */
    printf("IMR:%0x\r\n", pic_IMRr());
    for (count = 0; count != 0xff; count++)
    {
        printf("%02x ", timer_get16()); /* timer割り込みを禁止にしたので値が変わらない*/
    }
    printf("\r\n");

    printf("IMR:%0x\r\n", pic_IMRr());
    pic_IMRw(~(1 << 0) & pic_IMRr()); /* bit0 をクリア(timer割り込み許可) */
    printf("IMR:%0x\r\n", pic_IMRr());
    for (count = 0; count != 0xff; count++)
    {
        printf("%02x ", timer_get16()); /* timer割り込みを許可したので値が変わる */
    }
    printf("\r\n");
}

void printstatus(void)
{
    printf("fdc status reg %0x\r\n", fdc_status());
}

void test_track00(void)
{
    printf("track00(0) ");
    if (FALSE == track00(0))
        printf("time out\r\n");
    else
        printf("ok\r\n");
}

void test_seek(unsigned char C)
{
    printf("seek %d ", C);
    if (FALSE == seek(0, C))
        printf("time out\r\n");
    else
        printf("ok\r\n");
}

void test_readid(void)
{
    printf("read id ");
    if (FALSE == readid(0x80, FDMFM, 0, BUFF))
    {
        printf("error %0x\r\n", BUFF[0]);
    }
    dump(BUFF, 4, 4);
}

void test_readdata(void)
{
    unsigned char ret;
    printf("read id ");
    if (FALSE == readid(0x80, FDMFM, 0, BUFF))
    {
        printf("error %0x\r\n", BUFF[0]);
    }
    dump(BUFF, 4, 4);

    printstatus();
    printf("read data ");
    ret = readdata(0x80, FDMFM, BUFF[0], BUFF[1], BUFF[2], BUFF[3], BUFF);
    if (ENoError != ret)
    {
        printf("error %0x\r\n", ret);
        printf(errmsg(ret));
        printf("\r\n");
    }
    else
        puts("ok");
    dump(BUFF, 512 + (32 * 4), 32);
}

void test_readdeleted(void)
{
    unsigned char ret;

    printf("read id ");
    if (FALSE == readid(0x80, FDMFM, 1, BUFF))
    {
        printf("error %0x\r\n", BUFF[0]);
    }
    dump(BUFF, 4, 4);

    printf("read deleted.. ");
    ret = readdeleted(0x80, FDMFM, BUFF[0], BUFF[1], BUFF[2], BUFF[3], BUFF);
    if (ENoError != ret)
    {
        printf(errmsg(ret));
        printf("\r\n");
    }
    else
        puts("ok");
    dump(BUFF, 512 + (32 * 4), 32);
    printstatus();
}
void test_verify(void)
{
    unsigned char ret;

    printf("read id ");
    if (FALSE == readid(0x80, FDMFM, 1, BUFF))
    {
        printf("error %0x\r\n", BUFF[0]);
    }
    dump(BUFF, 4, 4);
    printf("verify.. ");
    /* でたらめなIDから読んでエラーを確認する */
    ret = verify(0x80, FDMFM, 10, BUFF[1], BUFF[2], BUFF[3], BUFF);
    if (FALSE == ret)
    {
        printf(errmsg(BUFF[0]));
        printf("\r\n");
    }
    else
        puts("ok");
    printstatus();
}

void test_writedata(void)
{
    unsigned char ret;

    test_seek(75);
    printf("read id ");
    if (FALSE == readid(0x80, FDMFM, 1, BUFF))
    {
        printf("error %0x\r\n", BUFF[0]);
    }
    dump(BUFF, 4, 4);
    printf("write data.. ");
    ret = writedata(0x80, FDMFM, 75, 0, 1, 2, BUFF);
    if (FALSE == ret)
        puts("error");
    else
        puts("ok");
    printstatus();
    dump(BUFF, 512 + (32 * 4), 32);

    printf("read data ");
    ret = readdata(0x80, FDMFM, 75, 0, 1, 2, BUFF);
    if (ENoError != ret)
    {
        printf("error %0x\r\n", ret);
        printf(errmsg(ret));
        printf("\r\n");
    }
    dump(BUFF, 512 + (32 * 4), 32);
    printstatus();
}

void test_readdiag(void)
{
    unsigned char ret;
    printf("readdiag.. ");
    ret = readdiag(0x80, FDMFM, 0, 2, BUFF);
    if (FALSE == ret)
        puts("error");
    else
        puts("ok");
    printf("C:%02x H:%02x R:%02x N:%02x\r\n", DISKSTAT.D0.C, DISKSTAT.D0.H, DISKSTAT.D0.R, DISKSTAT.D0.N);
    dump(BUFF, 512 + (32 * 4), 32);
    printstatus();
}

void test_sectsense(void)
{
    unsigned char ret;
    printf("sectsense.. ");
    ret = sectsense(0x80, BUFF[0], BUFF[1], 0x30);
    if (0 == ret)
    {
        puts("error");
        return;
    }
    printf("sectsense ");
    if (FD2HD == (ret & FD2HD))
        printf("2HD ");
    else
        printf("2DD ");

    if (0 == (ret & FDMFM))
        printf("FM\r\n");
    else
        printf("MFM\r\n");
}

void test_modsense(void)
{
    unsigned char ret;
    printf("modsense.. ");
    ret = modsense(0x80, BUFF[0], BUFF[1]);
    if (ret == FDMFM)
        puts("modsense : MFM");
    else
        puts("modsense : FM");
}

void test_secthead(void)
{
    printf("secthead.. ");
    if (FALSE == secthead(0x80, FDMFM, 0, BUFF))
    {
        printf(errmsg(BUFF[0]));
        puts("");
        return;
    }
    printf("%d\r\n", BUFF[0]);
}

extern unsigned char fdc_getGPL(unsigned char MF, unsigned char N, unsigned char format);
extern unsigned char fdc_getfmt(unsigned char device);
extern unsigned char fdc_getEOT(unsigned char MF, unsigned char N, unsigned char format);
void tracksize(unsigned char device)
{
    unsigned char MF, N, SC, format;
    unsigned short gap3, sectsize, trsize;

    if (FDMFM == (device & FDMFM))
    {
        MF = FDMFM;
    }
    else
    {
        MF = FDFM;
    }

    format = fdc_getfmt(device);

    for (N = 0; N < 8; N++)
    {
        SC = fdc_getEOT(MF, N, format);
        gap3 = fdc_getGPL(MF, N, format);
        sectsize = 22 /*ID*/ + 22 /*GAP2*/ + sectlength(N) + 18 /* DAM + CRC */ + gap3;
        trsize = 146 /* pre */
                 + (unsigned short)SC * sectsize;
        printf("%d*%2d:%d ", sectlength(N), SC, trsize);
    }
    printf("\r\n");
}

void print_tracksize(void)
{
    unsigned char device;

    /* 2HD MFM 500kbps 500/8*1024byte/s = 64000yte/s
    360/60回転/s 6回転/s
    300/60回転/s 5回転/s */
    printf("2HD 360rpm %d / 2HD 300rpm %d\r\n", 1024 / 8 * 500 / 6, 1024 / 8 * 500 / 5);

    device = FD2DD | FDFM;
    printf("2DD     FM  ");
    tracksize(device);

    device = FD2DD | FDMFM;
    printf("2DD     MFM ");
    tracksize(device);

    DISKWORK.F2HD360 = MODE360;
    device = FD2HD | FDFM;
    printf("2HD FM  360 ");
    tracksize(device);

    device = FD2HD | FDMFM;
    printf("2HD MFM 360 ");
    tracksize(device);

    DISKWORK.F2HD360 = MODE300;
    device = FD2HD | FDFM;
    printf("2HD FM  300 ");
    tracksize(device);

    device = FD2HD | FDMFM;
    printf("2HD MFM 300 ");
    tracksize(device);
}

/* 記録密度、回転数の切り替え動作テスト */
void modechange(void)
{
    unsigned char device, H;
    device = 0;
    H = 0;

    diskinit(device);
    DISKWORK.F2HD360 = MODE300;
    if (readid(FD2HD | device, FDMFM, H, BUFF))
    {
        printf("2HD MFM 300 %2d %2d %2d %2d\r\n", BUFF[0], BUFF[1], BUFF[2], BUFF[3]);
        return;
    }
    /*IDが見つからなかった後はFDCの応答がおかしくなって、読めるはずの設定でもリードエラーが出るので、
    FDCをリセットする必要がある。*/
    diskinit(device);
    DISKWORK.F2HD360 = MODE300;
    if (readid(FD2HD | device, FDFM, H, BUFF))
    {
        printf("2HD FM 300 %2d %2d %2d %2d\r\n", BUFF[0], BUFF[1], BUFF[2], BUFF[3]);
        return;
    }

    diskinit(device);
    DISKWORK.F2HD360 = MODE360;
    if (readid(FD2HD | device, FDMFM, H, BUFF))
    {
        printf("2HD MFM 360 %2d %2d %2d %2d\r\n", BUFF[0], BUFF[1], BUFF[2], BUFF[3]);
        return;
    }

    diskinit(device);
    DISKWORK.F2HD360 = MODE360;
    if (readid(FD2HD | device, FDFM, H, BUFF))
    {
        printf("2HD FM 360 %2d %2d %2d %2d\r\n", BUFF[0], BUFF[1], BUFF[2], BUFF[3]);
        return;
    }

    diskinit(device);
    if (readid(FD2DD | device, FDMFM, H, BUFF))
    {
        printf("2DD MFM %2d %2d %2d %2d\r\n", BUFF[0], BUFF[1], BUFF[2], BUFF[3]);
        return;
    }

    diskinit(device);
    if (readid(FD2DD | device, FDFM, H, BUFF))
    {
        printf("2DD FM %2d %2d %2d %2d\r\n", BUFF[0], BUFF[1], BUFF[2], BUFF[3]);
        return;
    }
    /*いきなり2DD指定すると2DD読めた。*/
    /*IDが見つからないとその後しばらくリードエラーが続くので
    転送レートを変更するときFDCリセット必要かも*/
    puts("READ ERROR");
}

/* 2HDの1.44MB(300RPM)と1.2MB(360RPM)の切り替えテスト
2PINのモードセレクト信号が変化することを確認した。
1.2MBが読めないのは手持ちのマザーボードが 3MODE対応ではないみたい */
void rotationspeed(void)
{

    puts("500kbps"); /* 2PIN MODE SELECT H */
    fdc_setdatarate(0x00);
    wait_ms(2000);

    puts("300kbps"); /* 2PIN MODE SELECT L */
    fdc_setdatarate(0x01);
    wait_ms(2000);

    puts("250kbps"); /* 2PIN MODE SELECT L */
    fdc_setdatarate(0x02);
    wait_ms(2000);

    puts("1000kbps"); /* 2PIN MODE SELECT H */
    fdc_setdatarate(0x03);
    wait_ms(2000);

    diskinit(0);
    fdc_drivesel(0);

    puts("3mode500kbps"); /* 2PIN MODE SELECT H */
    fdc_setdatarate(0x04);
    wait_ms(2000);

    puts("3mode500kbps_360rpm"); /* 2PIN MODE SELECT L 300Mbpsになっているので1.2MB読めない */
    fdc_setdatarate(0x05);
    wait_ms(2000);

    puts("3mode250kbps"); /* 2PIN MODE SELECT L */
    fdc_setdatarate(0x06);
    wait_ms(2000);

    puts("3mode1000kbps"); /* 2PIN MODE SELECT H */
    fdc_setdatarate(0x07);
    wait_ms(2000);
}

void test_format(void)
{
    unsigned char count;

    diskinit(0);
    DISKWORK.F2HD360 = MODE300;
    puts("READID");
    if (readid(FD2HD | 0, FDMFM, 0 /*H*/, BUFF))
    {
        dump(BUFF, 4, 4);
        puts("");
    }
    else
    {
        puts("ERROR");
    }

    puts("FORMAT");
    for (count = 0; 8 > count; count++)
    {
        BUFF[count * 4 + 0] = 80;    /*C*/
        BUFF[count * 4 + 1] = 0;     /*H*/
        BUFF[count * 4 + 2] = count; /*R*/
        BUFF[count * 4 + 3] = 3;     /*N 2:512 3:1024 */
    }

    if (!trackfmt(0 /*device*/, FDMFM /*FM/MFM*/,
                  BUFF[0] /*C*/,
                  BUFF[1] /*H*/,
                  BUFF[3] /*N*/,
                  8 /*SC*/,
                  0x5e /*DATA*/,
                  BUFF))
    {
        /*エラー*/
        printf("ERROR ST0:%2x ST1:%2x ST2:%2x\r\n",
               DISKSTAT.D0.ST0,
               DISKSTAT.D0.ST1,
               DISKSTAT.D0.ST2);
    }
    else
    {
        puts("NO ERROR");
    }

    puts("READID");
    if (readid(FD2HD | 0, FDMFM, 0 /*H*/, BUFF))
    {
        dump(BUFF, 4, 4);
        puts("");
    }
    else
    {
        puts("ERROR");
    }
}

void test_file(void)
{
    FILE *file;
    char str[100];

    file = fopen("test.tmp", "w");
    fputs("01234567890123456789", file);
    fclose(file);



    file = fopen("test.tmp", "r+b");
    fgets(str, sizeof str, file);
    puts(str);

    fseek(file, 5, SEEK_SET);
    fputs("abcd", file);

    fseek(file, 0, SEEK_SET);
    fgets(str, sizeof str, file);
    puts(str);
    
    fclose(file);
}

int main()
{
    /*
    printstatus();
    printf("fdc wRQM %0x\r\n", fdc_wRQM(1));
    printf("fdc rRQM %0x\r\n", fdc_rRQM());

    diskinit(0);
    test_track00();
*/

    /*
         test_seek(50);
     */
    /*
    test_seek(1);
    */
    /*
         test_readid();

         test_readdata();
         test_readdeleted();
         test_verify();

         test_writedata();

         test_readid();
         test_readid();
         test_readid();
         test_readid();
         test_readid();
         test_readid();
         test_readid();
         test_readid();
         test_readid();
         test_readid();
    */
    /*
    test_readid();
    printstatus();

    test_sectsense();
    */
    /* 2HD 300rpmと360rpm自動検出を試みる */
    /*
       printstatus();
       test_modsense();
       printstatus();
       test_secthead();
       printstatus();

       test_readdiag();
    */
    /*
               test_irqmask();
       */
    /*
        disk_exit();
    print_tracksize();
    */
    /*
            diskinit(0);
            test_track00();
            test_readid();
            test_readdata();
            test_readdiag();
            test_secthead();
            test_sectsense();
            test_seek(1);
            test_secthead();
            test_readdiag();
            */
    /*2023年9月4日 関数単体テストは正常に動作する*/

    /*2023年10月26日 1.44MB以外読めない2DDも*/
    /*IDリードエラーもたびたび出る、リトライしたほうがいいのかな？*/
    /*modechange();*/
    /*rotationspeed();*/
    /*test_format();*/
    /*diskexit();*/
    test_file();
    return 0; /* 正常終了はゼロ */
}