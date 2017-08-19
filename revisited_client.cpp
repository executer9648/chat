#include <stdio.h>	
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#ifndef DEFPORT	
#define DEFPORT					(27015)
#endif // !DEFPORT	

#ifndef MAX_RECV_LEN
#define MAX_RECV_LEN			(256)
#endif // !MAX_RECV_LEN

#ifndef SUCCESS
#define SUCCESS					(0)
#endif // !SUCCESS

#ifndef DEF_EVENTS
#define DEF_EVENTS				(1)
#endif // !DEF_EVENTS

#ifndef MAX_EVENTS
#define MAX_EVENTS				(2)
#endif // !MAX_EVENTS

#ifndef FAIL
#define FAIL					(2)
#endif // !FAIL

#ifndef VERSION
#define VERSION					(2)
#endif // !VERSION

#ifndef AGAIN
#define AGAIN					(3)
#endif // !AGAIN

#ifndef CONNECT_CMD
#define CONNECT_CMD				("/c")
#endif // !CONNECT_CMD

#ifndef EXIT
#define EXIT					("/q")
#endif // !EXIT

#ifndef MY_ERROR
#define MY_ERROR				("e")
#endif // !MY_ERROR

#ifndef RECV_HANDLE_INDEX
#define RECV_HANDLE_INDEX		(0)
#endif // !RECV_HANDLE_INDEX

#ifndef SHUT_DONW_EVENT_INDEX
#define SHUT_DOWN_EVENT_INDEX	(1)
#endif // !SHUT_DONW_EVENT_INDEX

typedef struct _CLIENT 
{
	SOCKET connect_socket;
	sockaddr_in sockaddrin;
	HANDLE recvt;
}CLIENT;

//global varibales
CLIENT global_client;
WSAEVENT shut_down_event;

// function that makes a connection to the socket
int connect_socket(SOCKET socket, sockaddr_in sockaddrin) 
{
	int result = 0;
	result = connect(socket, (sockaddr*)&sockaddrin, sizeof(sockaddrin));
	if (result == SOCKET_ERROR) 
	{
		if (WSAGetLastError() == WSAEADDRNOTAVAIL)
			printf("The requested address is not valid in its context.\n");
		return FAIL;
	}

	return SUCCESS;
}

void print_help()
{
	printf("///////////////////// COMMANDS  ////////////////////////////////\n");
	printf("/c:IP ============ connect to a server with the address (IP) \n");
	printf("/s:YOUR MESSAGE == sends a message\n");
	printf("/j:ROOM NAME ===== joins or creates a room \n");
	printf("/r =============== list all rooms \n");
	printf("/l =============== leaves your current room\n");
	printf("/d =============== disconnect from the server\n");
	printf("/q =============== close client program\n");
	printf("**you cant leave the default room**\n");
	printf("**if you are alone in the room the server will echo the message**\n");
	printf("/////////////////////////////////////////////////////////////////\n");
}

void user_input(char * msg)
{
	int ch = 0;

	fgets(msg, MAX_RECV_LEN, stdin);
	if (!strchr(msg, '\n')) 
	{
		do
		{
			ch = getchar();
		} while (ch != '\n');
		printf("command to long\n");
		strcpy(msg, MY_ERROR);
	}
}

// function that stores the desired ip for the connection if the user inputed /c:IP
int listen_to_commands() 
{
	int result = 0;
	char msg[MAX_RECV_LEN] = { 0 };
	char *cmd = NULL;

	user_input(msg);
	if (!strcmp(msg, MY_ERROR))
		return AGAIN;

	strtok(msg, "\n");

	cmd = strtok(msg, ":");
	if (!strcmp(cmd, CONNECT_CMD))
	{
		cmd = strtok(NULL, ":");
		if (cmd == NULL)
		{
			printf("no IP was inputed\n");
			return AGAIN;
		}
		global_client.sockaddrin.sin_addr.s_addr = inet_addr(cmd);

		printf("connecting to server\n");

		result = connect_socket(global_client.connect_socket, global_client.sockaddrin);
		if (result == FAIL)
		{
			printf("connection failed:%d\n",WSAGetLastError());
			return AGAIN;
		}

		printf("connection established\n");

		return SUCCESS;
	}
	else if (!strcmp(cmd, EXIT))
	{
		return FAIL;
	}
	else
	{
		printf("invalid command\n");
		return AGAIN;
	}
}

// Initializes winsock
int initialize_winsock() 
{
	int result = 0;
	WSADATA wsdata;
	WORD ver = MAKEWORD(VERSION, VERSION);
	result = WSAStartup(ver, &wsdata);
	if (result != SUCCESS) {
		printf("WSAStartup failed with error: %d\n", result);
		return FAIL;
	}
	return SUCCESS;
}

// function that fills the sockaddr_in struct with data for the socket connection 
void fill_socket(sockaddr_in *sockaddrin) 
{
	sockaddrin->sin_family = AF_INET;
	sockaddrin->sin_port = htons(DEFPORT);
}

// a thread for this client that only recives data
DWORD WINAPI recv_thread(LPVOID  none) 
{
	int recv_len = 0;
	int index = 0;
	char recv_box[MAX_RECV_LEN] = { 0 };
	
	WSANETWORKEVENTS result;
	WSAEVENT wsaevent[MAX_EVENTS];

	wsaevent[RECV_HANDLE_INDEX] = WSACreateEvent();
	if (wsaevent[RECV_HANDLE_INDEX] == WSA_INVALID_EVENT)
	{
		printf("create event failed %d", WSAGetLastError());
		ExitThread(0);
	}
	wsaevent[SHUT_DOWN_EVENT_INDEX] = shut_down_event;

	if (WSAEventSelect(global_client.connect_socket, wsaevent[RECV_HANDLE_INDEX], FD_READ) != 0)
	{
		printf("error in the event select:%d",WSAGetLastError());
		ExitThread(0);
	}
	
	while (TRUE) 
	{
		index = WSAWaitForMultipleEvents(MAX_EVENTS, wsaevent, FALSE, WSA_INFINITE, TRUE);
		if (index - WSA_WAIT_EVENT_0 == SHUT_DOWN_EVENT_INDEX)
			break;

		WSAEnumNetworkEvents(global_client.connect_socket, wsaevent[RECV_HANDLE_INDEX], &result);
			
		memset(recv_box, 0, MAX_RECV_LEN);
		if (result.lNetworkEvents & FD_READ) 
		{
			recv_len = recv(global_client.connect_socket, recv_box, MAX_RECV_LEN, 0);

			if (recv_len < 0) 
			{
				printf("error reciving with ERROR ID: %d\n", WSAGetLastError());
				continue;
			}
			
			printf("%s\n", recv_box);
		}
	}
	ExitThread(0);
}

// function that arranges a connection and when a creation is made the recive thread is activated
int create_socket_connection()
{
	int result = 0;

	global_client.connect_socket = NULL;
	fill_socket(&global_client.sockaddrin);

	printf("to connect use the /c:IP command\n");

	global_client.connect_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (global_client.connect_socket == INVALID_SOCKET)
	{
		printf("socket failed with error: %d\n", WSAGetLastError());
		return FAIL;
	}

	do
	{
		result = listen_to_commands();
	}while (result == AGAIN);

	if (result == FAIL)
	{
		closesocket(global_client.connect_socket);
		return FAIL;
	}
	
	WSAResetEvent(shut_down_event);

	global_client.recvt = CreateThread(NULL, 0, recv_thread, (LPVOID)NULL, 0, NULL);
	if (global_client.recvt == NULL)
	{
		closesocket(global_client.connect_socket);
		printf("failed to create a recv thread for the client:%d\n",GetLastError());
		return FAIL;
	}

	return SUCCESS;
}

/*
a thread that only sends data to the server
if the user(client) inputs /q this function terminates and the client exits
if the user(client) inputs /d this function waits for new input for a new connection
*/
void send_function() 
{
	int result = 0;
	char msg[MAX_RECV_LEN] = { 0 };
	BOOL bresult = FALSE;
	while (TRUE) 
	{
		user_input(msg);
		if (!strcmp(msg, MY_ERROR))
			continue;

		strtok(msg, "\n");

		if (strlen(msg) == 1)
		{
			printf("invalid command\n");
			continue;
		}
			

		result = send(global_client.connect_socket, msg, (int)strlen(msg), 0);
		if (result <0)
			printf("socket send failed with error: %d\n", WSAGetLastError());

		if (!strcmp(msg, "/d"))
		{
			bresult = WSASetEvent(shut_down_event);
			if (bresult == FALSE)
			{
				printf("signaling the event failed:%d\n",WSAGetLastError());
			}
			WaitForSingleObject(global_client.recvt, INFINITE);
			closesocket(global_client.connect_socket);
			result = create_socket_connection();
			if (result == FAIL)
				break;
		}

		else if (!strcmp(msg, "/q")) 
		{
			break;
		}
	}
	bresult = WSASetEvent(shut_down_event);
	if (bresult == FALSE)
	{
		printf("signaling the event failed:%d\n", WSAGetLastError());
	}
	WaitForSingleObject(global_client.recvt, INFINITE);
}

int main(int agrc, char agrv[])
{
	int result = 0;

	shut_down_event = WSACreateEvent();
	if (shut_down_event == WSA_INVALID_EVENT)
	{
		printf("create event failed %d", WSAGetLastError());
		return FAIL;
	}

	printf("Hello user(client)\n");
	print_help();

	result = initialize_winsock();
	if (result == FAIL)
	{
		return FAIL;
	}
		
	result = create_socket_connection();
	if (result == FAIL)
	{
		WSACleanup();
		return FAIL;
	}
		
	send_function();

	closesocket(global_client.connect_socket);
	WSACleanup();

	return SUCCESS;
}