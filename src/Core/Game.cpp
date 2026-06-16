#include "Core/Game.hpp"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>

#include "Common/Network/Packets.hpp"
#include "Common/World/WorldConfig.hpp"
#include "Core/AssetManager.hpp"
#include "ECS/Components/ColliderComponent.hpp"
#include "ECS/Components/NetworkComponent.hpp"
#include "ECS/Components/SpriteComponent.hpp"
#include "ECS/Components/TransformComponent.hpp"
#include "ECS/Components/UILabel.hpp"
#include "Game.hpp"
#include "World/Map.hpp"

// Khai báo đối tượng bản đồ và ECS manager toàn cục cho Client
Map* map = nullptr;
Manager manager;

// Định nghĩa các biến tĩnh của lớp Game để chia sẻ tài nguyên trong ECS
SDL_Renderer* Game::renderer = nullptr;
SDL_Event Game::event;
// Khởi tạo camera mặc định tại vị trí (0,0), độ phân giải 800x640, zoom=1.0f
Camera Game::camera(0, 0, 800, 640, 1.0f, 0.1f, 0.5f, 2.0f);
AssetManager* Game::assets = nullptr;
bool Game::isRunning = false;
bool Game::isIsometric = true;  // Mặc định bật góc nhìn Isometric nghiêng 2.5D

// Constructor khởi tạo hạt giống ngẫu nhiên (random seed)
Game::Game() { srand(time(nullptr)); }

// Destructor gọi hàm clean giải phóng tài nguyên
Game::~Game() { clean(); }

// Hàm khởi tạo Client: Cấu hình cửa sổ, Renderer, kết nối mạng và tải Asset ban
// đầu
void Game::init(const char* title, int width, int height, bool fullscreen) {
  // Tắt giả lập sự kiện chuột từ chạm cảm ứng trên di động để tránh
  // double-trigger
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

  SDL_WindowFlags flags = 0;
  if (fullscreen) {
    flags = SDL_WINDOW_FULLSCREEN;  // Cấu hình toàn màn hình nếu được yêu cầu
  }

  // 1. Khởi tạo hệ thống video của SDL
  if (SDL_Init(SDL_INIT_VIDEO)) {
    std::cout << "Client: Subsystems Initialised!..." << std::endl;

    // Khởi tạo thư viện SDL_net cho kết nối socket TCP
    if (!NET_Init()) {
      std::cerr << "Client: NET_Init failed: " << SDL_GetError() << std::endl;
    }

    // Tạo cửa sổ game SDL
    window = SDL_CreateWindow(title, width, height, flags);
    if (window) {
      std::cout << "Client: Window created!" << std::endl;

      int realW = width;
      int realH = height;
      SDL_GetWindowSize(window, &realW, &realH);
      std::cout << "Client: Real Window Size: " << realW << "x" << realH
                << std::endl;

      // Đồng bộ kích thước cửa sổ game thực tế với hệ thống Camera
      camera.initW = realW;
      camera.initH = realH;
      camera.w = realW;
      camera.h = realH;
      camera.exactX = static_cast<float>(camera.x);
      camera.exactY = static_cast<float>(camera.y);

      // Cập nhật vị trí các phím ảo ban đầu dựa trên kích thước Window thực tế
      updateVirtualControlsLayout(realW, realH);
    }

    // Tạo renderer để vẽ đồ họa được tăng tốc phần cứng GPU
    renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer) {
      SDL_SetRenderDrawColor(renderer, 255, 255, 255,
                             255);  // Màu nền trắng mặc định
      std::cout << "Client: Renderer created!" << std::endl;
    }

    isRunning = true;  // Đánh dấu vòng lặp game chính thức chạy
  }

  // 2. Khởi tạo thư viện SDL_ttf để vẽ chữ font vector
  if (!TTF_Init()) {
    std::cout << "Client: SDL_TTF Init Failed: " << SDL_GetError() << std::endl;
  }

  // 3. Tải tài nguyên hình ảnh và font chữ qua AssetManager
  assets = new AssetManager(&manager);
  assets->AddTexture("player", "assets/wolf-all.png");
  assets->AddTexture("projectile", "assets/proj.png");
  assets->AddFont("arial", "assets/arial.ttf", 16);

  // 4. Tải biểu diễn bản đồ địa hình tĩnh phía Client
  map = new Map("world", 3, 32);
  map->LoadMap("assets/world_test.tmx");

  // 5. Khởi tạo nhãn chữ hiển thị giao diện người chơi (HUD labels)
  SDL_Color white = {255, 255, 255, 255};
  hudLabel = &manager.addEntity();
  hudLabel->addComponent<UILabel>(10, 10, "Connecting to server...", "arial",
                                  white);

  targetHudLabel = &manager.addEntity();
  targetHudLabel->addComponent<UILabel>(10, 30, "No target selected", "arial",
                                        white);

  // 6. Thực hiện kết nối tới Server
  connectToServer();
}

// Thực hiện kết nối TCP tới máy chủ game (mặc định IP localhost 127.0.0.1, port
// 5050)
void Game::connectToServer() {
  // Phân giải địa chỉ IP của server
  NET_Address* address = NET_ResolveHostname("127.0.0.1");
  if (!address) {
    std::cerr << "Client: Failed to resolve hostname: " << SDL_GetError()
              << std::endl;
    return;
  }

  std::cout << "Client: Resolving server address..." << std::endl;
  // Chờ tối đa 3 giây để hoàn tất phân giải địa chỉ
  if (NET_WaitUntilResolved(address, 3000) == 1) {  // 1 nghĩa là thành công
    // Tạo socket kết nối TCP tới server port 5050
    clientSocket = NET_CreateClient(address, 5050, 0);
    if (clientSocket) {
      std::cout << "Client: Connected to server successfully!" << std::endl;
      isConnected = true;

      // Tạo và gửi gói tin đăng nhập (ClientLoginMsg) lên Server ngay khi kết
      // nối thành công
      ClientLoginMsg login;
      login.header.id = PacketID::CLIENT_LOGIN;
      login.header.length = sizeof(login);
      // Tạo tên đăng nhập ngẫu nhiên (ví dụ: Player_385)
      std::string name = "Player_" + std::to_string(std::rand() % 1000);
      std::strncpy(login.username, name.c_str(), sizeof(login.username) - 1);
      // Gửi gói tin qua socket TCP
      NET_WriteToStreamSocket(clientSocket, &login, sizeof(login));
    } else {
      std::cerr << "Client: Failed to create socket connection: "
                << SDL_GetError() << std::endl;
    }
  } else {
    std::cerr << "Client: Server resolution timed out." << std::endl;
  }
  NET_UnrefAddress(address);  // Giải phóng tài nguyên phân giải địa chỉ
}

// Bắt và xử lý các sự kiện chuột, bàn phím của hệ điều hành gửi tới cửa sổ
void Game::handleEvents() {
  SDL_PollEvent(&event);  // Đọc sự kiện tiếp theo trong hàng đợi

  switch (event.type) {
    case SDL_EVENT_QUIT:
      isRunning = false;  // Thoát game khi nhấn nút đóng cửa sổ (X)
      break;

    case SDL_EVENT_WINDOW_RESIZED: {
      int newW = event.window.data1;
      int newH = event.window.data2;
      std::cout << "Client: Window resized to " << newW << "x" << newH
                << std::endl;
      camera.initW = newW;
      camera.initH = newH;
      camera.w = newW;
      camera.h = newH;
      updateVirtualControlsLayout(newW, newH);
      break;
    }

    case SDL_EVENT_MOUSE_WHEEL:
      // Xử lý cuộn chuột giữa để phóng to / thu nhỏ camera
      if (event.wheel.y > 0) {
        std::cout << "zoom in: " << camera.w << std::endl;
        camera.zoomIn();
      } else {
        std::cout << "zoom out: " << camera.w << std::endl;
        camera.zoomOut();
      }
      break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      // Xử lý sự kiện nhấp chuột trái chọn mục tiêu (Target) hoặc nhấn phím ảo
      if (event.button.button == SDL_BUTTON_LEFT) {
        float winMouseX, winMouseY;
        SDL_GetMouseState(&winMouseX, &winMouseY);
        float mouseX = 0.0f;
        float mouseY = 0.0f;
        SDL_RenderCoordinatesFromWindow(renderer, winMouseX, winMouseY, &mouseX,
                                        &mouseY);
        std::cout << mouseX << ',' << mouseY << std::endl;

        bool clickedVirtualControl = false;
        if (showVirtualControls) {
          // A. Joystick click
          float distToJoy =
              std::hypot(mouseX - joystickCenterX, mouseY - joystickCenterY);
          if (distToJoy <= joystickOuterRadius + 20.0f) {
            clickedVirtualControl = true;
            float dx = mouseX - joystickCenterX;
            float dy = mouseY - joystickCenterY;
            float dist = std::hypot(dx, dy);
            if (dist > joystickOuterRadius) {
              dx = (dx / dist) * joystickOuterRadius;
              dy = (dy / dist) * joystickOuterRadius;
            }
            joystickKnobX = joystickCenterX + dx;
            joystickKnobY = joystickCenterY + dy;
          }

          // B. Attack button click
          float distToAttack =
              std::hypot(mouseX - btnAttackX, mouseY - btnAttackY);
          if (distToAttack <= btnAttackRadius) {
            clickedVirtualControl = true;
            if (currentTargetId > 0 && warMode) {
              sendAction(3, currentTargetId);
            }
          }

          // C. War button click
          float distToWar = std::hypot(mouseX - btnWarX, mouseY - btnWarY);
          if (distToWar <= btnWarRadius) {
            clickedVirtualControl = true;
            sendAction(0, 0);
          }

          // D. Res button click
          float distToRes = std::hypot(mouseX - btnResX, mouseY - btnResY);
          if (distToRes <= btnResRadius) {
            clickedVirtualControl = true;
            sendAction(2, 0);
          }

          // E. Cam button click
          float distToCam = std::hypot(mouseX - btnCamX, mouseY - btnCamY);
          if (distToCam <= btnCamRadius) {
            clickedVirtualControl = true;
            isIsometric = !isIsometric;
            for (auto& entity : manager.getGroup(groupColliders)) {
              if (isIsometric) {
                entity->getComponent<ColliderComponent>().tex =
                    TextureManager::LoadTexture("assets/test-col.png");
              } else {
                entity->getComponent<ColliderComponent>().tex =
                    TextureManager::LoadTexture("assets/ColTex.png");
              }
            }
          }

          // F. Zoom In button click
          float distToZoomIn =
              std::hypot(mouseX - btnZoomInX, mouseY - btnZoomInY);
          if (distToZoomIn <= btnZoomInRadius) {
            clickedVirtualControl = true;
            camera.zoomIn();
          }

          // G. Zoom Out button click
          float distToZoomOut =
              std::hypot(mouseX - btnZoomOutX, mouseY - btnZoomOutY);
          if (distToZoomOut <= btnZoomOutRadius) {
            clickedVirtualControl = true;
            camera.zoomOut();
          }
        }

        if (clickedVirtualControl) {
          break;  // Bỏ qua quét target dưới đất
        }

        // Quy đổi tọa độ chuột thực tế theo tỉ lệ camera zoom
        float clickX = mouseX / camera.zoom;
        float clickY = mouseY / camera.zoom;

        // Quét qua danh sách thực thể mạng để kiểm tra xem chuột có click trúng
        // con nào không
        uint32_t clickedId = 0;
        for (auto const& [entId, entity] : serverEntities) {
          if (entId == myEntityId) continue;  // Không tự chọn chính bản thân

          if (entity->hasComponent<TransformComponent>()) {
            auto& trans = entity->getComponent<TransformComponent>();
            float w = static_cast<float>(trans.width * trans.scale);
            float h = static_cast<float>(trans.height * trans.scale);

            float screenX = 0;
            float screenY = 0;
            if (isIsometric) {
              // Tính toán tọa độ hiển thị Isometric thực tế của Sprite quái vật
              float isoX = (trans.position.x - trans.position.y) * 0.5f;
              float isoY = (trans.position.x + trans.position.y) * 0.25f -
                           trans.position.z;
              screenX = isoX - camera.x;
              screenY = isoY - camera.y - g_WorldConfig.blockHeight;
            } else {
              // Tính toán tọa độ hiển thị Cartesian thực tế của Sprite
              screenX = trans.position.x - camera.x;
              screenY = trans.position.y - camera.y - trans.position.z;
            }

            // Kiểm tra xem clickX, clickY có nằm bên trong hình hộp hiển thị
            // của Sprite không
            if (clickX >= screenX && clickX <= screenX + w &&
                clickY >= screenY && clickY <= screenY + h) {
              clickedId = entId;  // Lưu lại ID thực thể bị nhấp trúng
              break;
            }
          }
        }

        // Nếu click trúng một mục tiêu hợp lệ: cập nhật target và gửi lệnh lên
        // Server
        if (clickedId > 0) {
          currentTargetId = clickedId;
          sendAction(1,
                     currentTargetId);  // actionType=1: Đặt mục tiêu tấn công
        }
      }
      // Xử lý nhấn giữ chuột phải để bắt đầu kéo (drag) di chuyển camera tự do
      if (event.button.button == SDL_BUTTON_RIGHT) {
        float winMouseX, winMouseY;
        SDL_GetMouseState(&winMouseX, &winMouseY);
        float mouseX = 0.0f;
        float mouseY = 0.0f;
        SDL_RenderCoordinatesFromWindow(renderer, winMouseX, winMouseY, &mouseX,
                                        &mouseY);
        lastPosCamX = mouseX;
        lastPosCamY = mouseY;
        cameraFlow =
            false;  // Tắt tự động bám theo player để cho phép kéo tự do
        isResettingCamera = false;  // Hủy cờ khôi phục camera
      }
      break;

    case SDL_EVENT_MOUSE_MOTION:
      // Xử lý di chuyển chuột khi đang nhấn giữ chuột phải (kéo bản đồ)
      if (event.motion.state & SDL_BUTTON_RMASK) {
        float mouseX = 0.0f;
        float mouseY = 0.0f;
        SDL_RenderCoordinatesFromWindow(renderer, event.motion.x,
                                        event.motion.y, &mouseX, &mouseY);

        // Tính khoảng dịch chuyển chuột (Delta) so với vị trí cũ
        float deltaX = mouseX - lastPosCamX;
        float deltaY = mouseY - lastPosCamY;

        // Dịch chuyển ống kính camera theo chiều ngược lại của tay kéo
        camera.x -= static_cast<int>(deltaX);
        camera.y -= static_cast<int>(deltaY);

        // Cập nhật vị trí chuột cũ cho lần tính toán tiếp theo
        lastPosCamX = mouseX;
        lastPosCamY = mouseY;
      }
      break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (event.button.button == SDL_BUTTON_LEFT) {
        // Reset Joystick về tâm khi thả chuột trái
        joystickKnobX = joystickCenterX;
        joystickKnobY = joystickCenterY;
      }
      // Thả chuột phải: Kích hoạt bộ đếm thời gian 1 giây để khôi phục camera
      // flow tự động bám theo player
      if (event.button.button == SDL_BUTTON_RIGHT) {
        if (!cameraFlow) {
          lastRightMouseReleaseTime =
              SDL_GetTicks();        // Ghi nhận thời điểm thả chuột
          isResettingCamera = true;  // Bật cờ chuẩn bị khôi phục bám đuổi
        }
      }
      break;

    case SDL_EVENT_FINGER_DOWN: {
      int winW = 800, winH = 640;
      SDL_GetWindowSize(window, &winW, &winH);
      float winTouchX = event.tfinger.x * winW;
      float winTouchY = event.tfinger.y * winH;
      float touchX = 0.0f;
      float touchY = 0.0f;
      SDL_RenderCoordinatesFromWindow(renderer, winTouchX, winTouchY, &touchX,
                                      &touchY);
      SDL_FingerID fingerId = event.tfinger.fingerID;

      // A. Joystick touch
      float distToJoy =
          std::hypot(touchX - joystickCenterX, touchY - joystickCenterY);
      if (distToJoy <= joystickOuterRadius + 20.0f) {
        joystickFingerId = fingerId;
        float dx = touchX - joystickCenterX;
        float dy = touchY - joystickCenterY;
        float dist = std::hypot(dx, dy);
        if (dist > joystickOuterRadius) {
          dx = (dx / dist) * joystickOuterRadius;
          dy = (dy / dist) * joystickOuterRadius;
        }
        joystickKnobX = joystickCenterX + dx;
        joystickKnobY = joystickCenterY + dy;
      }

      // B. Attack button touch
      float distToAttack = std::hypot(touchX - btnAttackX, touchY - btnAttackY);
      if (distToAttack <= btnAttackRadius) {
        if (currentTargetId > 0 && warMode) {
          sendAction(3, currentTargetId);
        }
      }

      // C. War button touch
      float distToWar = std::hypot(touchX - btnWarX, touchY - btnWarY);
      if (distToWar <= btnWarRadius) {
        sendAction(0, 0);
      }

      // D. Res button touch
      float distToRes = std::hypot(touchX - btnResX, touchY - btnResY);
      if (distToRes <= btnResRadius) {
        sendAction(2, 0);
      }

      // E. Cam button touch
      float distToCam = std::hypot(touchX - btnCamX, touchY - btnCamY);
      if (distToCam <= btnCamRadius) {
        isIsometric = !isIsometric;
        for (auto& entity : manager.getGroup(groupColliders)) {
          if (isIsometric) {
            entity->getComponent<ColliderComponent>().tex =
                TextureManager::LoadTexture("assets/test-col.png");
          } else {
            entity->getComponent<ColliderComponent>().tex =
                TextureManager::LoadTexture("assets/ColTex.png");
          }
        }
      }

      // F. Zoom In button touch
      float distToZoomIn = std::hypot(touchX - btnZoomInX, touchY - btnZoomInY);
      if (distToZoomIn <= btnZoomInRadius) {
        camera.zoomIn();
      }

      // G. Zoom Out button touch
      float distToZoomOut =
          std::hypot(touchX - btnZoomOutX, touchY - btnZoomOutY);
      if (distToZoomOut <= btnZoomOutRadius) {
        camera.zoomOut();
      }
      break;
    }

    case SDL_EVENT_FINGER_MOTION: {
      if (joystickFingerId == event.tfinger.fingerID) {
        int winW = 800, winH = 640;
        SDL_GetWindowSize(window, &winW, &winH);
        float winTouchX = event.tfinger.x * winW;
        float winTouchY = event.tfinger.y * winH;
        float touchX = 0.0f;
        float touchY = 0.0f;
        SDL_RenderCoordinatesFromWindow(renderer, winTouchX, winTouchY, &touchX,
                                        &touchY);

        float dx = touchX - joystickCenterX;
        float dy = touchY - joystickCenterY;
        float dist = std::hypot(dx, dy);
        if (dist > joystickOuterRadius) {
          dx = (dx / dist) * joystickOuterRadius;
          dy = (dy / dist) * joystickOuterRadius;
        }
        joystickKnobX = joystickCenterX + dx;
        joystickKnobY = joystickCenterY + dy;
      }
      break;
    }

    case SDL_EVENT_FINGER_UP: {
      if (joystickFingerId == event.tfinger.fingerID) {
        joystickFingerId = -1;
        joystickKnobX = joystickCenterX;
        joystickKnobY = joystickCenterY;
      }
      break;
    }

    case SDL_EVENT_KEY_DOWN:
      // 1. Nếu đang ở chế độ gõ chat:
      if (typingChat) {
        if (event.key.key == SDLK_RETURN) {
          // Nhấn Enter lần 2 để gửi tin nhắn đi
          if (!chatInputBuffer.empty()) {
            sendChat(chatInputBuffer);  // Gửi gói tin chat lên server
            chatInputBuffer.clear();    // Xóa sạch bộ đệm nhập liệu
          }
          typingChat = false;  // Thoát chế độ gõ chat
        } else if (event.key.key == SDLK_BACKSPACE) {
          // Xóa ký tự cuối cùng
          if (!chatInputBuffer.empty()) {
            chatInputBuffer.pop_back();
          }
        } else {
          // Tích lũy các ký tự mã ASCII in được vào bộ đệm chat (tối đa 100 ký
          // tự)
          if (event.key.key >= 32 && event.key.key <= 126 &&
              chatInputBuffer.size() < 100) {
            chatInputBuffer += static_cast<char>(event.key.key);
          }
        }
      }
      // 2. Chế độ điều khiển phím tắt thông thường:
      else {
        switch (event.key.key) {
          case SDLK_RETURN:
            typingChat = true;  // Nhấn Enter để mở hộp gõ chat
            break;
          case SDLK_I:
            // Nhấn I để bật/tắt chuyển đổi góc nhìn Isometric và 2D phẳng
            // (Cartesian)
            isIsometric = !isIsometric;
            // Thay đổi kết cấu ảnh vẽ collider để phân biệt chế độ hiển thị va
            // chạm
            for (auto& entity : manager.getGroup(groupColliders)) {
              if (isIsometric) {
                entity->getComponent<ColliderComponent>().tex =
                    TextureManager::LoadTexture("assets/test-col.png");
              } else {
                entity->getComponent<ColliderComponent>().tex =
                    TextureManager::LoadTexture("assets/ColTex.png");
              }
            }
            std::cout << "Client: Camera mode toggled. Isometric: "
                      << (isIsometric ? "ON" : "OFF") << std::endl;
            break;
          case SDLK_G:
            // Nhấn G để gửi yêu cầu bật/tắt War Mode (actionType = 0)
            sendAction(0, 0);
            break;
          case SDLK_K:
            // Nhấn K để tấn công chủ động (actionType = 3) nếu đã chọn mục tiêu
            // và đang ở War Mode
            if (currentTargetId > 0 && warMode) {
              sendAction(3, currentTargetId);
            }
            break;
          case SDLK_R:
            // Nhấn R để gửi yêu cầu hồi sinh (actionType = 2) khi đã chết
            sendAction(2, 0);
            break;
          case SDLK_ESCAPE:
            isRunning = false;  // Nhấn ESC để thoát game nhanh
            break;
          default:
            break;
        }
      }
      break;

    default:
      break;
  }
}

// Gửi gói tin yêu cầu di chuyển nhân vật lên Server
void Game::sendMoveRequest(uint8_t direction) {
  if (!isConnected) return;
  ClientMoveMsg msg;
  msg.header.id = PacketID::CLIENT_MOVE_REQUEST;
  msg.header.length = sizeof(msg);
  msg.direction = direction;
  NET_WriteToStreamSocket(clientSocket, &msg, sizeof(msg));
}

// Gửi gói tin chat văn bản lên Server
void Game::sendChat(const std::string& text) {
  if (!isConnected) return;
  ClientChatMsg msg;
  msg.header.id = PacketID::CLIENT_CHAT_SEND;
  msg.header.length = sizeof(msg);
  std::strncpy(msg.text, text.c_str(), sizeof(msg.text) - 1);
  NET_WriteToStreamSocket(clientSocket, &msg, sizeof(msg));
}

// Gửi gói tin yêu cầu hành động chiến đấu, nhắm target hoặc hồi sinh lên Server
void Game::sendAction(uint8_t actionType, uint32_t targetId) {
  if (!isConnected) return;
  ClientActionMsg msg;
  msg.header.id = PacketID::CLIENT_ACTION_REQUEST;
  msg.header.length = sizeof(msg);
  msg.actionType = actionType;
  msg.targetEntityId = targetId;
  NET_WriteToStreamSocket(clientSocket, &msg, sizeof(msg));
}

// Nhận dữ liệu mạng từ socket TCP, nạp vào netBuffer để bóc tách gói
void Game::handleNetwork() {
  if (!isConnected) return;

  uint8_t tempBuf[1024];
  // Đọc byte thô từ socket mạng (không chặn luồng - non-blocking)
  int bytesRead =
      NET_ReadFromStreamSocket(clientSocket, tempBuf, sizeof(tempBuf));

  if (bytesRead > 0) {
    netBuffer.Append(tempBuf,
                     bytesRead);  // Đưa dữ liệu thô vào bộ tích lũy gói

    uint16_t packetId;
    std::vector<uint8_t> packetData;
    // Bóc tách tuần tự tất cả các gói tin mạng hoàn chỉnh có trong bộ đệm
    while (netBuffer.HasCompletePacket(packetId, packetData)) {
      handlePacket(packetId, packetData);
    }
  } else if (bytesRead < 0) {
    // Giá trị trả về < 0 chỉ ra server đã ngắt kết nối
    std::cout << "Client: Disconnected from server." << std::endl;
    isConnected = false;
    hudLabel->getComponent<UILabel>().SetLabelText("Disconnected from server",
                                                   "arial");
  }
}

// Xử lý logic của từng gói tin sau khi bóc tách hoàn chỉnh dựa trên ID gói tin
void Game::handlePacket(uint16_t id, const std::vector<uint8_t>& data) {
  // 1. Server chấp nhận đăng nhập và trả về ID cùng vị trí khởi đầu của nhân
  // vật
  if (id == PacketID::SERVER_LOGIN_ACK) {
    if (data.size() < sizeof(ServerLoginAckMsg)) return;
    const auto* msg = reinterpret_cast<const ServerLoginAckMsg*>(data.data());
    myEntityId = msg->playerEntityId;  // Lưu ID nhân vật của bản thân
    std::cout << "Client: Login Ack. My ID is " << myEntityId << std::endl;
  }
  // 2. Server cập nhật thông số thực thể (vị trí thế giới, HP, trạng thái,
  // notoriety...)
  else if (id == PacketID::SERVER_ENTITY_UPDATE) {
    if (data.size() < sizeof(ServerEntityUpdateMsg)) return;
    const auto* msg =
        reinterpret_cast<const ServerEntityUpdateMsg*>(data.data());

    auto it = serverEntities.find(msg->entityId);
    // TH1: Thực thể mới xuất hiện (chưa có trong danh sách lưu trữ Client):
    // Tiến hành spawn thực thể mới
    if (it == serverEntities.end()) {
      auto& ent(manager.addEntity());

      // Thêm thành phần vị trí 3D
      ent.addComponent<TransformComponent>(msg->x, msg->y, msg->z, 64, 64, 2);
      // Thêm thành phần chạy hoạt ảnh sói
      ent.addComponent<SpriteComponent>("player", true);
      // Thêm thành phần đồng bộ dữ liệu mạng (HP, tên, hướng di chuyển)
      ent.addComponent<NetworkComponent>(msg->entityId, msg->name,
                                         msg->currentHp, msg->maxHp, msg->state,
                                         msg->notoriety, msg->direction);
      // Thêm thành phần hộp va chạm debug
      ent.addComponent<ColliderComponent>("player");
      ent.addGroup(groupColliders);

      auto& netComp = ent.getComponent<NetworkComponent>();
      // Khởi tạo tọa độ nội suy ban đầu trùng khít với tọa độ nhận từ Server
      netComp.targetX = msg->x;
      netComp.targetY = msg->y;
      netComp.targetZ = msg->z;
      netComp.startX = msg->x;
      netComp.startY = msg->y;
      netComp.startZ = msg->z;
      netComp.lastUpdateTicks = SDL_GetTicks();

      ent.addGroup(groupPlayers);            // Đưa vào nhóm thực thể vẽ
      serverEntities[msg->entityId] = &ent;  // Đưa vào bảng tra cứu ID thực thể

      std::cout << "Client: Spawned Entity " << msg->name
                << " (ID: " << msg->entityId << ")" << std::endl;
    }
    // TH2: Thực thể đã tồn tại: Cập nhật tọa độ đích mới nhận từ server để
    // chuẩn bị thực hiện nội suy
    else {
      auto* ent = it->second;
      auto& netComp = ent->getComponent<NetworkComponent>();
      auto& trans = ent->getComponent<TransformComponent>();

      // Gán tọa độ xuất phát bằng tọa độ vẽ hiện tại trên màn hình Client
      netComp.startX = trans.position.x;
      netComp.startY = trans.position.y;
      netComp.startZ = trans.position.z;

      // Gán tọa độ đích đến mới nhận được từ Server
      netComp.targetX = msg->x;
      netComp.targetY = msg->y;
      netComp.targetZ = msg->z;
      netComp.lastUpdateTicks =
          SDL_GetTicks();  // Đánh dấu mốc thời gian nhận gói tin cuối cùng

      // Cập nhật các thống kê máu và hướng di chuyển
      netComp.hp = msg->currentHp;
      netComp.maxHp = msg->maxHp;
      netComp.state = msg->state;
      netComp.notoriety = msg->notoriety;
      netComp.direction = msg->direction;

      auto& sprite = ent->getComponent<SpriteComponent>();
      sprite.spriteFlip = SDL_FLIP_NONE;  // Reset chế độ lật ảnh

      // Nếu đây là nhân vật của bản thân, cập nhật trạng thái War Mode dựa theo
      // state nhận về (state = 2 nghĩa là WAR)
      if (msg->entityId == myEntityId) {
        warMode = (netComp.state == 2);
      }
    }
  }
  // 3. Server thông báo xóa một thực thể biến mất khỏi tầm nhìn
  else if (id == PacketID::SERVER_ENTITY_DESPAWN) {
    if (data.size() < sizeof(ServerEntityDespawnMsg)) return;
    const auto* msg =
        reinterpret_cast<const ServerEntityDespawnMsg*>(data.data());

    auto it = serverEntities.find(msg->entityId);
    if (it != serverEntities.end()) {
      it->second->destroy();  // Đánh dấu xóa thực thể khỏi ECS
      serverEntities.erase(it);
      if (msg->entityId == currentTargetId) {
        currentTargetId =
            0;  // Xóa target nếu quái bị chọn đã biến mất hoặc chết
      }
      std::cout << "Client: Despawned Entity ID " << msg->entityId << std::endl;
    }
  }
  // 4. Server thông báo chat bong bóng của ai đó nói
  else if (id == PacketID::SERVER_CHAT_BROADCAST) {
    if (data.size() < sizeof(ServerChatBroadcastMsg)) return;
    const auto* msg =
        reinterpret_cast<const ServerChatBroadcastMsg*>(data.data());

    auto it = serverEntities.find(msg->entityId);
    if (it != serverEntities.end()) {
      auto& net = it->second->getComponent<NetworkComponent>();
      net.chatText = msg->text;  // Gán nội dung văn bản nói
      net.lastChatTime =
          SDL_GetTicks();  // Đánh dấu thời điểm nói để đếm ngược ẩn
    }
  }
  // 5. Server thông báo sự kiện chiến đấu và lượng máu bị trừ
  else if (id == PacketID::SERVER_COMBAT_EVENT) {
    if (data.size() < sizeof(ServerCombatEventMsg)) return;
    const auto* msg =
        reinterpret_cast<const ServerCombatEventMsg*>(data.data());

    std::cout << "Client: Combat event. " << msg->attackerId << " hit "
              << msg->targetId << " for " << msg->damage << " damage."
              << std::endl;

    // Tìm kiếm kẻ tấn công (attacker) để phát hoạt ảnh tấn công
    auto it = serverEntities.find(msg->attackerId);
    if (it != serverEntities.end() && it->second != nullptr) {
      auto* attackerEnt = it->second;
      if (attackerEnt->hasComponent<SpriteComponent>() &&
          attackerEnt->hasComponent<NetworkComponent>()) {
        auto& sprite = attackerEnt->getComponent<SpriteComponent>();
        auto& net = attackerEnt->getComponent<NetworkComponent>();

        // Xác định hướng của attacker dựa trên net.direction
        std::string dirStr = "S";
        switch (net.direction) {
          case 7:
          case 0:
            dirStr = "E";  // Bắc / Tây Bắc -> hoạt ảnh hướng E
            break;
          case 1:
          case 2:
            dirStr = "S";  // Đông Bắc / Đông -> hoạt ảnh hướng S
            break;
          case 3:
          case 4:
            dirStr = "W";  // Đông Nam / Nam -> hoạt ảnh hướng W
            break;
          case 5:
          case 6:
            dirStr = "N";  // Tây Nam / Tây -> hoạt ảnh hướng N
            break;
          default:
            dirStr = "S";
            break;
        }

        // Phát hoạt ảnh tấn công ưu tiên (priority = true)
        sprite.Play("Attack" + dirStr, true);
      }
    }
  }
}

// Cập nhật logic game Client mỗi frame
void Game::update() {
  handleNetwork();  // Đọc dữ liệu từ Server mạng

  // 1. Kiểm tra di chuyển bằng bàn phím (WASD) hoặc phím ảo (Joystick) để gửi
  // yêu cầu lên Server
  if (isConnected && myEntityId > 0 && !typingChat) {
    int targetDir = -1;

    // A. Kiểm tra Joystick ảo
    float joyDx = joystickKnobX - joystickCenterX;
    float joyDy = joystickKnobY - joystickCenterY;
    float joyDist = std::hypot(joyDx, joyDy);

    if (joyDist > 10.0f) {
      // Có di chuyển bằng Joystick, tính toán hướng (0-7)
      float angle = std::atan2(joyDy, joyDx);
      float angleDeg = angle * 180.0f / M_PI;

      if (angleDeg >= -22.5f && angleDeg < 22.5f)
        targetDir = 1;
      else if (angleDeg >= 22.5f && angleDeg < 67.5f)
        targetDir = 2;
      else if (angleDeg >= 67.5f && angleDeg < 112.5f)
        targetDir = 3;
      else if (angleDeg >= 112.5f && angleDeg < 157.5f)
        targetDir = 4;
      else if (angleDeg >= 157.5f || angleDeg < -157.5f)
        targetDir = 5;
      else if (angleDeg >= -157.5f && angleDeg < -112.5f)
        targetDir = 6;
      else if (angleDeg >= -112.5f && angleDeg < -67.5f)
        targetDir = 7;
      else if (angleDeg >= -67.5f && angleDeg < -22.5f)
        targetDir = 0;
    } else {
      // B. Nếu không dùng Joystick, kiểm tra bàn phím WASD
      const bool* keyState = SDL_GetKeyboardState(nullptr);
      int kDx = 0;
      int kDy = 0;
      if (keyState[SDL_SCANCODE_W] || keyState[SDL_SCANCODE_UP]) kDy -= 1;
      if (keyState[SDL_SCANCODE_S] || keyState[SDL_SCANCODE_DOWN]) kDy += 1;
      if (keyState[SDL_SCANCODE_A] || keyState[SDL_SCANCODE_LEFT]) kDx -= 1;
      if (keyState[SDL_SCANCODE_D] || keyState[SDL_SCANCODE_RIGHT]) kDx += 1;

      if (kDx != 0 || kDy != 0) {
        uint8_t dir = 4;  // Mặc định đi hướng Nam
        if (kDx == 0 && kDy == -1)
          dir = 0;  // Bắc
        else if (kDx == 1 && kDy == -1)
          dir = 1;  // Đông Bắc
        else if (kDx == 1 && kDy == 0)
          dir = 2;  // Đông
        else if (kDx == 1 && kDy == 1)
          dir = 3;  // Đông Nam
        else if (kDx == 0 && kDy == 1)
          dir = 4;  // Nam
        else if (kDx == -1 && kDy == 1)
          dir = 5;  // Tây Nam
        else if (kDx == -1 && kDy == 0)
          dir = 6;  // Tây
        else if (kDx == -1 && kDy == -1)
          dir = 7;  // Tây Bắc

        // Nếu đang hiển thị chế độ Isometric nghiêng, xoay lệch hướng di chuyển
        // đi 45 độ ngược chiều kim đồng hồ
        if (isIsometric) {
          dir = (dir + 7) % 8;
        }
        targetDir = dir;
      }
    }

    // C. Gửi yêu cầu di chuyển lên server có giới hạn tần suất (Rate Limiting)
    if (targetDir != -1) {
      uint32_t now = SDL_GetTicks();
      // Gửi ngay nếu đổi hướng hoặc cách lần gửi trước >= 100ms
      if (targetDir != lastMoveDir || now - lastMoveSendTime >= 100) {
        sendMoveRequest(targetDir);
        lastMoveDir = targetDir;
        lastMoveSendTime = now;
      }
    } else {
      // Khi dừng di chuyển, reset hướng gửi trước đó về -1
      lastMoveDir = -1;
    }
  }

  // 2. Nội suy tuyến tính (Linear Interpolation) vị trí hiển thị của
  // quái/player khác Việc nội suy này giúp bù đắp độ trễ mạng (50ms interval
  // giữa các tick Server) làm thực thể chuyển động mượt mà thay vì giật cục.
  for (auto const& [entId, ent] : serverEntities) {
    if (ent->hasComponent<TransformComponent>() &&
        ent->hasComponent<NetworkComponent>()) {
      auto& trans = ent->getComponent<TransformComponent>();
      auto& net = ent->getComponent<NetworkComponent>();

      float dx = net.targetX - trans.position.x;
      float dy = net.targetY - trans.position.y;
      float dist = std::hypot(dx, dy);

      // Nếu khoảng cách quá xa (lớn hơn 150px) do bị dịch chuyển tức thời
      // (teleport/spawn): dịch chuyển thẳng tới đích ngay lập tức
      if (dist > 150.0f) {
        trans.position.x = net.targetX;
        trans.position.y = net.targetY;
        trans.position.z = net.targetZ;
        net.startX = net.targetX;
        net.startY = net.targetY;
        net.startZ = net.targetZ;
      }
      // Tiến hành nội suy tuyến tính vị trí hiển thị giữa start và target trong
      // khoảng thời gian đệm 50ms của Server
      else {
        uint32_t elapsed = SDL_GetTicks() - net.lastUpdateTicks;
        float t = static_cast<float>(elapsed) /
                  50.0f;  // Tính toán tỷ lệ phần trăm thời gian trôi qua (t
                          // trong khoảng [0.0, 1.0])
        if (t > 1.0f) t = 1.0f;

        // Nội suy tọa độ vị trí 3D
        trans.position.x = net.startX + (net.targetX - net.startX) * t;
        trans.position.y = net.startY + (net.targetY - net.startY) * t;
        trans.position.z = net.startZ + (net.targetZ - net.startZ) * t;
      }

      // Tự động chuyển đổi và phát hoạt ảnh di chuyển/đứng yên tương ứng theo
      // hướng thực tế nhận từ mạng
      if (ent->hasComponent<SpriteComponent>()) {
        auto& sprite = ent->getComponent<SpriteComponent>();
        std::string dirStr = "S";  // Hướng mặc định

        // Ánh xạ 8 hướng từ gói tin thành 4 hướng chính (E, W, S, N) của sprite
        // sheet sói
        switch (net.direction) {
          case 7:
          case 0:
            dirStr = "E";  // Tây Bắc (7) hoặc Bắc (0) -> Đi lên phía sau (East
                           // trong hoạt ảnh)
            break;
          case 1:
          case 2:
            dirStr = "S";  // Đông Bắc (1) hoặc Đông (2) -> Đi chéo lên bên phải
                           // (South trong hoạt ảnh)
            break;
          case 3:
          case 4:
            dirStr = "W";  // Đông Nam (3) hoặc Nam (4) -> Đi chéo xuống bên
                           // phải (West trong hoạt ảnh)
            break;
          case 5:
          case 6:
            dirStr = "N";  // Tây Nam (5) hoặc Tây (6) -> Đi chéo xuống bên trái
                           // (North trong hoạt ảnh)
            break;
          default:
            dirStr = "S";
            break;
        }

        // Tính khoảng cách dịch chuyển thực tế giữa vị trí đích và xuất phát
        // của Server tick này
        float moveDx = net.targetX - net.startX;
        float moveDy = net.targetY - net.startY;
        float moveDist = std::hypot(moveDx, moveDy);

        uint32_t elapsed = SDL_GetTicks() - net.lastUpdateTicks;
        // Thực thể coi là đang di chuyển nếu có khoảng cách di chuyển thực tế
        // và thời gian nhận gói tin chưa quá 170ms
        bool recentlyMoving = (moveDist > 0.5f) && (elapsed < 170);

        // Chạy hoạt ảnh đi bộ hoặc đứng yên tương ứng
        if (recentlyMoving) {
          sprite.Play("Walk" +
                      dirStr);  // Gọi hoạt ảnh ví dụ: "WalkW", "WalkS"...
        } else {
          sprite.Play("Idle" + dirStr);  // Gọi hoạt ảnh đứng yên: "IdleW"...
        }
      }
    }
  }

  // Làm sạch các Entity chết và chạy cập nhật logic của hệ thống ECS
  manager.refresh();
  manager.update();

  // 3. Cập nhật vị trí Camera bám theo người chơi cục bộ (Local Player) hoặc xử
  // lý camera flow kéo chuột
  auto myIt = serverEntities.find(myEntityId);
  if (myIt != serverEntities.end() && myIt->second != nullptr) {
    auto& netComp = myIt->second->getComponent<NetworkComponent>();
    // Reset toàn bộ trạng thái phím ảo khi nhân vật chết để tránh kẹt điều
    // khiển sau khi hồi sinh
    if (netComp.hp <= 0) {
      joystickKnobX = joystickCenterX;
      joystickKnobY = joystickCenterY;
      joystickFingerId = -1;
      lastMoveDir = -1;
    }

    auto& trans = myIt->second->getComponent<TransformComponent>();

    int targetCamX = 0;
    int targetCamY = 0;

    // Chiều rộng và chiều cao vùng nhìn của camera
    float viewWidth = static_cast<float>(camera.initW);
    float viewHeight = static_cast<float>(camera.initH);

    // Điều chỉnh kích thước vùng nhìn của thế giới tương ứng với tỷ lệ thu
    // phóng (zoom) hiện tại
    if (camera.zoom > 0.0f) {
      viewWidth = viewWidth / camera.zoom;
      viewHeight = viewHeight / camera.zoom;
    }

    if (isIsometric) {
      // Quy đổi tọa độ Cartesian phẳng của nhân vật sang tọa độ hiển thị
      // Isometric
      float isoX = (trans.position.x - trans.position.y) * 0.5f;
      float isoY =
          (trans.position.x + trans.position.y) * 0.25f - trans.position.z;

      // Tâm hiển thị thực tế của nhân vật trên màn hình
      float spriteCenterX = isoX + ((trans.width * trans.scale) / 2.0f);
      float spriteCenterY = isoY + ((trans.height * trans.scale) / 2.0f);

      // Điểm góc trái trên của Camera để đưa nhân vật vào chính giữa màn hình
      targetCamX = static_cast<int>(spriteCenterX - (viewWidth / 2.0f));
      targetCamY = static_cast<int>(spriteCenterY - (viewHeight / 2.0f));

      // Giới hạn Camera không trôi ra ngoài biên bản đồ (Clamp) đối với
      // Isometric
      int minX = -10 * g_WorldConfig.scaledSize;
      int maxX = 13 * g_WorldConfig.scaledSize;
      int minY = -320;
      int maxY = 11 * g_WorldConfig.scaledSize;
      if (targetCamX < minX) targetCamX = minX;
      if (targetCamY < minY) targetCamY = minY;
      if (targetCamX > maxX - static_cast<int>(viewWidth))
        targetCamX = maxX - static_cast<int>(viewWidth);
      if (targetCamY > maxY - static_cast<int>(viewHeight))
        targetCamY = maxY - static_cast<int>(viewHeight);
    } else {
      // Tâm của nhân vật trên hệ phẳng Cartesian 2D
      float spriteCenterX =
          trans.position.x + ((trans.width * trans.scale) / 2.0f);
      float spriteCenterY = trans.position.y +
                            ((trans.height * trans.scale) / 2.0f) -
                            trans.position.z;

      targetCamX = static_cast<int>(spriteCenterX - (viewWidth / 2.0f));
      targetCamY = static_cast<int>(spriteCenterY - (viewHeight / 2.0f));

      // Giới hạn biên bản đồ cho Cartesian
      if (targetCamX < 0) targetCamX = 0;
      if (targetCamY < 0) targetCamY = 0;
      if (targetCamX >
          25 * g_WorldConfig.scaledSize - static_cast<int>(viewWidth))
        targetCamX =
            25 * g_WorldConfig.scaledSize - static_cast<int>(viewWidth);
      if (targetCamY >
          20 * g_WorldConfig.scaledSize - static_cast<int>(viewHeight))
        targetCamY =
            20 * g_WorldConfig.scaledSize - static_cast<int>(viewHeight);
    }

    // Cơ chế khôi phục Camera Flow sau khi kéo tự do: Chờ đủ 1 giây (1000ms)
    // sau khi thả chuột phải thì tự động trôi camera từ từ về lại bám theo nhân
    // vật
    if (isResettingCamera && !cameraFlow) {
      if (SDL_GetTicks() - lastRightMouseReleaseTime >= 1000) {
        cameraFlow = true;
        isResettingCamera = false;
      }
    }

    // Cập nhật nội suy di chuyển camera mượt mà
    camera.update(static_cast<float>(targetCamX),
                  static_cast<float>(targetCamY), cameraFlow);

    // --- TỰ ĐỘNG CHỌN MỤC TIÊU (AUTO-AIM) KHI HƯỚNG VỀ QUÁI VẬT ---
    {
      auto& playerNet = myIt->second->getComponent<NetworkComponent>();
      // Chỉ thực hiện aim khi player còn sống
      if (playerNet.hp > 0 && playerNet.state != 3) {
        // Bảng vector hướng chuẩn hóa 2D tương ứng với direction (0-7)
        const float dirVectors[8][2] = {
            {0.8944f, -0.4472f},   // 0: Bắc Isometric
            {1.0000f, 0.0000f},    // 1: Đông Bắc Isometric
            {0.8944f, 0.4472f},    // 2: Đông Isometric
            {0.0000f, 1.0000f},    // 3: Đông Nam Isometric
            {-0.8944f, 0.4472f},   // 4: Nam Isometric
            {-1.0000f, 0.0000f},   // 5: Tây Nam Isometric
            {-0.8944f, -0.4472f},  // 6: Tây Isometric
            {0.0000f, -1.0000f}    // 7: Tây Bắc Isometric
        };

        uint8_t pDir = playerNet.direction;
        if (pDir < 8) {
          float pDX = dirVectors[pDir][0];
          float pDY = dirVectors[pDir][1];

          uint32_t bestTargetId = 0;
          float minDistance =
              160.0f;  // Khoảng cách nhắm mục tiêu tối đa (160px)
          const float FOV_THRESHOLD = 0.8f;  // cos(36 độ) ~ 0.8

          for (auto const& [entId, ent] : serverEntities) {
            if (entId == myEntityId || ent == nullptr) continue;

            if (ent->hasComponent<TransformComponent>() &&
                ent->hasComponent<NetworkComponent>()) {
              auto& npcTrans = ent->getComponent<TransformComponent>();
              auto& npcNet = ent->getComponent<NetworkComponent>();

              // Chỉ aim quái vật (notoriety == 2) còn sống
              if (npcNet.notoriety == 2 && npcNet.hp > 0 && npcNet.state != 3) {
                float vX = npcTrans.position.x - trans.position.x;
                float vY = npcTrans.position.y - trans.position.y;
                float dist = std::hypot(vX, vY);

                if (dist > 0.0f && dist <= minDistance) {
                  // Chuẩn hóa vector hướng tới NPC
                  float uX = vX / dist;
                  float uY = vY / dist;

                  // Tính tích vô hướng
                  float dot = uX * pDX + uY * pDY;

                  if (dot >= FOV_THRESHOLD) {
                    minDistance = dist;
                    bestTargetId = entId;
                  }
                }
              }
            }
          }

          // Cập nhật target nếu tìm thấy quái tốt hơn và gửi tin lên Server
          if (bestTargetId > 0 && bestTargetId != currentTargetId) {
            currentTargetId = bestTargetId;
            sendAction(1, currentTargetId);
            std::cout << "Client: Auto-aimed target ID " << currentTargetId
                      << std::endl;
          }
        }
      }
    }

    // Cập nhật nội dung hiển thị cho nhãn chữ HUD dưới cùng
    auto& net = myIt->second->getComponent<NetworkComponent>();
    std::stringstream ss;
    ss << net.name << " | HP: " << net.hp << "/" << net.maxHp << " | Pos: ("
       << static_cast<int>(trans.position.x) << ", "
       << static_cast<int>(trans.position.y) << ", "
       << static_cast<int>(trans.position.z) << ")"
       << " | WarMode: " << (warMode ? "WAR" : "NORMAL")
       << " (G: WarMode, K: Attack) | Camera: "
       << (isIsometric ? "ISOMETRIC" : "CARTESIAN") << " (I to toggle)";
    if (typingChat) {
      ss << "\n[Say]: " << chatInputBuffer
         << "_";  // Nhấp nháy ký tự nhập liệu chat
    }
    hudLabel->getComponent<UILabel>().SetLabelText(ss.str(), "arial");
  } else {
    // Đang chờ gói tin LoginAck của Server để spawn nhân vật bản thân
    hudLabel->getComponent<UILabel>().SetLabelText("Logging in...", "arial");
  }

  // 4. Cập nhật nội dung hiển thị cho nhãn chữ HUD mục tiêu (Target HUD)
  if (currentTargetId > 0) {
    auto it = serverEntities.find(currentTargetId);
    // Nếu quái vật mục tiêu còn sống và nằm trong bảng thực thể hiển thị
    if (it != serverEntities.end()) {
      auto& net = it->second->getComponent<NetworkComponent>();
      std::stringstream ss;
      ss << "Target: " << net.name << " | HP: " << net.hp << "/" << net.maxHp;
      targetHudLabel->getComponent<UILabel>().SetLabelText(ss.str(), "arial");

      // Cấu hình màu sắc nhãn chữ dựa theo notoriety danh tiếng của mục tiêu
      SDL_Color redColor = {255, 100, 100,
                            255};  // Đỏ cho quái / quái đỏ sát nhân
      SDL_Color blueColor = {100, 150, 255,
                             255};  // Xanh dương cho người chơi vô tội
      targetHudLabel->getComponent<UILabel>().SetLabelColor(
          net.notoriety == 2 ? redColor : blueColor);
    } else {
      // Nếu mục tiêu đã biến mất hoặc chết
      currentTargetId = 0;
      targetHudLabel->getComponent<UILabel>().SetLabelText("No target selected",
                                                           "arial");
    }
  } else {
    // Không chọn mục tiêu nào
    targetHudLabel->getComponent<UILabel>().SetLabelText("No target selected",
                                                         "arial");
    SDL_Color white = {255, 255, 255, 255};
    targetHudLabel->getComponent<UILabel>().SetLabelColor(white);
  }
}

// Vẽ toàn bộ thế giới game lên Renderer SDL
void Game::render() {
  SDL_RenderClear(renderer);  // Xóa sạch khung hình cũ

  // --- ÁP DỤNG HỆ SỐ ZOOM CHO THẾ GIỚI GAME ---
  // Toàn bộ các thực thể thuộc thế giới (map tiles, sprites) được vẽ bằng cách
  // nhân tỉ lệ zoom tương ứng
  SDL_SetRenderScale(renderer, Game::camera.zoom, Game::camera.zoom);

  // 1. Vẽ các gạch nền đất (groupMap) của bản đồ tĩnh
  for (auto& t : manager.getGroup(Game::groupMap)) {
    t->draw();
  }

  // Vẽ các hộp va chạm gỡ lỗi (chỉ hiện nếu showColliders == true)
  for (auto& c : manager.getGroup(Game::groupColliders)) {
    c->draw();
  }

  // 2. Vẽ tất cả người chơi và quái vật (NPCs) trong nhóm groupPlayers
  for (auto& p : manager.getGroup(Game::groupPlayers)) {
    p->draw();  // Vẽ hoạt ảnh sprite nhân vật sói

    if (p->hasComponent<NetworkComponent>()) {
      auto& net = p->getComponent<NetworkComponent>();
      auto& trans = p->getComponent<TransformComponent>();

      float screenX = 0;
      float screenY = 0;
      if (isIsometric) {
        float isoX = (trans.position.x - trans.position.y) * 0.5f;
        float isoY =
            (trans.position.x + trans.position.y) * 0.25f - trans.position.z;
        screenX = isoX - camera.x;
        screenY = isoY - camera.y - g_WorldConfig.blockHeight;
      } else {
        screenX = trans.position.x - camera.x;
        screenY = trans.position.y - camera.y - trans.position.z;
      }

      // Vẽ tên thực thể bay lơ lửng trên đầu Sprite nhân vật
      SDL_Color tagColor = {255, 255, 255, 255};
      if (net.notoriety == 0)
        tagColor = {100, 180, 255, 255};  // Màu xanh lam cho người vô tội
      else if (net.notoriety == 1)
        tagColor = {180, 180, 180, 255};  // Màu xám cho tội phạm
      else if (net.notoriety == 2)
        tagColor = {255, 80, 80, 255};  // Màu đỏ cho quái vật / sát nhân

      TTF_Font* f = assets->GetFont("arial");
      if (f) {
        // Vẽ tên nhãn chữ
        SDL_Surface* nameSurf =
            TTF_RenderText_Blended(f, net.name.c_str(), 0, tagColor);
        if (nameSurf) {
          SDL_Texture* nameTex =
              SDL_CreateTextureFromSurface(renderer, nameSurf);
          if (nameTex) {
            float w = 0, h = 0;
            SDL_GetTextureSize(nameTex, &w, &h);
            // Định vị trí vẽ căn giữa trên đầu thực thể
            SDL_FRect dest = {screenX + (trans.width * trans.scale) / 2 - w / 2,
                              screenY - 25, w, h};
            SDL_RenderTexture(renderer, nameTex, nullptr, &dest);
            SDL_DestroyTexture(nameTex);
          }
          SDL_DestroySurface(nameSurf);
        }

        // Vẽ bong bóng tin nhắn chat hiển thị trong 5 giây sau khi nói
        uint32_t ticks = SDL_GetTicks();
        if (ticks - net.lastChatTime < 5000 && !net.chatText.empty()) {
          SDL_Color chatColor = {255, 255, 150,
                                 255};  // Chữ màu vàng nhạt dễ nhìn
          SDL_Surface* chatSurf =
              TTF_RenderText_Blended(f, net.chatText.c_str(), 0, chatColor);
          if (chatSurf) {
            SDL_Texture* chatTex =
                SDL_CreateTextureFromSurface(renderer, chatSurf);
            if (chatTex) {
              float w = 0, h = 0;
              SDL_GetTextureSize(chatTex, &w, &h);
              // Định vị trí bong bóng chat cao hơn nhãn tên 25px
              SDL_FRect dest = {
                  screenX + (trans.width * trans.scale) / 2 - w / 2,
                  screenY - 50, w, h};
              SDL_RenderTexture(renderer, chatTex, nullptr, &dest);
              SDL_DestroyTexture(chatTex);
            }
            SDL_DestroySurface(chatSurf);
          }
        }
      }

      // Vẽ thanh máu (HP Bar) nhỏ hiển thị ngay sát phía trên đầu thực thể
      float barW = 50.0f;  // Độ dài tối đa thanh máu
      float barH = 5.0f;   // Chiều cao thanh máu
      float hpPercent =
          static_cast<float>(net.hp) / static_cast<float>(net.maxHp);
      if (hpPercent < 0.0f) hpPercent = 0.0f;

      // Hộp nền màu xám đen
      SDL_FRect bgBar = {screenX + (trans.width * trans.scale) / 2 - barW / 2,
                         screenY - 8, barW, barH};
      // Hộp lượng máu màu đỏ
      SDL_FRect fgBar = {screenX + (trans.width * trans.scale) / 2 - barW / 2,
                         screenY - 8, barW * hpPercent, barH};

      SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
      SDL_RenderFillRect(renderer, &bgBar);
      SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
      SDL_RenderFillRect(renderer, &fgBar);
    }
  }

  // --- RESET TỶ LỆ DRAW SCALE VỀ 1.0 ĐỂ VẼ GIAO DIỆN HUD CỐ ĐỊNH ---
  // Nếu không reset, HUD chữ UI sẽ bị co dãn biến dạng theo mức độ camera zoom
  // của bản đồ thế giới.
  SDL_SetRenderScale(renderer, 1.0f, 1.0f);
  // 3. Vẽ các nhãn giao diện tĩnh (HUD labels)
  hudLabel->draw();
  targetHudLabel->draw();

  // 4. Vẽ các phím ảo trên màn hình nếu được kích hoạt
  if (showVirtualControls) {
    // A. Vẽ vòng tròn ngoài Joystick (Xám trong suốt)
    SDL_Color joyOuterColor = {180, 180, 180, 140};
    drawOutlineCircle(renderer, joystickCenterX, joystickCenterY,
                      joystickOuterRadius, joyOuterColor);

    // B. Vẽ núm Joystick tròn bên trong
    SDL_Color joyInnerColor = {240, 240, 240, 200};
    drawFilledCircle(renderer, joystickKnobX, joystickKnobY,
                     joystickInnerRadius, joyInnerColor);

    // C. Vẽ nút Attack (ATK) màu đỏ mờ
    SDL_Color attackColor = {255, 80, 80, 160};
    drawFilledCircle(renderer, btnAttackX, btnAttackY, btnAttackRadius,
                     attackColor);

    // D. Vẽ nút WarMode (WAR) màu cam mờ
    SDL_Color warColor = {255, 165, 0, 160};
    drawFilledCircle(renderer, btnWarX, btnWarY, btnWarRadius, warColor);

    // E. Vẽ nút Hồi sinh (RES) màu xanh lục mờ
    SDL_Color resColor = {100, 220, 100, 160};
    drawFilledCircle(renderer, btnResX, btnResY, btnResRadius, resColor);

    // F. Vẽ nút Đổi Camera (CAM) màu tím hoa sen mờ (tránh trùng màu xanh da
    // trời của nền game)
    SDL_Color camColor = {200, 100, 255, 150};
    drawFilledCircle(renderer, btnCamX, btnCamY, btnCamRadius, camColor);

    // G. Vẽ nút Zoom In (+) màu xanh lam sáng mờ
    SDL_Color zoomInColor = {80, 180, 255, 140};
    drawFilledCircle(renderer, btnZoomInX, btnZoomInY, btnZoomInRadius,
                     zoomInColor);

    // H. Vẽ nút Zoom Out (-) màu xanh lam sáng mờ
    SDL_Color zoomOutColor = {80, 180, 255, 140};
    drawFilledCircle(renderer, btnZoomOutX, btnZoomOutY, btnZoomOutRadius,
                     zoomOutColor);

    // G. Vẽ text chỉ dẫn lên các nút
    TTF_Font* f = assets->GetFont("arial");
    if (f) {
      SDL_Color textColor = {255, 255, 255, 230};

      // Chữ ATK
      SDL_Surface* sAtk = TTF_RenderText_Blended(f, "ATK", 0, textColor);
      if (sAtk) {
        SDL_Texture* tAtk = SDL_CreateTextureFromSurface(renderer, sAtk);
        if (tAtk) {
          float w = 0, h = 0;
          SDL_GetTextureSize(tAtk, &w, &h);
          SDL_FRect dest = {btnAttackX - w / 2.0f, btnAttackY - h / 2.0f, w, h};
          SDL_RenderTexture(renderer, tAtk, nullptr, &dest);
          SDL_DestroyTexture(tAtk);
        }
        SDL_DestroySurface(sAtk);
      }

      // Chữ WAR
      SDL_Surface* sWar = TTF_RenderText_Blended(f, "WAR", 0, textColor);
      if (sWar) {
        SDL_Texture* tWar = SDL_CreateTextureFromSurface(renderer, sWar);
        if (tWar) {
          float w = 0, h = 0;
          SDL_GetTextureSize(tWar, &w, &h);
          SDL_FRect dest = {btnWarX - w / 2.0f, btnWarY - h / 2.0f, w, h};
          SDL_RenderTexture(renderer, tWar, nullptr, &dest);
          SDL_DestroyTexture(tWar);
        }
        SDL_DestroySurface(sWar);
      }

      // Chữ RES
      SDL_Surface* sRes = TTF_RenderText_Blended(f, "RES", 0, textColor);
      if (sRes) {
        SDL_Texture* tRes = SDL_CreateTextureFromSurface(renderer, sRes);
        if (tRes) {
          float w = 0, h = 0;
          SDL_GetTextureSize(tRes, &w, &h);
          SDL_FRect dest = {btnResX - w / 2.0f, btnResY - h / 2.0f, w, h};
          SDL_RenderTexture(renderer, tRes, nullptr, &dest);
          SDL_DestroyTexture(tRes);
        }
        SDL_DestroySurface(sRes);
      }

      // Chữ CAM
      SDL_Surface* sCam = TTF_RenderText_Blended(f, "CAM", 0, textColor);
      if (sCam) {
        SDL_Texture* tCam = SDL_CreateTextureFromSurface(renderer, sCam);
        if (tCam) {
          float w = 0, h = 0;
          SDL_GetTextureSize(tCam, &w, &h);
          SDL_FRect dest = {btnCamX - w / 2.0f, btnCamY - h / 2.0f, w, h};
          SDL_RenderTexture(renderer, tCam, nullptr, &dest);
          SDL_DestroyTexture(tCam);
        }
        SDL_DestroySurface(sCam);
      }

      // Chữ + (Zoom In)
      SDL_Surface* sZoomIn = TTF_RenderText_Blended(f, "+", 0, textColor);
      if (sZoomIn) {
        SDL_Texture* tZoomIn = SDL_CreateTextureFromSurface(renderer, sZoomIn);
        if (tZoomIn) {
          float w = 0, h = 0;
          SDL_GetTextureSize(tZoomIn, &w, &h);
          SDL_FRect dest = {btnZoomInX - w / 2.0f, btnZoomInY - h / 2.0f, w, h};
          SDL_RenderTexture(renderer, tZoomIn, nullptr, &dest);
          SDL_DestroyTexture(tZoomIn);
        }
        SDL_DestroySurface(sZoomIn);
      }

      // Chữ - (Zoom Out)
      SDL_Surface* sZoomOut = TTF_RenderText_Blended(f, "-", 0, textColor);
      if (sZoomOut) {
        SDL_Texture* tZoomOut =
            SDL_CreateTextureFromSurface(renderer, sZoomOut);
        if (tZoomOut) {
          float w = 0, h = 0;
          SDL_GetTextureSize(tZoomOut, &w, &h);
          SDL_FRect dest = {btnZoomOutX - w / 2.0f, btnZoomOutY - h / 2.0f, w,
                            h};
          SDL_RenderTexture(renderer, tZoomOut, nullptr, &dest);
          SDL_DestroyTexture(tZoomOut);
        }
        SDL_DestroySurface(sZoomOut);
      }
    }
  }

  SDL_RenderPresent(renderer);  // Đưa toàn bộ các buffer vẽ lên màn hình
}

// Giải phóng tài nguyên, đóng các subsystem của SDL và dọn dẹp kết nối socket
void Game::clean() {
  if (clientSocket) {
    NET_DestroyStreamSocket(clientSocket);  // Giải phóng socket mạng TCP
    clientSocket = nullptr;
  }

  if (window) {
    SDL_DestroyWindow(window);  // Hủy cửa sổ SDL
    window = nullptr;
  }
  if (renderer) {
    SDL_DestroyRenderer(renderer);  // Hủy bộ vẽ renderer
    renderer = nullptr;
  }
  if (assets) {
    delete assets;  // Hủy AssetManager giải phóng textures và fonts
    assets = nullptr;
  }
  if (map) {
    delete map;
    map = nullptr;
  }

  // Tắt hoàn toàn các subsystem mạng, vẽ chữ và SDL
  NET_Quit();
  TTF_Quit();
  SDL_Quit();
  std::cout << "Client: Game Cleaned" << std::endl;
}

// Vẽ vòng tròn đặc (Filled Circle) bằng cách vẽ các dòng quét ngang (Horizontal
// Scanlines)
void Game::drawFilledCircle(SDL_Renderer* renderer, float centerX,
                            float centerY, float radius, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  for (float dy = -radius; dy <= radius; dy += 1.0f) {
    float dx = std::sqrt(radius * radius - dy * dy);
    SDL_RenderLine(renderer, centerX - dx, centerY + dy, centerX + dx,
                   centerY + dy);
  }
}

// Vẽ đường viền vòng tròn (Outline Circle) bằng cách kết nối các điểm trên
// đường tròn
void Game::drawOutlineCircle(SDL_Renderer* renderer, float centerX,
                             float centerY, float radius, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  const float step = 0.05f;
  float lastX = centerX + radius;
  float lastY = centerY;
  for (float theta = step; theta <= 2.0f * M_PI + step; theta += step) {
    float nextX = centerX + radius * std::cos(theta);
    float nextY = centerY + radius * std::sin(theta);
    SDL_RenderLine(renderer, lastX, lastY, nextX, nextY);
    lastX = nextX;
    lastY = nextY;
  }
}

// Cập nhật vị trí và kích thước các phím ảo (Virtual Controls) động dựa theo
// kích thước màn hình Window thực tế
void Game::updateVirtualControlsLayout(int winW, int winH) {
  float fWinW = static_cast<float>(winW);
  float fWinH = static_cast<float>(winH);

  // 1. Tính toán bán kính động của Joystick và các nút bấm theo chiều cao màn
  // hình fWinH
  joystickOuterRadius = fWinH * 0.15f;  // 15% chiều cao màn hình (to rõ nét)
  joystickInnerRadius = fWinH * 0.06f;  // 6% chiều cao màn hình

  btnAttackRadius =
      fWinH * 0.10f;  // 10% chiều cao màn hình (nút Attack rất to và dễ bấm)
  btnWarRadius = fWinH * 0.065f;  // 6.5% chiều cao màn hình
  btnResRadius = fWinH * 0.065f;  // 6.5% chiều cao màn hình
  btnCamRadius = fWinH * 0.065f;  // 6.5% chiều cao màn hình

  btnZoomInRadius = fWinH * 0.05f;  // 5% chiều cao màn hình cho nút Zoom
  btnZoomOutRadius = fWinH * 0.05f;

  // 2. Tính toán vị trí tâm các phím ảo
  // Joystick: Đặt ở góc dưới bên trái, cách mép dưới và mép trái 1.3 lần bán
  // kính ngoài
  joystickCenterX = joystickOuterRadius * 1.3f;
  joystickCenterY = fWinH - joystickOuterRadius * 1.3f;
  // Đặt núm xoay về tâm joystick mặc định
  joystickKnobX = joystickCenterX;
  joystickKnobY = joystickCenterY;

  // Nút Attack (ATK): Đặt ở góc dưới bên phải, cách mép một khoảng bằng 1.5 lần
  // bán kính nút
  btnAttackX = fWinW - btnAttackRadius * 1.5f;
  btnAttackY = fWinH - btnAttackRadius * 1.5f;

  // Khoảng cách bố trí các nút phụ xung quanh nút Attack
  float dist = btnAttackRadius * 2.1f;

  // WAR: thẳng đứng phía trên nút Attack (góc 90 độ)
  btnWarX = btnAttackX;
  btnWarY = btnAttackY - dist;

  // RES: lệch chéo lên phía trên-trái (góc 135 độ)
  btnResX = btnAttackX - dist * 0.707f;
  btnResY = btnAttackY - dist * 0.707f;

  // CAM: nằm ngang sang phía bên trái nút Attack (góc 180 độ)
  btnCamX = btnAttackX - dist;
  btnCamY = btnAttackY;

  // ZOOM: đặt ở góc trên bên phải màn hình
  btnZoomInX = fWinW - btnZoomInRadius * 1.5f;
  btnZoomInY = btnZoomInRadius * 1.5f;

  btnZoomOutX = fWinW - btnZoomInRadius * 3.5f;
  btnZoomOutY = btnZoomInRadius * 1.5f;

  std::cout << "Client: Dynamic Layout updated. Joystick Radius: "
            << joystickOuterRadius << ", Attack Radius: " << btnAttackRadius
            << ", Screen: " << winW << "x" << winH << std::endl;
}
