/***
 Copyright (C) 2015 Intel Corporation

 Author: Auke-jan H. Kok <auke-jan.h.kok@intel.com>

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

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <stdio.h>
#include <stdbool.h>

#include <glib.h>

#include "lib.h"

#ifdef DEBUG
static gboolean node_dump(GNode *node, __unused__ gpointer data) {
	fprintf(stderr, "debug: ");
	for (guint i = 0; i < g_node_depth(node); i++)
		fprintf(stderr, "    ");
	fprintf(stderr, "[%d]:%s\n", g_node_depth(node), node ? (char*)node->data : "(null)");
	return(false);
}

void cloud_config_dump(GNode *node) {
	fprintf(stderr, "debug: " "======== Dumping userdata GNode: ========\n");
	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, node_dump, NULL);
}
#else
void cloud_config_dump(GNode* node) {}
#endif /* DEBUG */
