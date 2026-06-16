# Z-Axis and Staircase System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Z-axis support, a multi-layer map structure, smooth stairs/ramps climbing logic, and static/dynamic objects on different elevation levels for the client and server.

**Architecture:** Extend TransformComponent, SpriteComponent, and TileComponent to support height coordinate (Z) on client and server. Re-structure the map parser to support Ground, Decoration, Height-level, and Slope-type grids. Perform linear interpolation on client and server to smoothly transition player Z coordinates on stair slopes.

**Tech Stack:** C++, SDL3, SDL3_net.

---

### Task 1: Extend Packets and TransformComponent for Z-Axis

**Files:**
- Modify: `src/Common/Network/Packets.hpp`
- Modify: `src/ECS/Components/TransformComponent.hpp`

- [ ] **Step 1: Add `z` fields in Network packets**
  Update `ServerEntityUpdateMsg` and `ServerLoginAckMsg` inside `src/Common/Network/Packets.hpp` to include `z` representation.
  ```cpp
  // Inside Packets.hpp:
  struct ServerEntityUpdateMsg {
      PacketHeader header;
      uint32_t entityId;
      float x;
      float y;
      float z; // <--- NEW: Add z position
      uint8_t direction;
      uint8_t state;
      uint8_t notoriety;
      char name[32];
      int32_t currentHp;
      int32_t maxHp;
  };

  struct ServerLoginAckMsg {
      PacketHeader header;
      uint32_t playerEntityId;
      float startX;
      float startY;
      float startZ; // <--- NEW: Add startZ position
      int32_t mapWidth;
      int32_t mapHeight;
  };
  ```

- [ ] **Step 2: Add `z` field to TransformComponent**
  Modify `src/ECS/Components/TransformComponent.hpp` to hold the $Z$ position.
  ```cpp
  // Inside TransformComponent.hpp:
  class TransformComponent : public Component {
  public:
      Vector2D position;
      Vector2D velocity;
      float z = 0.0f; // <--- NEW: Add Z-axis height position (in pixels)
      
      int height = 32;
      int width = 32;
      int scale = 1;
      int speed = 3;
      bool blocked = false;

      TransformComponent() {
          position.Zero();
          z = 0.0f;
      }
      
      TransformComponent(int sc) {
          position.Zero();
          scale = sc;
          z = 0.0f;
      }

      TransformComponent(float x, float y) {
          position.x = x;
          position.y = y;
          z = 0.0f;
      }

      TransformComponent(float x, float y, float zpos, int h, int w, int sc) {
          position.x = x;
          position.y = y;
          z = zpos;
          height = h;
          width = w;
          scale = sc;
      }
  };
  ```

- [ ] **Step 3: Commit changes**
  ```bash
  git add src/Common/Network/Packets.hpp src/ECS/Components/TransformComponent.hpp
  git commit -m "feat: add Z coordinate to packets and TransformComponent"
  ```

---

### Task 2: Multi-Layer MapData Parser (Shared between Client and Server)

**Files:**
- Modify: `src/Common/World/MapData.hpp`
- Modify: `src/Common/World/MapData.cpp`

- [ ] **Step 1: Update MapData.hpp header**
  Add declarations for ground layer, decoration layer, height grid, tile type grid, and slope interpolation.
  ```cpp
  // Inside MapData.hpp:
  enum class TileType : uint8_t {
      Walkable = 0,
      Blocked = 1,
      Ramp_SW_NE = 2, // Slope going up from West/South-West to East/North-East
      Ramp_SE_NW = 3  // Slope going up from East/South-East to West/North-West
  };

  class MapData {
  public:
      MapData() = default;
      ~MapData() = default;

      bool LoadMap(const std::string& path, int sizeX, int sizeY);
      [[nodiscard]] bool IsBlocked(float x, float y, float z, float w, float h) const noexcept;
      [[nodiscard]] bool IsTileBlocked(int tileX, int tileY, float entityZ) const noexcept;
      [[nodiscard]] float GetTileHeight(int tileX, int tileY) const noexcept;
      [[nodiscard]] TileType GetTileType(int tileX, int tileY) const noexcept;
      [[nodiscard]] float GetInterpolatedHeight(float x, float y) const noexcept;

      [[nodiscard]] int GetWidth() const noexcept { return width; }
      [[nodiscard]] int GetHeight() const noexcept { return height; }
      [[nodiscard]] int GetTileSize() const noexcept { return tileSize; }
      [[nodiscard]] int GetMapScale() const noexcept { return mapScale; }
      [[nodiscard]] int GetScaledSize() const noexcept { return scaledSize; }

  private:
      int width = 0;
      int height = 0;
      int tileSize = 32;
      int mapScale = 3;
      int scaledSize = 96;
      std::vector<std::vector<int>> groundGrid;
      std::vector<std::vector<int>> decorGrid;
      std::vector<std::vector<float>> heightGrid;
      std::vector<std::vector<TileType>> typeGrid;
  };
  ```

- [ ] **Step 2: Update MapData.cpp implementation**
  Implement parsing of 4 blocks from `map.map`. First block: Ground tiles. Second block: Decoration tiles. Third block: Height values. Fourth block: Tile Types (0: walkable, 1: blocked, 2: ramp SW-NE, 3: ramp SE-NW).
  Implement linear interpolation of Z height for ramps.
  ```cpp
  // Inside MapData.cpp:
  #include "Common/World/MapData.hpp"
  #include <fstream>
  #include <iostream>
  #include <cmath>
  #include <algorithm>

  bool MapData::LoadMap(const std::string& path, int sizeX, int sizeY) {
      std::ifstream mapFile(path);
      if (!mapFile.is_open()) {
          std::cerr << "MapData: Failed to open map file: " << path << std::endl;
          return false;
      }
      width = sizeX;
      height = sizeY;

      char c = 0;
      groundGrid.assign(sizeY, std::vector<int>(sizeX, 0));
      decorGrid.assign(sizeY, std::vector<int>(sizeX, 0));
      heightGrid.assign(sizeY, std::vector<float>(sizeX, 0.0f));
      typeGrid.assign(sizeY, std::vector<TileType>(sizeX, TileType::Walkable));

      // 1. Read Ground Grid
      for (int y = 0; y < sizeY; ++y) {
          for (int x = 0; x < sizeX; ++x) {
              if (!mapFile.get(c)) return false;
              int val = (c - '0') * 10;
              if (!mapFile.get(c)) return false;
              val += (c - '0');
              groundGrid[y][x] = val;
              mapFile.ignore(); // skip comma or newline
          }
      }
      mapFile.ignore(); // skip separator newline

      // 2. Read Decoration/Cliff Grid
      for (int y = 0; y < sizeY; ++y) {
          for (int x = 0; x < sizeX; ++x) {
              if (!mapFile.get(c)) return false;
              int val = (c - '0') * 10;
              if (!mapFile.get(c)) return false;
              val += (c - '0');
              decorGrid[y][x] = val;
              mapFile.ignore();
          }
      }
      mapFile.ignore();

      // 3. Read Height Grid
      for (int y = 0; y < sizeY; ++y) {
          for (int x = 0; x < sizeX; ++x) {
              if (!mapFile.get(c)) return false;
              int val = (c - '0') * 10;
              if (!mapFile.get(c)) return false;
              val += (c - '0');
              // Store as pixel height (scale 3 * 32 = 96 pixels high per tier)
              heightGrid[y][x] = static_cast<float>(val * 96);
              mapFile.ignore();
          }
      }
      mapFile.ignore();

      // 4. Read Type/Collision Grid
      for (int y = 0; y < sizeY; ++y) {
          for (int x = 0; x < sizeX; ++x) {
              if (!mapFile.get(c)) return false;
              int typeVal = c - '0';
              typeGrid[y][x] = static_cast<TileType>(typeVal);
              mapFile.ignore();
          }
      }
      mapFile.close();
      return true;
  }

  bool MapData::IsTileBlocked(int tileX, int tileY, float entityZ) const noexcept {
      if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height)
          return true;

      // Solid block
      if (typeGrid[tileY][tileX] == TileType::Blocked)
          return true;

      // Entity Z must match tile Z height. Give it a tiny margin (e.g. 10.0f)
      float tileZ = heightGrid[tileY][tileX];
      if (std::abs(entityZ - tileZ) > 10.0f && typeGrid[tileY][tileX] == TileType::Walkable) {
          // If the tile is walkable but on a different height tier, block the player 
          // (e.g. player at Z=0 cannot walk on hills at Z=96 directly without ramp)
          return true;
      }
      return false;
  }

  bool MapData::IsBlocked(float x, float y, float z, float w, float h) const noexcept {
      int x0 = static_cast<int>(x) / scaledSize;
      int y0 = static_cast<int>(y) / scaledSize;
      int x1 = static_cast<int>(x + w - 1.0f) / scaledSize;
      int y1 = static_cast<int>(y + h - 1.0f) / scaledSize;

      for (int ty = y0; ty <= y1; ++ty) {
          for (int tx = x0; tx <= x1; ++tx) {
              if (IsTileBlocked(tx, ty, z))
                  return true;
          }
      }
      return false;
  }

  float MapData::GetTileHeight(int tileX, int tileY) const noexcept {
      if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height)
          return 0.0f;
      return heightGrid[tileY][tileX];
  }

  TileType MapData::GetTileType(int tileX, int tileY) const noexcept {
      if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height)
          return TileType::Blocked;
      return typeGrid[tileY][tileX];
  }

  float MapData::GetInterpolatedHeight(float x, float y) const noexcept {
      int tileX = static_cast<int>(x) / scaledSize;
      int tileY = static_cast<int>(y) / scaledSize;

      if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height)
          return 0.0f;

      TileType type = typeGrid[tileY][tileX];
      float baseH = heightGrid[tileY][tileX];

      if (type == TileType::Ramp_SW_NE) {
          // Ramp going up from SW (left) to NE (right)
          // Z increases as Cartesian X increases within this tile
          float dx = x - static_cast<float>(tileX * scaledSize);
          float ratio = dx / static_cast<float>(scaledSize);
          ratio = std::clamp(ratio, 0.0f, 1.0f);
          return baseH + ratio * 96.0f;
      }
      else if (type == TileType::Ramp_SE_NW) {
          // Ramp going up from SE (down) to NW (up)
          // Z increases as Cartesian Y decreases (going North)
          float dy = y - static_cast<float>(tileY * scaledSize);
          float ratio = (static_cast<float>(scaledSize) - dy) / static_cast<float>(scaledSize);
          ratio = std::clamp(ratio, 0.0f, 1.0f);
          return baseH + ratio * 96.0f;
      }

      return baseH;
  }
  ```

- [ ] **Step 3: Commit changes**
  ```bash
  git add src/Common/World/MapData.hpp src/Common/World/MapData.cpp
  git commit -m "feat: implement multi-layer MapData parser and Z interpolation"
  ```

---

### Task 3: Server-side Z-Axis Physics and Synchronisation

**Files:**
- Modify: `src/Server/Core/ServerApp.hpp`
- Modify: `src/Server/Core/ServerApp.cpp`

- [ ] **Step 1: Add Z coordinate to Server structures**
  Modify `ClientConnection` and `ServerNPC` inside `src/Server/Core/ServerApp.hpp` to have a `z` field.
  ```cpp
  // Inside ServerApp.hpp, ClientConnection:
  struct ClientConnection {
      // ... existing fields ...
      float x = 800.0f;
      float y = 640.0f;
      float z = 0.0f; // <--- NEW: Z height position
      float targetX = 800.0f;
      float targetY = 640.0f;
      // ... existing fields ...
  };

  // Inside ServerApp.hpp, ServerNPC:
  struct ServerNPC {
      // ... existing fields ...
      float x = 0.0f;
      float y = 0.0f;
      float z = 0.0f; // <--- NEW: Z height position
      float targetX = 0.0f;
      float targetY = 0.0f;
      // ... existing fields ...
  };
  ```

- [ ] **Step 2: Update Server login acknowledgment with startZ**
  Modify `processPacket()` in `src/Server/Core/ServerApp.cpp` to initialize player Z coordinates and send them in login ACK:
  ```cpp
  // Inside ServerApp.cpp, in processPacket ClientLogin handler:
  client->x = 800.0f;
  client->y = 640.0f;
  client->z = mapData.GetInterpolatedHeight(client->x, client->y); // Init Z

  ServerLoginAckMsg ack;
  ack.header.id = PacketID::SERVER_LOGIN_ACK;
  ack.header.length = sizeof(ack);
  ack.playerEntityId = client->entityId;
  ack.startX = client->x;
  ack.startY = client->y;
  ack.startZ = client->z; // <--- NEW
  ```

- [ ] **Step 3: Update replicated packets for clients and NPCs**
  Update `processPacket()` where other players and NPCs are replicated. Include Z when sending `SERVER_ENTITY_UPDATE`.
  Also, update the `replicateState()` function:
  ```cpp
  // Inside ServerApp.cpp, replicateState():
  for (const auto& client : clients) {
      if (client->socket != nullptr && client->entityId > 0) {
          ServerEntityUpdateMsg selfMsg;
          selfMsg.header.id = PacketID::SERVER_ENTITY_UPDATE;
          selfMsg.header.length = sizeof(selfMsg);
          selfMsg.entityId = client->entityId;
          selfMsg.x = client->x;
          selfMsg.y = client->y;
          selfMsg.z = client->z; // <--- NEW: replicate Z
          selfMsg.direction = client->direction;
          selfMsg.state = client->isDead ? 3 : client->state;
          selfMsg.notoriety = client->isDead ? 1 : client->notoriety;
          std::strncpy(selfMsg.name, client->username, sizeof(selfMsg.name) - 1);
          selfMsg.currentHp = client->hp;
          selfMsg.maxHp = client->maxHp;
          broadcastPacket(&selfMsg, sizeof(selfMsg));
      }
  }

  for (const auto& npc : npcs) {
      if (!npc.isDead) {
          ServerEntityUpdateMsg npcMsg;
          npcMsg.header.id = PacketID::SERVER_ENTITY_UPDATE;
          npcMsg.header.length = sizeof(npcMsg);
          npcMsg.entityId = npc.entityId;
          npcMsg.x = npc.x;
          npcMsg.y = npc.y;
          npcMsg.z = npc.z; // <--- NEW: replicate Z
          npcMsg.direction = npc.direction;
          npcMsg.state = npc.state;
          npcMsg.notoriety = npc.notoriety;
          std::strncpy(npcMsg.name, npc.name, sizeof(npcMsg.name) - 1);
          npcMsg.currentHp = npc.hp;
          npcMsg.maxHp = npc.maxHp;
          broadcastPacket(&npcMsg, sizeof(npcMsg));
      }
  }
  ```

- [ ] **Step 4: Update Player movement physics with Z check**
  Update `ServerApp::updatePhysics()` to pass Z to `IsBlocked()` and recalculate Z height:
  ```cpp
  // Inside ServerApp.cpp, updatePhysics():
  // Under case state == 1 (walking):
  float nextX = client->x + dx * speed;
  float nextY = client->y + dy * speed;

  // Predict new height tier to check Z alignment blockages
  float nextZ = mapData.GetInterpolatedHeight(nextX + 142.0f, nextY + 115.0f);

  if (!mapData.IsBlocked(nextX + 118.0f, nextY + 80.0f, nextZ, 48.0f, 70.0f)) {
      client->x = nextX;
      client->y = nextY;
      client->z = nextZ;
  }
  ```

- [ ] **Step 5: Update NPC movement physics with Z check**
  Update NPC tracking in `ServerApp::updateNPCs()`:
  ```cpp
  // Inside ServerApp.cpp, updateNPCs():
  // For walking towards player:
  float nextX = npc.x + dx * step;
  float nextY = npc.y + dy * step;
  float nextZ = mapData.GetInterpolatedHeight(nextX + 142.0f, nextY + 115.0f);

  if (!mapData.IsBlocked(nextX + 118.0f, nextY + 80.0f, nextZ, 48.0f, 70.0f)) {
      npc.x = nextX;
      npc.y = nextY;
      npc.z = nextZ;
  }

  // And for random wandering:
  float nextXW = npc.x + dx * 1.5f;
  float nextYW = npc.y + dy * 1.5f;
  float nextZW = mapData.GetInterpolatedHeight(nextXW + 142.0f, nextYW + 115.0f);

  if (!mapData.IsBlocked(nextXW + 118.0f, nextYW + 80.0f, nextZW, 48.0f, 70.0f)) {
      npc.x = nextXW;
      npc.y = nextYW;
      npc.z = nextZW;
  } else {
      npc.state = 0;
  }
  ```

- [ ] **Step 6: Commit changes**
  ```bash
  git add src/Server/Core/ServerApp.hpp src/Server/Core/ServerApp.cpp
  git commit -m "feat: server Z height calculation and networking update"
  ```

---

### Task 4: Client-side Z-Axis Map Loading and Tile Rendering

**Files:**
- Modify: `src/ECS/Components/TileComponent.hpp`
- Modify: `src/World/Map.hpp`
- Modify: `src/World/Map.cpp`

- [ ] **Step 1: Render TileComponent with Z height**
  Add a `float z` field to `TileComponent` and subtract it from the screen Y coordinate in `update()`.
  ```cpp
  // Inside TileComponent.hpp:
  class TileComponent : public Component {
  public:
    SDL_Texture *texture{nullptr};
    SDL_Rect srcRect{};
    SDL_FRect destRect{};
    Vector2D position;
    float z = 0.0f; // <--- NEW: Tile Z-offset

    TileComponent() = default;
    ~TileComponent() override = default;

    TileComponent(int srcX, int srcY, int xpos, int ypos, float zpos, int tsize, int tscale,
                  const std::string &id) {
      texture = Game::assets->GetTexture(id);
      srcRect.x = srcX;
      srcRect.y = srcY;
      srcRect.w = srcRect.h = tsize;
      position.x = static_cast<float>(xpos);
      position.y = static_cast<float>(ypos);
      z = zpos; // <--- NEW
      destRect.w = destRect.h = static_cast<float>(tsize * tscale);
    }

    void update() override {
      if (Game::isIsometric) {
        float isoX = (position.x - position.y) * 0.5f;
        float isoY = (position.x + position.y) * 0.25f - z; // <--- NEW: subtract Z
        destRect.x = isoX - Game::camera.x;
        destRect.y = isoY - Game::camera.y;
      } else {
        destRect.x = position.x - Game::camera.x;
        destRect.y = position.y - Game::camera.y - z; // <--- NEW: subtract Z
      }
    }
    // ...
  };
  ```

- [ ] **Step 2: Update Map.hpp header**
  Modify `AddTile` and `LoadMap` signatures.
  ```cpp
  // Inside Map.hpp:
  class Map {
  public:
      Map(std::string tID, int ms, int ts);
      ~Map() = default;

      void LoadMap(const std::string& path, int sizeX, int sizeY);
      void AddTile(int srcX, int srcY, int xpos, int ypos, float zpos); // <--- NEW: zpos

  private:
      std::string texID;
      int mapScale = 1;
      int tileSize = 32;
      int scaledSize = 32;
  };
  ```

- [ ] **Step 3: Update Map.cpp map loader implementation**
  Update `Map::LoadMap` to read the new 4-layer layout from `assets/map.map`.
  Instantiate tiles for both layers (Ground layer and Decoration layer) at their parsed height.
  ```cpp
  // Inside Map.cpp:
  #include "World/Map.hpp"
  #include <fstream>
  #include <iostream>
  #include <vector>
  #include "Core/Game.hpp"
  #include "ECS/Components/TileComponent.hpp"
  #include "ECS/ECS.hpp"

  extern Manager manager;

  Map::Map(std::string tID, int ms, int ts)
      : texID(std::move(tID)), mapScale(ms), tileSize(ts) {
    scaledSize = ms * ts;
  }

  void Map::LoadMap(const std::string& path, int sizeX, int sizeY) {
    std::ifstream mapFile(path);
    if (!mapFile.is_open()) {
      std::cerr << "Failed to open map file: " << path << std::endl;
      return;
    }

    char c = 0;
    std::vector<std::vector<int>> ground(sizeY, std::vector<int>(sizeX, 0));
    std::vector<std::vector<int>> decor(sizeY, std::vector<int>(sizeX, 0));
    std::vector<std::vector<float>> height(sizeY, std::vector<float>(sizeX, 0.0f));

    // 1. Read Ground tiles
    for (int y = 0; y < sizeY; ++y) {
      for (int x = 0; x < sizeX; ++x) {
        if (!mapFile.get(c)) break;
        int val = (c - '0') * 10;
        if (!mapFile.get(c)) break;
        val += (c - '0');
        ground[y][x] = val;
        mapFile.ignore();
      }
    }
    mapFile.ignore();

    // 2. Read Decor tiles
    for (int y = 0; y < sizeY; ++y) {
      for (int x = 0; x < sizeX; ++x) {
        if (!mapFile.get(c)) break;
        int val = (c - '0') * 10;
        if (!mapFile.get(c)) break;
        val += (c - '0');
        decor[y][x] = val;
        mapFile.ignore();
      }
    }
    mapFile.ignore();

    // 3. Read Heights
    for (int y = 0; y < sizeY; ++y) {
      for (int x = 0; x < sizeX; ++x) {
        if (!mapFile.get(c)) break;
        int val = (c - '0') * 10;
        if (!mapFile.get(c)) break;
        val += (c - '0');
        height[y][x] = static_cast<float>(val * 96);
        mapFile.ignore();
      }
    }
    mapFile.ignore();

    // 4. Read types & instantiate colliders and tiles
    for (int y = 0; y < sizeY; ++y) {
      for (int x = 0; x < sizeX; ++x) {
        if (!mapFile.get(c)) break;
        int typeVal = c - '0';
        mapFile.ignore();

        float tileZ = height[y][x];

        // Add Ground Tile
        int gVal = ground[y][x];
        int gSrcY = (gVal / 10) * tileSize;
        int gSrcX = (gVal % 10) * tileSize;
        AddTile(gSrcX, gSrcY, x * scaledSize, y * scaledSize, tileZ);

        // Add Decor/Cliff/Stairs Tile if present (not 0)
        int dVal = decor[y][x];
        if (dVal > 0) {
          int dSrcY = (dVal / 10) * tileSize;
          int dSrcX = (dVal % 10) * tileSize;
          AddTile(dSrcX, dSrcY, x * scaledSize, y * scaledSize, tileZ);
        }
      }
    }
    mapFile.close();
  }

  void Map::AddTile(int srcX, int srcY, int xpos, int ypos, float zpos) {
    auto& tile(manager.addEntity());
    tile.addComponent<TileComponent>(srcX, srcY, xpos, ypos, zpos, tileSize, mapScale,
                                     texID);
    tile.addGroup(Game::groupMap);
  }
  ```

- [ ] **Step 4: Commit changes**
  ```bash
  git add src/ECS/Components/TileComponent.hpp src/World/Map.hpp src/World/Map.cpp
  ```

---

### Task 5: Client-side Sprite Z-Axis Shift and Smooth Sync

**Files:**
- Modify: `src/ECS/Components/SpriteComponent.hpp`
- Modify: `src/Core/Game.cpp`

- [ ] **Step 1: Shift Sprites in SpriteComponent**
  Update `SpriteComponent::update()` in `src/ECS/Components/SpriteComponent.hpp` to subtract `transform->z` from screen Y.
  ```cpp
  // Inside SpriteComponent.hpp, update():
  if (Game::isIsometric) {
    float isoX = (transform->position.x - transform->position.y) * 0.5f;
    float isoY = (transform->position.x + transform->position.y) * 0.25f - transform->z; // <--- NEW: subtract Z
    destRect.x = isoX - Game::camera.x;
    destRect.y = isoY - Game::camera.y;
  } else {
    destRect.x = transform->position.x - Game::camera.x;
    destRect.y = transform->position.y - Game::camera.y - transform->z; // <--- NEW: subtract Z
  }
  ```

- [ ] **Step 2: Smooth Z Interpolation in Game.cpp**
  Ensure player and NPC Z coordinates are parsed and linearly interpolated smoothly along with X and Y.
  First, inside `Game::handlePacket()`, update packet parsing for `SERVER_ENTITY_UPDATE` to save target Z:
  ```cpp
  // Inside Game.cpp, handlePacket SERVER_ENTITY_UPDATE:
  if (it == serverEntities.end()) {
      // ... new replication ...
      ent.addComponent<TransformComponent>(msg->x, msg->y, msg->z, 64, 64, 2); // <--- Pass msg->z
      // ...
      auto& netComp = ent.getComponent<NetworkComponent>();
      netComp.targetX = msg->x;
      netComp.targetY = msg->y;
      netComp.targetZ = msg->z; // <--- Store target Z
      netComp.startX = msg->x;
      netComp.startY = msg->y;
      netComp.startZ = msg->z; // <--- Store start Z
  ```
  *Note: We need to define `targetZ` and `startZ` fields in `NetworkComponent.hpp`. Let's modify it.*

- [ ] **Step 3: Add Z parameters to NetworkComponent**
  Add `targetZ` and `startZ` fields to `NetworkComponent.hpp`.
  ```cpp
  // Modify: src/ECS/Components/NetworkComponent.hpp
  // Inside NetworkComponent.hpp:
  class NetworkComponent : public Component {
  public:
      uint32_t entityId;
      std::string name;
      int hp;
      int maxHp;
      uint8_t state;
      uint8_t notoriety;
      uint8_t direction;

      float startX = 0;
      float startY = 0;
      float startZ = 0; // <--- NEW
      float targetX = 0;
      float targetY = 0;
      float targetZ = 0; // <--- NEW
      // ...
  };
  ```

- [ ] **Step 4: Interpolate Z in Game::update()**
  Modify `Game::update()` to linearly interpolate player and NPC Z heights over the 50ms interval:
  ```cpp
  // Inside Game.cpp, update():
  // Inside for (auto const& [entId, ent] : serverEntities):
  float dx = net.targetX - trans.position.x;
  float dy = net.targetY - trans.position.y;
  float dist = std::hypot(dx, dy);

  if (dist > 150.0f) {
      trans.position.x = net.targetX;
      trans.position.y = net.targetY;
      trans.z = net.targetZ; // <--- NEW
      net.startX = net.targetX;
      net.startY = net.targetY;
      net.startZ = net.targetZ; // <--- NEW
  } else {
      uint32_t elapsed = SDL_GetTicks() - net.lastUpdateTicks;
      float t = static_cast<float>(elapsed) / 50.0f;
      if (t > 1.0f) t = 1.0f;

      trans.position.x = net.startX + (net.targetX - net.startX) * t;
      trans.position.y = net.startY + (net.targetY - net.startY) * t;
      trans.z = net.startZ + (net.targetZ - net.startZ) * t; // <--- NEW
  }
  ```

- [ ] **Step 5: Adjust Floating Tag offsets in Game::render()**
  Floating tags and chat bubbles must shift up along with player height.
  Update the rendering offset in `Game::render()`:
  ```cpp
  // Inside Game.cpp, render():
  // Modify screenX and screenY calculation to apply trans.z:
  if (isIsometric) {
      float isoX = (trans.position.x - trans.position.y) * 0.5f;
      float isoY = (trans.position.x + trans.position.y) * 0.25f - trans.z; // <--- NEW: apply trans.z
      screenX = isoX - camera.x;
      screenY = isoY - camera.y;
  } else {
      screenX = trans.position.x - camera.x;
      screenY = trans.position.y - camera.y - trans.z; // <--- NEW: apply trans.z
  }
  ```

- [ ] **Step 6: Commit changes**
  ```bash
  git add src/ECS/Components/SpriteComponent.hpp src/Core/Game.cpp src/ECS/Components/NetworkComponent.hpp
  git commit -m "feat: client smooth Z rendering and interpolation"
  ```

---

### Task 6: Custom Multi-Layer Map assets and layout

**Files:**
- Modify: `assets/map.map`

- [ ] **Step 1: Redefine map.map with Z levels and staircase ramps**
  Update `assets/map.map` to define ground tiles, decoration tiles, height tiers, and type grids.
  We will construct:
  - Base level (Z=0, height value `00`).
  - Elevated Hill/Mountain (Z=96, height value `01`).
  - Stairs/Ramps connecting base and hill.
  Let's structure the file with exactly 20 rows per block, separated by a blank line.
  
  *Refer to spec structure. Let's design the layout properly.*

- [ ] **Step 2: Commit map file**
  ```bash
  git add assets/map.map
  ```

---

### Task 7: Compilation and Verification

- [ ] **Step 1: Compile the project**
  Ensure all client and server code compiles cleanly:
  Run: `cmake --build build --config Debug`

- [ ] **Step 2: Test gameplay**
  Start `OUGameServer` and `OUGameClient`. Ensure:
  1. Hills are displayed raised up.
  2. Character can walk up the staircase ramp.
  3. Character height Z increases smoothly.
  4. Character is blocked at cliffs.
