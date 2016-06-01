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
 * @file key.c
 * @brief
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/des.h>

#include <yaca_crypto.h>
#include <yaca_error.h>
#include <yaca_key.h>

#include "internal.h"

/* This callback only exists to block the default OpenSSL one and
 * allow us to check for a proper error code when the key is encrypted
 */
int password_dummy_cb(char *buf, int size, int rwflag, void *u)
{
	const char empty[] = "";

	memcpy(buf, empty, sizeof(empty));

	return sizeof(empty);
}

int base64_decode_length(const char *data, size_t data_len, size_t *len)
{
	assert(data != NULL);
	assert(data_len != 0);
	assert(len != NULL);

	size_t padded = 0;
	size_t tmp_len = data_len;

	if (data_len % 4 != 0)
		return YACA_ERROR_INVALID_ARGUMENT;

	if (data[tmp_len - 1] == '=') {
		padded = 1;
		if (data[tmp_len - 2] == '=')
			padded = 2;
	}

	*len = data_len / 4 * 3 - padded;
	return YACA_ERROR_NONE;
}

#define TMP_BUF_LEN 512

int base64_decode(const char *data, size_t data_len, BIO **output)
{
	assert(data != NULL);
	assert(data_len != 0);
	assert(output != NULL);

	int ret;
	BIO *b64 = NULL;
	BIO *src = NULL;
	BIO *dst = NULL;
	char tmpbuf[TMP_BUF_LEN];
	size_t b64_len;
	char *out;
	long out_len;

	/* This is because of BIO_new_mem_buf() having its length param typed int */
	if (data_len > INT_MAX)
		return YACA_ERROR_INVALID_ARGUMENT;

	/* First phase of correctness checking, calculate expected output length */
	ret = base64_decode_length(data, data_len, &b64_len);
	if (ret != YACA_ERROR_NONE)
		return ret;

	b64 = BIO_new(BIO_f_base64());
	if (b64 == NULL) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		return ret;
	}

	src = BIO_new_mem_buf(data, data_len);
	if (src == NULL) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	BIO_push(b64, src);

	dst = BIO_new(BIO_s_mem());
	if (dst == NULL) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

	/* Try to decode */
	for (;;) {
		ret = BIO_read(b64, tmpbuf, TMP_BUF_LEN);
		if (ret < 0) {
			ret = YACA_ERROR_INTERNAL;
			ERROR_DUMP(ret);
			goto exit;
		}

		if (ret == YACA_ERROR_NONE)
			break;

		if (BIO_write(dst, tmpbuf, ret) != ret) {
			ret = YACA_ERROR_INTERNAL;
			ERROR_DUMP(ret);
			goto exit;
		}
	}

	BIO_flush(dst);

	/* Check wether the length of the decoded data is what we expected */
	out_len = BIO_get_mem_data(dst, &out);
	if (out_len < 0) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}
	if ((size_t)out_len != b64_len) {
		ret = YACA_ERROR_INVALID_ARGUMENT;
		goto exit;
	}

	*output = dst;
	dst = NULL;
	ret = YACA_ERROR_NONE;

exit:
	BIO_free_all(b64);
	BIO_free_all(dst);

	return ret;
}

int import_simple(yaca_key_h *key,
                  yaca_key_type_e key_type,
                  const char *data,
                  size_t data_len)
{
	assert(key != NULL);
	assert(data != NULL);
	assert(data_len != 0);

	int ret;
	BIO *decoded = NULL;
	const char *key_data;
	size_t key_data_len;
	struct yaca_key_simple_s *nk = NULL;

	ret = base64_decode(data, data_len, &decoded);
	if (ret == YACA_ERROR_NONE) {
		/* Conversion successfull, get the BASE64 */
		long len = BIO_get_mem_data(decoded, &key_data);
		if (len <= 0 || key_data == NULL) {
			ret = YACA_ERROR_INTERNAL;
			ERROR_DUMP(ret);
			return ret;
		}
		key_data_len = len;
	} else if (ret == YACA_ERROR_INVALID_ARGUMENT) {
		/* This was not BASE64 or it was corrupted, treat as RAW */
		key_data_len = data_len;
		key_data = data;
	} else {
		/* Some other, possibly unrecoverable error, give up */
		return ret;
	}

	/* key_bits has to fit in size_t */
	if (key_data_len > SIZE_MAX / 8) {
		ret = YACA_ERROR_INVALID_ARGUMENT;
		goto exit;
	}

	/* DES key length verification */
	if (key_type == YACA_KEY_TYPE_DES) {
		size_t key_bits = key_data_len * 8;
		if (key_bits != YACA_KEY_UNSAFE_64BIT &&
		    key_bits != YACA_KEY_UNSAFE_128BIT &&
		    key_bits != YACA_KEY_192BIT) {
			ret = YACA_ERROR_INVALID_ARGUMENT;
			goto exit;
		}
	}

	nk = yaca_zalloc(sizeof(struct yaca_key_simple_s) + key_data_len);
	if (nk == NULL) {
		ret = YACA_ERROR_OUT_OF_MEMORY;
		goto exit;
	}

	memcpy(nk->d, key_data, key_data_len);
	nk->bits = key_data_len * 8;
	nk->key.type = key_type;

	*key = (yaca_key_h)nk;
	nk = NULL;
	ret = YACA_ERROR_NONE;

exit:
	BIO_free_all(decoded);

	return ret;
}

bool check_import_wrong_pass()
{
	unsigned long err = ERR_peek_error();
	unsigned long err_bad_password_1 = ERR_PACK(ERR_LIB_PEM, PEM_F_PEM_DO_HEADER, PEM_R_BAD_DECRYPT);
	unsigned long err_bad_password_2 = ERR_PACK(ERR_LIB_EVP, EVP_F_EVP_DECRYPTFINAL_EX, EVP_R_BAD_DECRYPT);

	if (err == err_bad_password_1 || err == err_bad_password_2)
		return true;

	return false;
}

int import_evp(yaca_key_h *key,
               yaca_key_type_e key_type,
               const char *password,
               const char *data,
               size_t data_len)
{
	assert(key != NULL);
	assert(password == NULL || password[0] != '\0');
	assert(data != NULL);
	assert(data_len != 0);

	int ret;
	BIO *src = NULL;
	EVP_PKEY *pkey = NULL;
	bool wrong_pass = false;
	pem_password_cb *cb = NULL;
	bool private;
	yaca_key_type_e type;
	struct yaca_key_evp_s *nk = NULL;

	/* Neither PEM nor DER will ever be shorter then 4 bytes (12 seems
	 * to be minimum for DER, much more for PEM). This is just to make
	 * sure we have at least 4 bytes for strncmp() below.
	 */
	if (data_len < 4)
		return YACA_ERROR_INVALID_ARGUMENT;

	/* This is because of BIO_new_mem_buf() having its length param typed int */
	if (data_len > INT_MAX)
		return YACA_ERROR_INVALID_ARGUMENT;

	src = BIO_new_mem_buf(data, data_len);
	if (src == NULL) {
		ERROR_DUMP(YACA_ERROR_INTERNAL);
		return YACA_ERROR_INTERNAL;
	}

	/* Block the default OpenSSL password callback */
	if (password == NULL)
		cb = password_dummy_cb;

	/* Possible PEM */
	if (strncmp("----", data, 4) == 0) {
		if (pkey == NULL && !wrong_pass) {
			BIO_reset(src);
			pkey = PEM_read_bio_PrivateKey(src, NULL, cb, (void*)password);
			if (check_import_wrong_pass())
				wrong_pass = true;
			private = true;
			ERROR_CLEAR();
		}

		if (pkey == NULL && !wrong_pass) {
			BIO_reset(src);
			pkey = PEM_read_bio_PUBKEY(src, NULL, cb, (void*)password);
			if (check_import_wrong_pass())
				wrong_pass = true;
			private = false;
			ERROR_CLEAR();
		}

		if (pkey == NULL && !wrong_pass) {
			BIO_reset(src);
			X509 *x509 = PEM_read_bio_X509(src, NULL, cb, (void*)password);
			if (check_import_wrong_pass())
				wrong_pass = true;
			if (x509 != NULL)
				pkey = X509_get_pubkey(x509);
			X509_free(x509);
			private = false;
			ERROR_CLEAR();
		}
	}
	/* Possible DER */
	else {
		if (pkey == NULL && !wrong_pass) {
			BIO_reset(src);
			pkey = d2i_PKCS8PrivateKey_bio(src, NULL, cb, (void*)password);
			if (check_import_wrong_pass())
				wrong_pass = true;
			private = true;
			ERROR_CLEAR();
		}

		if (pkey == NULL && !wrong_pass) {
			BIO_reset(src);
			pkey = d2i_PrivateKey_bio(src, NULL);
			private = true;
			ERROR_CLEAR();
		}

		if (pkey == NULL && !wrong_pass) {
			BIO_reset(src);
			pkey = d2i_PUBKEY_bio(src, NULL);
			private = false;
			ERROR_CLEAR();
		}
	}

	BIO_free(src);

	if (wrong_pass)
		return YACA_ERROR_PASSWORD_INVALID;

	if (pkey == NULL)
		return YACA_ERROR_INVALID_ARGUMENT;

	switch (EVP_PKEY_type(pkey->type)) {
	case EVP_PKEY_RSA:
		type = private ? YACA_KEY_TYPE_RSA_PRIV : YACA_KEY_TYPE_RSA_PUB;
		break;

	case EVP_PKEY_DSA:
		type = private ? YACA_KEY_TYPE_DSA_PRIV : YACA_KEY_TYPE_DSA_PUB;
		break;

//	case EVP_PKEY_EC:
//		type = private ? YACA_KEY_TYPE_EC_PRIV : YACA_KEY_TYPE_EC_PUB;
//		break;

	default:
		ret = YACA_ERROR_INVALID_ARGUMENT;
		goto exit;
	}

	if (type != key_type) {
		ret = YACA_ERROR_INVALID_ARGUMENT;
		goto exit;
	}

	nk = yaca_zalloc(sizeof(struct yaca_key_evp_s));
	if (nk == NULL) {
		ret = YACA_ERROR_OUT_OF_MEMORY;
		goto exit;
	}

	nk->evp = pkey;
	*key = (yaca_key_h)nk;
	(*key)->type = type;

	pkey = NULL;
	ret = YACA_ERROR_NONE;

exit:
	EVP_PKEY_free(pkey);

	return ret;
}

int export_simple_raw(struct yaca_key_simple_s *simple_key,
                      char **data,
                      size_t *data_len)
{
	assert(simple_key != NULL);
	assert(data != NULL);
	assert(data_len != NULL);

	size_t key_len = simple_key->bits / 8;

	*data = yaca_malloc(key_len);
	if (*data == NULL) {
		ERROR_DUMP(YACA_ERROR_OUT_OF_MEMORY);
		return YACA_ERROR_OUT_OF_MEMORY;
	}

	memcpy(*data, simple_key->d, key_len);
	*data_len = key_len;

	return YACA_ERROR_NONE;
}

int export_simple_base64(struct yaca_key_simple_s *simple_key,
                         char **data,
                         size_t *data_len)
{
	assert(simple_key != NULL);
	assert(data != NULL);
	assert(data_len != NULL);

	int ret;
	size_t key_len = simple_key->bits / 8;
	BIO *b64;
	BIO *mem;
	char *bio_data;
	long bio_data_len;

	b64 = BIO_new(BIO_f_base64());
	if (b64 == NULL) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		return ret;
	}

	mem = BIO_new(BIO_s_mem());
	if (mem == NULL) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	BIO_push(b64, mem);
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

	ret = BIO_write(b64, simple_key->d, key_len);
	if (ret <= 0 || (unsigned)ret != key_len) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	ret = BIO_flush(b64);
	if (ret <= 0) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	bio_data_len = BIO_get_mem_data(mem, &bio_data);
	if (bio_data_len <= 0) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	*data = yaca_malloc(bio_data_len);
	if (*data == NULL) {
		ret = YACA_ERROR_OUT_OF_MEMORY;
		ERROR_DUMP(ret);
		goto exit;
	}

	memcpy(*data, bio_data, bio_data_len);
	*data_len = bio_data_len;
	ret = YACA_ERROR_NONE;

exit:
	BIO_free_all(b64);

	return ret;
}

int export_evp_default_bio(struct yaca_key_evp_s *evp_key,
                           yaca_key_file_fmt_e key_file_fmt,
                           const char *password,
                           BIO *mem)
{
	assert(evp_key != NULL);
	assert(password == NULL || password[0] != '\0');
	assert(mem != NULL);

	int ret;
	const EVP_CIPHER *enc = NULL;

	if (password != NULL)
		enc = EVP_aes_256_cbc();

	switch (key_file_fmt) {

	case YACA_KEY_FILE_FORMAT_PEM:
		switch (evp_key->key.type) {

		case YACA_KEY_TYPE_RSA_PRIV:
			ret = PEM_write_bio_RSAPrivateKey(mem, EVP_PKEY_get0(evp_key->evp),
			                                  enc, NULL, 0, NULL, (void*)password);
			break;
		case YACA_KEY_TYPE_DSA_PRIV:
			ret = PEM_write_bio_DSAPrivateKey(mem, EVP_PKEY_get0(evp_key->evp),
			                                  enc, NULL, 0, NULL, (void*)password);
			break;

		case YACA_KEY_TYPE_RSA_PUB:
		case YACA_KEY_TYPE_DSA_PUB:
			ret = PEM_write_bio_PUBKEY(mem, evp_key->evp);
			break;

//		case YACA_KEY_TYPE_DH_PRIV:
//		case YACA_KEY_TYPE_DH_PUB:
//		case YACA_KEY_TYPE_EC_PRIV:
//		case YACA_KEY_TYPE_EC_PUB:
//			TODO NOT_IMPLEMENTED
		default:
			return YACA_ERROR_INVALID_ARGUMENT;
		}

		break;

	case YACA_KEY_FILE_FORMAT_DER:
		switch (evp_key->key.type) {

		case YACA_KEY_TYPE_RSA_PRIV:
			ret = i2d_RSAPrivateKey_bio(mem, EVP_PKEY_get0(evp_key->evp));
			break;

		case YACA_KEY_TYPE_DSA_PRIV:
			ret = i2d_DSAPrivateKey_bio(mem, EVP_PKEY_get0(evp_key->evp));
			break;

		case YACA_KEY_TYPE_RSA_PUB:
		case YACA_KEY_TYPE_DSA_PUB:
			ret = i2d_PUBKEY_bio(mem, evp_key->evp);
			break;

//		case YACA_KEY_TYPE_DH_PRIV:
//		case YACA_KEY_TYPE_DH_PUB:
//		case YACA_KEY_TYPE_EC_PRIV:
//		case YACA_KEY_TYPE_EC_PUB:
//			TODO NOT_IMPLEMENTED
		default:
			return YACA_ERROR_INVALID_ARGUMENT;
		}

		break;

	default:
		return YACA_ERROR_INVALID_ARGUMENT;
	}

	if (ret <= 0) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		return ret;
	}

	return YACA_ERROR_NONE;
}

int export_evp_pkcs8_bio(struct yaca_key_evp_s *evp_key,
                         yaca_key_file_fmt_e key_file_fmt,
                         const char *password,
                         BIO *mem)
{
	assert(evp_key != NULL);
	assert(password == NULL || password[0] != '\0');
	assert(mem != NULL);

	int ret;
	int nid = -1;

	if (password != NULL)
		nid = NID_pbeWithMD5AndDES_CBC;

	switch (key_file_fmt) {

	case YACA_KEY_FILE_FORMAT_PEM:
		switch (evp_key->key.type) {

		case YACA_KEY_TYPE_RSA_PRIV:
		case YACA_KEY_TYPE_DSA_PRIV:
			ret = PEM_write_bio_PKCS8PrivateKey_nid(mem, evp_key->evp, nid,
			                                        NULL, 0, NULL, (void*)password);
			break;
//		case YACA_KEY_TYPE_DH_PRIV:
//		case YACA_KEY_TYPE_EC_PRIV:
//			TODO NOT_IMPLEMENTED
		default:
			/* Public keys are not supported by PKCS8 */
			return YACA_ERROR_INVALID_ARGUMENT;
		}

		break;

	case YACA_KEY_FILE_FORMAT_DER:
		switch (evp_key->key.type) {

		case YACA_KEY_TYPE_RSA_PRIV:
		case YACA_KEY_TYPE_DSA_PRIV:
			ret = i2d_PKCS8PrivateKey_nid_bio(mem, evp_key->evp, nid,
			                                  NULL, 0, NULL, (void*)password);
			break;

//		case YACA_KEY_TYPE_DH_PRIV:
//		case YACA_KEY_TYPE_EC_PRIV:
//			TODO NOT_IMPLEMENTED
		default:
			/* Public keys are not supported by PKCS8 */
			return YACA_ERROR_INVALID_ARGUMENT;
		}

		break;

	default:
		return YACA_ERROR_INVALID_ARGUMENT;
	}

	if (ret <= 0) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		return ret;
	}

	return YACA_ERROR_NONE;
}

int export_evp(struct yaca_key_evp_s *evp_key,
               yaca_key_fmt_e key_fmt,
               yaca_key_file_fmt_e key_file_fmt,
               const char *password,
               char **data,
               size_t *data_len)
{
	assert(evp_key != NULL);
	assert(password == NULL || password[0] != '\0');
	assert(data != NULL);
	assert(data_len != NULL);

	int ret = YACA_ERROR_NONE;
	BIO *mem;
	char *bio_data;
	long bio_data_len;

	mem = BIO_new(BIO_s_mem());
	if (mem == NULL) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		return ret;
	}

	switch (key_fmt) {
	case YACA_KEY_FORMAT_DEFAULT:
		ret = export_evp_default_bio(evp_key, key_file_fmt, password, mem);
		break;
	case YACA_KEY_FORMAT_PKCS8:
		ret = export_evp_pkcs8_bio(evp_key, key_file_fmt, password, mem);
		break;
	default:
		ret = YACA_ERROR_INVALID_ARGUMENT;
		break;
	}

	if (ret != YACA_ERROR_NONE)
		goto exit;

	ret = BIO_flush(mem);
	if (ret <= 0) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	bio_data_len = BIO_get_mem_data(mem, &bio_data);
	if (bio_data_len <= 0) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	*data = yaca_malloc(bio_data_len);
	if (*data == NULL) {
		ret = YACA_ERROR_OUT_OF_MEMORY;
		ERROR_DUMP(ret);
		goto exit;
	}

	memcpy(*data, bio_data, bio_data_len);
	*data_len = bio_data_len;
	ret = YACA_ERROR_NONE;

exit:
	BIO_free_all(mem);

	return ret;
}

int gen_simple(struct yaca_key_simple_s **out, size_t key_bits)
{
	assert(out != NULL);

	int ret;
	struct yaca_key_simple_s *nk;
	size_t key_byte_len = key_bits / 8;

	nk = yaca_zalloc(sizeof(struct yaca_key_simple_s) + key_byte_len);
	if (nk == NULL)
		return YACA_ERROR_OUT_OF_MEMORY;

	nk->bits = key_bits;

	ret = yaca_rand_bytes(nk->d, key_byte_len);
	if (ret != YACA_ERROR_NONE)
		return ret;

	*out = nk;
	return YACA_ERROR_NONE;
}

int gen_simple_des(struct yaca_key_simple_s **out, size_t key_bits)
{
	assert(out != NULL);

	if (key_bits != YACA_KEY_UNSAFE_64BIT &&
	    key_bits != YACA_KEY_UNSAFE_128BIT &&
	    key_bits != YACA_KEY_192BIT)
		return YACA_ERROR_INVALID_ARGUMENT;

	int ret;
	struct yaca_key_simple_s *nk;
	size_t key_byte_len = key_bits / 8;

	nk = yaca_zalloc(sizeof(struct yaca_key_simple_s) + key_byte_len);
	if (nk == NULL)
		return YACA_ERROR_OUT_OF_MEMORY;

	DES_cblock *des_key = (DES_cblock*)nk->d;
	if (key_byte_len >= 8) {
		ret = DES_random_key(des_key);
		if (ret != 1) {
			ret = YACA_ERROR_INTERNAL;
			ERROR_DUMP(ret);
			goto exit;
		}
	}
	if (key_byte_len >= 16) {
		ret = DES_random_key(des_key + 1);
		if (ret != 1) {
			ret = YACA_ERROR_INTERNAL;
			ERROR_DUMP(ret);
			goto exit;
		}
	}
	if (key_byte_len >= 24) {
		ret = DES_random_key(des_key + 2);
		if (ret != 1) {
			ret = YACA_ERROR_INTERNAL;
			ERROR_DUMP(ret);
			goto exit;
		}
	}

	nk->bits = key_bits;
	*out = nk;
	nk = NULL;
	ret = YACA_ERROR_NONE;

exit:
	yaca_free(nk);

	return ret;
}

// TODO: consider merging gen_evp_*, they share awful lot of common code
int gen_evp_rsa(struct yaca_key_evp_s **out, size_t key_bits)
{
	assert(out != NULL);
	assert(key_bits > 0);
	assert(key_bits % 8 == 0);

	int ret;
	struct yaca_key_evp_s *nk;
	EVP_PKEY_CTX *ctx;
	EVP_PKEY *pkey = NULL;

	nk = yaca_zalloc(sizeof(struct yaca_key_evp_s));
	if (nk == NULL)
		return YACA_ERROR_OUT_OF_MEMORY;

	ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
	if (ctx == NULL) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	ret = EVP_PKEY_keygen_init(ctx);
	if (ret != 1) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	ret = EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, key_bits);
	if (ret != 1) {
		ret = ERROR_HANDLE();
		goto exit;
	}

	ret = EVP_PKEY_keygen(ctx, &pkey);
	if (ret != 1) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	nk->evp = pkey;
	pkey = NULL;
	*out = nk;
	nk = NULL;

	ret = YACA_ERROR_NONE;

exit:
	EVP_PKEY_CTX_free(ctx);
	yaca_free(nk);

	return ret;
}

int gen_evp_dsa(struct yaca_key_evp_s **out, size_t key_bits)
{
	assert(out != NULL);
	assert(key_bits > 0);
	assert(key_bits % 8 == 0);

	/* Openssl generates 512-bit key for key lengths smaller than 512. It also
	 * rounds key size to multiplication of 64. */
	if(key_bits < 512 || key_bits % 64 != 0)
		return YACA_ERROR_INVALID_ARGUMENT;

	int ret;
	struct yaca_key_evp_s *nk;
	EVP_PKEY_CTX *pctx = NULL;
	EVP_PKEY_CTX *kctx = NULL;
	EVP_PKEY *pkey = NULL;
	EVP_PKEY *params = NULL;

	nk = yaca_zalloc(sizeof(struct yaca_key_evp_s));
	if (nk == NULL)
		return YACA_ERROR_OUT_OF_MEMORY;

	pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DSA, NULL);
	if (pctx == NULL) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	ret = EVP_PKEY_paramgen_init(pctx);
	if (ret != 1) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	ret = EVP_PKEY_CTX_set_dsa_paramgen_bits(pctx, key_bits);
	if (ret != 1) {
		ret = ERROR_HANDLE();
		goto exit;
	}

	ret = EVP_PKEY_paramgen(pctx, &params);
	if (ret != 1) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	kctx = EVP_PKEY_CTX_new(params, NULL);
	if (kctx == NULL) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	ret = EVP_PKEY_keygen_init(kctx);
	if (ret != 1) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	ret = EVP_PKEY_keygen(kctx, &pkey);
	if (ret != 1) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	nk->evp = pkey;
	pkey = NULL;
	*out = nk;
	nk = NULL;

	ret = YACA_ERROR_NONE;

exit:
	EVP_PKEY_CTX_free(kctx);
	EVP_PKEY_free(params);
	EVP_PKEY_CTX_free(pctx);
	yaca_free(nk);

	return ret;
}

struct yaca_key_simple_s *key_get_simple(const yaca_key_h key)
{
	struct yaca_key_simple_s *k;

	if (key == YACA_KEY_NULL)
		return NULL;

	switch (key->type) {
	case YACA_KEY_TYPE_SYMMETRIC:
	case YACA_KEY_TYPE_DES:
	case YACA_KEY_TYPE_IV:
		k = (struct yaca_key_simple_s *)key;

		/* sanity check */
		assert(k->bits != 0);
		assert(k->bits % 8 == 0);
		assert(k->d != NULL);

		return k;
	default:
		return NULL;
	}
}

struct yaca_key_evp_s *key_get_evp(const yaca_key_h key)
{
	struct yaca_key_evp_s *k;

	if (key == YACA_KEY_NULL)
		return NULL;

	switch (key->type) {
	case YACA_KEY_TYPE_RSA_PUB:
	case YACA_KEY_TYPE_RSA_PRIV:
	case YACA_KEY_TYPE_DSA_PUB:
	case YACA_KEY_TYPE_DSA_PRIV:
		k = (struct yaca_key_evp_s *)key;

		/* sanity check */
		assert(k->evp != NULL);

		return k;
	default:
		return NULL;
	}
}

API int yaca_key_get_type(const yaca_key_h key, yaca_key_type_e *key_type)
{
	const struct yaca_key_s *lkey = (const struct yaca_key_s *)key;

	if (lkey == NULL || key_type == NULL)
		return YACA_ERROR_INVALID_ARGUMENT;

	*key_type = lkey->type;
	return YACA_ERROR_NONE;
}

API int yaca_key_get_bits(const yaca_key_h key, size_t *key_bits)
{
	const struct yaca_key_simple_s *simple_key = key_get_simple(key);
	const struct yaca_key_evp_s *evp_key = key_get_evp(key);

	if (key_bits == NULL)
		return YACA_ERROR_INVALID_ARGUMENT;

	if (simple_key != NULL) {
		*key_bits = simple_key->bits;
		return YACA_ERROR_NONE;
	}

	if (evp_key != NULL) {
		int ret;

		// TODO: handle ECC keys when they're implemented
		ret = EVP_PKEY_bits(evp_key->evp);
		if (ret <= 0) {
			ret = YACA_ERROR_INTERNAL;
			ERROR_DUMP(ret);
			return ret;
		}

		*key_bits = ret;
		return YACA_ERROR_NONE;
	}

	return YACA_ERROR_INVALID_ARGUMENT;
}

API int yaca_key_import(yaca_key_type_e key_type,
                        const char *password,
                        const char *data,
                        size_t data_len,
                        yaca_key_h *key)
{
	if (key == NULL || data == NULL || data_len == 0)
		return YACA_ERROR_INVALID_ARGUMENT;

	/* allow an empty password, OpenSSL returns an error with "" */
	if (password != NULL && password[0] == '\0')
		password = NULL;

	switch (key_type) {
	case YACA_KEY_TYPE_SYMMETRIC:
	case YACA_KEY_TYPE_DES:
	case YACA_KEY_TYPE_IV:
		if (password != NULL)
			return YACA_ERROR_INVALID_ARGUMENT;
		return import_simple(key, key_type, data, data_len);
	case YACA_KEY_TYPE_RSA_PUB:
	case YACA_KEY_TYPE_RSA_PRIV:
	case YACA_KEY_TYPE_DSA_PUB:
	case YACA_KEY_TYPE_DSA_PRIV:
		return import_evp(key, key_type, password, data, data_len);
//	case YACA_KEY_TYPE_DH_PUB:
//	case YACA_KEY_TYPE_DH_PRIV:
//	case YACA_KEY_TYPE_EC_PUB:
//	case YACA_KEY_TYPE_EC_PRIV:
//		TODO NOT_IMPLEMENTED
	default:
		return YACA_ERROR_INVALID_ARGUMENT;
	}
}

API int yaca_key_export(const yaca_key_h key,
                        yaca_key_fmt_e key_fmt,
                        yaca_key_file_fmt_e key_file_fmt,
                        const char *password,
                        char **data,
                        size_t *data_len)
{
	struct yaca_key_simple_s *simple_key = key_get_simple(key);
	struct yaca_key_evp_s *evp_key = key_get_evp(key);

	if (data == NULL || data_len == NULL)
		return YACA_ERROR_INVALID_ARGUMENT;

	/* allow an empty password, OpenSSL returns an error with "" */
	if (password != NULL && password[0] == '\0')
		password = NULL;

	if (password != NULL && simple_key != NULL)
		return YACA_ERROR_INVALID_ARGUMENT;

	if (key_fmt == YACA_KEY_FORMAT_DEFAULT &&
	    key_file_fmt == YACA_KEY_FILE_FORMAT_RAW &&
	    simple_key != NULL)
		return export_simple_raw(simple_key, data, data_len);

	if (key_fmt == YACA_KEY_FORMAT_DEFAULT &&
	    key_file_fmt == YACA_KEY_FILE_FORMAT_BASE64 &&
	    simple_key != NULL)
		return export_simple_base64(simple_key, data, data_len);

	if (evp_key != NULL)
		return export_evp(evp_key, key_fmt, key_file_fmt,
		                  password, data, data_len);

	return YACA_ERROR_INVALID_ARGUMENT;
}

API int yaca_key_gen(yaca_key_type_e key_type,
                     size_t key_bits,
                     yaca_key_h *key)
{
	int ret;
	struct yaca_key_simple_s *nk_simple = NULL;
	struct yaca_key_evp_s *nk_evp = NULL;

	if (key == NULL || key_bits == 0 || key_bits % 8 != 0)
		return YACA_ERROR_INVALID_ARGUMENT;

	switch (key_type) {
	case YACA_KEY_TYPE_SYMMETRIC:
	case YACA_KEY_TYPE_IV:
		ret = gen_simple(&nk_simple, key_bits);
		break;
	case YACA_KEY_TYPE_DES:
		ret = gen_simple_des(&nk_simple, key_bits);
		break;
	case YACA_KEY_TYPE_RSA_PRIV:
		ret = gen_evp_rsa(&nk_evp, key_bits);
		break;
	case YACA_KEY_TYPE_DSA_PRIV:
		ret = gen_evp_dsa(&nk_evp, key_bits);
		break;
//	case YACA_KEY_TYPE_DH_PRIV:
//	case YACA_KEY_TYPE_EC_PRIV:
//		TODO NOT_IMPLEMENTED
	default:
		return YACA_ERROR_INVALID_ARGUMENT;
	}

	if (ret != YACA_ERROR_NONE)
		return ret;

	if (nk_simple != NULL) {
		nk_simple->key.type = key_type;
		*key = (yaca_key_h)nk_simple;
	} else if (nk_evp != NULL) {
		nk_evp->key.type = key_type;
		*key = (yaca_key_h)nk_evp;
	}

	return YACA_ERROR_NONE;

}

API int yaca_key_extract_public(const yaca_key_h prv_key, yaca_key_h *pub_key)
{
	int ret;
	struct yaca_key_evp_s *evp_key = key_get_evp(prv_key);
	struct yaca_key_evp_s *nk;
	BIO *mem = NULL;
	EVP_PKEY *pkey = NULL;

	if (prv_key == YACA_KEY_NULL || evp_key == NULL || pub_key == NULL)
		return YACA_ERROR_INVALID_ARGUMENT;

	nk = yaca_zalloc(sizeof(struct yaca_key_evp_s));
	if (nk == NULL)
		return YACA_ERROR_OUT_OF_MEMORY;

	mem = BIO_new(BIO_s_mem());
	if (mem == NULL) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	ret = i2d_PUBKEY_bio(mem, evp_key->evp);
	if (ret != 1) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	pkey = d2i_PUBKEY_bio(mem, NULL);
	if (pkey == NULL) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	BIO_free(mem);
	mem = NULL;

	switch (prv_key->type) {
	case YACA_KEY_TYPE_RSA_PRIV:
		nk->key.type = YACA_KEY_TYPE_RSA_PUB;
		break;
	case YACA_KEY_TYPE_DSA_PRIV:
		nk->key.type = YACA_KEY_TYPE_DSA_PUB;
		break;
//	case YACA_KEY_TYPE_EC_PRIV:
//		nk->key.type = YACA_KEY_TYPE_EC_PUB;
//		break;
	default:
		ret = YACA_ERROR_INVALID_ARGUMENT;
		goto exit;
	}

	nk->evp = pkey;
	pkey = NULL;
	*pub_key = (yaca_key_h)nk;
	nk = NULL;
	ret = YACA_ERROR_NONE;

exit:
	EVP_PKEY_free(pkey);
	BIO_free(mem);
	yaca_free(nk);

	return ret;
}

API void yaca_key_free(yaca_key_h key)
{
	struct yaca_key_simple_s *simple_key = key_get_simple(key);
	struct yaca_key_evp_s *evp_key = key_get_evp(key);

	if (simple_key != NULL)
		yaca_free(simple_key);

	if (evp_key != NULL) {
		EVP_PKEY_free(evp_key->evp);
		yaca_free(evp_key);
	}
}

API int yaca_key_derive_pbkdf2(const char *password,
                               const char *salt,
                               size_t salt_len,
                               int iter,
                               yaca_digest_algo_e algo,
                               size_t key_bits,
                               yaca_key_h *key)
{
	const EVP_MD *md;
	struct yaca_key_simple_s *nk;
	size_t key_byte_len = key_bits / 8;
	int ret;

	if (password == NULL || salt == NULL || salt_len == 0 ||
	    iter == 0 || key_bits == 0 || key == NULL)
		return YACA_ERROR_INVALID_ARGUMENT;

	if (key_bits % 8) /* Key length must be multiple of 8-bits */
		return YACA_ERROR_INVALID_ARGUMENT;

	ret = digest_get_algorithm(algo, &md);
	if (ret != YACA_ERROR_NONE)
		return ret;

	nk = yaca_zalloc(sizeof(struct yaca_key_simple_s) + key_byte_len);
	if (nk == NULL)
		return YACA_ERROR_OUT_OF_MEMORY;

	nk->bits = key_bits;
	nk->key.type = YACA_KEY_TYPE_SYMMETRIC; // TODO: how to handle other keys?

	ret = PKCS5_PBKDF2_HMAC(password, -1, (const unsigned char*)salt,
	                        salt_len, iter, md, key_byte_len,
	                        (unsigned char*)nk->d);
	if (ret != 1) {
		ret = YACA_ERROR_INTERNAL;
		ERROR_DUMP(ret);
		goto exit;
	}

	*key = (yaca_key_h)nk;
	nk = NULL;
	ret = YACA_ERROR_NONE;
exit:
	yaca_free(nk);

	return ret;
}
