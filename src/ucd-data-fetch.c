
/***
 Copyright © 2017-2019 Intel Corporation

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

#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#define FAIL(err) do { perror(err); exit(EXIT_FAILURE); } while(0)

#define USER_DATA_PATH "/var/lib/cloud"

struct cloud_struct {
	char *name;
	char *ip;
	uint16_t port;
	char *request_sshkey_path;
	char *request_hostname_path;
	char *request_userdata_path;
	char *cloud_config_header;
};

#define MAX_CONFIGS 6
static struct cloud_struct config[MAX_CONFIGS] = {
	{
		"aws",
		"169.254.169.254",
		80,
		"/latest/meta-data/public-keys/0/openssh-key",
		"/latest/meta-data/hostname",
		"/latest/user-data",
		"#cloud-config\n" \
		"users:\n" \
		"  - name: clear\n" \
		"    groups: wheelnopw\n" \
		"ssh_authorized_keys:\n"
	},
	{
		"oci",
		"169.254.169.254",
		80,
		"/opc/v1/instance/metadata/ssh_authorized_keys",
		NULL,
		NULL,
		"#cloud-config\n" \
		"users:\n" \
		"  - name: opc\n" \
		"    groups: wheelnopw\n" \
		"    gecos: Oracle Public Cloud User\n" \
		"ssh_authorized_keys:\n"
	},
	{
		"tencent",
		"169.254.0.23",
		80,
		"/latest/meta-data/public-keys/0/openssh-key",
		NULL,
		NULL,
		"#cloud-config\n" \
		"users:\n" \
		"  - name: tencent\n" \
		"    groups: wheelnopw\n" \
		"ssh_authorized_keys:\n"
	},
	{
		"aliyun",
		"100.100.100.200",
		80,
		"/latest/meta-data/public-keys/0/openssh-key",
		NULL,
		NULL,
		"#cloud-config\n" \
		"users:\n" \
		"  - name: aliyun\n" \
		"    groups: wheelnopw\n" \
		"ssh_authorized_keys:\n"
	},
	{
		"equinix",
		"metadata.platformequinix.com",
		80,
		"/2009-04-04/meta-data/public-keys",
		"/2009-04-04/meta-data/hostname",
		"/userdata",
		"#cloud-config\n" \
		"users:\n" \
		"  - name: clear\n" \
		"    groups: wheelnopw\n" \
		"ssh_authorized_keys:\n"
	},
	{
		"test",
		"127.0.0.254",
		8123,
		"/public-keys",
		"/hostname",
		"/user-data",
		"#cloud-config\n" \
		"users:\n" \
		"  - name: clear\n" \
		"    groups: wheelnopw\n" \
		"ssh_authorized_keys:\n"
	}
};



/*
 * parse_headers:
 * f: file descriptor for input (stream)
 * *cl: output content-length
 * return values: status code
 * - 0: an actual error occurred.
 * - 1: parsed headers OK in full, ready to read content.
 * - 2: non-200 exit status, but no error in conversation.
 */
static int parse_headers(FILE *f, size_t *cl)
{
	for (;;) {
		char buf[512];
		char *r = fgets(buf, sizeof(buf), f);
		if (!r) {
			return 0;
		}
		if (strncmp(buf, "\r\n", 2) == 0) {
			/* end of headers */
			break;
		} else if (strncmp(buf, "Content-Length:", 15) == 0) {
			/* content length */
			*cl = (size_t)strtol(&buf[16], NULL, 10);
			if (errno == EINVAL || errno == ERANGE) {
				return 0;
			}
		} else if ((strncmp(buf, "HTTP/1.0", 8) == 0) ||
			   (strncmp(buf, "HTTP/1.1", 8) == 0)) {
			long int status = strtol(&buf[8], NULL, 10);
			if (errno == EINVAL || errno == ERANGE) {
				return 0;
			}
			/* fail if non-200 exit code */
			if (status < 200 || status >= 299 ) {
				return 2;
			}
		}
	}
	return 1;
}

/**
 * write_lines() - write remaining lines from stream f into out, while minding cl length
 * - if prefix != NULL, each line written is prefixed with the prefix.
 * - returns 0 on success, 1 on failure
 * - after this call, the value of `cl` outside the function is invalid.
 */
static int write_lines(int out, FILE *f, size_t cl, const char *prefix)
{
	for (;;) {
		if (cl == 0) {
			return 0;
		}

		char buf[2048] = {0};

		char *r = fgets(buf, sizeof(buf), f);
		if (ferror(f)) {
			return 1;
		} else if (!r) {
			return 0;
		}

		size_t len = strlen(r);
		cl -= len;

		if (prefix) {
			if (write(out, prefix, strlen(prefix)) < (ssize_t)strlen(prefix))
				return 1;
		}

		if (write(out, buf, len) < (ssize_t)len) {
			return 1;
		}

		/* Make sure this line ends with a newline when we write it */
		if (buf[len-1] != '\n') {
			if (write(out, "\n", 1) < (ssize_t)1) {
				return 1;
			}
		}
	}
}

int main(int argc, char *argv[]) {
	int conf = -1;
	int sockfd;
	char *request, *request2, *request3;
	char *outpath;
	int n = 0;

	if (argc != 2) {
		FAIL("No cloud service provider passed as arg1, unable to continue\n");
	}

	if ((strcmp(argv[1], "--help") == 0) ||
	    (strcmp(argv[1], "-h") == 0)) {
		fprintf(stderr, "Usage: ucd-userdata-fetch <cloud service provider name>\n"
			"Known cloud service provider names:\n");
		for (int i = 0; i < MAX_CONFIGS; i++)
			fprintf(stderr, "      - %s\n", config[i].name);
		exit(EXIT_SUCCESS);
	}

	for (int i = 0; i < MAX_CONFIGS; i++) {
		if (strcmp(argv[1], config[i].name) == 0) {
			conf = i;
			break;
		}
	}

	if (conf == -1) {
		fprintf(stderr, "Unknown cloud service provider name: %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		FAIL("socket()");
	}

	struct sockaddr_in server;
	memset(&server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(config[conf].ip);
	server.sin_port = htons(config[conf].port);

	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 50000000;

	/* Do we need to look up a hostname? */
	if ((int) server.sin_addr.s_addr == -1) {
		n = 0;
		for (;;) {
			struct hostent *hp = gethostbyname(config[conf].ip);
			if (hp != NULL) {
				if (hp->h_length > 0) {
					/* Got it; use the resulting IP address */
					server.sin_family = (short unsigned int) (hp->h_addrtype & 0xFFFF);
					memcpy(&(server.sin_addr.s_addr), hp->h_addr, (size_t) hp->h_length);
					break;
				}
				else {
					fprintf(stderr, "gethostbyname(): empty response");
					exit(EXIT_FAILURE);
				}
			}

			if ((h_errno != TRY_AGAIN) && (h_errno != NO_RECOVERY)) {
				herror("gethostbyname()");
				exit(EXIT_FAILURE);
			}
			nanosleep(&ts, NULL);
			if (++n > 2000) { /* 100 secs */
				herror("gethostbyname()");
				exit(EXIT_FAILURE);
			}
		}
	}

	for (;;) {
		int r = connect(sockfd, (struct sockaddr *)&server, sizeof(server));
		if (r == 0) {
			break;
		}
		if ((errno != EAGAIN) && (errno != ENETUNREACH) && (errno != ETIMEDOUT)) {
			FAIL("connect()");
		}
		nanosleep(&ts, NULL);
		if (++n > 2400) { /* 120 secs - any used up in gethostbyname */
			FAIL("timeout in connect()");
		}
	}

	/* First, request the OpenSSH pubkey */
	if (asprintf(&request, "GET %s HTTP/1.1\r\nhost: %s\r\nConnection: keep-alive\r\n\r\n",
			config[conf].request_sshkey_path, config[conf].ip) < 0) {
		FAIL("asprintf");
	}

	size_t len = strlen(request);

	if (write(sockfd, request, len) < (ssize_t)len) {
		close(sockfd);
		FAIL("write()");
	}

	FILE *f = fdopen(sockfd, "r");
	if (!f) {
		close(sockfd);
		FAIL("fdopen()");
	}

	size_t cl;
	int result = parse_headers(f, &cl);
	if (result != 1) {
		fclose(f);
		FAIL("parse_headers()");
	}

	int out;
	(void) mkdir(USER_DATA_PATH, 0);
	if (asprintf(&outpath, "%s/%s-user-data", USER_DATA_PATH, config[conf].name) < 0) {
		fclose(f);
		FAIL("asprintf()");
	}
	/* Special case for testing -- can't use/don't need privileged directory */
	if (0 == strcmp(config[conf].name, "test")) {
		if (asprintf(&outpath, "%s-user-data", config[conf].name) < 0) {
			fclose(f);
			FAIL("asprintf()");
		}
	}
	(void) unlink(outpath);
	out = open(outpath, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (out < 0) {
		fclose(f);
		FAIL("open()");
	}

	/* Insert cloud-config header above SSH key. */
	len = strlen(config[conf].cloud_config_header);
	if (write(out, config[conf].cloud_config_header, len) < (ssize_t)len) {
		close(out);
		fclose(f);
		unlink(outpath);
		FAIL("write()");
	}

	/* Write out SSH keys */
	if (write_lines(out, f, cl, "  - ") != 0) {
		close(out);
		fclose(f);
		unlink(outpath);
		FAIL("write_lines()");
	}

	close(sockfd);

	/* reopen socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		FAIL("socket()");
	}

	n = 0;
	for (;;) {
		int r = connect(sockfd, (struct sockaddr *)&server, sizeof(server));
		if (r == 0) {
			break;
		}
		if ((errno != EAGAIN) && (errno != ENETUNREACH) && (errno != ETIMEDOUT)) {
			FAIL("connect()");
		}
		nanosleep(&ts, NULL);
		if (++n > 200) { /* 10 secs */
			FAIL("timeout in connect()");
		}
	}

	/* next, get hostname */
	if (!config[conf].request_hostname_path)
		goto user_data;

	if (asprintf(&request2, "GET %s HTTP/1.1\r\nhost: %s \r\nConnection: keep-alive\r\n\r\n",
				config[conf].request_hostname_path, config[conf].ip) < 0) {
		FAIL("asprintf");
	}
	len = strlen(request2);

	if (write(sockfd, request2, len) < (ssize_t)len) {
		close(sockfd);
		FAIL("write()");
	}

	f = fdopen(sockfd, "r");
	if (!f) {
		close(sockfd);
		FAIL("fdopen()");
	}

	/* parse/discard the header and body */
	result = parse_headers(f, &cl);
	if (result == 0) {
		/* error - exit */
		fclose(f);
		close(out);
		FAIL("parse_headers()");
	}

	/* don't write part #2 if 404 or some non-error */
	if ((result != 2) && (write_lines(out, f, cl, "hostname: ") != 0)) {
		close(out);
		fclose(f);
		unlink(outpath);
		FAIL("write_lines()");
	}

	/* cleanup */
	fclose(f);

	/* reopen socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		FAIL("socket()");
	}

	n = 0;
	for (;;) {
		int r = connect(sockfd, (struct sockaddr *)&server, sizeof(server));
		if (r == 0) {
			break;
		}
		if ((errno != EAGAIN) && (errno != ENETUNREACH) && (errno != ETIMEDOUT)) {
			FAIL("connect()");
		}
		nanosleep(&ts, NULL);
		if (++n > 200) { /* 10 secs */
			FAIL("timeout in connect()");
		}
	}

user_data:
	/* next, get user-data */
	if (!config[conf].request_userdata_path)
		goto finish;

	if (asprintf(&request3, "GET %s HTTP/1.1\r\nhost: %s \r\nConnection: keep-alive\r\n\r\n",
			config[conf].request_userdata_path, config[conf].ip) < 0) {
		FAIL("asprintf");
	}
	len = strlen(request3);

	if (write(sockfd, request3, len) < (ssize_t)len) {
		close(sockfd);
		FAIL("write()");
	}

	f = fdopen(sockfd, "r");
	if (!f) {
		close(sockfd);
		FAIL("fdopen()");
	}

	/* parse/discard the header and body */
	result = parse_headers(f, &cl);
	if (result == 0) {
		/* error - exit */
		fclose(f);
		close(out);
		FAIL("parse_headers()");
	}

	/* don't write part #2 if 404 or some non-error */
	if ((result != 2) && (write_lines(out, f, cl, NULL) != 0)) {
		close(out);
		fclose(f);
		unlink(outpath);
		FAIL("write_lines()");
	}

	/* cleanup */
	close(out);
	fclose(f);

finish:

	/* Don't run ucd for the test template */
	if (strcmp(config[conf].name, "test") != 0) {
		(void) execl(BINDIR "/ucd", BINDIR "/ucd", "-u", outpath, (char *)NULL);
		FAIL("exec()");
	}

	return 0;
}
