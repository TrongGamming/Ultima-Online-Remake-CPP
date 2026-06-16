#include "Server/Core/ServerApp.hpp"
#include <iostream>
#include <memory>

// Điểm khởi chạy chương trình Game Server (Hàm main)
int main(int argc, char* argv[]) {
    std::cout << "Starting Ultima Online Remake Server..." << std::endl;
    
    // Sử dụng unique_ptr để quản lý vòng đời ứng dụng ServerApp tự động giải phóng bộ nhớ khi tắt
    auto app = std::make_unique<ServerApp>();
    
    // Khởi tạo Server: Tải bản đồ, tạo listen socket port 5050 và spawn sẵn các quái NPC tuần tra
    if (!app->init()) {
        std::cerr << "Failed to initialize server application." << std::endl;
        return 1; // Thoát lỗi nếu khởi tạo socket hoặc tải map thất bại
    }
    
    // Chạy vòng lặp Server chính (Game Server Loop) xử lý kết nối, packet mạng, vật lý, AI quái tuần tra
    app->run();
    
    // Thực hiện dọn dẹp tài nguyên, đóng cổng socket lắng nghe khi tắt server
    app->clean();
    
    return 0;
}
