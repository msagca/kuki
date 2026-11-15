# The root of a game's own repository, with kuki pinned as a submodule in `external/kuki`.
#
# kuki is added as a subdirectory rather than found as an installed package, so the engine is built
# from source with the game: a change to it is one rebuild away, and the submodule's commit says
# exactly which engine the game was built against. kuki builds only its engine when it is not the
# top-level project -- no editor, no sample games, no tests, no installer.
cmake_minimum_required(VERSION 3.31)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
project(MyGame LANGUAGES C CXX)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/kuki/CMakeLists.txt")
  message(
    FATAL_ERROR
    "external/kuki is empty. Run `git submodule update --init --recursive` first."
  )
endif()
add_subdirectory(external/kuki)
add_subdirectory(game)
