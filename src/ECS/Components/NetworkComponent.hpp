#pragma once
#include <cstdint>
#include <string>

#include "ECS/ECS.hpp"

// NetworkComponent lưu trữ thông tin trạng thái mạng của một thực thể (đồng bộ
// hóa từ Server). Nó chứa thông tin như ID thực thể, HP, hướng quay mặt, chế độ
// chiến đấu, notoriety, và các tham số nội suy (interpolation) vị trí di chuyển
// mượt mà (start/target coordinates).
class NetworkComponent : public Component {
 public:
  uint32_t serverId =
      0;             // ID độc nhất của thực thể được cấp phát bởi game server
  std::string name;  // Tên hiển thị (username hoặc tên quái vật)
  int32_t hp = 100;  // Lượng máu hiện tại
  int32_t maxHp = 100;  // Lượng máu tối đa
  uint8_t state =
      0;  // Trạng thái: 0: Idle, 1: Walk, 2: Attack/WarMode, 3: Ghost (hồn ma)

  // Mức độ nguy hiểm / tai tiếng (Notoriety):
  // 0: Blue (Innocent - Lành tính), 1: Gray (Criminal - Tội phạm), 2: Red
  // (Murderer - Sát nhân)
  uint8_t notoriety = 0;
  uint8_t direction =
      4;  // Hướng quay mặt hiện tại (0-7 từ Bắc, Đông Bắc, đến Tây Bắc)

  // Vị trí ĐÍCH (Target) mà thực thể đang di chuyển tới, nhận từ gói tin Server
  // cập nhật mới nhất
  float targetX = 0.0f;
  float targetY = 0.0f;
  float targetZ = 0.0f;  // Tọa độ đích trục Z (độ cao địa hình)

  // Vị trí BẮT ĐẦU (Start) của thực thể trước khi thực hiện nội suy bước di
  // chuyển mới
  float startX = 0.0f;
  float startY = 0.0f;
  float startZ = 0.0f;  // Tọa độ bắt đầu trục Z

  uint32_t lastUpdateTicks =
      0;  // Thời điểm (mili-giây) nhận được gói tin cập nhật cuối cùng để tính
          // nội suy theo thời gian

  // Bong bóng chat hiển thị trên đầu thực thể
  uint32_t lastChatTime = 0;  // Thời điểm người chơi nói câu chat cuối cùng
                              // (dùng để đếm ngược ẩn bong bóng sau 5s)
  std::string chatText;  // Nội dung câu chat hiện tại

  // Constructor khởi tạo đầy đủ các thông số của thực thể mạng nhận từ Server
  NetworkComponent(uint32_t sId, const std::string& n, int32_t h, int32_t mH,
                   uint8_t st, uint8_t noto, uint8_t dir = 4)
      : serverId(sId),
        name(n),
        hp(h),
        maxHp(mH),
        state(st),
        notoriety(noto),
        direction(dir) {
    targetX = 0.0f;
    targetY = 0.0f;
    targetZ = 0.0f;
    startX = 0.0f;
    startY = 0.0f;
    startZ = 0.0f;
    lastUpdateTicks = 0;
  }

  ~NetworkComponent() override = default;
};