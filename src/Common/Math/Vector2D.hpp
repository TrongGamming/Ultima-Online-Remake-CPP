#pragma once
#include <iostream>

// Lớp Vector2D đại diện cho một Vector toán học hai chiều (x, y)
// Thường được dùng để lưu trữ tọa độ, vận tốc, lực hoặc các đại lượng hướng trong không gian 2D.
class Vector2D {
public:
  // Thành phần x và y của Vector (kiểu số thực float)
  float x = 0.0f;
  float y = 0.0f;

  // Constructor mặc định khởi tạo Vector rỗng (x = 0, y = 0)
  Vector2D() = default;
  
  // Constructor khởi tạo Vector với các giá trị x, y cụ thể
  Vector2D(float x, float y);

  // Phương thức cộng thêm một Vector khác vào Vector hiện tại
  Vector2D &Add(const Vector2D &vec);
  
  // Phương thức trừ Vector hiện tại đi một Vector khác
  Vector2D &Subtract(const Vector2D &vec);
  
  // Phương thức nhân Vector hiện tại với một Vector khác (từng thành phần nhân với nhau)
  Vector2D &Multiply(const Vector2D &vec);
  
  // Phương thức chia Vector hiện tại cho một Vector khác (từng thành phần chia cho nhau)
  Vector2D &Divide(const Vector2D &vec);

  // Các toán tử friend cho phép thực hiện phép tính cộng, trừ, nhân, chia giữa hai Vector2D
  // Trả về một đối tượng Vector2D mới mà không làm thay đổi các toán hạng
  friend Vector2D operator+(Vector2D v1, const Vector2D &v2);
  friend Vector2D operator-(Vector2D v1, const Vector2D &v2);
  friend Vector2D operator*(Vector2D v1, const Vector2D &v2);
  friend Vector2D operator/(Vector2D v1, const Vector2D &v2);

  // Các toán tử gán tích hợp (+=, -=, *=, /=) giúp thay đổi trực tiếp Vector hiện tại
  Vector2D &operator+=(const Vector2D &vec);
  Vector2D &operator-=(const Vector2D &vec);
  Vector2D &operator*=(const Vector2D &vec);
  Vector2D &operator/=(const Vector2D &vec);

  // Toán tử nhân Vector với một số vô hướng (scalar), nhân cả x và y với số đó
  Vector2D operator*(float scalar) const;
  
  // Reset Vector hiện tại về trạng thái gốc (x = 0.0f, y = 0.0f)
  Vector2D &Zero();

  // Toán tử friend để in Vector2D ra luồng xuất (ví dụ std::cout) dưới định dạng "(x,y)"
  friend std::ostream &operator<<(std::ostream &stream, const Vector2D &vec);
};
