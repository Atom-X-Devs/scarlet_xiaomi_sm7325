// SPDX-License-Identifier: GPL-2.0-or-later

#define pr_fmt(fmt) "zram: deflate: " fmt

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/zlib.h>

#include "backend_deflate.h"

/* Use the same value as crypto API */
#define DEFLATE_DEF_WINBITS		11
#define DEFLATE_DEF_MEMLEVEL		MAX_MEM_LEVEL

static void deflate_release_params(struct zcomp_params *params)
{
}

static int deflate_setup_params(struct zcomp_params *params)
{
	if (params->level == ZCOMP_PARAM_NO_LEVEL)
		params->level = Z_DEFAULT_COMPRESSION;

	return 0;
}

static void deflate_destroy_cctx(struct zcomp_ctx *ctx)
{
	struct z_stream_s *cctx = ctx->context;

	if (!cctx)
		return;

	zlib_deflateEnd(cctx);
	vfree(cctx->workspace);
	kfree(cctx);
}

static int deflate_create_cctx(struct zcomp_params *params,
			       struct zcomp_ctx *ctx)
{
	struct z_stream_s *cctx;
	size_t size;
	int ret = -ENOMEM;

	cctx = kzalloc(sizeof(*cctx), GFP_KERNEL);
	if (!cctx)
		return -ENOMEM;

	size = zlib_deflate_workspacesize(-DEFLATE_DEF_WINBITS, DEFLATE_DEF_MEMLEVEL);
	cctx->workspace = vzalloc(size);
	if (!cctx->workspace)
		goto free_ctx;

	ret = zlib_deflateInit2(cctx, params->level, Z_DEFLATED,
			       -DEFLATE_DEF_WINBITS, DEFLATE_DEF_MEMLEVEL,
			       Z_DEFAULT_STRATEGY);
	if (ret != Z_OK) {
		ret = -EINVAL;
		goto free_workspace;
	}

	ctx->context = cctx;
	return 0;

free_workspace:
	vfree(cctx->workspace);
free_ctx:
	kfree(cctx);
	return ret;
}

static void deflate_destroy_dctx(struct zcomp_ctx *ctx)
{
	struct z_stream_s *dctx = ctx->context;

	if (!dctx)
		return;

	zlib_inflateEnd(dctx);
	vfree(dctx->workspace);
	kfree(dctx);
}

static int deflate_create_dctx(struct zcomp_params *params,
			       struct zcomp_ctx *ctx)
{
	struct z_stream_s *dctx;
	int ret = -ENOMEM;

	dctx = kzalloc(sizeof(*dctx), GFP_KERNEL);
	if (!dctx)
		return -ENOMEM;

	dctx->workspace = vzalloc(zlib_inflate_workspacesize());
	if (!dctx->workspace)
		goto free_ctx;

	ret = zlib_inflateInit2(dctx, -DEFLATE_DEF_WINBITS);
	if (ret != Z_OK) {
		ret = -EINVAL;
		goto free_workspace;
	}

	ctx->context = dctx;
	return 0;

free_workspace:
	vfree(dctx->workspace);
free_ctx:
	kfree(dctx);
	return ret;
}

static int deflate_compress(struct zcomp_params *params, struct zcomp_ctx *ctx,
			    struct zcomp_req *req)
{
	struct z_stream_s *deflate = ctx->context;
	int ret;

	ret = zlib_deflateReset(deflate);
	if (ret != Z_OK)
		return -EINVAL;

	deflate->next_in = (u8 *)req->src;
	deflate->avail_in = req->src_len;
	deflate->next_out = (u8 *)req->dst;
	deflate->avail_out = req->dst_len;

	ret = zlib_deflate(deflate, Z_FINISH);
	if (ret != Z_STREAM_END)
		return -EINVAL;

	req->dst_len = deflate->total_out;
	return 0;
}

static int deflate_decompress(struct zcomp_params *params,
			      struct zcomp_ctx *ctx,
			      struct zcomp_req *req)
{
	struct z_stream_s *inflate = ctx->context;
	int ret;

	ret = zlib_inflateReset(inflate);
	if (ret != Z_OK)
		return -EINVAL;

	inflate->next_in = (u8 *)req->src;
	inflate->avail_in = req->src_len;
	inflate->next_out = (u8 *)req->dst;
	inflate->avail_out = req->dst_len;

	ret = zlib_inflate(inflate, Z_SYNC_FLUSH);
	if (ret != Z_STREAM_END)
		return -EINVAL;

	return 0;
}

const struct zcomp_ops backend_deflate = {
	.compress	= deflate_compress,
	.decompress	= deflate_decompress,
	.create_cctx	= deflate_create_cctx,
	.destroy_cctx	= deflate_destroy_cctx,
	.create_dctx	= deflate_create_dctx,
	.destroy_dctx	= deflate_destroy_dctx,
	.setup_params	= deflate_setup_params,
	.release_params	= deflate_release_params,
	.name		= "deflate",
};
