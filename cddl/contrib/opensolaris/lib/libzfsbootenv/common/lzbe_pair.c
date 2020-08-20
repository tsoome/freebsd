/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */
/*
 * Copyright 2020 Toomas Soome <tsoome@me.com>
 */

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
		rv = zpool_set_bootenv(zphdl, nv);
		nvlist_free(nv);
	}

	zpool_close(zphdl);
	libzfs_fini(hdl);
	return (rv);
}
