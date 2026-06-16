#pragma once

#include <SDL3/SDL.h>

#include <cmath>

// Lớp Camera kế thừa từ SDL_Rect quản lý vùng nhìn của người chơi trong thế giới game.
// Hỗ trợ dịch chuyển mượt mà (smooth scrolling) bằng Lerp và thu phóng (zoom) hình ảnh thế giới.
typedef struct Camera : SDL_Rect {
  float zoom = 1.0f;       // Tỷ lệ thu phóng hiện tại của camera (1.0 là tỷ lệ chuẩn)
  float targetZoom = 1.0f; // Tỷ lệ thu phóng mục tiêu (mượt mà thay đổi zoom dần về đây)
  float zoomSpeed = 0.1f;  // Tốc độ thay đổi thu phóng mỗi nấc lăn chuột
  float minZoom = 0.5f;    // Giới hạn zoom tối thiểu (thu nhỏ tối đa)
  float maxZoom = 2.0f;    // Giới hạn zoom tối đa (phóng to tối đa)
  float exactX = 0.0f;     // Tọa độ X dạng số thực chính xác của camera (để dịch chuyển mượt dạng sub-pixel)
  float exactY = 0.0f;     // Tọa độ Y dạng số thực chính xác của camera
  int initW = 800;         // Chiều rộng cửa sổ game mặc định ban đầu
  int initH = 640;         // Chiều cao cửa sổ game mặc định ban đầu

  // Constructor mặc định
  Camera() {
    this->x = 0;
    this->y = 0;
    this->w = 800;
    this->h = 640;
    this->zoom = 1.0f;
    this->targetZoom = 1.0f;
    this->zoomSpeed = 0.1f;
    this->minZoom = 0.5f;
    this->maxZoom = 2.0f;
    this->exactX = 0.0f;
    this->exactY = 0.0f;
    this->initW = 800;
    this->initH = 640;
  }

  // Constructor khởi tạo với đầy đủ thông số
  Camera(int x_val, int y_val, int w_val, int h_val, float zoom_val,
         float zoomSpeed_val, float minZoom_val, float maxZoom_val) {
    this->x = x_val;
    this->y = y_val;
    this->w = w_val;
    this->h = h_val;
    this->zoom = zoom_val;
    this->targetZoom = zoom_val;
    this->zoomSpeed = zoomSpeed_val;
    this->minZoom = minZoom_val;
    this->maxZoom = maxZoom_val;
    this->exactX = static_cast<float>(x_val);
    this->exactY = static_cast<float>(y_val);
    this->initW = w_val;
    this->initH = h_val;
  }

  // Thực hiện phóng to camera lên một nấc
  void zoomIn() {
    if (targetZoom < maxZoom) {
      targetZoom += zoomSpeed;
      if (targetZoom > maxZoom) targetZoom = maxZoom;
    }
  }

  // Thực hiện thu nhỏ camera đi một nấc
  void zoomOut() {
    if (targetZoom > minZoom) {
      targetZoom -= zoomSpeed;
      if (targetZoom < minZoom) targetZoom = minZoom;
    }
  }

  // Cập nhật vị trí và tỉ lệ zoom của camera mỗi frame bằng thuật toán nội suy tuyến tính (Lerp)
  // targetCamX, targetCamY: Tọa độ đích camera muốn dịch chuyển tới (tâm của người chơi)
  // cameraFlow: true thì di chuyển camera mượt bám theo, false thì camera cố định (hoặc kéo chuột)
  void update(float targetCamX, float targetCamY, bool cameraFlow) {
    // 1. Dùng thuật toán Lerp dịch chuyển zoom hiện tại tiến dần về targetZoom
    float zoomLerpSpeed = 0.08f;
    if (std::fabs(targetZoom - zoom) > 0.0001f) {
      zoom += (targetZoom - zoom) * zoomLerpSpeed;
    } else {
      zoom = targetZoom;
    }

    // Cập nhật lại kích thước w, h vùng nhìn thực tế trong thế giới game tương ứng với tỷ lệ zoom
    // (Nếu zoom càng lớn thì vùng nhìn w,h càng co hẹp lại)
    this->w = static_cast<int>(static_cast<float>(initW) / zoom);
    this->h = static_cast<int>(static_cast<float>(initH) / zoom);

    // 2. Dùng thuật toán Lerp dịch chuyển camera mượt bám theo mục tiêu
    if (cameraFlow) {
      float posLerpSpeed = 0.08f;
      float diffX = targetCamX - exactX;
      float diffY = targetCamY - exactY;

      // Nếu khoảng cách lệch cực nhỏ, gán trực tiếp bằng tọa độ đích để tránh rung màn hình
      if (std::fabs(diffX) < 0.1f && std::fabs(diffY) < 0.1f) {
        exactX = targetCamX;
        exactY = targetCamY;
      } else {
        exactX += diffX * posLerpSpeed;
        exactY += diffY * posLerpSpeed;
      }
    } else {
      // Nếu không flow (camera bị kéo bằng chuột phải), đồng bộ lại tọa độ exact số thực theo tọa độ integer x, y hiện tại
      exactX = static_cast<float>(this->x);
      exactY = static_cast<float>(this->y);
    }

    // Làm tròn tọa độ số thực về số nguyên để gán vào thuộc tính x, y của SDL_Rect phục vụ render
    this->x = static_cast<int>(std::round(exactX));
    this->y = static_cast<int>(std::round(exactY));
  }
} Camera;
