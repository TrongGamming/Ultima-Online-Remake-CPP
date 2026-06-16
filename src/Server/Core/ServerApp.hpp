#pragma once
#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>

#include <memory>
#include <vector>

#include "Common/Network/NetworkBuffer.hpp"
#include "Common/World/MapData.hpp"

// Cấu trúc quản lý thông tin kết nối và trạng thái của mỗi người chơi (Client) trên Server
struct ClientConnection {
  NET_StreamSocket* socket = nullptr; // Socket TCP kết nối tới Client
  NetworkBuffer buffer;               // Bộ đệm tích lũy dữ liệu mạng nhận được từ client này
  uint32_t entityId = 0;              // ID thực thể duy nhất cấp phát cho người chơi này
  char username[32] = "";             // Tên nhân vật đăng ký
  
  // Tọa độ vị trí hiện tại trong thế giới game (Cartesian phẳng)
  float x = 800.0f;
  float y = 640.0f;
  float z = 0.0f; // Cao độ hiện tại (trục Z) dựa trên địa hình bên dưới chân
  
  // Tọa độ đích của lần di chuyển trước (dùng cho vật lý/phát hiện kẹt)
  float targetX = 800.0f;
  float targetY = 640.0f;
  
  uint8_t direction = 4;  // Hướng quay mặt hiện tại (mặc định 4: Nam)
  uint8_t state = 0;      // Trạng thái (0: Idle, 1: Walk, 2: Attack, 3: Ghost)
  uint8_t notoriety = 0;  // Danh tiếng (0: Xanh dương - Innocent, 1: Xám - Criminal, 2: Đỏ - Murderer)
  int32_t hp = 100;       // Máu hiện tại
  int32_t maxHp = 100;    // Máu tối đa
  bool isRunning = false; // Cờ báo chạy nhanh (chưa sử dụng nhiều)
  bool warMode = false;   // Chế độ sẵn sàng chiến đấu (WAR Mode)
  
  uint32_t lastAttackTime = 0; // Thời điểm ra đòn đánh cuối cùng (để tính cooldown tốc độ đánh 1.2s)
  uint32_t targetEntityId = 0; // ID thực thể quái vật đang nhắm chọn tấn công
  bool isDead = false;         // Cờ báo người chơi đã chết
  uint32_t lastDeathTime = 0;  // Thời điểm chết
  uint32_t lastMoveTime = 0;   // Thời điểm di chuyển cuối cùng để chuyển về Idle sau 150ms nếu dừng
};

// Cấu trúc quản lý trạng thái của mỗi quái vật (NPC) trên Server
struct ServerNPC {
  uint32_t entityId = 0;      // ID duy nhất của quái vật
  char name[32] = "Angry Orc"; // Tên quái vật
  
  // Tọa độ vị trí hiện tại của quái vật (Cartesian phẳng)
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f; // Cao độ hiện tại (Z)
  
  float targetX = 0.0f;
  float targetY = 0.0f;
  
  uint8_t direction = 4;      // Hướng di chuyển (0-7)
  uint8_t state = 0;          // Trạng thái hoạt động (0: Idle, 1: Walking, 2: Attacking)
  uint8_t notoriety = 2;      // Khởi tạo notoriety mặc định bằng 2 (Red - màu đỏ chủ động tấn công)
  int32_t hp = 50;            // Máu hiện tại
  int32_t maxHp = 50;         // Máu tối đa
  
  uint32_t lastMoveTime = 0;   // Mốc thời gian di chuyển lang thang tuần tra cuối cùng
  uint32_t lastAttackTime = 0; // Mốc thời gian ra đòn đánh người chơi cuối cùng (cooldown 1.5s)
  uint32_t targetPlayerId = 0;  // ID thực thể của người chơi đang bị quái đuổi đánh (0 = không có)
  
  // Tọa độ điểm spawn hồi sinh gốc
  float spawnX = 0.0f;
  float spawnY = 0.0f;
  
  bool isDead = false;        // Cờ báo quái đã chết
  uint32_t deathTime = 0;     // Thời điểm chết (để đếm ngược hồi sinh sau 10s)
};

// Lớp ServerApp là nhân lõi của Game Server.
// Quản lý việc lắng nghe kết nối TCP mới, xử lý các packet điều khiển từ client,
// giả lập vật lý di chuyển cứng và dốc, chạy AI quái đuổi đánh người chơi/lang thang tuần tra,
// và định kỳ đồng bộ (replicate) trạng thái của toàn bộ thực thể về cho tất cả client.
class ServerApp {
 public:
  ServerApp();
  ~ServerApp();

  // Khởi tạo SDL_net, nạp bản đồ đĩa, tạo socket lắng nghe trên port 5050 và spawn sẵn quái
  bool init();
  
  // Vòng lặp Server chính (duy trì ổn định tần số 60Hz)
  void run();
  
  // Đóng socket lắng nghe, ngắt toàn bộ kết nối của client và tắt SDL_net
  void clean();

 private:
  void acceptNewClients(); // Chấp nhận các kết nối TCP mới từ các client
  void handleClientData(); // Định kỳ đọc byte từ tất cả client sockets, tích lũy bộ đệm và xử lý packet
  
  // Giải mã và thực thi logic của các packet điều khiển từ client cụ thể
  void processPacket(std::shared_ptr<ClientConnection>& client, uint16_t id,
                     const std::vector<uint8_t>& data);
  
  void updatePhysics(); // Giả lập bước di chuyển của người chơi dựa trên hướng đi, tốc độ và kiểm tra va chạm địa hình IsBlocked
  void updateNPCs();    // Giả lập AI quái vật: dò người chơi gần (Aggro), di chuyển tiếp cận, tấn công hoặc tự đi lang thang tuần tra
  void replicateState(); // Đồng bộ trạng thái toàn bộ thực thể (vị trí x, y, z, hp, state) về cho mọi client với tần số 20Hz (mỗi 50ms)
  void broadcastPacket(const void* data, size_t size); // helper gửi một gói tin tới tất cả các client đang kết nối
  void removeDisconnectedClients();                    // Dọn dẹp các đối tượng kết nối của các client đã ngắt kết nối
  
  void spawnNPC(const char* name, float x, float y);              // Helper tạo mới một quái vật NPC trên bản đồ
  void checkCombat(std::shared_ptr<ClientConnection>& client);   // Xử lý đòn đánh của người chơi lên quái vật mục tiêu (cooldown, range, dmg)

  NET_Server* serverSocket = nullptr; // Socket lắng nghe kết nối TCP trên port 5050
  std::vector<std::shared_ptr<ClientConnection>> clients; // Danh sách quản lý kết nối các người chơi
  std::vector<ServerNPC> npcs;                            // Danh sách quản lý quái vật NPC
  MapData mapData;                                        // Dữ liệu bản đồ nạp để tính va chạm địa hình và Z-height
  bool isRunning = true;                                  // Trạng thái vòng lặp Server
  uint32_t nextEntityId = 1;                              // Bộ đếm cấp phát ID thực thể duy nhất tiếp theo
  uint32_t lastReplicationTime = 0;                       // Mốc thời gian thực hiện đồng bộ gói tin cuối cùng
};
