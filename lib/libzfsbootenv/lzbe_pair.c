/*-
 * Copyright 2020 Toomas Soome <tsoome@me.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <string.h>
#include <libzfs.h>
#include <libzfsbootenv.h>

/*
 * Store pair defined by key, type and value.
 */
int
lzbe_set_pair(const char *pool, const char *key, const char *type,
    const char *value)
{
	libzfs_handle_t *hdl;
	zpool_handle_t *zphdl;
	nvlist_t *nv;
	int rv = -1;

	if (pool == NULL || *pool == '\0' || type == NULL)
		return (rv);

	if ((hdl = libzfs_init()) == NULL) {
		return (rv);
	}

	zphdl = zpool_open(hdl, pool);
	if (zphdl == NULL) {
		libzfs_fini(hdl);
		return (rv);
	}

	rv = zpool_get_bootenv(zphdl, &nv);
	if (rv == 0) {
		if (strcmp(type, "DATA_TYPE_STRING") == 0) {
			if ((value == NULL || *value == '\0') &&
			    nvlist_exists(nv, key)) {
				fnvlist_remove(nv, key);
			} else {
				fnvlist_add_string(nv, key, value);
			}
		}
	}
	rv = zpool_set_bootenv(zphdl, nv);

	nvlist_free(nv);
	zpool_close(zphdl);
	libzfs_fini(hdl);
	return (rv);
}
