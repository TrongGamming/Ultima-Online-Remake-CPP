cmake_minimum_required(VERSION 3.22)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(OUGame CXX)

# =========================
# Compiler / Standard
# =========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MINGW)
    add_compile_options(-Wa,-mbig-obj)
endif()

# =========================
# FetchContent
# =========================
include(FetchContent)

# =========================
# SDL OPTIONS (optimize build)
# =========================
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)

set(SDL_IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_IMAGE_TESTS OFF CACHE BOOL "" FORCE)

set(SDL_TTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_TTF_TESTS OFF CACHE BOOL "" FORCE)

set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)

set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)

# =========================
# SDL3
# =========================
message(STATUS "Fetching SDL3...")
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.0
)
FetchContent_MakeAvailable(SDL3)

# =========================
# SDL_image
# =========================
message(STATUS "Fetching SDL_image...")
FetchContent_Declare(
    SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-3.2.0
)
FetchContent_MakeAvailable(SDL3_image)

# =========================
# SDL_ttf
# =========================
message(STATUS "Fetching SDL_ttf...")
FetchContent_Declare(
    SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG release-3.2.0
)
FetchContent_MakeAvailable(SDL3_ttf)

# =========================
# SDL_net
# =========================
message(STATUS "Fetching SDL_net...")
FetchContent_Declare(
    SDL3_net
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_net.git
    GIT_TAG main
)
FetchContent_MakeAvailable(SDL3_net)

# =========================
# tinyxml2
# =========================
message(STATUS "Fetching tinyxml2...")
FetchContent_Declare(
    tinyxml2
    GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
    GIT_TAG 10.0.0
)
FetchContent_MakeAvailable(tinyxml2)

find_package(ZLIB REQUIRED)

# =========================
# Common Library (Refreshed for WorldConfig)
# =========================
file(GLOB_RECURSE COMMON_SRC
    "src/Common/*.cpp"
)

add_library(OUCommon STATIC ${COMMON_SRC})

target_include_directories(OUCommon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(OUCommon PUBLIC
    SDL3::SDL3
    SDL3_net::SDL3_net
    tinyxml2::tinyxml2
    ZLIB::ZLIB
)

# =========================
# Client Source Files
# =========================
file(GLOB_RECURSE CLIENT_SRC
    "src/Core/*.cpp"
    "src/World/*.cpp"
)
list(APPEND CLIENT_SRC "src/main.cpp")

# =========================
# Client & Server Targets
# =========================
if(ANDROID)
    # Build shared library cho Android APK (SDLActivity yêu cầu thư viện tên main)
    add_library(main SHARED ${CLIENT_SRC})
    
    target_include_directories(main PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_link_libraries(main PRIVATE
        OUCommon
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf
        SDL3_net::SDL3_net
        log
    )
else()
    # Build executables cho Windows / PC
    add_executable(OUGameClient ${CLIENT_SRC})
    
    target_include_directories(OUGameClient PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_link_libraries(OUGameClient PRIVATE
        OUCommon
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf
        SDL3_net::SDL3_net
    )

    # =========================
    # Server
    # =========================
    file(GLOB_RECURSE SERVER_SRC
        "src/Server/*.cpp"
    )

    add_executable(OUGameServer ${SERVER_SRC})
    
    target_include_directories(OUGameServer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_link_libraries(OUGameServer PRIVATE
        OUCommon
        SDL3::SDL3
        SDL3_net::SDL3_net
    )

    # =========================
    # Assets copy (client only)
    # =========================
    add_custom_command(TARGET OUGameClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:OUGameClient>/assets
        COMMENT "Copying assets..."
    )

    # =========================
    # Windows DLL copy
    # =========================
    if(WIN32)
        add_custom_command(TARGET OUGameClient POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy -t
            $<TARGET_FILE_DIR:OUGameClient>
            $<TARGET_RUNTIME_DLLS:OUGameClient>
            COMMAND_EXPAND_LISTS
            COMMENT "Copying Client DLLs..."
        )

        add_custom_command(TARGET OUGameServer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy -t
            $<TARGET_FILE_DIR:OUGameServer>
            $<TARGET_RUNTIME_DLLS:OUGameServer>
            COMMAND_EXPAND_LISTS
            COMMENT "Copying Server DLLs..."
        )
    endif()
endif()