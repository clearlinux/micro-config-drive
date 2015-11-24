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

#include <json-glib/json-glib.h>


gchar* json_string(JsonNode* node) {
	gchar buffer[64];
	GType valueType = json_node_get_value_type(node);

	switch (valueType) {
	case G_TYPE_STRING:
		return json_node_dup_string(node);

	case G_TYPE_INT:
		sprintf(buffer, "%d", (int)json_node_get_int(node));
		break;

	case G_TYPE_DOUBLE:
		sprintf(buffer, "%f", json_node_get_double(node));
		break;

	case G_TYPE_BOOLEAN:
		if (json_node_get_boolean(node)) {
			sprintf(buffer, "%s", "true");
		} else {
			sprintf(buffer, "%s", "false");
		}
		break;

	default:
		sprintf(buffer, "%s", "Unknown type");
		break;
	}

	return g_strdup(buffer);
}

void json_parse(JsonNode* root, GNode* node, bool parsing_array) {
	if (JSON_NODE_TYPE(root) == JSON_NODE_OBJECT) {
		JsonObject *object = json_node_get_object(root);

		if (object) {
			guint j;
			guint size;
			GList* keys, *key = NULL;
			GList* values, *value = NULL;

			size = json_object_get_size(object);
			keys = json_object_get_members(object);
			values = json_object_get_values(object);
			node = g_node_append(node, g_node_new(NULL));

			for (j = 0, key = keys, value = values; j < size; j++) {
				if (key) {
					node = g_node_append(node->parent, g_node_new(g_strdup(key->data)));
				}
				if (value) {
					json_parse(value->data, node, false);
				}

				key = g_list_next(key);
				value = g_list_next(value);
			}

			if (keys) {
				g_list_free(keys);
			}
			if (values) {
				g_list_free(values);
			}
		}
	} else if (JSON_NODE_TYPE(root) == JSON_NODE_ARRAY) {
		JsonArray* array = json_node_get_array(root);
		guint array_size = json_array_get_length (array);
		JsonNode *array_element;

		for (guint i = 0; i < array_size; i++) {
			array_element = json_array_get_element(array, i);
			json_parse(array_element, node, true);
		}
	} else if (JSON_NODE_TYPE(root) == JSON_NODE_VALUE) {
		node = g_node_append(node, g_node_new(json_string(root)));

		if (parsing_array) {
			node = g_node_append(node, g_node_new(NULL));
		}
	}
}
