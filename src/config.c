#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "public/esq.h"
#include "public/libesq.h"
#include "internal/monkeycall.h"
#include "internal/config.h"

#define MAX_FILE_SIZE	(1024 * 1024)
#define ERROR_LOG_BUF_SIZE	(1024)

int esq_load_configuration_from_file(const char * file)	{
	char errbuf[ERROR_LOG_BUF_SIZE];
	char * code_buf;
	struct stat sb;
	int conf_size;
	int errlen;
	int fd;
	int r;

	if (path_is_very_safe(file))	{
		;
	}
	else	{
		printf("ERROR: %s, %d: configuration file name is not safe\n", __func__, __LINE__);
		return ESQ_ERROR_PARAM;
	}

	fd = open(file, O_RDONLY);
	if (-1 == fd)	{
		printf("ERROR: %s, %d: failed to open '%s' as configuration file\n", __func__, __LINE__, file);
		return ESQ_ERROR_NOTFOUND;
	}

	r = fstat(fd, &sb);
	if (r)	{
		printf("ERROR: %s, %d: failed to get file size of '%s'\n", __func__, __LINE__, file);
		close(fd);
		return ESQ_ERROR_LIBCALL;
	}

	conf_size = sb.st_size;
	if (conf_size > MAX_FILE_SIZE)	{
		printf("ERROR: %s, %d: size of '%s' is not supported\n", __func__, __LINE__, file);
		close(fd);
		return ESQ_ERROR_SIZE;
	}

	code_buf = malloc(conf_size + 1);
	if (!code_buf)	{
		printf("ERROR: %s, %d: no memory for loading '%s'\n", __func__, __LINE__, file);
		close(fd);
		return ESQ_ERROR_MEMORY;
	}

	r = read(fd, code_buf, conf_size);
	if (r != conf_size)	{
		printf("ERROR: %s, %d: failed to read '%s'\n", __func__, __LINE__, file);
		close(fd);
		free(code_buf);
		return ESQ_ERROR_MEMORY;
	}

	code_buf[conf_size] = '\0';

	errlen = sizeof(errbuf);
	r = esq_mkc_execute(code_buf, conf_size, errbuf, &errlen);

	close(fd);
	free(code_buf);

	if (r)	{
		printf("%s\n", errbuf);
		return ESQ_ERROR_PARAM;
	}
	else	{
		return ESQ_OK;
	}
}

/* eof */
