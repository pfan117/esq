#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include <sys/types.h>	/* stat(), lstat */
#include <sys/stat.h>
//#include <unistd.h>
//#include <fcntl.h>	/* open() */

#include "public/esq.h"
#include "public/libesq.h"

#if defined MODULE_TEST_MODE
	#define PRINTF printf
#else
	#define PRINTF esq_log
#endif

int
esq_enum_path(char * path, int length, esq_file_enumerators_cb_t cb, void * cb_param)	{
	DIR * dir;
	int l;
	int r;

	dir = opendir(path);
	if (dir)	{
		//printf("DBG: %s() %d: %s is a folder\n", __func__, __LINE__, path);

		if ((length > 0) && ('/' == path[length - 1]))	{
			;
		}
		else	{
			if (length + 1 < ESQ_FILE_PATH_MAX)	{
				path[length] = '/';
				length ++;
			}
			else	{
				closedir(dir);
				return ESQ_OK;	/* ignore */
			}
		}

		struct dirent * ent;

		for (ent = readdir(dir); ent; ent = readdir(dir))	{

			if ('.' == ent->d_name[0])	{
				continue;	/* ignore */
			}

			l = strnlen(ent->d_name, ESQ_FILE_PATH_MAX);
			if (l <= 0 || l >= ESQ_FILE_PATH_MAX)	{
				continue;	/* ignore */
			}

			if (length + l >= ESQ_FILE_PATH_MAX)	{
				continue;	/* ignore */
			}

			memcpy(path + length, ent->d_name, l);
			path[length + l] = '\0';

			r = esq_enum_path(path, length + l, cb, cb_param);

			if (r)	{
				closedir(dir);
				return ESQ_OK;
			}
		}

		closedir(dir);
		return ESQ_OK;
	}
	else	{
		if (ENOTDIR == errno)	{
			//printf("DBG: %s() %d: %s is not a folder\n", __func__, __LINE__, path);
		}
		else if (ENOENT == errno)	{
			//printf("DBG: %s() %d: %s not exist\n", __func__, __LINE__, path);
			return ESQ_ERROR_NOTFOUND;
		}
		else	{
			//printf("DBG: %s() %d: failed to access %s\n", __func__, __LINE__, path);
			return ESQ_ERROR_PARAM;
		}
	}

	struct stat buf;

	r = stat(path, &buf);
	if (r)	{
		return ESQ_OK;	/* ignore */
	}

	if (S_IFREG != (buf.st_mode & S_IFMT))	{
		//printf("DBG: %s() %d: %s is not a regular file\n", __func__, __LINE__, path);
		return ESQ_OK;	/* ignore */
	}

	r = cb(path, length, cb_param);

	return ESQ_OK;
}

int
esq_file_enumerators(const char * root, esq_file_enumerators_cb_t cb, void * cb_param)	{
	char path[ESQ_FILE_PATH_MAX + 4];
	int length;
	int r;

	length = strnlen(root, ESQ_FILE_PATH_MAX);
	if (length <= 0 || length >= ESQ_FILE_PATH_MAX)	{
		PRINTF("ERROR: %s() %d: path_name too long\n", __func__, __LINE__);
		return ESQ_ERROR_PARAM;
	}

	memcpy(path, root, length);
	path[length] = '\0';

	r = esq_enum_path(path, length, cb, cb_param);

	return r;
}

#if defined MODULE_TEST_MODE

int demo_file_enumerators_cb(const char * path_name, int length, void * param)	{
	printf("INFO: %s() %d: path_name %s, length %d, param %p\n", __func__, __LINE__, path_name, length, param);
	return 0;
}

int
main(void)	{
	int r;

	r = esq_file_enumerators("/etc", demo_file_enumerators_cb, &r);

	printf("INFO: %s() %d: r = %d, &r = %p\n", __func__, __LINE__, r, &r);

	return 0;
}

#endif

/* eof */
