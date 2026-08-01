#ifndef NETWORK_H
#define NETWORK_H

#include "protocol.h"

//Server
int NetworkManager_Initialize(void);
int NetworkManager_WaitForECU(void);
int NetworkManager_PollForECU(void);

//Client
int NetworkManager_InitializeClient(void);
int NetworkManager_ConnectToServer(void);
int NetworkManager_ReceiveEvent(void);
int NetworkManager_SendAck(EventPacket* event);
int NetworkManager_RegisterClient(void);
void NetworkManager_SetECUName(const char* ecuName);

//Shared
void NetworkManager_SelectTransportMode(void);
TransportMode NetworkManager_GetTransportMode(void);
int NetworkManager_GetConnectedClients(void);
const char* NetworkManager_GetEventTypeName(EventType type);
const char* NetworkManager_GetSeverityName(Severity severity);

// Event generation & demo/test hooks 
void NetworkManager_CreateEvent(EventPacket* event);
void NetworkManager_BroadcastEvent(EventPacket* event);
void NetworkManager_InitDemoEvents(void);
void NetworkManager_SendDemoEvent(int index);
void NetworkManager_ResendLastEvent(void);

#endif