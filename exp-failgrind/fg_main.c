/*--------------------------------------------------------------------*/
/*--- Failgrind: malloc and syscall failure generator.   fg_main.c ---*/
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
#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_options.h"
#include "pub_tool_tooliface.h"

#include "failgrind.h"

static Int clo_percent;
static Int clo_fail_class;
static Int clo_functions_to_fail;

static Bool fg_handle_client_request( ThreadId tid, UWord* arg, UWord* ret )
{
	switch (arg[0]) {
	case _VG_USERREQ__GET_PERCENT:
		*ret = clo_percent;
		break;
	case _VG_USERREQ__GET_FUNCTIONS_MAY_FAULT:
		*ret = clo_functions_to_fail;
		break;
	default:
		VG_(message)(Vg_UserMsg,
				"Warning: unknown FailGrind client request code %llx\n",
				(ULong)arg[0]);
		return False;
	}
	return True;
}

static Bool fg_process_cmd_line_option(const HChar* arg)
{
	const HChar *tmp_str;
	Int i = 0, len, j = 0;

	if VG_BINT_CLO(arg, "--percent", clo_percent, 1, 100) {}
	else if VG_BINT_CLO(arg, "--failclass", clo_fail_class, 0, FG_CLASS_LAST * 2 - 1) {}
	else if VG_STR_CLO(arg, "--failfunc", tmp_str) {
		len = VG_(strlen)(tmp_str);
		do {
			if (tmp_str[i] == ',' || i == len ) {
				if (VG_(strncmp)(tmp_str + j, "malloc", 5) == 0) {
					clo_functions_to_fail |= FG_FUNC_malloc;
				} else if (VG_(strncmp)(tmp_str + j, "open", 4) == 0) {
					clo_functions_to_fail |= FG_FUNC_open;
				} else if (VG_(strncmp)(tmp_str + j, "read", 4) == 0) {
					clo_functions_to_fail |= FG_FUNC_read;
				} else if (VG_(strncmp)(tmp_str + j, "write", 5) == 0) {
					clo_functions_to_fail |= FG_FUNC_write;
				} else if (VG_(strncmp)(tmp_str + j, "fopen", 5) == 0) {
					clo_functions_to_fail |= FG_FUNC_fopen;
				} else if (VG_(strncmp)(tmp_str + j, "fread", 5) == 0) {
					clo_functions_to_fail |= FG_FUNC_fread;
				} else if (VG_(strncmp)(tmp_str + j, "fwrite", 6) == 0) {
					clo_functions_to_fail |= FG_FUNC_fwrite;
				} else if (VG_(strncmp)(tmp_str + j, "socket", 6) == 0) {
					clo_functions_to_fail |= FG_FUNC_socket;
				} else if (VG_(strncmp)(tmp_str + j, "connect", 6) == 0) {
					clo_functions_to_fail |= FG_FUNC_connect;
				} else if (VG_(strncmp)(tmp_str + j, "bind", 4) == 0) {
					clo_functions_to_fail |= FG_FUNC_bind;
				} else if (VG_(strncmp)(tmp_str + j, "listen", 6) == 0) {
					clo_functions_to_fail |= FG_FUNC_listen;
				} else if (VG_(strncmp)(tmp_str + j, "accept", 6) == 0) {
					clo_functions_to_fail |= FG_FUNC_accept;
				} else if (VG_(strncmp)(tmp_str + j, "send", 4) == 0) {
					clo_functions_to_fail |= FG_FUNC_send;
				} else if (VG_(strncmp)(tmp_str + j, "recv", 4) == 0) {
					clo_functions_to_fail |= FG_FUNC_recv;
				} else if (VG_(strncmp)(tmp_str + j, "all", 3) == 0) {
					clo_functions_to_fail = FG_FUNC_LAST * 2 - 1;
				} else {
					VG_(printf)("ERROR: Unrecognized function: ");
					for (; j < i && tmp_str[j] != ','; j++)
						VG_(printf)("%c", tmp_str[j]);
					VG_(printf)("\n");
					return False;
				}
				j = i + 1;
			}
			i++;
		} while (i <= len);
	} else
		return False;
	return True;
}

static void fg_print_usage(void)
{
	VG_(printf)(
"	--percent=<number>	percent of function call to fail\n"
"	--failfunc=fn1,fn2,..	Comma separated list of functions to fail\n"
"	--failclass=<number>	Class of function to fail\n"
"		Malloc %d\n"
"		IO operations (open, read, write) %d\n"
"		FIO operations (fopen, fread, fwrite) %d\n"
"		Network operations (socket connect accept listen bind poll setsockopt getsockopt) %d\n"
	, FG_CLASS_MALLOC, FG_CLASS_IO, FG_CLASS_FIO, FG_CLASS_NETWORK
	);
}

static void fg_print_debug_usage(void)
{
	VG_(printf)(
"    (none)\n"
	);
}

static void fg_post_clo_init(void)
{
	if ((clo_fail_class & FG_CLASS_MALLOC) > 0)
		clo_functions_to_fail |= FG_FUNC_malloc;
	if ((clo_fail_class & FG_CLASS_IO) > 0) {
		clo_functions_to_fail |= FG_FUNC_open;
		clo_functions_to_fail |= FG_FUNC_read;
		clo_functions_to_fail |= FG_FUNC_write;
	}
	if ((clo_fail_class & FG_CLASS_FIO) > 0) {
		clo_functions_to_fail |= FG_FUNC_fopen;
		clo_functions_to_fail |= FG_FUNC_fread;
		clo_functions_to_fail |= FG_FUNC_fwrite;
	}
	if ((clo_fail_class & FG_CLASS_NETWORK) > 0) {
		clo_functions_to_fail |= FG_FUNC_socket;
		clo_functions_to_fail |= FG_FUNC_connect;
		clo_functions_to_fail |= FG_FUNC_bind;
		clo_functions_to_fail |= FG_FUNC_listen;
		clo_functions_to_fail |= FG_FUNC_accept;
		clo_functions_to_fail |= FG_FUNC_send;
		clo_functions_to_fail |= FG_FUNC_recv;
	}
}

static
IRSB* fg_instrument ( VgCallbackClosure* closure,
                      IRSB* bb,
                      const VexGuestLayout* layout,
                      const VexGuestExtents* vge, const VexArchInfo* archinfo_Host,
                      IRType gWordTy, IRType hWordTy )
{
    return bb;
}

static void fg_fini(Int exitcode)
{
}

static void fg_pre_clo_init(void)
{
   VG_(details_name)            ("Failgrind");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("malloc and syscall failure generator");
   VG_(details_copyright_author)(
      "Copyright (C) 2012-2015, and GNU GPL'd, by LABBE Corentin.");
   VG_(details_bug_reports_to)  (VG_BUGS_TO);

   VG_(details_avg_translation_sizeB) ( 275 );

   VG_(basic_tool_funcs)        (fg_post_clo_init,
                                 fg_instrument,
                                 fg_fini);
   VG_(needs_client_requests)     (fg_handle_client_request);
   VG_(needs_command_line_options)(fg_process_cmd_line_option,
				fg_print_usage,
				fg_print_debug_usage);
	clo_functions_to_fail = 0;
}

VG_DETERMINE_INTERFACE_VERSION(fg_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
