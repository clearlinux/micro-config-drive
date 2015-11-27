/***
 Copyright (C) 2015 Intel Corporation

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

#include <stdbool.h>

#include <glib.h>

#include "lib.h"
#include "handlers.h"

#define MOD "runcmd: "

#define COMMAND_SIZE 4096

static void runcmd_item(GNode* node, gpointer data) {
	gchar command_line[COMMAND_SIZE] = { 0 };
	if (!node->data) {
		g_node_children_foreach(node, G_TRAVERSE_ALL, runcmd_item, command_line);
		if (!exec_task(command_line)) {
			LOG(MOD "Execute command failed\n");
		}
	} else {
		g_strlcat(data, node->data,COMMAND_SIZE);
		g_strlcat(data, " ", COMMAND_SIZE);
	}
}

void runcmd_handler(GNode *node) {
	LOG(MOD "Runcmd Handler running...\n");
	g_node_children_foreach(node, G_TRAVERSE_ALL, runcmd_item, NULL);
}

struct cc_module_handler_struct runcmd_cc_module = {
	.name = "runcmd",
	.handler = &runcmd_handler
};
