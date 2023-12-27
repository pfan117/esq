#include <nghttp2/nghttp2.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include <sys/types.h>	/* stat(), lstat */
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>	/* open() */

#include "public/esq.h"
#include "public/esqll.h"
#include "public/h2esq.h"
#include "public/libesq.h"
#include "modules/simple-file-service/internal.h"

sfs_stream_ctx_t *
sfs_new_ctx(void)	{
	sfs_stream_ctx_t * p;

	p = malloc(sizeof(sfs_stream_ctx_t));
	if (p)	{
		bzero(p, sizeof(sfs_stream_ctx_t));
		p->fd = -1;
		p->post_fd = -1;
		return p;
	}
	else	{
		return NULL;
	}
}

/* eof */
