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

#include <stdio.h>
#include <crypto/crypto.h>
#include <crypto/digest.h>
#include <crypto/simple.h>
#include "lorem.h"
#include "misc.h"

void digest_simple(void)
{
	int ret = 0;
	char *digest;
	size_t digest_len;

	ret = crypto_digest_calc(CRYPTO_DIGEST_SHA256,
				 lorem1024,
				 1024, &digest, &digest_len);
	if (ret < 0)
		return;

	dump_hex(digest, digest_len, "Message digest: ");

	crypto_free(digest);
}

void digest_advanced(void)
{
	int ret = 0;

	crypto_ctx_h ctx;
	ret = crypto_digest_init(&ctx, CRYPTO_DIGEST_SHA256);
	if (ret) return;

	ret = crypto_digest_update(ctx, lorem1024, 1024);
	if (ret) goto exit_ctx;

	// TODO: rename to crypto_digest_get_length??
	size_t digest_len;
	digest_len = crypto_get_digest_length(ctx);
	if (digest_len <= 0) goto exit_ctx;

	{
		char digest[digest_len];

		ret = crypto_digest_final(ctx, digest, &digest_len);
		if (ret < 0) goto exit_ctx;

		dump_hex(digest, digest_len, "Message digest: ");
	}

exit_ctx:
	crypto_ctx_free(ctx);
}

int main()
{
	int ret = 0;
	if ((ret = crypto_init()))
		return ret;

	digest_simple();

	digest_advanced();

	crypto_exit(); // TODO: what about handing of return value from exit??
	return ret;
}
