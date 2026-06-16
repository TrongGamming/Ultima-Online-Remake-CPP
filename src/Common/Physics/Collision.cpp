#include "Common/Physics/Collision.hpp"

// Định nghĩa thuật toán kiểm tra va chạm AABB giữa hai hình chữ nhật recA và recB
bool Collision::AABB(const SDL_Rect& recA, const SDL_Rect& recB) noexcept
{
    // Kiểm tra xem hai hình chữ nhật có chồng chéo lên nhau theo cả hai chiều ngang (X) và dọc (Y) hay không.
    // Nếu có chồng chéo trên cả hai trục thì chắc chắn hai hình chữ nhật va chạm nhau.
    return (
            // Trục X: Cạnh phải của recA phải nằm bên phải cạnh trái của recB
            recA.x + recA.w >= recB.x &&
            // Trục X: Cạnh phải của recB phải nằm bên phải cạnh trái của recA
            recB.x + recB.w >= recA.x &&
            // Trục Y: Cạnh dưới của recA phải nằm bên dưới cạnh trên của recB
            recA.y + recA.h >= recB.y &&
            // Trục Y: Cạnh dưới của recB phải nằm bên dưới cạnh trên của recA
            recB.y + recB.h >= recA.y
    );
}
