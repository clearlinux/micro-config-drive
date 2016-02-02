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

#include "handlers.h"
#include "cloud_config.h"
#include "lib.h"

#define MOD "users: "
#define COMMAND_SIZE 2048
#define BUFFER_SIZE 1024

static void users_add_username(GNode* node, char* command, gpointer data);
static void users_add_groups(GNode* node, char* command, gpointer data);
static void users_add_option_format(GNode* node, char* command, gpointer data);
static void users_add_option(GNode* node, char* command, gpointer data);
static gboolean users_sudo_item(GNode* node, gpointer data);
static gboolean users_ssh_key_item(GNode* node, gpointer data);

struct users_options_data {
	const gchar* key;
	void (*func)(GNode* node, char* command, gpointer data);
	gpointer data;
};

static gchar users_current_username[LOGIN_NAME_MAX];

static struct users_options_data users_options[] = {
	{"name",		users_add_username,		" %s "		},
	{"gecos",		users_add_option_format,	" -c '%s' "	},
	{"homedir",		users_add_option_format,	" -d %s "	},
	{"primary-group",	users_add_option_format,	" -g %s "	},
	{"groups",		users_add_groups,	        NULL    	},
	{"lock-passwd",		NULL,				NULL		},
	{"inactive",		NULL,				NULL		},
	{"passwd",		users_add_option_format,	" -p '%s' "	},
	{"no-create-home",	users_add_option,		" -M , -m "	},
	{"no-user-group",	users_add_option,		" -N , -U "	},
	{"no-log-init",		users_add_option,		" -l ,"		},
	{"expiredate",		users_add_option_format,	" -e %s "	},
	{"ssh-authorized-keys",	NULL,  				NULL		},
	{"sudo",		NULL,				NULL 		},
	{"system",		users_add_option_format,	" -r "		},
	{NULL}
};

static void users_add_username(GNode* node, char* command, gpointer data) {
	char buffer[BUFFER_SIZE];
	g_snprintf(buffer, BUFFER_SIZE, data, node->data);
	g_strlcat(command, buffer, COMMAND_SIZE);

	g_strlcpy(users_current_username, node->data, LOGIN_NAME_MAX);

	if (!cloud_config_get_global("first_user")) {
		cloud_config_set_global("first_user", users_current_username);
	}
}

static void users_add_groups(GNode* node, char* command, __unused__ gpointer data) {
	GNode* group;
	if (node->data) {
		users_add_option_format(node, command, " -G %s ");
	} else if (node->children) {
		g_strlcat(command, " -G '", COMMAND_SIZE);
		for(group=node->children; group; group=group->next) {
			g_strlcat(command, group->data, COMMAND_SIZE);
			g_strlcat(command, ",", COMMAND_SIZE);
		}
		/* remove last , and add '*/
		command[strlen(command)-1] = 0;
		g_strlcat(command, "' ", COMMAND_SIZE);
	}
}

static void users_add_option_format(GNode* node, char* command, gpointer data) {
	char buffer[BUFFER_SIZE];
	g_snprintf(buffer, BUFFER_SIZE, data, node->data);
	g_strlcat(command, buffer, COMMAND_SIZE);
}

static void users_add_option(GNode* node, char* command, gpointer data) {
	bool b;
	gchar** tokens = g_strsplit(data, ",", 2);
	guint len = g_strv_length(tokens);
	cloud_config_bool(node, &b);
	if (b) {
		if (len > 0) {
			g_strlcat(command, tokens[0], COMMAND_SIZE);
		}
	} else {
		if (len > 1) {
			g_strlcat(command, tokens[1], COMMAND_SIZE);
		}
	}
	g_strfreev(tokens);
}

static gboolean users_sudo_item(GNode* node, gpointer data) {
	g_string_append_printf((GString*)data, "%s\n", (char*)node->data);
	return false;
}

static gboolean users_ssh_key_item(GNode* node, gpointer data) {
	g_string_append_printf((GString*)data, "%s\n", (char*)node->data);
	return false;
}

static void users_item(GNode* node, gpointer data) {
	if (node->data) {
		/* to avoid bugs with key(gecos, etc) as username */
		if (node->children) {
			for (size_t i = 0; users_options[i].key != NULL; ++i) {
				if (0 == g_strcmp0(node->data, users_options[i].key)) {
					if (users_options[i].func) {
						users_options[i].func(node->children, data,
							users_options[i].data);
					}
					return;
				}
			}
			LOG(MOD "No handler for %s.\n", (char*)node->data);
			return;
		}
		users_add_username(node, data, "%s");
	} else {
		bool b;
		GString* sudo_directives;
		GString* ssh_keys;
		gchar command[COMMAND_SIZE] = USERADD_PATH " ";
		memset(users_current_username, 0, LOGIN_NAME_MAX);
		g_node_children_foreach(node, G_TRAVERSE_ALL, users_item, command);
		if (0 == strlen(users_current_username)) {
			LOG(MOD "Missing username.\n");
			return;
		}

		LOG(MOD "Adding %s user...\n", users_current_username);
		exec_task(command);

		CLOUD_CONFIG_KEY(LOCK_PASSWD, "lock-passwd");
		CLOUD_CONFIG_KEY(INACTIVE, "inactive");
		CLOUD_CONFIG_KEY(SSH_AUTH_KEYS, "ssh-authorized-keys");
		CLOUD_CONFIG_KEY(SUDO, "sudo");

		GNode *item = cloud_config_find(node, LOCK_PASSWD);
		if (item) {
			cloud_config_bool(item, &b);
			if (b) {
				LOG(MOD "Locking %s user.\n", users_current_username);
				g_snprintf(command, COMMAND_SIZE, PASSWD_PATH " -l %s",
					users_current_username);
				exec_task(command);
			}
		}

		item = cloud_config_find(node, INACTIVE);
		if (item) {
			cloud_config_bool(item, &b);
			if (b) {
				LOG(MOD "Deactivating %s user...\n", users_current_username);
				g_snprintf(command, COMMAND_SIZE, USERMOD_PATH " --expiredate 1 %s",
					users_current_username);
				exec_task(command);
			}
		}

		item = cloud_config_find(node, SSH_AUTH_KEYS);
		if (item) {
			ssh_keys = g_string_new("");
			g_node_traverse(item->parent, G_IN_ORDER, G_TRAVERSE_LEAVES,
				-1, users_ssh_key_item, ssh_keys);
			if (!write_ssh_keys(ssh_keys, users_current_username)) {
				LOG(MOD "Cannot write ssh keys\n");
			}
			g_string_free(ssh_keys, true);
		}

		item = cloud_config_find(node, SUDO);
		if (item) {
			sudo_directives = g_string_new("");
			g_string_printf(sudo_directives, "# User rules\n");
			g_node_traverse(item->parent, G_IN_ORDER, G_TRAVERSE_LEAVES,
				-1, users_sudo_item, sudo_directives);
			g_string_append(sudo_directives, "\n");
			if (!write_sudo_directives(sudo_directives, "users-cloud-init")) {
				LOG(MOD "Cannot write sudo directives\n");
			}
			g_string_free(sudo_directives, true);
		}
	}
}

void users_handler(GNode *node) {
	LOG(MOD "Users Handler running...\n");
	g_node_children_foreach(node, G_TRAVERSE_ALL, users_item, NULL);
}

struct cc_module_handler_struct users_cc_module = {
	.name = "users",
	.handler = &users_handler
};

