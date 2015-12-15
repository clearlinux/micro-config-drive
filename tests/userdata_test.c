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
#include <stdio.h>
#include <sys/stat.h>

#include <check.h>
#include <glib.h>

#include "userdata.h"

START_TEST(test_userdata_process_file)
{
	int fd;
	char filename[] = "/tmp/test_userdata_process_file-XXXXXX";
	char* script_outfile = "/tmp/test_userdata_process_file-result";
	GString* script = g_string_new("#!/bin/bash\necho \"this is a test!\" > ");
	FILE* file;
	struct stat buf;

	g_string_append(script, script_outfile);

	fd = mkstemp(filename);
	ck_assert(fd != -1);
	file = fdopen(fd, "w");
	ck_assert(file != NULL);
	fwrite(script->str, sizeof(script->str[0]), script->len, file);
	ck_assert(fclose(file) != EOF);
	ck_assert(userdata_process_file(filename) == true);
	ck_assert(remove(filename) != -1);

	ck_assert(stat(script_outfile, &buf) != -1);
	ck_assert(remove(script_outfile) != -1);
}
END_TEST


Suite* make_userdata_suite(void) {
	Suite *s;
	TCase *tc_process_file;

	s = suite_create("userdata");

	tc_process_file = tcase_create("tc_process_file");
	tcase_add_test(tc_process_file, test_userdata_process_file);

	suite_add_tcase(s, tc_process_file);

	return s;
}

int main(void) {
	int number_failed;
	Suite* s;
	SRunner* sr;

	s = make_userdata_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
