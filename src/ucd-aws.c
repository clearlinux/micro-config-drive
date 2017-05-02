
/***
 Copyright © 2017 Intel Corporation

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

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#define FAIL(err) do { perror(err); exit(EXIT_FAILURE); } while(0)

#define AWS_USER_DATA_PATH "/var/lib/cloud"
#define AWS_USER_DATA "aws-user-data"
#define AWS_IP "169.254.169.254"
#define AWS_REQUEST "GET /latest/user-data HTTP/1.1\r\nhost: " AWS_IP "\r\nConnection: close\r\n\r\n"

int main(void) {
	int sockfd;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		FAIL("socket()");
	}
	
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(AWS_IP);
	server.sin_port = htons(80);

	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 50000000;

	for (;;) {
		int n = 0;
		int r = connect(sockfd, (struct sockaddr *)&server, sizeof(server));
		if (r == 0) {
			break;
		}
		if ((r != EAGAIN) && (r != ENETUNREACH) && (r != ETIMEDOUT)) {
			FAIL("connect()");
		}
		nanosleep(&ts, NULL);
		if (++n > 200) { /* 10 secs */
			FAIL("timeout in connect()");
		}
	}

	char *message = AWS_REQUEST;
	size_t len = strlen(message);

	char response[4096];
	memset(response, 0, sizeof(response));

	if (write(sockfd, message, len) < (ssize_t)len) {
		close(sockfd);
		FAIL("write()");
	}

	FILE *f = fdopen(sockfd, "r");
	if (!f) {
		close(sockfd);
		FAIL("fdopen()");
	}
	for (;;) {
		char buf[512];
		char *r = fgets(buf, sizeof(buf), f);
		if (!r) {
			close(sockfd);
			FAIL("fgets()");
		}
		if (strncmp(buf, "\r\n", 2) == 0) {
			break;
		}
	}

	int out;
	(void) mkdir(AWS_USER_DATA_PATH, 0);
	(void) unlink(AWS_USER_DATA_PATH "/" AWS_USER_DATA);
	out = open(AWS_USER_DATA_PATH "/" AWS_USER_DATA, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (out < 0) {
		close(sockfd);
		FAIL("open()");
	}

	for (;;) {
		char buf[512];
		char *r = fgets(buf, sizeof(buf), f);
		size_t len = strlen(buf);
		if (r == 0) {
			break;
		}
		if (write(out, buf, len) < (ssize_t)len) {
			close(out);
			fclose(f);
			unlink(AWS_USER_DATA_PATH "/" AWS_USER_DATA);
			FAIL("write()");
		}
	}

	close(out);
	fclose(f);

	(void) execl(BINDIR "/ucd", BINDIR "/ucd", "-u",
			AWS_USER_DATA_PATH "/" AWS_USER_DATA, (char *)NULL);
	FAIL("exec()");
}
