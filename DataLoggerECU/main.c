#include <stdio.h>
#include "network.h"

int main(void)
{
    NetworkManager_SelectTransportMode();

    if (NetworkManager_InitializeClient() != 0)
        return -1;

    NetworkManager_SetECUName("DataLoggerECU");

    if (NetworkManager_ConnectToServer() != 0)
        return -1;

    if (NetworkManager_GetTransportMode() == TRANSPORT_UDP)
    {
        NetworkManager_RegisterClient();
    }
  
    while (1)
    {
        if (NetworkManager_ReceiveEvent() != 0)
        {
            printf("Connection lost.\n");
            break;
        }
    }

    return 0;
}