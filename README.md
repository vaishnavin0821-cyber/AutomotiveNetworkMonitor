A lightweight client-server event update system in C over TCP/IP. One server (NetworkEventManager) generates events and pushes them to multiple connected clients (ECUs). Clients detect and ignore duplicate events, update local state on new events, and acknowledge every event back to the server. Transport (TCP or UDP) is selectable at runtime by both the server and each client.

Requirement

Develop a lightweight event update system in C or C++ over a TCP/IP stack. The system shall include one server and multiple clients communicating over a TCP/IP stack of your choice. The server shall generate update events and push them to all connected clients. Each client shall receive the event, check whether the event was already processed, ignore duplicate events, update its local state for new events, and send an acknowledgement back to the server. The server/client shall have the option to send/receive the data over TCP or UDP based on the user selection.

## Components

| Executable | Role |
|---|---|
| `NetworkEventManager` | Server — generates events, broadcasts to all connected clients |
| `GatewayECU` | Client |
| `DiagnosticECU` | Client |
| `DataLoggerECU` | Client |

## What's implemented

- **TCP communication** — server socket, `accept()` for incoming client connections, `send()`/`recv()` for events and ACKs.
- **UDP communication** — connectionless, so each client first sends a `RegistrationPacket` so the server can learn its address; events use `sendto()`, ACKs use `recvfrom()`.
- **Transport selection** — both server and client choose TCP or UDP at startup.
- **Readable event data** — event type and severity are shown as names (e.g. "Engine Start", "HIGH") instead of raw numbers, along with event ID and source ECU.
- **Duplicate detection** — each client keeps a history of the last 20 processed event IDs, ignores repeats, but still sends an ACK for them.
- **ACK mechanism** — every event is acknowledged back to the server with ECU name, event ID, and status, for both TCP and UDP.
- **Multiple client support** — server tracks up to 10 connected clients (TCP sockets or UDP address/port pairs) and broadcasts to all of them.
- **Server-driven event generation** — the server generates a new event automatically on a timer, independent of client connections, so events keep flowing even after all clients are already connected.
- **Manual demo commands** — the server can also be told to send a specific event type, or resend the last event, for live demonstration of duplicate handling.


### Key design decisions

- **Event generation is decoupled from client connections** — a timer drives event broadcast, independent of when or how many clients are connected.
- **Single-threaded event loop with `select()`** — accepting clients, broadcasting, and reading commands run in one loop, avoiding multi-thread coordination issues on shared sockets.
- **Sliding-window duplicate history** — a circular buffer of the last 20 event IDs; once full, the oldest entry is overwritten instead of the system refusing to record new ones.
- **Manual trigger commands** — allow sending a specific event type or resending the last event on demand, for reliable live demonstration.
- **Receive timeout on ACKs** — a 3-second timeout is applied when waiting for a client's acknowledgement, on both TCP (set per-client socket) and UDP (set on the shared server socket). Without this, one unresponsive client could block the entire single-threaded server indefinitely; the timeout ensures the server logs the failure and continues serving other clients.

## Building

Windows / Visual Studio, using Winsock2 (`Ws2_32.lib`).

1. Open the solution and build each project (`NetworkEventManager`, `GatewayECU`, `DiagnosticECU`, `DataLoggerECU`).
2. All four share the same `network.c` / `network.h` / `protocol.h`.

## Running the demo

1. Start `NetworkEventManager.exe` first. Choose transport mode (`1` = TCP, `2` = UDP).
2. Start each client `.exe`, choosing the **same** transport mode.
3. Clients register/connect — no event is sent at this point.
4. The server automatically generates and broadcasts a new event every 5 seconds to all currently connected clients.

### Server console commands

| Key | Action |
|---|---|
| `1`–`5` | Manually broadcast a specific demo event (Engine Start / Low Battery / Door Open / Diagnostic Request / ECU Connected) |
| `d` | Resend the most recently generated event, to demonstrate duplicate detection |

Pressing `1` twice in a row (or `d` after any event has fired) causes connected clients to print `Duplicate event detected. Event ID X already processed. Ignoring event.` while still acknowledging the resend.

## Known limitations

- **Sequential broadcast** — clients are served one at a time, not simultaneously; a slow client adds up to a 3-second delay (capped by the timeout) before the server moves on.
- **In-memory state only** — client list and event history reset on restart; no persistence layer.
- **Fixed-size duplicate window** — each client tracks only its last 20 event IDs, sufficient for a demo but not designed for long-running scale.