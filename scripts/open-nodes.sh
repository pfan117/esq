function open_db_node_0
{
	if [[ "${ENABLE_PERF}" == "node0" ]] ; then
		echo "INFO: perfing on DB node0"
		perf record -e cpu-clock -g ./esq \
		modules/server-stop.so  \
		demo/range_map.mkc \
		demo/db0/config.mkc \
		&
	else
		./esq \
		modules/server-stop.so  \
		demo/range_map.mkc \
		demo/db0/config.mkc \
		&
	fi

	node_pid=$!

	echo "node 0 pid = " $node_pid
}

function open_db_node_1
{
	if [[ "${ENABLE_PERF}" == "node1" ]] ; then
		echo "INFO: perfing on DB node1"
		perf record -e cpu-clock -g ./esq \
		modules/server-stop.so  \
		demo/range_map.mkc \
		demo/db1/config.mkc \
		&
	else
		./esq \
		modules/server-stop.so  \
		demo/range_map.mkc \
		demo/db1/config.mkc \
		&
	fi

	node_pid=$!

	echo "node 1 pid = " $node_pid
}

function open_web_node
{
	if [[ "${ENABLE_PERF}" == "web" ]] ; then
		echo "INFO: perfing on web node"
		perf record -e cpu-clock -g ./esq \
		modules/server-stop.so  \
		modules/simple-file-service.so \
		modules/foo_server.so  \
		demo/range_map.mkc \
		demo/web/config.mkc \
		&
	else
		./esq \
		modules/server-stop.so  \
		modules/simple-file-service.so \
		modules/foo_server.so  \
		demo/range_map.mkc \
		demo/web/config.mkc \
		&
	fi

	node_pid=$!

	echo "web node pid = " $node_pid
}

function sleep_loop
{
	while true; do
		sleep 9000
	done

	echo ""
	ps -ef
	echo ""
}

# eof
