#pragma once

#include "Common/Math/Vector2D.hpp"
#include "ECS/ECS.hpp"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <unordered_map>

// AssetManager chịu trách nhiệm quản lý tập trung và chia sẻ các tài nguyên dùng chung trong game
// (như texture hình ảnh, font chữ) để tránh việc nạp trùng lặp tài nguyên, giúp tối ưu hóa RAM và VRAM.
// Đồng thời nó cũng cung cấp helper để khởi tạo nhanh các thực thể động như đạn bay (projectile).
class AssetManager {
public:
  // Constructor liên kết quản lý với một Manager của hệ thống ECS
  explicit AssetManager(Manager *man);
  
  // Destructor giải phóng toàn bộ tài nguyên hình ảnh và font chữ đã tải
  ~AssetManager();

  // Helper khởi tạo nhanh một thực thể đạn bay (projectile) có đầy đủ các component cần thiết
  // pos: Vị trí xuất phát
  // vel: Vector hướng vận tốc
  // range: Tầm bắn tối đa
  // speed: Tốc độ bay mỗi frame
  // id: ID của texture hiển thị đạn
  void CreateProjectile(Vector2D pos, Vector2D vel, int range, int speed,
                        const std::string &id);

  // Tải hình ảnh từ đĩa cứng và đăng ký vào bộ quản lý với một khóa ID cụ thể
  void AddTexture(const std::string &id, const char *path);
  
  // Lấy ra con trỏ quản lý texture hình ảnh tương ứng với ID đã đăng ký
  [[nodiscard]] SDL_Texture *GetTexture(const std::string &id);

  // Tải font chữ TTF từ đĩa cứng với kích thước chỉ định và đăng ký vào bộ quản lý
  void AddFont(const std::string &id, const std::string &path, int fontSize);
  
  // Lấy ra con trỏ quản lý font chữ tương ứng với ID đã đăng ký
  [[nodiscard]] TTF_Font *GetFont(const std::string &id);

private:
  Manager *manager; // Con trỏ tới ECS Manager để tạo thực thể đạn
  std::unordered_map<std::string, SDL_Texture *> textures; // Bản đồ lưu trữ tài nguyên hình ảnh
  std::unordered_map<std::string, TTF_Font *> fonts;       // Bản đồ lưu trữ tài nguyên font chữ
};
