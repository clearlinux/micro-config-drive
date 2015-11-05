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


gboolean ssh_authorized_keys_write_ssh_key(const gchar* ssh_key, const gchar* username) {
	int fd;
	gchar auth_keys_path[PATH_MAX];
	struct passwd *pw;

	pw = getpwnam(username);

	if (pw && pw->pw_dir) {
		g_snprintf(auth_keys_path, PATH_MAX, "%s/.ssh", pw->pw_dir);

		if (make_dir(auth_keys_path, S_IRWXU) != 0) {
			LOG(MOD "Cannot create %s.\n", auth_keys_path);
			return false;
		}

		if (chown_path(auth_keys_path, username, username) != 0) {
			LOG(MOD "Cannot change the owner and group of %s.\n", auth_keys_path);
			return false;
		}

		g_strlcat(auth_keys_path, "/authorized_keys", PATH_MAX);
		fd = open(auth_keys_path, O_CREAT|O_APPEND|O_WRONLY, S_IRUSR|S_IWUSR);
		if (-1 == fd) {
			LOG(MOD "Cannot open %s.\n", auth_keys_path);
			return false;
		}

		LOG(MOD "Using %s\n", auth_keys_path);
		LOG(MOD "Writing %s\n", ssh_key);
		write(fd, ssh_key, strlen(ssh_key));
		write(fd, "\n", 1);
		close(fd);

		if (chown_path(auth_keys_path, username, username) != 0) {
			LOG(MOD "Cannot change the owner and group of %s.\n", auth_keys_path);
			return false;
		}
	}
	return true;
}

gboolean ssh_authorized_keys_item(GNode* node, gpointer username) {
	if (ssh_authorized_keys_write_ssh_key(node->data, username)) {
		return false;
	}

	return true;
}

void ssh_authorized_keys_handler(GNode *node) {
	LOG(MOD "SSH authorized keys Handler running...\n");
	gchar *username = cloud_config_get_global("first_user");
	if (!username) {
		username = DEFAULT_USER_USERNAME;
	}

	LOG(MOD "User %s\n", (char*)username);
	g_node_traverse(node, G_IN_ORDER, G_TRAVERSE_LEAVES,
			-1, ssh_authorized_keys_item, username);
}

struct cc_module_handler_struct ssh_authorized_keys_cc_module = {
	.name = "ssh_authorized_keys",
	.handler = &ssh_authorized_keys_handler
};
