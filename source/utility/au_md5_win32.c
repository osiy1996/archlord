#include "utility/au_md5.h"

#include "core/file_system.h"
#include "core/malloc.h"

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <Windows.h>
#include <wincrypt.h>

boolean au_md5_crypt(
	void * data,
	size_t size,
	const uint8_t * key,
	uint32_t key_size)
{
	static HCRYPTPROV prov;
	static CRITICAL_SECTION lock;
	HCRYPTHASH hash;
	HCRYPTKEY h_key;
	size_t offset = 0;
	if (!prov) {
		if (!CryptAcquireContextA(&prov, 0,
			"Microsoft Base Cryptographic Provider v1.0", 1,
			CRYPT_VERIFYCONTEXT)) {
			return FALSE;
		}
		InitializeCriticalSection(&lock);
	}
	EnterCriticalSection(&lock);
	if (!CryptCreateHash(prov, CALG_MD5, 0, 0, &hash)) {
		LeaveCriticalSection(&lock);
		return FALSE;
	}
	if (!CryptHashData(hash, key, key_size, 0)) {
		CryptDestroyHash(hash);
		LeaveCriticalSection(&lock);
		return FALSE;
	}
	if (!CryptDeriveKey(prov, CALG_RC4, hash, 4, &h_key)) {
		CryptDestroyHash(hash);
		LeaveCriticalSection(&lock);
		return FALSE;
	}
	while (size) {
		DWORD len = (DWORD)MIN(size, (size_t)MAXDWORD);
		if (!CryptDecrypt(h_key, 0, FALSE, 0, 
				(BYTE *)data + offset, &len)) {
			CryptDestroyKey(h_key);
			CryptDestroyHash(hash);
			LeaveCriticalSection(&lock);
			return FALSE;
		}
		offset += len;
		size -= len;
	}
	CryptDestroyKey(h_key);
	CryptDestroyHash(hash);
	LeaveCriticalSection(&lock);
	return TRUE;
}

boolean au_md5_copy_and_encrypt_file(const char * src_path, const char * dst_path)
{
	void * data;
	size_t size = 0;
	boolean result;
	if (!get_file_size(src_path, &size))
		return FALSE;
	data = alloc(size);
	if (!load_file(src_path, data, size)) {
		dealloc(data);
		return FALSE;
	}
	if (!au_md5_crypt(data, size, "1111", 4)) {
		dealloc(data);
		return FALSE;
	}
	result = make_file(dst_path, data, size);
	dealloc(data);
	return result;
}
