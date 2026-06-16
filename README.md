# Ultima Online Remake (OUGame) - C++20 & SDL3 Multiplayer Game Engine

Dự án này là một phiên bản làm lại (remake) của tựa game huyền thoại **Ultima Online**, được xây dựng từ đầu bằng ngôn ngữ **C++20** và thư viện **SDL3** (Simple DirectMedia Layer phiên bản 3 mới nhất). Dự án thể hiện khả năng thiết kế kiến trúc game engine, lập trình hệ thống, tối ưu hóa hiệu năng, xử lý mạng (multiplayer) và phát triển đa nền tảng (PC & Android) của nhà phát triển.

---

## 🌟 Tính Năng Nổi Bật (Key Features)

*   **Kiến trúc Client - Server Độc Lập**: 
    *   **Server**: Nhận kết nối từ nhiều Client, xử lý logic thế giới, AI tuần tra của quái NPC, đồng bộ hóa trạng thái thực thể thông qua giao thức TCP (sử dụng thư viện mạng `SDL3_net`).
    *   **Client**: Nhận input từ người chơi, hiển thị đồ họa 2D qua Camera, thực hiện nội suy (interpolation) và tương tác với thế giới thực tế thông qua socket mạng.
*   **Hệ thống ECS (Entity Component System) tự thiết kế**: Hệ thống quản lý thực thể (Entities), thành phần dữ liệu (Components), và các bộ xử lý logic (Systems) hiệu năng cao, giúp tách biệt dữ liệu và logic, dễ dàng bảo trì và mở rộng tính năng mới.
*   **Quản lý Tài Nguyên (Asset Management)**: Thiết kế mẫu `AssetManager` và `TextureManager` quản lý vòng đời của hình ảnh (Texture), font chữ (TTF), giúp tránh rò rỉ bộ nhớ (memory leaks) bằng cách tận dụng con trỏ thông minh (`std::unique_ptr`, `std::shared_ptr`) và cơ chế RAII.
*   **Tiled Map Engine**: Sử dụng thư viện `tinyxml2` để phân tích (parse) và render bản đồ lưới 2D từ các tệp cấu hình XML/TMX.
*   **FPS Limiter & Game Loop**: Triển khai Game Loop chuẩn công nghiệp với bộ giới hạn khung hình (FPS Limiter) giúp duy trì hiệu năng ổn định ở mức 60 FPS mà không làm quá tải CPU.
*   **Hỗ trợ Đa Nền tảng (Cross-platform)**: Hỗ trợ biên dịch và chạy trên PC (Windows, Linux, macOS) và đóng gói sang ứng dụng Android (APK) thông qua Android NDK tích hợp sẵn.

---

## 🛠️ Công Nghệ & Thư Viện Sử Dụng (Tech Stack)

*   **Ngôn ngữ**: C++20 (sử dụng các tính năng hiện đại như Smart Pointers, STL nâng cao, constexpr...).
*   **Build System**: CMake (>= 3.22) với cơ chế `FetchContent` giúp tự động tải và biên dịch các thư viện dependency mà không cần cài đặt thủ công.
*   **Đồ họa & Sự kiện**: [SDL3 (Simple DirectMedia Layer v3.2.0)](https://github.com/libsdl-org/SDL).
*   **Xử lý UI & Font**: [SDL3_ttf](https://github.com/libsdl-org/SDL_ttf) & [SDL3_image](https://github.com/libsdl-org/SDL_image).
*   **Hệ thống Mạng**: [SDL3_net](https://github.com/libsdl-org/SDL_net) (hỗ trợ TCP/UDP socket phi chặn).
*   **Parser XML**: [tinyxml2](https://github.com/leethomason/tinyxml2) để đọc cấu hình map.
*   **Hệ điều hành mục tiêu**: Windows, Linux, macOS, Android.

---

## 📂 Cấu Trúc Dự Án (Project Structure)

Dự án được cấu trúc rõ ràng theo chuẩn mô hình Client-Server và ECS:

*   [CMakeLists.txt](file:///d:/WorkSpace/Game/OU/CMakeLists.txt): Tệp cấu hình CMake chính điều phối việc fetch các thư viện dependencies, định nghĩa thư viện dùng chung `OUCommon` và phân chia 2 build target: `OUGameClient` và `OUGameServer`.
*   [src/](file:///d:/WorkSpace/Game/OU/src): Thư mục chứa toàn bộ mã nguồn của dự án:
    *   [src/Common/](file:///d:/WorkSpace/Game/OU/src/Common): Chứa cấu hình thế giới (`WorldConfig`), mã hóa gói tin và các tiện ích dùng chung cho cả Client và Server.
    *   [src/ECS/](file:///d:/WorkSpace/Game/OU/src/ECS): Hệ thống Entity Component System tự xây dựng ([ECS.hpp](file:///d:/WorkSpace/Game/OU/src/ECS/ECS.hpp)) và các Components định nghĩa thuộc tính nhân vật, đồ họa, vị trí, mạng lưới.
    *   [src/Core/](file:///d:/WorkSpace/Game/OU/src/Core): Lõi của Game Client ([Game.cpp](file:///d:/WorkSpace/Game/OU/src/Core/Game.cpp), [Game.hpp](file:///d:/WorkSpace/Game/OU/src/Core/Game.hpp)), quản lý tài nguyên ([AssetManager](file:///d:/WorkSpace/Game/OU/src/Core/AssetManager.cpp)), Camera di chuyển theo người chơi.
    *   [src/World/](file:///d:/WorkSpace/Game/OU/src/World): Quản lý cơ sở dữ liệu bản đồ ([Map.cpp](file:///d:/WorkSpace/Game/OU/src/World/Map.cpp)) tải từ XML.
    *   [src/Server/](file:///d:/WorkSpace/Game/OU/src/Server): Mã nguồn cho Game Server ([main.cpp](file:///d:/WorkSpace/Game/OU/src/Server/main.cpp)) chịu trách nhiệm lắng nghe kết nối, xử lý logic vật lý, quái vật NPC và đồng bộ hóa đa người chơi.
    *   [src/main.cpp](file:///d:/WorkSpace/Game/OU/src/main.cpp): Điểm khởi chạy của Game Client.
*   [android-project/](file:///d:/WorkSpace/Game/OU/android-project): Cấu hình dự án Android (Gradle, JNI, Manifest) phục vụ đóng gói APK.
*   [assets/](file:///d:/WorkSpace/Game/OU/assets): Tài nguyên hình ảnh (textures), font chữ (ttf), bản đồ dạng XML.

---

## 🚀 Hướng Dẫn Biên Dịch & Chạy (Build Guide)

### Yêu Cầu Hệ Thống (Prerequisites)
*   **CMake**: Phiên bản 3.22 trở lên.
*   **C++ Compiler**: Trình biên dịch hỗ trợ chuẩn C++20 (MSVC trên Windows, GCC hoặc Clang trên Linux/macOS).
*   **ZLIB**: Thư viện nén cần thiết được cài trên hệ thống (đối với Windows có thể tải qua vcpkg hoặc đi kèm SDK).

### Các bước biên dịch trên PC (Windows / Linux / macOS)

1.  Tạo thư mục build và cấu hình dự án bằng CMake:
    ```bash
    mkdir build
    cd build
    cmake ..
    ```
    *(CMake sẽ tự động tải các gói thư viện SDL3, SDL3_image, SDL3_ttf, SDL3_net và tinyxml2 từ GitHub)*

2.  Biên dịch dự án:
    ```bash
    cmake --build . --config Release
    ```

3.  Sau khi biên dịch thành công, các file thực thi và tài nguyên DLL sẽ được copy vào thư mục output:
    *   Chạy Server: `./bin/OUGameServer` (hoặc `OUGameServer.exe`)
    *   Chạy Client: `./bin/OUGameClient` (hoặc `OUGameClient.exe`)

### Biên dịch cho Android

Dự án tích hợp sẵn thư mục `android-project`. Bạn có thể mở dự án này bằng **Android Studio**, cấu hình Android NDK để biên dịch tệp thư viện chia sẻ `libmain.so` thông qua CMake của Android và chạy trực tiếp trên thiết bị/máy ảo Android.

---

## 💡 Tư Duy Thiết Kế & Best Practices áp dụng

1.  **Memory Safety & RAII**: Hạn chế tối đa sử dụng con trỏ thô (`raw pointers`). Tận dụng triệt để `std::unique_ptr` và `std::shared_ptr` để tự động hóa vòng đời đối tượng, phòng tránh rò rỉ bộ nhớ (Memory Leak) và lỗi giải phóng hai lần (Double Free).
2.  **Clean Code & SOLID**: Các hệ thống được phân rã thành các module chức năng riêng biệt. `Game` đóng vai trò quản lý tổng thế, `AssetManager` lo việc nạp và giải phóng hình ảnh/font chữ, `Map` lo việc render lưới ô bản đồ, `ECS` quản lý logic nhân vật.
3.  **Performance Optimization**: Hệ thống ECS cho phép truy cập bộ nhớ tuyến tính tuần tự (Data-Oriented Design) giúp tối ưu bộ đệm CPU (CPU Cache Hits).
4.  **Network Synchronization**: Sử dụng socket không chặn (Non-blocking TCP sockets) của `SDL3_net` để xử lý nhiều gói tin mạng đồng thời mà không làm gián đoạn (freeze) vòng lặp render đồ họa ở client.
