/*
 *  Copyright (c) 2016 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Krzysztof Jackiewicz <k.jackiewicz@samsung.com>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License
 */

/**
 * @file digest.c
 * @brief
 */

#include <assert.h>

#include <openssl/evp.h>

#include <yaca_crypto.h>
#include <yaca_digest.h>
#include <yaca_error.h>

#include "internal.h"

struct yaca_digest_ctx_s {
	struct yaca_context_s ctx;

	EVP_MD_CTX *mdctx;
};

static struct yaca_digest_ctx_s *get_digest_ctx(const yaca_context_h ctx)
{
	if (ctx == YACA_CONTEXT_NULL)
		return NULL;

	switch (ctx->type) {
	case YACA_CTX_DIGEST:
		return (struct yaca_digest_ctx_s *)ctx;
	default:
		return NULL;
	}
}

static int get_digest_output_length(const yaca_context_h ctx, size_t input_len, size_t *output_len)
{
	struct yaca_digest_ctx_s *c = get_digest_ctx(ctx);

	if (c == NULL)
		return YACA_ERROR_INVALID_PARAMETER;

	int md_size = EVP_MD_CTX_size(c->mdctx);
	if (md_size <= 0)
		return YACA_ERROR_INTERNAL;

	*output_len = md_size;

	return YACA_ERROR_NONE;
}

static void destroy_digest_context(yaca_context_h ctx)
{
	struct yaca_digest_ctx_s *c = get_digest_ctx(ctx);

	if (c == NULL)
		return;

	EVP_MD_CTX_destroy(c->mdctx);
	c->mdctx = NULL;
}

int digest_get_algorithm(yaca_digest_algorithm_e algo, const EVP_MD **md)
{
	int ret = YACA_ERROR_NONE;

	if (!md)
		return YACA_ERROR_INVALID_PARAMETER;

	*md = NULL;

	switch (algo) {
	case YACA_DIGEST_MD5:
		*md = EVP_md5();
		break;
	case YACA_DIGEST_SHA1:
		*md = EVP_sha1();
		break;
	case YACA_DIGEST_SHA224:
		*md = EVP_sha224();
		break;
	case YACA_DIGEST_SHA256:
		*md = EVP_sha256();
		break;
	case YACA_DIGEST_SHA384:
		*md = EVP_sha384();
		break;
	case YACA_DIGEST_SHA512:
		*md = EVP_sha512();
		break;
	default:
		ret = YACA_ERROR_INVALID_PARAMETER;
		break;
	}

	if (ret == YACA_ERROR_NONE && *md == NULL) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
	}

	return ret;
}

API int yaca_digest_initialize(yaca_context_h *ctx, yaca_digest_algorithm_e algo)
{
	int ret;
	struct yaca_digest_ctx_s *nc = NULL;
	const EVP_MD *md;

	if (ctx == NULL)
		return YACA_ERROR_INVALID_PARAMETER;

	ret = yaca_zalloc(sizeof(struct yaca_digest_ctx_s), (void**)&nc);
	if (ret != YACA_ERROR_NONE)
		return ret;

	nc->ctx.type = YACA_CTX_DIGEST;
	nc->ctx.ctx_destroy = destroy_digest_context;
	nc->ctx.get_output_length = get_digest_output_length;

	ret = digest_get_algorithm(algo, &md);
	if (ret != YACA_ERROR_NONE)
		goto exit;

	nc->mdctx = EVP_MD_CTX_create();
	if (nc->mdctx == NULL) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	ret = EVP_DigestInit(nc->mdctx, md);
	if (ret != 1) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	*ctx = (yaca_context_h)nc;
	nc = NULL;
	ret = YACA_ERROR_NONE;

exit:
	yaca_context_destroy((yaca_context_h)nc);

	return ret;
}

API int yaca_digest_update(yaca_context_h ctx, const char *data, size_t data_len)
{
	struct yaca_digest_ctx_s *c = get_digest_ctx(ctx);
	int ret;

	if (c == NULL || data == NULL || data_len == 0)
		return YACA_ERROR_INVALID_PARAMETER;

	ret = EVP_DigestUpdate(c->mdctx, data, data_len);
	if (ret != 1) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		return ret;
	}

	return YACA_ERROR_NONE;
}

API int yaca_digest_finalize(yaca_context_h ctx, char *digest, size_t *digest_len)
{
	struct yaca_digest_ctx_s *c = get_digest_ctx(ctx);
	int ret;
	unsigned len = 0;

	if (c == NULL || digest == NULL || digest_len == NULL)
		return YACA_ERROR_INVALID_PARAMETER;

	if (*digest_len == 0 || *digest_len > UINT_MAX) /* DigestFinal accepts UINT */
		return YACA_ERROR_INVALID_PARAMETER;

	ret = EVP_DigestFinal_ex(c->mdctx, (unsigned char*)digest, &len);
	if (ret != 1) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		return ret;
	}

	*digest_len = len;

	return YACA_ERROR_NONE;
}
