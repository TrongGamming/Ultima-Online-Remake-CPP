#include "Core/TextureManager.hpp"

#include <iostream>

#include "Core/Game.hpp"

// Nạp hình ảnh từ file và trả về con trỏ SDL_Texture nằm trên GPU
SDL_Texture* TextureManager::LoadTexture(const char* texture) {
  std::string actualPath = GetAssetPath(texture);
  // 1. Dùng SDL_image tải ảnh thô lên RAM dưới dạng SDL_Surface
  SDL_Surface* tempSurface = IMG_Load(actualPath.c_str());
  if (!tempSurface) {
    std::cerr << "IMG_Load Error for path '" << texture
              << "': " << SDL_GetError() << std::endl;
    return nullptr;
  }
  
  // 2. Chuyển đổi SDL_Surface thô thành SDL_Texture tối ưu hóa trên VRAM của GPU để render cực nhanh
  SDL_Texture* tex = SDL_CreateTextureFromSurface(Game::renderer, tempSurface);
  
  // 3. Giải phóng bộ nhớ RAM của surface thô ngay sau khi tạo xong texture
  SDL_DestroySurface(tempSurface);
  
  return tex; // Trả về con trỏ quản lý texture
}

// Vẽ một phần của texture lên màn hình đích
void TextureManager::Draw(SDL_Texture* tex, SDL_Rect src, SDL_FRect dest,
                          SDL_FlipMode flip) {
  if (!tex) return; // Bảo vệ an toàn nullptr
  // Ghi chú: src là vùng cắt ảnh nguồn trên tilesheet, dest là vùng vẽ ra màn hình thực tế.
  // Đổi kiểu src từ SDL_Rect (số nguyên) sang SDL_FRect (số thực) để phù hợp với hàm vẽ SDL3 mới.
  SDL_FRect srcF = {static_cast<float>(src.x), static_cast<float>(src.y),
                    static_cast<float>(src.w), static_cast<float>(src.h)};
  
  // Gọi hàm vẽ của SDL3 hỗ trợ xoay và lật ảnh (xoay 0.0 độ, tâm xoay mặc định)
  SDL_RenderTextureRotated(Game::renderer, tex, &srcF, &dest, 0.0, nullptr,
                           flip);
}

#include <vector>
#include <sstream>

std::string TextureManager::StandardizePath(const std::string& path) {
  std::string normalized = path;
  for (char& c : normalized) {
    if (c == '\\') c = '/';
  }

  std::vector<std::string> parts;
  std::stringstream ss(normalized);
  std::string part;
  while (std::getline(ss, part, '/')) {
    if (part == "." || part.empty()) {
      continue;
    }
    if (part == "..") {
      if (!parts.empty() && parts.back() != "..") {
        parts.pop_back();
      } else {
        parts.push_back("..");
      }
    } else {
      parts.push_back(part);
    }
  }

  std::string result;
  for (size_t i = 0; i < parts.size(); ++i) {
    result += parts[i];
    if (i < parts.size() - 1) {
      result += "/";
    }
  }

  if (!normalized.empty() && normalized[0] == '/') {
    result = "/" + result;
  }
  return result;
}

std::string TextureManager::ResolvePath(const std::string& basePath, const std::string& relPath) {
  size_t lastSlash = basePath.find_last_of("/\\");
  std::string dir = (lastSlash == std::string::npos) ? "" : basePath.substr(0, lastSlash + 1);
  return StandardizePath(dir + relPath);
}

