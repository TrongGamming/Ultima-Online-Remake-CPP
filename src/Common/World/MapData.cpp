#include "Common/World/MapData.hpp"
#include <cmath>

#include <SDL3/SDL.h>
#include <tinyxml2.h>
#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <vector>

#include "Common/World/WorldConfig.hpp"

namespace {
int GetLocalId(int gid, const std::vector<int>& firstgids) {
  if (gid == 0) return 0;
  int targetFirstGid = 1;
  for (int fg : firstgids) {
    if (fg <= gid) {
      targetFirstGid = fg;
    } else {
      break;
    }
  }
  return gid - targetFirstGid;
}

std::vector<uint8_t> DecodeBase64(const std::string& input) {
  const std::string b64chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<int> T(256, -1);
  for (int i = 0; i < 64; i++) T[b64chars[i]] = i;

  std::vector<uint8_t> out;
  int val = 0, valb = -8;
  for (unsigned char c : input) {
    if (std::isspace(c)) continue;
    if (c == '=') break;
    if (T[c] == -1) continue;
    val = (val << 6) + T[c];
    valb += 6;
    if (valb >= 0) {
      out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}

std::vector<uint8_t> DecompressZlib(const std::vector<uint8_t>& compressedData,
                                    size_t expectedSize) {
  std::vector<uint8_t> decompressed(expectedSize);
  uLongf destLen = expectedSize;
  int res = uncompress(decompressed.data(), &destLen, compressedData.data(),
                       compressedData.size());
  if (res != Z_OK) {
    std::cerr << "MapData: Zlib decompression failed with error code: " << res
              << std::endl;
    return {};
  }
  decompressed.resize(destLen);
  return decompressed;
}

void ProcessLayerData(const std::string& base64Text, int chunkX, int chunkY,
                      int chunkW, int chunkH, const std::vector<int>& firstGids,
                      int zLevel, float layerZ, bool isCollision,
                      float collisionHeight,
                      int mapW, int mapH,
                      std::vector<std::vector<int>>& groundGrid,
                      std::vector<std::vector<int>>& decorGrid,
                      std::vector<std::vector<float>>& heightGrid3D_level,
                      std::vector<std::vector<std::vector<TileType>>>& typeGrid3D,
                      std::vector<std::vector<TileType>>& rampGrid3D_level) {
  std::vector<uint8_t> compressed = DecodeBase64(base64Text);
  size_t expectedSize = chunkW * chunkH * 4;
  std::vector<uint8_t> decompressed = DecompressZlib(compressed, expectedSize);
  if (decompressed.size() < expectedSize) return;

  for (int cy = 0; cy < chunkH; ++cy) {
    for (int cx = 0; cx < chunkW; ++cx) {
      int mapX = chunkX + cx;
      int mapY = chunkY + cy;
      if (mapX < 0 || mapX >= mapW || mapY < 0 || mapY >= mapH) continue;

      int idx = (cy * chunkW + cx) * 4;
      uint32_t gid = decompressed[idx] | (decompressed[idx + 1] << 8) |
                     (decompressed[idx + 2] << 16) |
                     (decompressed[idx + 3] << 24);
      gid = gid & ~0xF0000000;  // Loại bỏ flipping flags

      if (isCollision) {
        if (gid > 0) {
          int typeVal = GetLocalId(gid, firstGids);
          TileType newType = static_cast<TileType>(typeVal);

          // typeGrid3D: Luôn ghi đè cho zLevel hiện tại
          if (zLevel >= 0 && static_cast<size_t>(zLevel) < typeGrid3D.size()) {
            typeGrid3D[zLevel][mapY][mapX] = newType;
          }

          // rampGrid: Bảo toàn kiểu Ramp (không ghi đè ramp bằng non-ramp)
          // Dùng riêng cho nội suy dốc trong GetInterpolatedHeight
          if (newType == TileType::Ramp_SW_NE ||
              newType == TileType::Ramp_SE_NW) {
            rampGrid3D_level[mapY][mapX] = newType;
          }
        }
      } else {
        if (gid > 0) {
          heightGrid3D_level[mapY][mapX] = layerZ;
          int localId = GetLocalId(gid, firstGids);
          if (zLevel == 0) {
            groundGrid[mapY][mapX] = localId;
          } else {
            decorGrid[mapY][mapX] = localId;
          }
        }
      }
    }
  }
}
}  // namespace

// Tải dữ liệu bản đồ từ file .tmx XML (định dạng Base64 Zlib)
bool MapData::LoadMap(const std::string& path) {
  size_t dataSize = 0;
  char* fileData = static_cast<char*>(SDL_LoadFile(path.c_str(), &dataSize));
  if (!fileData) {
    std::cerr << "MapData: Failed to open TMX file via SDL_LoadFile: " << path
              << " Error: " << SDL_GetError() << std::endl;
    return false;
  }

  tinyxml2::XMLDocument doc;
  tinyxml2::XMLError err = doc.Parse(fileData, dataSize);
  if (err != tinyxml2::XML_SUCCESS) {
    std::cerr << "MapData: Failed to parse TMX XML: " << doc.ErrorStr()
              << std::endl;
    SDL_free(fileData);
    return false;
  }

  tinyxml2::XMLElement* mapNode = doc.FirstChildElement("map");
  if (!mapNode) {
    std::cerr << "MapData: TMX file lacks <map> element: " << path << std::endl;
    SDL_free(fileData);
    return false;
  }

  // Đọc kích thước bản đồ
  int mapW = 0;
  int mapH = 0;
  mapNode->QueryIntAttribute("width", &mapW);
  mapNode->QueryIntAttribute("height", &mapH);

  if (mapW <= 0 || mapH <= 0) {
    std::cerr << "MapData: Invalid map dimensions: " << mapW << "x" << mapH
              << std::endl;
    SDL_free(fileData);
    return false;
  }

  width = mapW;
  height = mapH;

  // Đọc danh sách firstgid của tất cả tileset để chuyển đổi gid toàn cục sang
  // local index
  std::vector<int> firstGids;
  for (tinyxml2::XMLElement* tilesetNode =
           mapNode->FirstChildElement("tileset");
       tilesetNode != nullptr;
       tilesetNode = tilesetNode->NextSiblingElement("tileset")) {
    int fg = 0;
    if (tilesetNode->QueryIntAttribute("firstgid", &fg) ==
        tinyxml2::XML_SUCCESS) {
      firstGids.push_back(fg);
    }
  }
  std::sort(firstGids.begin(), firstGids.end());

  // Phân bổ bộ nhớ cho các lưới dữ liệu đa tầng
  groundGrid.assign(height, std::vector<int>(width, 0));
  decorGrid.assign(height, std::vector<int>(width, 0));
  heightGrid3D.clear();
  typeGrid3D.clear();
  rampGrid3D.clear();

  int terrainLayerCount = 0;

  // Duyệt qua tất cả các lớp Layer trong file TMX
  for (tinyxml2::XMLElement* layerNode = mapNode->FirstChildElement("layer");
       layerNode != nullptr;
       layerNode = layerNode->NextSiblingElement("layer")) {
    const char* layerName = layerNode->Attribute("name");
    if (!layerName) continue;

    tinyxml2::XMLElement* dataNode = layerNode->FirstChildElement("data");
    if (!dataNode) continue;

    const char* encoding = dataNode->Attribute("encoding");
    const char* compression = dataNode->Attribute("compression");
    if (!encoding || std::string(encoding) != "base64" || !compression ||
        std::string(compression) != "zlib") {
      std::cerr
          << "MapData WARNING: Layer '" << layerName
          << "' is not in base64/zlib format. Please configure it in Tiled."
          << std::endl;
      continue;
    }

    std::string nameStr(layerName);
    bool isCollision = (nameStr.rfind("Collision", 0) == 0 ||
                        nameStr.rfind("collision", 0) == 0);
    int zLevel = 0;
    float layerZ = 0.0f;
    float collisionHeight = 0.0f;
    if (!isCollision) {
      zLevel = terrainLayerCount;
      terrainLayerCount++;
      layerZ = static_cast<float>(zLevel * g_WorldConfig.blockHeight);
      if (zLevel >= 0 && static_cast<size_t>(zLevel) >= typeGrid3D.size()) {
        typeGrid3D.resize(zLevel + 1, std::vector<std::vector<TileType>>(height, std::vector<TileType>(width, TileType::Walkable)));
        heightGrid3D.resize(zLevel + 1, std::vector<std::vector<float>>(height, std::vector<float>(width, static_cast<float>(zLevel * g_WorldConfig.blockHeight))));
        rampGrid3D.resize(zLevel + 1, std::vector<std::vector<TileType>>(height, std::vector<TileType>(width, TileType::Walkable)));
      }
    } else {
      // Mỗi collision layer tương ứng với terrain layer TRƯỚC nó
      int collLevel = (terrainLayerCount > 0) ? terrainLayerCount - 1 : 0;
      collisionHeight = static_cast<float>(collLevel * g_WorldConfig.blockHeight);
      zLevel = collLevel;
      if (zLevel >= 0 && static_cast<size_t>(zLevel) >= typeGrid3D.size()) {
        typeGrid3D.resize(zLevel + 1, std::vector<std::vector<TileType>>(height, std::vector<TileType>(width, TileType::Walkable)));
        heightGrid3D.resize(zLevel + 1, std::vector<std::vector<float>>(height, std::vector<float>(width, static_cast<float>(zLevel * g_WorldConfig.blockHeight))));
        rampGrid3D.resize(zLevel + 1, std::vector<std::vector<TileType>>(height, std::vector<TileType>(width, TileType::Walkable)));
      }
    }

    // Kiểm tra xem đây có phải là bản đồ vô hạn (có chứa chunk) hay bản đồ kích
    // thước cố định
    tinyxml2::XMLElement* chunkNode = dataNode->FirstChildElement("chunk");
    if (chunkNode) {
      // Trường hợp 1: Bản đồ vô hạn (Tiled Infinite map)
      for (; chunkNode != nullptr;
           chunkNode = chunkNode->NextSiblingElement("chunk")) {
        int chunkX = 0, chunkY = 0, chunkW = 0, chunkH = 0;
        chunkNode->QueryIntAttribute("x", &chunkX);
        chunkNode->QueryIntAttribute("y", &chunkY);
        chunkNode->QueryIntAttribute("width", &chunkW);
        chunkNode->QueryIntAttribute("height", &chunkH);

        const char* chunkText = chunkNode->GetText();
        if (!chunkText) continue;

        ProcessLayerData(chunkText, chunkX, chunkY, chunkW, chunkH, firstGids,
                         zLevel, layerZ, isCollision, collisionHeight,
                         width, height, groundGrid,
                         decorGrid, heightGrid3D[zLevel], typeGrid3D,
                         rampGrid3D[zLevel]);
      }
    } else {
      // Trường hợp 2: Bản đồ kích thước cố định (Fixed size map)
      const char* base64Text = dataNode->GetText();
      if (base64Text) {
        ProcessLayerData(base64Text, 0, 0, width, height, firstGids, zLevel,
                         layerZ, isCollision, collisionHeight,
                         width, height, groundGrid,
                         decorGrid, heightGrid3D[zLevel], typeGrid3D,
                         rampGrid3D[zLevel]);
      }
    }
  }

  SDL_free(fileData);
  std::cout << "MapData: Loaded Base64 Zlib TMX map '" << path << "' (" << width
            << "x" << height << ") with " << terrainLayerCount
            << " height layers successfully." << std::endl;

  // Tự động in map ra terminal để debug
  PrintMap();

  return true;
}

// Kiểm tra xem một ô gạch cụ thể có cản trở thực thể đứng ở cao độ entityZ không.
// Duyệt TẤT CẢ z-level để kiểm tra collision chính xác.
bool MapData::IsTileBlocked(int tileX, int tileY,
                            float entityZ) const noexcept {
  if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height) return true;
  if (typeGrid3D.empty()) return false;

  int maxZ = static_cast<int>(typeGrid3D.size());
  int entityZLevel = static_cast<int>((entityZ + 1.0f) / g_WorldConfig.blockHeight);
  if (entityZLevel < 0) entityZLevel = 0;
  if (entityZLevel >= maxZ) entityZLevel = maxZ - 1;

  // Duyệt tất cả z-level để kiểm tra Blocked
  for (int zl = 0; zl < maxZ; ++zl) {
    TileType type = typeGrid3D[zl][tileY][tileX];
    if (type == TileType::Blocked) {
      float collisionFloorZ = static_cast<float>(zl * g_WorldConfig.blockHeight);
      float collisionCeilZ = collisionFloorZ + g_WorldConfig.blockHeight;
      // Entity bị block nếu z nằm trong phạm vi tầng collision [floor, ceil)
      if (entityZ >= collisionFloorZ - 1.0f && entityZ < collisionCeilZ) {
        return true;
      }
    }
  }

  // Kiểm tra chênh lệch chiều cao tại zLevel hiện tại của entity
  if (entityZLevel >= 0 && static_cast<size_t>(entityZLevel) < heightGrid3D.size()) {
    float tileZ = heightGrid3D[entityZLevel][tileY][tileX];
    TileType type = typeGrid3D[entityZLevel][tileY][tileX];
    if (type == TileType::Walkable &&
        std::abs(entityZ - tileZ) > g_WorldConfig.blockHeight * 0.5f) {
      return true;
    }
  }

  return false;
}

// Kiểm tra va chạm hộp (AABB) của thực thể tại tọa độ (x, y) với kích thước (w,
// h) ở độ cao z. Duyệt TẤT CẢ z-level để xác định collision chính xác.
bool MapData::IsBlocked(float x, float y, float z, float w,
                        float h) const noexcept {
  int x0 = static_cast<int>(x) / scaledSize;
  int y0 = static_cast<int>(y) / scaledSize;
  int x1 = static_cast<int>(x + w - 1.0f) / scaledSize;
  int y1 = static_cast<int>(y + h - 1.0f) / scaledSize;

  if (typeGrid3D.empty()) return false;
  int maxZ = static_cast<int>(typeGrid3D.size());

  // Tìm zLevel hiện tại của entity
  int entityZLevel = static_cast<int>((z + 1.0f) / g_WorldConfig.blockHeight);
  if (entityZLevel < 0) entityZLevel = 0;
  if (entityZLevel >= maxZ) entityZLevel = maxZ - 1;

  // Kiểm tra tất cả tile trong AABB bounding box
  for (int ty = y0; ty <= y1; ++ty) {
    for (int tx = x0; tx <= x1; ++tx) {
      // Ngoài biên bản đồ → chặn
      if (tx < 0 || ty < 0 || tx >= width || ty >= height) return true;

      // Duyệt tất cả z-level để kiểm tra collision
      for (int zl = 0; zl < maxZ; ++zl) {
        TileType type = typeGrid3D[zl][ty][tx];
        if (type == TileType::Blocked) {
          // Tầng collision này block ô gạch này.
          // Chỉ block entity nếu entity đang ở cùng tầng hoặc bên dưới tầng collision.
          // Ý nghĩa: Collision_1 (zLevel=0) block entity ở z=0.
          //           Collision_2 (zLevel=1) block entity ở z=48 (blockHeight).
          // Entity ở tầng thấp hơn collision thì collision không ảnh hưởng.
          // Entity ở tầng cao hơn collision thì collision không ảnh hưởng (đã vượt qua).
          float collisionFloorZ = static_cast<float>(zl * g_WorldConfig.blockHeight);
          float collisionCeilZ = collisionFloorZ + g_WorldConfig.blockHeight;
          // Entity bị block nếu z nằm trong phạm vi tầng collision [floor, ceil)
          if (z >= collisionFloorZ - 1.0f && z < collisionCeilZ) {
            return true;
          }
        }
      }
    }
  }

  // Kiểm tra chênh lệch chiều cao tại pivot point (tâm entity)
  int pivotTileX = static_cast<int>(x + w / 2.0f) / scaledSize;
  int pivotTileY = static_cast<int>(y + h / 2.0f) / scaledSize;

  if (pivotTileX >= 0 && pivotTileY >= 0 && pivotTileX < width &&
      pivotTileY < height) {
    // Kiểm tra height mismatch tại zLevel hiện tại của entity
    if (entityZLevel >= 0 && static_cast<size_t>(entityZLevel) < heightGrid3D.size()) {
      float tileZ = heightGrid3D[entityZLevel][pivotTileY][pivotTileX];
      TileType type = typeGrid3D[entityZLevel][pivotTileY][pivotTileX];
      if (type == TileType::Walkable &&
          std::abs(z - tileZ) > g_WorldConfig.blockHeight * 0.5f) {
        return true;
      }
    }
  }

  return false;
}

// Lấy chiều cao Z gốc của một ô gạch cụ thể
float MapData::GetTileHeight(int tileX, int tileY, int zLevel) const noexcept {
  if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height) return 0.0f;
  if (zLevel < 0 || static_cast<size_t>(zLevel) >= heightGrid3D.size()) return 0.0f;
  return heightGrid3D[zLevel][tileY][tileX];
}

// Lấy kiểu địa hình của một ô gạch cụ thể
TileType MapData::GetTileType(int tileX, int tileY, int zLevel) const noexcept {
  if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height)
    return TileType::Blocked;
  if (zLevel < 0 || static_cast<size_t>(zLevel) >= typeGrid3D.size()) return TileType::Walkable;
  return typeGrid3D[zLevel][tileY][tileX];
}

// Nội suy cao độ Z thực tế tại tọa độ pixel (x, y) để tạo chuyển động trơn tru
// khi lên/xuống dốc (Ramp)
float MapData::GetInterpolatedHeight(float x, float y, float entityZ) const noexcept {
  // Chuyển đổi tọa độ pixel thế giới sang chỉ số ô gạch
  int tileX = static_cast<int>(x) / scaledSize;
  int tileY = static_cast<int>(y) / scaledSize;

  // Ngoài biên bản đồ thì độ cao bằng 0
  if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height) return 0.0f;

  // Thêm 1.0f epsilon để tránh làm tròn xuống sai zLevel khi đang đứng đúng biên Z
  int zLevel = static_cast<int>((entityZ + 1.0f) / g_WorldConfig.blockHeight);
  if (zLevel < 0) zLevel = 0;
  if (rampGrid3D.empty()) return entityZ;
  if (static_cast<size_t>(zLevel) >= rampGrid3D.size()) zLevel = static_cast<int>(rampGrid3D.size()) - 1;

  // Dùng rampGrid thay vì typeGrid để nội suy dốc
  // rampGrid bảo toàn kiểu Ramp ngay cả khi collision layer cao hơn ghi đè typeGrid
  TileType type = rampGrid3D[zLevel][tileY][tileX];
  float baseH = heightGrid3D[zLevel][tileY][tileX];

  // NẾU TILE HIỆN TẠI LÀ WALKABLE (Không có dốc ở layer hiện tại)
  // VÀ LAYER BÊN DƯỚI LÀ RAMP, CHÚNG TA ĐANG Ở ĐỈNH DỐC -> PHẢI DÙNG RAMP BÊN DƯỚI ĐỂ CÓ THỂ ĐI XUỐNG!
  if (type == TileType::Walkable && zLevel > 0) {
    TileType typeBelow = rampGrid3D[zLevel - 1][tileY][tileX];
    if (typeBelow == TileType::Ramp_SW_NE || typeBelow == TileType::Ramp_SE_NW) {
      type = typeBelow;
      baseH = heightGrid3D[zLevel - 1][tileY][tileX];
    }
  }

  // Nếu là dốc từ Tây Nam (SW) lên Đông Bắc (NE)
  if (type == TileType::Ramp_SW_NE) {
    // Độ cao Z tăng dần khi tọa độ X tăng trong lòng ô gạch này
    float dx = x - static_cast<float>(tileX * scaledSize);
    float ratio = dx / static_cast<float>(scaledSize);
    // Giới hạn tỉ lệ ratio trong khoảng [0.0f, 1.0f]
    ratio = (ratio < 0.0f) ? 0.0f : ((ratio > 1.0f) ? 1.0f : ratio);

    // Chiều cao nội suy từ baseH (chân dốc) đến đỉnh dốc (baseH + blockHeight)
    return baseH + ratio * static_cast<float>(g_WorldConfig.blockHeight);
  }
  // Nếu là dốc từ Đông Nam (SE) lên Tây Bắc (NW)
  else if (type == TileType::Ramp_SE_NW) {
    // Độ cao Z tăng dần khi tọa độ Y giảm trong lòng ô gạch này (di chuyển về
    // hướng Bắc)
    float dy = y - static_cast<float>(tileY * scaledSize);
    float ratio =
        (static_cast<float>(scaledSize) - dy) / static_cast<float>(scaledSize);
    ratio = (ratio < 0.0f) ? 0.0f : ((ratio > 1.0f) ? 1.0f : ratio);

    // Chiều cao nội suy từ baseH (chân dốc) đến đỉnh dốc (baseH + blockHeight)
    return baseH + ratio * static_cast<float>(g_WorldConfig.blockHeight);
  }

  // Nếu là ô phẳng thông thường, trả về chiều cao gốc cố định của ô gạch đó
  return baseH;
}

void MapData::PrintMap() const {
  std::cout << "\n================= SERVER MAPDATA DEBUG PRINT =================" << std::endl;
  std::cout << "Dimensions: " << width << "x" << height << " tiles" << std::endl;
  std::cout << "Tile Size: " << tileSize << "px | Scale: x" << mapScale << " | Scaled Size: " << scaledSize << "px" << std::endl;
  std::cout << "Z-Levels: " << typeGrid3D.size() << std::endl;

  for (size_t z = 0; z < typeGrid3D.size(); ++z) {
    std::cout << "\n--- Z-Level " << z << " Collision Grid (typeGrid3D) ---" << std::endl;
    for (int y = 0; y < height; ++y) {
      if (y < 10) std::cout << " ";
      std::cout << y << " | ";
      
      for (int x = 0; x < width; ++x) {
        char symbol = '.';
        switch (typeGrid3D[z][y][x]) {
          case TileType::Walkable:
            symbol = '.';
            break;
          case TileType::Blocked:
            symbol = '#';
            break;
          case TileType::Ramp_SW_NE:
            symbol = '/';
            break;
          case TileType::Ramp_SE_NW:
            symbol = '\\';
            break;
        }
        std::cout << symbol << " ";
      }
      std::cout << std::endl;
    }
    
    std::cout << "     ";
    for (int x = 0; x < width; ++x) {
      std::cout << (x % 10) << " ";
    }
    std::cout << std::endl;

    if (z < heightGrid3D.size()) {
      std::cout << "\n--- Z-Level " << z << " Height Grid (heightGrid3D in pixels) ---" << std::endl;
      for (int y = 0; y < height; ++y) {
        if (y < 10) std::cout << " ";
        std::cout << y << " | ";
        for (int x = 0; x < width; ++x) {
          std::cout << static_cast<int>(heightGrid3D[z][y][x]) << "\t";
        }
        std::cout << std::endl;
      }
      
      std::cout << "     ";
      for (int x = 0; x < width; ++x) {
        std::cout << x << "\t";
      }
      std::cout << std::endl;
    }
  }
  std::cout << "==============================================================\n" << std::endl;
}
