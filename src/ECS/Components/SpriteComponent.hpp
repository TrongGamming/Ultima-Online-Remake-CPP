#pragma once

#include <SDL3/SDL.h>

#include <map>
#include <string>

#include "Common/World/WorldConfig.hpp"
#include "Core/AssetManager.hpp"
#include "Core/Game.hpp"
#include "Core/TextureManager.hpp"
#include "ECS/Components/Animation.hpp"
#include "ECS/Components/TransformComponent.hpp"

// SpriteComponent chịu trách nhiệm quản lý hình ảnh hiển thị (Sprite),
// chạy hoạt ảnh (Animation) trên Sprite Sheet và tính toán tọa độ render
// Isometric/Cartesian để gửi cho hàm vẽ đồ họa của SDL.
class SpriteComponent : public Component {
 private:
  TransformComponent* transform{
      nullptr};  // Con trỏ liên kết tới Transform của thực thể để lấy vị trí vẽ
  SDL_Texture* texture{
      nullptr};  // Con trỏ tới tài nguyên SDL_Texture chứa hình ảnh của sprite
  SDL_Rect srcRect{};    // Vùng cắt hình chữ nhật trên Sprite Sheet (chọn khung
                         // hình hoạt ảnh cần vẽ)
  SDL_FRect destRect{};  // Vùng vẽ hình chữ nhật trên màn hình (tọa độ pixel
                         // thực tế và kích thước sau khi scale)

  bool animated = false;  // Cờ bật/tắt chạy hoạt ảnh
  int frames = 0;         // Số khung hình của hoạt ảnh hiện tại
  int speed = 100;        // Tốc độ chuyển khung hình hoạt ảnh (ms)

  // Cơ chế ưu tiên để các hoạt ảnh chiến đấu/tấn công được diễn ra trọn vẹn
  // không bị ghi đè
  std::string currentAnimName;  // Tên hoạt ảnh hiện tại
  uint32_t animStartTicks = 0;  // Thời điểm bắt đầu phát hoạt ảnh
  uint32_t animPlayDuration =
      0;                    // Tổng thời lượng diễn ra hoạt ảnh (frames * speed)
  bool isPriority = false;  // Cờ báo hoạt ảnh đang chạy có độ ưu tiên cao
                            // (không bị ngắt giữa chừng)

 public:
  int rowIndex = 0;  // Dòng hiện tại trên Sprite Sheet đang vẽ hoạt ảnh
  int colIndex = 0;  // Cột bắt đầu của hoạt ảnh hiện tại trên Sprite Sheet
  std::map<std::string, Animation>
      animations;  // Bản đồ ánh xạ tên hoạt ảnh (ví dụ "IdleW", "WalkS") sang
                   // cấu trúc Animation

  SDL_FlipMode spriteFlip =
      SDL_FLIP_NONE;  // Chế độ lật ảnh (không lật, lật ngang hoặc lật dọc)

  SpriteComponent() = default;

  // Constructor vẽ ảnh tĩnh (chỉ cần truyền ID texture trong AssetManager)
  SpriteComponent(const std::string& id) { setTex(id); }

  // Constructor vẽ ảnh động: Đăng ký các hoạt ảnh mặc định của nhân vật (đi bộ,
  // đứng yên, tấn công theo 4 hướng)
  SpriteComponent(const std::string& id, bool isAnimated) {
    animated = isAnimated;

    // Đăng ký các hoạt ảnh tương ứng với các dòng trên Sprite Sheet sói
    // (wolf-all.png)
    Animation idleW(12, 11, 4, 136);  // Đứng yên hướng Tây (dòng 12, cột bắt
                                      // đầu 11, 4 khung hình, tốc độ 136ms)
    Animation idleS(13, 11, 4,
                    136);  // Đứng yên hướng Nam (dòng 13, cột bắt đầu 11)
    Animation idleN(14, 11, 4,
                    136);  // Đứng yên hướng Bắc (dòng 14, cột bắt đầu 11)
    Animation idleE(15, 11, 4,
                    136);  // Đứng yên hướng Đông (dòng 15, cột bắt đầu 11)
    Animation walkW(
        12, 0, 8,
        100);  // Đi bộ hướng Tây (dòng 12, cột bắt đầu 0, 8 khung hình)
    Animation walkS(13, 0, 8, 100);  // Đi bộ hướng Nam (dòng 13, cột bắt đầu 0)
    Animation walkN(14, 0, 8, 100);  // Đi bộ hướng Bắc (dòng 14, cột bắt đầu 0)
    Animation walkE(15, 0, 8,
                    100);  // Đi bộ hướng Đông (dòng 15, cột bắt đầu 0)

    // Đăng ký các hoạt ảnh tấn công (Attack) mới ở cột 8-10 (3 khung hình, tốc
    // độ 100ms)
    Animation attackW(0, 9, 5, 100);
    Animation attackS(1, 9, 5, 100);
    Animation attackN(2, 9, 5, 100);
    Animation attackE(3, 9, 5, 100);

    animations.emplace("IdleW", idleW);
    animations.emplace("IdleN", idleN);
    animations.emplace("IdleE", idleE);
    animations.emplace("IdleS", idleS);
    animations.emplace("WalkW", walkW);
    animations.emplace("WalkS", walkS);
    animations.emplace("WalkN", walkN);
    animations.emplace("WalkE", walkE);
    animations.emplace("AttackW", attackW);
    animations.emplace("AttackS", attackS);
    animations.emplace("AttackN", attackN);
    animations.emplace("AttackE", attackE);

    Play("IdleW");  // Mặc định chạy hoạt ảnh đứng yên hướng Tây
    setTex(id);
  }

  // Tải texture từ AssetManager
  void setTex(const std::string& id) { texture = Game::assets->GetTexture(id); }

  // Khởi tạo thành phần: lấy Transform và đặt kích thước khung cắt ban đầu bằng
  // kích thước nhân vật
  void init() override {
    transform = &entity->getComponent<TransformComponent>();

    srcRect.x = srcRect.y = 0;
    srcRect.w = transform->width;
    srcRect.h = transform->height;
  }

  // Chuyển sang phát hoạt ảnh có tên cụ thể
  // priority: Đặt bằng true để đánh dấu hoạt ảnh quan trọng (như Attack) không
  // được ghi đè bởi Walk/Idle
  void Play(const std::string& animName, bool priority = false) {
    // Nếu đang chạy hoạt ảnh ưu tiên, bỏ qua tất cả yêu cầu phát hoạt ảnh thông
    // thường
    if (isPriority && !priority) {
      uint32_t elapsed = SDL_GetTicks() - animStartTicks;
      if (elapsed < animPlayDuration) {
        return;  // Chờ cho hoạt ảnh ưu tiên chạy xong
      } else {
        isPriority = false;  // Kết thúc thời gian ưu tiên
      }
    }

    // Nếu có sự thay đổi hoạt ảnh hoặc bắt buộc phát hoạt ảnh ưu tiên
    if (currentAnimName != animName || priority) {
      currentAnimName = animName;
      frames = animations[animName].frames;
      rowIndex = animations[animName].row;
      colIndex = animations[animName].col;
      speed = animations[animName].speed;

      if (priority) {
        isPriority = true;
        animStartTicks = SDL_GetTicks();
        animPlayDuration =
            frames * speed;  // Tính tổng thời gian hoạt ảnh chạy hết 1 lượt
      }
    }
  }

  // Cập nhật khung hình hoạt ảnh và tọa độ hiển thị vẽ trên màn hình
  void update() override {
    // 1. Tính toán khung hình hiện tại
    if (animated) {
      int currentFrame = 0;
      if (isPriority) {
        // Hoạt ảnh ưu tiên: Tính khung hình tuần tự bắt đầu từ 0 tại thời điểm
        // phát
        uint32_t elapsed = SDL_GetTicks() - animStartTicks;
        currentFrame = static_cast<int>(elapsed / speed);
        if (currentFrame >= frames) {
          currentFrame = frames - 1;  // Giữ ở frame cuối cùng khi kết thúc
          isPriority = false;         // Giải phóng cờ ưu tiên
        }
      } else {
        // Hoạt ảnh lặp tuần hoàn (Idle, Walk): Tính theo thời gian hệ thống
        currentFrame = static_cast<int>((SDL_GetTicks() / speed) % frames);
      }

      // Chọn đúng cột cắt trên tilesheet
      srcRect.x = (srcRect.w * currentFrame) + (srcRect.w * colIndex);
    }

    // Xác định dòng cắt (srcRect.y)
    srcRect.y = rowIndex * transform->height;

    // 2. Tính toán vị trí vẽ destRect (đã quy đổi theo hệ Isometric/Cartesian
    // và trừ đi camera)
    if (Game::isIsometric) {
      // Công thức chuyển đổi Cartesian sang Isometric
      float isoX = (transform->position.x - transform->position.y) * 0.5f;
      float isoY = (transform->position.x + transform->position.y) * 0.25f -
                   transform->position.z;

      destRect.x = isoX - Game::camera.x;
      // Trừ đi g_WorldConfig.blockHeight để đưa chân nhân vật khớp với bề mặt
      // khối gạch địa hình
      destRect.y = isoY - Game::camera.y - g_WorldConfig.blockHeight;
    } else {
      // Hệ Cartesian thường
      destRect.x = transform->position.x - Game::camera.x;
      destRect.y =
          transform->position.y - Game::camera.y - transform->position.z;
    }

    // Kích thước hiển thị thực tế sau khi áp dụng tỉ lệ scale (scale mặc định
    // là 2)
    destRect.w = static_cast<float>(transform->width * transform->scale);
    destRect.h = static_cast<float>(transform->height * transform->scale);
  }

  // Gọi hàm vẽ Sprite của TextureManager
  void draw() override {
    TextureManager::Draw(texture, srcRect, destRect, spriteFlip);
  }
};
