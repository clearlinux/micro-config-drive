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
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>

#include <glib.h>

#include "handlers.h"
#include "disk.h"
#include "lib.h"
#include "userdata.h"
#include "datasources.h"
#include "default_user.h"
#include "openstack.h"


/* Long options */
enum {
	OPT_OPENSTACK_METADATA_FILE=1001,
	OPT_OPENSTACK_USER_DATA,
	OPT_OPENSTACK_METADATA,
	OPT_NO_GROWPART,
};

static struct option opts[] = {
	{ "user-data-file",             required_argument, NULL, 'u' },
	{ "openstack-metadata-file",    required_argument, NULL, OPT_OPENSTACK_METADATA_FILE },
	{ "openstack-user-data",        no_argument, NULL, OPT_OPENSTACK_USER_DATA },
	{ "openstack-metadata",         no_argument, NULL, OPT_OPENSTACK_METADATA },
	{ "help",                       no_argument, NULL, 'h' },
	{ "version",                    no_argument, NULL, 'v' },
	{ "first-boot",                 no_argument, NULL, 'b' },
	{ "no-growpart",                no_argument, NULL, OPT_NO_GROWPART },
	{ NULL, 0, NULL, 0 }
};

int main(int argc, char *argv[]) {
	int c;
	int i;
	int result_code = EXIT_SUCCESS;
	bool no_growpart = false;
	bool openstack_flag = false;
	bool first_boot = false;
	struct datasource_options_struct datasource_opts = { 0 };
	gchar* userdata_filename = NULL;
	gchar* tmp_metafile = NULL;
	gchar metadata_filename[PATH_MAX] = { 0 };
	gchar command[LINE_MAX];
	gchar* root_disk;
	GString* sudo_directives = NULL;

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
			LOG("-u, --user-data-file [file]            specify a custom user data file\n");
			LOG("    --openstack-metadata-file [file]   specify an Openstack metadata file\n");
			LOG("    --openstack-user-data              get and process user data from Openstack\n");
			LOG("                                       metadata service\n");
			LOG("    --openstack-metadata               get and process metadata from Openstack\n");
			LOG("                                       metadata service\n");
			LOG("-h, --help                             display this help message\n");
			LOG("-v, --version                          display the version number of this program\n");
			LOG("-b, --first-boot                       set up the system in its first boot\n");
			LOG("    --no-growpart                      do not verify disk partitions.\n");
			LOG("                                       %s will not resize the filesystem\n", argv[0]);
			LOG("If no user data or metadata is provided on the command line,\n");
			LOG("%s will fetch these through the datasources API's.\n", argv[0]);
			exit(EXIT_SUCCESS);
			break;

		case 'v':
			fprintf(stdout, PACKAGE_NAME " " PACKAGE_VERSION "\n");
			exit(EXIT_SUCCESS);
			break;

		case 'b':
			first_boot = true;
			break;

		case '?':
			exit(EXIT_FAILURE);
			break;

		case OPT_OPENSTACK_METADATA_FILE:
			openstack_flag = true;
			tmp_metafile = g_strdup(optarg);
			break;

		case OPT_OPENSTACK_USER_DATA:
			openstack_flag = true;
			datasource_opts.user_data = true;
			break;

		case OPT_OPENSTACK_METADATA:
			openstack_flag = true;
			datasource_opts.metadata = true;
			break;

		case OPT_NO_GROWPART:
			no_growpart = true;
			break;
		}
	}

	/* check if metadata file exists */
	if (tmp_metafile) {
		if (!realpath(tmp_metafile, metadata_filename)) {
			LOG("Unable to determine real file path for '%s'\n", tmp_metafile);
		}
		g_free(tmp_metafile);
	}

	#ifdef HAVE_CONFIG_H
		LOG("clr-cloud-init version: %s\n", PACKAGE_VERSION);
	#endif /* HAVE_CONFIG_H */

	/* at one point in time this should likely be a fatal error */
	if (geteuid() != 0) {
		LOG("%s isn't running as root, this will most likely fail!\n", argv[0]);
	}

	if (!no_growpart) {
		root_disk = disk_for_path("/");
		if (root_disk) {
			if (!disk_resize_grow(root_disk)) {
				LOG("Resizing and growing disk '%s' failed\n", root_disk);
			}
		} else {
			LOG("Root disk not found\n");
		}
	}

	if (first_boot) {
		/* default user will be used by ccmodules and datasources */
		command[0] = 0;
		snprintf(command, LINE_MAX, USERADD_PATH
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
		sudo_directives = g_string_new(DEFAULT_USER_SUDO);
		if (!write_sudo_directives(sudo_directives, DEFAULT_USER_USERNAME"-cloud-init")) {
			LOG("Failed to enable sudo rule for user: %s\n", DEFAULT_USER_SUDO);
		}
		g_string_free(sudo_directives, true);

		/* lock root account for security */
		snprintf(command, LINE_MAX, USERMOD_PATH " -p '!' root");
		exec_task(command);
	}

	/* process userdata file */
	if (userdata_filename) {
		result_code = userdata_process_file(userdata_filename);
	}

	/* process metadata file */
	if (metadata_filename[0]) {
		if (openstack_flag) {
			result_code = openstack_process_metadata(metadata_filename);
		}
	}

	if (!userdata_filename && !metadata_filename[0]) {
		/* get/process userdata and metadata from datasources */
		for (i = 0; cloud_structs[i] != NULL; ++i) {
			result_code = cloud_structs[i]->handler(&datasource_opts);
			if (EXIT_SUCCESS == result_code) {
				break;
			}
		}
	}

	exit(result_code);
}
