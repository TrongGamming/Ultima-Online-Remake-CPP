#include "World/Map.hpp"

#include <SDL3/SDL.h>
#include <tinyxml2.h>
#include <sstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cctype>

#include "Core/Game.hpp"
#include "Core/TextureManager.hpp"
#include "ECS/Components/ColliderComponent.hpp"
#include "ECS/Components/TileComponent.hpp"
#include "ECS/ECS.hpp"
#include "Common/World/WorldConfig.hpp"
#include "Common/World/MapData.hpp" // Đính kèm để sử dụng enum class TileType

// Con trỏ tới ECS Manager toàn cục được định nghĩa trong Game.cpp
extern Manager manager;

// Constructor khởi tạo thông số bản đồ và tính toán kích thước ô gạch sau khi nhân scale
Map::Map(std::string tID, int ms, int ts)
    : texID(std::move(tID)), mapScale(ms), tileSize(ts) {
  scaledSize = ms * ts; // Ví dụ 3 * 32 = 96px
}

#include <zlib.h>

namespace {
const TilesetInfo* GetTilesetForGid(int gid, const std::vector<TilesetInfo>& tilesets) {
  if (gid == 0) return nullptr;
  const TilesetInfo* target = nullptr;
  for (auto const& ts : tilesets) {
    if (ts.firstgid <= gid) {
      target = &ts;
    } else {
      break;
    }
  }
  return target;
}

std::vector<uint8_t> DecodeBase64(const std::string& input) {
  const std::string b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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

std::vector<uint8_t> DecompressZlib(const std::vector<uint8_t>& compressedData, size_t expectedSize) {
  std::vector<uint8_t> decompressed(expectedSize);
  uLongf destLen = expectedSize;
  int res = uncompress(decompressed.data(), &destLen, compressedData.data(), compressedData.size());
  if (res != Z_OK) {
    std::cerr << "Map: Zlib decompression failed with error code: " << res << std::endl;
    return {};
  }
  decompressed.resize(destLen);
  return decompressed;
}

void ProcessLayerDataClient(const std::string& base64Text, int chunkX, int chunkY, int chunkW, int chunkH,
                            const std::vector<TilesetInfo>& tilesets, int zLevel, float layerZ, bool isCollision,
                            int mapW, int mapH, int scaledSize,
                            std::vector<std::vector<std::vector<TileType>>>& typeGrid3D,
                            std::vector<std::vector<bool>>& baseGrassCreated,
                            Map* mapInstance) {
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
      uint32_t gid = decompressed[idx] | 
                     (decompressed[idx + 1] << 8) | 
                     (decompressed[idx + 2] << 16) | 
                     (decompressed[idx + 3] << 24);
      gid = gid & ~0xF0000000; // Loại bỏ flipping flags

      if (isCollision) {
        if (gid > 0) {
          const TilesetInfo* ts = GetTilesetForGid(gid, tilesets);
          if (ts) {
            int localId = gid - ts->firstgid;
            TileType newType = static_cast<TileType>(localId);
            // Luôn ghi đè: collision layer cuối cùng thắng (đồng bộ với server)
            if (zLevel >= 0 && static_cast<size_t>(zLevel) < typeGrid3D.size()) {
              typeGrid3D[zLevel][mapY][mapX] = newType;
            }
          }
        }
      } else {
        if (gid > 0) {
          const TilesetInfo* ts = GetTilesetForGid(gid, tilesets);
          if (ts) {
            int localId = gid - ts->firstgid;
            int srcY = (localId / ts->columns) * ts->tileHeight;
            int srcX = (localId % ts->columns) * ts->tileWidth;

            // Vẽ TẤT CẢ gạch tại Z=0, giống cách Tiled render isometric map:
            // - Chiều cao trực quan (vách đá, dốc) đã được mã hóa SẴN trong sprite gạch
            // - Các layer đơn giản chồng lên nhau theo thứ tự (layer trên đè layer dưới)
            // - KHÔNG cần dịch vị trí Z vì sprite 32x32 đã bao gồm cả mặt trên và vách đá
            mapInstance->AddTile(srcX, srcY, mapX * scaledSize, mapY * scaledSize, 0.0f, ts->textureId, ts->tileHeight);
          }
        }
      }
    }
  }
}
}


// Hàm tải dữ liệu bản đồ từ file .tmx, khởi tạo các ô gạch và các hộp va chạm tương ứng
void Map::LoadMap(const std::string& path) {
  std::string actualPath = TextureManager::GetAssetPath(path);
  size_t dataSize = 0;
  char* fileData = static_cast<char*>(SDL_LoadFile(actualPath.c_str(), &dataSize));
  if (!fileData) {
    std::cerr << "Map: Failed to open TMX file via SDL_LoadFile: " << path
              << " Error: " << SDL_GetError() << std::endl;
    return;
  }

  tinyxml2::XMLDocument doc;
  tinyxml2::XMLError err = doc.Parse(fileData, dataSize);
  if (err != tinyxml2::XML_SUCCESS) {
    std::cerr << "Map: Failed to parse TMX XML: " << doc.ErrorStr() << std::endl;
    SDL_free(fileData);
    return;
  }

  tinyxml2::XMLElement* mapNode = doc.FirstChildElement("map");
  if (!mapNode) {
    std::cerr << "Map: TMX file lacks <map> element" << std::endl;
    SDL_free(fileData);
    return;
  }

  // Đọc kích thước bản đồ
  int mapW = 0;
  int mapH = 0;
  mapNode->QueryIntAttribute("width", &mapW);
  mapNode->QueryIntAttribute("height", &mapH);

  if (mapW <= 0 || mapH <= 0) {
    std::cerr << "Map: Invalid map dimensions: " << mapW << "x" << mapH << std::endl;
    SDL_free(fileData);
    return;
  }

  // Đọc tất cả các tileset và nạp texture động
  tilesets.clear();
  for (tinyxml2::XMLElement* tilesetNode = mapNode->FirstChildElement("tileset");
       tilesetNode != nullptr;
       tilesetNode = tilesetNode->NextSiblingElement("tileset")) {
    int firstgid = 0;
    tilesetNode->QueryIntAttribute("firstgid", &firstgid);

    const char* tsxSource = tilesetNode->Attribute("source");

    std::string name;
    int tileWidth = 32;
    int tileHeight = 32;
    int columns = 0;
    std::string imagePath;

    if (tsxSource != nullptr) {
      // Tileset ngoài
      std::string tsxFullPath = TextureManager::ResolvePath(actualPath, tsxSource);

      size_t tsxDataSize = 0;
      char* tsxData = static_cast<char*>(SDL_LoadFile(tsxFullPath.c_str(), &tsxDataSize));
      if (!tsxData) {
        std::cerr << "Map: Failed to load external tsx file: " << tsxFullPath << std::endl;
        continue;
      }

      tinyxml2::XMLDocument tsxDoc;
      if (tsxDoc.Parse(tsxData, tsxDataSize) == tinyxml2::XML_SUCCESS) {
        tinyxml2::XMLElement* tsNode = tsxDoc.FirstChildElement("tileset");
        if (tsNode) {
          const char* nameAttr = tsNode->Attribute("name");
          if (nameAttr) name = nameAttr;

          tsNode->QueryIntAttribute("tilewidth", &tileWidth);
          tsNode->QueryIntAttribute("tileheight", &tileHeight);
          tsNode->QueryIntAttribute("columns", &columns);

          tinyxml2::XMLElement* imgNode = tsNode->FirstChildElement("image");
          if (imgNode) {
            const char* imgSource = imgNode->Attribute("source");
            if (imgSource) {
              imagePath = TextureManager::ResolvePath(tsxFullPath, imgSource);
            }
          }
        }
      }
      SDL_free(tsxData);
    } else {
      // Tileset nhúng trực tiếp
      const char* nameAttr = tilesetNode->Attribute("name");
      if (nameAttr) name = nameAttr;

      tilesetNode->QueryIntAttribute("tilewidth", &tileWidth);
      tilesetNode->QueryIntAttribute("tileheight", &tileHeight);
      tilesetNode->QueryIntAttribute("columns", &columns);

      tinyxml2::XMLElement* imgNode = tilesetNode->FirstChildElement("image");
      if (imgNode) {
        const char* imgSource = imgNode->Attribute("source");
        if (imgSource) {
          imagePath = TextureManager::ResolvePath(actualPath, imgSource);
        }
      }
    }

    if (!name.empty() && !imagePath.empty()) {
      std::cout << "Map: Auto-loading tileset texture '" << name << "' from: " << imagePath << std::endl;
      // Dùng Game::assets->AddTexture để đăng ký texture vào AssetManager
      Game::assets->AddTexture(name, imagePath.c_str());

      TilesetInfo tsInfo;
      tsInfo.firstgid = firstgid;
      tsInfo.tileWidth = tileWidth;
      tsInfo.tileHeight = tileHeight;
      tsInfo.columns = columns;
      tsInfo.textureId = name;
      tilesets.push_back(tsInfo);
    }
  }

  // Sắp xếp tilesets theo firstgid tăng dần
  std::sort(tilesets.begin(), tilesets.end(), [](const TilesetInfo& a, const TilesetInfo& b) {
    return a.firstgid < b.firstgid;
  });

  // Lưu kích thước bản đồ
  width = mapW;
  height = mapH;

  // Khởi tạo lưới va chạm và đánh dấu cỏ nền
  typeGrid3D.clear();
  std::vector<std::vector<bool>> baseGrassCreated(mapH, std::vector<bool>(mapW, false));

  int terrainLayerCount = 0;

  // Duyệt qua tất cả các lớp Layer trong file TMX
  for (tinyxml2::XMLElement* layerNode = mapNode->FirstChildElement("layer");
       layerNode != nullptr;
       layerNode = layerNode->NextSiblingElement("layer")) {

    const char* layerName = layerNode->Attribute("name");
    if (!layerName) continue;

    std::string nameStr(layerName);
    bool isCollision = (nameStr.rfind("Collision", 0) == 0 || nameStr.rfind("collision", 0) == 0);

    tinyxml2::XMLElement* dataNode = layerNode->FirstChildElement("data");
    if (!dataNode) continue;

    const char* encoding = dataNode->Attribute("encoding");
    const char* compression = dataNode->Attribute("compression");
    if (!encoding || std::string(encoding) != "base64" || !compression || std::string(compression) != "zlib") {
      std::cerr << "Map WARNING: Layer '" << layerName << "' is not in base64/zlib format. Please configure it in Tiled." << std::endl;
      continue;
    }

    int zLevel = 0;
    float layerZ = 0.0f;
    if (!isCollision) {
      zLevel = terrainLayerCount;
      terrainLayerCount++;
      layerZ = static_cast<float>(zLevel * g_WorldConfig.blockHeight);
      if (zLevel >= 0 && static_cast<size_t>(zLevel) >= typeGrid3D.size()) {
        typeGrid3D.resize(zLevel + 1, std::vector<std::vector<TileType>>(mapH, std::vector<TileType>(mapW, TileType::Walkable)));
      }
    } else {
      int collLevel = (terrainLayerCount > 0) ? terrainLayerCount - 1 : 0;
      zLevel = collLevel;
      if (zLevel >= 0 && static_cast<size_t>(zLevel) >= typeGrid3D.size()) {
        typeGrid3D.resize(zLevel + 1, std::vector<std::vector<TileType>>(mapH, std::vector<TileType>(mapW, TileType::Walkable)));
      }
    }

    tinyxml2::XMLElement* chunkNode = dataNode->FirstChildElement("chunk");
    if (chunkNode) {
      for (; chunkNode != nullptr; chunkNode = chunkNode->NextSiblingElement("chunk")) {
        int chunkX = 0, chunkY = 0, chunkW = 0, chunkH = 0;
        chunkNode->QueryIntAttribute("x", &chunkX);
        chunkNode->QueryIntAttribute("y", &chunkY);
        chunkNode->QueryIntAttribute("width", &chunkW);
        chunkNode->QueryIntAttribute("height", &chunkH);

        const char* chunkText = chunkNode->GetText();
        if (!chunkText) continue;

        ProcessLayerDataClient(chunkText, chunkX, chunkY, chunkW, chunkH, tilesets, zLevel, layerZ, isCollision,
                               mapW, mapH, scaledSize, typeGrid3D, baseGrassCreated, this);
      }
    } else {
      const char* base64Text = dataNode->GetText();
      if (base64Text) {
        ProcessLayerDataClient(base64Text, 0, 0, mapW, mapH, tilesets, zLevel, layerZ, isCollision,
                               mapW, mapH, scaledSize, typeGrid3D, baseGrassCreated, this);
      }
    }
  }

  // Khởi tạo các thực thể va chạm địa hình dựa trên typeGrid3D
  // Mỗi Blocked tile ở mỗi z-level tạo một collider entity riêng,
  // với position.z tương ứng tầng collision để debug visual đúng vị trí.
  for (size_t z = 0; z < typeGrid3D.size(); ++z) {
    for (int y = 0; y < mapH; ++y) {
      for (int x = 0; x < mapW; ++x) {
        if (typeGrid3D[z][y][x] == TileType::Blocked) {
          auto& tcol(manager.addEntity());
          tcol.addComponent<ColliderComponent>("terrain", x * scaledSize,
                                               y * scaledSize, scaledSize);
          tcol.getComponent<TransformComponent>().position.z = static_cast<float>(z * g_WorldConfig.blockHeight);
          tcol.addGroup(Game::groupColliders);
        }
      }
    }
  }

  SDL_free(fileData);
  std::cout << "Map: Loaded Base64 Zlib TMX map '" << path << "' (" << mapW << "x" << mapH << ") with " << terrainLayerCount << " layers successfully." << std::endl;

  // Tự động in map ra terminal để debug
  PrintMap();
}

// Tạo mới một Entity ô gạch (Tile), đính kèm TileComponent và phân nhóm vào groupMap để vẽ
void Map::AddTile(int srcX, int srcY, int xpos, int ypos, float zpos, const std::string& textureId, int tSize) {
  auto& tile(manager.addEntity());
  tile.addComponent<TileComponent>(srcX, srcY, xpos, ypos, zpos, tSize, mapScale,
                                   textureId);
  tile.addGroup(Game::groupMap);
}

void Map::PrintMap() const {
  std::cout << "\n================= CLIENT MAP DEBUG PRINT =================" << std::endl;
  std::cout << "Dimensions: " << width << "x" << height << " tiles" << std::endl;
  std::cout << "Tile Size: " << tileSize << "px | Scale: x" << mapScale << " | Scaled Size: " << scaledSize << "px" << std::endl;
  std::cout << "Z-Levels: " << typeGrid3D.size() << std::endl;
  std::cout << "Loaded Tilesets (" << tilesets.size() << "):" << std::endl;
  for (size_t i = 0; i < tilesets.size(); ++i) {
    std::cout << "  [" << i << "] GID: " << tilesets[i].firstgid 
              << " | ID: " << tilesets[i].textureId 
              << " | Size: " << tilesets[i].tileWidth << "x" << tilesets[i].tileHeight 
              << " | Cols: " << tilesets[i].columns << std::endl;
  }

  for (size_t z = 0; z < typeGrid3D.size(); ++z) {
    std::cout << "\n--- Z-Level (Collision Layer) " << z << " ---" << std::endl;
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
  }
  std::cout << "==========================================================\n" << std::endl;
}

