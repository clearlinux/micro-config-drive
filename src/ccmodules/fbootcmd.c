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

#define MOD "fbootcmd: "


static void fbootcmd_item(GNode* node, gpointer command_line) {
	if (!node->data) {
		g_node_children_foreach(node, G_TRAVERSE_ALL, fbootcmd_item, command_line);
		if (!exec_task(((GString*)command_line)->str)) {
			LOG(MOD "Execute command failed\n");
		}
		g_string_set_size((GString*)command_line, 0);
	} else {
		g_string_append_printf((GString*)command_line, "%s ", (char*)node->data);
	}
}

void fbootcmd_handler(GNode *node) {
	GString* command_line = NULL;
	LOG(MOD "fbootcmd handler running...\n");
	if (is_first_boot()) {
		LOG(MOD "Running first boot commands\n");
		command_line = g_string_new("");
		g_node_children_foreach(node, G_TRAVERSE_ALL, fbootcmd_item, command_line);
		g_string_free(command_line, true);
	}
}

struct cc_module_handler_struct fbootcmd_cc_module = {
	.name = "fbootcmd",
	.handler = &fbootcmd_handler
};
