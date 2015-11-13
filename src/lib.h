/***
 Copyright (C) 2015 Intel Corporation

 Author: Auke-jan H. Kok <auke-jan.h.kok@intel.com>
 Author: Julio Montes <julio.montes@intel.com>

 This file is part of clr-cloud-init.

 clr-cloud-init is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 clr-cloud-init is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with clr-cloud-init. If not, see <http://www.gnu.org/licenses/>.

 In addition, as a special exception, the copyright holders give
 permission to link the code of portions of this program with the
 OpenSSL library under certain conditions as described in each
 individual source file, and distribute linked combinations
 including the two.
 You must obey the GNU General Public License in all respects
 for all of the code used other than OpenSSL.  If you modify
 file(s) with this exception, you may extend this exception to your
 version of the file(s), but you are not obligated to do so.  If you
 do not wish to do so, delete this exception statement from your
 version.  If you delete this exception statement from all source
 files in the program, then also delete it here.
***/

#pragma once

#include <glib.h>

#include "debug.h"

#ifdef DEBUG
	#define STRINGIZE_DETAIL(x) #x
	#define STRINGIZE(x) STRINGIZE_DETAIL(x)
	#define LOGD(...) LOG(__BASE_FILE__":"STRINGIZE(__LINE__)" - "__VA_ARGS__)
#else
	#define LOGD(...)
	#define cloud_config_dump(...)
#endif /* DEBUG */

void exec_task(const gchar* task);
void LOG(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int make_dir(const char* pathname, mode_t mode);
int chown_path(const char* pathname, const char* ownername, const char* groupname);
int write_sudo_string(const gchar* filename, const gchar* data);
int write_ssh_key(const gchar* ssh_key, const gchar* username);
gboolean get_partition(const gchar* mountpoint, gchar* partition, guint partition_len) ;
