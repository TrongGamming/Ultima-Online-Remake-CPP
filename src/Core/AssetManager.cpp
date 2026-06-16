#include "Core/AssetManager.hpp"
#include "ECS/Components/ProjectileComponent.hpp"
#include "ECS/Components/SpriteComponent.hpp"
#include "ECS/Components/TransformComponent.hpp"
#include "ECS/Components/ColliderComponent.hpp"
#include "Core/Game.hpp"
#include "Core/TextureManager.hpp"
#include <iostream>

// Constructor gán con trỏ ECS Manager
AssetManager::AssetManager(Manager *man) : manager(man) {}

// Destructor thực hiện giải phóng tài nguyên để tránh rò rỉ bộ nhớ
AssetManager::~AssetManager() {
  // 1. Quét bản đồ texture và giải phóng tất cả SDL_Texture đang có
  for (auto &[id, tex] : textures) {
    if (tex) {
      SDL_DestroyTexture(tex);
    }
  }
  textures.clear();

  // 2. Quét bản đồ font và đóng toàn bộ font TTF bằng SDL_ttf
  for (auto &[id, font] : fonts) {
    if (font) {
      TTF_CloseFont(font);
    }
  }
  fonts.clear();
}

// Khởi tạo một viên đạn bay (Projectile) trong ECS
void AssetManager::CreateProjectile(Vector2D pos, Vector2D vel, int range,
                                    int speed, const std::string &id) {
  // Tạo thực thể Entity mới từ Manager
  auto &projectile(manager->addEntity());
  
  // Thêm TransformComponent quản lý tọa độ vị trí và kích thước 32x32px
  projectile.addComponent<TransformComponent>(pos.x, pos.y, 32, 32, 1);
  
  // Thêm SpriteComponent vẽ hình ảnh viên đạn (không chạy hoạt ảnh)
  projectile.addComponent<SpriteComponent>(id, false);
  
  // Thêm ProjectileComponent để xử lý vận tốc bay, tầm bay và hủy đạn
  projectile.addComponent<ProjectileComponent>(range, speed, vel);
  
  // Thêm ColliderComponent để xử lý va chạm của đạn với tường hoặc quái vật
  projectile.addComponent<ColliderComponent>("projectile");
  
  // Phân loại viên đạn vào nhóm groupProjectiles
  projectile.addGroup(Game::groupProjectiles);
}

// Tải một file ảnh lên bộ nhớ đồ họa và gán với khóa ID
void AssetManager::AddTexture(const std::string &id, const char *path) {
  SDL_Texture *tex = TextureManager::LoadTexture(path);
  if (tex) {
    textures.emplace(id, tex); // Đưa vào bản đồ textures nếu tải thành công
  } else {
    std::cerr << "Failed to load texture for ID: " << id
              << " from path: " << path << std::endl;
  }
}

// Lấy texture từ bản đồ textures
SDL_Texture *AssetManager::GetTexture(const std::string &id) {
  auto it = textures.find(id);
  if (it != textures.end()) {
    return it->second; // Trả về con trỏ texture
  }
  return nullptr;
}

// Tải một font chữ TTF với kích thước size cụ thể
void AssetManager::AddFont(const std::string &id, const std::string &path,
                           int fontSize) {
  std::string actualPath = TextureManager::GetAssetPath(path);
  // Dùng thư viện SDL_ttf để mở font
  TTF_Font *font = TTF_OpenFont(actualPath.c_str(), static_cast<float>(fontSize));
  if (font) {
    fonts.emplace(id, font); // Lưu trữ nếu thành công
  } else {
    std::cerr << "Failed to load font for ID: " << id << " from path: " << path
              << " with error: " << SDL_GetError() << std::endl;
  }
}

// Lấy font chữ từ bản đồ fonts
TTF_Font *AssetManager::GetFont(const std::string &id) {
  auto it = fonts.find(id);
  if (it != fonts.end()) {
    return it->second; // Trả về con trỏ font
  }
  return nullptr;
}
