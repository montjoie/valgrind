/*--------------------------------------------------------------------*/
/*-- Failgrind: malloc and syscall failure generator. fg_interceps.c -*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Failgrind, malloc and syscall failure generator,
   which volontary make fail some functions.

   Copyright (C) 2012-2017 Corentin LABBE
      clabbe.montjoie@gmail.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_options.h"
#include "pub_tool_machine.h"
#include "pub_tool_redir.h"

#include "failgrind.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

/* for bind */
#include <sys/types.h>
#include <sys/socket.h>

/* for fopen */
#include <stdio.h>

#include <time.h>

static int init_done;
static int fault_percent;
static int functions_to_fail;

void * I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,malloc) ( size_t size );

int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,open) ( const char *pathname, int flags, mode_t mode );
ssize_t I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,read) ( int fd, void *buf, size_t count );
ssize_t I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,write) ( int fd, void *buf, size_t count );

FILE * I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,fopen) ( const char *pathname, const char *mode );
size_t I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,fread) ( void *ptr, size_t size, size_t nmemb, FILE *stream );
size_t I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,fwrite) ( const void *ptr, size_t size, size_t nmemb, FILE *stream );

int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,socket) ( int domain, int type, int protocol );
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,connect) ( int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen );
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,bind) ( int sockfd, const struct sockaddr *my_addr, socklen_t addrlen );
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,listen) ( int sockfd, int backlog );
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,accept) ( int sock, struct sockaddr *addr, socklen_t addrlen );
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,recv) ( int s, void *buf, int len, unsigned int flags );
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,send) ( int s, void *buf, int len, unsigned int flags );

static const int error_list_for_malloc[] = {ENOMEM};

static const int error_list_for_open[] = {EACCES, EEXIST, EFAULT, EISDIR, ELOOP, EMFILE, ENAMETOOLONG, ENFILE, ENODEV, ENOENT, ENOMEM, ENOSPC, ENOTDIR, ENXIO, EOVERFLOW, EPERM, EROFS, ETXTBSY, EWOULDBLOCK};
static const int error_list_for_read[] = {EAGAIN, EBADF, EFAULT, EINTR, EINVAL, EIO, EISDIR};
static const int error_list_for_write[] = {EAGAIN, EBADF, EFAULT, EFBIG, EINTR, EINVAL, EIO, ENOSPC, EPIPE};

static const int error_list_for_fopen[] = {EACCES, EEXIST, EFAULT, EISDIR, ELOOP, EMFILE, ENAMETOOLONG, ENFILE, ENODEV, ENOENT, ENOMEM, ENOSPC, ENOTDIR, ENXIO, EOVERFLOW, EPERM, EROFS, ETXTBSY, EWOULDBLOCK};
/* manpage of fread/fwrite does not list possible errors values, so copy read/write values for the moment */
static const int error_list_for_fread[] = {EAGAIN, EBADF, EFAULT, EINTR, EINVAL, EIO, EISDIR};
static const int error_list_for_fwrite[] = {EAGAIN, EBADF, EFAULT, EFBIG, EINTR, EINVAL, EIO, ENOSPC, EPIPE};

static const int error_list_for_socket[] = {EACCES, EAFNOSUPPORT, EINVAL, EMFILE, ENFILE, ENOBUFS, ENOMEM, EPROTONOSUPPORT};
static const int error_list_for_connect[] = {EACCES, EPERM, EADDRINUSE, EAFNOSUPPORT, EAGAIN, EALREADY, EBADF, ECONNREFUSED, EFAULT, EINPROGRESS, EINTR, EISCONN, ENETUNREACH, ENOTSOCK, ETIMEDOUT};

static const int error_list_for_bind[] = {EACCES, EADDRINUSE, EBADF, EINVAL, ENOTSOCK, EADDRNOTAVAIL, EFAULT, ELOOP, ENAMETOOLONG, ENOENT, ENOMEM, ENOTDIR, EROFS};
static const int error_list_for_listen[] = {EADDRINUSE, EBADF, ENOTSOCK, EOPNOTSUPP};
static const int error_list_for_accept[] = {EAGAIN, EWOULDBLOCK, EBADF, ECONNABORTED, EINTR, EINVAL, EMFILE, ENFILE, ENOTSOCK, EOPNOTSUPP, EFAULT, ENOBUFS, ENOMEM, EPROTO, EPERM, ENOSR, ESOCKTNOSUPPORT, EPROTONOSUPPORT, ETIMEDOUT};
static const int error_list_for_recv[] = {EAGAIN, EBADF, ECONNREFUSED, EDESTADDRREQ, EFAULT, EINTR, EINVAL, ENOMEM, ENOTCONN, ENOTSOCK, };
static const int error_list_for_send[] = {EAGAIN, EWOULDBLOCK, EBADF, ECONNRESET, EDESTADDRREQ, EFAULT, EINTR, EINVAL, EISCONN, EMSGSIZE, ENOBUFS, ENOMEM, ENOTCONN, ENOTSOCK, EOPNOTSUPP, EPIPE};

/* TODO fopen fread fwrite
	daemon
	socket connect accept listen bind poll setsockopt getsockopt send recv
	fgets fputs
	getaddrinfo getnameinfo
	execve
	ioctl fcntl
*/

#define MUST_FAIL 0
#define NO_FAIL 1
/*============================================================================*/
/*============================================================================*/
/* Do all init (get values gived by user) and determine if we fail a function or not */
static int do_fail(int fnid) {
	if (init_done == 0) {
		fault_percent = VALGRIND_GET_PERCENT;
		functions_to_fail = VALGRIND_GET_FUNCTIONS_MAY_FAULT;
		srand(time(NULL));
		init_done = 1;
	}

	if ((functions_to_fail & fnid) == 0)
		return NO_FAIL;

	if (rand()%100 >= fault_percent)
		return NO_FAIL;
	return MUST_FAIL;
}

#define FAILGRIND_GET_ERROR(_fg_fnname) _fg_fnname[rand() % (sizeof(_fg_fnname)/sizeof(_fg_fnname[0])) ]

/*============================================================================*/
/*============================================================================*/
void * I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,malloc) ( size_t size )
{
	OrigFn fn;
	void * r;

	VALGRIND_GET_ORIG_FN(fn);
/*	VALGRIND_PRINTF("wrapper-pre malloc %d\n", size);*/

	if (do_fail(FG_FUNC_malloc) == NO_FAIL) {
		CALL_FN_W_W(r, fn, size);
	} else {
		VALGRIND_PRINTF("Do a fault for malloc\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_malloc);
		return NULL;
	}
/*	VALGRIND_PRINTF("wrapper-post malloc %p\n", r);*/
	return r;
}

/*============================================================================*/
/*============================================================================*/
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,open) ( const char *pathname, int flags, mode_t mode )
{
	OrigFn fn;
	int r = 0;

	VALGRIND_GET_ORIG_FN(fn);
	if (pathname != NULL)
		VALGRIND_PRINTF("wrapper-pre open %s\n", pathname);
	else
		VALGRIND_PRINTF("wrapper-pre open\n");

	if (do_fail(FG_FUNC_open) == NO_FAIL) {
		CALL_FN_W_WWW(r, fn, pathname, flags, mode);
	} else {
		VALGRIND_PRINTF("Do a fault for open\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_open);
		return -1;
	}
/*	VALGRIND_PRINTF("wrapper-post open %d\n", r);*/
	return r;
}
/*============================================================================*/
/*============================================================================*/
ssize_t I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,read) ( int fd, void *buf, size_t count )
{
	OrigFn fn;
	ssize_t r = 0;

	VALGRIND_GET_ORIG_FN(fn);
	VALGRIND_PRINTF("wrapper-pre read\n");

	if (do_fail(FG_FUNC_read) == NO_FAIL) {
		CALL_FN_W_WWW(r, fn, fd, buf, count);
	} else {
		VALGRIND_PRINTF("Do a fault for read\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_read);
		return -1;
	}
	VALGRIND_PRINTF("wrapper-post read\n");
	return r;
}

/*============================================================================*/
/*============================================================================*/
/* TODO faulting a write to fd stdin or stdout is useless,
	but we cannot whitelist theses fd because the program could close it.
	Perhaps tracking all close call to theses fd could help*/
ssize_t I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,write) ( int fd, void *buf, size_t count )
{
	OrigFn fn;
	ssize_t r = 0;

	VALGRIND_GET_ORIG_FN(fn);
	VALGRIND_PRINTF("wrapper-pre write fd %d\n", fd);

	if (do_fail(FG_FUNC_write) == NO_FAIL || fd < 3) {
		CALL_FN_W_WWW(r, fn, fd, buf, count);
	} else {
		VALGRIND_PRINTF("Do a fault for write\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_write);
		return -1;
	}
	VALGRIND_PRINTF("wrapper-post write\n");
	return r;
}

/*============================================================================*/
/*============================================================================*/
FILE * I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,fopen) ( const char *pathname, const char *mode )
{
	OrigFn fn;
	FILE *r = NULL;

	VALGRIND_GET_ORIG_FN(fn);
	if (pathname != NULL)
		VALGRIND_PRINTF("wrapper-pre fopen %s\n", pathname);
	else
		VALGRIND_PRINTF("wrapper-pre fopen\n");

	if (do_fail(FG_FUNC_fopen) == NO_FAIL) {
		CALL_FN_W_WW(r, fn, pathname, mode);
	} else {
		VALGRIND_PRINTF("Do a fault for fopen\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_fopen);
		return NULL;
	}
	VALGRIND_PRINTF("wrapper-post fopen\n");
	return r;
}

/*============================================================================*/
/*============================================================================*/
size_t I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,fread) ( void *ptr, size_t size, size_t nmemb, FILE *stream )
{
	OrigFn fn;
	size_t r = 0;

	VALGRIND_GET_ORIG_FN(fn);

	if (do_fail(FG_FUNC_fread) == NO_FAIL) {
		CALL_FN_W_WWWW(r, fn, ptr, size, nmemb, stream);
	} else {
		VALGRIND_PRINTF("Do a fault for fread\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_fread);
		return 0;
	}
	return r;
}

/*============================================================================*/
/*============================================================================*/
size_t I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,fwrite) ( const void *ptr, size_t size, size_t nmemb, FILE *stream )
{
	OrigFn fn;
	size_t r = 0;

	VALGRIND_GET_ORIG_FN(fn);

	if (do_fail(FG_FUNC_fwrite) == NO_FAIL) {
		CALL_FN_W_WWWW(r, fn, ptr, size, nmemb, stream);
	} else {
		VALGRIND_PRINTF("Do a fault for fwrite\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_fwrite);
		return 0;
	}
	return r;
}

/*============================================================================*/
/*============================================================================*/
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,socket) ( int domain, int type, int protocol )
{
	OrigFn fn;
	int r;

	VALGRIND_GET_ORIG_FN(fn);
	VALGRIND_PRINTF("wrapper-pre socket\n");

	if (do_fail(FG_FUNC_socket) == NO_FAIL) {
		CALL_FN_W_WWW(r, fn, domain, type, protocol);
	} else {
		VALGRIND_PRINTF("Do a fault for socket\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_socket);
		return -1;
	}
	return r;
}

/*============================================================================*/
/*============================================================================*/
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,connect) ( int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen )
{
	OrigFn fn;
	int r;

	VALGRIND_GET_ORIG_FN(fn);
	VALGRIND_PRINTF("wrapper-pre connect\n");

	if (do_fail(FG_FUNC_connect) == NO_FAIL) {
		CALL_FN_W_WWW(r, fn, sockfd, serv_addr, addrlen);
	} else {
		VALGRIND_PRINTF("Do a fault for connect\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_connect);
		return -1;
	}
	return r;
}

/*============================================================================*/
/*============================================================================*/
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,bind) ( int sockfd, const struct sockaddr *my_addr, socklen_t addrlen )
{
	OrigFn fn;
	int r;

	VALGRIND_GET_ORIG_FN(fn);
	VALGRIND_PRINTF("wrapper-pre bind\n");

	if (do_fail(FG_FUNC_bind) == NO_FAIL) {
		CALL_FN_W_WWW(r, fn, sockfd, my_addr, addrlen);
	} else {
		VALGRIND_PRINTF("Do a fault for bind\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_bind);
		return -1;
	}
	return r;
}

/*============================================================================*/
/*============================================================================*/
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,listen) ( int sockfd, int backlog )
{
	OrigFn fn;
	int r;

	VALGRIND_GET_ORIG_FN(fn);
	VALGRIND_PRINTF("wrapper-pre listen\n");

	if (do_fail(FG_FUNC_listen) == NO_FAIL) {
		CALL_FN_W_WW(r, fn, sockfd, backlog);
	} else {
		VALGRIND_PRINTF("Do a fault for listen\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_listen);
		return -1;
	}
	return r;
}

/*============================================================================*/
/*============================================================================*/
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,accept) ( int sock, struct sockaddr *addr, socklen_t addrlen )
{
	OrigFn fn;
	int r;

	VALGRIND_GET_ORIG_FN(fn);
	VALGRIND_PRINTF("wrapper-pre accept\n");

	if (do_fail(FG_FUNC_accept) == NO_FAIL) {
		CALL_FN_W_WWW(r, fn, sock, addr, addrlen);
	} else {
		VALGRIND_PRINTF("Do a fault for accept\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_accept);
		return -1;
	}
	return r;
}

/*============================================================================*/
/*============================================================================*/
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,recv) ( int s, void *buf, int len, unsigned int flags )
{
	OrigFn fn;
	int r;

	VALGRIND_GET_ORIG_FN(fn);
	VALGRIND_PRINTF("wrapper-pre recv\n");

	if (do_fail(FG_FUNC_recv) == NO_FAIL) {
		CALL_FN_W_WWWW(r, fn, s, buf, len, flags);
	} else {
		VALGRIND_PRINTF("Do a fault for recv\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_recv);
		return -1;
	}
	return r;
}

/*============================================================================*/
/*============================================================================*/
int I_WRAP_SONAME_FNNAME_ZU(VG_Z_LIBC_SONAME,send) ( int s, void *buf, int len, unsigned int flags )
{
	OrigFn fn;
	int r;

	VALGRIND_GET_ORIG_FN(fn);
	VALGRIND_PRINTF("wrapper-pre send\n");

	if (do_fail(FG_FUNC_send) == NO_FAIL) {
		CALL_FN_W_WWWW(r, fn, s, buf, len, flags);
	} else {
		VALGRIND_PRINTF("Do a fault for send\n");
		errno = FAILGRIND_GET_ERROR(error_list_for_send);
		return -1;
	}
	return r;
}

