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
 * @file key.h
 * @brief
 */

#ifndef KEY_H
#define KEY_H

#include <stddef.h>
#include <crypto/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup  Key  Key and IV handling functions
 *
 * TODO: extended description and examples.
 *
 * @{
 */

#define OWL_KEY_NULL ((owl_key_h) NULL)

// TODO: We need a way to import keys encrypted with hw (or other) keys. New function like owl_key_load or sth??

/**
 * @brief owl_key_get_length  Get key's length.
 *
 * @param[in] key  Key which length we return.
 *
 * @return negative on error (@see error.h) or key length.
 */
int owl_key_get_length(const owl_key_h key);

/**
 * @brief owl_key_import  Imports a key from the arbitrary format.
 *
 * @param[out] key       Returned key (must be freed with @see owl_key_free).
 * @param[in]  key_fmt   Format of the key.
 * @param[in]  key_type  Type of the key.
 * @param[in]  data      Blob containing the key.
 * @param[in]  data_len  Size of the blob.
 *
 * @return 0 on success, negative on error (@see error.h).
 */
int owl_key_import(owl_key_h *key,
		   owl_key_fmt_e key_fmt,
		   owl_key_type_e key_type,
		   const char *data,
		   size_t data_len);

/**
 * @brief owl_key_export  Exports a key to arbitrary format. Export may fail if key is HW-based.
 *
 * @param[in]  key       Key to be exported.
 * @param[in]  key_fmt   Format of the key.
 * @param[out] data      Data, allocated by the library, containing exported key (must be freed with @see owl_free).
 * @param[out] data_len  Size of the output data.
 *
 * @return 0 on success, negative on error (@see error.h).
 */
int owl_key_export(const owl_key_h key,
		   owl_key_fmt_e key_fmt,
		   char **data,
		   size_t *data_len);

// TODO: still a matter of ordering, should the key in key_gen functions be first or last?

/**
 * @brief owl_key_gen  Generates a secure symmetric key (or an initialization vector).
 *
 * @param[out] sym_key   Newly generated key (must be freed with @see owl_key_free).
 * @param[in]  key_type  Type of the key to be generated.
 * @param[in]  key_len   Length of the key to be generated.
 *
 * @return 0 on success, negative on error (@see error.h).
 */
int owl_key_gen(owl_key_h *sym_key,
		owl_key_type_e key_type,
		size_t key_len);

/**
 * @brief owl_key_gen_pair  Generates a new key pair.
 *
 * @param[out] prv_key   Newly generated private key (must be freed with @see owl_key_free).
 * @param[out] pub_key   Newly generated public key (must be freed with @see owl_key_free).
 * @param[in]  key_type  Type of the key to be generated (must be OWL_KEY_TYPE_PAIR*).
 * @param[in]  key_len   Length of the key to be generated.
 *
 * @return 0 on success, negative on error (@see error.h).
 */
int owl_key_gen_pair(owl_key_h *prv_key,
		     owl_key_h *pub_key,
		     owl_key_type_e key_type,
		     size_t key_len);

/**
 * @brief owl_key_free  Frees the key created by the library.
 *                         Passing OWL_KEY_NULL is allowed.
 *
 * @param key  Key to be freed.
 *
 */
void owl_key_free(owl_key_h key);

/**@}*/

/**
 * @defgroup  Key-Derivation  Key derivation functions
 *
 * TODO: rethink separate group.
 * TODO: extended description and examples.
 *
 * @{
 */

/**
 * @brief owl_key_derive_dh  Derives a key using Diffie-Helmann or EC Diffie-Helmann key exchange protocol.
 *
 * @param[in]  prv_key  Our private key.
 * @param[in]  pub_key  Peer public key.
 * @param[out] sym_key  Shared secret, that can be used as a symmetric key (must be freed with @see owl_key_free).
 *
 * @return 0 on success, negative on error (@see error.h).
 */
int owl_key_derive_dh(const owl_key_h prv_key,
		      const owl_key_h pub_key,
		      owl_key_h *sym_key);

/**
 * @brief owl_key_derive_kea  Derives a key using KEA key exchange protocol.
 *
 * @param[in]  prv_key       Our DH private component.
 * @param[in]  pub_key       Peers' DH public component.
 * @param[in]  prv_key_auth  Our private key used to create signature on our
 *                           DH public component sent to peer to verify our identity.
 * @param[in]  pub_key_auth  Peers' public key used for signature verification
 *                           of pub_key from peer (peer authentication).
 * @param[out] sym_key       Shared secret, that can be used as a symmetric key (must be freed with @see owl_key_free).
 *
 * @return 0 on success, negative on error (@see error.h).
 */
int owl_key_derive_kea(const owl_key_h prv_key,
		       const owl_key_h pub_key,
		       const owl_key_h prv_key_auth,
		       const owl_key_h pub_key_auth,
		       owl_key_h *sym_key);

/**
 * @brief owl_key_derive_pbkdf2  Derives a key from user password (PKCS #5 a.k.a. pbkdf2 algorithm).
 *
 * @param[in]  password  User password as a NULL-terminated string.
 * @param[in]  salt      Salt, should be non-zero.
 * @param[in]  salt_len  Length of the salt.
 * @param[in]  iter      Number of iterations. (TODO: add enum to proposed number of iterations, pick sane defaults).
 * @param[in]  algo      Digest algorithm that should be used in key generation. (TODO: sane defaults).
 * @param[in]  key_len   Length of a key to be generated.
 * @param[out] key       Newly generated key (must be freed with @see owl_key_free).
 *
 * @return 0 on success, negative on error (@see error.h).
 */
int owl_key_derive_pbkdf2(const char *password,
			  const char *salt,
			  size_t salt_len,
			  int iter,
			  owl_digest_algo_e algo,
			  owl_key_len_e key_len,
			  owl_key_h *key);

// TODO: specify
//int owl_key_wrap(owl_key_h key, ??);
//int owl_key_unwrap(owl_key_h key, ??);

/**@}*/

#ifdef __cplusplus
} /* extern */
#endif

#endif /* KEY_H */
