#pragma once
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <string>

// Lớp TextureManager cung cấp các phương thức tĩnh (static helper)
// để nạp và vẽ ảnh nhanh chóng bằng thư viện SDL3 và SDL_image.
class TextureManager {
public:
    // Tải một tệp ảnh từ đĩa cứng (PNG, JPG...) và chuyển đổi thành SDL_Texture
    // fileName: Đường dẫn tuyệt đối hoặc tương đối tới tệp ảnh
    [[nodiscard]] static SDL_Texture* LoadTexture(const char* fileName);
    
    // Vẽ texture lên màn hình với vị trí và hướng lật chỉ định
    // tex: Con trỏ tới texture cần vẽ
    // src: Vùng cắt hình chữ nhật từ texture nguồn (srcRect)
    // dest: Vùng vẽ hình chữ nhật số thực trên màn hình đích (destRect)
    // flip: Hướng lật ảnh (không lật, lật ngang, lật dọc)
    static void Draw(SDL_Texture* tex, SDL_Rect src, SDL_FRect dest, SDL_FlipMode flip);

    // Chuẩn hóa đường dẫn asset cho Android (loại bỏ tiền tố "assets/")
    static std::string GetAssetPath(const std::string& path) {
#if defined(ANDROID) || defined(__ANDROID__)
        if (path.rfind("assets/", 0) == 0) {
            return path.substr(7);
        }
#endif
        return path;
    }

    // Chuẩn hóa đường dẫn: thay thế '\' bằng '/', loại bỏ các '.' và xử lý '..'
    static std::string StandardizePath(const std::string& path);
    
    // Giải quyết đường dẫn tương đối (relPath) so với đường dẫn của tệp cơ sở (basePath)
    static std::string ResolvePath(const std::string& basePath, const std::string& relPath);
};

