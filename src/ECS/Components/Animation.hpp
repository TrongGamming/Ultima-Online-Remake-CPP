#pragma once

// Cấu trúc Animation định nghĩa các thông số để chạy hoạt ảnh Sprite từ một ảnh Sprite Sheet (tấm ảnh chứa nhiều khung hình).
struct Animation {
  // Chỉ số dòng (row index) của hoạt ảnh này trên ảnh Sprite Sheet (bắt đầu từ 0)
  int row = 0;
  
  // Chỉ số cột (column index) bắt đầu của hoạt ảnh (bắt đầu từ 0)
  int col = 0;
  
  // Số lượng khung hình (frames) của hoạt ảnh này
  int frames = 0;
  
  // Tốc độ chuyển cảnh giữa các khung hình (bằng mili-giây, ví dụ 100ms chuyển 1 khung hình)
  int speed = 0;

  // Constructor mặc định rỗng
  Animation() = default;
  
  // Constructor khởi tạo đầy đủ các thông số hoạt ảnh
  Animation(int r, int c, int f, int sp)
      : row(r), col(c), frames(f), speed(sp) {}
};
