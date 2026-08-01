#include "network.h"
#include <stdio.h>

int main(void)
{
    NetworkManager_SelectTransportMode();
    if (NetworkManager_InitializeClient() != 0)
    {
        printf("Gateway initialization failed.\n");
        return -1;
    }

    NetworkManager_SetECUName("GatewayECU");

    if (NetworkManager_ConnectToServer() != 0)
    {
        printf("Failed to connect to server.\n");
        return -1;
    }

    if (NetworkManager_GetTransportMode() == TRANSPORT_UDP)
    {
        NetworkManager_RegisterClient();
    }

    while (1)
    {
        if (NetworkManager_ReceiveEvent() != 0)
        {
            printf("Connection to server lost.\n");
            break;
        }
    }

    return 0;
}