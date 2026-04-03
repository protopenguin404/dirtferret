#
# Global setup.
#

cmake_minimum_required(VERSION 3.21)

project(cef)

set(CEF_ROOT "$ENV{CEF_ROOT}")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")

find_package(CEF REQUIRED)

add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

add_subdirectory(src)

