#pragma once
#include <SDL3/SDL.h>

// Lớp Collision quản lý việc tính toán va chạm vật lý cơ bản trong game.
class Collision
{
public:
    // Thuật toán kiểm tra va chạm AABB (Axis-Aligned Bounding Box) giữa hai hình chữ nhật recA và recB.
    // Trả về true nếu hai hình chữ nhật giao nhau (va chạm), ngược lại trả về false.
    // Từ khóa [[nodiscard]] cảnh báo trình biên dịch nếu kết quả trả về của hàm bị bỏ qua.
    // noexcept tối ưu hóa hiệu năng bằng cách thông báo hàm này không ném ra ngoại lệ.
    [[nodiscard]] static bool AABB(const SDL_Rect& recA, const SDL_Rect& recB) noexcept;
};
