#!/bin/bash

source "scripts/build-functions.sh"
source "scripts/install-functions.sh"

function do_install	{
	do_clean
	make_out_dir

	echo "INFO: building for debug mode"
	build_target "debug"

	echo "INFO: copying files"
	copy_files_into_install_folder "./" "${HOME}/local-gcc-memcheck"

	do_clean
	make_out_dir

	echo "INFO: building"
	build_target "release"

	echo "INFO: copying files"
	copy_files_into_install_folder "./" "${HOME}/local"

	do_clean
	make_out_dir

	echo "INFO: install success"
	echo ""
}

# eof
