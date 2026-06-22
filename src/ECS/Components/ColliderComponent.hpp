#pragma once
#include <SDL3/SDL.h>

#include <string>

#include "Common/World/WorldConfig.hpp"
#include "Core/Game.hpp"
#include "Core/TextureManager.hpp"
#include "ECS/Components/TransformComponent.hpp"
#include "ECS/ECS.hpp"

// ColliderComponent quản lý hộp va chạm của một thực thể (AABB trong không gian
// Cartesian). Nó cung cấp khả năng tự động cập nhật vị trí hộp va chạm theo
// thực thể, đồng thời hỗ trợ tính toán tọa độ hiển thị Isometric để vẽ các
// khung va chạm trực quan khi gỡ lỗi (debug).
class ColliderComponent : public Component {
 public:
  // Cờ tĩnh cho biết có hiển thị tất cả các hộp va chạm trên màn hình hay không
  // (dành cho debug)
  static inline bool showColliders = false;

  SDL_Rect collider{};  // Hộp va chạm chính trong không gian phẳng Cartesian 2D
                        // (x, y, w, h)
  std::string tag;  // Thẻ phân biệt loại collider (ví dụ: "player", "terrain",
                    // "npc"...)

  SDL_Texture* tex{nullptr};  // Texture ảnh vẽ hộp va chạm để gỡ lỗi trực quan
  SDL_Rect srcR{};            // Vùng cắt của texture vẽ
  SDL_FRect destR{};  // Tọa độ vẽ hộp va chạm lên màn hình (đã quy đổi theo hệ
                      // Camera & Isometric)

  TransformComponent* transform{nullptr};  // Con trỏ tới TransformComponent của
                                           // thực thể để lấy tọa độ vị trí

  // Constructor khởi tạo chỉ với thẻ phân loại (ví dụ đối với Player/NPC sẽ tự
  // động tính vị trí từ Transform)
  ColliderComponent(const std::string& t) { tag = t; }

  // Constructor khởi tạo với vị trí và kích thước cụ thể (ví dụ đối với các ô
  // gạch cản trở của map)
  ColliderComponent(const std::string& t, int xpos, int ypos, int size) {
    tag = t;
    // Lưu tọa độ Cartesian phẳng gốc
    collider.x = xpos;
    collider.y = ypos;
    collider.w = size;
    collider.h = size;
  }

  // Destructor giải phóng texture vẽ debug
  ~ColliderComponent() override {
    if (tex) {
      SDL_DestroyTexture(tex);
    }
  }

  // Khởi tạo thành phần: Liên kết với Transform và tải ảnh vẽ debug
  void init() override {
    // Đảm bảo thực thể có TransformComponent để lấy vị trí
    if (!entity->hasComponent<TransformComponent>()) {
      entity->addComponent<TransformComponent>();
    }

    transform = &entity->getComponent<TransformComponent>();

    // Tải ảnh khung lưới va chạm để vẽ debug
    tex = TextureManager::LoadTexture("assets/test-col.png");
    srcR = {0, 0, 32, 32};
    // Khởi tạo kích thước vẽ debug ban đầu
    destR = {static_cast<float>(collider.x), static_cast<float>(collider.y),
             static_cast<float>(collider.w), static_cast<float>(collider.h)};
  }

  // Cập nhật vị trí hộp va chạm theo tọa độ Cartesian thực tế của thực thể và
  // chuyển đổi sang không gian camera
  void update() override {
    // Nếu không phải là collider tĩnh của bản đồ (terrain):
    // Cập nhật hộp va chạm Cartesian bám sát theo vị trí của nhân vật cộng với
    // các khoảng offset cấu hình
    if (tag != "terrain") {
      collider.x = static_cast<int>(transform->position.x +
                                    g_WorldConfig.colliderOffsetX);
      collider.y = static_cast<int>(transform->position.y +
                                    g_WorldConfig.colliderOffsetY);
      collider.w = static_cast<int>(g_WorldConfig.colliderWidth);
      collider.h = static_cast<int>(g_WorldConfig.colliderHeight);
    }

    // Tính toán tọa độ và kích thước hiển thị vẽ debug (destR) dựa trên chế độ
    // camera hiện tại
    if (Game::isIsometric) {
      // 1. Chuyển đổi tọa độ Cartesian sang Isometric
      float isoX = (collider.x - collider.y) * 0.5f;
      float isoY = (collider.x + collider.y) * 0.25f - transform->position.z;

      // 2. Trừ đi tọa độ camera để lấy vị trí tương đối trên màn hình
      destR.x = isoX - Game::camera.x;
      destR.y = isoY - Game::camera.y;

      // 3. Quy đổi hình dáng vẽ: chiều cao hình thoi dẹt bằng một nửa chiều
      // rộng (tỷ lệ 2:1 chuẩn Isometric)
      destR.w = static_cast<float>(collider.w);
      destR.h = static_cast<float>(collider.h) / 2.0f;
    } else {
      // Giữ nguyên hệ tọa độ Cartesian phẳng 2D thông thường
      destR.x = static_cast<float>(collider.x - Game::camera.x);
      destR.y = static_cast<float>(collider.y - Game::camera.y -
                                   transform->position.z);
      destR.w = static_cast<float>(collider.w);
      destR.h = static_cast<float>(collider.h);
    }
  }

  // Vẽ hộp va chạm lên màn hình nếu cờ showColliders được bật lên true
  void draw() override {
    if (showColliders) {
      TextureManager::Draw(tex, srcR, destR, SDL_FLIP_NONE);
    }
  }
};
