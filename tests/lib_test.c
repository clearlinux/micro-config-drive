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

#include <check.h>

#include "lib.h"


START_TEST(test_lib_exec_task)
{
	ck_assert(exec_task("echo 'this is a test!'") == true);
}
END_TEST

START_TEST(test_lib_chown_path)
{
	int fd;
	char template[] = "/tmp/fileXXXXXX";
	uid_t user_id;
	gid_t group_id;
	struct passwd *pswd;
	struct group *grp;

	fd = mkstemp(template);
	ck_assert(fd != -1);

	user_id = getuid();
	group_id = getgid();

	pswd = getpwuid(user_id);
	ck_assert(pswd != NULL);

	grp = getgrgid(group_id);
	ck_assert(grp != NULL);

	ck_assert(chown_path(template, pswd->pw_name, grp->gr_name) == 0);
}
END_TEST

Suite* make_lib_suite(void) {
	Suite *s;
	TCase *tc_exec_task;
	TCase *tc_chown_path;

	s = suite_create("lib");

	tc_exec_task = tcase_create("tc_exec_task");
	tcase_add_test(tc_exec_task, test_lib_exec_task);

	tc_chown_path = tcase_create("tc_chown_path");
	tcase_add_test(tc_chown_path, test_lib_chown_path);

	suite_add_tcase(s, tc_exec_task);
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
