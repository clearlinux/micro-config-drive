/***
 Copyright © 2015 Intel Corporation

 Author: Julio Montes <julio.montes@intel.com>

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
#include <stdio.h>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <glib.h>

#include "handlers.h"
#include "cloud_config.h"
#include "lib.h"
#include "default_user.h"

#define MOD "SSH authorized keys: "


static gboolean ssh_authorized_keys_item(GNode* node, gpointer data) {
	g_string_append_printf((GString*)data, "%s\n", (char*)node->data);
	return false;
}

void ssh_authorized_keys_handler(GNode *node) {
	GString* ssh_keys = NULL;
	LOG(MOD "SSH authorized keys Handler running...\n");
	gchar *username = cloud_config_get_global("first_user");
	if (!username) {
		username = DEFAULT_USER_USERNAME;
	}

	LOG(MOD "User %s\n", (char*)username);
	ssh_keys = g_string_new("");
	g_node_traverse(node, G_IN_ORDER, G_TRAVERSE_LEAVES,
			-1, ssh_authorized_keys_item, ssh_keys);
	if (!write_ssh_keys(ssh_keys, username)) {
		LOG(MOD "Cannot write ssh keys\n");
	}
	g_string_free(ssh_keys, true);
}

struct cc_module_handler_struct ssh_authorized_keys_cc_module = {
	.name = "ssh_authorized_keys",
	.handler = &ssh_authorized_keys_handler
};
