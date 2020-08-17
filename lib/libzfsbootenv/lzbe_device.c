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
#include <sys/zfs_bootenv.h>
#include <sys/vdev_impl.h>

/*
 * Store device name to zpool label bootenv area.
 */
int
lzbe_set_boot_device(const char *pool, const char *device)
{
	libzfs_handle_t *hdl;
	zpool_handle_t *zphdl;
	nvlist_t *nv;
	char *descriptor;
	int rv = -1;

	if (pool == NULL || *pool == '\0')
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
	if (rv != 0)
		nv = fnvlist_alloc();

	/* version is mandatory */
	if (!nvlist_exists(nv, BOOTENV_VERSION))
		fnvlist_add_uint64(nv, BOOTENV_VERSION, VB_NVLIST);

	/*
	 * If device name is empty, remove boot device configuration.
	 */
	if (device == NULL || *device == '\0') {
		if (nvlist_exists(nv, OS_BOOTONCE))
			fnvlist_remove(nv, OS_BOOTONCE);
	} else {
		/*
		 * Use device name directly if it does start with
		 * prefix "zfs:". Otherwise, add prefix and sufix.
		 */
		if (strncmp(device, "zfs:", 4) == 0) {
			fnvlist_add_string(nv, OS_BOOTONCE, device);
		} else {
			descriptor = NULL;
			if (asprintf(&descriptor, "zfs:%s:", device) > 0)
				fnvlist_add_string(nv, OS_BOOTONCE, descriptor);
			else
				rv = ENOMEM;
			free(descriptor);
		}
	}

	rv = zpool_set_bootenv(zphdl, nv);
	if (rv != 0)
		fprintf(stderr, "%s\n", libzfs_error_description(hdl));

	fnvlist_free(nv);
	zpool_close(zphdl);
	libzfs_fini(hdl);
	return (rv);
}

/*
 * Return boot device name from bootenv, if set.
 */
int
lzbe_get_boot_device(const char *pool, char **device)
{
	libzfs_handle_t *hdl;
	zpool_handle_t *zphdl;
	nvlist_t *nv;
	char *val;
	int rv = -1;

	if (pool == NULL || *pool == '\0' || device == NULL)
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
		rv = nvlist_lookup_string(nv, OS_BOOTONCE, &val);
		if (rv == 0) {
			/*
			 * zfs device descriptor is in form of "zfs:dataset:",
			 * we only do need dataset name.
			 */
			if (strncmp(val, "zfs:", 4) == 0)
				val += 4;
			val = strdup(val);
			if (val != NULL) {
				size_t len = strlen(val);

				if (val[len - 1] == ':')
					val[len - 1] = '\0';
				*device = val;
			} else {
				rv = ENOMEM;
			}
		}
		nvlist_free(nv);
	}

	zpool_close(zphdl);
	libzfs_fini(hdl);
	return (rv);
}
