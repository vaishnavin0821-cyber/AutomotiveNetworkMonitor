#include <stdio.h>
#include "network.h"
#include <windows.h>
#include <conio.h>

int main(void)
{
    NetworkManager_SelectTransportMode();

    if (NetworkManager_Initialize() != 0)
    {
        printf("Server initialization failed.\n");
        return -1;
    }

    NetworkManager_InitDemoEvents();

    printf("\n[Server Commands] 1=Engine Start 2=Low Battery 3=Door Open 4=Diagnostic Request 5=ECU Connected\n");
    printf("Server also auto-generates a new event every 5 seconds.\n\n");

    DWORD lastAutoEventTime = GetTickCount();
    const DWORD AUTO_EVENT_INTERVAL_MS = 5000;

    while (1)
    {
        if (_kbhit())
        {
            char cmd = _getch();
            switch (cmd)
            {
            case '1': NetworkManager_SendDemoEvent(0); break;
            case '2': NetworkManager_SendDemoEvent(1); break;
            case '3': NetworkManager_SendDemoEvent(2); break;
            case '4': NetworkManager_SendDemoEvent(3); break;
            case '5': NetworkManager_SendDemoEvent(4); break;
            case 'd':
                NetworkManager_ResendLastEvent();
                break;
            default: break;
            }
        }

        NetworkManager_PollForECU();

        DWORD now = GetTickCount();
        if (now - lastAutoEventTime >= AUTO_EVENT_INTERVAL_MS)
        {
            EventPacket autoEvent;
            NetworkManager_CreateEvent(&autoEvent);
            NetworkManager_BroadcastEvent(&autoEvent);
            lastAutoEventTime = now;
        }
    }

    return 0;
}