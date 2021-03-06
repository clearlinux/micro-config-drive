/***
 Copyright © 2015 Intel Corporation

 Author: Auke-jan H. Kok <auke-jan.h.kok@intel.com>

 This file is part of micro-config-drive.

 micro-config-drive is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 micro-config-drive is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with micro-config-drive. If not, see <http://www.gnu.org/licenses/>.

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

#include "handlers.h"
#include "lib.h"

#define MOD "service: "
#define COMMAND_SIZE 2048


static gboolean service_action(GNode* node, gpointer data) {
	gchar c[COMMAND_SIZE];
	g_snprintf(c, COMMAND_SIZE, SYSTEMCTL_PATH " %s %s", (char*)data, (char*)node->data);
	exec_task(c);
	return false;
}

static void service_item(GNode* node, __unused__ gpointer data) {
	if (!node->data) {
		node = node->children;
	} else {
		LOG(MOD "Unexpected non-sequence data at %s!\n", (char*)node->data);
	}

	if (!node->children) {
		LOG(MOD "service action %s provided but no service name to apply action to\n!", (char*)node->data);
		return;
	}

	if ((g_strcmp0(node->data, "enable") != 0) &&
	    (g_strcmp0(node->data, "disable") != 0) &&
	    (g_strcmp0(node->data, "start") != 0) &&
	    (g_strcmp0(node->data, "stop") != 0) &&
	    (g_strcmp0(node->data, "restart") != 0) &&
	    (g_strcmp0(node->data, "reload") != 0) &&
	    (g_strcmp0(node->data, "isolate") != 0) &&
	    (g_strcmp0(node->data, "mask") != 0) &&
	    (g_strcmp0(node->data, "unmask") != 0)) {
		LOG(MOD "service action %s is not a valid service action\n", (char*)node->data);
		return;
	}

	g_node_traverse(node, G_IN_ORDER, G_TRAVERSE_LEAVES, -1, service_action, node->data);
}

void service_handler(GNode *node) {
	LOG(MOD "Service Handler running...\n");
	g_node_children_foreach(node, G_TRAVERSE_ALL, service_item, NULL);
}

struct cc_module_handler_struct service_cc_module = {
	.name = "service",
	.handler = &service_handler
};

