/* SPDX-License-Identifier: GPL-2.0 */
/*
 * zpool memory storage api
 *
 * Copyright (C) 2014 Dan Streetman
 *
 * This is a common frontend for the zbud and zsmalloc memory
 * storage pool implementations.  Typically, this is used to
 * store compressed memory.
 */

#ifndef _ZPOOL_H_
#define _ZPOOL_H_

struct zpool;
struct scatterlist;
struct zs_handle;

struct zpool_ops {
	int (*evict)(struct zpool *pool, unsigned long handle);
};

/*
 * Control how a handle is mapped.  It will be ignored if the
 * implementation does not support it.  Its use is optional.
 * Note that this does not refer to memory protection, it
 * refers to how the memory will be copied in/out if copying
 * is necessary during mapping; read-write is the safest as
 * it copies the existing memory in on map, and copies the
 * changed memory back out on unmap.  Write-only does not copy
 * in the memory and should only be used for initialization.
 * If in doubt, use ZPOOL_MM_DEFAULT which is read-write.
 */
enum zpool_mapmode {
	ZPOOL_MM_RW, /* normal read-write mapping */
	ZPOOL_MM_RO, /* read-only (no copy-out at unmap time) */
	ZPOOL_MM_WO, /* write-only (no copy-in at map time) */

	ZPOOL_MM_DEFAULT = ZPOOL_MM_RW
};

bool zpool_has_pool(char *type);

struct zpool *zpool_create_pool(const char *type, const char *name,
			gfp_t gfp, const struct zpool_ops *ops);

const char *zpool_get_type(struct zpool *pool);

void zpool_destroy_pool(struct zpool *pool);

int zpool_malloc(struct zpool *pool, size_t size, gfp_t gfp,
			unsigned long *handle);

void zpool_free(struct zpool *pool, unsigned long handle);

int zpool_shrink(struct zpool *pool, unsigned int pages,
			unsigned int *reclaimed);

void *zpool_map_handle(struct zpool *pool, unsigned long handle,
		       enum zpool_mapmode mapmode);
void zpool_unmap_handle(struct zpool *pool, unsigned long handle);

bool zpool_uses_zs_handles(struct zpool *pool);
int zpool_zs_store(struct zpool *pool, struct zs_handle *handle,
		   const void *src, size_t size, gfp_t gfp);
void zpool_zs_free(struct zpool *pool, struct zs_handle *handle);
void zpool_zs_read_sg_begin(struct zpool *pool, const struct zs_handle *handle,
			    struct scatterlist *sg, size_t size);
void zpool_zs_read_sg_end(struct zpool *pool, const struct zs_handle *handle);

u64 zpool_get_total_size(struct zpool *pool);


/**
 * struct zpool_driver - driver implementation for zpool
 * @type:	name of the driver.
 * @list:	entry in the list of zpool drivers.
 * @create:	create a new pool.
 * @destroy:	destroy a pool.
 * @malloc:	allocate mem from a pool.
 * @free:	free mem from a pool.
 * @shrink:	shrink the pool.
 * @map:	map a handle.
 * @unmap:	unmap a handle.
 * @zs_store:	allocate and fill an object owned by a caller-provided zs_handle.
 * @zs_free:	free an object owned by a zs_handle.
 * @zs_read_sg_begin: pin an object owned by a zs_handle and fill a scatterlist.
 * @zs_read_sg_end: unpin an object after reading its scatterlist.
 * @total_size:	get total size of a pool.
 *
 * This is created by a zpool implementation and registered
 * with zpool. A driver using zs_handle provides all four zs_* callbacks
 * instead of the malloc, free, map and unmap callbacks.
 */
struct zpool_driver {
	char *type;
	struct module *owner;
	atomic_t refcount;
	struct list_head list;

	void *(*create)(const char *name,
			gfp_t gfp,
			const struct zpool_ops *ops,
			struct zpool *zpool);
	void (*destroy)(void *pool);

	int (*malloc)(void *pool, size_t size, gfp_t gfp,
				unsigned long *handle);
	void (*free)(void *pool, unsigned long handle);

	int (*shrink)(void *pool, unsigned int pages,
				unsigned int *reclaimed);

	void *(*map)(void *pool, unsigned long handle, enum zpool_mapmode mm);
	void (*unmap)(void *pool, unsigned long handle);

	int (*zs_store)(void *pool, struct zs_handle *handle, const void *src,
			size_t size, gfp_t gfp);
	void (*zs_free)(void *pool, struct zs_handle *handle);
	void (*zs_read_sg_begin)(void *pool, const struct zs_handle *handle,
				 struct scatterlist *sg, size_t size);
	void (*zs_read_sg_end)(void *pool, const struct zs_handle *handle);

	u64 (*total_size)(void *pool);
};

void zpool_register_driver(struct zpool_driver *driver);

int zpool_unregister_driver(struct zpool_driver *driver);

bool zpool_evictable(struct zpool *pool);

#endif
