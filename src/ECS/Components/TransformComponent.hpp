#pragma once
#include "Common/Math/Vector2D.hpp"
#include "Common/Math/Vector3D.hpp"
#include "ECS/ECS.hpp"

// TransformComponent quản lý vị trí 3D, vận tốc 2D, kích thước vật lý và tỷ lệ scale của thực thể.
// Đây là thành phần nền tảng cho mọi chuyển động vật lý và định vị hình ảnh hiển thị trong game.
class TransformComponent : public Component {
 public:
  Vector3D position; // Vị trí hiện tại trong không gian 3D thế giới game (position.z biểu thị chiều cao)
  Vector2D velocity; // Hướng vận tốc di chuyển hiện tại (giá trị chuẩn hóa -1, 0, 1)

  int height = 32; // Chiều cao Sprite gốc (chưa nhân scale, mặc định 32 pixel)
  int width = 32;  // Chiều rộng Sprite gốc (chưa nhân scale, mặc định 32 pixel)
  int scale = 1;   // Hệ số phóng to hiển thị của thực thể (mặc định x1)
  int speed = 3;   // Tốc độ di chuyển thực tế (số pixel di chuyển trên mỗi frame)

  bool blocked = false; // Cờ báo thực thể có đang bị kẹt hoặc va chạm vật cản cứng không

  // Constructor mặc định đặt vị trí về (0, 0, 0)
  TransformComponent() { position.Zero(); }

  // Constructor khởi tạo với hệ số scale
  TransformComponent(int sc) {
    position.Zero();
    scale = sc;
  }

  // Constructor khởi tạo tọa độ vị trí 3D ban đầu
  TransformComponent(float x, float y, float z = 0) {
    position.x = x;
    position.y = y;
    position.z = z;
  }

  // Constructor khởi tạo tọa độ 2D phẳng, kích thước và hệ số scale
  TransformComponent(float x, float y, int h, int w, int sc) {
    position.x = x;
    position.y = y;
    position.z = 0.0f;
    height = h;
    width = w;
    scale = sc;
  }

  // Constructor khởi tạo đầy đủ vị trí 3D, kích thước và hệ số scale
  TransformComponent(float x, float y, float zpos, int h, int w, int sc) {
    position.x = x;
    position.y = y;
    position.z = zpos;
    height = h;
    width = w;
    scale = sc;
  }

  // Khởi tạo thành phần: reset vector vận tốc di chuyển về 0
  void init() override { velocity.Zero(); }

  // Cập nhật tọa độ vị trí mỗi frame dựa trên hướng vận tốc di chuyển (velocity) và tốc độ (speed)
  void update() override {
    // Tích lũy dịch chuyển dọc trục X
    position.x += static_cast<float>(velocity.x * speed);
    // Tích lũy dịch chuyển dọc trục Y
    position.y += static_cast<float>(velocity.y * speed);
  }
};
