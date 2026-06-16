# Design Spec: Z-Axis, Multi-Layer Map, and Staircase System

This design specification details the integration of Z-axis (height/elevation), a multi-layer map representation (ground layer + decoration/cliff layer), smooth stairs/ramps climbing logic, and support for placing static decoration tiles and dynamic interactive entities (monsters, chests) on different height levels.

---

## 1. Goal & Requirements
- **Z-Axis representation**: Add a $Z$ coordinate (height in pixels) to game entities (players, NPCs).
- **Multi-layer map layout**:
  - **Ground layer**: Nền gạch (grass, ground).
  - **Decoration layer**: Đồ vật tĩnh (cây cối, đá, vách đá, bậc cầu thang) xếp lớp vẽ Y-sorting với nhân vật.
  - **Height level map**: Xác định độ cao nền $Z$ cho từng ô gạch (ví dụ: Z=0, Z=96).
  - **Collision and type grid**: Đánh dấu ô bị chặn, ô đi được và hướng của cầu thang dốc (Ramp).
- **Smooth stairs climbing**: Khi nhân vật đi trên dốc cầu thang, độ cao $Z$ tăng/giảm tuyến tính mượt mà dựa trên vị trí Cartesian của họ trong ô cầu thang.
- **Objects on Grass**:
  - Tĩnh: Các tile trang trí (cây, đá) ở layer 1.
  - Động: Thực thể (quái vật, rương) có thuộc tính $Z$ tương ứng với tầng cỏ họ đứng.

---

## 2. Technical Details & Proposed Changes

### 2.1. Network Packet Changes
File: [Packets.hpp](file:///d:/WorkSpace/Game/OU/src/Common/Network/Packets.hpp)

We need to add the `z` coordinate to the entity update packets so that the client renders them at the correct height:
- Add `float z` to `ServerEntityUpdateMsg` (Entity update).
- Add `float startZ` to `ServerLoginAckMsg` (Login confirmation).

```cpp
struct ServerEntityUpdateMsg {
    PacketHeader header;
    uint32_t entityId;
    float x;
    float y;
    float z; // <--- NEW: Z-axis position
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
    float startZ; // <--- NEW: Z-axis start position
    int32_t mapWidth;
    int32_t mapHeight;
};
```

---

### 2.2. Common Code Changes

#### 2.2.1. TransformComponent
File: [TransformComponent.hpp](file:///d:/WorkSpace/Game/OU/src/ECS/Components/TransformComponent.hpp)
- Add `float z = 0.0f` to store current height.
- Update constructors and defaults.

```cpp
class TransformComponent : public Component {
public:
    Vector2D position;
    Vector2D velocity;
    float z = 0.0f; // <--- NEW: Z position (height offset)

    int height = 32;
    int width = 32;
    int scale = 1;
    int speed = 3;
    bool blocked = false;
    // ... constructors ...
};
```

#### 2.2.2. MapData (Shared Map Parser)
File: [MapData.hpp](file:///d:/WorkSpace/Game/OU/src/Common/World/MapData.hpp) & [MapData.cpp](file:///d:/WorkSpace/Game/OU/src/Common/World/MapData.cpp)
We need to load the multi-layered map. The file `map.map` will be modified to have four blocks:
1. **Ground layer** (Grass/Ground)
2. **Decoration/Cliff/Stairs layer** (static objects and stairs)
3. **Height Map** (base Z value per tile: e.g., `00` to `99`)
4. **Collision & Tile Type Grid** (e.g. `0` = Walkable, `1` = Wall, `2` = Ramp SW-NE, `3` = Ramp SE-NW)

Let's expand `MapData` structures:
```cpp
enum class TileType : uint8_t {
    Walkable = 0,
    Blocked = 1,
    Ramp_SW_NE = 2, // Slope going up from West/South-West to East/North-East
    Ramp_SE_NW = 3  // Slope going up from East/South-East to West/North-West
};

class MapData {
public:
    bool LoadMap(const std::string& path, int sizeX, int sizeY);
    [[nodiscard]] bool IsBlocked(float x, float y, float z, float w, float h) const noexcept;
    [[nodiscard]] float GetTileHeight(int tileX, int tileY) const noexcept;
    [[nodiscard]] TileType GetTileType(int tileX, int tileY) const noexcept;
    [[nodiscard]] float GetInterpolatedHeight(float x, float y) const noexcept;

private:
    int width = 0;
    int height = 0;
    int tileSize = 32;
    int mapScale = 3;
    int scaledSize = 96;
    std::vector<std::vector<int>> groundGrid;
    std::vector<std::vector<int>> decorGrid;
    std::vector<std::vector<float>> heightGrid; // Z values per tile
    std::vector<std::vector<TileType>> typeGrid; // Tile types (collision/ramps)
};
```

Inside `MapData::GetInterpolatedHeight(float x, float y)`:
- Determine which tile `tileX`, `tileY` the coordinate resides in.
- If it's a regular tile (`TileType::Walkable`), return `heightGrid[tileY][tileX]`.
- If it's a ramp:
  - Calculate relative offset within the tile: `float dx = x - (tileX * 96)` and `float dy = y - (tileY * 96)`.
  - Let $Z_{base}$ be the height of the lower tile, and $Z_{diff} = 96.0f$ (one step height).
  - For `Ramp_SW_NE` (SW is lower, NE is higher): Z increases as X increases.
    $$Z = Z_{base} + \frac{dx}{96.0f} \times Z_{diff}$$
  - For `Ramp_SE_NW` (SE is lower, NW is higher): Z increases as Y decreases (going North).
    $$Z = Z_{base} + \frac{96.0f - dy}{96.0f} \times Z_{diff}$$
  - Clamp $Z$ to $[Z_{base}, Z_{base} + Z_{diff}]$.

---

### 2.3. Server-side Changes

#### 2.3.1. Client Connection & NPC Structures
File: [ServerApp.hpp](file:///d:/WorkSpace/Game/OU/src/Server/Core/ServerApp.hpp)
- Add `float z` to both `ClientConnection` and `ServerNPC`.

#### 2.3.2. Physics & Movement Logic
File: [ServerApp.cpp](file:///d:/WorkSpace/Game/OU/src/Server/Core/ServerApp.cpp)
In `ServerApp::updatePhysics()`:
- Calculate `nextX` and `nextY` as usual.
- Check collision: `mapData.IsBlocked(nextX + 118.0f, nextY + 80.0f, client->z, 48.0f, 70.0f)`.
- If not blocked:
  - Update `client->x = nextX` and `client->y = nextY`.
  - Update `client->z = mapData.GetInterpolatedHeight(client->x + 142.0f, client->y + 115.0f)` (calculated at foot/center position).

In `ServerApp::updateNPCs()`:
- Do the same height updates for monsters (npcs) so they slide smoothly on stairs and spawn at correct heights.

---

### 2.4. Client-side Changes

#### 2.4.1. TileComponent & Rendering
File: [TileComponent.hpp](file:///d:/WorkSpace/Game/OU/src/ECS/Components/TileComponent.hpp)
We need to load layers, and tile coordinates should be drawn considering the Z offset.
- Ground layer tiles drawn at their base tile $Z$ (e.g., Z=96 tiles drawn shifted up).
- Add `float z` to `TileComponent` so that tiles themselves are rendered at their correct height level!
- Rendering update:
  ```cpp
  void update() override {
    if (Game::isIsometric) {
      float isoX = (position.x - position.y) * 0.5f;
      float isoY = (position.x + position.y) * 0.25f - z; // <--- NEW: apply Z
      destRect.x = isoX - Game::camera.x;
      destRect.y = isoY - Game::camera.y;
    } else {
      destRect.x = position.x - Game::camera.x;
      destRect.y = position.y - Game::camera.y - z;
    }
  }
  ```

#### 2.2.2. Map Loader (Client-side)
File: [Map.hpp](file:///d:/WorkSpace/Game/OU/src/World/Map.hpp) & [Map.cpp](file:///d:/WorkSpace/Game/OU/src/World/Map.cpp)
- Update `Map::LoadMap` to parse the 4 sections from `map.map`.
- Instantiate tiles for ground layer (Layer 0) and decoration layer (Layer 1).
- Assign correct Z values to tiles so hills and stairs are drawn raised up!

#### 2.4.3. Sprite Rendering (Players & NPCs)
File: [SpriteComponent.hpp](file:///d:/WorkSpace/Game/OU/src/ECS/Components/SpriteComponent.hpp)
- In `update()`, adjust coordinate offset using `transform->z`:
  ```cpp
  if (Game::isIsometric) {
    float isoX = (transform->position.x - transform->position.y) * 0.5f;
    float isoY = (transform->position.x + transform->position.y) * 0.25f - transform->z; // <--- NEW
    destRect.x = isoX - Game::camera.x;
    destRect.y = isoY - Game::camera.y;
  } else {
    destRect.x = transform->position.x - Game::camera.x;
    destRect.y = transform->position.y - Game::camera.y - transform->z; // <--- NEW
  }
  ```

---

## 3. Verification Plan

### 3.1. Automated Verification
- Write a simple test map check logic or verify compilation:
  `cmake --build build --config Debug`

### 3.2. Manual Verification
1. Launch Server (`OUGameServer`).
2. Launch Client (`OUGameClient`).
3. Observe rendering of the new map (hills, mountains, decorations on grass, stairs).
4. Walk the player onto the stairs and check if the player slides upward smoothly on the Z axis.
5. Move off the stairs onto the hill and verify that the player remains at height Z=96.
6. Verify colliders at both levels (Z=0 and Z=96) behave correctly.
7. Check monsters (NPCs) spawning on the grass hills.
