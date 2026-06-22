#pragma once
#include <string>
#include <vector>
#include "Common/World/MapData.hpp" // Đính kèm để sử dụng enum class TileType

// Thông tin chi tiết về từng Tileset được nạp từ bản đồ
struct TilesetInfo {
    int firstgid = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    int columns = 0;
    std::string textureId;
};

// Lớp Map chịu trách nhiệm xây dựng hình ảnh bản đồ game trên Client.
// Đọc cấu trúc lưới gạch từ tệp .map và sinh ra các Entity chứa TileComponent
// tương ứng để vẽ địa hình (bao gồm nền đất cỏ, dốc, vách đá) và các Collider cản trở.
class Map 
{
public:
    // Constructor khởi tạo bản đồ
    // tID: Khóa ID của texture chứa tilesheet địa hình trong AssetManager
    // ms: Hệ số tỷ lệ phóng to (map scale, ví dụ x3)
    // ts: Kích thước pixel một ô gạch gốc (tile size, ví dụ 32px)
    Map(std::string tID, int ms, int ts);
    ~Map() = default;

    // Tải cấu trúc bản đồ từ đĩa cứng và thiết lập các gạch vẽ, các hộp va chạm
    // path: Đường dẫn đến tệp map cấu hình (ví dụ "assets/untitled.tmx")
    void LoadMap(const std::string& path);
    
    // Tạo mới một thực thể gạch (Tile) và thêm vào danh sách quản lý của ECS
    // srcX, srcY: Tọa độ pixel cắt trên tilesheet
    // xpos, ypos: Vị trí phẳng Cartesian thế giới
    // zpos: Độ cao Z của ô gạch
    // textureId: Khóa ID của texture chứa ô gạch này
    // tSize: Kích thước ô gạch
    void AddTile(int srcX, int srcY, int xpos, int ypos, float zpos, const std::string& textureId, int tSize);

    // In cấu trúc bản đồ và lưới va chạm ra terminal để debug
    void PrintMap() const;

private:
    std::string texID;   // Khóa ID texture của tilesheet mặc định
    int mapScale = 1;    // Tỷ lệ phóng to bản đồ khi render
    int tileSize = 32;   // Kích thước ô gạch gốc
    int scaledSize = 32; // Kích thước ô gạch thực tế sau khi nhân hệ số scale (tileSize * mapScale)
    
    int width = 0;       // Chiều rộng bản đồ (số cột gạch)
    int height = 0;      // Chiều cao bản đồ (số hàng gạch)
    std::vector<std::vector<std::vector<TileType>>> typeGrid3D; // Lưới va chạm 3D để debug
    std::vector<std::vector<std::vector<float>>> heightGrid3D;  // Lưới độ cao Z (pixel) 3D cho nội suy isometric
    std::vector<std::vector<std::vector<TileType>>> rampGrid3D; // Lưới ramp 3D (không bị ghi đè bởi non-ramp)
    
    std::vector<TilesetInfo> tilesets; // Danh sách các tileset động được nạp
};

