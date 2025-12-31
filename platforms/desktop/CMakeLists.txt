# See how to install sdl2: https://wiki.libsdl.org/SDL2/Installation
cmake_minimum_required (VERSION 3.8)
project(DesktopBuild)

# リポジトリのルート（platforms/desktop から 2つ上）
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# LovyanGFX
add_definitions(-DLGFX_SDL)

# ビルド対象にするファイルを指定する;
# LovyanGFXのあるパスと位置関係を変えた場合は相対パス記述を環境に合わせて調整すること;
# LovyanGFX のルート
set(LGFX_ROOT "${REPO_ROOT}/dependencies/LovyanGFX")

# LovyanGFX の実装ファイル一式（不足分を補完）
file(GLOB LGFX_SRCS
  "${LGFX_ROOT}/src/lgfx/v1/*.cpp"
  "${LGFX_ROOT}/src/lgfx/v1/misc/*.cpp"
  "${LGFX_ROOT}/src/lgfx/v1/panel/Panel_Device.cpp"
  "${LGFX_ROOT}/src/lgfx/v1/panel/Panel_FrameBufferBase.cpp"
  "${LGFX_ROOT}/src/lgfx/v1/platforms/sdl/*.cpp"
  "${LGFX_ROOT}/src/lgfx/utility/*.c"
  "${LGFX_ROOT}/src/lgfx/Fonts/efont/*.c"
  "${LGFX_ROOT}/src/lgfx/Fonts/IPA/*.c"
)

add_library(lovyangfx STATIC ${LGFX_SRCS})
target_include_directories(lovyangfx PUBLIC "${LGFX_ROOT}/src")
target_compile_features(lovyangfx PUBLIC cxx_std_17)


IF (CMAKE_SYSTEM_NAME MATCHES "Linux")

# Linux環境の場合;
# SDL2_INCLUDE_DIRS と SDL2_LIBRARIES を事前に設定しておく;
    find_package(SDL2 REQUIRED SDL2)
    include_directories(${SDL2_INCLUDE_DIRS})
    # target_link_libraries(lovyangfx -lpthread ${SDL2_LIBRARIES})
    target_include_directories(lovyangfx PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(lovyangfx PRIVATE -lpthread ${SDL2_LIBRARIES})

ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")

# Windows環境の場合;
# SDL2を配置したパス内のincludeのパスを指定する;
    target_include_directories(lovyangfx PUBLIC "C:/SDL2/include")

# SDL2を配置したパス内の.libファイルのパスを指定する;
    target_link_libraries(lovyangfx PUBLIC "C:/SDL2/lib/x64/SDL2.lib")


    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>")

ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Darwin")

# MacOS(Darwin)環境の場合;
    find_package(SDL2 REQUIRED COMPONENTS SDL2)
    target_link_libraries(lovyangfx PRIVATE SDL2::SDL2)

ENDIF ()

target_compile_features(lovyangfx PUBLIC cxx_std_17)

# Mooncake 
add_subdirectory(dependencies/mooncake)

# Smooth ui toolkit 
add_subdirectory(dependencies/smooth_ui_toolkit)

# App layer
file(GLOB_RECURSE APP_LAYER_SRCS
    "${REPO_ROOT}/app/*.c"
    "${REPO_ROOT}/app/*.cpp"
)
# デスクトップではIDF依存の実装を除外（必要になったら戻す）
list(FILTER APP_LAYER_SRCS EXCLUDE REGEX ".*/app/libs/measurement_pipeline/.*")
list(FILTER APP_LAYER_SRCS EXCLUDE REGEX ".*/components/cal_store/src/.*")
message(STATUS "APP_LAYER_SRCS=${APP_LAYER_SRCS}")

set(APP_LAYER_INCS
    "${REPO_ROOT}/app"
)

add_library(app_layer ${APP_LAYER_SRCS})
target_include_directories(app_layer PUBLIC ${APP_LAYER_INCS})
target_include_directories(app_layer PRIVATE
    "${REPO_ROOT}/app/libs/measurement_pipeline/include"
    "${REPO_ROOT}/components/cal_store/include"
    "${REPO_ROOT}/dependencies/LovyanGFX/src"
    "${CMAKE_CURRENT_LIST_DIR}/shims"
)

target_link_libraries(app_layer PUBLIC mooncake smooth_ui_toolkit lovyangfx)

set(EXTRA_INCS 
    dependencies/LovyanGFX/src/
)

# Desktop build（platforms/desktop 配下だけを拾う）
file(GLOB APP_DESKTOP_BUILD_SRCS
  "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/hal_desktop/*.cpp"
)
add_executable(app_desktop_build ${APP_DESKTOP_BUILD_SRCS})
message(STATUS "APP_DESKTOP_BUILD_SRCS=${APP_DESKTOP_BUILD_SRCS}")
target_include_directories(app_desktop_build PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}"
  "${CMAKE_CURRENT_LIST_DIR}/shims"
  "${REPO_ROOT}/dependencies/LovyanGFX/src"
)
target_link_libraries(app_desktop_build PUBLIC app_layer lovyangfx)

# 设置构建路径
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build/desktop)

# 确保构建路径存在
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR})

# 设置生成的可执行文件输出路径
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}) 

# # 设置生成的库文件输出路径
# set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
