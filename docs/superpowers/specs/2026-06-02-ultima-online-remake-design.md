# Design Spec: Ultima Online Remake (Client-Server using SDL3 & SDL3_net)

## Goal
Build an isometric 2D multiplayer RPG inspired by Ultima Online using a Server-Authoritative architecture. The server processes all game physics and rule checks and replicates visible entities to clients over TCP connections via SDL3_net. The client handles rendering, UI, and sending user inputs.

## Architectural Decisions
1. **Target Separation**:
   - `OUCommon` (Static Library): Shared math (`Vector2D`), physics (`Collision`), networking structs (`Packets.hpp`), and map reading logic.
   - `OUGameClient` (Executable): Graphical window, input processing, network client socket, client ECS (interpolating entity movements, rendering tile maps, sprites, UI stats, name labels, and chat).
   - `OUGameServer` (Executable): Headless console application, network server listener socket, server ECS (moving entities, verifying physics against static map collision layer, spawning monster NPCs, processing combat, managing chat messages).

2. **Network Protocol**:
   - Running over TCP stream sockets (`NET_StreamSocket`).
   - Packets are serialized as flat binary structs.
   - Packet header contains `id` (2 bytes) and `length` (2 bytes) to packet-frame TCP streams.
   - Incoming stream packets are buffered locally until a complete packet is received.

3. **Gameplay Mechanics**:
   - **Movement**: Server-authoritative. Client requests movement in a direction. Server calculates final position, checks collision on server-loaded map, and broadcasts new positions.
   - **Combat**: Auto-attack in War Mode. Deals damage periodically based on speed. Shows floating text on client.
   - **Stats**: Strength, Dexterity, Intelligence, Health, Mana, Stamina.
   - **Notoriety**: Name tag color based on alignment: Innocent (Blue), Criminal (Gray), Murderer (Red).
   - **Ghost & Resurrection**: Health drops to 0 turns player into a ghost (alpha sprite). Player can resurrect at a healer statue.
   - **Chat**: Messages broadcast to nearby players and rendered above their heads.

4. **Asset Acquisition**:
   - A Python script `download_assets.py` will fetch free, open-source sprites and tilesets from public repositories (OpenGameArt / Kenney) to local `assets/` so the game can build and render.

---

## Detailed Component Specifications

### 1. Network Protocol (`src/Common/Network/Packets.hpp`)
```cpp
#pragma once
#include <cstdint>

#pragma pack(push, 1)

struct PacketHeader {
    uint16_t id;      // Packet Type ID
    uint16_t length;  // Total length of packet including header
};

// Client -> Server
struct ClientLoginMsg {
    PacketHeader header;
    char username[32];
};

struct ClientMoveMsg {
    PacketHeader header;
    uint8_t direction; // 0-7 directions
};

struct ClientChatMsg {
    PacketHeader header;
    char text[128];
};

struct ClientActionMsg {
    PacketHeader header;
    uint8_t actionType; // 0: Toggle War Mode, 1: Attack, 2: Resurrect
    uint32_t targetEntityId;
};

// Server -> Client
struct ServerLoginAckMsg {
    PacketHeader header;
    uint32_t playerEntityId;
    float startX;
    float startY;
    int32_t mapWidth;
    int32_t mapHeight;
};

struct ServerEntityUpdateMsg {
    PacketHeader header;
    uint32_t entityId;
    float x;
    float y;
    uint8_t direction;
    uint8_t state;       // 0: Idle, 1: Walk, 2: Attack, 3: Ghost
    uint8_t notoriety;   // 0: Innocent (Blue), 1: Criminal (Gray), 2: Murderer (Red)
    char name[32];
    int32_t currentHp;
    int32_t maxHp;
};

struct ServerEntityDespawnMsg {
    PacketHeader header;
    uint32_t entityId;
};

struct ServerChatBroadcastMsg {
    PacketHeader header;
    uint32_t entityId;
    char text[128];
};

struct ServerCombatEventMsg {
    PacketHeader header;
    uint32_t attackerId;
    uint32_t targetId;
    int32_t damage;
};

#pragma pack(pop)
```

### 2. Network Buffer (`src/Common/Network/NetworkBuffer.hpp`)
A buffer helper that slices stream sockets into packets:
```cpp
#pragma once
#include <vector>
#include <cstdint>
#include <cstring>

class NetworkBuffer {
public:
    void Append(const uint8_t* data, size_t size) {
        buffer.insert(buffer.end(), data, data + size);
    }
    
    bool HasCompletePacket(uint16_t& outId, std::vector<uint8_t>& outData) {
        if (buffer.size() < 4) return false;
        
        uint16_t length;
        std::memcpy(&length, &buffer[2], sizeof(uint16_t));
        
        if (buffer.size() < length) return false;
        
        std::memcpy(&outId, &buffer[0], sizeof(uint16_t));
        outData.assign(buffer.begin(), buffer.begin() + length);
        
        buffer.erase(buffer.begin(), buffer.begin() + length);
        return true;
    }
    
    void Clear() {
        buffer.clear();
    }
    
private:
    std::vector<uint8_t> buffer;
};
```
