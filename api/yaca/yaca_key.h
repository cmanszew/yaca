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
 * @file   yaca_key.h
 * @brief  Advanced API for the key and IV handling.
 */

#ifndef YACA_KEY_H
#define YACA_KEY_H

#include <stddef.h>
#include <yaca_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup CAPI_YACA_KEY_MODULE
 * @{
 */

/**
 * @brief  NULL value for yaca_key_h type.
 *
 * @since_tizen 3.0
 */
#define YACA_KEY_NULL ((yaca_key_h) NULL)

/**
 * @brief  Gets key's type.
 *
 * @since_tizen 3.0
 *
 * @param[in]  key       Key which type we return
 * @param[out] key_type  Key type
 *
 * @return #YACA_ERROR_NONE on success, negative on error
 * @retval #YACA_ERROR_NONE Successful
 * @retval #YACA_ERROR_INVALID_PARAMETER Either of the params is NULL
 *
 * @see #yaca_key_type_e
 */
int yaca_key_get_type(const yaca_key_h key, yaca_key_type_e *key_type);

/**
 * @brief  Gets key's length (in bits).
 *
 * @since_tizen 3.0
 *
 * @remarks  @a key can be any symmetric (including an IV) or
 *           asymmetric key (including key generation parameters).
 *
 * @remarks  For Diffie-Helmann @a key_bit_len returns prime length in bits. Values
 *           used to generate the key/parammeters in yaca_key_generate() are not
 *           restored. Neither generator number nor values from #yaca_key_bit_length_dh_rfc_e.
 *
 * @remarks  For Elliptic Curves @a key_bit_len returns values from #yaca_key_bit_length_ec_e.
 *
 * @param[in]  key          Key which length we return
 * @param[out] key_bit_len  Key length in bits
 *
 * @return #YACA_ERROR_NONE on success, negative on error
 * @retval #YACA_ERROR_NONE Successful
 * @retval #YACA_ERROR_INVALID_PARAMETER Either of the params is NULL
 * @retval #YACA_ERROR_INTERNAL Internal error
 *
 * @see #yaca_key_bit_length_e
 * @see #yaca_key_bit_length_dh_rfc_e
 * @see #yaca_key_bit_length_ec_e
 */
int yaca_key_get_bit_length(const yaca_key_h key, size_t *key_bit_len);

/**
 * @brief  Imports a key or key generation parameters.
 *
 * @since_tizen 3.0
 *
 * @remarks  Everywhere where either a key (of any type) or an asymmetric key is referred
 *           in the documentation of this function key generator parameters are also included.
 *
 * @remarks  This function imports a key trying to match it to the @a key_type specified.
 *           It should autodetect both the key format and the file format.
 *
 * @remarks  For symmetric, IV and DES keys RAW binary format and BASE64 encoded
 *           binary format are supported.
 *           For asymmetric keys PEM and DER file formats are supported.
 *
 * @remarks  Asymmetric keys can be in their default ASN1 structure formats (like
 *           PKCS#1, SSleay or PKCS#3). Private asymmetric keys can also be in
 *           PKCS#8 format. Additionally it is possible to import public RSA/DSA/EC
 *           keys from X509 certificate.
 *
 * @remarks  If the key is encrypted the algorithm will be autodetected and password
 *           used. If it's not known if the key is encrypted one should pass NULL as
 *           password and check for the #YACA_ERROR_INVALID_PASSWORD return code.
 *
 * @remarks  If the imported key will be detected as a format that does not support
 *           encryption and password was passed #YACA_ERROR_INVALID_PARAMETER will
 *           be returned. For a list of keys and formats that do support encryption
 *           see yaca_key_export() documentation.
 *
 * @remarks  The @a key should be released using yaca_key_destroy()
 *
 * @param[in]  key_type  Type of the key
 * @param[in]  password  null terminated password for the key (can be NULL)
 * @param[in]  data      Blob containing the key
 * @param[in]  data_len  Size of the blob
 * @param[out] key       Returned key
 *
 * @return #YACA_ERROR_NONE on success, negative on error
 * @retval #YACA_ERROR_NONE Successful
 * @retval #YACA_ERROR_INVALID_PARAMETER Required parameters have incorrect values (NULL, 0,
 *                                       invalid @a key_type or @a data_len too big)
 * @retval #YACA_ERROR_OUT_OF_MEMORY Out of memory error
 * @retval #YACA_ERROR_INTERNAL Internal error
 * @retval #YACA_ERROR_INVALID_PASSWORD Invalid password given or password was required
 *                                      and none was given
 *
 * @see #yaca_key_type_e
 * @see yaca_key_export()
 * @see yaca_key_destroy()
 */
int yaca_key_import(yaca_key_type_e key_type,
                    const char *password,
                    const char *data,
                    size_t data_len,
                    yaca_key_h *key);

/**
 * @brief  Exports a key or key generation parameters to arbitrary format.
 *
 * @since_tizen 3.0
 *
 * @remarks  Everywhere where either a key (of any type) or an asymmetric key is referred
 *           in the documentation of this function key generator parameters are also included.
 *
 * @remarks  This function exports the key to an arbitrary key format and key file format.
 *
 * @remarks  For key formats two values are allowed:
 *           - #YACA_KEY_FORMAT_DEFAULT: this is the only option possible in case of symmetric
 *                                       keys (or IV), for asymmetric keys it will export to their
 *                                       default ASN1 structure format (e.g. PKCS#1, SSLeay, PKCS#3).
 *           - #YACA_KEY_FORMAT_PKCS8: this will only work for private asymmetric keys.
 *
 * @remarks  The following file formats are supported:
 *           - #YACA_KEY_FILE_FORMAT_RAW:    used only for symmetric, raw binary format
 *           - #YACA_KEY_FILE_FORMAT_BASE64: used only for symmetric, BASE64 encoded binary form
 *           - #YACA_KEY_FILE_FORMAT_PEM:    used only for asymmetric, PEM file format
 *           - #YACA_KEY_FILE_FORMAT_DER:    used only for asymmetric, DER file format
 *
 * @remarks  Encryption is supported and optional for RSA/DSA private keys in the
 *           #YACA_KEY_FORMAT_DEFAULT with #YACA_KEY_FILE_FORMAT_PEM format. If no password is
 *           provided the exported key will be unencrypted. The encryption algorithm used
 *           in this case is AES-256-CBC.
 *
 * @remarks  Encryption is obligatory for #YACA_KEY_FORMAT_PKCS8 format (for both, PEM and DER
 *           file formats). If no password is provided the #YACA_ERROR_INVALID_PARAMETER will
 *           be returned. The encryption algorithm used in this case is PBE with DES-CBC.
 *
 * @remarks  Encryption is not supported for the symmetric, public keys and key generation
 *           parameters in all their supported formats. If a password is provided in such
 *           case the #YACA_ERROR_INVALID_PARAMETER will be returned.
 *
 * @param[in]  key           Key to be exported
 * @param[in]  key_fmt       Format of the key
 * @param[in]  key_file_fmt  Format of the key file
 * @param[in]  password      Password used for the encryption (can be NULL)
 * @param[out] data          Data, allocated by the library, containing exported key
 *                           (must be freed with yaca_free())
 * @param[out] data_len      Size of the output data
 *
 * @return #YACA_ERROR_NONE on success, negative on error
 * @retval #YACA_ERROR_NONE Successful
 * @retval #YACA_ERROR_INVALID_PARAMETER Required parameters have incorrect values (NULL, 0,
 *                                       invalid key/file format or @ data_len too big)
 * @retval #YACA_ERROR_OUT_OF_MEMORY Out of memory error
 * @retval #YACA_ERROR_INTERNAL Internal error
 *
 * @see #yaca_key_format_e
 * @see #yaca_key_file_format_e
 * @see yaca_key_import()
 * @see yaca_key_destroy()
 */
int yaca_key_export(const yaca_key_h key,
                    yaca_key_format_e key_fmt,
                    yaca_key_file_format_e key_file_fmt,
                    const char *password,
                    char **data,
                    size_t *data_len);

/**
 * @brief  Generates a secure key or key generation parameters (or an initialization vector).
 *
 * @since_tizen 3.0
 *
 * @remarks  This function is used to generate symmetric keys, private asymmetric keys
 *           or key generation parameters for key types that support them (DSA, DH and EC).
 *
 * @remarks  Supported key lengths:
 *           - RSA: length >= 256bits
 *           - DSA: length >= 512bits, multiple of 64
 *           - DH: a value taken from #yaca_key_bit_length_dh_rfc_e or
 *                 (YACA_KEY_LENGTH_DH_GENERATOR_* | prime_length_in_bits),
 *                 where prime_length_in_bits can be any positive number
 *           - EC: a value taken from #yaca_key_bit_length_ec_e
 *
 * @remarks  The @a key should be released using yaca_key_destroy()
 *
 * @param[in]  key_type     Type of the key to be generated
 * @param[in]  key_bit_len  Length of the key (in bits) to be generated
 * @param[out] key          Newly generated key
 *
 * @return #YACA_ERROR_NONE on success, negative on error
 * @retval #YACA_ERROR_NONE Successful
 * @retval #YACA_ERROR_INVALID_PARAMETER key is NULL, incorrect @a key_type or
 *                                       @a key_bit_len is not dividable by 8
 * @retval #YACA_ERROR_OUT_OF_MEMORY Out of memory error
 * @retval #YACA_ERROR_INTERNAL Internal error
 *
 * @see #yaca_key_type_e
 * @see #yaca_key_bit_length_e
 * @see #yaca_key_bit_length_dh_rfc_e
 * @see #YACA_KEY_LENGTH_DH_GENERATOR_2
 * @see #YACA_KEY_LENGTH_DH_GENERATOR_5
 * @see #yaca_key_bit_length_ec_e
 * @see yaca_key_destroy()
 */
int yaca_key_generate(yaca_key_type_e key_type,
                      size_t key_bit_len,
                      yaca_key_h *key);

/**
 * @brief  Generates a secure private asymmetric key from parameters.
 *
 * @since_tizen 3.0
 *
 * @remarks  This function is used to generate private asymmetric keys
 *           based on pre-generated parameters.
 *
 * @remarks  The @a key should be released using yaca_key_destroy()
 *
 * @param[in]  params   Pre-generated parameters
 * @param[out] prv_key  Newly generated private key
 *
 * @return #YACA_ERROR_NONE on success, negative on error
 * @retval #YACA_ERROR_NONE Successful
 * @retval #YACA_ERROR_INVALID_PARAMETER key is NULL or incorrect @a params
 * @retval #YACA_ERROR_OUT_OF_MEMORY Out of memory error
 * @retval #YACA_ERROR_INTERNAL Internal error
 *
 * @see yaca_key_destroy()
 * @see yaca_key_generate()
 * @see yaca_key_extract_parameters()
 */
int yaca_key_generate_from_parameters(const yaca_key_h params, yaca_key_h *prv_key);

/**
 * @brief  Extracts public key from a private one.
 *
 * @since_tizen 3.0
 *
 * @remarks  The @a pub_key should be released using yaca_key_destroy()
 *
 * @param[in]  prv_key  Private key to extract the public one from
 * @param[out] pub_key  Extracted public key
 *
 * @return #YACA_ERROR_NONE on success, negative on error
 * @retval #YACA_ERROR_NONE Successful
 * @retval #YACA_ERROR_INVALID_PARAMETER @a prv_key is of invalid type or @a pub_key is NULL
 * @retval #YACA_ERROR_OUT_OF_MEMORY Out of memory error
 * @retval #YACA_ERROR_INTERNAL Internal error
 *
 * @see yaca_key_generate()
 * @see yaca_key_import()
 * @see yaca_key_destroy()
 */
int yaca_key_extract_public(const yaca_key_h prv_key, yaca_key_h *pub_key);

/**
 * @brief  Extracts parameters from a private or a public key.
 *
 * @since_tizen 3.0
 *
 * @remarks  The @a params_key should be released using yaca_key_destroy()
 *
 * @param[in]  key      A key to extract the parameters from
 * @param[out] params   Extracted parameters
 *
 * @return #YACA_ERROR_NONE on success, negative on error
 * @retval #YACA_ERROR_NONE Successful
 * @retval #YACA_ERROR_INVALID_PARAMETER @a key is of invalid type or @a params is NULL
 * @retval #YACA_ERROR_OUT_OF_MEMORY Out of memory error
 * @retval #YACA_ERROR_INTERNAL Internal error
 *
 * @see yaca_key_generate()
 * @see yaca_key_generate_from_parameters()
 * @see yaca_key_import()
 * @see yaca_key_destroy()
 */
int yaca_key_extract_parameters(const yaca_key_h key, yaca_key_h *params);

/**
 * @brief  Release the key created by the library. Passing YACA_KEY_NULL is allowed.
 *
 * @since_tizen 3.0
 *
 * @param[in,out] key  Key to be released
 *
 * @see yaca_key_import()
 * @see yaca_key_export()
 * @see yaca_key_generate()
 */
void yaca_key_destroy(yaca_key_h key);

/**
 * @brief  Derives a key using Diffie-Helmann or EC Diffie-Helmann key exchange protocol.
 *
 * @since_tizen 3.0
 *
 * @remarks  The @a sym_key should be released using yaca_key_destroy()
 *
 * @param[in]  prv_key  Our private key
 * @param[in]  pub_key  Peer public key
 * @param[out] sym_key  Shared secret, that can be used as a symmetric key
 *
 * @return #YACA_ERROR_NONE on success, negative on error
 * @retval #YACA_ERROR_NONE Successful
 * @retval #YACA_ERROR_INVALID_PARAMETER Required parameters have incorrect values
 * @retval #YACA_ERROR_OUT_OF_MEMORY Out of memory error
 * @retval #YACA_ERROR_INTERNAL Internal error
 *
 * @see yaca_key_destroy()
 */
int yaca_key_derive_dh(const yaca_key_h prv_key,
                       const yaca_key_h pub_key,
                       yaca_key_h *sym_key);

/**
 * @brief  Derives a key from user password (PKCS #5 a.k.a. pbkdf2 algorithm).
 *
 * @since_tizen 3.0
 *
 * @remarks  The @a key should be released using yaca_key_destroy()
 *
 * @param[in]  password     User password as a NULL-terminated string
 * @param[in]  salt         Salt, should be a non-empty string
 * @param[in]  salt_len     Length of the salt
 * @param[in]  iterations   Number of iterations
 * @param[in]  algo         Digest algorithm that should be used in key generation
 * @param[in]  key_bit_len  Length of a key (in bits) to be generated
 * @param[out] key          Newly generated key
 *
 * @return #YACA_ERROR_NONE on success, negative on error
 * @retval #YACA_ERROR_NONE Successful
 * @retval #YACA_ERROR_INVALID_PARAMETER Required parameters have incorrect values (NULL, 0,
 *                                       invalid algo or key_bit_len not dividable by 8)
 * @retval #YACA_ERROR_OUT_OF_MEMORY Out of memory error
 * @retval #YACA_ERROR_INTERNAL Internal error
 *
 * @see #yaca_digest_algorithm_e
 * @see yaca_key_destroy()
 */
int yaca_key_derive_pbkdf2(const char *password,
                           const char *salt,
                           size_t salt_len,
                           size_t iterations,
                           yaca_digest_algorithm_e algo,
                           size_t key_bit_len,
                           yaca_key_h *key);

/**
  * @}
  */

#ifdef __cplusplus
} /* extern */
#endif

#endif /* YACA_KEY_H */
