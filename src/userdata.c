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

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>

#include <glib.h>

#include "lib.h"
#include "interpreters.h"
#include "handlers.h"

#define MOD "userdata: "

int userdata_process_file(const gchar* filename) {
	char shebang[LINE_MAX] = { 0 };

	LOG(MOD "Looking for shebang file %s\n", filename);
	FILE *userdata_file = fopen(filename, "rb");
	fgets(shebang, LINE_MAX, userdata_file);
	fclose(userdata_file);
	LOG(MOD "Shebang found %s\n", shebang);

	/* built-in interpreters */
	for (int i = 0; interpreter_structs[i] != NULL; ++i) {
		if (g_str_has_prefix(shebang, interpreter_structs[i]->shebang)) {
			return interpreter_structs[i]->handler(filename);
		}
	}

	LOG(MOD "No interpreter found for %s\n", shebang);
	//FIXME define return codes, not use magic numbers
	return 1;
}
