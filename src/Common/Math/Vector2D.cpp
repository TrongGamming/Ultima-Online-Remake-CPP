#include "Common/Math/Vector2D.hpp"

// Định nghĩa constructor khởi tạo Vector2D với hai giá trị số thực x và y
Vector2D::Vector2D(float x, float y)
{
    // Gán tham số x được truyền vào cho thuộc tính x của đối tượng
    this->x = x;
    // Gán tham số y được truyền vào cho thuộc tính y của đối tượng
    this->y = y;
}

// Phương thức cộng: cộng thành phần x và y của Vector truyền vào với Vector hiện tại
Vector2D& Vector2D::Add(const Vector2D& vec)
{
    // Cộng x của Vector hiện tại với x của Vector vec
    this->x += vec.x;
    // Cộng y của Vector hiện tại với y của Vector vec
    this->y += vec.y;
    // Trả về tham chiếu đến chính đối tượng hiện tại để cho phép gọi chuỗi phương thức (chaining)
    return *this;
}

// Phương thức trừ: trừ thành phần x và y của Vector hiện tại đi Vector truyền vào
Vector2D& Vector2D::Subtract(const Vector2D& vec)
{
    // Trừ x của Vector hiện tại đi x của Vector vec
    this->x -= vec.x;
    // Trừ y của Vector hiện tại đi y của Vector vec
    this->y -= vec.y;
    // Trả về tham chiếu đến chính đối tượng hiện tại
    return *this;
}

// Phương thức nhân: nhân thành phần x và y của Vector hiện tại với Vector truyền vào
Vector2D& Vector2D::Multiply(const Vector2D& vec)
{
    // Nhân x của Vector hiện tại với x của Vector vec
    this->x *= vec.x;
    // Nhân y của Vector hiện tại với y của Vector vec
    this->y *= vec.y;
    // Trả về tham chiếu đến chính đối tượng hiện tại
    return *this;
}

// Phương thức chia: chia thành phần x và y của Vector hiện tại cho Vector truyền vào
Vector2D& Vector2D::Divide(const Vector2D& vec)
{
    // Chia x của Vector hiện tại cho x của Vector vec
    this->x /= vec.x;
    // Chia y của Vector hiện tại cho y của Vector vec
    this->y /= vec.y;
    // Trả về tham chiếu đến chính đối tượng hiện tại
    return *this;
}

// Định nghĩa toán tử cộng (+): tạo bản sao v1, gọi Add(v2) và trả về Vector mới kết quả
Vector2D operator+(Vector2D v1, const Vector2D& v2)
{
    return v1.Add(v2);
}

// Định nghĩa toán tử trừ (-): tạo bản sao v1, gọi Subtract(v2) và trả về Vector mới kết quả
Vector2D operator-(Vector2D v1, const Vector2D& v2)
{
    return v1.Subtract(v2);
}

// Định nghĩa toán tử nhân (*): nhân hai Vector2D với nhau
Vector2D operator*(Vector2D v1, const Vector2D& v2)
{
    return v1.Multiply(v2);
}

// Định nghĩa toán tử chia (/): chia Vector2D thứ nhất cho Vector2D thứ hai
Vector2D operator/(Vector2D v1, const Vector2D& v2)
{
    return v1.Divide(v2);
}

// Định nghĩa toán tử cộng gán (+=): gọi phương thức Add và thay đổi đối tượng hiện tại
Vector2D& Vector2D::operator+=(const Vector2D& vec)
{
    return this->Add(vec);
}

// Định nghĩa toán tử trừ gán (-=): gọi phương thức Subtract
Vector2D& Vector2D::operator-=(const Vector2D& vec)
{
    return this->Subtract(vec);
}

// Định nghĩa toán tử nhân gán (*=): gọi phương thức Multiply
Vector2D& Vector2D::operator*=(const Vector2D& vec)
{
    return this->Multiply(vec);
}

// Định nghĩa toán tử chia gán (/=): gọi phương thức Divide
Vector2D& Vector2D::operator/=(const Vector2D& vec)
{
    return this->Divide(vec);
}

// Định nghĩa toán tử nhân với một số vô hướng: nhân cả x và y với số đó, trả về một đối tượng mới
Vector2D Vector2D::operator*(float scalar) const
{
    return Vector2D(this->x * scalar, this->y * scalar);
}

// Phương thức reset Vector về (0.0f, 0.0f)
Vector2D& Vector2D::Zero()
{
    // Đặt x về 0
    this->x = 0.0f;
    // Đặt y về 0
    this->y = 0.0f;
    // Trả về tham chiếu tới đối tượng
    return *this;
}

// Định nghĩa toán tử chèn luồng (<<): dùng để in tọa độ Vector dạng "(x,y)" thông qua stream
std::ostream& operator<<(std::ostream& stream, const Vector2D& vec)
{
    stream << "(" << vec.x << "," << vec.y << ")";
    return stream;
}
