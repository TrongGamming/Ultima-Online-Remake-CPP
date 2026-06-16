#pragma once
#include <iostream>

// Lớp Vector3D đại diện cho một Vector toán học 3 chiều (x, y, z)
// Dùng để biểu diễn tọa độ 3D, vận tốc 3D, hoặc định hướng trong game có trục Z biểu thị độ cao.
class Vector3D {
 public:
  // Các thành phần tọa độ của Vector
  float x; // Thành phần trục X (phương ngang)
  float y; // Thành phần trục Y (phương dọc)
  float z; // Thành phần trục Z (độ cao / chiều sâu)

  // Constructor mặc định (các giá trị x, y, z chưa được khởi tạo tự động, 
  // nên cần gán cẩn thận hoặc dùng constructor 3 tham số)
  Vector3D() = default;

  // Constructor khởi tạo Vector3D với các giá trị x, y, z được truyền vào
  Vector3D(float x, float y, float z) : x(x), y(y), z(z) {}

  // Phương thức cộng: Cộng thêm các thành phần của vector vec vào vector hiện tại
  Vector3D& Add(const Vector3D& vec);
  
  // Phương thức trừ: Trừ các thành phần của vector hiện tại đi các thành phần của vector vec
  Vector3D& Subtract(const Vector3D& vec);
  
  // Phương thức nhân: Nhân từng thành phần của vector hiện tại với vector vec
  Vector3D& Multiply(const Vector3D& vec);
  
  // Phương thức chia: Chia từng thành phần của vector hiện tại cho vector vec
  Vector3D& Divide(const Vector3D& vec);

  // Toán tử friend cho phép cộng, trừ, nhân, chia hai đối tượng Vector3D
  // Trả về một đối tượng Vector3D bản sao mới chứa kết quả
  friend Vector3D operator+(Vector3D v1, const Vector3D& v2);
  friend Vector3D operator-(Vector3D v1, const Vector3D& v2);
  friend Vector3D operator*(Vector3D v1, const Vector3D& v2);
  friend Vector3D operator/(Vector3D v1, const Vector3D& v2);

  // Toán tử gán tích hợp (+=, -=, *=, /=) giúp thay đổi trực tiếp Vector hiện tại
  Vector3D& operator+=(const Vector3D& vec);
  Vector3D& operator-=(const Vector3D& vec);
  Vector3D& operator*=(const Vector3D& vec);
  Vector3D& operator/=(const Vector3D& vec);

  // Toán tử nhân Vector3D với số vô hướng (scalar)
  Vector3D operator*(float scalar) const;
  
  // Reset các thành phần Vector3D về 0.0f
  Vector3D& Zero();

  // Toán tử friend để ghi Vector3D vào luồng xuất (std::cout) dạng "(x,y,z)"
  friend std::ostream& operator<<(std::ostream& stream, const Vector3D& vec);
};