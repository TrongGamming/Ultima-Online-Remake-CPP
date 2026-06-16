#pragma once

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_net/SDL_net.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>

#include "Common/Network/NetworkBuffer.hpp"
#include "Core/AssetManager.hpp"
#include "Core/Camera.hpp"

// Forward declaration của lớp Entity từ ECS
class Entity;

// Lớp Game là lớp lõi (Core Class) của Game Client.
// Quản lý vòng đời trò chơi, khởi tạo SDL, bắt sự kiện bàn phím/chuột, cập nhật vòng lặp logic,
// xử lý vẽ đồ họa (render) và giao tiếp mạng TCP với Server thông qua SDL3_net.
class Game {
 public:
  Game();
  ~Game();

  // Khởi tạo các hệ thống con của SDL, tạo cửa sổ, renderer, các nhãn UI và gọi kết nối Server
  // title: Tiêu đề cửa sổ
  // width, height: Độ phân giải cửa sổ game (ví dụ: 800x640)
  // fullscreen: true để bật chế độ toàn màn hình
  void init(const char* title, int width, int height, bool fullscreen);

  // Nhận và phân phối các sự kiện SDL (bàn phím, cuộn chuột zoom, click chuột chọn mục tiêu, kéo thả camera)
  void handleEvents();
  
  // Cập nhật logic game mỗi frame: nhận dữ liệu mạng, nội suy vị trí thực thể, cập nhật camera bám theo player, cập nhật HUD
  void update();
  
  // Kiểm tra xem vòng lặp game có đang chạy hay không
  [[nodiscard]] bool running() const noexcept { return isRunning; }
  
  // Xóa màn hình và vẽ toàn bộ ô gạch, người chơi, quái vật, tên thực thể và bong bóng chat
  void render();
  
  // Giải phóng tài nguyên, đóng socket mạng và tắt các subsystem của SDL khi thoát game
  void clean();

  // Các biến tĩnh toàn cục cho phép các Component truy cập dễ dàng
  static SDL_Renderer* renderer; // Con trỏ renderer vẽ đồ họa của SDL
  static SDL_Event event;        // Cấu trúc chứa sự kiện SDL hiện tại
  static bool isRunning;         // Trạng thái vòng lặp game
  static bool isIsometric;       // Chế độ camera (true: Isometric góc nghiêng, false: Cartesian phẳng)
  static Camera camera;          // Đối tượng camera quản lý góc nhìn và zoom
  static AssetManager* assets;   // Quản lý tài nguyên hình ảnh và font chữ

  // Định nghĩa các nhóm group phân loại thực thể để quản lý vẽ đè lên nhau theo thứ tự (z-ordering)
  enum groupLabels : std::size_t {
    groupMap,        // Nhóm 0: Ô gạch địa hình nền bản đồ (vẽ dưới cùng)
    groupPlayers,    // Nhóm 1: Người chơi và quái vật (NPCs)
    groupColliders,  // Nhóm 2: Hộp va chạm debug
    groupProjectiles // Nhóm 3: Đạn bay (vẽ trên cùng)
  };

  // Các phương thức mạng của Client (Client Networking)
  void connectToServer();                         // Thực hiện phân giải địa chỉ và kết nối đến Server port 5050
  void handleNetwork();                           // Định kỳ đọc luồng byte từ socket TCP đưa vào netBuffer và bóc tách gói
  void sendMoveRequest(uint8_t direction);        // Gửi gói tin yêu cầu di chuyển theo hướng (0-7)
  void sendChat(const std::string& text);          // Gửi tin nhắn chat dạng văn bản
  void sendAction(uint8_t actionType, uint32_t targetId); // Gửi gói tin hành động (War Mode, chọn mục tiêu, hồi sinh)

 private:
  // Xử lý gói tin sau khi bóc tách hoàn chỉnh dựa trên mã ID gói tin và mảng dữ liệu byte thô
  void handlePacket(uint16_t id, const std::vector<uint8_t>& data);

  int cnt = 0;
  SDL_Window* window = nullptr; // Con trỏ quản lý cửa sổ hiển thị của SDL

  // Các biến phục vụ kết nối mạng
  NET_StreamSocket* clientSocket = nullptr; // Socket TCP kết nối tới Server
  NetworkBuffer netBuffer;                  // Bộ đệm tích lũy dữ liệu mạng thô của Client
  uint32_t myEntityId = 0;                  // ID thực thể của chính người chơi được Server cấp
  bool isConnected = false;                 // Cờ báo trạng thái kết nối thành công tới Server

  // Bản đồ lưu trữ các thực thể được đồng bộ từ Server (ID thực thể -> Con trỏ Entity của ECS)
  std::unordered_map<uint32_t, Entity*> serverEntities;

  // Giao diện người dùng (HUD UI)
  Entity* hudLabel = nullptr;       // Nhãn chữ HUD dưới cùng (thông tin HP, vị trí, chế độ WarMode)
  Entity* targetHudLabel = nullptr; // Nhãn chữ HUD trên cùng hiển thị thông tin mục tiêu (Target) đang chọn
  std::string chatInputBuffer;     // Bộ đệm chứa các ký tự tin nhắn đang gõ
  bool typingChat = false;          // Cờ báo người chơi có đang mở hộp gõ chat (nhấn Enter) hay không
  bool warMode = false;             // Chế độ chiến đấu (true: đang ở WAR mode sẵn sàng đánh quái)
  uint32_t currentTargetId = 0;     // ID thực thể của mục tiêu đang được nhấp chọn tấn công
  
  // Các biến điều khiển camera bằng chuột phải
  uint32_t lastPosCamX = 0;              // Vị trí chuột X cuối cùng khi nhấn giữ chuột phải để kéo map
  uint32_t lastPosCamY = 0;              // Vị trí chuột Y cuối cùng
  bool cameraFlow = true;                // Cờ cho phép camera tự động trôi bám theo nhân vật
  uint32_t lastRightMouseReleaseTime = 0; // Thời điểm thả chuột phải (để chờ 1 giây trước khi tự động kéo camera về player)
  bool isResettingCamera = false;        // Cờ báo đang trong thời gian đệm chuẩn bị khôi phục camera flow

  // Các biến phục vụ phím ảo (Virtual Controls)
  bool showVirtualControls = true; // Mặc định hiển thị phím ảo
  
  // Vị trí và thông số Joystick ảo
  float joystickCenterX = 100.0f;
  float joystickCenterY = 500.0f;
  float joystickKnobX = 100.0f;
  float joystickKnobY = 500.0f;
  float joystickOuterRadius = 60.0f;
  float joystickInnerRadius = 25.0f;
  SDL_FingerID joystickFingerId = -1; // ID của ngón tay đang kéo Joystick (-1 là không có)
  int lastMoveDir = -1;               // Hướng di chuyển gửi gần nhất (-1 là đứng im)
  uint32_t lastMoveSendTime = 0;      // Mốc thời gian gửi di chuyển gần nhất
  
  // Vị trí các nút bấm chức năng
  float btnAttackX = 700.0f;
  float btnAttackY = 500.0f;
  float btnAttackRadius = 35.0f;
  
  float btnWarX = 700.0f;
  float btnWarY = 400.0f;
  float btnWarRadius = 25.0f;
  
  float btnResX = 620.0f;
  float btnResY = 450.0f;
  float btnResRadius = 25.0f;
  
  float btnCamX = 620.0f;
  float btnCamY = 550.0f;
  float btnCamRadius = 25.0f;

  // Vị trí các nút Zoom ảo trên di động
  float btnZoomInX = 0.0f;
  float btnZoomInY = 0.0f;
  float btnZoomInRadius = 20.0f;

  float btnZoomOutX = 0.0f;
  float btnZoomOutY = 0.0f;
  float btnZoomOutRadius = 20.0f;

  // Các hàm hỗ trợ vẽ hình học cơ bản của SDL3
  void drawFilledCircle(SDL_Renderer* renderer, float centerX, float centerY, float radius, SDL_Color color);
  void drawOutlineCircle(SDL_Renderer* renderer, float centerX, float centerY, float radius, SDL_Color color);
  void updateVirtualControlsLayout(int winW, int winH);
};
