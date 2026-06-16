# Smooth Zoom Design

Design and specification for implementing smooth zoom and camera transitions in the OU Game Engine client.

## Goal
Fix the camera zoom behavior to transition smoothly instead of snapping instantly to discrete zoom factors, and eliminate jitter/stutter during camera tracking by transitioning to sub-pixel floating-point position LERPing.

## Proposed Changes

### Component: Camera Struct
Modify the `Camera` struct in [Game.hpp](file:///d:/WorkSpace/Game/OU/src/Core/Game.hpp) to:
1. Support `targetZoom` and precise sub-pixel float coordinates `exactX` and `exactY`.
2. Retrieve the base screen width and height (`initW`, `initH`) from the game initialization parameters rather than hardcoding `800x640`.
3. Interpolate `zoom` towards `targetZoom` smoothly using a configurable LERP factor.
4. Interpolate `exactX` and `exactY` towards target camera positions to provide jitter-free panning.
5. Round camera position to integer values (`this->x`, `this->y`) for SDL rendering compatibility.
6. Synchronize floating-point coordinates `exactX` and `exactY` with integer coordinates when the map is manually dragged.

### Component: Client Engine Init and Update
Modify the `Game` class in [Game.cpp](file:///d:/WorkSpace/Game/OU/src/Core/Game.cpp):
1. In `Game::init()`, record the actual `width` and `height` parameters into `camera.initW` and `camera.initH`.
2. In `Game::update()`, replace hardcoded resolution viewport calculations with dynamic fields (`camera.initW` and `camera.initH`).
3. Delegate camera position updating and zoom LERPing to `camera.update()`.

## Verification Plan

### Manual Verification
1. Run the client and server.
2. Scroll the mouse wheel to zoom in and out. Verify that the transition is completely smooth and continuous.
3. Move the character using WASD keys while zoomed in, normal, and zoomed out. Verify that the camera follows the character smoothly with zero jitter/stutter.
4. Drag the camera using the right mouse button. Verify that the camera pans correctly and transitions back to centering on the character after the release delay.
