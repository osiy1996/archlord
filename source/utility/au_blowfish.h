#ifndef _AU_BLOWFISH_H_
#define _AU_BLOWFISH_H_

#include "core/macros.h"
#include "core/types.h"

BEGIN_DECLS

struct au_blowfish_state {
	uint32_t P[18]; /*!< Blowfish round keys */
	uint32_t S[4][256]; /*!< key dependent S-boxes */
};

struct au_blowfish {
	struct au_blowfish_state public_state;
	struct au_blowfish_state private_state;
	uint32_t id;
};

void au_blowfish_init(struct au_blowfish * bf);

void au_blowfish_reset(struct au_blowfish * bf);

void au_blowfish_set_public_key(
	struct au_blowfish * bf, 
	const void * key,
	uint32_t key_size);

void au_blowfish_set_private_key(
	struct au_blowfish * bf, 
	const void * key,
	uint32_t key_size);

/*
 * Encrypts packet data using public key.
 *
 * Provided buffer must be large enough (>= length + 8),
 * otherwise it will overflow.
 *
 * Encrypted length will be returned in length.
 */
void au_blowfish_encrypt_public(
	struct au_blowfish * bf, 
	void * data,
	uint16_t * length);

/*
 * Encrypts packet data using private key.
 *
 * Provided buffer must be large enough (>= length + 8),
 * otherwise it will overflow.
 *
 * Encrypted length will be returned in length.
 */
void au_blowfish_encrypt_private(
	struct au_blowfish * bf, 
	void * data,
	uint16_t * length);

/*
 * Decrypts packet data using public key.
 *
 * Decrypted length will be returned in length.
 */
boolean au_blowfish_decrypt_public(
	struct au_blowfish * bf, 
	void * data,
	uint16_t * length);

/*
 * Decrypts packet data using private key.
 *
 * Decrypted length will be returned in length.
 */
boolean au_blowfish_decrypt_private(
	struct au_blowfish * bf, 
	void * data,
	uint16_t * length);

END_DECLS

#endif /* _AU_BLOWFISH_H_ */
