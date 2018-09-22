cmake_minimum_required(VERSION 3.5)

project(LearningGLES)

set(LEARNING_GLES_SOURCES
  Window.cpp
  WaylandWindow.cpp
)

set(LEARNING_GLES_LIBRARIES
  -lEGL
  -lGLESv2
  -lwayland-client
  -lwayland-egl
)

add_library(LearningGLES ${LEARNING_GLES_SOURCES})
target_link_libraries(LearningGLES PUBLIC ${LEARNING_GLES_LIBRARIES})

add_executable(example main.cpp)
target_link_libraries(example PUBLIC LearningGLES)
