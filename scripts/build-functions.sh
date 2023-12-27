#!/bin/bash

function do_clean
{
	make clean
	rm -rf out
}

function make_out_dir
{
	mkdir -p out || exit 1
	DIR_NAMES=`find src/* mco/* session/* utils/* libs/* bee/* modules/* nghttp2_wrapper/* | xargs dirname {} | sort | uniq`
	for DIR_NAME in ${DIR_NAMES}
	do
		if [[ "." == "${DIR_NAME}" ]] ; then
			continue
		fi
		mkdir -p out/${DIR_NAME}
	done
}

function build_target
{
	bt_type=$1
	if [[ "$bt_type" == "gprof" ]] ; then
		export LD_LIBRARY_PATH="${HOME}/local/lib/:/usr/local/lib:${LD_LIBRARY_PATH}"
		make GCC=gcc debug=1 gprof=1 -j || exit 1
	elif [[ "$bt_type" == "mt" ]] ; then
		export LD_LIBRARY_PATH="${HOME}/local-gcc-memcheck/lib/:${HOME}/local/lib:/usr/local/lib:${LD_LIBRARY_PATH}"
		make mt GCC=gcc debug=1 memcheck=1 mt=1 -j || exit 1
	elif [[ "$bt_type" == "debug" ]] ; then
		export LD_LIBRARY_PATH="${HOME}/local-gcc-memcheck/lib/:${HOME}/local/lib:/usr/local/lib:${LD_LIBRARY_PATH}"
		make GCC=gcc debug=1 memcheck=1 -j || exit 1
	else
		# build for release
		make -j || exit 1
	fi

	echo ""
	# ldd esq
}

# eof
