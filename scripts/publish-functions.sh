#!/bin/bash

TMP_PUBLISH_FOLDER="publish_tmp"
PUBLISH_PKG="deploy_esq.tgz"

function do_publish	{
	rm -rf ${TMP_PUBLISH_FOLDER}
	rm -f ${PUBLISH_PKG}
	mkdir -p ${TMP_PUBLISH_FOLDER}/scripts || exit 1
	mkdir -p ${TMP_PUBLISH_FOLDER}/release/modules || exit 1
	mkdir -p ${TMP_PUBLISH_FOLDER}/debug/modules|| exit 1

	cp scripts/deploy.sh ${TMP_PUBLISH_FOLDER} || exit 1
	cp scripts/install-functions.sh ${TMP_PUBLISH_FOLDER}/scripts || exit 1

	do_clean
	make_out_dir
	build_target "release"
	cp -r public ${TMP_PUBLISH_FOLDER}/release || exit 1
	cp esq ${TMP_PUBLISH_FOLDER}/release || exit 1
	cp modules/simple-file-service.so ${TMP_PUBLISH_FOLDER}/release/modules || exit 1

	do_clean
	make_out_dir
	build_target "debug"
	cp -r public ${TMP_PUBLISH_FOLDER}/debug || exit 1
	cp esq ${TMP_PUBLISH_FOLDER}/debug || exit 1
	cp modules/simple-file-service.so ${TMP_PUBLISH_FOLDER}/debug/modules || exit 1

	date > ${TMP_PUBLISH_FOLDER}/timestamp.txt || exit 1

	echo "INFO: create deploy package"
	cd ${TMP_PUBLISH_FOLDER} || exit 1
	tar czf ../${PUBLISH_PKG} . || exit 1

	echo "INFO: clean"
	cd ..
	rm -rf ${TMP_PUBLISH_FOLDER}

	echo ""
}

# eos
