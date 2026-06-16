#pragma once

#include "Core/Game.hpp"
#include "ECS/Components/SpriteComponent.hpp"
#include "ECS/Components/TransformComponent.hpp"
#include "ECS/ECS.hpp"

// KeyboardController xử lý các sự kiện nhấn/thả bàn phím trực tiếp bằng SDL_Event.
// Thường được sử dụng trong chế độ chơi đơn (Singleplayer) hoặc để kiểm tra vận tốc vật lý (velocity)
// của thực thể cục bộ trước khi chuyển sang đồng bộ qua mạng của máy chủ.
class KeyboardController : public Component {
 public:
  TransformComponent *transform{nullptr}; // Con trỏ tới Transform để cập nhật hướng vận tốc di chuyển
  SpriteComponent *sprite{nullptr};       // Con trỏ tới Sprite để cập nhật hướng nhìn và chạy hoạt ảnh tương ứng

  // Khởi tạo thành phần: lấy liên kết tới Transform và Sprite của thực thể
  void init() override {
    transform = &entity->getComponent<TransformComponent>();
    sprite = &entity->getComponent<SpriteComponent>();
  }

  // Cập nhật logic di chuyển dựa trên sự kiện bàn phím mỗi frame
  void update() override {
    // 1. Xử lý sự kiện nhấn phím xuống (SDL_EVENT_KEY_DOWN)
    if (Game::event.type == SDL_EVENT_KEY_DOWN) {
      switch (Game::event.key.key) {
      case SDLK_W:
        transform->velocity.y = -1; // Di chuyển lên trên (trục Y giảm)
        sprite->Play("Walk");       // Phát hoạt ảnh đi bộ
        break;
      case SDLK_A:
        transform->velocity.x = -1; // Di chuyển sang trái (trục X giảm)
        sprite->Play("Walk");       // Phát hoạt ảnh đi bộ
        sprite->spriteFlip = SDL_FLIP_HORIZONTAL; // Lật ngược Sprite sang trái
        break;
      case SDLK_D:
        transform->velocity.x = 1;  // Di chuyển sang phải (trục X tăng)
        sprite->Play("Walk");       // Phát hoạt ảnh đi bộ
        break;
      case SDLK_S:
        transform->velocity.y = 1;  // Di chuyển xuống dưới (trục Y tăng)
        sprite->Play("Walk");       // Phát hoạt ảnh đi bộ
        break;
      default:
        break;
      }
    }

    // 2. Xử lý sự kiện thả phím ra (SDL_EVENT_KEY_UP)
    if (Game::event.type == SDL_EVENT_KEY_UP) {
      switch (Game::event.key.key) {
      case SDLK_W:
        transform->velocity.y = 0;  // Dừng di chuyển dọc
        sprite->Play("Idle");       // Phát hoạt ảnh đứng yên
        break;
      case SDLK_A:
        transform->velocity.x = 0;  // Dừng di chuyển ngang
        sprite->Play("Idle");       // Phát hoạt ảnh đứng yên
        sprite->spriteFlip = SDL_FLIP_NONE; // Tắt lật ảnh (trở lại hướng mặc định)
        break;
      case SDLK_D:
        transform->velocity.x = 0;  // Dừng di chuyển ngang
        sprite->Play("Idle");       // Phát hoạt ảnh đứng yên
        break;
      case SDLK_S:
        transform->velocity.y = 0;  // Dừng di chuyển dọc
        sprite->Play("Idle");       // Phát hoạt ảnh đứng yên
        break;
      case SDLK_ESCAPE:
        Game::isRunning = false;    // Nhấn ESC để thoát game nhanh
        break;
      default:
        break;
      }
    }
  }
};
