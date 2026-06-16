# Keyboard Movement and Fullscreen Map Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow the player to move using Arrow keys / WASD keys (with Shift to run), and expand the map loading radius so that the tiles cover the entire fullscreen viewport.

**Architecture:** 
1. Modify `RenderSystem::Render` to query active blocks with a radius of `4` instead of `2`.
2. Modify `MovementSystem::Update` to query the keyboard state via `SDL_GetKeyboardState`, check for WASD / Arrow keys, check Shift for running, abort autopilot if keyboard movement is detected, and trigger movement packets.

**Tech Stack:** C++, SDL3, ECS

---

### Task 1: Expand Map Rendering Radius

**Files:**
- Modify: [RenderSystem.cpp](file:///d:/WorkSpace/Game/OU/Client/src/Systems/RenderSystem.cpp#L220-L226)

- [ ] **Step 1: Increase active block radius**
  In [RenderSystem.cpp](file:///d:/WorkSpace/Game/OU/Client/src/Systems/RenderSystem.cpp), locate the `GetActiveBlocks` call and change the radius from `2` to `4`:
  ```cpp
  auto activeBlocks = mapManager.GetActiveBlocks(playerBlockX, playerBlockY, 4);
  ```

- [ ] **Step 2: Verify code compiles**
  Run: `cmake --build build`
  Expected: Successful compilation.

- [ ] **Step 3: Commit**
  ```bash
  git add Client/src/Systems/RenderSystem.cpp
  git commit -m "feat: increase map active block radius to 4 to cover fullscreen"
  ```

---

### Task 2: Implement Keyboard Movement in MovementSystem

**Files:**
- Modify: [MovementSystem.cpp](file:///d:/WorkSpace/Game/OU/Client/src/Systems/MovementSystem.cpp#L101-L177)

- [ ] **Step 1: Integrate keyboard state polling in MovementSystem::Update**
  Update [MovementSystem.cpp](file:///d:/WorkSpace/Game/OU/Client/src/Systems/MovementSystem.cpp) to check keyboard states, update the movement vector, abort autopilot if keyboard keys are pressed, and trigger the player moves.
  
  Replace the start of `MovementSystem::Update` with:
  ```cpp
  void MovementSystem::Update(ECS::Coordinator* ecs) {
      auto& joystick = ecs->GetResource<Resources::JoystickResource>();

      // Poll keyboard state for WASD / Arrow keys
      const bool* keyState = SDL_GetKeyboardState(nullptr);
      int kDx = 0;
      int kDy = 0;
      if (keyState[SDL_SCANCODE_UP] || keyState[SDL_SCANCODE_W]) kDy -= 1;
      if (keyState[SDL_SCANCODE_DOWN] || keyState[SDL_SCANCODE_S]) kDy += 1;
      if (keyState[SDL_SCANCODE_LEFT] || keyState[SDL_SCANCODE_A]) kDx -= 1;
      if (keyState[SDL_SCANCODE_RIGHT] || keyState[SDL_SCANCODE_D]) kDx += 1;
      
      bool hasKeyboardInput = (kDx != 0 || kDy != 0);

      // Abort autopilot if manually controlled by joystick OR keyboard
      if (joystick.active || hasKeyboardInput) {
          for (auto entity : mEntities) {
              auto& netComp = ecs->GetComponent<Components::NetworkComponent>(entity);
              if (netComp.isMe) {
                  auto& predComp = ecs->GetComponent<Components::MovementPredictionComponent>(entity);
                  if (!predComp.autopilotPath.empty()) {
                      predComp.autopilotPath.clear();
                      auto& selection = ecs->GetResource<Resources::SelectionResource>();
                      selection.showSelection = false;
                      std::cout << "[Autopilot] Aborted by manual input." << std::endl;
                  }
                  break;
              }
          }
      }

      // Handle manual movement (Joystick or Keyboard)
      if (joystick.active || hasKeyboardInput) {
          float dx = 0.0f;
          float dy = 0.0f;
          float dist = 0.0f;
          bool isRunning = false;

          if (joystick.active) {
              dx = joystick.knobX - joystick.centerX;
              dy = joystick.knobY - joystick.centerY;
              dist = std::sqrt(dx * dx + dy * dy);
              isRunning = (dist > joystick.outerRadius * 0.7f);
          } else {
              dx = static_cast<float>(kDx);
              dy = static_cast<float>(kDy);
              dist = 1.0f; // Pass deadzone check
              isRunning = keyState[SDL_SCANCODE_LSHIFT] || keyState[SDL_SCANCODE_RSHIFT];
          }

          if (dist > 15.0f || !joystick.active) {
              for (auto entity : mEntities) {
                  auto& netComp = ecs->GetComponent<Components::NetworkComponent>(entity);
                  if (netComp.isMe) {
                      auto& predComp = ecs->GetComponent<Components::MovementPredictionComponent>(entity);
                      uint8_t dir = GetDirection(static_cast<int>(dx), static_cast<int>(dy));
                      uint32_t now = SDL_GetTicks();
                      uint32_t cooldown = isRunning ? 100 : 200;

                      if (now - predComp.lastMoveTime >= cooldown) {
                          predComp.lastMoveTime = now;
                          Core::ClientMovementMsg msg;
                          msg.header.id = Core::PacketID::CLIENT_MOVEMENT;
                          msg.header.length = sizeof(Core::ClientMovementMsg);
                          msg.sequence = now;
                          msg.direction = dir;
                          msg.isRunning = isRunning ? 1 : 0;

                          PredictMove(entity, ecs, msg);
                          EmitPacket(ecs, 1, &msg, sizeof(msg), false);
                      }
                      break;
                  }
              }
          }
      }
      // Handle Autopilot (Autoplay search)
      else {
          // Keep existing autopilot logic unchanged
  ```

- [ ] **Step 2: Verify code compiles**
  Run: `cmake --build build`
  Expected: Successful compilation.

- [ ] **Step 3: Commit**
  ```bash
  git add Client/src/Systems/MovementSystem.cpp
  git commit -m "feat: implement WASD/Arrow key movement and run shift keys"
  ```
