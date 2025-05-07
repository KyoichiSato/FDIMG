/*
fdimg.cの関数を使うときのヘッダ

This code is provided under a CC0 Public Domain License.
https://creativecommons.org/publicdomain/zero/1.0/deed.ja

2023年11月12日作成 佐藤恭一 kyoutan.jpn.org
*/

/* fdimg.c で定義している関数 */
extern int addext(char filename[], char exte[]);
extern int d88idx_read(void);
extern int d88idx_write(void);
extern int d88sect_read(void);

/* fdimg.c で定義しているグローバル変数 */
extern struct st_d88_header D88IDX;
extern struct st_d88_sect_header D88SECT;
/*extern unsigned char BUFF[];*/
extern unsigned char IDBUFF[];
extern FILE *D88FILE;
