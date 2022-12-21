cmake_minimum_required(VERSION 3.14.0)

project(NSWCalibration_extern)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

if(NSW_STANDALONE_CI_BUILD)
  message(STATUS " Executing a standalone CI build, fetching NSW dependencies")
  option(NSW_CI_BUILD_USE_EXTERNAL_NSWCONFIG CACHE ON)
  if(NOT NSW_DEP_FETCH_METHOD)
    set(NSW_DEP_FETCH_METHOD "ci" CACHE INTERNAL "")
  else()
    set(NSW_DEP_FETCH_METHOD "${NSW_DEP_FETCH_METHOD}" CACHE INTERNAL "")
  endif()
endif()

option(NSW_EXTERN_WARN "Enable compiler warnings for extern" OFF)
if(NOT NSW_EXTERN_WARN)
  ## Disable compiler warnings in all external components
  add_compile_options(-w)
endif()
