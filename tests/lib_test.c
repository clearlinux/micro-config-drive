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

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include <check.h>

#include "lib.h"

START_TEST(test_lib_exec_task)
{
	int fd;
	char filename[] = "/tmp/test_lib_exec_task-XXXXXX";
	const char *text = "this is a test!";
	char line[LINE_MAX] = { 0 };
	FILE* file;

	fd = mkstemp(filename);
	ck_assert(fd != -1);

	snprintf(line, LINE_MAX, "echo -n '%s' > %s", text, filename);
	ck_assert(exec_task(line) == true);

	file = fdopen(fd, "r");
	ck_assert(file != NULL);
	fgets(line, LINE_MAX, file);
	ck_assert_str_eq(line, text);
	ck_assert(fclose(file) != EOF);

	ck_assert(remove(filename) != -1);
}
END_TEST

START_TEST(test_lib_write_file)
{
	int fd;
	bool write_file_result;
	char filename[] = "/tmp/test_lib_write_file-XXXXXX";
	GString* text;
	char line[LINE_MAX] = { 0 };
	FILE* file;
	struct stat buf;

	text = g_string_new("this is a test!");

	fd = mkstemp(filename);
	ck_assert(fd != -1);

	write_file_result = write_file(text, filename,
		O_CREAT|O_WRONLY, S_IRWXU|S_IRWXG|S_IRWXO);
	ck_assert(write_file_result == true);

	file = fopen(filename, "r");
	ck_assert(file != NULL);
	fgets(line, LINE_MAX, file);
	ck_assert_str_eq(line, text->str);
	ck_assert(fclose(file) != EOF);
	g_string_free(text, true);


	ck_assert(stat(filename, &buf) != -1);
	ck_assert((buf.st_mode&S_IRWXU) == S_IRWXU);
	ck_assert((buf.st_mode&S_IRWXG) == S_IRWXG);
	ck_assert((buf.st_mode&S_IRWXO) == S_IRWXO);

	ck_assert(remove(filename) != -1);
}
END_TEST

START_TEST(test_lib_chown_path)
{
	int fd;
	char filename[] = "/tmp/test_lib_chown_path-XXXXXX";
	uid_t user_id;
	gid_t group_id;
	struct passwd *pswd;
	struct group *grp;

	fd = mkstemp(filename);
	ck_assert(fd != -1);

	user_id = getuid();
	pswd = getpwuid(user_id);
	ck_assert(pswd != NULL);

	group_id = getgid();
	grp = getgrgid(group_id);
	ck_assert(grp != NULL);

	ck_assert(chown_path(filename, pswd->pw_name, grp->gr_name) == 0);

	ck_assert(remove(filename) == 0);
}
END_TEST

Suite* make_lib_suite(void) {
	Suite *s;
	TCase *tc_exec_task;
	TCase *tc_write_file;
	TCase *tc_chown_path;

	s = suite_create("lib");

	tc_exec_task = tcase_create("tc_exec_task");
	tcase_add_test(tc_exec_task, test_lib_exec_task);

	tc_write_file = tcase_create("tc_write_file");
	tcase_add_test(tc_write_file, test_lib_write_file);

	tc_chown_path = tcase_create("tc_chown_path");
	tcase_add_test(tc_chown_path, test_lib_chown_path);

	suite_add_tcase(s, tc_exec_task);
	suite_add_tcase(s, tc_write_file);
	suite_add_tcase(s, tc_chown_path);

	return s;
}

int main(void) {
	int number_failed;
	Suite* s;
	SRunner* sr;

	s = make_lib_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
