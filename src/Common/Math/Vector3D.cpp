#include "./Vector3D.hpp"

// Định nghĩa phương thức cộng: cộng thành phần x, y, z của Vector v vào Vector hiện tại
Vector3D& Vector3D::Add(const Vector3D& v) {
  this->x += v.x; // Cộng x hiện tại với x của v
  this->y += v.y; // Cộng y hiện tại với y của v
  this->z += v.z; // Cộng z hiện tại với z của v
  return *this;   // Trả về tham chiếu đối tượng hiện tại
}

// Định nghĩa phương thức trừ: trừ thành phần x, y, z của Vector hiện tại đi Vector v
Vector3D& Vector3D::Subtract(const Vector3D& v) {
  this->x -= v.x; // Trừ x hiện tại đi x của v
  this->y -= v.y; // Trừ y hiện tại đi y của v
  this->z -= v.z; // Trừ z hiện tại đi z của v
  return *this;   // Trả về tham chiếu đối tượng hiện tại
}

// Định nghĩa phương thức nhân: nhân từng thành phần x, y, z của Vector hiện tại với Vector v
Vector3D& Vector3D::Multiply(const Vector3D& v) {
  this->x *= v.x; // Nhân x hiện tại với x của v
  this->y *= v.y; // Nhân y hiện tại với y của v
  this->z *= v.z; // Nhân z hiện tại với z của v
  return *this;   // Trả về tham chiếu đối tượng hiện tại
}

// Định nghĩa phương thức chia: chia từng thành phần x, y, z của Vector hiện tại cho Vector v
Vector3D& Vector3D::Divide(const Vector3D& v) {
  this->x /= v.x; // Chia x hiện tại cho x của v
  this->y /= v.y; // Chia y hiện tại cho y của v
  this->z /= v.z; // Chia z hiện tại cho z của v
  return *this;   // Trả về tham chiếu đối tượng hiện tại
}

// Định nghĩa các toán tử số học toán hạng 3D
Vector3D operator*(Vector3D v1, const Vector3D& v2) { return v1.Multiply(v2); } // Toán tử nhân hai Vector3D
Vector3D operator-(Vector3D v1, const Vector3D& v2) { return v1.Subtract(v2); } // Toán tử trừ hai Vector3D
Vector3D operator/(Vector3D v1, const Vector3D& v2) { return v1.Divide(v2); } // Toán tử chia hai Vector3D
Vector3D operator+(Vector3D v1, const Vector3D& v2) { return v1.Add(v2); }    // Toán tử cộng hai Vector3D

// Định nghĩa toán tử gán tích hợp
Vector3D& Vector3D::operator+=(const Vector3D& v) { return this->Add(v); }
Vector3D& Vector3D::operator-=(const Vector3D& v) { return this->Subtract(v); }
Vector3D& Vector3D::operator*=(const Vector3D& v) { return this->Multiply(v); }
Vector3D& Vector3D::operator/=(const Vector3D& v) { return this->Divide(v); }

// Phương thức reset Vector3D về (0, 0, 0)
Vector3D& Vector3D::Zero() {
  this->x = 0; // Đặt x về 0
  this->y = 0; // Đặt y về 0
  this->z = 0; // Đặt z về 0
  return *this; // Trả về tham chiếu đối tượng
}

// Định nghĩa toán tử nhân Vector3D với một số vô hướng số thực
Vector3D Vector3D::operator*(float scalar) const {
  // Trả về một đối tượng Vector3D mới với các thành phần được nhân với scalar
  return Vector3D(this->x * scalar, this->y * scalar, this->z * scalar);
}

// Định nghĩa toán tử chèn luồng (<<): in thông tin Vector3D dưới định dạng "(x,y,z)" thông qua stream
std::ostream& operator<<(std::ostream& stream, const Vector3D& vec) {
  stream << "(" << vec.x << "," << vec.y << "," << vec.z << ")";
  return stream;
}