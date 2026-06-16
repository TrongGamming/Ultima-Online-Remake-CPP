#pragma once

#include "Core/AssetManager.hpp"
#include "Core/Game.hpp"
#include "ECS/ECS.hpp"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>

// UILabel quản lý việc hiển thị văn bản (Text) trên màn hình trò chơi.
// Hỗ trợ vẽ HUD, thông báo chat, tên thực thể và cập nhật động nội dung/màu sắc văn bản bằng thư viện SDL_ttf.
class UILabel : public Component {
public:
  // Constructor khởi tạo nhãn chữ UI
  // xpos, ypos: Vị trí vẽ nhãn trên màn hình (đầu ra tọa độ pixel cố định)
  // text: Nội dung chuỗi ban đầu
  // font: Tên định danh font chữ đăng ký trong AssetManager
  // colour: Màu sắc chữ (SDL_Color)
  UILabel(int xpos, int ypos, std::string text, std::string font,
          SDL_Color &colour)
      : labelText(text), labelFont(font), textColour(colour) {
    position.x = xpos;
    position.y = ypos;
    position.w = 0;
    position.h = 0;

    SetLabelText(labelText, labelFont); // Tạo texture chữ ban đầu
  }

  // Destructor giải phóng tài nguyên texture chữ để tránh rò rỉ bộ nhớ đồ họa (memory leak)
  ~UILabel() override {
    if (labelTexture) {
      SDL_DestroyTexture(labelTexture);
    }
  }

  // Thay đổi màu sắc của nhãn chữ và vẽ lại texture mới
  void SetLabelColor(SDL_Color colour) {
    textColour = colour;
    SetLabelText(labelText, labelFont);
  }

  // Cập nhật nội dung văn bản và tạo lại texture hình ảnh chữ tương ứng
  // text: Nội dung mới cần đổi sang
  // font: Font chữ sử dụng
  void SetLabelText(std::string text, std::string font) {
    // Cập nhật thuộc tính của đối tượng lớp để dùng lại khi đổi màu
    labelText = text;
    labelFont = font;
    
    // Lấy font đăng ký từ AssetManager
    TTF_Font *f = Game::assets->GetFont(font);
    if (!f) {
      std::cerr << "Font not found: " << font << std::endl;
      return;
    }
    
    // Vẽ văn bản ra một bề mặt thô (SDL_Surface)
    SDL_Surface *surf = TTF_RenderText_Blended(f, text.c_str(), 0, textColour);
    
    // Nếu texture chữ cũ đang tồn tại, giải phóng nó trước khi tạo mới
    if (labelTexture) {
      SDL_DestroyTexture(labelTexture);
      labelTexture = nullptr;
    }
    
    // Chuyển đổi bề mặt SDL_Surface sang SDL_Texture để GPU có thể vẽ nhanh lên màn hình
    labelTexture = SDL_CreateTextureFromSurface(Game::renderer, surf);
    SDL_DestroySurface(surf); // Giải phóng surface thô ngay sau khi đổi xong

    // Lấy kích thước pixel thực tế của chuỗi chữ vừa được sinh ra để cập nhật khung vẽ
    float w = 0, h = 0;
    SDL_GetTextureSize(labelTexture, &w, &h);
    position.w = static_cast<int>(w);
    position.h = static_cast<int>(h);
  }

  // Vẽ chữ lên màn hình máy tính tại tọa độ position (đơn vị tọa độ màn hình trực tiếp)
  void draw() override {
    if (!labelTexture) return; // Bảo vệ an toàn nullptr
    SDL_FRect dest = {
        static_cast<float>(position.x), static_cast<float>(position.y),
        static_cast<float>(position.w), static_cast<float>(position.h)};
    SDL_RenderTexture(Game::renderer, labelTexture, nullptr, &dest);
  }

private:
  SDL_Rect position{};            // Tọa độ màn hình và kích thước hộp bao quanh chữ
  std::string labelText;          // Nội dung chuỗi chữ đang hiển thị
  std::string labelFont;          // Tên font chữ đang sử dụng
  SDL_Color textColour{};         // Màu sắc của văn bản
  SDL_Texture *labelTexture{nullptr}; // Con trỏ quản lý texture đồ họa của chuỗi văn bản
};
