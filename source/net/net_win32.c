#include <net/internal.h>
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <core/log.h>

boolean init_internal()
{
	WSADATA wsa;
	int r;
	r = WSAStartup(MAKEWORD(2, 2), &wsa);
	if (r != NO_ERROR) {
		ERROR("WSAStartup() failed with %d", r);
		return FALSE;
	}
	return TRUE;
}
