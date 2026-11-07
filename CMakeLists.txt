cmake_minimum_required(VERSION 3.20)
option(LARSOFT2SUPERA_BUILD_ART "Build against installed LArSoft and art" ON)
option(LARSOFT2SUPERA_BUILD_WRITER_MODULE "Build the art module with the LArCV2 output adapter" ON)
if(LARSOFT2SUPERA_BUILD_ART)
  find_package(cetmodules 3.20 REQUIRED)
endif()
project(larsoft2supera VERSION 0.1.0 LANGUAGES CXX)
if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CTest)
include(GNUInstallDirs)
find_package(Supera 2 CONFIG REQUIRED COMPONENTS data)

if(LARSOFT2SUPERA_BUILD_ART)
  include(CetCMakeEnv)
  cet_cmake_env()
  find_package(art REQUIRED)
  find_package(lardataobj REQUIRED)
  if(LARSOFT2SUPERA_BUILD_WRITER_MODULE)
    find_package(Supera 2 CONFIG REQUIRED COMPONENTS larcv2)
  endif()
else()
  # Fast tests compile the genuine pinned value types, not replacements or mocks.
  # These source trees are supplied by the caller and are never installed.
  foreach(dependency IN ITEMS LARDATAOBJ_SOURCE)
    if(NOT IS_DIRECTORY "${${dependency}}")
      message(FATAL_ERROR "Set ${dependency} to the pinned dependency source tree")
    endif()
  endforeach()
  add_library(test_value_types STATIC
    "${LARDATAOBJ_SOURCE}/lardataobj/RecoBase/OpFlash.cxx")
  target_include_directories(test_value_types SYSTEM PUBLIC
    "${LARDATAOBJ_SOURCE}")
  add_library(lardataobj::RecoBase ALIAS test_value_types)
endif()

add_library(optical STATIC src/Optical.cxx)
add_library(larsoft2supera::optical ALIAS optical)
set_target_properties(optical PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(optical PUBLIC cxx_std_17)
target_compile_options(optical PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_include_directories(optical PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(optical PUBLIC lardataobj::RecoBase supera::data)

if(BUILD_TESTING)
  add_executable(optical_test tests/Optical_test.cxx)
  target_link_libraries(optical_test PRIVATE optical)
  target_compile_options(optical_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME optical_fields COMMAND optical_test)
endif()

if(LARSOFT2SUPERA_BUILD_ART)
  if(LARSOFT2SUPERA_BUILD_WRITER_MODULE)
    include(BuildPlugins)
    add_subdirectory(plugins)
  endif()
  install(TARGETS optical EXPORT opticalTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(DIRECTORY fcl/ DESTINATION ${CMAKE_INSTALL_DATADIR}/larsoft2supera/fcl)
  install(EXPORT opticalTargets NAMESPACE larsoft2supera::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/larsoft2supera)
  include(CMakePackageConfigHelpers)
  configure_package_config_file(cmake/larsoft2superaConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/larsoft2superaConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/larsoft2supera)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/larsoft2superaConfig.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/larsoft2supera)
endif()
