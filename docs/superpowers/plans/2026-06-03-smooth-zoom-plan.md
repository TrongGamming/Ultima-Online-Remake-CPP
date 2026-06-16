# Smooth Zoom Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement smooth zoom transitions and sub-pixel camera tracking to eliminate visual jitter and stutter.

**Architecture:** Introduce `targetZoom` and sub-pixel float coordinates (`exactX`, `exactY`) in the `Camera` struct. Linearly interpolate the zoom factor and coordinates in the game loop, and update viewport size dynamically based on the game's actual base resolution.

**Tech Stack:** C++20, SDL3

---

### Task 1: Refactor Camera Struct

**Files:**
- Modify: [src/Core/Game.hpp](file:///d:/WorkSpace/Game/OU/src/Core/Game.hpp:15-40)

- [ ] **Step 1: Modify Game.hpp to add constructors, target zoom, sub-pixel floats, base resolution fields, and the update method.**

Replace the existing `Camera` struct:
```cpp
typedef struct Camera : SDL_Rect {
  float zoom = 1.0f;
  float zoomSpeed = 0.1f;
  float minZoom = 0.5f;
  float maxZoom = 2.0f;

  void zoomIn() {
    if (zoom < maxZoom) {
      zoom += zoomSpeed;
      if (zoom > maxZoom) zoom = maxZoom;  // Giới hạn biên trên
    }
    // Tính toán kích thước Viewport dựa trên tỉ lệ zoom mới
    this->w = static_cast<int>(800.0f / zoom);
    this->h = static_cast<int>(640.0f / zoom);
  }

  void zoomOut() {
    if (zoom > minZoom) {
      zoom -= zoomSpeed;
      if (zoom < minZoom) zoom = minZoom;  // Giới hạn biên dưới
    }
    // Tính toán kích thước Viewport dựa trên tỉ lệ zoom mới
    this->w = static_cast<int>(800.0f / zoom);
    this->h = static_cast<int>(640.0f / zoom);
  }
} Camera;
```

With:
```cpp
typedef struct Camera : SDL_Rect {
  float zoom = 1.0f;
  float targetZoom = 1.0f;
  float zoomSpeed = 0.1f;
  float minZoom = 0.5f;
  float maxZoom = 2.0f;
  float exactX = 0.0f;
  float exactY = 0.0f;
  int initW = 800;
  int initH = 640;

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

  Camera(int x_val, int y_val, int w_val, int h_val, float zoom_val, float zoomSpeed_val, float minZoom_val, float maxZoom_val) {
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

  void zoomIn() {
    if (targetZoom < maxZoom) {
      targetZoom += zoomSpeed;
      if (targetZoom > maxZoom) targetZoom = maxZoom;
    }
  }

  void zoomOut() {
    if (targetZoom > minZoom) {
      targetZoom -= zoomSpeed;
      if (targetZoom < minZoom) targetZoom = minZoom;
    }
  }

  void update(float targetCamX, float targetCamY, bool cameraFlow) {
    // 1. Lerp zoom factor towards targetZoom
    float zoomLerpSpeed = 0.08f;
    if (std::abs(targetZoom - zoom) > 0.0001f) {
      zoom += (targetZoom - zoom) * zoomLerpSpeed;
    } else {
      zoom = targetZoom;
    }

    // Dynamic width and height of the camera view based on intermediate zoom
    this->w = static_cast<int>(static_cast<float>(initW) / zoom);
    this->h = static_cast<int>(static_cast<float>(initH) / zoom);

    // 2. Lerp camera coordinates smoothly (sub-pixel camera movement)
    if (cameraFlow) {
      float posLerpSpeed = 0.08f;
      float diffX = targetCamX - exactX;
      float diffY = targetCamY - exactY;

      if (std::abs(diffX) < 0.1f && std::abs(diffY) < 0.1f) {
        exactX = targetCamX;
        exactY = targetCamY;
      } else {
        exactX += diffX * posLerpSpeed;
        exactY += diffY * posLerpSpeed;
      }
    } else {
      // Sync exact float values back if manually dragged
      exactX = static_cast<float>(this->x);
      exactY = static_cast<float>(this->y);
    }

    // Assign back to integer SDL_Rect fields for rendering compatibility
    this->x = static_cast<int>(std::round(exactX));
    this->y = static_cast<int>(std::round(exactY));
  }
} Camera;
```

---

### Task 2: Integrate Dynamic Viewport and Camera Update in Game

**Files:**
- Modify: [src/Core/Game.cpp](file:///d:/WorkSpace/Game/OU/src/Core/Game.cpp)

- [ ] **Step 1: Set base camera dimensions dynamically in Game::init().**
In `Game::init()` inside `src/Core/Game.cpp`, find where the window is verified:
```cpp
    window = SDL_CreateWindow(title, width, height, flags);
    if (window) {
      std::cout << "Client: Window created!" << std::endl;
    }
```
Update it to record dynamic size in `camera`:
```cpp
    window = SDL_CreateWindow(title, width, height, flags);
    if (window) {
      std::cout << "Client: Window created!" << std::endl;
      camera.initW = width;
      camera.initH = height;
      camera.w = width;
      camera.h = height;
    }
```

- [ ] **Step 2: Update dynamic camera viewport dimension calculations in Game::update().**
Replace the hardcoded `800.0f` and `640.0f` coordinate sizes:
```cpp
    // Vị trí kích thước màn hình gốc (Gốc tọa độ cửa sổ window của bạn là
    // 800x640)
    float viewWidth = 800.0f;
    float viewHeight = 640.0f;

    // Khi có Zoom, kích thước vùng nhìn thực tế trong thế giới game sẽ co giãn
    // tương ứng
    if (camera.zoom > 0.0f) {
      viewWidth = 800.0f / camera.zoom;
      viewHeight = 640.0f / camera.zoom;
    }
```
With:
```cpp
    // Vị trí kích thước màn hình gốc
    float viewWidth = static_cast<float>(camera.initW);
    float viewHeight = static_cast<float>(camera.initH);

    // Khi có Zoom, kích thước vùng nhìn thực tế trong thế giới game sẽ co giãn
    // tương ứng
    if (camera.zoom > 0.0f) {
      viewWidth = viewWidth / camera.zoom;
      viewHeight = viewHeight / camera.zoom;
    }
```

- [ ] **Step 3: Call camera.update() inside Game::update().**
Replace the manual positioning LERP:
```cpp
    // --- KIỂM TRA ĐIỀU KIỆN THỜI GIAN CHỜ 1 GIÂY ---
    if (isResettingCamera && !cameraFlow) {
      if (SDL_GetTicks() - lastRightMouseReleaseTime >= 1000) {
        cameraFlow = true;
        isResettingCamera = false;
      }
    }

    // --- THỰC HIỆN KÉO CAMERA TỪ TỪ (LERP) ---
    if (cameraFlow) {
      // Tốc độ hồi camera (0.1f nghĩa là mỗi khung hình đi được 10% khoảng cách
      // còn lại) Bạn có thể tăng lên 0.15f để kéo nhanh hơn hoặc giảm xuống
      // 0.05f để kéo chậm hơn
      float lerpSpeed = 0.08f;

      float diffX = static_cast<float>(targetCamX - camera.x);
      float diffY = static_cast<float>(targetCamY - camera.y);

      // Nếu khoảng cách còn rất nhỏ thì khóa hẳn vào vị trí đích để tránh sai
      // số float
      if (std::abs(diffX) < 1.0f && std::abs(diffY) < 1.0f) {
        camera.x = targetCamX;
        camera.y = targetCamY;
      } else {
        camera.x += static_cast<int>(diffX * lerpSpeed);
        camera.y += static_cast<int>(diffY * lerpSpeed);
      }
    }
```
With:
```cpp
    // --- KIỂM TRA ĐIỀU KIỆN THỜI GIAN CHỜ 1 GIÂY ---
    if (isResettingCamera && !cameraFlow) {
      if (SDL_GetTicks() - lastRightMouseReleaseTime >= 1000) {
        cameraFlow = true;
        isResettingCamera = false;
      }
    }

    // --- THỰC HIỆN KÉO CAMERA TỪ TỪ VÀ ZOOM MƯỢT ---
    camera.update(static_cast<float>(targetCamX), static_cast<float>(targetCamY), cameraFlow);
```

---

### Task 3: Build and Verify

- [ ] **Step 1: Build the client project using CMake.**

Run: `cmake --build build`
Expected: Compilation completes with no compiler errors.
