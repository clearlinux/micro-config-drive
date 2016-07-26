/***
 Copyright © 2015 Intel Corporation

 Author: Auke-jan H. Kok <auke-jan.h.kok@intel.com>
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

#pragma once

#include <yaml.h>
#include <glib.h>

/*
 * Macro helper - allows for easy retrieval of node data.
 * Use as follows:
 *     CLOUD_CONFIG_KEY(NAME_OF_KEY, "PATHPART1", "PATHPART2" ......)
 */
#define CLOUD_CONFIG_KEY(name, ...) gchar* name[] = { __VA_ARGS__, 0}

bool cloud_config_bool(GNode* node, bool *b);

bool cloud_config_int(const GNode* node, int *i);

bool cloud_config_int_base(const GNode* node, int *i, int base);

GNode *cloud_config_find(GNode* node, gchar** path);

void cloud_config_set_global(gpointer key, gpointer value);
gpointer cloud_config_get_global(gpointer option);
