# FDIMG.EXE
# LSI C-86用のmakefile
#
# This code is provided under a CC0 Public Domain License.
# https://creativecommons.org/publicdomain/zero/1.0/deed.ja
#
# 2016年 5月18日 作成 佐藤恭一 kyoutan.jpn.org
# 2023年10月29日 更新

all : fdimg98.exe fdimgAT.exe fdseek98.exe fdseekAT.exe

clean :
	-del *.obj
	-del test.exe

fdimg98.exe : PC98.obj diskPC98.obj d88info.obj diskinfo.obj
	lcc -O -o FDIMG98.EXE PC98.obj diskPC98.obj d88info.obj diskinfo.obj -lnoexpand.obj

fdimgAT.exe : PCAT.obj diskPCAT.obj timePCAT.obj d88info.obj diskinfo.obj
	lcc -O -o FDIMGAT.EXE PCAT.obj diskPCAT.obj timePCAT.obj d88info.obj diskinfo.obj -lnoexpand.obj

fdseek98.exe : fdseek.c diskPC98.obj diskbios.h fdimage.h
	lcc -O -o FDSEEK98.EXE fdseek.c diskPC98.obj -lnoexpand.obj

fdseekAT.exe : fdseek.c diskPCAT.obj timePCAT.obj diskbios.h fdimage.h
	lcc -O -o FDSEEKAT.EXE fdseek.c diskPCAT.obj timePCAT.obj -lnoexpand.obj

test.exe : test.c diskPCAT.obj timePCAT.obj
	lcc -O -o TEST.EXE test.c diskPCAT.obj timePCAT.obj -lnoexpand.obj


PC98.obj : fdimg.c diskbios.h fdimage.h
	lcc -DJPN -c -O -o PC98.obj fdimg.c

PCAT.obj : fdimg.c diskbios.h fdimage.h
	lcc -c -O -o PCAT.obj fdimg.c

diskPC98.obj : diskbios.h diskPC98.h
diskPCAT.obj : diskbios.h diskPCAT.h timePCAT.h
timePCAT.obj : diskbios.h timePCAT.h
d88info.obj : fdimg.h fdimage.h diskbios.h
diskinfo.obj : fdimg.h diskbios.h
