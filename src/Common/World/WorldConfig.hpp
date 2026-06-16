#pragma once

// Cấu trúc WorldConfig chứa các cài đặt cấu hình toàn cục cho thế giới game.
// Quản lý kích thước gạch (tile), chiều cao block, các thông số va chạm
// (collider offset) và điểm neo (pivot) để tính độ cao bản đồ dạng Isometric.
struct WorldConfig {
  // Chiều cao hiển thị của một khối block gạch trong hệ Isometric (dùng để dịch
  // chuyển sprite)
  float blockHeight = 48.0f;

  // Kích thước của một ô gạch đã được scale (ví dụ gốc là 32px nhân với tỷ lệ
  // scale 3 thành 96px)
  int scaledSize = 96;

  // Độ lệch trục X của hộp va chạm (Collider) so với góc trên-trái thực thể
  float colliderOffsetX = 24.0f;

  // Độ lệch trục Y của hộp va chạm (Collider) so với góc trên-trái thực thể
  float colliderOffsetY = 24.0f;

  // Chiều rộng hộp va chạm của thực thể dùng để kiểm tra AABB
  float colliderWidth = 48.0f;

  // Chiều cao hộp va chạm của thực thể dùng để kiểm tra AABB
  float colliderHeight = 48.0f;

  // Điểm neo trục X (tâm của collider) dùng để nội suy độ cao địa hình bản đồ
  float pivotOffsetX = 48.0f;

  // Điểm neo trục Y (tâm của collider) dùng để nội suy độ cao địa hình bản đồ
  float pivotOffsetY = 48.0f;

  // Constructor khởi tạo và tính toán điểm neo tự động
  WorldConfig() { Initialize(); }

  // Phương thức tính toán tâm (pivot) của thực thể dựa trên độ lệch (offset) và
  // kích thước collider Tâm này sẽ là điểm tiếp xúc mặt đất thực tế để lấy giá
  // trị chiều cao Z của địa hình.
  void Initialize() {
    pivotOffsetX = colliderOffsetX + (colliderWidth / 2.0f);
    pivotOffsetY = colliderOffsetY + (colliderHeight / 2.0f);
  }
};

// Khai báo biến toàn cục đại diện cho cấu hình thế giới game (được định nghĩa
// trong WorldConfig.cpp)
extern WorldConfig g_WorldConfig;
