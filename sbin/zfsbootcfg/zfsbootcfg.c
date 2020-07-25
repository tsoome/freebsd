/*-
 * Copyright (c) 2016 Andriy Gapon <avg@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <kenv.h>
#include <unistd.h>

#include <libzfsbootenv.h>

#ifndef ZFS_MAXNAMELEN
#define	ZFS_MAXNAMELEN	256
#endif

int
main(int argc, char * const *argv)
{
	char buf[ZFS_MAXNAMELEN], *name;
	const char *key, *value, *type;
	int rv;
	bool print;

	name = NULL;
	key = NULL;
	type = NULL;
	value = NULL;
	print = false;
	while ((rv = getopt(argc, argv, "k:pt:v:z:")) != -1) {
		switch (rv) {
		case 'k':
			key = optarg;
			break;
		case 'p':
			print = true;
			break;
		case 't':
			type = optarg;
			break;
		case 'v':
			value = optarg;
			break;
		case 'z':
			name = optarg;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 1)
		value = argv[0];

	if (argc > 1) {
		fprintf(stderr, "usage: zfsbootcfg <boot.config(5) options>\n");
		return (1);
	}

	if (name == NULL) {
		rv = kenv(KENV_GET, "vfs.root.mountfrom", buf, sizeof(buf));
		if (rv <= 0) {
			perror("can't get vfs.root.mountfrom");
			return (1);
		}

		if (strncmp(buf, "zfs:", 4) == 0) {
			name = strchr(buf + 4, '/');
			if (name != NULL)
				*name = '\0';
			name = buf + 4;
		} else {
			perror("not a zfs root");
			return (1);
		}
	}

	rv = 0;
	if (key != NULL || value != NULL) {
		if (type == NULL)
			type = "DATA_TYPE_STRING";

		if (key == NULL || strcmp(key, "command") == 0)
			rv = lzbe_set_boot_device(name, value);
		else
			rv = lzbe_set_pair(name, key, type, value);

		if (rv == 0)
			printf("zfs bootenv is successfully written\n");
		else
			printf("error: %d\n", rv);
	} else if (!print) {
		char *ptr;

		if (lzbe_get_boot_device(name, &ptr) == 0) {
			printf("zfs:%s:\n", ptr);
			free(ptr);
		}
	}

	if (print) {
		rv = lzbe_bootenv_print(name, stdout);
	}

	return (rv);
}
