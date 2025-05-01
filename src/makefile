# LSI C-86用のmakefile
# このファイルは CC0 パブリック・ドメイン提供です。
# https://creativecommons.org/publicdomain/zero/1.0/deed.ja
#
# 2016年5月18日 佐藤恭一 kyoutan.jpn.org

fdimg.exe : fdimg.obj diskbios.obj
	lcc -o fdimg.exe fdimg.obj diskbios.obj -lnoexpand.obj
fdimg.obj diskbios.obj : diskbios.h
