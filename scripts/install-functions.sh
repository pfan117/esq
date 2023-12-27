#!/bin/bash

function copy_files_into_install_folder
{
	INSTALL_SRC_PREFIX=$1
	INSTALL_DST_PREFIX=$2

	mkdir -p ${INSTALL_DST_PREFIX}/bin || exit 1
	mkdir -p ${INSTALL_DST_PREFIX}/lib/esq || exit 1
	mkdir -p ${INSTALL_DST_PREFIX}/include/esq || exit 1

	cp -f ${INSTALL_SRC_PREFIX}/public/esq.h ${INSTALL_DST_PREFIX}/include/esq || exit 1
	cp -f ${INSTALL_SRC_PREFIX}/public/h2esq.h ${INSTALL_DST_PREFIX}/include/esq || exit 1
	cp -f ${INSTALL_SRC_PREFIX}/public/libesq.h ${INSTALL_DST_PREFIX}/include/esq || exit 1
	cp -f ${INSTALL_SRC_PREFIX}/public/esqll.h ${INSTALL_DST_PREFIX}/include/esq || exit 1
	cp -f ${INSTALL_SRC_PREFIX}/public/pony-bee.h ${INSTALL_DST_PREFIX}/include/esq || exit 1

	cp -f ${INSTALL_SRC_PREFIX}/esq ${INSTALL_DST_PREFIX}/bin/ || exit 1
	cp -f ${INSTALL_SRC_PREFIX}/modules/*.so ${INSTALL_DST_PREFIX}/lib/esq || exit 1
}

# eos
