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

#include <stand.h>
#include <sys/endian.h>
#include <sys/stdint.h>
#include <zfsimpl.h>
#include "libzfs.h"

enum xdr_op {
	XDR_OP_ENCODE = 1,
	XDR_OP_DECODE = 2
};

typedef struct xdr {
	enum xdr_op xdr_op;
	int (*xdr_getint)(const void *, int *);
	int (*xdr_putint)(void *, int);
	int (*xdr_getuint)(const void *, unsigned *);
	int (*xdr_putuint)(void *, unsigned);
} xdr_t;

static int nvlist_xdr_nvlist(const xdr_t *, nvlist_t *);
static int nvlist_size(const xdr_t *, const uint8_t *);
static int xdr_int(const xdr_t *, void *, int *);
static int xdr_u_int(const xdr_t *, void *, unsigned *);

/* Basic primitives for XDR translation operations, getint and putint. */
static int
_getint(const void *buf, int *ip)
{
	*ip = be32dec(buf);
	return (sizeof(int));
}

static int
_putint(void *buf, int i)
{
	int *ip = buf;

	*ip = htobe32(i);
	return (sizeof(int));
}

static int
_getuint(const void *buf, unsigned *ip)
{
	*ip = be32dec(buf);
	return (sizeof(unsigned));
}

static int
_putuint(void *buf, unsigned i)
{
	unsigned *up = buf;

	*up = htobe32(i);
	return (sizeof(int));
}

/*
 * read native data without translation.
 */
static int
mem_int(const void *buf, int *i)
{
	*i = *(int *)buf;
	return (sizeof(int));
}

static int
mem_uint(const void *buf, unsigned *u)
{
	*u = *(int *)buf;
	return (sizeof(int));
}

/*
 * XDR data translations.
 */
static int
xdr_short(const xdr_t *xdr, uint8_t *buf, short *ip)
{
	int i, rv = 0;

	i = *ip;
	rv = xdr_int(xdr, buf, &i);
	if (xdr->xdr_op == XDR_OP_DECODE) {
		*ip = i;
	}
	return (rv);
}

static int
xdr_u_short(const xdr_t *xdr, uint8_t *buf, unsigned short *ip)
{
	unsigned u;
	int rv;

	u = *ip;
	rv = xdr_u_int(xdr, buf, &u);
	if (xdr->xdr_op == XDR_OP_DECODE) {
		*ip = u;
	}
	return (rv);
}

static int
xdr_int(const xdr_t *xdr, void *buf, int *ip)
{
	int rv = 0;
	int *i = buf;

	switch (xdr->xdr_op) {
	case XDR_OP_ENCODE:
		/* Encode value *ip, store to buf */
		rv = xdr->xdr_putint(buf, *ip);
		break;

	case XDR_OP_DECODE:
		/* Decode buf, return value to *ip */
		rv = xdr->xdr_getint(buf, i);
		*ip = *i;
		break;
	}
	return (rv);
}

static int
xdr_u_int(const xdr_t *xdr, void *buf, unsigned *ip)
{
	int rv = 0;
	unsigned *u = buf;

	switch (xdr->xdr_op) {
	case XDR_OP_ENCODE:
		/* Encode value *ip, store to buf */
		rv = xdr->xdr_putuint(buf, *ip);
		break;

	case XDR_OP_DECODE:
		/* Decode buf, return value to *ip */
		rv = xdr->xdr_getuint(buf, u);
		*ip = *u;
		break;
	}
	return (rv);
}

static int
xdr_string(const xdr_t *xdr, const void *buf, nv_string_t *s)
{
	int size = 0;

	switch (xdr->xdr_op) {
	case XDR_OP_ENCODE:
		size = s->nv_size;
		size += xdr->xdr_putuint(&s->nv_size, s->nv_size);
		size = NV_ALIGN4(size);
		break;

	case XDR_OP_DECODE:
		size = xdr->xdr_getuint(buf, &s->nv_size);
		size = NV_ALIGN4(size + s->nv_size);
		break;
	}
	return (size);
}

static int
xdr_int64(const xdr_t *xdr, uint8_t *buf, int64_t *lp)
{
	int hi, rv = 0;
	unsigned lo;

	switch (xdr->xdr_op) {
	case XDR_OP_ENCODE:
		/* Encode value *lp, store to buf */
		hi = *lp >> 32;
		lo = *lp & UINT32_MAX;
		rv = xdr->xdr_putint(buf, hi);
		rv += xdr->xdr_putint(buf + rv, lo);
		break;

	case XDR_OP_DECODE:
		/* Decode buf, return value to *ip */
		rv = xdr->xdr_getint(buf, &hi);
		rv += xdr->xdr_getuint(buf + rv, &lo);
		*lp = (((int64_t)hi) << 32) | lo;
	}
	return (rv);
}

static int
xdr_uint64(const xdr_t *xdr, uint8_t *buf, uint64_t *lp)
{
	unsigned hi, lo;
	int rv = 0;

	switch (xdr->xdr_op) {
	case XDR_OP_ENCODE:
		/* Encode value *ip, store to buf */
		hi = *lp >> 32;
		lo = *lp & UINT32_MAX;
		rv = xdr->xdr_putint(buf, hi);
		rv += xdr->xdr_putint(buf + rv, lo);
		break;

	case XDR_OP_DECODE:
		/* Decode buf, return value to *ip */
		rv = xdr->xdr_getuint(buf, &hi);
		rv += xdr->xdr_getuint(buf + rv, &lo);
		*lp = (((uint64_t)hi) << 32) | lo;
	}
	return (rv);
}

static int
xdr_char(const xdr_t *xdr, uint8_t *buf, char *cp)
{
	int i, rv = 0;

	i = *cp;
	rv = xdr_int(xdr, buf, &i);
	if (xdr->xdr_op == XDR_OP_DECODE) {
		*cp = i;
	}
	return (rv);
}

/*
 * nvlist management functions.
 */
void
nvlist_destroy(nvlist_t *nvl)
{
	if (nvl != NULL) {
		/* Free data if it was allocated by us. */
		if (nvl->nv_asize > 0)
			free(nvl->nv_data);
	}
	free(nvl);
}

char *
nvstring_get(nv_string_t *nvs)
{
	char *s;

	s = malloc(nvs->nv_size + 1);
	if (s != NULL) {
		bcopy(nvs->nv_data, s, nvs->nv_size);
		s[nvs->nv_size] = '\0';
	}
	return (s);
}

/*
 * Create empty nvlist.
 * The nvlist is terminated by 2x zeros (8 bytes).
 */
nvlist_t *
nvlist_create(int flag)
{
	nvlist_t *nvl;
	nvs_data_t *nvs;

	nvl = calloc(1, sizeof(*nvl));
	if (nvl == NULL)
		return (nvl);

	nvl->nv_header.nvh_encoding = NV_ENCODE_XDR;
	nvl->nv_header.nvh_endian = _BYTE_ORDER == _LITTLE_ENDIAN;

	nvl->nv_asize = nvl->nv_size = sizeof(*nvs);
	nvs = calloc(1, nvl->nv_asize);
	if (nvs == NULL) {
		free(nvl);
		return (NULL);
	}
	/* data in nvlist is byte stream */
	nvl->nv_data = (uint8_t *)nvs;

	nvs->nvl_version = NV_VERSION;
	nvs->nvl_nvflag = flag;
	return (nvl);
}

static int
nvlist_xdr_nvp(const xdr_t *xdr, nvlist_t *nvl)
{
	nv_string_t *nv_string;
	nv_pair_data_t *nvp_data;
	nvlist_t nvlist;
	unsigned type, nelem;
	xdr_t xdrmem = {
	    .xdr_op = XDR_OP_DECODE,
	    .xdr_getint = mem_int,
	    .xdr_getuint = mem_uint
	};

	nv_string = (nv_string_t *)nvl->nv_idx;
	nvl->nv_idx += xdr_string(xdr, &nv_string->nv_size, nv_string);
	nvp_data = (nv_pair_data_t *)nvl->nv_idx;

	type = nvp_data->nv_type;
	nelem = nvp_data->nv_nelem;
	nvl->nv_idx += xdr_u_int(xdr, nvl->nv_idx, &type);
	nvl->nv_idx += xdr_u_int(xdr, nvl->nv_idx, &nelem);

	switch (type) {
	case DATA_TYPE_NVLIST:
	case DATA_TYPE_NVLIST_ARRAY:
		bzero(&nvlist, sizeof(nvlist));
		nvlist.nv_data = &nvp_data->nv_data[0];
		nvlist.nv_idx = nvlist.nv_data;
		for (unsigned i = 0; i < nelem; i++) {
			if (xdr->xdr_op == XDR_OP_ENCODE)
				nvlist.nv_asize =
				    nvlist_size(&xdrmem, nvlist.nv_data);
			else
				nvlist.nv_asize =
				    nvlist_size(xdr, nvlist.nv_data);
			nvlist_xdr_nvlist(xdr, &nvlist);
			nvl->nv_idx = nvlist.nv_idx;
			nvlist.nv_data = nvlist.nv_idx;
		}
		break;

	case DATA_TYPE_BOOLEAN:
		/* BOOLEAN does not take value space */
		break;
	case DATA_TYPE_BYTE:
	case DATA_TYPE_INT8:
	case DATA_TYPE_UINT8:
		nvl->nv_idx += xdr_char(xdr, &nvp_data->nv_data[0],
		    (char *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_INT16:
		nvl->nv_idx += xdr_short(xdr, &nvp_data->nv_data[0],
		    (short *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_UINT16:
		nvl->nv_idx += xdr_u_short(xdr, &nvp_data->nv_data[0],
		    (unsigned short *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_BOOLEAN_VALUE:
	case DATA_TYPE_INT32:
		nvl->nv_idx += xdr_int(xdr, &nvp_data->nv_data[0],
		    (int *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_UINT32:
		nvl->nv_idx += xdr_u_int(xdr, &nvp_data->nv_data[0],
		    (unsigned *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_INT64:
		nvl->nv_idx += xdr_int64(xdr, &nvp_data->nv_data[0],
		    (int64_t *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_UINT64:
		nvl->nv_idx += xdr_uint64(xdr, &nvp_data->nv_data[0],
		    (uint64_t *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_STRING:
		nv_string = (nv_string_t *)&nvp_data->nv_data[0];
		nvl->nv_idx += xdr_string(xdr, &nvp_data->nv_data[0],
		    nv_string);

		break;
	}
	return (0);
}

static int
nvlist_xdr_nvlist(const xdr_t *xdr, nvlist_t *nvl)
{
	nvp_header_t *nvph;
	nvs_data_t *nvs;
	unsigned encoded_size, decoded_size;
	int rv;

	nvl->nv_idx = nvl->nv_data;
	nvs = (nvs_data_t *)nvl->nv_data;
	nvph = &nvs->nvl_pair;

	switch (xdr->xdr_op) {
	case XDR_OP_ENCODE:
		nvl->nv_idx += xdr->xdr_putuint(nvl->nv_idx,
		    nvs->nvl_version);
		nvl->nv_idx += xdr->xdr_putuint(nvl->nv_idx,
		    nvs->nvl_nvflag);

		encoded_size = nvph->encoded_size;
		decoded_size = nvph->decoded_size;

		nvl->nv_idx += xdr->xdr_putuint(nvl->nv_idx,
		    encoded_size);
		nvl->nv_idx += xdr->xdr_putuint(nvl->nv_idx,
		    decoded_size);
		break;

	case XDR_OP_DECODE:
		nvl->nv_idx += xdr->xdr_getuint(nvl->nv_idx,
		    &nvs->nvl_version);
		nvl->nv_idx += xdr->xdr_getuint(nvl->nv_idx,
		    &nvs->nvl_nvflag);

		nvl->nv_idx += xdr->xdr_getuint(nvl->nv_idx,
		    &nvph->encoded_size);
		nvl->nv_idx += xdr->xdr_getuint(nvl->nv_idx,
		    &nvph->decoded_size);

		encoded_size = nvph->encoded_size;
		decoded_size = nvph->decoded_size;
		break;

	default:
		return (EINVAL);
	}

	rv = 0;
	while (encoded_size && decoded_size) {
		rv = nvlist_xdr_nvp(xdr, nvl);
		if (rv != 0)
			return (rv);

		nvph = (nvp_header_t *)(nvl->nv_idx);
		switch (xdr->xdr_op) {
		case XDR_OP_ENCODE:
			encoded_size = nvph->encoded_size;
			decoded_size = nvph->decoded_size;

			nvl->nv_idx += xdr->xdr_putuint(nvl->nv_idx,
			    encoded_size);
			nvl->nv_idx += xdr->xdr_putuint(nvl->nv_idx,
			    decoded_size);
			break;

		case XDR_OP_DECODE:
			nvl->nv_idx += xdr->xdr_getuint(&nvph->encoded_size,
			    &nvph->encoded_size);
			nvl->nv_idx += xdr->xdr_getuint(&nvph->decoded_size,
			    &nvph->decoded_size);

			encoded_size = nvph->encoded_size;
			decoded_size = nvph->decoded_size;
			break;
		}
	}
	return (rv);
}

static int
nvlist_size(const xdr_t *xdr, const uint8_t *stream)
{
	const uint8_t *p, *pair;
	unsigned encoded_size, decoded_size;

	p = stream;
	p += 2 * sizeof(unsigned);

	pair = p;
	p += xdr->xdr_getuint(p, &encoded_size);
	p += xdr->xdr_getuint(p, &decoded_size);
	while (encoded_size && decoded_size) {
		p = pair + encoded_size;
		pair = p;
		p += xdr->xdr_getuint(p, &encoded_size);
		p += xdr->xdr_getuint(p, &decoded_size);
	}
	return (p - stream);
}

/*
 * Export nvlist to byte stream format.
 */
int
nvlist_export(nvlist_t *nvl)
{
	int rv;
	xdr_t xdr = {
	    .xdr_op = XDR_OP_ENCODE,
	    .xdr_putint = _putint,
	    .xdr_putuint = _putuint
	};

	if (nvl->nv_header.nvh_encoding != NV_ENCODE_XDR)
		return (ENOTSUP);

	nvl->nv_idx = nvl->nv_data;
	rv = nvlist_xdr_nvlist(&xdr, nvl);

	return (rv);
}

/*
 * Import nvlist from byte stream.
 * Determine the stream size and allocate private copy.
 * Then translate the data.
 */
nvlist_t *
nvlist_import(const char *stream)
{
	nvlist_t *nvl;
	xdr_t xdr = {
	    .xdr_op = XDR_OP_DECODE,
	    .xdr_getint = _getint,
	    .xdr_getuint = _getuint
	};

	/* Check the nvlist head. */
	if (stream[0] != NV_ENCODE_XDR ||
	    (stream[1] != '\0' && stream[1] != '\1') ||
	    stream[2] != '\0' || stream[3] != '\0' ||
	    be32toh(*(uint32_t *)(stream + 4)) != NV_VERSION ||
	    be32toh(*(uint32_t *)(stream + 8)) != NV_UNIQUE_NAME)
		return (NULL);

	nvl = malloc(sizeof(*nvl));
	if (nvl == NULL)
		return (nvl);

	nvl->nv_header.nvh_encoding = stream[0];
	nvl->nv_header.nvh_endian = stream[1];
	nvl->nv_header.nvh_reserved1 = stream[2];
	nvl->nv_header.nvh_reserved2 = stream[3];
	nvl->nv_asize = nvl->nv_size = nvlist_size(&xdr,
	    (const uint8_t *)stream + 4);
	nvl->nv_data = malloc(nvl->nv_asize);
	if (nvl->nv_data == NULL) {
		free(nvl);
		return (NULL);
	}
	nvl->nv_idx = nvl->nv_data;
	bcopy(stream + 4, nvl->nv_data, nvl->nv_asize);

	if (nvlist_xdr_nvlist(&xdr, nvl) == 0) {
		nvl->nv_idx = nvl->nv_data;
	} else {
		free(nvl->nv_data);
		free(nvl);
		nvl = NULL;
	}

	return (nvl);
}

/*
 * remove pair from this nvlist.
 */
int
nvlist_remove(nvlist_t *nvl, const char *name, data_type_t type)
{
	uint8_t *head, *tail;
	nvs_data_t *data;
	nvp_header_t *nvp;
	nv_string_t *nvp_name;
	nv_pair_data_t *nvp_data;
	size_t size;

	if (nvl == NULL || nvl->nv_data == NULL || name == NULL)
		return (EINVAL);

	head = nvl->nv_data;
	data = (nvs_data_t *)head;
	nvp = &data->nvl_pair;	/* first pair in nvlist */
	head = (uint8_t *)nvp;

	while (nvp->encoded_size != 0 && nvp->decoded_size != 0) {
		nvp_name = (nv_string_t *)(head + sizeof(*nvp));

		nvp_data = (nv_pair_data_t *)
		    NV_ALIGN4((uintptr_t)&nvp_name->nv_data[0] +
		    nvp_name->nv_size);

		if (memcmp(nvp_name->nv_data, name, nvp_name->nv_size) == 0 &&
		    nvp_data->nv_type == type) {
			/*
			 * set tail to point to next nvpair and size
			 * is the length of the tail.
			 */
			tail = head + nvp->encoded_size;
			size = nvl->nv_data + nvl->nv_size - tail;

			/* adjust the size of the nvlist. */
			nvl->nv_size -= nvp->encoded_size;
			bcopy(tail, head, size);
			return (0);
		}
		/* Not our pair, skip to next. */
		head = head + nvp->encoded_size;
		nvp = (nvp_header_t *)head;
	}
	return (ENOENT);
}

int
nvlist_find(const nvlist_t *nvl, const char *name, data_type_t type,
    int *elementsp, void *valuep, int *sizep)
{
	nvs_data_t *data;
	nvp_header_t *nvp;
	nv_string_t *nvp_name;
	nv_pair_data_t *nvp_data;
	nvlist_t *nvlist;

	if (nvl == NULL || nvl->nv_data == NULL || name == NULL)
		return (EINVAL);

	data = (nvs_data_t *)nvl->nv_data;
	nvp = &data->nvl_pair;	/* first pair in nvlist */

	while (nvp->encoded_size != 0 && nvp->decoded_size != 0) {
		nvp_name = (nv_string_t *)((uint8_t *)nvp + sizeof(*nvp));
		nvp_data = (nv_pair_data_t *)
		    NV_ALIGN4((uintptr_t)&nvp_name->nv_data[0] +
		    nvp_name->nv_size);

		if (memcmp(nvp_name->nv_data, name, nvp_name->nv_size) == 0 &&
		    nvp_data->nv_type == type) {
			if (elementsp != NULL)
				*elementsp = nvp_data->nv_nelem;
			switch (nvp_data->nv_type) {
			case DATA_TYPE_UINT64:
				*(uint64_t *)valuep =
				    *(uint64_t *)nvp_data->nv_data;
				return (0);
			case DATA_TYPE_STRING:
				nvp_name = (nv_string_t *)nvp_data->nv_data;
				if (sizep != NULL) {
					*sizep = nvp_name->nv_size;
				}
				*(const uint8_t **)valuep =
				    &nvp_name->nv_data[0];
				return (0);
			case DATA_TYPE_NVLIST:
			case DATA_TYPE_NVLIST_ARRAY:
				nvlist = malloc(sizeof(*nvlist));
				if (nvlist != NULL) {
					nvlist->nv_header = nvl->nv_header;
					nvlist->nv_asize = 0;
					nvlist->nv_size = 0;
					nvlist->nv_idx = NULL;
					nvlist->nv_data = &nvp_data->nv_data[0];
					*(nvlist_t **)valuep = nvlist;
					return (0);
				}
				return (ENOMEM);
			}
			return (EIO);
		}
		/* Not our pair, skip to next. */
		nvp = (nvp_header_t *)((uint8_t *)nvp + nvp->encoded_size);
	}
	return (ENOENT);
}

int
nvlist_add_uint64(nvlist_t *nvl, const char *name, uint64_t value)
{
	nvs_data_t *nvs;
	nvp_header_t head, *hp;
	uint8_t *ptr;
	size_t namelen, valuelen;

	nvs = (nvs_data_t *)nvl->nv_data;
	if (nvs->nvl_nvflag & NV_UNIQUE_NAME)
		(void) nvlist_remove(nvl, name, DATA_TYPE_UINT64);

	namelen = strlen(name);
	valuelen = sizeof(value);
	head.encoded_size = 4 + 4 + 4 + NV_ALIGN4(namelen) + 4 + 4 +
	    4 + NV_ALIGN(valuelen + 1);
	head.decoded_size = NV_ALIGN(4 * 4 + namelen + 1) + valuelen;

	if (nvl->nv_asize - nvl->nv_size < head.encoded_size + 8) {
		ptr = realloc(nvl->nv_data, nvl->nv_asize + head.encoded_size);
		if (ptr == NULL)
			return (ENOMEM);
		nvl->nv_data = ptr;
		nvl->nv_asize += head.encoded_size;
	}
	nvl->nv_idx = nvl->nv_data + nvl->nv_size - sizeof(*hp);
	bzero(nvl->nv_idx, head.encoded_size + 8);
	hp = (nvp_header_t *)nvl->nv_idx;
	*hp = head;
	nvl->nv_idx += sizeof(*hp);
	*(unsigned *)nvl->nv_idx = namelen;
	nvl->nv_idx += sizeof(unsigned);
	strlcpy((char *)nvl->nv_idx, name, namelen + 1);
	nvl->nv_idx += NV_ALIGN4(namelen);
	*(unsigned *)nvl->nv_idx = DATA_TYPE_UINT64;
	nvl->nv_idx += sizeof(unsigned);
	*(unsigned *)nvl->nv_idx = 1;
	nvl->nv_idx += sizeof(unsigned);
	*(uint64_t *)nvl->nv_idx = value;
	nvl->nv_size += head.encoded_size;

	return (0);
}

int
nvlist_add_string(nvlist_t *nvl, const char *name, const char *value)
{
	nvs_data_t *nvs;
	nvp_header_t head, *hp;
	uint8_t *ptr;
	size_t namelen, valuelen;

	nvs = (nvs_data_t *)nvl->nv_data;
	if (nvs->nvl_nvflag & NV_UNIQUE_NAME)
		(void) nvlist_remove(nvl, name, DATA_TYPE_STRING);

	namelen = strlen(name);
	valuelen = strlen(value);
	head.encoded_size = 4 + 4 + 4 + NV_ALIGN4(namelen) + 4 + 4 +
	    4 + NV_ALIGN(valuelen + 1);
	head.decoded_size = NV_ALIGN(4 * 4 + namelen + 1) +
	    NV_ALIGN(valuelen + 1);

	if (nvl->nv_asize - nvl->nv_size < head.encoded_size + 8) {
		ptr = realloc(nvl->nv_data, nvl->nv_asize + head.encoded_size);
		if (ptr == NULL)
			return (ENOMEM);
		nvl->nv_data = ptr;
		nvl->nv_asize += head.encoded_size;
	}
	nvl->nv_idx = nvl->nv_data + nvl->nv_size - sizeof(*hp);
	bzero(nvl->nv_idx, head.encoded_size + 8);
	hp = (nvp_header_t *)nvl->nv_idx;
	*hp = head;
	nvl->nv_idx += sizeof(*hp);
	*(unsigned *)nvl->nv_idx = namelen;
	nvl->nv_idx += sizeof(unsigned);
	strlcpy((char *)nvl->nv_idx, name, namelen + 1);
	nvl->nv_idx += NV_ALIGN4(namelen);
	*(unsigned *)nvl->nv_idx = DATA_TYPE_STRING;
	nvl->nv_idx += sizeof(unsigned);
	*(unsigned *)nvl->nv_idx = 1;
	nvl->nv_idx += sizeof(unsigned);
	*(unsigned *)nvl->nv_idx = valuelen;
	nvl->nv_idx += sizeof(unsigned);
	strlcpy((char *)nvl->nv_idx, value, valuelen + 1);
	nvl->nv_idx += NV_ALIGN4(valuelen);
	nvl->nv_size += head.encoded_size;

	return (0);
}

/*
 * Return the next nvlist in an nvlist array.
 */
int
nvlist_next(nvlist_t *nvl)
{
	nvs_data_t *data;
	nvp_header_t *nvp;

	if (nvl == NULL || nvl->nv_data == NULL || nvl->nv_asize != 0)
		return (EINVAL);

	data = (nvs_data_t *)nvl->nv_data;
	nvp = &data->nvl_pair;	/* first pair in nvlist */

	while (nvp->encoded_size != 0 && nvp->decoded_size != 0) {
		nvp = (nvp_header_t *)((uint8_t *)nvp + nvp->encoded_size);
	}
	nvl->nv_data = (uint8_t *)nvp + sizeof(*nvp);
	return (0);
}

void
nvlist_print(nvlist_t *nvl, unsigned int indent)
{
	static const char *typenames[] = {
		"DATA_TYPE_UNKNOWN",
		"DATA_TYPE_BOOLEAN",
		"DATA_TYPE_BYTE",
		"DATA_TYPE_INT16",
		"DATA_TYPE_UINT16",
		"DATA_TYPE_INT32",
		"DATA_TYPE_UINT32",
		"DATA_TYPE_INT64",
		"DATA_TYPE_UINT64",
		"DATA_TYPE_STRING",
		"DATA_TYPE_BYTE_ARRAY",
		"DATA_TYPE_INT16_ARRAY",
		"DATA_TYPE_UINT16_ARRAY",
		"DATA_TYPE_INT32_ARRAY",
		"DATA_TYPE_UINT32_ARRAY",
		"DATA_TYPE_INT64_ARRAY",
		"DATA_TYPE_UINT64_ARRAY",
		"DATA_TYPE_STRING_ARRAY",
		"DATA_TYPE_HRTIME",
		"DATA_TYPE_NVLIST",
		"DATA_TYPE_NVLIST_ARRAY",
		"DATA_TYPE_BOOLEAN_VALUE",
		"DATA_TYPE_INT8",
		"DATA_TYPE_UINT8",
		"DATA_TYPE_BOOLEAN_ARRAY",
		"DATA_TYPE_INT8_ARRAY",
		"DATA_TYPE_UINT8_ARRAY"
	};
	nvs_data_t *data;
	nvp_header_t *nvp;
	nv_string_t *nvp_name;
	nv_pair_data_t *nvp_data;
	nvlist_t nvlist;
	unsigned i, j;
	xdr_t xdr = {
	    .xdr_op = XDR_OP_DECODE,
	    .xdr_getint = mem_int,
	    .xdr_getuint = mem_uint
	};

	data = (nvs_data_t *)nvl->nv_data;
	nvp = &data->nvl_pair;  /* first pair in nvlist */
	while (nvp->encoded_size != 0 && nvp->decoded_size != 0) {
		nvp_name = (nv_string_t *)((uintptr_t)nvp + sizeof(*nvp));
		nvp_data = (nv_pair_data_t *)
		    NV_ALIGN4((uintptr_t)&nvp_name->nv_data[0] +
		    nvp_name->nv_size);

		for (i = 0; i < indent; i++)
			printf(" ");

		printf("%s [%d] %.*s", typenames[nvp_data->nv_type],
		    nvp_data->nv_nelem, nvp_name->nv_size, nvp_name->nv_data);

		switch (nvp_data->nv_type) {
		case DATA_TYPE_UINT64: {
			uint64_t val;

			val = *(uint64_t *)nvp_data->nv_data;
			printf(" = 0x%jx\n", (uintmax_t)val);
			break;
		}

		case DATA_TYPE_STRING: {
			nvp_name = (nv_string_t *)&nvp_data->nv_data[0];
			printf(" = \"%.*s\"\n", nvp_name->nv_size,
			    nvp_name->nv_data);
			break;
		}

		case DATA_TYPE_NVLIST:
			printf("\n");
			nvlist.nv_data = &nvp_data->nv_data[0];
			nvlist_print(&nvlist, indent + 2);
			break;

		case DATA_TYPE_NVLIST_ARRAY:
			nvlist.nv_data = &nvp_data->nv_data[0];
			for (j = 0; j < nvp_data->nv_nelem; j++) {
				data = (nvs_data_t *)nvlist.nv_data;
				printf("[%d]\n", j);
				nvlist_print(&nvlist, indent + 2);
				if (j != nvp_data->nv_nelem - 1) {
					for (i = 0; i < indent; i++)
						printf(" ");
					printf("%s %.*s",
					    typenames[nvp_data->nv_type],
					    nvp_name->nv_size,
					    nvp_name->nv_data);
				}
				nvlist.nv_data = (uint8_t *)data +
				    nvlist_size(&xdr, nvlist.nv_data);
			}
			break;

		default:
			printf("\n");
		}
		nvp = (nvp_header_t *)((uint8_t *)nvp + nvp->encoded_size);
	}
	printf("%*s\n", indent + 13, "End of nvlist");
}
