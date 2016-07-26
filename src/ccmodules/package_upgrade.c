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
#include <stdio.h>

#include <glib.h>

#include "handlers.h"
#include "cloud_config.h"
#include "lib.h"

#define MOD "package_upgrade: "

void package_upgrade_handler(GNode *node) {
	bool do_upgrade;

	LOG(MOD "System Software Update Handler running...\n");
	GNode* val = g_node_first_child(node);
	if (!val) {
		LOG(MOD "Corrupt userdata!\n");
		return;
	}
	if (!cloud_config_bool(val, &do_upgrade)) {
		return;
	}
	if (do_upgrade) {
		LOG(MOD "Performing system software update.\n");
#if defined(PACKAGE_MANAGER_SWUPD)
		exec_task("/usr/bin/swupd update");
#elif defined(PACKAGE_MANAGER_YUM)
		exec_task("/usr/bin/yum update");
#elif defined(PACKAGE_MANAGER_DNF)
		exec_task("/usr/bin/dnf update --refresh");
#elif defined(PACKAGE_MANAGER_APT)
		exec_task("/usr/bin/apt-get upgrade");
#elif defined(PACKAGE_MANAGER_TDNF)
		exec_task("/usr/bin/tdnf update --refresh --assumeyes");
#endif
	} else {
		LOG(MOD "Skipping system software update.\n");
	}
}

struct cc_module_handler_struct package_upgrade_cc_module = {
	.name = "package_upgrade",
	.handler = &package_upgrade_handler
};

