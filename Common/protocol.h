#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define SERVER_PORT 50000
#define MAX_CLIENTS 10
#define MAX_NAME_LENGTH 32

/* Transport Mode */
typedef enum
{
    TRANSPORT_TCP = 1,
    TRANSPORT_UDP
} TransportMode;

/* Event Types */
typedef enum
{
    EVENT_ECU_CONNECTED,
    EVENT_ENGINE_START,
    EVENT_LOW_BATTERY,
    EVENT_DOOR_OPEN,
    EVENT_DIAGNOSTIC_REQUEST
} EventType;

/* Severity */
typedef enum
{
    SEVERITY_LOW = 1,
    SEVERITY_MEDIUM,
    SEVERITY_HIGH
} Severity;

/* Event Packet */
typedef struct
{
    uint32_t eventID;
    EventType eventType;
    Severity severity;
    char sourceECU[MAX_NAME_LENGTH];
} EventPacket;

/* ACK Packet */
typedef struct
{
    uint32_t eventID;
    char ecuName[MAX_NAME_LENGTH];
    uint8_t success;
} AckPacket;

/* Registration Packet (UDP clients only - lets a connectionless
   client announce itself so the server can learn its address) */
typedef struct
{
    char ecuName[MAX_NAME_LENGTH];
} RegistrationPacket;

#endif