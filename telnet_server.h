#ifndef telnet_server
#define telnet_server

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <arpa/telnet.h>

#include "curl_handle.h"

enum {
    RES_SUCCESS         = 0,
    RES_NOT_SUPPORT     = 1,
    RES_PARAMETER_ERROR = 2,
    RES_NOT_EXIST       = 3,
    RES_BUSY            = 4,
    RES_NO_SPACE        = 5,
    RES_ERROR_PLATFORM  = 6,
    RES_SEND_ERROR      = 7,
};

#define PORT 23
#define BUFFER_SIZE 1024

struct TelnetServer {
	//typedef TelnetServer self_t;

	pthread_t m_thrMonitor;

	struct sockaddr_in m_ServerAddr;
	struct sockaddr_in m_ClientAddr;
	socklen_t m_ClientAddrLen;

	char pszBuffer[BUFFER_SIZE];

	bool m_bReceiveData;

	uint nConnectTimes;

	explicit TelnetServer();

	~TelnetServer();

	void init();

	int Main();

	void receive_client_data();

public:
	int m_nServerSocket;
	int m_nClientSocket;

};

#endif