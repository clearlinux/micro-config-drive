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
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <curl/curl.h>
#include <glib.h>

#include "lib.h"
#include "curl.h"

#define MOD "curl: "

#ifdef DEBUG
	#define CURL_VERBOSE 1
	#define CURL_NO_PROGRESS 0
#else
	#define CURL_VERBOSE 0
	#define CURL_NO_PROGRESS 1
#endif /* DEBUG */


bool curl_common_init(CURL** curl) {
	*curl = curl_easy_init();

	if (!*curl) {
		LOG(MOD "Curl easy init failed\n");
		goto fail1;
	}
	if (curl_easy_setopt(*curl, CURLOPT_VERBOSE, CURL_VERBOSE) != CURLE_OK) {
		goto fail2;
	}
	if (curl_easy_setopt(*curl, CURLOPT_NOPROGRESS, CURL_NO_PROGRESS) != CURLE_OK) {
		goto fail2;
	}
	if (curl_easy_setopt(*curl, CURLOPT_PROXY, "") != CURLE_OK) {
		goto fail2;
	}
	if (curl_easy_setopt(*curl, CURLOPT_FAILONERROR, 1) != CURLE_OK) {
		goto fail2;
	}

	return true;

fail2:
	curl_easy_cleanup(*curl);
fail1:
	return false;
}

gchar* curl_fetch_file(CURL* curl, gchar* url, const gchar* outdir, int attempts, useconds_t u_sleep) {
	int fd;
	FILE* file;
	gchar* filename;

	filename = malloc(sizeof(gchar)*PATH_MAX);
	g_snprintf(filename, PATH_MAX, "%s/cloud-init-XXXXXX", outdir);

	fd = mkstemp(filename);
	if (fd < 0) {
		LOG(MOD "mkstemp failed with: %s\n", strerror(errno));
		return NULL;
	}

	file = fdopen(fd, "w");

	if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK) {
		LOG(MOD "set url failed\n");
		fclose(file);
		return NULL;
	}
	if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, file) != CURLE_OK) {
		LOG(MOD "set write data failed\n");
		fclose(file);
		return NULL;
	}

	if (curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 0) != CURLE_OK) {
		LOG(MOD "unset connect only failed\n");
		fclose(file);
		return false;
	}

	for (int i = 0; i < attempts; ++i) {
		LOG(MOD "%s attempt %d\n", url, i);
		if (curl_easy_perform(curl) == CURLE_OK) {
			fclose(file);
			return filename;
		}
		usleep(u_sleep);
	}

	return NULL;
}

bool curl_ping(CURL* curl, gchar* url, int attempts, useconds_t u_sleep) {
	if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK) {
		LOG(MOD "set url failed\n");
		return false;
	}

	if (curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1) != CURLE_OK) {
		LOG(MOD "set connect only failed\n");
		return false;
	}

	for (int i = 0; i < attempts; ++i) {
		LOG(MOD "%s attempt %d\n", url, i);
		if (curl_easy_perform(curl) == CURLE_OK) {
			return true;
		}
		usleep(u_sleep);
	}

	return false;
}
