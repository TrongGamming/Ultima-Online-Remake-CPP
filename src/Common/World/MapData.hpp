#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Enum định nghĩa các loại địa hình ô gạch (TileType) cho mục đích va chạm và leo dốc
enum class TileType : uint8_t {
    Walkable = 0,   // Có thể đi bộ bình thường (địa hình phẳng)
    Blocked = 1,    // Bị chặn hoàn toàn (tường, vực sâu, chướng ngại vật cứng)
    Ramp_SW_NE = 2, // Dốc đi lên hướng từ Tây Nam (dưới-trái) sang Đông Bắc (trên-phải)
    Ramp_SE_NW = 3  // Dốc đi lên hướng từ Đông Nam (dưới-phải) sang Tây Bắc (trên-trái)
};

// Lớp MapData lưu trữ và xử lý cấu trúc dữ liệu bản đồ đa tầng của thế giới game.
// Đọc dữ liệu từ file map cấu hình và cung cấp các hàm va chạm cứng, va chạm chiều cao,
// và nội suy độ cao trơn tru trên dốc (Ramp).
class MapData {
public:
    MapData() = default;
    ~MapData() = default;

    // Tải dữ liệu bản đồ từ file .tmx
    bool LoadMap(const std::string& path);
    
    // Kiểm tra xem hình hộp va chạm (AABB) của thực thể có bị chặn ở vị trí (x, y, z) hay không
    [[nodiscard]] bool IsBlocked(float x, float y, float z, float w, float h) const noexcept;
    
    // Kiểm tra xem một ô gạch cụ thể (tileX, tileY) có cản trở thực thể ở độ cao entityZ không
    [[nodiscard]] bool IsTileBlocked(int tileX, int tileY, float entityZ) const noexcept;
    
    // Lấy độ cao Z gốc của một ô gạch cụ thể
    [[nodiscard]] float GetTileHeight(int tileX, int tileY, int zLevel) const noexcept;
    [[nodiscard]] TileType GetTileType(int tileX, int tileY, int zLevel) const noexcept;
    
    // Tính toán độ cao nội suy chính xác của nhân vật tại tọa độ số thực (x, y) khi đang leo dốc (Ramp)
    [[nodiscard]] float GetInterpolatedHeight(float x, float y, float entityZ) const noexcept;

    // In thông tin bản đồ và các lưới (typeGrid3D, heightGrid3D) ra terminal để debug
    void PrintMap() const;
    
    // Các hàm getter lấy thông số của bản đồ
    [[nodiscard]] int GetWidth() const noexcept { return width; } // Lấy chiều rộng bản đồ (số ô gạch)
    [[nodiscard]] int GetHeight() const noexcept { return height; } // Lấy chiều cao bản đồ (số ô gạch)
    [[nodiscard]] int GetTileSize() const noexcept { return tileSize; } // Lấy kích thước gốc của một ô gạch (32px)
    [[nodiscard]] int GetMapScale() const noexcept { return mapScale; } // Lấy tỷ lệ phóng to bản đồ (x3)
    [[nodiscard]] int GetScaledSize() const noexcept { return scaledSize; } // Lấy kích thước gạch sau khi scale (96px)

private:
    int width = 0;       // Chiều rộng bản đồ (số cột gạch)
    int height = 0;      // Chiều cao bản đồ (số hàng gạch)
    int tileSize = 32;   // Kích thước ô gạch gốc (32 pixel)
    int mapScale = 3;    // Hệ số tỷ lệ vẽ (mặc định x3)
    int scaledSize = 96; // Kích thước ô gạch vẽ thực tế: tileSize * mapScale (96 pixel)
    
    std::vector<std::vector<int>> groundGrid;      // Lưới chỉ số tile nền đất (Ground)
    std::vector<std::vector<int>> decorGrid;       // Lưới chỉ số các vật trang trí hoặc vách đá (Decoration/Cliff)
    std::vector<std::vector<std::vector<float>>> heightGrid3D;    // Lưới lưu trữ độ cao Z thực tế (bằng pixel) của từng ô gạch
    std::vector<std::vector<std::vector<TileType>>> typeGrid3D;   // Lưới kiểu va chạm 3D (Z, Y, X)
    std::vector<std::vector<std::vector<TileType>>> rampGrid3D;   // Lưới lưu riêng kiểu Ramp (không bị ghi đè bởi non-ramp, dùng cho nội suy dốc)
};
