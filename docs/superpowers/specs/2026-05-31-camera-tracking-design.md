# Camera Tracking and Smooth Panning Design Spec

## Goal
Implement a smooth camera tracking system that centers on the local player character, with an inertia/lag effect when moving. The camera should also support manual dragging (with right click) and resume tracking smoothly after a short cooldown.

## Architecture

### 1. Camera Resource Update

We will add fields to `Resources::CameraResource` to track manual drag states and smooth centering flags.

[Client/include/Client/Resources.hpp](file:///d:/WorkSpace/Game/OU/Client/include/Client/Resources.hpp):
```cpp
struct CameraResource {
    int cameraX = 400;   
    int cameraY = 200;   
    float zoom = 1.0f;   

    // Track when the user last dragged the camera manually (in ms)
    uint32_t lastManualDragTime = 0;
    // Set to true after the first centering, to avoid panned initialization
    bool isInitialized = false;
};
```

### 2. Auto-Tracking Implementation

Inside the main render function, the camera will continuously update its target coordinate based on the player's current screen position:

[Client/src/Systems/RenderSystem.cpp](file:///d:/WorkSpace/Game/OU/Client/src/Systems/RenderSystem.cpp):
```cpp
// 1. Calculate delta time (dt)
static uint32_t lastTicks = SDL_GetTicks();
uint32_t currentTicks = SDL_GetTicks();
float dt = (currentTicks - lastTicks) / 1000.0f;
lastTicks = currentTicks;
if (dt > 0.1f) dt = 0.1f;

// 2. Query window size
int windowW = 1024, windowH = 768;
SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

// 3. Track player screen coordinate
if (foundMe) {
    Core::ScreenCoord playerScreen = Core::Isometric::WorldToScreen({ (int16_t)playerX, (int16_t)playerY, (int8_t)playerZ }, 0, 0);

    // 4. Center player with lag if 1.5 seconds have passed since manual drag
    if (currentTicks - camera.lastManualDragTime > 1500) {
        int targetCamX = windowW / 2 - playerScreen.x * camera.zoom;
        int targetCamY = windowH / 2 - playerScreen.y * camera.zoom;

        if (!camera.isInitialized) {
            camera.cameraX = targetCamX;
            camera.cameraY = targetCamY;
            camera.isInitialized = true;
        } else {
            float lerpSpeed = 4.5f; // Easing speed
            camera.cameraX += (targetCamX - camera.cameraX) * lerpSpeed * dt;
            camera.cameraY += (targetCamY - camera.cameraY) * lerpSpeed * dt;
        }
    }
}
```

### 3. Manual Drag Interactivity

Inside `ClientApp.cpp`, the right-click camera drag logic will update `lastManualDragTime`.

[Client/src/ClientApp.cpp](file:///d:/WorkSpace/Game/OU/Client/src/ClientApp.cpp):
```cpp
// On mouse movement drag:
camera.lastManualDragTime = SDL_GetTicks();
```
