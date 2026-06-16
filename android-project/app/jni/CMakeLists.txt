cmake_minimum_required(VERSION 3.22)
project(GAME)

# Kích hoạt cờ ANDROID để CMakeLists.txt gốc chuyển hướng build
set(ANDROID ON)

# Trỏ trực tiếp tới CMakeLists.txt gốc ở thư mục cha 3 cấp (d:/WorkSpace/Game/OU)
add_subdirectory(../../.. src_build)
