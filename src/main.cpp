#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <memory>

#include "Core/Game.hpp"

// Điểm khởi chạy chương trình Client (Hàm main)
int main(int argc, char* argv[]) {
  // Số khung hình hiển thị trên giây mục tiêu (60 FPS)
  const int FPS = 60;

  // Khoảng thời gian trễ lý thuyết giữa các khung hình tính bằng mili-giây
  // (1000ms / 60 ~ 16.67ms)
  const int frameDelay = 1000 / FPS;

  Uint64 frameStart;  // Thời điểm bắt đầu của một frame
  Uint64 frameTime;   // Thời gian thực tế tiêu tốn để xử lý xong một frame

  // Sử dụng unique_ptr để quản lý vòng đời đối tượng Game Client tự động giải
  // phóng khi kết thúc hàm
  auto game = std::make_unique<Game>();

  // Khởi tạo Game Client với tiêu đề, độ phân giải 800x640 và không bật chế độ
  // toàn màn hình
  game->init("OU Game Engine (C++20 & SDL3)", 800, 640, false);

  // Vòng lặp game chính (Game Loop) chạy liên tục cho đến khi game->running()
  // trả về false
  while (game->running()) {
    // 1. Ghi lại thời điểm bắt đầu frame
    frameStart = SDL_GetTicks();

    // 2. Xử lý các sự kiện bàn phím/chuột từ hệ điều hành
    game->handleEvents();

    // 3. Cập nhật logic game (nhận mạng, di chuyển, nội suy, camera...)
    game->update();

    // 4. Vẽ đồ họa game lên màn hình
    game->render();

    // 5. Tính toán thời gian thực tế đã dùng trong frame này
    frameTime = SDL_GetTicks() - frameStart;

    // 6. Cơ chế giới hạn FPS (FPS Limiter):
    // Nếu thời gian xử lý nhanh hơn 16.67ms, bắt luồng hiện tại ngủ (delay)
    // một khoảng bù trừ để duy trì tốc độ game ổn định ở mức 60 FPS.
    if (frameDelay > frameTime) {
      SDL_Delay(static_cast<Uint32>(frameDelay - frameTime));
    }
  }

  // Giải phóng tài nguyên, đóng socket và dọn dẹp các subsystem của SDL trước
  // khi tắt ứng dụng
  game->clean();
  return 0;
}
