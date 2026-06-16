# Camera Tracking Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a smooth, Lerp-based camera tracking system that centers on the main player character, including manual drag bypass and automatic smooth centering.

**Architecture:** We will add fields to `CameraResource` for initialization and manual drag detection, update `ClientApp` to register drag ticks, and modify `RenderSystem` to update the camera position using a time-delta-based Lerp.

**Tech Stack:** C++, SDL3, ECS

---

### Task 1: Update Camera Resource

**Files:**
- Modify: [Resources.hpp](file:///d:/WorkSpace/Game/OU/Client/include/Client/Resources.hpp#L24-L33)

- [ ] **Step 1: Add manual drag and initialization fields to CameraResource**
  Edit [Resources.hpp](file:///d:/WorkSpace/Game/OU/Client/include/Client/Resources.hpp) to add `lastManualDragTime` and `isInitialized` to the `CameraResource` struct:
  ```cpp
  struct CameraResource {
      int cameraX = 400;   
      int cameraY = 200;   
      float zoom = 1.0f;   
      
      // New fields for tracking manual drag and initialization state
      uint32_t lastManualDragTime = 0;
      bool isInitialized = false;
  };
  ```

- [ ] **Step 2: Verify code compiles**
  Run: `cmake --build build`
  Expected: Successful compilation of the Core and Client library.

- [ ] **Step 3: Commit**
  ```bash
  git add Client/include/Client/Resources.hpp
  git commit -m "feat: add lastManualDragTime and isInitialized to CameraResource"
  ```

---

### Task 2: Update Input/Mouse Drag Logic in ClientApp

**Files:**
- Modify: [ClientApp.cpp](file:///d:/WorkSpace/Game/OU/Client/src/ClientApp.cpp#L363-L368)

- [ ] **Step 1: Set lastManualDragTime when dragging the camera**
  Update the mouse motion handler in [ClientApp.cpp](file:///d:/WorkSpace/Game/OU/Client/src/ClientApp.cpp) to record the current ticks when the camera is manually dragged with the right mouse button:
  ```cpp
  if (g_isDraggingMouse) {
      camera.cameraX = static_cast<int>(g_dragStartCamX + (event.motion.x - g_dragStartMouseX));
      camera.cameraY = static_cast<int>(g_dragStartCamY + (event.motion.y - g_dragStartMouseY));
      camera.lastManualDragTime = SDL_GetTicks(); // Record drag time
  }
  ```

- [ ] **Step 2: Verify code compiles**
  Run: `cmake --build build`
  Expected: Successful compilation.

- [ ] **Step 3: Commit**
  ```bash
  git add Client/src/ClientApp.cpp
  git commit -m "feat: record manual camera drag ticks in ClientApp"
  ```

---

### Task 3: Implement Camera Tracking in RenderSystem

**Files:**
- Modify: [RenderSystem.cpp](file:///d:/WorkSpace/Game/OU/Client/src/Systems/RenderSystem.cpp#L159-L180)

- [ ] **Step 1: Add time-delta calculation and player center logic to RenderSystem::Render**
  Modify [RenderSystem.cpp](file:///d:/WorkSpace/Game/OU/Client/src/Systems/RenderSystem.cpp) to automatically update the camera coordinates based on the player's screen coordinate.
  
  Locate `RenderSystem::Render` and insert the tracking logic at the beginning (before building the render list but after retrieving the resources):
  ```cpp
  void RenderSystem::Render(SDL_Renderer* renderer, ECS::Coordinator* ecs, Core::MapManager& mapManager, SDL_Texture* textureAtlas) {
      auto& camera = ecs->GetResource<Resources::CameraResource>();
      auto& selection = ecs->GetResource<Resources::SelectionResource>();

      // --- NEW CAMERA TRACKING LOGIC ---
      static uint32_t lastTicks = SDL_GetTicks();
      uint32_t currentTicks = SDL_GetTicks();
      float dt = (currentTicks - lastTicks) / 1000.0f;
      lastTicks = currentTicks;
      if (dt > 0.1f) dt = 0.1f; // Cap delta time
      
      int windowW = 1024;
      int windowH = 768;
      SDL_GetRenderOutputSize(renderer, &windowW, &windowH);
      // ----------------------------------

      std::vector<RenderItem> renderList;

      int playerX = 5, playerY = 5;
      int playerZ = 0;
      bool foundMe = false;
      for (auto entity : mEntities) {
          if (ecs->HasComponent<Components::NetworkComponent>(entity)) {
              auto& netComp = ecs->GetComponent<Components::NetworkComponent>(entity);
              if (netComp.isMe) {
                  auto& posComp = ecs->GetComponent<Components::PositionComponent>(entity);
                  playerX = posComp.position.x;
                  playerY = posComp.position.y;
                  playerZ = posComp.position.z;
                  foundMe = true;
                  break;
              }
          }
      }

      // --- APPLY CAMERA LERP AND LAG ---
      if (foundMe) {
          Core::ScreenCoord playerScreen = Core::Isometric::WorldToScreen({ (int16_t)playerX, (int16_t)playerY, (int8_t)playerZ }, 0, 0);

          // Only auto-track if not manually dragging (1.5 seconds cooldown)
          if (currentTicks - camera.lastManualDragTime > 1500) {
              int targetCamX = windowW / 2 - playerScreen.x * camera.zoom;
              int targetCamY = windowH / 2 - playerScreen.y * camera.zoom;

              if (!camera.isInitialized) {
                  camera.cameraX = targetCamX;
                  camera.cameraY = targetCamY;
                  camera.isInitialized = true;
              } else {
                  float lerpSpeed = 4.5f; // Lower = smoother tracking with more lag behind player
                  camera.cameraX += static_cast<int>((targetCamX - camera.cameraX) * lerpSpeed * dt);
                  camera.cameraY += static_cast<int>((targetCamY - camera.cameraY) * lerpSpeed * dt);
              }
          }
      }
      // ----------------------------------
  ```

- [ ] **Step 2: Verify compilation and linkage**
  Run: `cmake --build build`
  Expected: Successful compilation of all targets with exit code 0.

- [ ] **Step 3: Commit**
  ```bash
  git add Client/src/Systems/RenderSystem.cpp
  git commit -m "feat: implement Lerp-based camera auto-tracking with movement lag"
  ```
