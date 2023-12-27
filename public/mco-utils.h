#ifndef __ESQ_MCO_SERVICE_IO_HEADER_INCLUDED__
#define __ESQ_MCO_SERVICE_IO_HEADER_INCLUDED__

extern int esq_mco_socket_connect_write(int fd, struct sockaddr * address, int address_size, char * buf, int buflen);
extern int esq_mco_socket_write(int fd, const char * buf, int buflen);
extern int esq_mco_socket_read(int fd, void * buf, int buflen);

#endif
