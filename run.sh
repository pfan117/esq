#!/bin/bash

if [[ -f /proc/sys/net/ipv4/tcp_fastopen ]] ; then
	TCP_FAST_OPEN_VAL=`cat /proc/sys/net/ipv4/tcp_fastopen`
	if [[ "3" == ${TCP_FAST_OPEN_VAL} ]] ; then
		:
	else
		echo "ERROR: value of /proc/sys/net/ipv4/tcp_fastopen supposed to be 3!"
		exit -1
	fi
else
	echo "ERROR: kernel not support TCP fast open"
	exit -1
fi

function show_usage
{
	echo ""
	echo "Usage: $0 [perf=web|node0|node1] [G] [gprof] [mt]"
	echo ""
	echo "examples:"
	echo "    $0"
	echo "    $0 perf=web"
	echo "    $0 perf=node0"
	echo "    $0 perf=node1"
	echo "    $0 gprof"
	echo ""
	echo "    $0 mt"
	echo "    $0 range_map"
	echo "    $0 single"
	echo "    $0 pkg"
	echo "    $0 doc | document"
	echo "    $0 c | clean"
	echo "    $0 install"
	echo "    $0 publish"
	echo ""
}

ENABLE_PERF="no"
ENABLE_GPROF="no"
MODULE_TEST_ENABLE="no"
SINGLE_NODE_MODE_ENABLE="no"
CREATE_PKG="no"
DO_DOXYGEN="no"
DO_CLEAN="no"
DO_INSTALL="no"
DO_PUBLISH="no"

for opt in "$@"
do
case $opt in
	perf=*)
	ENABLE_PERF=`echo $opt | sed 's/[-a-zA-Z0-9]*=//'`
	;;
	gprof)
	ENABLE_GPROF="yes"
	;;
	mt)
	MODULE_TEST_ENABLE="yes"
	;;
	single)
	SINGLE_NODE_MODE_ENABLE="yes"
	;;
	pkg)
	CREATE_PKG="yes"
	;;
	doc|document)
	DO_DOXYGEN="yes"
	;;
	c|clean)
	DO_CLEAN="yes"
	;;
	install)
	DO_INSTALL="yes"
	;;
	publish | pub)
	DO_PUBLISH="yes"
	;;
	*)
	show_usage
	exit 1
	;;
esac
done

function create_pkg
{
	PKG_FILENAME="../esq-$(date +%Y%m%d).tgz"
	rm -f tags
	make clean || exit 1
	rm -f $PKG_FILENAME
	tar czf $PKG_FILENAME . || exit 1
	mv -f $PKG_FILENAME . || exit 1
}

if [[ "yes" == "${DO_CLEAN}" ]] ; then
	source "scripts/build-functions.sh"
	do_clean
	exit 0
fi

if [[ "${DO_DOXYGEN=}" == "yes" ]] ; then
	echo "INFO: doxygen..."
	exit 0
fi

if [[ "yes" == "${CREATE_PKG}" ]] ; then
	create_pkg
	exit 0
fi

if [[ "${DO_INSTALL}" == "yes" ]] ; then
	source "scripts/install-for-development.sh" || exit 1
	do_install
	exit 0
fi

if [[ "${DO_PUBLISH}" == "yes" ]] ; then
	source "scripts/build-functions.sh" || exit 1
	source "scripts/publish-functions.sh" || exit 1
	do_publish
	exit 0
fi

source "scripts/open-nodes.sh"
source "scripts/build-functions.sh"

make_out_dir

if [[ "$ENABLE_GPROF" == "yes" ]] ; then
	build_target "gprof"
elif [[ "$MODULE_TEST_ENABLE" == "yes" ]] ; then
	build_target "mt"
else
	build_target "debug"
fi

ctags -R *

echo "run..."

if [[ "${MODULE_TEST_ENABLE}" == "yes" ]] ; then
	./esq modules/mt.so || exit
	exit 0
fi

if [[ "${SINGLE_NODE_MODE_ENABLE}" == "yes" ]] ; then
	open_web_node
	sleep_loop
	exit 0
fi

# default behavior, open 3 instances

open_db_node_0
open_db_node_1
open_web_node
sleep_loop

# eof
