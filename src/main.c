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

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>

#include <glib.h>

#include "handlers.h"
#include "lib.h"
#include "userdata.h"
#include "datasources.h"
#include "default_user.h"

static struct option opts[] = {
	{ "user-data-file", 1, NULL, 'u' },
	{ "help",           0, NULL, 'h' },
	{ "version",        0, NULL, 'v' },
	{ "first-boot",     0, NULL, 'b' },
	{ NULL, 0, NULL, 0 }
};

int main(int argc, char *argv[]) {
	int result_code = EXIT_SUCCESS;
	gchar *userdata_filename = NULL;
	bool first_boot = false;
	int c;
	int i;

	while (true) {
		c = getopt_long(argc, argv, "u:hvb", opts, &i);

		if (c == -1) {
			break;
		}

		switch (c) {

		case 'u':
			userdata_filename = g_strdup(optarg);
			break;

		case 'h':
			LOG("Usage: %s [options]\n", argv[0]);
			LOG("-u, --user-data-file [file]        specify a custom user data file\n");
			LOG("-h, --help                         display this help message\n");
			LOG("-v, --version                      display the version number of this program\n");
			LOG("-b, --first-boot                   set up the system in its first boot (create default user, etc)\n");
			exit(EXIT_FAILURE);
			break;

		case 'v':
			fprintf(stdout, PACKAGE_NAME " " PACKAGE_VERSION "\n");
			exit(EXIT_FAILURE);
			break;

		case 'b':
			first_boot = true;
			break;
		}
	}

	#ifdef HAVE_CONFIG_H
		LOG("clr-cloud-init version: %s\n", PACKAGE_VERSION);
	#endif /* HAVE_CONFIG_H */

	/* at one point in time this should likely be a fatal error */
	if (geteuid() != 0) {
		LOG("%s isn't running as root, this will most likely fail!\n", argv[0]);
	}

	if (first_boot) {
		/* default user will be used by ccmodules and datasources */
		char command[LINE_MAX] = { 0 };
		snprintf(command, LINE_MAX, "useradd"
				" -U -d '%s' -G '%s' -f '%s' -e '%s' -s '%s' -c '%s' -p '%s' '%s'"
				, DEFAULT_USER_HOME_DIR
				, DEFAULT_USER_GROUPS
				, DEFAULT_USER_INACTIVE
				, DEFAULT_USER_EXPIREDATE
				, DEFAULT_USER_SHELL
				, DEFAULT_USER_GECOS
				, DEFAULT_USER_PASSWORD
				, DEFAULT_USER_USERNAME);
		exec_task(command);

		/* default user will be able to use sudo */
		write_sudo_string(DEFAULT_USER_USERNAME"-cloud-init", DEFAULT_USER_SUDO);

		/* lock root account for security */
		snprintf(command, LINE_MAX, "usermod -p '!' root");
		exec_task(command);
	}

	if (!userdata_filename) {
		/* get/process userdata and metadata from datasources */
		for (i = 0; cloud_structs[i] != NULL; ++i) {
			result_code = cloud_structs[i]->handler(first_boot);
			if (EXIT_SUCCESS == result_code) {
				break;
			}
		}
	} else {
		result_code = userdata_process_file(userdata_filename);
	}

	exit(result_code);
}
