#include "Server/Core/ServerApp.hpp"

#include <cmath>
#include <iostream>

#include "Common/Network/Packets.hpp"
#include "Common/World/WorldConfig.hpp"

// Constructor mặc định
ServerApp::ServerApp() = default;

// Destructor gọi hàm clean để dọn dẹp tài nguyên
ServerApp::~ServerApp() { clean(); }

// Khởi tạo Server: Thiết lập mạng, nạp bản đồ đĩa, mở socket 5050 và spawn quái
// vật mặc định
bool ServerApp::init() {
  // 1. Khởi tạo thư viện SDL_net cho socket TCP
  if (!NET_Init()) {
    std::cerr << "Server: NET_Init failed: " << SDL_GetError() << std::endl;
    return false;
  }

  // 2. Nạp dữ liệu bản đồ từ file để server kiểm soát va chạm địa hình cứng
  // (AABB) và nội suy Z-height
  if (!mapData.LoadMap("assets/world_test.tmx")) {
    std::cerr << "Server: Failed to load map" << std::endl;
    return false;
  }

  // 3. Tạo socket lắng nghe (Listen Socket) trên cổng 5050
  serverSocket = NET_CreateServer(nullptr, 5050, 0);
  if (!serverSocket) {
    std::cerr << "Server: Failed to create server socket: " << SDL_GetError()
              << std::endl;
    return false;
  }

  std::cout << "Server: Listening on port 5050..." << std::endl;

  // 4. Sinh (Spawn) các quái vật tuần tra mặc định tại các tọa độ thế giới chỉ
  // định
  spawnNPC("Angry Orc", 400.0f, 300.0f);
  spawnNPC("Fierce Ogre", 1200.0f, 500.0f);
  spawnNPC("Skeleton Knight", 1800.0f, 900.0f);
  spawnNPC("Lizardman", 600.0f, 1100.0f);

  lastReplicationTime = SDL_GetTicks();
  return true;
}

// Khởi tạo một đối tượng quái vật mới trong danh sách quản lý
void ServerApp::spawnNPC(const char* name, float x, float y) {
  ServerNPC npc;
  npc.entityId = nextEntityId++;  // Cấp phát ID thực thể tăng dần
  std::strncpy(npc.name, name, sizeof(npc.name) - 1);
  npc.x = x;
  npc.y = y;
  // Nội suy cao độ Z thực tế của vị trí spawn trên bản đồ (sử dụng chân/tâm của
  // quái vật)
  npc.z = mapData.GetInterpolatedHeight(x + g_WorldConfig.pivotOffsetX,
                                        y + g_WorldConfig.pivotOffsetY, 0.0f);
  npc.spawnX = x;
  npc.spawnY = y;
  npc.hp = 60;
  npc.maxHp = 60;
  npcs.push_back(npc);  // Lưu vào danh sách NPC của Server
  std::cout << "Server: Spawned NPC " << name << " (ID: " << npc.entityId
            << ") at (" << x << ", " << y << ", " << npc.z << ")" << std::endl;
}

// Vòng lặp Server chính duy trì tần số ổn định 60 FPS
void ServerApp::run() {
  const int FPS = 60;
  const int frameDelay = 1000 / FPS;  // 16.67ms mỗi vòng lặp

  while (isRunning) {
    uint32_t frameStart = SDL_GetTicks();

    acceptNewClients();  // 1. Chấp nhận kết nối TCP mới
    handleClientData();  // 2. Nhận và xử lý gói tin mạng điều khiển từ người
                         // chơi
    updatePhysics();  // 3. Giả lập bước di chuyển vật lý của người chơi
    updateNPCs();     // 4. Giả lập AI quái vật tuần tra/aggro/tấn công
    replicateState();  // 5. Định kỳ đồng bộ trạng thái thế giới (20Hz) về các
                       // client
    removeDisconnectedClients();  // 6. Dọn dẹp kết nối của các client đã thoát

    uint32_t frameTime = SDL_GetTicks() - frameStart;
    // Cơ chế bù trừ ngủ (Delay) để duy trì tốc độ vòng lặp ổn định
    if (frameDelay > frameTime) {
      SDL_Delay(frameDelay - frameTime);
    }
  }
}

// Chấp nhận các kết nối TCP mới (Client kết nối) đưa vào danh sách clients
void ServerApp::acceptNewClients() {
  NET_StreamSocket* newSocket = nullptr;
  // Quét luồng chấp nhận kết nối TCP (không chặn luồng)
  while (NET_AcceptClient(serverSocket, &newSocket) && newSocket != nullptr) {
    auto conn = std::make_shared<ClientConnection>();
    conn->socket = newSocket;
    clients.push_back(conn);  // Thêm kết nối mới
    std::cout << "Server: New client connection accepted." << std::endl;
    newSocket = nullptr;  // Reset con trỏ cho vòng lặp tiếp theo
  }
}

// Đọc luồng byte từ tất cả client đang kết nối đưa vào bộ tích lũy gói
void ServerApp::handleClientData() {
  uint8_t tempBuf[1024];
  for (auto& client : clients) {
    // Đọc byte thô từ socket TCP của client này (không chặn luồng)
    int bytesRead =
        NET_ReadFromStreamSocket(client->socket, tempBuf, sizeof(tempBuf));

    if (bytesRead > 0) {
      client->buffer.Append(tempBuf, bytesRead);  // Đưa vào bộ đệm tích lũy

      uint16_t packetId;
      std::vector<uint8_t> packetData;
      // Bóc tách tất cả gói tin hoàn chỉnh có trong buffer và xử lý
      while (client->buffer.HasCompletePacket(packetId, packetData)) {
        processPacket(client, packetId, packetData);
      }
    } else if (bytesRead < 0) {
      // Nhận giá trị < 0 chỉ ra client ngắt kết nối socket
      std::cout << "Server: Client "
                << (client->entityId ? std::to_string(client->entityId)
                                     : "unknown")
                << " disconnected." << std::endl;

      // Nếu client đã login thành công, phát sóng gói tin Despawn thông báo cho
      // các client khác xóa sprite
      if (client->entityId > 0) {
        ServerEntityUpdateMsg
            selfMsg;  // Tránh lỗi biến chưa định nghĩa, gửi despawn trực tiếp
        ServerEntityDespawnMsg despawnMsg;
        despawnMsg.header.id = PacketID::SERVER_ENTITY_DESPAWN;
        despawnMsg.header.length = sizeof(despawnMsg);
        despawnMsg.entityId = client->entityId;
        broadcastPacket(
            &despawnMsg,
            sizeof(despawnMsg));  // Phát sóng tới toàn bộ người chơi khác
      }

      NET_DestroyStreamSocket(client->socket);  // Giải phóng socket mạng
      client->socket = nullptr;  // Đánh dấu để dọn dẹp ở cuối frame
    }
  }
}

// Giải mã cấu trúc và thực thi logic nghiệp vụ của từng gói tin gửi từ client
void ServerApp::processPacket(std::shared_ptr<ClientConnection>& client,
                              uint16_t id, const std::vector<uint8_t>& data) {
  // 1. Client yêu cầu đăng nhập: cấp phát ID, vị trí khởi đầu và đồng bộ các
  // thực thể khác
  if (id == PacketID::CLIENT_LOGIN) {
    if (data.size() < sizeof(ClientLoginMsg)) return;
    const auto* msg = reinterpret_cast<const ClientLoginMsg*>(data.data());

    client->entityId = nextEntityId++;  // Cấp phát ID duy nhất cho player mới
    std::strncpy(client->username, msg->username, sizeof(client->username) - 1);

    // Tọa độ khởi động mặc định tại trung tâm bản đồ
    client->x = 800.0f;
    client->y = 640.0f;
    client->z =
        mapData.GetInterpolatedHeight(client->x + g_WorldConfig.pivotOffsetX,
                                      client->y + g_WorldConfig.pivotOffsetY, 0.0f);
    client->hp = 100;
    client->maxHp = 100;
    client->notoriety = 0;  // Xanh dương: vô tội (Innocent)

    std::cout << "Server: Player '" << client->username << "' logged in as ID "
              << client->entityId << std::endl;

    // A. Gửi gói tin LoginAck phản hồi đồng ý đăng nhập kèm tọa độ khởi sinh về
    // cho chính client này
    ServerLoginAckMsg ack;
    ack.header.id = PacketID::SERVER_LOGIN_ACK;
    ack.header.length = sizeof(ack);
    ack.playerEntityId = client->entityId;
    ack.startX = client->x;
    ack.startY = client->y;
    ack.startZ = client->z;
    ack.mapWidth = mapData.GetWidth();
    ack.mapHeight = mapData.GetHeight();
    NET_WriteToStreamSocket(client->socket, &ack, sizeof(ack));

    // B. Gửi thông tin của tất cả những người chơi khác đang online về cho
    // người chơi mới đăng nhập này
    for (const auto& other : clients) {
      if (other->socket != nullptr && other->entityId > 0 &&
          other->entityId != client->entityId) {
        ServerEntityUpdateMsg otherMsg;
        otherMsg.header.id = PacketID::SERVER_ENTITY_UPDATE;
        otherMsg.header.length = sizeof(otherMsg);
        otherMsg.entityId = other->entityId;
        otherMsg.x = other->x;
        otherMsg.y = other->y;
        otherMsg.z = other->z;
        otherMsg.direction = other->direction;
        // Trả về đúng trạng thái (Ghost nếu chết, hoặc gửi state=2 nếu đối
        // phương đang bật War Mode)
        otherMsg.state =
            other->isDead ? 3 : (other->warMode ? 2 : other->state);
        otherMsg.notoriety = other->notoriety;
        std::strncpy(otherMsg.name, other->username, sizeof(otherMsg.name) - 1);
        otherMsg.currentHp = other->hp;
        otherMsg.maxHp = other->maxHp;
        NET_WriteToStreamSocket(client->socket, &otherMsg, sizeof(otherMsg));
      }
    }

    // C. Gửi thông tin của tất cả quái vật (NPCs) đang còn sống về cho người
    // chơi mới đăng nhập này
    for (const auto& npc : npcs) {
      if (!npc.isDead) {
        ServerEntityUpdateMsg npcMsg;
        npcMsg.header.id = PacketID::SERVER_ENTITY_UPDATE;
        npcMsg.header.length = sizeof(npcMsg);
        npcMsg.entityId = npc.entityId;
        npcMsg.x = npc.x;
        npcMsg.y = npc.y;
        npcMsg.z = npc.z;
        npcMsg.direction = npc.direction;
        npcMsg.state = npc.state;
        npcMsg.notoriety = npc.notoriety;
        std::strncpy(npcMsg.name, npc.name, sizeof(npcMsg.name) - 1);
        npcMsg.currentHp = npc.hp;
        npcMsg.maxHp = npc.maxHp;
        NET_WriteToStreamSocket(client->socket, &npcMsg, sizeof(npcMsg));
      }
    }
  }
  // 2. Client gửi yêu cầu di chuyển
  else if (id == PacketID::CLIENT_MOVE_REQUEST) {
    if (data.size() < sizeof(ClientMoveMsg)) return;
    const auto* msg = reinterpret_cast<const ClientMoveMsg*>(data.data());

    if (client->isDead)
      return;  // Hồn ma (Ghost) không thể di chuyển bình thường

    client->direction = msg->direction;
    client->lastMoveTime = SDL_GetTicks();
    client->state = 1;  // Đặt trạng thái thành di chuyển (Walking)
  }
  // 3. Client gửi tin nhắn chat
  else if (id == PacketID::CLIENT_CHAT_SEND) {
    if (data.size() < sizeof(ClientChatMsg)) return;
    const auto* msg = reinterpret_cast<const ClientChatMsg*>(data.data());

    std::cout << "Server: [" << client->username << "]: " << msg->text
              << std::endl;

    // Phát sóng (Broadcast) bong bóng chat này tới toàn bộ người chơi online để
    // hiển thị
    ServerChatBroadcastMsg chatMsg;
    chatMsg.header.id = PacketID::SERVER_CHAT_BROADCAST;
    chatMsg.header.length = sizeof(chatMsg);
    chatMsg.entityId = client->entityId;
    std::strncpy(chatMsg.text, msg->text, sizeof(chatMsg.text) - 1);
    broadcastPacket(&chatMsg, sizeof(chatMsg));
  }
  // 4. Client gửi yêu cầu hành động (War Mode, nhắm Target, Hồi sinh)
  else if (id == PacketID::CLIENT_ACTION_REQUEST) {
    if (data.size() < sizeof(ClientActionMsg)) return;
    const auto* msg = reinterpret_cast<const ClientActionMsg*>(data.data());

    if (msg->actionType == 0) {
      // A. Bật/Tắt chế độ chiến đấu (War Mode)
      client->warMode = !client->warMode;
      if (!client->warMode) {
        client->targetEntityId = 0;  // Hủy target nhắm đánh nếu tắt War Mode
      }
      std::cout << "Server: Player " << client->username
                << " warMode: " << (client->warMode ? "ON" : "OFF")
                << std::endl;
    } else if (msg->actionType == 1) {
      // B. Thiết lập mục tiêu tấn công (Nhấp chọn quái vật)
      client->targetEntityId = msg->targetEntityId;
      std::cout << "Server: Player " << client->username
                << " targets ID: " << client->targetEntityId << std::endl;
    } else if (msg->actionType == 2) {
      // C. Yêu cầu hồi sinh (Resurrect)
      if (client->isDead) {
        client->isDead = false;
        client->hp = client->maxHp / 2;  // Hồi sinh với 50% HP tối đa
        client->state = 0;               // Trở lại trạng thái Idle
        client->x = 800.0f;  // Dịch chuyển về shrine hồi sinh trung tâm
        client->y = 640.0f;
        std::cout << "Server: Player " << client->username << " resurrected."
                  << std::endl;
      }
    } else if (msg->actionType == 3) {
      // D. Chủ động tấn công bằng phím K helooooooooooooooo
      checkCombat(client);
    }
  }
}

// Giả lập chuyển động vật lý cho người chơi bám sát hướng đi yêu cầu và kiểm
// tra va chạm địa hình
void ServerApp::updatePhysics() {
  uint32_t now = SDL_GetTicks();
  for (auto& client : clients) {
    if (client->socket == nullptr || client->entityId == 0) continue;
    if (client->isDead) continue;  // Người chết không di chuyển vật lý

    if (client->state == 1) {
      // Sau 150ms nếu không nhận thêm gói tin di chuyển mới, chuyển trạng thái
      // về đứng yên (Idle)
      if (now - client->lastMoveTime > 150) {
        client->state = 0;  // Idle
      } else {
        // Quy đổi hướng đi (0-7) sang vector dịch chuyển Cartesian phẳng
        float dx = 0.0f;
        float dy = 0.0f;

        switch (client->direction) {
          case 0:  // Bắc Isometric (Di chuyển chéo góc lên-phải)
            dx = 1.0f;
            dy = -0.5f;
            break;
          case 1:  // Đông Bắc Isometric (Di chuyển thẳng ngang sang phải)
            dx = 1.0f;
            dy = 0.0f;
            break;
          case 2:  // Đông Isometric (Di chuyển chéo góc xuống-phải)
            dx = 1.0f;
            dy = 0.5f;
            break;
          case 3:  // Đông Nam Isometric (Di chuyển thẳng xuống dưới)
            dx = 0.0f;
            dy = 1.0f;
            break;
          case 4:  // Nam Isometric (Di chuyển chéo góc xuống-trái)
            dx = -1.0f;
            dy = 0.5f;
            break;
          case 5:  // Tây Nam Isometric (Di chuyển thẳng ngang sang trái)
            dx = -1.0f;
            dy = 0.0f;
            break;
          case 6:  // Tây Isometric (Di chuyển chéo góc lên-trái)
            dx = -1.0f;
            dy = -0.5f;
            break;
          case 7:  // Tây Bắc Isometric (Di chuyển thẳng đứng lên trên)
            dx = 0.0f;
            dy = -1.0f;
            break;
          default:
            break;
        }

        // Chuẩn hóa vector di chuyển về độ dài 1 (normalize)
        float length = std::hypot(dx, dy);
        if (length > 0.0f) {
          dx /= length;
          dy /= length;
        }

        // Tốc độ chạy nhanh 6px/frame hoặc đi bộ 3px/frame
        float speed = client->isRunning ? 6.0f : 3.0f;
        float nextX = client->x + dx * speed;
        float nextY = client->y + dy * speed;
        // Nội suy độ cao Z tại tọa độ mới để leo dốc mượt mà
        float nextZ =
            mapData.GetInterpolatedHeight(nextX + g_WorldConfig.pivotOffsetX,
                                          nextY + g_WorldConfig.pivotOffsetY, client->z);

        // Kiểm tra va chạm hộp (AABB) tại vị trí đích:
        // Nếu không bị cản bởi tường phẳng và chênh lệch chiều cao Z nằm trong
        // khoảng cho phép: Cập nhật vị trí mới
        if (!mapData.IsBlocked(nextX + g_WorldConfig.colliderOffsetX,
                               nextY + g_WorldConfig.colliderOffsetY, client->z,
                               g_WorldConfig.colliderWidth,
                               g_WorldConfig.colliderHeight)) {
          client->x = nextX;
          client->y = nextY;
          client->z = nextZ;  // Cập nhật độ cao Z
        }
      }
    }
  }
}

// Xử lý đòn đánh của người chơi lên quái vật mục tiêu
void ServerApp::checkCombat(std::shared_ptr<ClientConnection>& client) {
  // Đòn đánh chỉ hợp lệ khi: Người chơi còn sống, đang bật War Mode, và đã nhắm
  // mục tiêu hợp lệ
  if (client->isDead || !client->warMode || client->targetEntityId == 0) return;

  uint32_t ticks = SDL_GetTicks();
  // Kiểm tra thời gian hồi chiêu đánh (1.2 giây)
  if (ticks - client->lastAttackTime < 1200) return;

  // Tìm kiếm quái vật mục tiêu trong danh sách NPC
  for (auto& npc : npcs) {
    if (npc.entityId == client->targetEntityId && !npc.isDead) {
      // Kiểm tra khoảng cách đứng đánh (phạm vi cận chiến cực đại 80.0px)
      float dist = std::hypot(client->x - npc.x, client->y - npc.y);
      if (dist <= 80.0f) {
        client->lastAttackTime = ticks;  // Đánh dấu thời điểm đánh

        // Tính toán sát thương ngẫu nhiên (từ 10 đến 17 damage)
        int32_t dmg = 10 + (std::rand() % 8);
        npc.hp -= dmg;  // Trừ máu quái vật

        std::cout << "Server: Player ID " << client->entityId << " hit NPC "
                  << npc.name << " for " << dmg << " damage. NPC HP: " << npc.hp
                  << std::endl;

        // Phát sóng sự kiện combat cho toàn bộ client vẽ số hiển thị sát thương
        // hoặc hiệu ứng đánh
        ServerCombatEventMsg eventMsg;
        eventMsg.header.id = PacketID::SERVER_COMBAT_EVENT;
        eventMsg.header.length = sizeof(eventMsg);
        eventMsg.attackerId = client->entityId;
        eventMsg.targetId = npc.entityId;
        eventMsg.damage = dmg;
        broadcastPacket(&eventMsg, sizeof(eventMsg));

        // Nếu quái vật hết máu: xử lý chết và despawn
        if (npc.hp <= 0) {
          npc.hp = 0;
          npc.isDead = true;
          npc.deathTime =
              ticks;  // Lưu mốc thời gian chết để tính giờ hồi sinh sau 10s
          std::cout << "Server: NPC " << npc.name << " died." << std::endl;

          // Phát sóng gói tin despawn yêu cầu client xóa thực thể quái này khỏi
          // màn hình
          ServerEntityDespawnMsg despawnMsg;
          despawnMsg.header.id = PacketID::SERVER_ENTITY_DESPAWN;
          despawnMsg.header.length = sizeof(despawnMsg);
          despawnMsg.entityId = npc.entityId;
          broadcastPacket(&despawnMsg, sizeof(despawnMsg));

          client->targetEntityId =
              0;  // Xóa mục tiêu khỏi người chơi vừa hạ gục quái
        }
      }
      break;
    }
  }
}

// Cập nhật logic hành vi của quái vật (NPCs) và gọi kiểm tra đòn đánh của người
// chơi
void ServerApp::updateNPCs() {
  uint32_t ticks = SDL_GetTicks();

  // 1. Không tự động gọi checkCombat nữa để chuyển sang cơ chế nhấn K chủ động

  // 2. Chạy logic AI cho từng con quái vật trong danh sách
  for (auto& npc : npcs) {
    if (npc.isDead) {
      // Nếu quái đã chết: Đợi đủ 10 giây (10000ms) để thực hiện hồi sinh
      // (Respawn)
      if (ticks - npc.deathTime > 10000) {
        npc.isDead = false;
        npc.hp = npc.maxHp;
        npc.x = npc.spawnX;  // Hồi sinh lại tại vị trí spawn gốc
        npc.y = npc.spawnY;
        std::cout << "Server: Respawned NPC " << npc.name << " at (" << npc.x
                  << ", " << npc.y << ")" << std::endl;
      }
      continue;
    }

    // A. Cơ chế Aggro (Phát hiện mục tiêu): Tìm kiếm người chơi gần nhất trong
    // phạm vi 250px
    std::shared_ptr<ClientConnection> targetPlayer = nullptr;
    float minDist = 250.0f;

    for (auto& client : clients) {
      if (client->socket != nullptr && !client->isDead) {
        float dist = std::hypot(npc.x - client->x, npc.y - client->y);
        if (dist < minDist) {
          minDist = dist;
          targetPlayer = client;  // Mục tiêu đuổi theo là người chơi gần nhất
        }
      }
    }

    // B. Thực hiện đuổi theo và tấn công người chơi nếu tìm thấy mục tiêu
    if (targetPlayer) {
      npc.targetPlayerId = targetPlayer->entityId;
      npc.state = 1;  // Trạng thái di chuyển (Walking) để áp sát

      float dx = targetPlayer->x - npc.x;
      float dy = targetPlayer->y - npc.y;
      float len = std::hypot(dx, dy);

      // Nếu khoảng cách lớn hơn tầm đánh 50px: tiếp tục di chuyển xáp lại gần
      if (len > 50.0f) {
        dx /= len;
        dy /= len;
        float step = 2.0f;  // Tốc độ di chuyển đuổi theo của quái (2px/frame)

        float nextX = npc.x + dx * step;
        float nextY = npc.y + dy * step;
        float nextZ =
            mapData.GetInterpolatedHeight(nextX + g_WorldConfig.pivotOffsetX,
                                          nextY + g_WorldConfig.pivotOffsetY, npc.z);

        // Kiểm tra va chạm địa hình cứng trước khi di chuyển quái vật
        if (!mapData.IsBlocked(nextX + g_WorldConfig.colliderOffsetX,
                               nextY + g_WorldConfig.colliderOffsetY, npc.z,
                               g_WorldConfig.colliderWidth,
                               g_WorldConfig.colliderHeight)) {
          npc.x = nextX;
          npc.y = nextY;
          npc.z = nextZ;
        }
      }
      // Nếu đã nằm trong tầm đánh (khoảng cách <= 50.0px): Chuyển trạng thái
      // sang Attack (2) và ra đòn
      else {
        npc.state = 2;
        // Đòn đánh có cooldown 1.5 giây (1500ms)
        if (ticks - npc.lastAttackTime > 1500) {
          npc.lastAttackTime = ticks;
          // Quái vật gây sát thương ngẫu nhiên (từ 5 đến 10 damage) lên người
          // chơi
          int32_t dmg = 5 + (std::rand() % 6);
          targetPlayer->hp -= dmg;  // Trừ HP của người chơi

          std::cout << "Server: NPC " << npc.name << " hit Player ID "
                    << targetPlayer->entityId << " for " << dmg
                    << " damage. Player HP: " << targetPlayer->hp << std::endl;

          // Phát sóng sự kiện combat cho các client vẽ
          ServerCombatEventMsg eventMsg;
          eventMsg.header.id = PacketID::SERVER_COMBAT_EVENT;
          eventMsg.header.length = sizeof(eventMsg);
          eventMsg.attackerId = npc.entityId;
          eventMsg.targetId = targetPlayer->entityId;
          eventMsg.damage = dmg;
          broadcastPacket(&eventMsg, sizeof(eventMsg));

          // Nếu HP của người chơi giảm về 0: xử lý chết và chuyển sang chế độ
          // hồn ma (Ghost)
          if (targetPlayer->hp <= 0) {
            targetPlayer->hp = 0;
            targetPlayer->isDead = true;
            targetPlayer->state = 3;  // Ghost mode
            targetPlayer->lastDeathTime = ticks;
            std::cout << "Server: Player ID " << targetPlayer->entityId
                      << " died." << std::endl;
          }
        }
      }
    }
    // C. Nếu không có người chơi nào trong Aggro range: quái vật tự động đi
    // lang thang (Wander) tuần tra ngẫu nhiên
    else {
      npc.targetPlayerId = 0;
      // Định kỳ mỗi 3 giây (3000ms), quái vật tự chọn một hành vi mới ngẫu
      // nhiên
      if (ticks - npc.lastMoveTime > 3000) {
        npc.lastMoveTime = ticks;
        if (std::rand() % 2 == 0) {
          npc.state = 0;  // 50% cơ hội quái đứng yên nghỉ ngơi (Idle)
        } else {
          npc.state = 1;  // 50% cơ hội quái di chuyển lang thang
          npc.direction = std::rand() %
                          8;  // Chọn 1 hướng ngẫu nhiên trong 8 hướng di chuyển
        }
      }

      // Nếu quái đang đi lang thang tuần tra:
      if (npc.state == 1) {
        float dx = 0.0f, dy = 0.0f;

        switch (npc.direction) {
          case 0:
            dx = 0.0f;
            dy = -1.0f;
            break;  // Bắc
          case 1:
            dx = 1.0f;
            dy = -1.0f;
            break;  // Đông Bắc
          case 2:
            dx = 1.0f;
            dy = 0.0f;
            break;  // Đông
          case 3:
            dx = 1.0f;
            dy = 1.0f;
            break;  // Đông Nam
          case 4:
            dx = 0.0f;
            dy = 1.0f;
            break;  // Nam
          case 5:
            dx = -1.0f;
            dy = 1.0f;
            break;  // Tây Nam
          case 6:
            dx = -1.0f;
            dy = 0.0f;
            break;  // Tây
          case 7:
            dx = -1.0f;
            dy = -1.0f;
            break;  // Tây Bắc
          default:
            break;
        }

        // Chuẩn hóa vector di chuyển
        float length = std::hypot(dx, dy);
        if (length > 0.0f) {
          dx /= length;
          dy /= length;
        }

        // Quái vật đi tuần thong thả với tốc độ 1.5px/frame
        float nextX = npc.x + dx * 1.5f;
        float nextY = npc.y + dy * 1.5f;
        float nextZ =
            mapData.GetInterpolatedHeight(nextX + g_WorldConfig.pivotOffsetX,
                                          nextY + g_WorldConfig.pivotOffsetY, npc.z);

        // Kiểm tra va chạm địa hình trước khi bước đi
        if (!mapData.IsBlocked(nextX + g_WorldConfig.colliderOffsetX,
                               nextY + g_WorldConfig.colliderOffsetY, npc.z,
                               g_WorldConfig.colliderWidth,
                               g_WorldConfig.colliderHeight)) {
          npc.x = nextX;
          npc.y = nextY;
          npc.z = nextZ;
        } else {
          npc.state = 0;  // Nếu bị kẹt/chặn bởi tường: dừng di chuyển chuyển về
                          // đứng yên
        }
      }
    }
  }
}

// Đồng bộ hóa (Replication) trạng thái của toàn bộ thực thể về cho mọi người
// chơi online (tần số 20Hz - 50ms)
void ServerApp::replicateState() {
  uint32_t ticks = SDL_GetTicks();
  if (ticks - lastReplicationTime < 50) return;  // Cooldown đồng bộ 50ms
  lastReplicationTime = ticks;

  // 1. Gửi thông tin trạng thái cập nhật của tất cả các Player đang online
  for (const auto& client : clients) {
    if (client->socket != nullptr && client->entityId > 0) {
      ServerEntityUpdateMsg selfMsg;
      selfMsg.header.id = PacketID::SERVER_ENTITY_UPDATE;
      selfMsg.header.length = sizeof(selfMsg);
      selfMsg.entityId = client->entityId;
      selfMsg.x = client->x;
      selfMsg.y = client->y;
      selfMsg.z = client->z;  // Truyền độ cao Z
      selfMsg.direction = client->direction;
      // Trạng thái: Nếu chết là 3 (Ghost), nếu bật War Mode chiến đấu là 2,
      // ngược lại gửi trạng thái đi bộ/đứng yên
      selfMsg.state =
          client->isDead ? 3 : (client->warMode ? 2 : client->state);
      selfMsg.notoriety = client->isDead ? 1 : client->notoriety;
      std::strncpy(selfMsg.name, client->username, sizeof(selfMsg.name) - 1);
      selfMsg.currentHp = client->hp;
      selfMsg.maxHp = client->maxHp;
      broadcastPacket(&selfMsg, sizeof(selfMsg));  // Phát sóng tới mọi người
    }
  }

  // 2. Gửi thông tin trạng thái cập nhật của toàn bộ các quái vật NPC còn sống
  for (const auto& npc : npcs) {
    if (!npc.isDead) {
      ServerEntityUpdateMsg npcMsg;
      npcMsg.header.id = PacketID::SERVER_ENTITY_UPDATE;
      npcMsg.header.length = sizeof(npcMsg);
      npcMsg.entityId = npc.entityId;
      npcMsg.x = npc.x;
      npcMsg.y = npc.y;
      npcMsg.z = npc.z;  // Truyền độ cao Z
      npcMsg.direction = npc.direction;
      npcMsg.state = npc.state;
      npcMsg.notoriety = npc.notoriety;
      std::strncpy(npcMsg.name, npc.name, sizeof(npcMsg.name) - 1);
      npcMsg.currentHp = npc.hp;
      npcMsg.maxHp = npc.maxHp;
      broadcastPacket(&npcMsg, sizeof(npcMsg));  // Phát sóng tới mọi người
    }
  }
}

// Gửi một gói dữ liệu thô tới tất cả client đang kết nối hợp lệ
void ServerApp::broadcastPacket(const void* data, size_t size) {
  for (auto& client : clients) {
    if (client->socket != nullptr && client->entityId > 0) {
      // Ghi gói tin trực tiếp lên luồng socket TCP của client
      NET_WriteToStreamSocket(client->socket, data, static_cast<int>(size));
    }
  }
}

// Loại bỏ những client đã ngắt kết nối (socket bằng nullptr) ra khỏi danh sách
// clients
void ServerApp::removeDisconnectedClients() {
  std::erase_if(clients, [](const std::shared_ptr<ClientConnection>& conn) {
    return conn->socket == nullptr;
  });
}

// Dọn dẹp đóng các socket và ngắt kết nối mạng khi tắt Server
void ServerApp::clean() {
  // 1. Đóng kết nối của toàn bộ client
  for (auto& client : clients) {
    if (client->socket) {
      NET_DestroyStreamSocket(client->socket);
      client->socket = nullptr;
    }
  }
  clients.clear();

  // 2. Đóng socket lắng nghe chính của Server
  if (serverSocket) {
    NET_DestroyServer(serverSocket);
    serverSocket = nullptr;
  }

  // 3. Tắt subsystem mạng của SDL
  NET_Quit();
  std::cout << "Server: Shutdown complete." << std::endl;
}
