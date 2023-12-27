#define PRINT_DBG_LOG

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#define _ESQ_LIB_SOCKET_FUNCTIONS
#include "public/libesq.h"
#include "internal/log_print.h"

/* return -1 for error */
int
os_fast_open_sendto_nb(int fd, struct sockaddr * peer, int addrlen, const char * buf, int len)	{
	int r;

    r = sendto(fd, buf, len, MSG_FASTOPEN, peer, addrlen);

	if (-1 == r)	{
		if (EAGAIN == errno)	{
			return 0;
		}
 		else if (EWOULDBLOCK == errno)	{
			return 0;
		}
		else if (EINPROGRESS == errno)	{
			return 0;
		}
		else if (114 == errno)	{
			/* operation is already in progress */
			PRINTF("DBG: %s() %d: errno = %d (%s)\n", __func__, __LINE__, errno, strerror(errno));
			return 0;
		}
		else	{
			/* return -1 for error */
			PRINTF("DBG: %s() %d: errno = %d (%s)\n", __func__, __LINE__, errno, strerror(errno));
			return -1;
		}
	}
	else	{
		/* return size */
		return r;
	}
}

/* return -1 for error */
int
os_read_nb(int fd, char * buf, int len)	{
	int r;

    r = read(fd, buf, len);

	if (-1 == r)	{
		if (EAGAIN == errno)	{
			return 0;
		}
 		else if (EWOULDBLOCK == errno)	{
			return 0;
		}
		else	{
			/* return -1 for error */
			return -1;
		}
	}
	else	{
		/* return size */
		return r;
	}
}

/* return -1 for error */
int
os_write_nb(int fd, const char * buf, int len)	{
	int r;

    r = write(fd, buf, len);

	if (-1 == r)	{
		if (EAGAIN == errno)	{
			return 0;
		}
 		else if (EWOULDBLOCK == errno)	{
			return 0;
		}
		else if (EINPROGRESS == errno)	{
			return 0;
		}
		else	{
			/* return -1 for error */
			/* printf("errno = %d (%s)\n", errno, strerror(errno)); */
			return -1;
		}
	}
	else	{
		/* return size */
		return r;
	}
}

/* return -1 for error */
/* write buffer out into a socket */
int
os_write_b(int fd, const char * buf, int len)	{
	int offset = 0;
	int r;

	while (offset < len)	{
	    r = write(fd, buf, len);

		if (r <= 0)	{
			/* zero or -1 */
			if (EAGAIN == errno)	{
				continue;
			}
 			else if (EWOULDBLOCK == errno)	{
				continue;
			}
			else	{
				/* return -1 for error */
				/* printf("errno = %d (%s)\n", errno, strerror(errno)); */
				return -1;
			}
		}
		else	{
			/* return size */
			offset += r;
		}
	}

	return 0;
}

int
os_socket_set_timeout(int sd, int second)	{
	struct timeval tv = {second, 0};
	int r;
	r = setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (void *)&tv, sizeof(tv));
	if (r)    {
		return -1;
	}
	r = setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, (void *)&tv, sizeof(tv));
	if (r)    {
		return -1;
	}
	return 0;
}

int
os_socket_set_non_blocking(int fd)	{
	int r;

	r = fcntl(fd, F_GETFL);
	if (r < 0)	{
		printf("ERROR: %s, %d: failed to get FL, return value is %d, errno = %d (%s)\n"
				, __func__, __LINE__, r, errno, strerror(errno));
		return -1;
	}

	r |= O_NONBLOCK;

	r = fcntl(fd, F_SETFL, r);
	if (r < 0)	{
		printf("ERROR: %s, %d: failed to set FL, return value is %d, errno = %d (%s)\n"
				, __func__, __LINE__, r, errno, strerror(errno));
		return -1;
	}

	return 0;
}

int
os_open_tcp_socket(int blocking, int seconds)	{
	int fd;
	int r;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == fd)	{
		return -1;
	}

	if (blocking)	{
		;
	}
	else	{
		r = os_socket_set_non_blocking(fd);
		if (r)	{
			close(fd);
			return -1;
		}
	}

	if (seconds)	{
		r = os_socket_set_timeout(fd, seconds);
		if (r)	{
			close(fd);
			return -1;
		}
	}

	return fd;
}

static struct ifaddrs * esq_if_list = NULL;

int
os_get_if_list(void)	{
	if (getifaddrs(&esq_if_list) < 0)	{
		return -1;
	}
	return 0;
}

void
os_free_if_list(void)	{
	if (esq_if_list)	{
		freeifaddrs(esq_if_list);
		esq_if_list = NULL;
	}
}

int
os_ip_address_is_me(struct sockaddr_in * sin)	{
	struct ifaddrs * p;
	struct in_addr * ipa;
	struct in_addr * ipb;

	for (p = esq_if_list; p; p = p->ifa_next)	{
		if(AF_INET != p->ifa_addr->sa_family)	{
			continue;
		}

		ipa = &(sin->sin_addr);
		ipb = &(((struct sockaddr_in *)(p->ifa_addr))->sin_addr);

		if (memcmp(ipa, ipb, sizeof(struct in_addr)))	{
			continue;
		}
		else	{
			return 1;
		}
	}

	return 0;
}

/* eof */
