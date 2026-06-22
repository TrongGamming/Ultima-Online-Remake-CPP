#include "World/Map.hpp"

#include <SDL3/SDL.h> // Bao gồm thư viện SDL3 cho các hàm tiện ích và load file
#include <tinyxml2.h> // Bao gồm thư viện tinyxml2 để phân tích cú pháp (parse) file XML (.tmx)
#include <sstream>    // Dùng cho thao tác luồng chuỗi (string stream)
#include <iostream>   // Dùng cho luồng vào ra chuẩn (in log ra console)
#include <vector>     // Dùng cấu trúc dữ liệu mảng động (vector)
#include <algorithm>  // Dùng các thuật toán tiêu chuẩn như std::sort
#include <cctype>     // Dùng các hàm kiểm tra ký tự (như std::isspace)

#include "Core/Game.hpp" // Bao gồm class Game để truy cập các biến toàn cục (asset manager)
#include "Core/TextureManager.hpp" // Tiện ích quản lý đường dẫn và hình ảnh
#include "ECS/Components/ColliderComponent.hpp" // Thành phần va chạm cho ECS
#include "ECS/Components/TileComponent.hpp" // Thành phần gạch (Tile) cho ECS
#include "ECS/ECS.hpp" // Hệ thống Entity Component System
#include "Common/World/WorldConfig.hpp" // Chứa các hằng số cấu hình thế giới (như blockHeight)
#include "Common/World/MapData.hpp" // Đính kèm để sử dụng enum class TileType

// Con trỏ tới ECS Manager toàn cục được định nghĩa trong Game.cpp, quản lý tất cả các Entity
extern Manager manager;

// Constructor khởi tạo đối tượng Map. Nhận vào tID (không còn sử dụng nhiều nhưng giữ nguyên chữ ký), map scale, tile size
Map::Map(std::string tID, int ms, int ts)
    : texID(std::move(tID)), mapScale(ms), tileSize(ts) {
  // Tính toán kích thước ô gạch thực tế trên màn hình sau khi nhân với scale
  scaledSize = ms * ts; // Ví dụ: mapScale=3, tileSize=32 => scaledSize = 96px
}

#include <zlib.h> // Bao gồm thư viện zlib để giải nén dữ liệu map được nén bằng zlib

// Sử dụng không gian tên (namespace) ẩn danh để định nghĩa các hàm chỉ dùng nội bộ trong file Map.cpp
namespace {

// Hàm lấy thông tin Tileset dựa trên mã định danh ô gạch toàn cục (Global ID - GID)
const TilesetInfo* GetTilesetForGid(int gid, const std::vector<TilesetInfo>& tilesets) {
  if (gid == 0) return nullptr; // GID = 0 trong Tiled map có nghĩa là ô trống (không có gạch)
  const TilesetInfo* target = nullptr; // Khởi tạo con trỏ kết quả
  // Duyệt qua danh sách các tilesets đã tải
  for (auto const& ts : tilesets) {
    // Nếu firstgid của tileset này <= gid cần tìm, nó CÓ THỂ là tileset chứa ô gạch đó
    if (ts.firstgid <= gid) {
      target = &ts; // Lưu lại tham chiếu đến tileset có khả năng cao nhất
    } else {
      break; // Vì danh sách tilesets đã được sắp xếp tăng dần theo firstgid, nếu firstgid > gid thì dừng luôn
    }
  }
  return target; // Trả về tileset đã tìm thấy
}

// Hàm giải mã dữ liệu chuỗi dạng Base64 thành mảng các byte (uint8_t)
std::vector<uint8_t> DecodeBase64(const std::string& input) {
  // Chuỗi chứa các ký tự hợp lệ trong Base64
  const std::string b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<int> T(256, -1); // Bảng tra cứu (Lookup table) cho các ký tự ASCII, khởi tạo bằng -1
  for (int i = 0; i < 64; i++) T[b64chars[i]] = i; // Khởi tạo giá trị từ 0->63 cho các ký tự hợp lệ

  std::vector<uint8_t> out; // Mảng lưu trữ kết quả giải mã
  int val = 0, valb = -8; // val: bộ đệm bit tạm thời, valb: số bit hiện có trong bộ đệm trừ đi 8
  for (unsigned char c : input) {
    if (std::isspace(c)) continue; // Bỏ qua khoảng trắng (dấu cách, xuống dòng, v.v.)
    if (c == '=') break; // Gặp ký tự padding '=', kết thúc giải mã
    if (T[c] == -1) continue; // Bỏ qua ký tự không hợp lệ không nằm trong bảng tra cứu
    
    val = (val << 6) + T[c]; // Dịch trái 6 bit và thêm giá trị giải mã 6 bit của ký tự hiện tại
    valb += 6; // Cộng thêm 6 bit vào bộ đếm
    
    // Nếu có đủ 8 bit (tức là valb >= 0) thì trích xuất 1 byte
    if (valb >= 0) {
      // Dịch phải bộ đệm để lấy 8 bit cao nhất, dùng & 0xFF để đảm bảo kết quả là 1 byte
      out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
      valb -= 8; // Đã lấy 8 bit, trừ đi khỏi bộ đếm
    }
  }
  return out; // Trả về mảng byte đã giải mã
}

// Hàm giải nén dữ liệu nén Zlib thành mảng byte ban đầu
std::vector<uint8_t> DecompressZlib(const std::vector<uint8_t>& compressedData, size_t expectedSize) {
  std::vector<uint8_t> decompressed(expectedSize); // Tạo mảng chứa kết quả với kích thước dự kiến
  uLongf destLen = expectedSize; // Biến uLongf của zlib lưu kích thước bộ đệm đích
  
  // Gọi hàm uncompress của zlib: (đích, độ dài đích, nguồn, độ dài nguồn)
  int res = uncompress(decompressed.data(), &destLen, compressedData.data(), compressedData.size());
  
  // Kiểm tra kết quả giải nén
  if (res != Z_OK) {
    std::cerr << "Map: Zlib decompression failed with error code: " << res << std::endl;
    return {}; // Trả về mảng rỗng nếu giải nén thất bại
  }
  
  decompressed.resize(destLen); // Chỉnh lại kích thước mảng nếu giải nén nhỏ hơn dự kiến (hiếm khi xảy ra nếu expectedSize đúng)
  return decompressed; // Trả về mảng dữ liệu đã giải nén
}

// Hàm xử lý dữ liệu của một Chunk trong Layer (hỗ trợ Infinite Map hoặc Fixed Map của Tiled)
void ProcessLayerDataClient(const std::string& base64Text, int chunkX, int chunkY, int chunkW, int chunkH,
                            const std::vector<TilesetInfo>& tilesets, int zLevel, float layerZ, bool isCollision,
                            int mapW, int mapH, int scaledSize,
                            std::vector<std::vector<std::vector<TileType>>>& typeGrid3D,
                            std::vector<std::vector<std::vector<float>>>& heightGrid3D_level,
                            std::vector<std::vector<std::vector<TileType>>>& rampGrid3D_level,
                            std::vector<std::vector<bool>>& baseGrassCreated,
                            Map* mapInstance) {
  // Giải mã Base64 sang nén Zlib
  std::vector<uint8_t> compressed = DecodeBase64(base64Text);
  // Kích thước dự kiến: rộng * cao * 4 byte (mỗi GID là uint32_t chiếm 4 byte)
  size_t expectedSize = chunkW * chunkH * 4;
  // Giải nén Zlib sang dạng mảng byte
  std::vector<uint8_t> decompressed = DecompressZlib(compressed, expectedSize);
  
  // Đảm bảo dữ liệu giải nén đủ số lượng byte mong muốn
  if (decompressed.size() < expectedSize) return;

  // Duyệt qua từng ô gạch trong chunk (theo chiều Y)
  for (int cy = 0; cy < chunkH; ++cy) {
    // Duyệt qua từng ô gạch trong chunk (theo chiều X)
    for (int cx = 0; cx < chunkW; ++cx) {
      // Tính toán tọa độ thực tế trên bản đồ
      int mapX = chunkX + cx;
      int mapY = chunkY + cy;
      
      // Bỏ qua nếu tọa độ nằm ngoài giới hạn kích thước bản đồ
      if (mapX < 0 || mapX >= mapW || mapY < 0 || mapY >= mapH) continue;

      // Tính vị trí byte của ô gạch trong mảng giải nén (mỗi ô chiếm 4 byte)
      int idx = (cy * chunkW + cx) * 4;
      
      // Ghép 4 byte thành số nguyên 32-bit kiểu Little-Endian (cấu trúc mặc định của Tiled)
      uint32_t gid = decompressed[idx] | 
                     (decompressed[idx + 1] << 8) | 
                     (decompressed[idx + 2] << 16) | 
                     (decompressed[idx + 3] << 24);
                     
      // Lấy GID thực sự bằng cách loại bỏ 3 bit cờ lật (flipping flags) ở dạng bitmask (~0xF0000000)
      gid = gid & ~0xF0000000;

      // Nếu là lớp Va chạm (Collision Layer)
      if (isCollision) {
        if (gid > 0) { // Ô có gạch (khác 0)
          // Lấy tileset chứa gid này
          const TilesetInfo* ts = GetTilesetForGid(gid, tilesets);
          if (ts) {
            // Lấy ID nội bộ (Local ID) trong tileset đó (từ 0)
            int localId = gid - ts->firstgid;
            // Ép kiểu Local ID sang enum TileType (quy định: 1=Blocked, 2=Ramp_SW_NE, v.v...)
            TileType newType = static_cast<TileType>(localId);
            
            // Ghi đè kiểu va chạm vào lưới grid 3 chiều (z, y, x). Lớp va chạm trên cùng sẽ ghi đè lớp dưới
            if (zLevel >= 0 && static_cast<size_t>(zLevel) < typeGrid3D.size()) {
              typeGrid3D[zLevel][mapY][mapX] = newType;
            }

            // Ghi vào rampGrid3D (chỉ lưu ramp, không bị ghi đè bởi blocked)
            if (newType == TileType::Ramp_SW_NE || newType == TileType::Ramp_SE_NW ||
                newType == TileType::Ramp_NE_SW || newType == TileType::Ramp_NW_SE) {
              if (zLevel >= 0 && static_cast<size_t>(zLevel) < rampGrid3D_level.size()) {
                rampGrid3D_level[zLevel][mapY][mapX] = newType;
              }
            }
          }
        }
      } 
      // Nếu là lớp Địa hình/Trang trí (Terrain/Decor Layer)
      else {
        if (gid > 0) {
          // Cập nhật heightGrid3D với layerZ của terrain layer
          if (zLevel >= 0 && static_cast<size_t>(zLevel) < heightGrid3D_level.size()) {
            heightGrid3D_level[zLevel][mapY][mapX] = layerZ;
          }

          const TilesetInfo* ts = GetTilesetForGid(gid, tilesets);
          if (ts) {
            int localId = gid - ts->firstgid; // Local ID
            
            // Tính toán vị trí Y trên file ảnh Texture (spritesheet) của tileset
            int srcY = (localId / ts->columns) * ts->tileHeight;
            // Tính toán vị trí X trên file ảnh Texture của tileset
            int srcX = (localId % ts->columns) * ts->tileWidth;

            // Tạo đối tượng Tile Entity (để Render ra màn hình) thông qua Map instance
            // Truyền vị trí nguồn (srcX, srcY), vị trí hiển thị (mapX*scaledSize), zpos = 0.0f
            // Lưu ý: Isometric 2.5D dùng zpos = 0 vì độ cao được mã hoá sẵn trong ảnh spritesheet
            mapInstance->AddTile(srcX, srcY, mapX * scaledSize, mapY * scaledSize, 0.0f, ts->textureId, ts->tileHeight);
          }
        }
      }
    }
  }
}
} // Kết thúc namespace ẩn danh


// Phương thức: Tải dữ liệu bản đồ từ file .tmx, khởi tạo các ô gạch và các hộp va chạm tương ứng
void Map::LoadMap(const std::string& path) {
  // Lấy đường dẫn thực tế từ AssetManager (thư mục gốc chứa game)
  std::string actualPath = TextureManager::GetAssetPath(path);
  size_t dataSize = 0;
  
  // Dùng SDL_LoadFile để đọc toàn bộ file TMX vào bộ nhớ (hoạt động tốt với đa nền tảng)
  char* fileData = static_cast<char*>(SDL_LoadFile(actualPath.c_str(), &dataSize));
  if (!fileData) {
    std::cerr << "Map: Failed to open TMX file via SDL_LoadFile: " << path
              << " Error: " << SDL_GetError() << std::endl;
    return;
  }

  // Khởi tạo tài liệu XML dùng thư viện tinyxml2
  tinyxml2::XMLDocument doc;
  // Phân tích cú pháp dữ liệu đọc từ file
  tinyxml2::XMLError err = doc.Parse(fileData, dataSize);
  if (err != tinyxml2::XML_SUCCESS) {
    std::cerr << "Map: Failed to parse TMX XML: " << doc.ErrorStr() << std::endl;
    SDL_free(fileData); // Giải phóng bộ nhớ nếu lỗi
    return;
  }

  // Lấy node gốc <map> của file XML
  tinyxml2::XMLElement* mapNode = doc.FirstChildElement("map");
  if (!mapNode) {
    std::cerr << "Map: TMX file lacks <map> element" << std::endl;
    SDL_free(fileData);
    return;
  }

  // Đọc thuộc tính "width" và "height" của bản đồ (số lượng ô gạch)
  int mapW = 0;
  int mapH = 0;
  mapNode->QueryIntAttribute("width", &mapW);
  mapNode->QueryIntAttribute("height", &mapH);

  // Kiểm tra kích thước hợp lệ
  if (mapW <= 0 || mapH <= 0) {
    std::cerr << "Map: Invalid map dimensions: " << mapW << "x" << mapH << std::endl;
    SDL_free(fileData);
    return;
  }

  // Xoá danh sách tilesets cũ (nếu gọi load map nhiều lần)
  tilesets.clear();
  
  // Duyệt qua tất cả các thẻ <tileset> bên trong <map>
  for (tinyxml2::XMLElement* tilesetNode = mapNode->FirstChildElement("tileset");
       tilesetNode != nullptr;
       tilesetNode = tilesetNode->NextSiblingElement("tileset")) {
       
    int firstgid = 0;
    // Đọc firstgid của tileset (GID bắt đầu của tập gạch này)
    tilesetNode->QueryIntAttribute("firstgid", &firstgid);

    // Kiểm tra xem đây có phải là tileset external (nằm ở file .tsx khác) không bằng thuộc tính "source"
    const char* tsxSource = tilesetNode->Attribute("source");

    std::string name;
    int tileWidth = 32;
    int tileHeight = 32;
    int columns = 0;
    std::string imagePath;

    // Nếu có "source", nghĩa là external tileset (.tsx)
    if (tsxSource != nullptr) {
      // Phân tích đường dẫn file TSX dựa trên đường dẫn tương đối từ file TMX
      std::string tsxFullPath = TextureManager::ResolvePath(actualPath, tsxSource);

      size_t tsxDataSize = 0;
      // Đọc file .tsx vào bộ nhớ
      char* tsxData = static_cast<char*>(SDL_LoadFile(tsxFullPath.c_str(), &tsxDataSize));
      if (!tsxData) {
        std::cerr << "Map: Failed to load external tsx file: " << tsxFullPath << std::endl;
        continue;
      }

      // Parse file TSX
      tinyxml2::XMLDocument tsxDoc;
      if (tsxDoc.Parse(tsxData, tsxDataSize) == tinyxml2::XML_SUCCESS) {
        tinyxml2::XMLElement* tsNode = tsxDoc.FirstChildElement("tileset");
        if (tsNode) {
          // Lấy tên tileset
          const char* nameAttr = tsNode->Attribute("name");
          if (nameAttr) name = nameAttr;

          // Lấy kích thước gạch và số cột
          tsNode->QueryIntAttribute("tilewidth", &tileWidth);
          tsNode->QueryIntAttribute("tileheight", &tileHeight);
          tsNode->QueryIntAttribute("columns", &columns);

          // Lấy thông tin file ảnh spritesheet <image>
          tinyxml2::XMLElement* imgNode = tsNode->FirstChildElement("image");
          if (imgNode) {
            const char* imgSource = imgNode->Attribute("source");
            if (imgSource) {
              // Phân tích đường dẫn ảnh tương đối với file TSX
              imagePath = TextureManager::ResolvePath(tsxFullPath, imgSource);
            }
          }
        }
      }
      SDL_free(tsxData); // Giải phóng bộ nhớ file TSX
    } else {
      // Tileset nhúng trực tiếp trong file TMX (Embedded tileset)
      const char* nameAttr = tilesetNode->Attribute("name");
      if (nameAttr) name = nameAttr;

      // Đọc thông số
      tilesetNode->QueryIntAttribute("tilewidth", &tileWidth);
      tilesetNode->QueryIntAttribute("tileheight", &tileHeight);
      tilesetNode->QueryIntAttribute("columns", &columns);

      // Đọc file ảnh
      tinyxml2::XMLElement* imgNode = tilesetNode->FirstChildElement("image");
      if (imgNode) {
        const char* imgSource = imgNode->Attribute("source");
        if (imgSource) {
          imagePath = TextureManager::ResolvePath(actualPath, imgSource);
        }
      }
    }

    // Nếu thu thập đủ thông tin (tên, đường dẫn ảnh)
    if (!name.empty() && !imagePath.empty()) {
      std::cout << "Map: Auto-loading tileset texture '" << name << "' from: " << imagePath << std::endl;
      // Dùng Game::assets->AddTexture để đăng ký texture vào AssetManager
      // Hệ thống engine sẽ tự load ảnh và cache lại dựa trên 'name'
      Game::assets->AddTexture(name, imagePath.c_str());

      // Lưu trữ thông tin Tileset vào mảng
      TilesetInfo tsInfo;
      tsInfo.firstgid = firstgid;
      tsInfo.tileWidth = tileWidth;
      tsInfo.tileHeight = tileHeight;
      tsInfo.columns = columns;
      tsInfo.textureId = name;
      tilesets.push_back(tsInfo);
    }
  }

  // Sắp xếp danh sách tilesets theo firstgid tăng dần để hàm GetTilesetForGid hoạt động chính xác
  std::sort(tilesets.begin(), tilesets.end(), [](const TilesetInfo& a, const TilesetInfo& b) {
    return a.firstgid < b.firstgid;
  });

  // Lưu lại kích thước bản đồ toàn cục
  width = mapW;
  height = mapH;

  // Xoá lưới dữ liệu 3 chiều chứa loại va chạm
  typeGrid3D.clear();
  // Xoá lưới độ cao và ramp 3 chiều
  heightGrid3D.clear();
  rampGrid3D.clear();
  // Khởi tạo mảng bool ghi nhận xem cỏ nền cơ sở đã tạo chưa (đã bỏ dùng nhưng còn biến lưu tạm)
  std::vector<std::vector<bool>> baseGrassCreated(mapH, std::vector<bool>(mapW, false));

  // Biến đếm số lượng tầng địa hình (Terrain Layers), mỗi tầng sẽ là 1 zLevel
  int terrainLayerCount = 0;

  // Duyệt qua tất cả các lớp <layer> trong file TMX
  for (tinyxml2::XMLElement* layerNode = mapNode->FirstChildElement("layer");
       layerNode != nullptr;
       layerNode = layerNode->NextSiblingElement("layer")) {

    const char* layerName = layerNode->Attribute("name");
    if (!layerName) continue; // Bỏ qua nếu layer không có tên

    std::string nameStr(layerName);
    // Nhận diện layer va chạm dựa vào việc tên layer có bắt đầu bằng "Collision" hay không (phân biệt in hoa/thường)
    bool isCollision = (nameStr.rfind("Collision", 0) == 0 || nameStr.rfind("collision", 0) == 0);

    // Lấy node <data> bên trong <layer>
    tinyxml2::XMLElement* dataNode = layerNode->FirstChildElement("data");
    if (!dataNode) continue;

    // Đọc thuộc tính "encoding" và "compression" của <data>
    const char* encoding = dataNode->Attribute("encoding");
    const char* compression = dataNode->Attribute("compression");
    
    // Engine yêu cầu định dạng bắt buộc phải là Base64 mã hoá và nén bằng Zlib
    if (!encoding || std::string(encoding) != "base64" || !compression || std::string(compression) != "zlib") {
      std::cerr << "Map WARNING: Layer '" << layerName << "' is not in base64/zlib format. Please configure it in Tiled." << std::endl;
      continue;
    }

    int zLevel = 0;
    float layerZ = 0.0f; // Cao độ thực sự của layer
    
    // Xử lý phân cấp Z-Level
    if (!isCollision) {
      // Nếu là layer hiển thị (Terrain), ta tăng zLevel lên và tính cao độ
      zLevel = terrainLayerCount;
      terrainLayerCount++;
      // Ví dụ: zLevel=1 -> layerZ = 1 * blockHeight (vd: 1 * 48 = 48)
      layerZ = static_cast<float>(zLevel * g_WorldConfig.blockHeight);
      
      // Nếu mảng 3D chưa đủ sức chứa cho zLevel mới này, tiến hành resize và điền bằng giá trị Walkable
      if (zLevel >= 0 && static_cast<size_t>(zLevel) >= typeGrid3D.size()) {
        typeGrid3D.resize(zLevel + 1, std::vector<std::vector<TileType>>(mapH, std::vector<TileType>(mapW, TileType::Walkable)));
        heightGrid3D.resize(zLevel + 1, std::vector<std::vector<float>>(mapH, std::vector<float>(mapW, static_cast<float>(zLevel * g_WorldConfig.blockHeight))));
        rampGrid3D.resize(zLevel + 1, std::vector<std::vector<TileType>>(mapH, std::vector<TileType>(mapW, TileType::Walkable)));
      }
    } else {
      // Nếu là layer Collision, nó sẽ dùng chung Z-Level với layer terrain liền trước nó
      // Ví dụ: Terrain 1, Collision 1, Terrain 2, Collision 2. Collision 1 thuộc Terrain 1.
      int collLevel = (terrainLayerCount > 0) ? terrainLayerCount - 1 : 0;
      zLevel = collLevel;
      // Khởi tạo lưới 3D nếu chưa có
      if (zLevel >= 0 && static_cast<size_t>(zLevel) >= typeGrid3D.size()) {
        typeGrid3D.resize(zLevel + 1, std::vector<std::vector<TileType>>(mapH, std::vector<TileType>(mapW, TileType::Walkable)));
        heightGrid3D.resize(zLevel + 1, std::vector<std::vector<float>>(mapH, std::vector<float>(mapW, static_cast<float>(zLevel * g_WorldConfig.blockHeight))));
        rampGrid3D.resize(zLevel + 1, std::vector<std::vector<TileType>>(mapH, std::vector<TileType>(mapW, TileType::Walkable)));
      }
    }

    // Kiểm tra xem map được lưu dạng "chunk" (Infinite map) hay không
    tinyxml2::XMLElement* chunkNode = dataNode->FirstChildElement("chunk");
    if (chunkNode) {
      // Duyệt qua tất cả các chunk để đọc dữ liệu
      for (; chunkNode != nullptr; chunkNode = chunkNode->NextSiblingElement("chunk")) {
        int chunkX = 0, chunkY = 0, chunkW = 0, chunkH = 0;
        // Đọc toạ độ và kích thước của chunk đó
        chunkNode->QueryIntAttribute("x", &chunkX);
        chunkNode->QueryIntAttribute("y", &chunkY);
        chunkNode->QueryIntAttribute("width", &chunkW);
        chunkNode->QueryIntAttribute("height", &chunkH);

        // Lấy nội dung text Base64
        const char* chunkText = chunkNode->GetText();
        if (!chunkText) continue;

        // Gọi hàm xử lý và tạo Tile Entity
        ProcessLayerDataClient(chunkText, chunkX, chunkY, chunkW, chunkH, tilesets, zLevel, layerZ, isCollision,
                               mapW, mapH, scaledSize, typeGrid3D, heightGrid3D, rampGrid3D, baseGrassCreated, this);
      }
    } else {
      // Nếu là map kích thước cố định (Fixed size map), dữ liệu text base64 nằm trực tiếp ở dataNode
      const char* base64Text = dataNode->GetText();
      if (base64Text) {
        // Parse toàn bộ dữ liệu map trong 1 cục chunk với vị trí 0,0 và kích thước toàn map
        ProcessLayerDataClient(base64Text, 0, 0, mapW, mapH, tilesets, zLevel, layerZ, isCollision,
                               mapW, mapH, scaledSize, typeGrid3D, heightGrid3D, rampGrid3D, baseGrassCreated, this);
      }
    }
  }

  // Khởi tạo các thực thể (Entity) đóng vai trò cục va chạm (Collider) vật lý
  // Duyệt qua mảng lưới 3 chiều typeGrid3D để tìm các ô bị "Blocked"
  for (size_t z = 0; z < typeGrid3D.size(); ++z) {
    for (int y = 0; y < mapH; ++y) {
      for (int x = 0; x < mapW; ++x) {
        // Nếu tại tọa độ (x, y, z) là Blocked (có vật cản cứng)
        if (typeGrid3D[z][y][x] == TileType::Blocked) {
          // Lệch vị trí tile theo zLevel (đồng bộ với logic MapData của Server)
          int shiftedX = x + static_cast<int>(z);
          int shiftedY = y + static_cast<int>(z);
          
          // Kiểm tra xem vị trí sau khi lệch có nằm trong biên bản đồ hay không
          if (shiftedX >= 0 && shiftedY >= 0 && shiftedX < mapW && shiftedY < mapH) {
            // Tạo một Entity mới trong hệ thống ECS
            auto& tcol(manager.addEntity());
            // Thêm ColliderComponent để hỗ trợ check AABB. 'terrain' là tag va chạm.
            tcol.addComponent<ColliderComponent>("terrain", shiftedX * scaledSize,
                                                 shiftedY * scaledSize, scaledSize);
                                                 
            // Cài đặt thuộc tính vị trí Z của collider (cao độ). 
            // Rất quan trọng để check xem nhân vật đứng ở z=0 có bị va chạm với gạch z=48 hay không.
            tcol.getComponent<TransformComponent>().position.z = static_cast<float>(z * g_WorldConfig.blockHeight);
            
            // Thêm entity này vào groupColliders để xử lý va chạm đồng loạt ở Physics System
            tcol.addGroup(Game::groupColliders);
          }
        }
      }
    }
  }

  // Giải phóng dữ liệu file XML trong bộ nhớ
  SDL_free(fileData);
  std::cout << "Map: Loaded Base64 Zlib TMX map '" << path << "' (" << mapW << "x" << mapH << ") with " << terrainLayerCount << " layers successfully." << std::endl;

  // Tự động in map ra terminal để dễ dàng debug kết quả parse
  PrintMap();
}

// Phương thức: Tạo mới một Entity ô gạch (Tile), đính kèm TileComponent và phân nhóm vào groupMap để hiển thị
// srcX, srcY là toạ độ trên Sprite Sheet (Texture)
// xpos, ypos là toạ độ trên màn hình Game
void Map::AddTile(int srcX, int srcY, int xpos, int ypos, float zpos, const std::string& textureId, int tSize) {
  auto& tile(manager.addEntity()); // Thêm Entity vào ECS
  // Gắn TileComponent, chứa thông số hiển thị
  tile.addComponent<TileComponent>(srcX, srcY, xpos, ypos, zpos, tSize, mapScale, textureId);
  // Đưa vào nhóm (Group) hiển thị Map (thường Render trước nhân vật)
  tile.addGroup(Game::groupMap);
}

// Hàm: In thông tin debug của map ra console (ASCII Art)
void Map::PrintMap() const {
  std::cout << "\n================= CLIENT MAP DEBUG PRINT =================" << std::endl;
  std::cout << "Dimensions: " << width << "x" << height << " tiles" << std::endl;
  std::cout << "Tile Size: " << tileSize << "px | Scale: x" << mapScale << " | Scaled Size: " << scaledSize << "px" << std::endl;
  std::cout << "Z-Levels: " << typeGrid3D.size() << std::endl;
  
  // In danh sách các Tileset đã nạp
  std::cout << "Loaded Tilesets (" << tilesets.size() << "):" << std::endl;
  for (size_t i = 0; i < tilesets.size(); ++i) {
    std::cout << "  [" << i << "] GID: " << tilesets[i].firstgid 
              << " | ID: " << tilesets[i].textureId 
              << " | Size: " << tilesets[i].tileWidth << "x" << tilesets[i].tileHeight 
              << " | Cols: " << tilesets[i].columns << std::endl;
  }

  // In ra các lưới Z-level dưới dạng Text ký tự đặc biệt (#, ., /, \)
  for (size_t z = 0; z < typeGrid3D.size(); ++z) {
    std::cout << "\n--- Z-Level (Collision Layer) " << z << " ---" << std::endl;
    for (int y = 0; y < height; ++y) {
      if (y < 10) std::cout << " ";
      std::cout << y << " | ";
      
      for (int x = 0; x < width; ++x) {
        char symbol = '.'; // Ký hiệu cơ bản là dấu chấm (Walkable)
        switch (typeGrid3D[z][y][x]) {
          case TileType::Walkable:
            symbol = '.';
            break;
          case TileType::Blocked:
            symbol = '#'; // Vật cản cứng
            break;
          case TileType::Ramp_SW_NE:
            symbol = '/'; // Dốc xéo
            break;
          case TileType::Ramp_SE_NW:
            symbol = '\\'; // Dốc xéo
            break;
          case TileType::Ramp_NE_SW:
            symbol = '<';
            break;
          case TileType::Ramp_NW_SE:
            symbol = '>';
            break;
        }
        std::cout << symbol << " ";
      }
      std::cout << std::endl;
    }
    
    // In thanh tọa độ X dưới cùng
    std::cout << "     ";
    for (int x = 0; x < width; ++x) {
      std::cout << (x % 10) << " ";
    }
    std::cout << std::endl;
  }
  std::cout << "==========================================================\n" << std::endl;
}
