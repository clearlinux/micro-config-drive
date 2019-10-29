/***
 Copyright © 2019 Intel Corporation

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
#include <netdb.h>
#include <unistd.h>

#include <glib.h>

#include "handlers.h"
#include "cloud_config.h"
#include "lib.h"

#define MOD "package_upgrade: "

static int do_network_wait = -1;
// -1: default: unset
//  0: don't wait
//  1: wait
//  2: wait already happened

void wait_for_network(void) {
	if (do_network_wait == 1) {
		struct hostent *he = NULL;
		useconds_t slept = 0;
		useconds_t times = 0;

		// don't re-enter
		do_network_wait = 2;

		memset(he, 0, sizeof(struct hostent));
		LOG(MOD "Waiting for an active network connection.\n");
		for (;;) {
			he = gethostbyname(DNSTESTADDR);
			if (he) {
				LOG(MOD "Network appears active, waiting completed.\n");
				break;
			}
			times = times < 10 ? times + 1 : times;
			slept += times;
			if (slept > 3000) {
				LOG(MOD "Waited for network for 5 minutes, no answer - giving up.\n");
				break;
			}
			usleep(times * 100000);
		}
	}
}

void wait_for_network_handler(GNode *node) {
	bool do_wait;

	LOG(MOD "Wait System Software Update Handler running...\n");
	GNode* val = g_node_first_child(node);
	if (!val) {
		LOG(MOD "Corrupt userdata!\n");
		return;
	}
	if (!cloud_config_bool(val, &do_wait)) {
		return;
	}

	if (do_wait) {
		do_network_wait = 1;
		wait_for_network();
	} else {
		do_network_wait = 0;
		LOG(MOD "Disabling network wait.\n");
	}
}

struct cc_module_handler_struct wait_for_network_cc_module = {
	.name = "wait_for_network",
	.handler = &wait_for_network_handler
};
