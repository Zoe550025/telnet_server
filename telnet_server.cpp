#include "telnet_server.h"

const char *TAG = "Server";
#include "Log.hpp"

#define USERNAME "admin"
#define PASSWORD "0000"


TelnetServer* g_Server = NULL;

void signal_handle(int sig) {
    LOGI("[%s] - Signal Handle %d", __func__, sig);

    if (sig == SIGINT) {
        close(g_Server->m_nClientSocket);
        LOGI("[%s]close client socket", __func__);

        close(g_Server->m_nServerSocket);
        LOGI("[%s]close server socket", __func__);
    } else {
        LOGI("[%s] - Exception Handling", __func__);
    }
}

void* on_monitor_client(void *arg) {
	int *client_socket = (int *)arg;

    while (1) {
        // check client connection status
        sleep(1);

        if (recv(*client_socket, NULL, 0, MSG_PEEK | MSG_DONTWAIT) == 0) {
            printf("Client disconnected.\n");
            g_Server->m_bReceiveData = false;
            break;
        }
    }

    pthread_detach(pthread_self());

    pthread_exit(NULL);
}

TelnetServer::TelnetServer() {
	g_Server = this;

	init();

}

TelnetServer::~TelnetServer() {

}

void TelnetServer::init() {
	memset(&pszBuffer, 0, BUFFER_SIZE);

	m_bReceiveData = true;

	memset(&m_ServerAddr, 0, sizeof(m_ServerAddr));
    m_ServerAddr.sin_family = AF_INET;
    m_ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_ServerAddr.sin_port = htons(PORT);

    m_ClientAddrLen = sizeof(m_ClientAddr);

    nConnectTimes = 0;

}

int TelnetServer::Main() {
	// create socket
    if ((m_nServerSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // bind socket to the local server
    if (bind(m_nServerSocket, (struct sockaddr *)&m_ServerAddr, sizeof(m_ServerAddr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // listen
    if (listen(m_nServerSocket, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    LOGI("Listening for incoming connections on port %d...\n", PORT);

    while (true) {
        m_nClientSocket = accept(m_nServerSocket, (struct sockaddr *) &m_ClientAddr, &m_ClientAddrLen);
        if (m_nClientSocket < 0) {
            perror("ERROR on accept");
            exit(EXIT_FAILURE);
        }

        int result = pthread_create(&m_thrMonitor, NULL, on_monitor_client, (void *)&m_nClientSocket);
        if (result != 0) {
            perror("ERROR creating thread");
            exit(EXIT_FAILURE);
        }

    	send(m_nClientSocket, "Username: ", strlen("Username: "), 0);

        while (nConnectTimes < 3) {
        	memset(&pszBuffer, 0x00, sizeof(pszBuffer));

        	ssize_t bytesReceived = recv(m_nClientSocket, pszBuffer, BUFFER_SIZE, 0);

	        if (bytesReceived == -1) {
	            perror("Receive failed");
	            break;
	        } else if (bytesReceived == 0) {
	            printf("Client disconnected.\n");
	            m_bReceiveData = false;
	            break;
	        }

            if (strstr(pszBuffer, "\n") != NULL) {
            	//printf("%s--------------\n", pszBuffer);
            	remove_new_line(pszBuffer);
		        if (strcmp(pszBuffer, USERNAME) != 0) {
		            send(m_nClientSocket, "Invalid username\n", strlen("Invalid username\n"), 0);
		            nConnectTimes ++;
		            char chancesMsg[BUFFER_SIZE];
		            memset(&chancesMsg, 0x00, BUFFER_SIZE);
		            snprintf(chancesMsg, BUFFER_SIZE, "There are %d more chances\n", 3-nConnectTimes);
		            send(m_nClientSocket, chancesMsg, strlen(chancesMsg), 0);
		            send(m_nClientSocket, "Username: ", strlen("Username: "), 0);
		        }
		        else { nConnectTimes = 0; break; }
		        printf("Enter username: %s USERNAME: %s\n",pszBuffer, USERNAME);
        	}
        }

        send(m_nClientSocket, "Password: ", strlen("Password: "), 0);

    	unsigned char EchoOn[] = {IAC, WONT, TELOPT_ECHO};
    	unsigned char EchoOff[] = {IAC, WILL, TELOPT_ECHO};
    	unsigned char EchoOff2[] = {IAC, DONT, TELOPT_ECHO};
    	unsigned char EchoOn2[] = {IAC, DO, TELOPT_ECHO};

		send(m_nClientSocket, EchoOff, sizeof(EchoOff), 0);
		receive_client_data();	//Receive data to clean client socket buffer

    	nConnectTimes = 0;

    	char RecievedPassword[BUFFER_SIZE];
    	memset(&RecievedPassword, 0x00, sizeof(RecievedPassword));

    	ssize_t totalBytesReceived = 0;
    	bool lineReceived = false;

    	while (nConnectTimes < 3) {

    		while (!lineReceived && totalBytesReceived < BUFFER_SIZE - 1) {
	    		ssize_t bytesReceived = recv(m_nClientSocket, RecievedPassword + totalBytesReceived, BUFFER_SIZE - totalBytesReceived - 1, 0);
		        if (bytesReceived == -1) {
		            perror("Receive failed");
		            break;
		        } else if (bytesReceived == 0) {
		            printf("Client disconnected.\n");
		            m_bReceiveData = false;
		            break;
		        }

		        totalBytesReceived += bytesReceived;
	        	RecievedPassword[totalBytesReceived] = '\0';
	        	/*for (unsigned int i = 0; i < BUFFER_SIZE; i++) {
					printf("0x%02X ", RecievedPassword[i]);
				}*/

		        if (strstr(RecievedPassword, "\r") != NULL) {
		            lineReceived = true;
		        }

    		}

	    	if (lineReceived) {
	    		//printf("heyhey: %s\n", RecievedPassword);
	    		for (unsigned int i = 0; i < BUFFER_SIZE; i++) {
	    			if (RecievedPassword[i] != '\0')
						printf("0x%02X ", RecievedPassword[i]);
				}
	    		remove_new_line(RecievedPassword);


	    		if (strcmp(RecievedPassword, PASSWORD) != 0) {
		        	send(m_nClientSocket, EchoOn, sizeof(EchoOn), 0);
		        	receive_client_data();	//Receive data to clean client socket buffer

		            send(m_nClientSocket, "\r\nInvalid password\r\n", strlen("\r\nInvalid password\r\n"), 0);

		            nConnectTimes ++;

		            char chancesMsg[BUFFER_SIZE];
		            memset(&chancesMsg, 0x00, BUFFER_SIZE);
		            snprintf(chancesMsg, BUFFER_SIZE, "There are %d more chances\n", 3-nConnectTimes);
		            send(m_nClientSocket, chancesMsg, strlen(chancesMsg), 0);

		            printf("ask for another password\n");
		            send(m_nClientSocket, "Password: ", strlen("Password: "), 0);

		            send(m_nClientSocket, EchoOff, sizeof(EchoOff), 0);
		            receive_client_data();	//Receive data to clean client socket buffer


		            memset(&RecievedPassword, 0x00, sizeof(RecievedPassword));
		    		for (unsigned int i = 0; i < BUFFER_SIZE; i++) {
		    			if (RecievedPassword[i] != '\0')
							printf("0x%02X ", RecievedPassword[i]);
					}

		            lineReceived = false;
		            totalBytesReceived = 0;

		        }
		        else {
		        	nConnectTimes = 0;
		        	break;
		        }
	    	}
	    	else {
		        printf("Input line is too long or incomplete.\n");
		        // 清空缓冲区，准备接收下一次输入
	        	for (unsigned int i = 0; i < BUFFER_SIZE; i++) {
					printf("1234564 0x%02X ", RecievedPassword[i]);
				}
		        memset(&RecievedPassword, 0x00, sizeof(RecievedPassword));
		    }
    	}

    	printf("\ntry to echo on\n");
        send(m_nClientSocket, EchoOn, sizeof(EchoOn), 0);
        receive_client_data();	//Receive data to clean client socket buffer

        if (lineReceived && nConnectTimes == 0) {
        	send(m_nClientSocket, "\r\n\r\nLogin in!\r\n", sizeof("\r\n\r\nlogin in!\r\n"), 0);
        }
        else {
        	send(m_nClientSocket, "\r\n\r\nWrong password.\r\n", sizeof("\r\n\r\nWrong password.\r\n"), 0);

	        close(m_nClientSocket);
	        LOGI("close client socket");
	        break;
        }

        printf("7777777777777777\n");



		printf("\n8888888888888888888\n");

        char pszWelcomeMsg[BUFFER_SIZE] = "\r\n\r\nWelcome to the Telnet server!\r\n";
        send(m_nClientSocket, pszWelcomeMsg, strlen(pszWelcomeMsg), 0);

        char pszExplainationMsg[BUFFER_SIZE] = "\r\nType help to get information about the command.\r\n";
        send(m_nClientSocket, pszExplainationMsg, strlen(pszExplainationMsg), 0);

		memset(&pszBuffer, 0, sizeof(pszBuffer));
        while (m_bReceiveData) {

            //totalBytesReceived = 0;
//+totalBytesReceived- totalBytesReceived
            ssize_t bytesReceived = recv(m_nClientSocket, pszBuffer, BUFFER_SIZE , 0);
            if (bytesReceived == -1) {
                perror("Receive failed");
                break;
            } else if (bytesReceived == 0) {
                printf("Client disconnected.\n");
                m_bReceiveData = false;
                break;
            }

            //totalBytesReceived += bytesReceived;
            //pszBuffer[totalBytesReceived] = '\0';

            for (unsigned int i = 0; i < BUFFER_SIZE; i++) {
    			if (pszBuffer[i] != '\0')
					printf("0x%02X ", pszBuffer[i]);
			}

			for (ssize_t i = 0; i < bytesReceived; i++) {
		        if (pszBuffer[i] == '\r') {
		            printf("receive ENTER %s\n", pszBuffer);
		            // 处理接收到的数据
		            handle_client(pszBuffer, this);
		            break; // 可以选择跳出循环，因为已经处理了一个 ENTER
		        }
		    }

            // receive 'Enter' then do something
            /*if (strstr(pszBuffer, "\r") != NULL) {
                // return the data from the client
                if (write(m_nClientSocket, pszBuffer, strlen(pszBuffer)) < 0) {
                    perror("ERROR writing to socket");
                    exit(EXIT_FAILURE);
                }

                //handle_client(pszBuffer, this);
                printf("receive ENTER");
            }
            else {
            	printf("no enter!!!!!!");
            }*/
        }

        close(m_nClientSocket);
        LOGI("close client socket");

        m_bReceiveData = true;
    }

    close(m_nServerSocket);
    LOGI("close server socket");

    return 0;
}

void TelnetServer::receive_client_data() {
	unsigned char clientResponse[BUFFER_SIZE];
	ssize_t bytesReceived = recv(m_nClientSocket, clientResponse, BUFFER_SIZE, 0);

	if (bytesReceived > 0) {
		clientResponse[bytesReceived] = '\0';
		for (unsigned int i = 0; i < bytesReceived; i++) {
			printf("0x%02X ", clientResponse[i]);
		}
		printf("\n");
	} else if (bytesReceived == 0) {
	    printf("Client disconnected.\n");

	} else {
	    perror("Receive failed");
	}
}

int main(int argc, char *argv[]) {
	signal(SIGINT, signal_handle);

	printf("Start...\n");

	TelnetServer app;

	int ret = app.Main();

	return 0;
}
