#!/bin/bash

TMP_ETC_LIST=tmp_etc_find_list.inc
rm ${TMP_ETC_LIST}
echo "const char * etc_list[] = {" > ${TMP_ETC_LIST}
for L in `find /etc`
do
	echo "\"$L\"," >> ${TMP_ETC_LIST}
done
echo "};" >> ${TMP_ETC_LIST}


TMP_USR_INCLUDE_LIST=tmp_usr_include_find_list.inc
rm ${TMP_USR_INCLUDE_LIST}
echo "const char * usr_include_list[] = {" > ${TMP_USR_INCLUDE_LIST}
for L in `find /usr/include`
do
	echo "\"$L\"," >> ${TMP_USR_INCLUDE_LIST}
done
echo "};" >> ${TMP_USR_INCLUDE_LIST}

make || exit 1

./a.out

# eos
