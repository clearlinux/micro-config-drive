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

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>

#include <glib.h>
#include <yaml.h>

#include "cloud_config.h"
#include "handlers.h"
#include "ccmodules.h"
#include "lib.h"

#define SEQ 1
#define MAP 2
#define VAL 4

static GHashTable *cloud_config_global_data = NULL;

static bool cloud_config_parse(yaml_parser_t *parser, GNode *data, int state);
static gboolean cloud_config_simplify(GNode *node, gpointer data);
static void cloud_config_process(GNode *userdata, GList *handlers);

int cloud_config_main(const gchar* filename) {
	yaml_parser_t parser;
	GList* handlers = NULL;
	int i;

	cloud_config_global_data = g_hash_table_new(g_str_hash, g_str_equal);

	LOG("Parsing user data file %s\n", filename);
	GNode* userdata = g_node_new(g_strdup(filename));
	FILE* cloud_config_file = fopen(filename, "rb");

	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, cloud_config_file);
	cloud_config_parse(&parser, userdata, 0);
	yaml_parser_delete(&parser);
	fclose(cloud_config_file);

	g_node_traverse(userdata, G_POST_ORDER, G_TRAVERSE_ALL, -1, cloud_config_simplify, NULL);

	cloud_config_dump(userdata);

	/* built-in handlers */
	for (i = 0; cc_module_structs[i] != NULL; ++i) {
		LOG("Loaded handler for block \"%s\"\n", cc_module_structs[i]->name);
		handlers = g_list_prepend(handlers, cc_module_structs[i]);
	}

	cloud_config_process(userdata, handlers);

	g_node_traverse(userdata, G_POST_ORDER, G_TRAVERSE_ALL, -1, (GNodeTraverseFunc)gnode_free, NULL);
	g_node_destroy(userdata);

	g_list_free(handlers);
	g_hash_table_destroy(cloud_config_global_data);

	return 0;
}

bool cloud_config_bool(GNode* node, bool *b) {
	int i;
	const gchar *true_values[] = {"1", "true", "yes", "y", "on", NULL};
	const gchar *false_values[] = {"0", "false", "no", "n", "off", NULL};

	for (i=0; true_values[i] != NULL; ++i) {
		if (g_ascii_strncasecmp(node->data, true_values[i],
				strlen(true_values[i])) == 0) {
			*b = true;
			return true;
		}
	}

	for (i = 0; false_values[i] != NULL; ++i) {
		if (g_ascii_strncasecmp(node->data, false_values[i],
				strlen(false_values[i])) == 0) {
			*b = false;
			return true;
		}
	}

	/* unknown values: consider as "false" */
	LOG("Unknown bool string (\"%s\") interpreted as \"false\"\n", (char*)node->data);
	return false;
}

bool cloud_config_int_base(const GNode* node, int *i, int base) {
	char *endptr;
	*i = (int)strtol(node->data, &endptr, base);
	if (errno == ERANGE) {
		fprintf(stderr, "Number %s is out of range.\n", (char*)node->data);
		return false;
	}
	if (strlen( endptr ) > 0) {
		fprintf(stderr, "String %s is not a valid number.\n", (char*)endptr);
		return false;
	}
	return true;
}

bool cloud_config_int(const GNode* node, int *i) {
	return cloud_config_int_base(node, i, 10);
}

GNode *cloud_config_find(GNode* node, gchar** path) {
	GNode* child;

	if (!path) {
		if (g_node_depth(node) == 0) {
			return NULL;
		}
		/* reached the end of our search, so return this node as value */
		return g_node_first_child(node);
	}

	child = g_node_first_child(node);
	while (child) {
		if (g_strcmp0(child->data, path[0]) == 0) {
			/* recurse */
			return cloud_config_find(child, (gchar**)path[1]);
		}
		child = g_node_next_sibling(child);
	}

	return NULL;
}

void cloud_config_set_global(gpointer key, gpointer value) {
	g_hash_table_insert(cloud_config_global_data, key, value);
}

gpointer cloud_config_get_global(gpointer key) {
	return g_hash_table_lookup(cloud_config_global_data, key);
}

static bool cloud_config_parse(yaml_parser_t *parser, GNode *node, int state) {
	GNode *last_leaf = node;
	yaml_event_t event;
	bool finished = 0;

	while (!finished) {
		if (!yaml_parser_parse(parser, &event)) {
			LOG("An error occurred while the yaml file was parsed.\n");
			return false;
		}

		switch (event.type) {

		case YAML_SCALAR_EVENT:
			if (state & SEQ) {
				last_leaf = g_node_append(node, g_node_new(g_strdup((gchar*) event.data.scalar.value)));
			} else if (state & VAL) {
				g_node_append_data(last_leaf, g_strdup((gchar*) event.data.scalar.value));
				state &= MAP | SEQ;
			} else {
				last_leaf = g_node_append(node, g_node_new(g_strdup((gchar*) event.data.scalar.value)));
				state |= VAL;
			}
			break;

		case YAML_SEQUENCE_START_EVENT:
			/* remove VAL bit if it's set */
			if (state & MAP)
				state = MAP;
			if (state & SEQ) {
				last_leaf = g_node_append(node, g_node_new(NULL));
			} else {
				last_leaf = g_node_append(last_leaf, g_node_new(NULL));
			}
			if (!cloud_config_parse(parser, last_leaf, SEQ)) {
				return false;
			}
			last_leaf = last_leaf->parent;
			break;

		case YAML_SEQUENCE_END_EVENT:
			finished = true;
			break;

		case YAML_MAPPING_START_EVENT:
			last_leaf = g_node_append(node, g_node_new(NULL));
			if (!cloud_config_parse(parser, last_leaf, MAP)) {
				return false;
			}
			last_leaf = last_leaf->parent;
			break;

		case YAML_MAPPING_END_EVENT:
			last_leaf = last_leaf->parent;
			finished = true;
			break;

		case YAML_STREAM_END_EVENT:
			finished = true;
			break;

		case YAML_NO_TOKEN:
			LOG("Unexpectedly reached end of YAML input!");
			finished = true;
			break;

		default:
			/* Ignore these for now */
			break;
		}

		if (!finished) {
			yaml_event_delete(&event);
		}
	}
	return true;
}

static void cloud_config_process(GNode *userdata, GList *handlers) {
	/* toplevel node is always a sequence, so skip over that sequence */
	userdata = g_node_first_child(userdata);

	/* loop over all toplevel elements and find modules to handle them */
	for (guint i = 0; i < g_node_n_children(userdata); i++) {
		GNode *node = g_node_nth_child(userdata, i);
		struct cc_module_handler_struct* h;
		bool found = false;

		for (guint j = 0; j < g_list_length(handlers); j++) {
			h = g_list_nth_data(handlers, j);
			if (g_strcmp0(h->name, node->data) == 0) {
				found = true;
				break;
			}
		}

		if (found) {
			LOG("Executing handler for block \"%s\"\n", (char*)node->data);
			h->handler(node);
		} else {
			LOG("No handler found for block \"%s\"\n", (char*)node->data);
		}
	}
}

static gboolean cloud_config_simplify(GNode *node, __unused__ gpointer data) {
	if (node->data) {
		return false;
	}

	GNode *child = g_node_last_child(node);
	while (child) {
		if (child->data) {
			child = g_node_prev_sibling(child);
			continue;
		}
		GNode *remove = child;
		child = g_node_prev_sibling(child);
		g_node_append(node->parent, g_node_copy(remove));
		g_node_unlink(remove);
		g_node_destroy(remove);
	}

	if (g_node_n_children(node) == 0) {
		g_node_unlink(node);
		g_node_destroy(node);
	}

	return false;
}

struct interpreter_handler_struct cloud_config_interpreter = {
	.shebang = "#cloud-config",
	.handler = &cloud_config_main
};
