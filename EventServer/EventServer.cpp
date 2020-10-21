// EventServer.cpp : Event-based server example - By Gaz Robinson 2020
//

#define WIN32_LEAN_AND_MEAN
#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "NetUtility.h"

#define SERVERIP "127.0.0.1"
#define SERVERPORT 5555
#define MESSAGESIZE 40

//TODO: Boy, having a socket variable for each client will get tiring fast.
// It would be nice to have a nicer system for this...
SOCKET ListenSocket, AcceptSocket;
SOCKET sockets[64];
WSAEVENT ListenEvent, AcceptEvent;
WSAEVENT events[64];

//TODO add a 2D vector of sockest & events

int eventCount = 0;
int clientCount = 0;

//Structure to hold the result from WSAEnumNetworkEvents
WSANETWORKEVENTS NetworkEvents;

//Prototypes
void CleanupSocket(int index);

int main()
{
	StartWinSock();

	printf("Server starting\n");

	//Build socket address structure for binding the socket
	sockaddr_in InetAddr;
	InetAddr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &(InetAddr.sin_addr));
	InetAddr.sin_port = htons(SERVERPORT);

	//Create our TCP server/listen socket
	ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ListenSocket == INVALID_SOCKET) {
		die("socket failed");
	}
	sockets[eventCount] = ListenSocket;


	// Bind the server socket to its address.
	if (bind(ListenSocket, (SOCKADDR*)&InetAddr, sizeof(InetAddr)) != 0) {
		die("bind failed");
	}

	// Create a new event for checking listen socket activity
	ListenEvent = WSACreateEvent();
	if (ListenEvent == WSA_INVALID_EVENT) {
		die("server event creation failed");
	}
	//Assosciate this event with the socket types we're interested in
	//In this case, on the server, we're interested in Accepts and Closes
	WSAEventSelect(ListenSocket, ListenEvent, FD_ACCEPT | FD_CLOSE);
	events[eventCount] = ListenEvent;

	//Start listening for connection requests on the socket
	if (listen(ListenSocket, 1) == SOCKET_ERROR) {
		die("listen failed");
	}

	eventCount++;

	printf("Listening on socket...\n");

	//Which event has activity?
	int eventIndex = 0;

	while (true) {
		//Check our events for activity. 
		//Params: How many events, pointer to an array of events, should we wait on ALL events being ready?, how long should we wait?, ignore this one for now...
		//At the moment we're using a timeout of 0 to 'poll' for activity, we could move this to a seperate thread and let it block there to make it more efficient!
		//Here we check for events on the ListenEvent		
		DWORD returnVal;

			//for only 1 client use this
			//returnVal = WSAWaitForMultipleEvents(1, &ListenEvent, true, 0, false);
		
			returnVal = WSAWaitForMultipleEvents(eventCount, events, false, 0, false);

		if ((returnVal != WSA_WAIT_TIMEOUT) && (returnVal != WSA_WAIT_FAILED)) {
			eventIndex = returnVal - WSA_WAIT_EVENT_0; //In practice, eventIndex will equal returnVal, but this is here for compatability

			if (WSAEnumNetworkEvents(sockets[eventIndex], events[eventIndex], &NetworkEvents) == SOCKET_ERROR) {
				die("Retrieving event information failed");
			}
			if (NetworkEvents.lNetworkEvents & FD_ACCEPT)
			{
				if (NetworkEvents.iErrorCode[FD_ACCEPT_BIT] != 0) {
					printf("FD_ACCEPT failed with error %d\n", NetworkEvents.iErrorCode[FD_ACCEPT_BIT]);
					break;
				}
				// Accept a new connection, and add it to the socket and event lists
				AcceptSocket = accept(sockets[eventIndex], NULL, NULL);
				AcceptEvent = WSACreateEvent();
				//TODO: It'd be great if we could wait for a Read or Write event too...
				WSAEventSelect(AcceptSocket, AcceptEvent,FD_READ | FD_WRITE | FD_CLOSE);

				//TODO check for errors on accept

				sockets[eventCount] = AcceptSocket;
				events[eventCount] = AcceptEvent;
				eventCount++;			
				clientCount++;

				printf("Socket %d connected\n", AcceptSocket);
			}
			if (NetworkEvents.lNetworkEvents & FD_CLOSE)
			{
				//We ignore the error if the client just force quit
				if (NetworkEvents.iErrorCode[FD_CLOSE_BIT] != 0 && NetworkEvents.iErrorCode[FD_CLOSE_BIT] != 10053)
				{
					printf("FD_CLOSE failed with error %d\n", NetworkEvents.iErrorCode[FD_CLOSE_BIT]);
					break;
				}
				CleanupSocket(eventIndex);
			}

			if (NetworkEvents.lNetworkEvents & FD_READ)
			{
				//read something
			}

			//warning FD_WRITE responds as soon as it can write something
			//come up with a system to check if it SHOULD write something
			if (NetworkEvents.lNetworkEvents & FD_WRITE)
			{
				//write something
			}
		}
		else if (returnVal == WSA_WAIT_TIMEOUT) {
			//All good, we just have no activity
		}
		else if (returnVal == WSA_WAIT_FAILED) {
			die("WSAWaitForMultipleEvents failed!");
		}
			
		
	}
}

void CleanupSocket(int index) {

	if (closesocket(sockets[index]) != SOCKET_ERROR) {
		printf("Successfully closed socket %d\n", AcceptSocket);
	}
	if (WSACloseEvent(events[index]) == false) {
		die("WSACloseEvent() failed");
	}
	clientCount = 0;

	//TODO tidy up arrays, move everyone up 1 position to fill in dead space
}