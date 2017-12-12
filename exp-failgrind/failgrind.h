#ifndef __FAILGRIND_H
#define __FAILGRIND_H

#include "valgrind.h"

#define FG_CLASS_MALLOC (1u << 0)
#define FG_CLASS_IO (1u << 1)
#define FG_CLASS_FIO (1u << 2)
#define FG_CLASS_NETWORK (1u << 3)
/*#define FG_CLASS_NETWORK_CLIENT 8
#define FG_CLASS_NETWORK_SERVER 16
#define FG_CLASS_NETWORK_ALL 24*/
#define FG_CLASS_LAST (1u << 3)

#define FG_FUNC_malloc (1u << 0)

#define FG_FUNC_open (1u << 1)
#define FG_FUNC_read (1u << 2)
#define FG_FUNC_write (1u << 3)

#define FG_FUNC_fopen (1u << 4)
#define FG_FUNC_fread (1u << 5)
#define FG_FUNC_fwrite (1u << 6)

#define FG_FUNC_socket (1u << 7)
#define FG_FUNC_connect (1u << 8)
#define FG_FUNC_bind (1u << 9)
#define FG_FUNC_listen (1u << 10)
#define FG_FUNC_accept (1u << 11)
#define FG_FUNC_send (1u << 12)
#define FG_FUNC_recv (1u << 13)

#define FG_FUNC_LAST (1u << 13)

typedef
	enum {
	_VG_USERREQ__GET_PERCENT = VG_USERREQ_TOOL_BASE('F','G') + 256,
	_VG_USERREQ__GET_FUNCTIONS_MAY_FAULT
} Vg_FailGrindClientRequest;

#define VALGRIND_GET_PERCENT (unsigned)VALGRIND_DO_CLIENT_REQUEST_EXPR(0, _VG_USERREQ__GET_PERCENT, 0, 0, 0, 0, 0);
#define VALGRIND_GET_FUNCTIONS_MAY_FAULT (unsigned)VALGRIND_DO_CLIENT_REQUEST_EXPR(0, _VG_USERREQ__GET_FUNCTIONS_MAY_FAULT, 0, 0, 0, 0, 0);

#endif
