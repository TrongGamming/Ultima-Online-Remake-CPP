#pragma once

#include "Common/Math/Vector2D.hpp"
#include "Core/Game.hpp"
#include "Core/TextureManager.hpp"
#include "ECS/ECS.hpp"
#include <SDL3/SDL.h>

// TileComponent quản lý việc vẽ một ô gạch địa hình tĩnh trên bản đồ bản đồ.
// Mỗi ô gạch có một vị trí Cartesian cố định, chiều cao Z, kích thước, và texture từ tilesheet.
// Tính toán vị trí vẽ Isometric tương ứng với chiều cao Z để hiển thị địa hình đa tầng.
class TileComponent : public Component {
 public:
  SDL_Texture *texture{nullptr}; // Con trỏ tới texture chứa tấm ảnh tilesheet địa hình
  SDL_Rect srcRect{};             // Khung cắt hình chữ nhật xác định ô gạch nào cần lấy trên tilesheet
  SDL_FRect destRect{};           // Vùng vẽ hình chữ nhật trên màn hình (đã quy đổi theo camera và hệ Isometric)
  Vector2D position;             // Tọa độ phẳng Cartesian cố định của ô gạch trong thế giới game
  float z = 0.0f;                // Độ cao Z thực tế (tính bằng pixel) của ô gạch này

  TileComponent() = default;

  ~TileComponent() override = default;

  // Constructor khởi tạo ô gạch địa hình
  // srcX, srcY: Tọa độ pixel cắt trên tilesheet
  // xpos, ypos: Tọa độ phẳng Cartesian thế giới
  // zpos: Độ cao Z của ô gạch
  // tsize: Kích thước pixel ô gạch gốc (mặc định 32px)
  // tscale: Hệ số nhân kích thước (mặc định x3)
  // id: ID của texture lưu trong AssetManager
  TileComponent(int srcX, int srcY, int xpos, int ypos, float zpos, int tsize, int tscale,
                const std::string &id) {
    texture = Game::assets->GetTexture(id);

    // Thiết lập tọa độ và kích thước khung cắt trên tilesheet
    srcRect.x = srcX;
    srcRect.y = srcY;
    srcRect.w = srcRect.h = tsize;

    // Thiết lập tọa độ phẳng Cartesian
    position.x = static_cast<float>(xpos);
    position.y = static_cast<float>(ypos);
    z = zpos; // Lưu độ cao Z của ô gạch

    // Tính kích thước vẽ thực tế sau khi nhân hệ số scale (ví dụ 32 * 3 = 96px)
    destRect.w = destRect.h = static_cast<float>(tsize * tscale);
  }

  // Cập nhật vị trí vẽ của ô gạch trên màn hình theo camera hiện tại
  void update() override {
    if (Game::isIsometric) {
      // 1. Chuyển đổi tọa độ Cartesian sang Isometric
      float isoX = (position.x - position.y) * 0.5f;
      // Trừ đi z để dịch chuyển ô gạch lên trên theo trục dọc, tạo hiệu ứng chiều cao 3D
      float isoY = (position.x + position.y) * 0.25f - z; 
      
      // 2. Trừ đi tọa độ camera để lấy vị trí màn hình tương đối
      destRect.x = isoX - Game::camera.x;
      destRect.y = isoY - Game::camera.y;
    } else {
      // Hệ Cartesian phẳng thông thường
      destRect.x = position.x - Game::camera.x;
      destRect.y = position.y - Game::camera.y - z; // Dịch lên theo chiều cao z
    }
  }

  // Gọi hàm vẽ ô gạch
  void draw() override {
    TextureManager::Draw(texture, srcRect, destRect, SDL_FLIP_NONE);
  }
};
