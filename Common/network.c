#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "network.h"
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <windows.h>

#define MAX_EVENT_HISTORY 20

#pragma comment(lib, "Ws2_32.lib")

/* State */
static SOCKET serverSocket = INVALID_SOCKET;
static SOCKET clientSocket = INVALID_SOCKET;
static SOCKET clientSockets[MAX_CLIENTS];
static int connectedClients = 0;

static uint32_t processedEvents[MAX_EVENT_HISTORY];
static int processedEventCount = 0;

static struct sockaddr_in serverAddress;
static struct sockaddr_in clientAddress;
static int clientAddressLength = sizeof(clientAddress);
static struct sockaddr_in udpClientAddresses[MAX_CLIENTS];

static char localECUName[MAX_NAME_LENGTH];
static TransportMode transportMode = TRANSPORT_TCP;

static uint32_t nextEventID = 101;
static EventPacket lastEvent;
static EventPacket demoEvents[5];

/*  Internal helpers  */
static int IsDuplicateEvent(uint32_t eventID);
static void AddProcessedEvent(uint32_t eventID);

//server 

int NetworkManager_Initialize(void)
{
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup Failed\n");
        return -1;
    }

    if (transportMode == TRANSPORT_TCP)
    {
        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    }
    else
    {
        serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }

    if (serverSocket == INVALID_SOCKET)
    {
        printf("Socket creation failed\n");
        WSACleanup();
        return -1;
    }

    printf("Server socket created successfully.\n");

    if(transportMode == TRANSPORT_UDP)
    {
        DWORD timeout = 3000;
        setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(50000);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket,
        (struct sockaddr*)&serverAddress,
        sizeof(serverAddress)) == SOCKET_ERROR)
    {
        printf("Bind failed. Error = %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }

    printf("Server bound successfully on port 50000.\n");

    if (transportMode == TRANSPORT_TCP)
    {
        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
        {
            printf("Listen failed. Error = %d\n", WSAGetLastError());
            closesocket(serverSocket);
            WSACleanup();
            return -1;
        }

        printf("Server is listening for incoming connections...\n");
    }

    return 0;
}

/* Accepts/registers exactly one client. Does NOT generate an event —
   event generation is fully decoupled from connection handling and is
   driven by the timer loop in main.c,  */
int NetworkManager_WaitForECU(void)
{
    if (transportMode == TRANSPORT_TCP)
    {
        if (connectedClients >= MAX_CLIENTS)
        {
            SOCKET tempSocket = accept(
                serverSocket,
                (struct sockaddr*)&clientAddress,
                &clientAddressLength);

            printf("Max clients (%d) reached. Rejecting new connection.\n", MAX_CLIENTS);

            if (tempSocket != INVALID_SOCKET)
            {
                closesocket(tempSocket);
            }

            return -1;
        }

        clientSockets[connectedClients] = accept(
            serverSocket,
            (struct sockaddr*)&clientAddress,
            &clientAddressLength);

        if (clientSockets[connectedClients] == INVALID_SOCKET)
        {
            printf("Failed to accept client. Error = %d\n", WSAGetLastError());
            return -1;
        }

        printf("Client connected successfully.\n");
        connectedClients++;

        DWORD timeout = 3000; /* 3 seconds */
        setsockopt(
            clientSockets[connectedClients - 1],
            SOL_SOCKET,
            SO_RCVTIMEO,
            (const char*)&timeout,
            sizeof(timeout));

        printf("Total connected clients: %d\n", connectedClients);
    }
    else
    {
        if (connectedClients >= MAX_CLIENTS)
        {
            printf("Max clients (%d) reached. Ignoring new registration.\n", MAX_CLIENTS);
            return -1;
        }

        RegistrationPacket packet;
        int addressLength = sizeof(udpClientAddresses[connectedClients]);

        int bytesReceived = recvfrom(
            serverSocket,
            (char*)&packet,
            sizeof(RegistrationPacket),
            0,
            (struct sockaddr*)&udpClientAddresses[connectedClients],
            &addressLength);

        if (bytesReceived == SOCKET_ERROR)
        {
            printf("Failed to receive registration. Error = %d\n", WSAGetLastError());
            return -1;
        }

        printf("UDP client registered: %s\n", packet.ecuName);

        int alreadyRegistered = 0;

        for (int i = 0; i < connectedClients; i++)
        {
            if (udpClientAddresses[i].sin_addr.s_addr ==
                udpClientAddresses[connectedClients].sin_addr.s_addr &&
                udpClientAddresses[i].sin_port ==
                udpClientAddresses[connectedClients].sin_port)
            {
                alreadyRegistered = 1;
                break;
            }
        }

        if (!alreadyRegistered)
        {
            connectedClients++;
        }

        printf("Total connected clients: %d\n", connectedClients);
    }

    return 0;
}

/* Non-blocking check for a waiting client. Safe to call every loop
   iteration alongside keyboard polling and the auto-event timer —
   avoids the threading race that a background accept()/recvfrom()
   thread would create on a shared UDP socket. */
int NetworkManager_PollForECU(void)
{
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(serverSocket, &readfds);

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; /* 100 ms */

    int result = select(0, &readfds, NULL, NULL, &timeout);

    if (result > 0 && FD_ISSET(serverSocket, &readfds))
    {
        return NetworkManager_WaitForECU();
    }

    return 0;
}

//client

int NetworkManager_InitializeClient(void)
{
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup Failed\n");
        return -1;
    }

    if (transportMode == TRANSPORT_TCP)
    {
        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    }
    else
    {
        clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }

    if (clientSocket == INVALID_SOCKET)
    {
        printf("Client socket creation failed\n");
        WSACleanup();
        return -1;
    }

    printf("Client socket created successfully.\n");
    return 0;
}

int NetworkManager_ConnectToServer(void)
{
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(50000);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (transportMode == TRANSPORT_TCP)
    {
        if (connect(clientSocket,
            (struct sockaddr*)&serverAddress,
            sizeof(serverAddress)) == SOCKET_ERROR)
        {
            printf("Connection to server failed.\n");
            closesocket(clientSocket);
            WSACleanup();
            return -1;
        }
        printf("Connected to Network Event Manager successfully.\n");
    }
    else
    {
        printf("UDP mode selected.\n");
    }

    return 0;
}

int NetworkManager_RegisterClient(void)
{
    RegistrationPacket packet;

    strcpy_s(packet.ecuName, sizeof(packet.ecuName), localECUName);

    int bytesSent = sendto(
        clientSocket,
        (const char*)&packet,
        sizeof(RegistrationPacket),
        0,
        (struct sockaddr*)&serverAddress,
        sizeof(serverAddress));

    if (bytesSent == SOCKET_ERROR)
    {
        printf("Failed to register with server. Error = %d\n", WSAGetLastError());
        return -1;
    }

    printf("Client registered successfully.\n");
    return 0;
}

int NetworkManager_ReceiveEvent(void)
{
    EventPacket receivedEvent;
    int bytesReceived;

    if (transportMode == TRANSPORT_TCP)
    {
        bytesReceived = recv(
            clientSocket,
            (char*)&receivedEvent,
            sizeof(EventPacket),
            0);
    }
    else
    {
        struct sockaddr_in senderAddress;
        int senderLength = sizeof(senderAddress);

        bytesReceived = recvfrom(
            clientSocket,
            (char*)&receivedEvent,
            sizeof(EventPacket),
            0,
            (struct sockaddr*)&senderAddress,
            &senderLength);
    }

    if (bytesReceived == SOCKET_ERROR)
    {
        printf("Failed to receive event. Error = %d\n", WSAGetLastError());
        return -1;
    }

    printf("Received %d bytes.\n", bytesReceived);

    if (IsDuplicateEvent(receivedEvent.eventID))
    {
        printf("\nDuplicate event detected.\n");
        printf("Event ID %u already processed. Ignoring event.\n", receivedEvent.eventID);

        NetworkManager_SendAck(&receivedEvent);
        return 0;
    }

    AddProcessedEvent(receivedEvent.eventID);

    printf("\n========== CLIENT ==========\n");
    printf("ECU Name   : %s\n", localECUName);
    printf("New Event Received\n\n");
    printf("Event ID   : %u\n", receivedEvent.eventID);
    printf("Event Type : %s\n", NetworkManager_GetEventTypeName(receivedEvent.eventType));
    printf("Severity   : %s\n", NetworkManager_GetSeverityName(receivedEvent.severity));
    printf("Source ECU : %s\n", receivedEvent.sourceECU);
    printf("Local state updated successfully.\n");

    NetworkManager_SendAck(&receivedEvent);
    return 0;
}

int NetworkManager_SendAck(EventPacket* event)
{
    AckPacket ack;

    ack.eventID = event->eventID;
    strcpy_s(ack.ecuName, sizeof(ack.ecuName), localECUName);
    ack.success = 1;

    int bytesSent;

    if (transportMode == TRANSPORT_TCP)
    {
        bytesSent = send(
            clientSocket,
            (const char*)&ack,
            sizeof(AckPacket),
            0);
    }
    else
    {
        bytesSent = sendto(
            clientSocket,
            (const char*)&ack,
            sizeof(AckPacket),
            0,
            (struct sockaddr*)&serverAddress,
            sizeof(serverAddress));
    }

    if (bytesSent == SOCKET_ERROR)
    {
        printf("Failed to send ACK. Error = %d\n", WSAGetLastError());
        return -1;
    }

    printf("ACK sent to Network Event Manager.\n");
    printf("============================\n");
    return 0;
}

void NetworkManager_SetECUName(const char* ecuName)
{
    strcpy_s(localECUName, sizeof(localECUName), ecuName);
}

//shared

void NetworkManager_SelectTransportMode(void)
{
    int choice;

    printf("Select Transport Mode:\n");
    printf("1. TCP\n");
    printf("2. UDP\n");
    printf("Enter your choice: ");
    scanf_s("%d", &choice);

    transportMode = (choice == TRANSPORT_UDP) ? TRANSPORT_UDP : TRANSPORT_TCP;

    printf("Selected Transport Mode: %s\n",
        (transportMode == TRANSPORT_TCP) ? "TCP" : "UDP");
}

TransportMode NetworkManager_GetTransportMode(void)
{
    return transportMode;
}

int NetworkManager_GetConnectedClients(void)
{
    return connectedClients;
}

const char* NetworkManager_GetEventTypeName(EventType type)
{
    switch (type)
    {
    case EVENT_ECU_CONNECTED:      return "ECU Connected";
    case EVENT_ENGINE_START:       return "Engine Start";
    case EVENT_LOW_BATTERY:        return "Low Battery";
    case EVENT_DOOR_OPEN:          return "Door Open";
    case EVENT_DIAGNOSTIC_REQUEST: return "Diagnostic Request";
    default:                       return "Unknown";
    }
}

const char* NetworkManager_GetSeverityName(Severity severity)
{
    switch (severity)
    {
    case SEVERITY_LOW:    return "LOW";
    case SEVERITY_MEDIUM: return "MEDIUM";
    case SEVERITY_HIGH:   return "HIGH";
    default:              return "UNKNOWN";
    }
}

//Duplicate detection

static int IsDuplicateEvent(uint32_t eventID)
{
    for (int i = 0; i < processedEventCount; i++)
    {
        if (processedEvents[i] == eventID)
        {
            return 1;
        }
    }
    return 0;
}

static int nextSlotToOverwrite = 0;

static void AddProcessedEvent(uint32_t eventID)
{
    processedEvents[nextSlotToOverwrite] = eventID;
    nextSlotToOverwrite = (nextSlotToOverwrite + 1) % MAX_EVENT_HISTORY;

    if (processedEventCount < MAX_EVENT_HISTORY)
    {
        processedEventCount++;
    }
}

//event generation

   /* Generates a new, unique event (used by the periodic timer in main.c).
      Also remembers it as lastEvent so it can be replayed on demand via
      NetworkManager_ResendLastEvent() to demonstrate duplicate handling. */

void NetworkManager_CreateEvent(EventPacket* event)
{
    event->eventID = nextEventID++;
    event->eventType = (nextEventID % 4) + 1;
    event->severity = (nextEventID % 3) + 1;

    strcpy_s(event->sourceECU, sizeof(event->sourceECU), "NetworkEventManager");

    lastEvent = *event;
}

void NetworkManager_BroadcastEvent(EventPacket* event)
{
    if (transportMode == TRANSPORT_UDP)
    {
        for (int i = 0; i < connectedClients; i++)
        {
            int bytesSent = sendto(
                serverSocket,
                (const char*)event,
                sizeof(EventPacket),
                0,
                (struct sockaddr*)&udpClientAddresses[i],
                sizeof(udpClientAddresses[i]));

            if (bytesSent == SOCKET_ERROR)
            {
                printf("Failed to send UDP event. Error = %d\n", WSAGetLastError());
                continue;
            }

            printf("UDP event sent successfully to client %d.\n", i);

            AckPacket ack;
            struct sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);

            int bytesReceived = recvfrom(
                serverSocket,
                (char*)&ack,
                sizeof(AckPacket),
                0,
                (struct sockaddr*)&clientAddr,
                &clientAddrLen);

            if (bytesReceived == SOCKET_ERROR)
            {
                printf("Failed to receive UDP ACK. Error = %d\n", WSAGetLastError());
            }
            else
            {
                printf("\nUDP ACK received successfully!\n");
                printf("Event ID : %u\n", ack.eventID);
                printf("ECU Name : %s\n", ack.ecuName);
                printf("Status   : %u\n", ack.success);
            }
        }
    }
    else
    {
        for (int i = 0; i < connectedClients; i++)
        {
            int bytesSent = send(
                clientSockets[i],
                (const char*)event,
                sizeof(EventPacket),
                0);

            if (bytesSent == SOCKET_ERROR)
            {
                printf("Failed to send event to client %d. Error = %d\n", i, WSAGetLastError());
                continue;
            }

            printf("Event sent successfully to client %d (%d bytes).\n", i, bytesSent);

            AckPacket ack;
            int bytesReceived = recv(
                clientSockets[i],
                (char*)&ack,
                sizeof(AckPacket),
                0);

            if (bytesReceived == SOCKET_ERROR)
            {
                int error = WSAGetLastError();

                if (error == WSAETIMEDOUT)
                {
                    printf("ACK timeout from client %d\n", i);
                }
                else
                {
                    printf("Failed to receive ACK from client %d. Error = %d\n", i, error);
                }
            }
            else
            {
                printf("\n========== SERVER ==========\n");
                printf("ACK received from ECU : %s\n", ack.ecuName);
                printf("Event ID              : %u\n", ack.eventID);
                printf("Status                : %s\n", ack.success ? "SUCCESS" : "FAILED");
                printf("============================\n");
            }
        }
    }
}

/* =====================================================
   Demo / manual-trigger hooks
   (used from the server console loop in main.c to demonstrate
   specific behaviors — e.g. duplicate detection — on demand
   during the live demo)
   ===================================================== */

void NetworkManager_InitDemoEvents(void)
{
    demoEvents[0].eventID = 201; demoEvents[0].eventType = EVENT_ENGINE_START;       demoEvents[0].severity = SEVERITY_HIGH;
    demoEvents[1].eventID = 202; demoEvents[1].eventType = EVENT_LOW_BATTERY;        demoEvents[1].severity = SEVERITY_MEDIUM;
    demoEvents[2].eventID = 203; demoEvents[2].eventType = EVENT_DOOR_OPEN;          demoEvents[2].severity = SEVERITY_LOW;
    demoEvents[3].eventID = 204; demoEvents[3].eventType = EVENT_DIAGNOSTIC_REQUEST; demoEvents[3].severity = SEVERITY_MEDIUM;
    demoEvents[4].eventID = 205; demoEvents[4].eventType = EVENT_ECU_CONNECTED;      demoEvents[4].severity = SEVERITY_LOW;

    for (int i = 0; i < 5; i++)
    {
        strcpy_s(demoEvents[i].sourceECU, sizeof(demoEvents[i].sourceECU), "NetworkEventManager");
    }
}

void NetworkManager_SendDemoEvent(int index)
{
    if (index >= 0 && index < 5)
    {
        NetworkManager_BroadcastEvent(&demoEvents[index]);
    }
    else
    {
        printf("Invalid demo event index.\n");
    }
}

void NetworkManager_ResendLastEvent(void)
{
    printf("\n[DEMO] Resending last event to simulate a retransmission...\n");
    NetworkManager_BroadcastEvent(&lastEvent);
}