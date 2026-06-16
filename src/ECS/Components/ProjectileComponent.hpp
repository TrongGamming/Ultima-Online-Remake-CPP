#pragma once

#include "Core/Game.hpp"
#include "ECS/Components/TransformComponent.hpp"
#include "ECS/ECS.hpp"
#include "Common/Math/Vector2D.hpp"

// ProjectileComponent quản lý các vật thể bay (ví dụ: đạn, mũi tên, phép thuật).
// Điều khiển quãng đường bay tối đa (range), vận tốc bay (speed, velocity),
// và tự động hủy thực thể viên đạn khi bay ra ngoài tầm bắn hoặc bay ra khỏi góc nhìn Camera.
class ProjectileComponent : public Component {
 public:
  // Constructor khởi tạo tầm bắn, tốc độ bay và vector hướng vận tốc
  ProjectileComponent(int rng, int sp, Vector2D vel)
      : range(rng), speed(sp), velocity(vel) {}

  ~ProjectileComponent() override = default;

  // Khởi tạo thành phần: lấy Transform và gán vận tốc bay ban đầu
  void init() override {
    transform = &entity->getComponent<TransformComponent>();
    transform->velocity = velocity;
  }

  // Cập nhật logic viên đạn mỗi frame
  void update() override {
    // Tích lũy khoảng cách đã bay được dựa vào vận tốc speed
    distance += speed;

    // 1. Nếu vượt quá tầm bắn tối đa (range): Hủy viên đạn
    if (distance > range) {
      std::cout << "Out of Range" << std::endl;
      entity->destroy(); // Đánh dấu hủy thực thể đạn để Manager thu hồi
    } else {
      // 2. Chuyển đổi tọa độ viên đạn sang không gian camera để kiểm tra xem có bay ngoài màn hình không
      float screenX = 0;
      float screenY = 0;
      if (Game::isIsometric) {
        float isoX = (transform->position.x - transform->position.y) * 0.5f;
        float isoY = (transform->position.x + transform->position.y) * 0.25f;
        screenX = isoX - Game::camera.x;
        screenY = isoY - Game::camera.y;
      } else {
        screenX = transform->position.x - Game::camera.x;
        screenY = transform->position.y - Game::camera.y;
      }
      
      // 3. Nếu bay ra khỏi biên màn hình hiển thị (với buffer đệm 96px để tránh bị biến mất đột ngột ở rìa): Hủy viên đạn
      if (screenX > Game::camera.w || screenX < -96 ||
          screenY > Game::camera.h || screenY < -96) {
        std::cout << "Out of bounds!" << std::endl;
        entity->destroy();
      }
    }
  }

 private:
  TransformComponent *transform{nullptr}; // Con trỏ tới TransformComponent
  int range = 0;                          // Tầm bắn tối đa (quãng đường bay tối đa)
  int speed = 0;                          // Tốc độ bay mỗi frame
  int distance = 0;                       // Quãng đường thực tế đã bay được
  Vector2D velocity;                      // Vector vận tốc hướng bay
};
