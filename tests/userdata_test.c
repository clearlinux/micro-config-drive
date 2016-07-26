/***
 Copyright © 2015 Intel Corporation

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

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include <check.h>
#include <glib.h>

#include "userdata.h"

START_TEST(test_userdata_process_file)
{
	int fd_script;
	int fd_script_outfile;
	char script_file[] = "/tmp/test_userdata_process_file-XXXXXX";
	char script_outfile[] = "/tmp/test_userdata_process_file-XXXXXX";
	char *text = "this is a test!";
	GString* script_text = g_string_new("#!/bin/bash\necho -n ");
	FILE* file;
	char line[LINE_MAX] = { 0 };
	struct stat buf;

	fd_script_outfile = mkstemp(script_outfile);
	ck_assert(fd_script_outfile != -1);

	g_string_append_printf(script_text, "'%s' > %s\n", text, script_outfile);

	fd_script = mkstemp(script_file);
	ck_assert(fd_script != -1);
	file = fdopen(fd_script, "w");
	ck_assert(file != NULL);
	fwrite(script_text->str, sizeof(script_text->str[0]), script_text->len, file);
	g_string_free(script_text, true);
	ck_assert(fclose(file) != EOF);
	ck_assert(userdata_process_file(script_file) == true);
	ck_assert(remove(script_file) != -1);

	file = fdopen(fd_script_outfile, "r");
	ck_assert(file != NULL);
	fgets(line, LINE_MAX, file);
	ck_assert_str_eq(line, text);
	ck_assert(fclose(file) != EOF);
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
