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
#include <sys/sysinfo.h>

#include "async_task.h"
#include "lib.h"

#define MOD "async_task: "

static GThreadPool* thread_pool = NULL;
static GMainLoop* main_loop = NULL;
static guint tasks = 0;

G_LOCK_DEFINE(thread_pool);
G_LOCK_DEFINE(tasks);

struct async_task_data {
	GThreadFunc func;
	gpointer data;
};

static void async_task_callback(GPid pid, gint status, __unused__ gpointer null) {
	LOG("PID %d ends, exit status %d\n", pid, status);

	g_spawn_close_pid(pid);

	G_LOCK(tasks);
	--tasks;
	if (0 == tasks) {
		LOG(MOD "Quit main loop\n");
		g_main_loop_quit(main_loop);
	}
	G_UNLOCK(tasks);
}

static void async_task_run_task(struct async_task_data* data, __unused__ gpointer null) {
	if (data && data->func) {
		data->func(data->data);
	}

	g_free(data);
}

bool async_task_init(void) {
	main_loop = g_main_loop_new(NULL, 0);
	if (!main_loop) {
		LOG(MOD "Cannot create a new main loop\n");
		return false;
	}

	thread_pool = g_thread_pool_new((GFunc)async_task_run_task, NULL, get_nprocs(), true, NULL);
	if (!thread_pool) {
		LOG(MOD "Cannot create a new thread pool\n");
		g_main_loop_unref(main_loop);
		main_loop = NULL;
		return false;
	}

	return true;
}

bool async_task_run(GThreadFunc func, gpointer data) {
	GError *error = NULL;
	struct async_task_data* task_data = g_malloc(sizeof(struct async_task_data));
	task_data->func = func;
	task_data->data = data;

	G_LOCK(thread_pool);
	g_thread_pool_push(thread_pool, task_data, &error);
	G_UNLOCK(thread_pool);

	if (error) {
		LOG(MOD "Error pushing a new thread: %s\n", error->message);
		g_error_free(error);
		return false;
	}

	return true;
}

bool async_task_exec(const gchar* command) {
	GPid pid = 0;
	GError *error = NULL;
	gchar* command_line = g_strescape(command, NULL);
	gchar* argvp[] = {SHELL_PATH, "-c", command_line, NULL };

	g_spawn_async(NULL, argvp, NULL,
	              G_SPAWN_DO_NOT_REAP_CHILD,
				  NULL, NULL, &pid, &error);

	if (error) {
		LOG(MOD "Error running async command: %s\n", error->message);
		g_error_free(error);
		g_free(command_line);
		return false;
	}

	g_child_watch_add(pid, async_task_callback, NULL);

	LOG(MOD "Executing [%d]: %s -c \"%s\"\n", pid, SHELL_PATH, command_line);

	g_free(command_line);

	G_LOCK(tasks);
	++tasks;
	G_UNLOCK(tasks);

	return true;
}

void async_task_finish(void) {
	if (tasks) {
		g_main_loop_run(main_loop);
	}
	g_main_loop_unref(main_loop);
	main_loop = NULL;

	g_thread_pool_free(thread_pool, false, true);
	thread_pool = NULL;

	tasks = 0;
}
