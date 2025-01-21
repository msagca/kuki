cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project("LearningOpenGL")
add_subdirectory(${CMAKE_SOURCE_DIR}/external/glfw)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/glm)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/json)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_FBX_IMPORTER ON)
set(ASSIMP_BUILD_GLTF_IMPORTER ON)
set(ASSIMP_BUILD_ASSIMP_VIEW OFF)
set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF)
set(ASSIMP_INSTALL OFF)
set(ASSIMP_INSTALL_PDB OFF)
set(ASSIMP_WARNINGS_AS_ERRORS OFF)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/assimp)
find_package(OpenGL REQUIRED)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  endforeach()
endif()
add_library(imgui INTERFACE)
target_include_directories(imgui INTERFACE ${CMAKE_SOURCE_DIR}/external/imgui ${CMAKE_SOURCE_DIR}/external/imgui/backends)
target_sources(
  imgui
  INTERFACE
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp
)
file(GLOB_RECURSE CPP_SOURCES "${CMAKE_SOURCE_DIR}/source/*.cpp")
file(GLOB_RECURSE C_SOURCES "${CMAKE_SOURCE_DIR}/source/*.c")
add_executable(${PROJECT_NAME} ${CPP_SOURCES} ${C_SOURCES})
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE OpenGL::GL glfw glm imgui assimp nlohmann_json
)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/third-party)
if(UNIX)
  set(CMAKE_INSTALL_PREFIX $ENV{HOME}/.local)
elseif(WIN32)
  set(CMAKE_INSTALL_PREFIX $ENV{USERPROFILE}/.local)
endif()
set(INSTALL_DIR bin)
install(TARGETS ${PROJECT_NAME} DESTINATION ${INSTALL_DIR})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
# shaders
file(GLOB_RECURSE VERT_SOURCES "${CMAKE_SOURCE_DIR}/source/shader/*.vert")
file(GLOB_RECURSE FRAG_SOURCES "${CMAKE_SOURCE_DIR}/source/shader/*.frag")
set(SHADER_SOURCES ${VERT_SOURCES} ${FRAG_SOURCES})
target_sources(${PROJECT_NAME} PRIVATE ${SHADER_SOURCES})
foreach(FILE ${SHADER_SOURCES})
  cmake_path(APPEND FULL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${FILE})
  list(APPEND SHADER_FILES ${FULL_PATH})
endforeach()
list(LENGTH SHADER_FILES SHADER_COUNT)
if(SHADER_COUNT GREATER 0)
  set(SHADER_FOLDER ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shader)
  add_custom_target(CreateShaderFolder COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_FOLDER})
  add_custom_target(
    CopyShaders
    ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_FILES} ${SHADER_FOLDER}
    DEPENDS ${SHADER_FILES} CreateShaderFolder
  )
  add_dependencies(${PROJECT_NAME} CopyShaders)
  install(DIRECTORY ${SHADER_FOLDER} DESTINATION ${INSTALL_DIR})
endif()
# images
file(GLOB_RECURSE JPG_SOURCES "${CMAKE_SOURCE_DIR}/source/image/*.jpg")
file(GLOB_RECURSE PNG_SOURCES "${CMAKE_SOURCE_DIR}/source/image/*.png")
set(IMAGE_SOURCES ${JPG_SOURCES} ${PNG_SOURCES})
foreach(FILE ${IMAGE_SOURCES})
  cmake_path(APPEND FULL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${FILE})
  list(APPEND IMAGE_FILES ${FULL_PATH})
endforeach()
list(LENGTH IMAGE_FILES IMAGE_COUNT)
if(IMAGE_COUNT GREATER 0)
  set(IMAGE_FOLDER ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/image)
  add_custom_target(CreateImageFolder COMMAND ${CMAKE_COMMAND} -E make_directory ${IMAGE_FOLDER})
  add_custom_target(
    CopyImages
    ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${IMAGE_FILES} ${IMAGE_FOLDER}
    DEPENDS ${IMAGE_FILES} CreateImageFolder
  )
  add_dependencies(${PROJECT_NAME} CopyImages)
  install(DIRECTORY ${IMAGE_FOLDER} DESTINATION ${INSTALL_DIR})
endif()
# models
file(GLOB_RECURSE FBX_SOURCES "${CMAKE_SOURCE_DIR}/source/model/*.fbx")
file(GLOB_RECURSE GLTF_SOURCES "${CMAKE_SOURCE_DIR}/source/model/*.gltf")
file(GLOB_RECURSE BIN_SOURCES "${CMAKE_SOURCE_DIR}/source/model/*.bin")
file(GLOB_RECURSE OBJ_SOURCES "${CMAKE_SOURCE_DIR}/source/model/*.obj")
set(MODEL_SOURCES ${FBX_SOURCES} ${GLTF_SOURCES} ${BIN_SOURCES} ${OBJ_SOURCES})
foreach(FILE ${MODEL_SOURCES})
  cmake_path(APPEND FULL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${FILE})
  list(APPEND MODEL_FILES ${FULL_PATH})
endforeach()
list(LENGTH MODEL_FILES MODEL_COUNT)
if(MODEL_COUNT GREATER 0)
  set(MODEL_FOLDER ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/model)
  add_custom_target(CreateModelFolder COMMAND ${CMAKE_COMMAND} -E make_directory ${MODEL_FOLDER})
  add_custom_target(
    CopyModels
    ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MODEL_FILES} ${MODEL_FOLDER}
    DEPENDS ${MODEL_FILES} CreateModelFolder
  )
  add_dependencies(${PROJECT_NAME} CopyModels)
  install(DIRECTORY ${MODEL_FOLDER} DESTINATION ${INSTALL_DIR})
endif()
