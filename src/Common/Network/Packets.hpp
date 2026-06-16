#pragma once
#include <cstdint>

// Đảm bảo compiler đóng gói các struct sát nhau không có phần đệm (1-byte alignment).
// Điều này cực kỳ quan trọng đối với truyền tải dữ liệu qua mạng để đảm bảo cấu trúc byte đồng nhất giữa Client và Server.
#pragma pack(push, 1)

// Cấu trúc Header chung của mọi gói tin mạng (Packet)
struct PacketHeader {
    uint16_t id;      // Loại gói tin (Packet Type ID)
    uint16_t length;  // Tổng chiều dài của toàn bộ gói tin bằng byte (bao gồm cả Header)
};

// Định nghĩa các hằng số định danh loại gói tin mạng
namespace PacketID {
    constexpr uint16_t CLIENT_LOGIN          = 1; // Client -> Server: Yêu cầu đăng nhập
    constexpr uint16_t SERVER_LOGIN_ACK      = 2; // Server -> Client: Chấp nhận đăng nhập và gửi thông tin khởi tạo
    constexpr uint16_t CLIENT_MOVE_REQUEST   = 3; // Client -> Server: Yêu cầu di chuyển nhân vật
    constexpr uint16_t SERVER_ENTITY_UPDATE  = 4; // Server -> Client: Cập nhật vị trí, HP, state của thực thể khác hoặc chính mình
    constexpr uint16_t SERVER_ENTITY_DESPAWN = 5; // Server -> Client: Xóa thực thể khỏi màn hình (do quái chết hoặc player logout)
    constexpr uint16_t CLIENT_CHAT_SEND      = 6; // Client -> Server: Gửi tin nhắn chat
    constexpr uint16_t SERVER_CHAT_BROADCAST = 7; // Server -> Client: Phát tán tin nhắn chat của một ai đó tới mọi người
    constexpr uint16_t CLIENT_ACTION_REQUEST = 8; // Client -> Server: Yêu cầu hành động (Bật War Mode, chọn Target, Hồi sinh)
    constexpr uint16_t SERVER_COMBAT_EVENT   = 9; // Server -> Client: Thông báo sự kiện chiến đấu (ai đánh ai và bao nhiêu sát thương)
}

// 1. Gói tin đăng nhập: Client gửi tên người dùng lên Server
struct ClientLoginMsg {
    PacketHeader header; // Header gói tin
    char username[32];   // Tên người chơi (tối đa 31 ký tự + ký tự null kết thúc)
};

// 3. Gói tin di chuyển: Client gửi hướng muốn di chuyển lên Server
struct ClientMoveMsg {
    PacketHeader header;
    uint8_t direction; // Hướng di chuyển (0-7 từ Bắc, Đông Bắc, đến Tây Bắc)
};

// 6. Gói tin gửi chat: Client gửi nội dung tin nhắn dạng text lên Server
struct ClientChatMsg {
    PacketHeader header;
    char text[128]; // Nội dung tin nhắn chat (tối đa 127 ký tự)
};

// 8. Gói tin hành động: Client yêu cầu thực hiện hành động cụ thể
struct ClientActionMsg {
    PacketHeader header;
    uint8_t actionType; // Kiểu hành động: 0: Đổi chế độ chiến đấu (War/Normal), 1: Thiết lập mục tiêu tấn công, 2: Hồi sinh
    uint32_t targetEntityId; // ID của mục tiêu bị chọn tấn công (nếu actionType là 1)
};

// 2. Gói tin chấp nhận login: Server gửi lại thông tin vị trí xuất phát và kích thước bản đồ cho Client
struct ServerLoginAckMsg {
    PacketHeader header;
    uint32_t playerEntityId; // ID thực thể được Server cấp cho người chơi này
    float startX;            // Tọa độ X khởi đầu thế giới
    float startY;            // Tọa độ Y khởi đầu thế giới
    float startZ;            // Tọa độ Z (độ cao địa hình) khởi đầu thế giới
    int32_t mapWidth;        // Chiều rộng bản đồ (số ô gạch)
    int32_t mapHeight;       // Chiều cao bản đồ (số ô gạch)
};

// 4. Gói tin cập nhật thực thể: Server định kỳ đồng bộ vị trí và thông số của quái hoặc người chơi khác
struct ServerEntityUpdateMsg {
    PacketHeader header;
    uint32_t entityId;   // ID của thực thể được cập nhật
    float x;             // Vị trí thế giới trục X
    float y;             // Vị trí thế giới trục Y
    float z;             // Vị trí thế giới trục Z (độ cao)
    uint8_t direction;   // Hướng quay mặt hiện tại (0-7)
    uint8_t state;       // Trạng thái hiện tại (0: Idle, 1: Walking, 2: Attacking/War Mode, 3: Ghost)
    uint8_t notoriety;   // Độ danh tiếng (0: Xanh dương - Vô tội, 1: Xám - Tội phạm, 2: Đỏ - Kẻ sát nhân)
    char name[32];       // Tên hiển thị của thực thể
    int32_t currentHp;   // Lượng máu hiện tại
    int32_t maxHp;       // Lượng máu tối đa
};

// 5. Gói tin xóa thực thể: Server báo biến mất thực thể (quái vật chết, người chơi logout...)
struct ServerEntityDespawnMsg {
    PacketHeader header;
    uint32_t entityId; // ID thực thể cần xóa khỏi màn hình
};

// 7. Gói tin phát chat: Server gửi tin nhắn của một thực thể cụ thể đến các client khác để hiện bong bóng chat
struct ServerChatBroadcastMsg {
    PacketHeader header;
    uint32_t entityId; // ID của thực thể nói câu này
    char text[128];    // Nội dung câu nói
};

// 9. Gói tin combat: Server thông báo có sát thương xảy ra để client vẽ hiệu ứng hoặc in log
struct ServerCombatEventMsg {
    PacketHeader header;
    uint32_t attackerId; // ID của kẻ tấn công
    uint32_t targetId;   // ID của nạn nhân nhận sát thương
    int32_t damage;      // Số lượng máu bị trừ
};

// Khôi phục lại trạng thái alignment ban đầu của compiler
#pragma pack(pop)
