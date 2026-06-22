#include "Common/World/MapData.hpp"

#include <SDL3/SDL.h>  // Dùng SDL_LoadFile để đọc file đa nền tảng
#include <tinyxml2.h>  // Thư viện phân tích cú pháp file XML (.tmx)
#include <zlib.h>      // Thư viện nén/giải nén zlib cho map data

#include <algorithm>  // Cho các hàm như std::sort
#include <cctype>     // Kiểm tra ký tự (isspace)
#include <cmath>      // Các hàm toán học như std::abs
#include <iostream>   // In log
#include <sstream>    // Luồng chuỗi stringstream
#include <vector>     // Lớp mảng động tiêu chuẩn

#include "Common/World/WorldConfig.hpp"  // Các tham số cấu hình server-side (blockHeight)

// Không gian tên vô danh (anonymous namespace) để che giấu các hàm phụ trợ bên
// trong file MapData.cpp này
namespace {

// Lấy Local ID (ID bắt đầu từ 0 trong tileset) dựa vào Global ID (GID) và mảng
// các firstGids
int GetLocalId(int gid, const std::vector<int>& firstgids) {
  if (gid == 0) return 0;  // Nếu GID là 0 (ô trống) thì trả về 0
  int targetFirstGid = 1;  // Mặc định tileset đầu tiên (index 1)
  // Duyệt qua danh sách các firstgid (đã được sắp xếp tăng dần)
  for (int fg : firstgids) {
    if (fg <= gid) {
      targetFirstGid = fg;  // Tileset này có thể chứa GID
    } else {
      break;  // Nếu fg lớn hơn gid, ngừng lặp
    }
  }
  // Local ID = GID - FirstGID của tileset tương ứng
  return gid - targetFirstGid;
}

// Giải mã chuỗi văn bản Base64 thành mảng các Byte (giống Map.cpp client-side)
std::vector<uint8_t> DecodeBase64(const std::string& input) {
  // Tập ký tự của Base64
  const std::string b64chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<int> T(256, -1);  // Bảng ánh xạ mã ASCII -> Giá trị Base64 6-bit
  for (int i = 0; i < 64; i++) T[b64chars[i]] = i;

  std::vector<uint8_t> out;  // Buffer chứa kết quả
  int val = 0, valb = -8;    // Bộ biến đệm bit
  for (unsigned char c : input) {
    if (std::isspace(c)) continue;  // Bỏ qua whitespace
    if (c == '=') break;            // Dừng khi gặp dấu padding
    if (T[c] == -1) continue;       // Bỏ qua ký tự lạ
    val = (val << 6) + T[c];  // Dịch trái 6 bit và cộng thêm giá trị decode
    valb += 6;
    if (valb >= 0) {
      // Khi đủ 8 bit, ghi byte vào out và trừ đi 8 bit từ biến đệm
      out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}

// Giải nén dữ liệu Zlib từ buffer compressedData sang decompressed với kích
// thước mong đợi expectedSize
std::vector<uint8_t> DecompressZlib(const std::vector<uint8_t>& compressedData,
                                    size_t expectedSize) {
  std::vector<uint8_t> decompressed(expectedSize);  // Tạo mảng kết quả
  uLongf destLen = expectedSize;                    // Chiều dài bộ nhớ đệm đích

  // Hàm uncompress() của zlib. destLen ban đầu là kích thước tối đa, sau đó
  // được gán bằng size thực sự giải nén được
  int res = uncompress(decompressed.data(), &destLen, compressedData.data(),
                       compressedData.size());

  if (res != Z_OK) {  // Nếu quá trình giải nén thất bại
    std::cerr << "MapData: Zlib decompression failed with error code: " << res
              << std::endl;
    return {};  // Trả về mảng rỗng
  }
  decompressed.resize(
      destLen);  // Thu gọn lại size mảng (thường = expectedSize)
  return decompressed;
}

// Hàm phân tích khối dữ liệu chunk của TMX map
// Ghi dữ liệu giải nén vào nhiều grid khác nhau (groundGrid, decorGrid,
// typeGrid3D, heightGrid3D, rampGrid3D)
void ProcessLayerData(
    const std::string& base64Text, int chunkX, int chunkY, int chunkW,
    int chunkH, const std::vector<int>& firstGids, int zLevel, float layerZ,
    bool isCollision, float collisionHeight, int mapW, int mapH,
    std::vector<std::vector<int>>& groundGrid,
    std::vector<std::vector<int>>& decorGrid,
    std::vector<std::vector<float>>& heightGrid3D_level,
    std::vector<std::vector<std::vector<TileType>>>& typeGrid3D,
    std::vector<std::vector<TileType>>& rampGrid3D_level) {
  // Giải mã Base64 -> Nén Zlib
  std::vector<uint8_t> compressed = DecodeBase64(base64Text);
  // Mỗi GID là số nguyên 32-bit (4 byte) nên kích thước dự tính = Rộng * Cao *
  // 4
  size_t expectedSize = chunkW * chunkH * 4;
  // Giải nén ra mảng byte thuần
  std::vector<uint8_t> decompressed = DecompressZlib(compressed, expectedSize);

  if (decompressed.size() < expectedSize) return;  // Bảo vệ lỗi thiếu dữ liệu

  // Quét theo chiều Y rồi X trong chunk hiện tại
  for (int cy = 0; cy < chunkH; ++cy) {
    for (int cx = 0; cx < chunkW; ++cx) {
      int mapX = chunkX + cx;  // Toạ độ X tuyệt đối
      int mapY = chunkY + cy;  // Toạ độ Y tuyệt đối

      // Chống lỗi tràn chỉ số map
      if (mapX < 0 || mapX >= mapW || mapY < 0 || mapY >= mapH) continue;

      int idx = (cy * chunkW + cx) * 4;  // Chỉ số byte gốc của ô này
      // Đọc 4 byte dưới dạng số nguyên 32 bit (Little-endian, ghép từng byte)
      uint32_t gid = decompressed[idx] | (decompressed[idx + 1] << 8) |
                     (decompressed[idx + 2] << 16) |
                     (decompressed[idx + 3] << 24);
      // Xóa 3 bit cao nhất (thường Tiled dùng để biểu diễn việc Flip hình ảnh
      // X, Y, D)
      gid = gid & ~0xF0000000;

      if (isCollision) {
        // Lớp va chạm
        if (gid > 0) {
          // Quy đổi GID -> ID nội bộ của tileset va chạm
          int typeVal = GetLocalId(gid, firstGids);
          // Ép ID thành enum TileType (1: Blocked, 2: Dốc / Ramp)
          TileType newType = static_cast<TileType>(typeVal);

          // Ghi vào typeGrid3D (dùng check Block chung toàn map)
          if (zLevel >= 0 && static_cast<size_t>(zLevel) < typeGrid3D.size()) {
            typeGrid3D[zLevel][mapY][mapX] = newType;
          }

          if (newType == TileType::Ramp_SW_NE ||
              newType == TileType::Ramp_SE_NW ||
              newType == TileType::Ramp_NE_SW ||
              newType == TileType::Ramp_NW_SE) {
            rampGrid3D_level[mapY][mapX] = newType;
          }
        }
      } else {
        // Lớp địa hình (Terrain / Ground)
        if (gid > 0) {
          // Đánh dấu ô này có độ cao = layerZ (nền gốc của zLevel hiện tại)
          heightGrid3D_level[mapY][mapX] = layerZ;
          int localId = GetLocalId(gid, firstGids);
          // Ghi thông số vào 2 layer phẳng (nếu muốn logic mỏng).
          // Thường zLevel == 0 là Đất (ground), zLevel != 0 là Trang trí
          // (decor). Server ít khi dùng grid mỏng này, vì có typeGrid3D xử lý
          // đa chiều tốt hơn.
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

// Hàm chính của MapData: Parse file TMX, phân tách layer và xây dựng cấu trúc
// mảng lưới 3 chiều để Server check logic va chạm, cao độ
bool MapData::LoadMap(const std::string& path) {
  size_t dataSize = 0;
  // Đọc file map
  char* fileData = static_cast<char*>(SDL_LoadFile(path.c_str(), &dataSize));
  if (!fileData) {
    std::cerr << "MapData: Failed to open TMX file via SDL_LoadFile: " << path
              << " Error: " << SDL_GetError() << std::endl;
    return false;
  }

  tinyxml2::XMLDocument doc;
  // Phân tích file XML
  tinyxml2::XMLError err = doc.Parse(fileData, dataSize);
  if (err != tinyxml2::XML_SUCCESS) {
    std::cerr << "MapData: Failed to parse TMX XML: " << doc.ErrorStr()
              << std::endl;
    SDL_free(fileData);
    return false;
  }

  // Tìm node <map> gốc
  tinyxml2::XMLElement* mapNode = doc.FirstChildElement("map");
  if (!mapNode) {
    std::cerr << "MapData: TMX file lacks <map> element: " << path << std::endl;
    SDL_free(fileData);
    return false;
  }

  // Đọc Width, Height số lượng ô của map
  int mapW = 0;
  int mapH = 0;
  mapNode->QueryIntAttribute("width", &mapW);
  mapNode->QueryIntAttribute("height", &mapH);

  // Validate kích thước map
  if (mapW <= 0 || mapH <= 0) {
    std::cerr << "MapData: Invalid map dimensions: " << mapW << "x" << mapH
              << std::endl;
    SDL_free(fileData);
    return false;
  }

  width = mapW;   // Lưu vào biến member của MapData
  height = mapH;  // Lưu vào biến member của MapData

  // Trích xuất list firstgid của tất cả tilesets
  // Server không cần load Texture (ảnh), chỉ cần map index để convert ID va
  // chạm
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
  std::sort(firstGids.begin(),
            firstGids.end());  // Sắp xếp tăng dần để dễ tra cứu

  // Khởi tạo các Grid 2 chiều
  groundGrid.assign(height, std::vector<int>(width, 0));
  decorGrid.assign(height, std::vector<int>(width, 0));

  // Clear các mảng đa tầng 3 chiều
  heightGrid3D.clear();
  typeGrid3D.clear();
  rampGrid3D.clear();

  int terrainLayerCount =
      0;  // Đếm số layer địa hình thông thường để xây dựng thứ tự độ cao (z)

  // Quét vòng lặp qua tất cả <layer> trong map
  for (tinyxml2::XMLElement* layerNode = mapNode->FirstChildElement("layer");
       layerNode != nullptr;
       layerNode = layerNode->NextSiblingElement("layer")) {
    const char* layerName = layerNode->Attribute("name");
    if (!layerName) continue;  // Layer không tên => Bỏ qua

    // Tìm node <data> chứa map raw text
    tinyxml2::XMLElement* dataNode = layerNode->FirstChildElement("data");
    if (!dataNode) continue;

    // Check bắt buộc có encoding base64 và nén zlib
    const char* encoding = dataNode->Attribute("encoding");
    const char* compression = dataNode->Attribute("compression");
    if (!encoding || std::string(encoding) != "base64" || !compression ||
        std::string(compression) != "zlib") {
      std::cerr
          << "MapData WARNING: Layer '" << layerName
          << "' is not in base64/zlib format. Please configure it in Tiled."
          << std::endl;
      continue;  // Bỏ qua layer không đúng chuẩn nén
    }

    std::string nameStr(layerName);
    // Nhận diện xem nó là Terrain layer hay Collision layer
    bool isCollision = (nameStr.rfind("Collision", 0) == 0 ||
                        nameStr.rfind("collision", 0) == 0);

    int zLevel = 0;       // Chỉ số mảng Z
    float layerZ = 0.0f;  // Độ cao thực tế (tính bằng px) của layer
    float collisionHeight =
        0.0f;  // Dùng dự phòng cho logic nâng cao (không xài)

    // Thuật toán: Nếu là layer hiển thị (Terrain), ta coi nó là 1 Tầng Z mới.
    // Tầng đầu zLevel=0. Tầng kế zLevel=1.
    // Nếu là layer Collision, nó thuộc về tầng Terrain ngay TRƯỚC nó (hoặc z=0
    // nếu là layer đầu tiên)
    if (!isCollision) {
      zLevel = terrainLayerCount;
      terrainLayerCount++;
      // Độ cao của Terrain = Số tầng * Chiều cao 1 khối địa hình (VD: z=1 =>
      // 48px)
      layerZ = static_cast<float>(zLevel * g_WorldConfig.blockHeight);

      // Cấp phát/mở rộng chiều cho mảng 3D nếu cần (std::vector::resize tự thêm
      // phần tử cuối)
      if (zLevel >= 0 && static_cast<size_t>(zLevel) >= typeGrid3D.size()) {
        // Tầng va chạm
        typeGrid3D.resize(
            zLevel + 1,
            std::vector<std::vector<TileType>>(
                height, std::vector<TileType>(width, TileType::Walkable)));
        // Tầng độ cao
        heightGrid3D.resize(
            zLevel + 1,
            std::vector<std::vector<float>>(
                height, std::vector<float>(width, -999.0f)));
        // Tầng ramp (chứa dữ liệu dốc xéo)
        rampGrid3D.resize(
            zLevel + 1,
            std::vector<std::vector<TileType>>(
                height, std::vector<TileType>(width, TileType::Walkable)));
      }
    } else {
      // Logic gán zLevel cho layer va chạm
      int collLevel = (terrainLayerCount > 0) ? terrainLayerCount - 1 : 0;
      collisionHeight =
          static_cast<float>(collLevel * g_WorldConfig.blockHeight);
      zLevel = collLevel;

      // Cũng resize giống hệt bên trên phòng hờ trường hợp Map tác giả vẽ sai
      // (Collision nằm tuốt trên đầu file XML)
      if (zLevel >= 0 && static_cast<size_t>(zLevel) >= typeGrid3D.size()) {
        typeGrid3D.resize(
            zLevel + 1,
            std::vector<std::vector<TileType>>(
                height, std::vector<TileType>(width, TileType::Walkable)));
        heightGrid3D.resize(
            zLevel + 1,
            std::vector<std::vector<float>>(
                height, std::vector<float>(width, -999.0f)));
        rampGrid3D.resize(
            zLevel + 1,
            std::vector<std::vector<TileType>>(
                height, std::vector<TileType>(width, TileType::Walkable)));
      }
    }

    // Tiled hỗ trợ 2 dạng cấu trúc data: Map Vô Tận (Tiled Infinite map, xài
    // thẻ <chunk>) hoặc Map Kích Thước Cố Định (Fixed size map, raw data trực
    // tiếp)
    tinyxml2::XMLElement* chunkNode = dataNode->FirstChildElement("chunk");
    if (chunkNode) {
      // Trường hợp 1: Tiled Infinite map (bản đồ chia nhỏ ra nhiều chunk con)
      for (; chunkNode != nullptr;
           chunkNode = chunkNode->NextSiblingElement("chunk")) {
        // Đọc toạ độ và size chunk hiện hành
        int chunkX = 0, chunkY = 0, chunkW = 0, chunkH = 0;
        chunkNode->QueryIntAttribute("x", &chunkX);
        chunkNode->QueryIntAttribute("y", &chunkY);
        chunkNode->QueryIntAttribute("width", &chunkW);
        chunkNode->QueryIntAttribute("height", &chunkH);

        const char* chunkText = chunkNode->GetText();  // Nội dung chuỗi nén
        if (!chunkText) continue;

        // Gọi hàm xử lý và đổ vào các tham chiếu Mảng 3 chiều (ở chỉ số zLevel
        // hiện tại)
        ProcessLayerData(chunkText, chunkX, chunkY, chunkW, chunkH, firstGids,
                         zLevel, layerZ, isCollision, collisionHeight, width,
                         height, groundGrid, decorGrid, heightGrid3D[zLevel],
                         typeGrid3D, rampGrid3D[zLevel]);
      }
    } else {
      // Trường hợp 2: Fixed size map (Toàn bộ mảng map là 1 chuỗi dài Base64)
      const char* base64Text = dataNode->GetText();
      if (base64Text) {
        ProcessLayerData(base64Text, 0, 0, width, height, firstGids, zLevel,
                         layerZ, isCollision, collisionHeight, width, height,
                         groundGrid, decorGrid, heightGrid3D[zLevel],
                         typeGrid3D, rampGrid3D[zLevel]);
      }
    }
  }

  SDL_free(fileData);  // Dọn dẹp RAM
  std::cout << "MapData: Loaded Base64 Zlib TMX map '" << path << "' (" << width
            << "x" << height << ") with " << terrainLayerCount
            << " height layers successfully." << std::endl;

  // Gọi hàm in debug log ASCII mảng dữ liệu 3D map
  PrintMap();

  return true;
}

// -----------------------------------------------------------------------------
// PHYSICS & COLLISION - KIỂM TRA VA CHẠM DÀNH CHO GAME SERVER
// -----------------------------------------------------------------------------

// Kiểm tra nhanh xem toạ độ Tile (tileX, tileY) có chặn Entity đang ở chiều cao
// (entityZ) hay không
bool MapData::IsTileBlocked(int tileX, int tileY,
                            float entityZ) const noexcept {
  // Điểm ngoài bản đồ mặc định bị chặn không cho rớt ra vực
  if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height) return true;
  // Map lỗi không có dữ liệu 3D => cho đi bình thường (hoặc chặn tuỳ logic
  // nhưng tạm return false)
  if (typeGrid3D.empty()) return false;

  int maxZ = static_cast<int>(typeGrid3D.size());
  // Tìm tầng Z gần nhất có địa hình thực tế
  int entityZLevel = GetClosestZLevel(tileX, tileY, entityZ);

  // CHIẾN LƯỢC QUÉT VA CHẠM 3D:
  // Duyệt qua tất cả các Z-Level. Bởi vì:
  // Một cục đá đặt ở Z-Level 1 (cao 48px) có thể chặn Entity khi Entity nhảy
  // lên cao ngang với đỉnh cục đá, Hoặc nếu Entity đứng dưới đất (Z-Level 0)
  // thì cái cầu (Z-Level 1) nằm trên đầu Entity sẽ KHÔNG chặn Entity.
  for (int zl = 0; zl < maxZ; ++zl) {
    // Bỏ qua va chạm với block ở lớp dưới khi thực thể đã di chuyển lên tầng
    // trên
    if (zl < entityZLevel) {
      continue;
    }
    // Lệch vị trí tile theo zLevel
    int sx = tileX - zl;
    int sy = tileY - zl;
    if (sx < 0 || sy < 0 || sx >= width || sy >= height) {
      if (zl == entityZLevel) {
        return true; // Ngoài biên bản đồ ở tầng của thực thể -> Chặn
      }
      continue; // Tầng khác ngoài biên -> Bỏ qua
    }

    TileType type =
        typeGrid3D[zl][sy][sx];  // Kiểu Tile tại Tầng zl, X, Y
    if (type == TileType::Blocked) {
      // Khối đá/tường này nằm ở độ cao Z thực tế từ Floor (sàn) -> Ceil (trần)
      float collisionFloorZ =
          static_cast<float>(zl * g_WorldConfig.blockHeight);
      float collisionCeilZ = collisionFloorZ + g_WorldConfig.blockHeight;
      float effectiveCeilZ = collisionCeilZ;

      // Nới lỏng trần va chạm hiệu dụng của ô Blocked nếu nó nằm cạnh dốc hoặc
      // dưới gầm dốc của tầng trên, để rìa hộp AABB có thể đi qua thềm đỉnh dốc
      // khi thực thể đang leo dốc.
      if (static_cast<size_t>(zl + 1) < rampGrid3D.size()) {
        int uzl = zl + 1;
        int ux = tileX - uzl;
        int uy = tileY - uzl;
        if (ux >= 0 && uy >= 0 && ux < width && uy < height) {
          TileType upperType = rampGrid3D[uzl][uy][ux];
          if (upperType == TileType::Ramp_SW_NE ||
              upperType == TileType::Ramp_SE_NW ||
              upperType == TileType::Ramp_NE_SW ||
              upperType == TileType::Ramp_NW_SE) {
            effectiveCeilZ = collisionFloorZ + g_WorldConfig.blockHeight * 0.5f;
          }
        }
      }

      if (entityZ >= collisionFloorZ - 1.0f && entityZ < effectiveCeilZ) {
        return true;
      }
    }
  }

  // TÍNH TOÁN BẬC THỀM: Kiểm tra chênh lệch chiều cao tự nhiên giữa Tile và
  // Entity (tại tầng Z mà Entity đang đứng) Nếu chênh lệch cao độ quá mức
  // => Entity không thể đi lên thềm dốc đứng, sẽ bị chặn!
  if (entityZLevel >= 0 &&
      static_cast<size_t>(entityZLevel) < heightGrid3D.size()) {
    int sx = tileX - entityZLevel;
    int sy = tileY - entityZLevel;
    if (sx < 0 || sy < 0 || sx >= width || sy >= height) {
      return true;
    }
    float tileZ = heightGrid3D[entityZLevel][sy][sx];
    TileType type = typeGrid3D[entityZLevel][sy][sx];

    if (type == TileType::Walkable) {
      float heightDiff = std::abs(entityZ - tileZ);
      // FIX ISOMETRIC: Nới lỏng step threshold khi entity đang ở gần vùng ramp.
      // Khi entity đang leo dốc chéo isometric, Z đang thay đổi dần và có thể
      // chênh lệch tạm thời với tile lân cận. Cho phép lên tới 1 blockHeight
      // nếu có ramp gần đó, thay vì chỉ 0.5 blockHeight.
      bool nearRamp = HasNeighborRamp(tileX, tileY, entityZLevel);
      float maxStep = nearRamp ? g_WorldConfig.blockHeight
                               : g_WorldConfig.blockHeight * 0.5f;
      if (heightDiff > maxStep) {
        return true;
      }
    }
  }

  return false;
}

// Kiểm tra va chạm theo dạng hộp chữ nhật (AABB) của Entity: x, y (Tọa độ), w,
// h (Kích cỡ hộp), z (Cao độ)
bool MapData::IsBlocked(float x, float y, float z, float w,
                        float h) const noexcept {
  // Tìm ra các toạ độ lưới gạch (Tile grid coordinates) mà phần thân AABB đang
  // đè lên x0, y0: Góc trên bên trái (Top-Left) | x1, y1: Góc dưới bên phải
  // (Bottom-Right)
  int x0 = static_cast<int>(x) / scaledSize;
  int y0 = static_cast<int>(y) / scaledSize;
  int x1 = static_cast<int>(x + w - 1.0f) / scaledSize;
  int y1 = static_cast<int>(y + h - 1.0f) / scaledSize;

  if (typeGrid3D.empty()) return false;
  int maxZ = static_cast<int>(typeGrid3D.size());

  // Tìm tầng Z hiện tại của tâm vật lý của Entity
  int pivotTileX = static_cast<int>(x + w / 2.0f) / scaledSize;
  int pivotTileY = static_cast<int>(y + h / 2.0f) / scaledSize;
  int entityZLevel = GetClosestZLevel(pivotTileX, pivotTileY, z);

  // Lặp qua tất cả ô gạch mà AABB đè lên để kiểm tra
  for (int ty = y0; ty <= y1; ++ty) {
    for (int tx = x0; tx <= x1; ++tx) {
      // Ngoài biên bản đồ -> Chặn hoàn toàn
      if (tx < 0 || ty < 0 || tx >= width || ty >= height) return true;

      // Quét tất cả các tầng Z (giống logic hàm IsTileBlocked)
      for (int zl = 0; zl < maxZ; ++zl) {
        // Bỏ qua va chạm với block ở lớp dưới khi thực thể đã di chuyển lên
        // tầng trên
        if (zl < entityZLevel) {
          continue;
        }
        int sx = tx - zl;
        int sy = ty - zl;
        if (sx < 0 || sy < 0 || sx >= width || sy >= height) {
          if (zl == entityZLevel) {
            return true; // Ngoài biên bản đồ ở tầng của thực thể -> Chặn
          }
          continue; // Tầng khác ngoài biên -> Bỏ qua
        }

        TileType type = typeGrid3D[zl][sy][sx];
        if (type == TileType::Blocked) {  // Nếu ô có khối chướng ngại vật cứng
          // Tính Sàn/Trần ảo của khối chắn
          float collisionFloorZ =
              static_cast<float>(zl * g_WorldConfig.blockHeight);
          float collisionCeilZ = collisionFloorZ + g_WorldConfig.blockHeight;
          float effectiveCeilZ = collisionCeilZ;

          // Nới lỏng trần va chạm xuống 50% blockHeight nếu tầng trên có Ramp
          if (static_cast<size_t>(zl + 1) < rampGrid3D.size()) {
            int uzl = zl + 1;
            int ux = tx - uzl;
            int uy = ty - uzl;
            if (ux >= 0 && uy >= 0 && ux < width && uy < height) {
              TileType upperType = rampGrid3D[uzl][uy][ux];
              if (upperType == TileType::Ramp_SW_NE ||
                  upperType == TileType::Ramp_SE_NW ||
                  upperType == TileType::Ramp_NE_SW ||
                  upperType == TileType::Ramp_NW_SE) {
                effectiveCeilZ =
                    collisionFloorZ + g_WorldConfig.blockHeight * 0.5f;
              }
            }
          }

          // Kiểm tra giao cắt chiều cao: Nếu Entity lọt vào khoảng bao của cục
          // đá thì dội!
          if (z >= collisionFloorZ - 1.0f && z < effectiveCeilZ) {
            return true;
          }
        }
      }
    }
  }

  // TÍNH BẬC THỀM ĐỘ CAO CHO TÂM NHÂN VẬT (Pivot)
  // Chỉ kiểm tra rớt vực / trượt thềm ở điểm Pivot thay vị 4 góc AABB, để trải
  // nghiệm leo dốc tự nhiên.
  if (pivotTileX >= 0 && pivotTileY >= 0 && pivotTileX < width &&
      pivotTileY < height) {
    if (entityZLevel >= 0 &&
        static_cast<size_t>(entityZLevel) < heightGrid3D.size()) {
      int sx = pivotTileX - entityZLevel;
      int sy = pivotTileY - entityZLevel;
      if (sx >= 0 && sy >= 0 && sx < width && sy < height) {
        float tileZ = heightGrid3D[entityZLevel][sy][sx];
        TileType type = typeGrid3D[entityZLevel][sy][sx];

        if (type == TileType::Walkable) {
          float heightDiff = std::abs(z - tileZ);
          // FIX ISOMETRIC: Nới lỏng step threshold khi entity ở gần ramp.
          // Cho phép chênh lệch Z lớn hơn khi đang leo/xuống dốc isometric.
          bool nearRamp = HasNeighborRamp(pivotTileX, pivotTileY, entityZLevel);
          float maxStep = nearRamp ? g_WorldConfig.blockHeight
                                   : g_WorldConfig.blockHeight * 0.5f;
          if (heightDiff > maxStep) {
            return true;
          }
        }
      } else {
        return true; // Ngoài biên bản đồ ở tầng này
      }
    }
  }

  return false;
}

// Lấy cao độ gốc Z (Flat Height) của một vị trí Tile X, Y (Không tính dốc thoai
// thoải)
float MapData::GetTileHeight(int tileX, int tileY, int zLevel) const noexcept {
  if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height) return 0.0f;
  if (zLevel < 0 || static_cast<size_t>(zLevel) >= heightGrid3D.size())
    return 0.0f;
  int sx = tileX - zLevel;
  int sy = tileY - zLevel;
  if (sx < 0 || sy < 0 || sx >= width || sy >= height) return 0.0f;
  return heightGrid3D[zLevel][sy][sx];  // Trả về chiều cao gốc từ Terrain mapping
}

// Lấy dạng cấu trúc của Tile (Đi được / Dốc / Blocked)
TileType MapData::GetTileType(int tileX, int tileY, int zLevel) const noexcept {
  if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height)
    return TileType::Blocked;
  if (zLevel < 0 || static_cast<size_t>(zLevel) >= typeGrid3D.size())
    return TileType::Walkable;
  int sx = tileX - zLevel;
  int sy = tileY - zLevel;
  if (sx < 0 || sy < 0 || sx >= width || sy >= height)
    return TileType::Blocked;
  return typeGrid3D[zLevel][sy][sx];
}

// Hàm phụ trợ: Kiểm tra xem tile lân cận (trong bán kính 1 ô) có chứa Ramp hay
// không. Dùng cho GetInterpolatedHeight để duy trì Z khi entity di chuyển chéo
// isometric thoát khỏi tile ramp trước khi leo xong.
bool MapData::HasNeighborRamp(int tileX, int tileY, int zLevel) const noexcept {
  // Quét 4 ô lân cận (trên, dưới, trái, phải)
  const int dx[] = {-1, 1, 0, 0};
  const int dy[] = {0, 0, -1, 1};
  for (int i = 0; i < 4; ++i) {
    int nx = tileX + dx[i];
    int ny = tileY + dy[i];
    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
      int sx = nx - zLevel;
      int sy = ny - zLevel;
      if (sx >= 0 && sy >= 0 && sx < width && sy < height) {
        TileType neighborType = rampGrid3D[zLevel][sy][sx];
        if (neighborType == TileType::Ramp_SW_NE ||
            neighborType == TileType::Ramp_SE_NW ||
            neighborType == TileType::Ramp_NE_SW ||
            neighborType == TileType::Ramp_NW_SE) {
          return true;
        }
      }
    }
  }
  // Kiểm tra thêm tầng dưới (nếu entity đang đứng trên đỉnh ramp tầng dưới)
  if (zLevel > 0) {
    int bzl = zLevel - 1;
    for (int i = 0; i < 4; ++i) {
      int nx = tileX + dx[i];
      int ny = tileY + dy[i];
      if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
        int sx = nx - bzl;
        int sy = ny - bzl;
        if (sx >= 0 && sy >= 0 && sx < width && sy < height) {
          TileType neighborType = rampGrid3D[bzl][sy][sx];
          if (neighborType == TileType::Ramp_SW_NE ||
              neighborType == TileType::Ramp_SE_NW ||
              neighborType == TileType::Ramp_NE_SW ||
              neighborType == TileType::Ramp_NW_SE) {
            return true;
          }
        }
      }
    }
  }
  // Kiểm tra thêm tầng trên (nếu entity đang tiếp cận ramp tầng trên)
  if (static_cast<size_t>(zLevel + 1) < rampGrid3D.size()) {
    int uzl = zLevel + 1;
    for (int i = 0; i < 4; ++i) {
      int nx = tileX + dx[i];
      int ny = tileY + dy[i];
      if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
        int sx = nx - uzl;
        int sy = ny - uzl;
        if (sx >= 0 && sy >= 0 && sx < width && sy < height) {
          TileType neighborType = rampGrid3D[uzl][sy][sx];
          if (neighborType == TileType::Ramp_SW_NE ||
              neighborType == TileType::Ramp_SE_NW ||
              neighborType == TileType::Ramp_NE_SW ||
              neighborType == TileType::Ramp_NW_SE) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

// HÀM TRỌNG TÂM: Nội suy (Interpolate) độ cao Z trơn tru khi Entity đi vào vùng
// đường Dốc (Ramp). Dành cho Client render Y thay đổi khi di chuyển trên Ramp /
// Dành cho Server update Z của entity liên tục.
//
// FIX ISOMETRIC: Khi entity di chuyển chéo trong isometric (dx != 0 && dy !=
// 0), entity có thể rời tile ramp (theo 1 trục) trước khi đi hết ramp (theo
// trục kia). Giải pháp: Khi entity ở giữa 2 tầng mà tile hiện tại không có
// ramp, quét tile lân cận. Nếu có ramp gần đó → giữ nguyên entityZ để
// transition mượt mà.
float MapData::GetInterpolatedHeight(float x, float y,
                                     float entityZ) const noexcept {
  // Tìm tọa độ lưới ô
  int tileX = static_cast<int>(x) / scaledSize;
  int tileY = static_cast<int>(y) / scaledSize;

  // Lọt biên map -> 0.0
  if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height) return 0.0f;

  // Tính Z Level hiện tại
  if (rampGrid3D.empty()) return entityZ;
  int zLevel = GetClosestZLevel(tileX, tileY, entityZ);

  // Lệch vị trí tile theo zLevel
  int sx = tileX - zLevel;
  int sy = tileY - zLevel;
  TileType type = TileType::Walkable;
  float baseH = static_cast<float>(zLevel * g_WorldConfig.blockHeight);
  if (sx >= 0 && sy >= 0 && sx < width && sy < height) {
    type = rampGrid3D[zLevel][sy][sx];
    baseH = heightGrid3D[zLevel][sy][sx];
  }

  // TRƯỜNG HỢP LOGIC NÂNG CAO CHO CẦU THANG (Stairs/Ramp Down):
  // Entity đang đứng ở sàn phẳng Tầng 1 (Tầng trên), phía dưới chân là Đỉnh của
  // Ramp Tầng 0 dẫn ngược xuống. Nếu không xử lý, hàm chỉ trả về baseH của Tầng
  // 1 => Nhân vật trôi vèo qua không lọt xuống được.
  if (type == TileType::Walkable && zLevel > 0) {
    // Check xem ngay dưới sàn phẳng này có Ramp nào ở tầng thấp hơn dẫn lên
    // không
    int bzl = zLevel - 1;
    int bsx = tileX - bzl;
    int bsy = tileY - bzl;
    if (bsx >= 0 && bsy >= 0 && bsx < width && bsy < height) {
      TileType typeBelow = rampGrid3D[bzl][bsy][bsx];
      if (typeBelow == TileType::Ramp_SW_NE ||
          typeBelow == TileType::Ramp_SE_NW ||
          typeBelow == TileType::Ramp_NE_SW ||
          typeBelow == TileType::Ramp_NW_SE) {
        type = typeBelow;  // Chuyển kiểu thành Ramp của tầng dưới
        baseH = heightGrid3D[bzl][bsy][bsx];  // Lấy base chân dốc của tầng dưới
        zLevel = bzl;      // Cập nhật zLevel vì ta xét dốc của tầng dưới
      }
    }
  }

  // TRƯỜNG HỢP LOGIC NÂNG CAO CHO CẦU THANG LIÊN TIẾP (Ramp Up):
  // Entity đang đi lên từ dốc tầng dưới và chớm bước sang ô chứa dốc tầng trên.
  // Nếu ở tầng hiện tại (zLevel) ô này là Walkable, nhưng ở tầng trên (zLevel +
  // 1) lại có Ramp, và Z của nhân vật đã leo lên đủ cao (>= 40% blockHeight
  // tầng dưới).
  if (type == TileType::Walkable &&
      static_cast<size_t>(zLevel + 1) < rampGrid3D.size()) {
    int uzl = zLevel + 1;
    int usx = tileX - uzl;
    int usy = tileY - uzl;
    if (usx >= 0 && usy >= 0 && usx < width && usy < height) {
      TileType typeAbove = rampGrid3D[uzl][usy][usx];
      if (typeAbove == TileType::Ramp_SW_NE ||
          typeAbove == TileType::Ramp_SE_NW ||
          typeAbove == TileType::Ramp_NE_SW ||
          typeAbove == TileType::Ramp_NW_SE) {
        float floorZ = static_cast<float>(zLevel * g_WorldConfig.blockHeight);
        if (entityZ >= floorZ + g_WorldConfig.blockHeight * 0.4f) {
          zLevel = uzl;
          type = typeAbove;
          baseH = heightGrid3D[zLevel][usy][usx];
        }
      }
    }
  }

  // TÍNH TOÁN ĐỘ CAO Z NỘI SUY (Từ 0.0 -> 1.0)
  // scaledSize: kích thước pixel của một ô gạch đã scale

  // 1. Dốc dọc hướng Tây Nam (SW) -> Đông Bắc (NE)
  // Hướng này chạy dọc theo trục Y. Y nhỏ (NE) -> Z cao, Y lớn (SW) -> Z thấp.
  if (type == TileType::Ramp_SW_NE) {
    float dy = y - static_cast<float>(tileY * scaledSize);
    float ratio =
        (static_cast<float>(scaledSize) - dy) / static_cast<float>(scaledSize);
    ratio = (ratio < 0.0f) ? 0.0f : ((ratio > 1.0f) ? 1.0f : ratio);
    return baseH + ratio * static_cast<float>(g_WorldConfig.blockHeight);
  }
  // 2. Dốc dọc hướng Đông Nam (SE) -> Tây Bắc (NW)
  // Hướng này chạy dọc theo trục X. X nhỏ (NW) -> Z cao, X lớn (SE) -> Z thấp.
  else if (type == TileType::Ramp_SE_NW) {
    float dx = x - static_cast<float>(tileX * scaledSize);
    float ratio =
        (static_cast<float>(scaledSize) - dx) / static_cast<float>(scaledSize);
    ratio = (ratio < 0.0f) ? 0.0f : ((ratio > 1.0f) ? 1.0f : ratio);
    return baseH + ratio * static_cast<float>(g_WorldConfig.blockHeight);
  }
  // 3. Dốc dọc hướng Đông Bắc (NE) -> Tây Nam (SW)
  // Hướng này chạy dọc theo trục Y. Y nhỏ (NE) -> Z thấp, Y lớn (SW) -> Z cao.
  else if (type == TileType::Ramp_NE_SW) {
    float dy = y - static_cast<float>(tileY * scaledSize);
    float ratio = dy / static_cast<float>(scaledSize);
    ratio = (ratio < 0.0f) ? 0.0f : ((ratio > 1.0f) ? 1.0f : ratio);
    return baseH + ratio * static_cast<float>(g_WorldConfig.blockHeight);
  }
  // 4. Dốc dọc hướng Tây Bắc (NW) -> Đông Nam (SE)
  // Hướng này chạy dọc theo trục X. X nhỏ (NW) -> Z thấp, X lớn (SE) -> Z cao.
  else if (type == TileType::Ramp_NW_SE) {
    float dx = x - static_cast<float>(tileX * scaledSize);
    float ratio = dx / static_cast<float>(scaledSize);
    ratio = (ratio < 0.0f) ? 0.0f : ((ratio > 1.0f) ? 1.0f : ratio);
    return baseH + ratio * static_cast<float>(g_WorldConfig.blockHeight);
  }

  // FIX ISOMETRIC: Tile hiện tại KHÔNG CÓ ramp (Walkable), nhưng entity đang ở
  // GIỮA 2 tầng (Z không phải bội chẵn của blockHeight). Điều này xảy ra khi
  // entity di chuyển chéo isometric và rời khỏi tile ramp trước khi leo xong.
  // Nếu có ramp lân cận → giữ nguyên entityZ để transition mượt mà thay vì
  // rớt Z đột ngột.
  float floorZ = static_cast<float>(zLevel * g_WorldConfig.blockHeight);
  float remainder = entityZ - floorZ;
  if (remainder > 1.0f && remainder < g_WorldConfig.blockHeight - 1.0f) {
    // Entity đang nằm giữa 2 tầng → quét tile lân cận tìm ramp
    if (HasNeighborRamp(tileX, tileY, zLevel)) {
      return entityZ;  // Giữ nguyên Z hiện tại, không nhảy đột ngột
    }
  }

  // Nếu không nằm trong ô Ramp dốc, trả về trực tiếp chiều cao Flat base cố
  // định của sàn đó
  return baseH;
}

// Tìm tầng Z gần nhất có địa hình thực tế tại tọa độ tile logic
int MapData::GetClosestZLevel(int tileX, int tileY, float entityZ) const noexcept {
  if (heightGrid3D.empty()) return 0;
  int maxZ = static_cast<int>(heightGrid3D.size());
  int bestZ = 0;
  float minDiff = 999999.0f;

  for (int z = 0; z < maxZ; ++z) {
    int sx = tileX - z;
    int sy = tileY - z;
    if (sx >= 0 && sy >= 0 && sx < width && sy < height) {
      float floorZ = heightGrid3D[z][sy][sx];
      if (floorZ != -999.0f) {
        float diff = std::abs(entityZ - floorZ);
        if (diff < minDiff) {
          minDiff = diff;
          bestZ = z;
        }
      }
    }
  }

  if (minDiff == 999999.0f) {
    int fallbackZ = static_cast<int>((entityZ + 1.0f) / g_WorldConfig.blockHeight);
    if (fallbackZ < 0) fallbackZ = 0;
    if (fallbackZ >= maxZ) fallbackZ = maxZ - 1;
    return fallbackZ;
  }

  return bestZ;
}

// In thông số lưới bản đồ 3D ra Terminal để lập trình viên kiểm tra dữ liệu
// Server. Khác với Client, Server in kèm thêm 1 lưới Height Grid số nguyên.
void MapData::PrintMap() const {
  std::cout
      << "\n================= SERVER MAPDATA DEBUG PRINT ================="
      << std::endl;
  std::cout << "Dimensions: " << width << "x" << height << " tiles"
            << std::endl;
  std::cout << "Tile Size: " << tileSize << "px | Scale: x" << mapScale
            << " | Scaled Size: " << scaledSize << "px" << std::endl;
  std::cout << "Z-Levels: " << typeGrid3D.size() << std::endl;

  for (size_t z = 0; z < typeGrid3D.size(); ++z) {
    // Print lưới Collision
    std::cout << "\n--- Z-Level " << z << " Collision Grid (typeGrid3D) ---"
              << std::endl;
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

    std::cout << "     ";
    for (int x = 0; x < width; ++x) {
      std::cout << (x % 10) << " ";
    }
    std::cout << std::endl;

    // Print lưới Độ Cao Height
    if (z < heightGrid3D.size()) {
      std::cout << "\n--- Z-Level " << z
                << " Height Grid (heightGrid3D in pixels) ---" << std::endl;
      for (int y = 0; y < height; ++y) {
        if (y < 10) std::cout << " ";
        std::cout << y << " | ";
        for (int x = 0; x < width; ++x) {
          std::cout << static_cast<int>(heightGrid3D[z][y][x])
                    << "\t";  // In cao độ bằng pixel
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
  std::cout
      << "==============================================================\n"
      << std::endl;
}
