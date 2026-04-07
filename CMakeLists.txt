cmake_minimum_required(VERSION 3.15)
project(TaskScheduler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 1. Detect Apple Silicon to add architecture-specific optimizations
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    message(STATUS "Build target: Apple Silicon (M3/M1/M2)")
    # -O3: Maximize speed
    # -mcpu=native: Tells compiler to use M3 specific instructions (NEON, etc)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -mcpu=native")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
endif()

# 2. Find Threads correctly (Works for macOS pthreads & Linux pthread)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# Include directories
include_directories(include)

# Create Executable
add_executable(scheduler src/main.cpp)

# 3. Link Threads (Do NOT use -lpthread manually)
target_link_libraries(scheduler PRIVATE Threads::Threads)