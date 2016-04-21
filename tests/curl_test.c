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
#include <stdlib.h>
#include <sys/stat.h>

#include <check.h>
#include <glib.h>
#include <curl/curl.h>

#include "curl.h"

START_TEST(test_curl_fetch_file)
{
	int fd;
	char filename[] = "/tmp/test_lib_exec_task-XXXXXX";
	gchar* file_fetched;
    GString* url;
	CURL* curl = NULL;
	struct stat buf;

	url = g_string_new("file://");

	ck_assert(curl_common_init(&curl) == true);
	ck_assert(curl != NULL);

	fd = mkstemp(filename);
	ck_assert(fd != -1);

	g_string_append(url, filename);
	file_fetched = curl_fetch_file(curl, url->str, "/tmp", 1, 0);
	g_string_free(url, true);
	ck_assert(file_fetched != NULL);
	ck_assert(remove(filename) != -1);

	ck_assert(stat(file_fetched, &buf) != 1);
	ck_assert(remove(file_fetched) != -1);
}
END_TEST


Suite* make_curl_suite(void) {
	Suite *s;
	TCase *tc_fetch_file;

	s = suite_create("curl");

	tc_fetch_file = tcase_create("tc_fetch_file");
	tcase_add_test(tc_fetch_file, test_curl_fetch_file);

	suite_add_tcase(s, tc_fetch_file);

	return s;
}

int main(void) {
	int number_failed;
	Suite* s;
	SRunner* sr;

	s = make_curl_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
