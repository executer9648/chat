#include <stdio.h>	
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#ifndef DEFPORT
#define DEFPORT				(27015)
#endif // !DEFPORT

#ifndef MAX_RECV_LEN
#define MAX_RECV_LEN		(256)
#endif // !MAX_RECV_LEN

#ifndef MAX_ROOM_NAME
#define MAX_ROOM_NAME		(16)
#endif // !MAX_ROOM_NAME

#ifndef DEF_ROOM_ID
#define DEF_ROOM_ID			(0)
#endif // !DEF_ROOM_ID

#ifndef DEF_FLAG
#define DEF_FLAG			(0)
#endif // !DEF_FLAG

#ifndef SUCCESS
#define SUCCESS				(0)
#endif // !SUCCESS

#ifndef DEF_PROTOCAL
#define DEF_PROTOCAL        (0)
#endif // !DEF_PROTOCAL

#ifndef DOESNT_EXIT
#define DOESNT_EXIST		(0)
#endif // !DOESNT_EXIT

#ifndef EXISTS
#define EXISTS				(1)
#endif // !EXISTS

#ifndef FAIL
#define FAIL				(1)
#endif // !FAIL

#ifndef DEF_EVENT_AMOUNT
#define DEF_EVENT_AMOUNT	(1)
#endif // !DEF_EVENT_AMOUNT

#ifndef VERSION
#define VERSION				(2)
#endif // !VERSION

#ifndef MAX_USER_NAME
#define MAX_USER_NAME		(16)
#endif // !1

#ifndef CMD_INVALID
#define CMD_INVALID			("/")
#endif // !CMD_INVALID

#ifndef CMD_SEND
#define CMD_SEND			("/s")
#endif // !CMD_SEND

#ifndef CMD_DISCONNECT
#define CMD_DISCONNECT		("/d")
#endif // !CMD_DISCONNECT

#ifndef CMD_JOIN_ROOM
#define CMD_JOIN_ROOM		("/j")
#endif // !CMD_JOIN_ROOM

#ifndef CMD_QUIT
#define CMD_QUIT			("/q")
#endif // !CMD_QUIT

#ifndef CMD_LEAVE
#define CMD_LEAVE			("/l")
#endif // !CMD_LEAVE

#ifndef CMD_ROOMS
#define CMD_ROOMS			("/r")
#endif // !CMD_ROOMS

#ifndef EMPTY
#define EMPTY				("")
#endif // !EMPTY

enum CMD 
{
	CMD_SENDC = 0,
	CMD_JOINC,
	CMD_ROOMSC,
	CMD_LEAVEC,
	CMD_DISCONNECTC,
	CMD_INVALIDC
};

typedef struct _CLIENT_PEER 
{
	SOCKET client_socket;
	
	HANDLE client_thread_handle;
	
	struct _CLIENT_PEER * next_client;
	struct _CLIENT_PEER * next_in_room;
	
	int client_id;
	int room_id;
	
	char room_name[MAX_ROOM_NAME];
	char user_name[MAX_USER_NAME];
	
	struct _ROOM * my_room;
}CLIENT_PEER;

typedef struct	_ROOM 
{
	struct _CLIENT_PEER * room_client_head;
	
	struct _ROOM * next_room;
	
	int room_id;
	
	char room_name[MAX_ROOM_NAME];
}ROOM;

typedef struct _SERVER 
{
	SOCKET listen_socket;
	
	CLIENT_PEER * client_head;
	
	ROOM * room_head;
	
	CRITICAL_SECTION critical_object;
}SERVER;

typedef struct _MESSAGE 
{
	int cmd;
	char room_name[MAX_ROOM_NAME];
	char send_box[MAX_RECV_LEN];
}MESSAGE;

//GLOABL VARIABLES//
static SERVER my_server;
static ROOM def_room;
int client_amount = 0;

// initializes a server struct (global)
void initialize_my_server()
{
	my_server.client_head = NULL;
	my_server.room_head = &def_room;
	my_server.listen_socket = INVALID_SOCKET;
}

// initializes a room struct (global def)room 0
void initialize_def_room()
{
	def_room.room_id = 0;
	def_room.room_client_head = NULL;
	def_room.next_room = NULL;
	
	strcpy(def_room.room_name, "default room");
}

// initializes the message struct
void initialize_message(MESSAGE * msg) 
{
	msg->cmd = 0;
	memset(msg->room_name, 0, MAX_RECV_LEN);
	memset(msg->send_box, 0, MAX_RECV_LEN);
}

// function that removes the client from the all client list
void remove_client_from_server(CLIENT_PEER * client) 
{
	CLIENT_PEER * past = NULL;
	CLIENT_PEER * present = NULL;

	if (!my_server.client_head)
	{
		return;
	}
		

	if (my_server.client_head == client) 
	{
		my_server.client_head = my_server.client_head->next_client;
		return;
	}

	present = my_server.client_head;
	while ( (present->next_client) && (present != client) )
	{
		
		past = present;
		present = present->next_client;
	}

	past->next_client = present->next_client;
}

// function that removes the client from the current room list
// this function assumes there is a client in the room 
// if there wasnt then the room shouldnt exist
void remove_client_from_room(CLIENT_PEER * client) 
{
	CLIENT_PEER * past = NULL;
	CLIENT_PEER * present = NULL;
	ROOM * current_room = NULL;

	current_room = client->my_room;
	present = current_room->room_client_head;

	if (current_room->room_client_head == client)
	{
		current_room->room_client_head = current_room->room_client_head->next_in_room;
		return;
	}

	while ( (present->next_in_room) && (present != client) )
	{
		past = present;
		present = present->next_in_room;
	}
	past->next_in_room = present->next_in_room;
}

// function that removes and free's the target room
void free_room(ROOM * target)
{
	ROOM * present = my_server.room_head;
	ROOM * past = NULL;

	if (my_server.room_head == target)
	{
		my_server.room_head = my_server.room_head->next_room;
		free(target);
		return;
	}

	while ( (present->next_room) && (present != target) )
	{
		past = present;
		present = present->next_room;
	}
	past->next_room = present->next_room;
	free(present);
}

// function that removes the client from all of the lists and free's him
void free_client_node(CLIENT_PEER * client) 
{
	ROOM * old_room = client->my_room;

	EnterCriticalSection(&my_server.critical_object);

	remove_client_from_room(client);

	//if the old room doesnt have any clients and is not the default room then delete him
	if ( (!old_room->room_client_head) && (old_room->room_id != DEF_ROOM_ID) )
	{
		free_room(old_room);
	}	

	remove_client_from_server(client);

	client_amount--;

	LeaveCriticalSection(&my_server.critical_object);

	free(client);
}

// function that according to the name either finds the room or tells me that there isnt one with that name
ROOM * check_room_name(char * name) 
{
	ROOM * current = NULL;
	
	EnterCriticalSection(&my_server.critical_object);
	
	current = my_server.room_head;
	while (current)
	{
		if (!strcmp(current->room_name, name))
		{
			LeaveCriticalSection(&my_server.critical_object);
			return current;
		}
		current = current->next_room;
	}
	
	LeaveCriticalSection(&my_server.critical_object);
	return NULL;
}

// function that tells me if the name is taken or not
int check_user_name(char * name)
{
	int counter = 0;
	CLIENT_PEER * current = NULL;
	current = my_server.client_head;
	while (current)
	{
		if (!strcmp(current->user_name, name))
		{
			return EXISTS;
		}
		current = current->next_client;
	}
	return DOESNT_EXIST;
}
//strtok is not thread safe, we cant use it in a multi-threaded system
//thats why i wrote my own version of splitting the string. 
void my_strtok(char * recv, char * first_part)
{
	int span = 0;
	span = strcspn(recv, ":");
	strncpy(first_part, recv, span);
}

void my_strtok_second(char * recv, char * second_part)
{ 
	char * message;
	message = strchr(recv, ':');
	if (message == NULL)
	{
		strcpy(second_part, EMPTY);
		return;
	}
	message++;
	strcpy(second_part, message);
}

// fucntion that listens to what command the client has inputed and according to that the command is donef
void listen_to_commands(char * recv_box, MESSAGE * msg) 
{
	char cmd[MAX_RECV_LEN] = { 0 };
	
	my_strtok(recv_box,cmd);
	initialize_message(msg);

	if (!strcmp(cmd,CMD_SEND))
	{
		my_strtok_second(recv_box,cmd);
		if (strlen(cmd)==0)
		{
			msg->cmd = CMD_INVALIDC;
			strcpy(msg->send_box, "you didnt enter any variable");
		}
		else
		{
			msg->cmd = CMD_SENDC;
			strcpy(msg->send_box, cmd);
		}
	}
	else if (!strcmp(cmd, CMD_DISCONNECT))
	{
		msg->cmd = CMD_DISCONNECTC;
	}
	else if (!strcmp(cmd, CMD_JOIN_ROOM))
	{
		my_strtok_second(recv_box, cmd);
		if (strlen(cmd)==0)
		{
			msg->cmd = CMD_INVALIDC;
			strcpy(msg->send_box, "you didnt enter any variable");
		}
		else
		{
			msg->cmd = CMD_JOINC;
			strcpy(msg->room_name, cmd);
		}
	}
	else if (!strcmp(cmd, CMD_QUIT))
	{
		msg->cmd = CMD_DISCONNECTC;
	}
	else if (!strcmp(cmd, CMD_LEAVE))
	{
		msg->cmd = CMD_JOINC;
		strcpy(msg->room_name, "default room");
	}
	else if (!strcmp(cmd, CMD_ROOMS))
	{
		msg->cmd = CMD_ROOMSC;
	}
	else
	{
		msg->cmd = CMD_INVALIDC;
		strcpy(msg->send_box, "invalid command");
	}
}

// function that adds a room node to the room list
void add_room_node(ROOM * room, CLIENT_PEER * client)
{
	room->next_room = my_server.room_head;
	my_server.room_head = room;
	room->room_client_head = client;
}

// a thread for each client that does i/o and listening to the commands the client has sent
DWORD WINAPI client_thread (LPVOID clienter) 
{
	char recv_box[MAX_RECV_LEN] = { 0 };
	char msg[MAX_RECV_LEN] = { 0 };
	char room_names[MAX_RECV_LEN] = { 0 };
	int result = 0;
	CLIENT_PEER * client = NULL;
	CLIENT_PEER * current_client = NULL;
	ROOM * clients_room = NULL;
	ROOM * new_room = NULL;
	ROOM * old_room = NULL;
	MESSAGE msge;
	WSANETWORKEVENTS wsaresult;
	BOOL is_user_name_set = FALSE;

	client = (CLIENT_PEER * )clienter;
	WSAEVENT wsaevent[DEF_EVENT_AMOUNT] = { WSACreateEvent() };
	if (wsaevent[0] == WSA_INVALID_EVENT)
	{
		printf("create event failed %d", WSAGetLastError());
		ExitThread(0);
	}

	result = WSAEventSelect(client->client_socket, wsaevent[0], FD_READ | FD_CLOSE);
	if (result != SUCCESS)
	{
		printf("event select error %d\n", GetLastError());
		ExitThread(0);
	}

	strcpy(msg, "enter username with not more then 16 chars");
	while (TRUE)
	{
		// tells the client to enter a user name
		// if the chosen user name is taken then it will send him that the username is taken
		if (is_user_name_set == FALSE) 
		{
			send(client->client_socket, msg, strlen(msg), DEF_FLAG);
		}									

		memset(recv_box, 0, MAX_RECV_LEN);
		memset(room_names, 0, MAX_RECV_LEN);
		
		WSAWaitForMultipleEvents(DEF_EVENT_AMOUNT, wsaevent, FALSE, WSA_INFINITE, FALSE);
		WSAEnumNetworkEvents(client->client_socket, wsaevent[0], &wsaresult);
		if (!wsaresult.lNetworkEvents & FD_READ) 
		{
			continue;
		}
		
		result = recv(client->client_socket, recv_box, MAX_RECV_LEN, DEF_FLAG);

		if (result < 0) 
		{
			printf("client closed connection %d\n", WSAGetLastError());
			free_client_node(client);
			break;
		}
		//if the client didnt yet set a user name then check if the user name he chose already exists 
		//if it is ,the client will get the message - user name taken! if not ,than the chosen user name is set 
		if (is_user_name_set == FALSE) 
		{									
			EnterCriticalSection(&my_server.critical_object);
			if (check_user_name(recv_box) == EXISTS) 
			{
				strcpy(msg, "username taken!");
				LeaveCriticalSection(&my_server.critical_object);
				continue;
			}
			strcpy(client->user_name, recv_box);
			is_user_name_set = TRUE;
			LeaveCriticalSection(&my_server.critical_object);
			strcpy(msg, "username valid");
			send(client->client_socket, msg, strlen(msg), DEF_FLAG);
			continue;
		}

		listen_to_commands(recv_box,&msge);//checks the command the user inputed

		switch (msge.cmd)
		{
		case CMD_DISCONNECTC:
			free_client_node(client);
			goto end_while;

		case CMD_ROOMSC:
			new_room = my_server.room_head;

			while (new_room)
			{
				strcat(room_names, new_room->room_name);
				if (new_room->next_room)
					strcat(room_names, "\n");
				new_room = new_room->next_room;
			}

			result = send(client->client_socket, room_names, strlen(room_names), DEF_FLAG);
			if (result < 0)
				printf("error sending the room list %d", WSAGetLastError());

			break;

		case CMD_JOINC:
			new_room = check_room_name(msge.room_name);//checks if a room already exists 

			if (!new_room)//if room doesnt exist then it is created and entered
			{
				printf("creating a new room\n");

				new_room = (ROOM * ) malloc(sizeof(ROOM));
				if (new_room == NULL)
				{
					printf("malloc failed\n");
					printf("NOT ENOUGH MEMROY\n");
					printf("coudnt create a room\n");
					break;
				}
				strcpy(new_room->room_name, msge.room_name);

				old_room = client->my_room;

				EnterCriticalSection(&my_server.critical_object);

				remove_client_from_room(client);

				if ( (!old_room->room_client_head) && old_room->room_id != 0)//deletes the old client room if there isnt anyone there
					free_room(old_room);

				strcpy(client->room_name, msge.room_name);
				client->next_in_room = NULL;
				client->my_room = new_room;

				add_room_node(new_room, client);

				LeaveCriticalSection(&my_server.critical_object);

				printf("the new room created %s \n", msge.room_name);
			}
			else//if the room exists then it is entered
			{
				printf("the room found\n");
				old_room = client->my_room;

				EnterCriticalSection(&my_server.critical_object);

				remove_client_from_room(client);

				if ( (!old_room->room_client_head) && old_room->room_id != 0)//deletes the old client room if there isnt anyone there
					free_room(old_room);

				strcpy(client->room_name, msge.room_name);
				client->next_in_room = new_room->room_client_head;
				client->my_room = new_room;

				new_room->room_client_head = client;

				LeaveCriticalSection(&my_server.critical_object);
			}
			break;

		case CMD_SENDC:
			current_client = client->my_room->room_client_head;
			strcat(msge.send_box, " #sent by: ");
			strcat(msge.send_box, client->user_name);
			do 
			{     //if there isnt anyone in the room except myself then echo my message
				if (!client->my_room->room_client_head->next_in_room) 
				{
					result = send(client->client_socket, msge.send_box, (int)strlen(msge.send_box), DEF_FLAG);
					if (result == FAIL)
					{
						printf("error sending %d", WSAGetLastError());
						continue;
					}
					current_client = current_client->next_in_room;
					break;
				}

				if (current_client->client_id == client->client_id) 
				{
					current_client = current_client->next_in_room;
					continue;
				}

				result = send(current_client->client_socket, msge.send_box, (int)strlen(msge.send_box), DEF_FLAG);
				if (result == FAIL)
				{
					printf("error sending %d", WSAGetLastError());
					continue;
				}

				current_client = current_client->next_in_room;
			} while (current_client);
			break;

		default:
			result = send(client->client_socket, msge.send_box, (int)strlen(msge.send_box), DEF_FLAG);
			if (result == FAIL)
			{
				printf("error sending %d", WSAGetLastError());
				continue;
			}
		}
	}
end_while:
	ExitThread(0);
}
// function that adds the client to the client list
void add_client_node(CLIENT_PEER * client)
{
	client->next_client = my_server.client_head;
	client->next_in_room = def_room.room_client_head;

	my_server.client_head = client;
	def_room.room_client_head = client;

}

// initializes a new client peer struct that represents a new client that is being connected
CLIENT_PEER * create_client_peer(SOCKET socket) 
{
	CLIENT_PEER * client = NULL;

	client = (CLIENT_PEER * ) malloc(sizeof(CLIENT_PEER));
	if (!client)
	{
		printf("malloc failed\n");
		printf("NOT ENOUGH MEMROY\n");
		return NULL;
	}

	EnterCriticalSection(&my_server.critical_object);

	//initializes a new client peer struct 
	client->client_socket = socket;
	client->client_id = client_amount + 1;
	client_amount++;
	client->room_id = DEF_ROOM_ID;
	client->my_room = &def_room;
	
	strcpy(client->room_name, def_room.room_name);
	memset(client->user_name, 0, MAX_USER_NAME);

	add_client_node(client);

	LeaveCriticalSection(&my_server.critical_object);

	return client;
}

// a loop that will constantly look for new client connection and will create each client a thread
// the new client is by def going to be in room 0
void accept_loop() 
{
	SOCKET client_socketir = INVALID_SOCKET;
	CLIENT_PEER * result = NULL;

	while (TRUE)
	{//start of the while
		client_socketir = accept(my_server.listen_socket, NULL, NULL);
		if (client_socketir == INVALID_SOCKET)
		{
			printf("accept faild with %d\n", WSAGetLastError());
			continue;
		}

		result = create_client_peer(client_socketir);//creates a client peer struct

		if (!result)
		{
			closesocket(client_socketir);
			continue;
		}

		my_server.client_head->client_thread_handle = CreateThread(NULL, 0, client_thread, (LPVOID)result, 0, NULL);

		if (!my_server.client_head->client_thread_handle)
		{
			printf("error creating client thread %d\n", GetLastError());
			closesocket(result->client_socket);
			free_client_node(result);
		}

	}// end of the while
}

// initializes winsock
int initializewinsock() 
{
	int result = 0;
	WSADATA wsdata;
	WORD ver = MAKEWORD(VERSION, VERSION);
	result = WSAStartup(ver, &wsdata);
	if (result != SUCCESS) 
	{
		printf("WSAStartup failed with error: %d\n", result);
		return FAIL;
	}
	return SUCCESS;
}

// binds a socket
int bindsock(SOCKET socket, sockaddr_in sockaddrin) 
{
	int result = 0;
	result = bind(socket, (sockaddr * ) &sockaddrin, sizeof(sockaddrin));
	if (result == SOCKET_ERROR) 
	{
		printf("bind failed with error: %ld\n", WSAGetLastError());
		return FAIL;
	}
	return SUCCESS;
}

// declares a socket to be listening
int listensock(SOCKET socket) 
{
	int result = 0;
	result = listen(socket, SOMAXCONN);
	if (result == SOCKET_ERROR) 
	{
		printf("Listen failed with error: %ld\n", WSAGetLastError());
		return FAIL;
	}
	return SUCCESS;
}

// fill a sockaddr_in struct
void fill_sock(sockaddr_in * sockaddrin)
{
	sockaddrin->sin_family = AF_INET;
	sockaddrin->sin_port = htons(DEFPORT);
	sockaddrin->sin_addr.s_addr = INADDR_ANY;
}

// initializes a socket to be able to create new connections via accept
int create_server_socket()
{
	int result = 0;
	SOCKADDR_IN sockaddrin;

	fill_sock(&sockaddrin);

	my_server.listen_socket = socket(AF_INET, SOCK_STREAM, DEF_PROTOCAL);

	if (my_server.listen_socket == INVALID_SOCKET)
	{
		printf("socket failed %d", WSAGetLastError());
		return FAIL;
	}

	result = bindsock(my_server.listen_socket, sockaddrin);
	if (result == FAIL)
		goto exit;
	
	result = listensock(my_server.listen_socket);
	if (result == FAIL)
		goto exit;
	
	return SUCCESS;
exit:
	closesocket(my_server.listen_socket);
	return FAIL;
}

int main(int argc, char argv[])
{
	int result = 0;

	InitializeCriticalSection(&my_server.critical_object);

	result = initializewinsock();
	if (result == FAIL)
	{
		DeleteCriticalSection(&my_server.critical_object);
		return FAIL;
	}

	initialize_def_room();
	
	initialize_my_server();
	
	if (create_server_socket() != FAIL)
	{
		accept_loop();
	}
	
	DeleteCriticalSection(&my_server.critical_object);
	WSACleanup();

	return SUCCESS;
}
