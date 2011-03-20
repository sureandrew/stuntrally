# - Find MyGUI includes and library
#
# This module defines
# MYGUI_INCLUDE_DIRS
# MYGUI_LIBRARIES, the libraries to link against to use MYGUI.
# MYGUI_LIB_DIR, the location of the libraries
# MYGUI_FOUND, If false, do not try to use MYGUI
#
# Copyright © 2007, Matt Williams
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
CMAKE_POLICY(PUSH)

IF (MYGUI_LIBRARIES AND MYGUI_INCLUDE_DIRS)
    SET(MYGUI_FIND_QUIETLY TRUE)
ENDIF (MYGUI_LIBRARIES AND MYGUI_INCLUDE_DIRS)

IF (WIN32) #Windows
    MESSAGE(STATUS "Looking for MyGUI")
    SET(OGRESDK $ENV{OGRE_HOME})
    SET(OGRESOURCE $ENV{OGRE_SRC})
    IF (OGRESDK)
        MESSAGE(STATUS "Using MyGUI in OGRE SDK")
        STRING(REGEX REPLACE "[\\]" "/" OGRESDK "${OGRESDK}" )
        SET(MYGUI_INCLUDE_DIRS ${OGRESDK}/include/MYGUI)
        SET(MYGUI_LIB_DIR ${OGRESDK}/lib)
        SET(MYGUI_LIBRARIES debug MyGUI_d optimized MyGUI)
    ENDIF (OGRESDK)
    IF (OGRESOURCE)
        MESSAGE(STATUS "Using MyGUI in OGRE dependencies")
        STRING(REGEX REPLACE "[\\]" "/" OGRESDK "${OGRESOURCE}" )
        SET(MYGUI_INCLUDE_DIRS ${OGRESOURCE}/OgreMain/include/MYGUI)
        SET(MYGUI_LIB_DIR ${OGRESOURCE}/lib)
        SET(MYGUI_LIBRARIES debug MyGUI_d optimized MyGUI)
    ENDIF (OGRESOURCE)
ELSE (WIN32) #Unix
    CMAKE_MINIMUM_REQUIRED(VERSION 2.4.7 FATAL_ERROR)
    FIND_PACKAGE(PkgConfig)
    PKG_SEARCH_MODULE(MYGUI MYGUI MyGUI)
    IF (MYGUI_INCLUDE_DIRS)
        SET(MYGUI_INCLUDE_DIRS ${MYGUI_INCLUDE_DIRS})
        SET(MYGUI_LIB_DIR ${MYGUI_LIBDIR})
        SET(MYGUI_LIBRARIES ${MYGUI_LIBRARIES} CACHE STRING "")
    ELSE (MYGUI_INCLUDE_DIRS)
        FIND_PATH(MYGUI_INCLUDE_DIRS MyGUI.h PATHS /usr/local/include /usr/include PATH_SUFFIXES MyGUI MYGUI)
        FIND_LIBRARY(MYGUI_LIBRARIES mygui PATHS /usr/lib /usr/local/lib)
        SET(MYGUI_LIB_DIR ${MYGUI_LIBRARIES})
        STRING(REGEX REPLACE "(.*)/.*" "\\1" MYGUI_LIB_DIR "${MYGUI_LIB_DIR}")
        STRING(REGEX REPLACE ".*/" "" MYGUI_LIBRARIES "${MYGUI_LIBRARIES}")
    ENDIF (MYGUI_INCLUDE_DIRS)
ENDIF (WIN32)

#Do some preparation
SEPARATE_ARGUMENTS(MYGUI_INCLUDE_DIRS)
SEPARATE_ARGUMENTS(MYGUI_LIBRARIES)

SET(MYGUI_INCLUDE_DIRS ${MYGUI_INCLUDE_DIRS} CACHE PATH "")
SET(MYGUI_LIBRARIES ${MYGUI_LIBRARIES} CACHE STRING "")
SET(MYGUI_LIB_DIR ${MYGUI_LIB_DIR} CACHE PATH "")

IF (MYGUI_INCLUDE_DIRS AND MYGUI_LIBRARIES)
    SET(MYGUI_FOUND TRUE)
ENDIF (MYGUI_INCLUDE_DIRS AND MYGUI_LIBRARIES)

IF (MYGUI_FOUND)
    IF (NOT MYGUI_FIND_QUIETLY)
        MESSAGE(STATUS " libraries : ${MYGUI_LIBRARIES} from ${MYGUI_LIB_DIR}")
        MESSAGE(STATUS " includes : ${MYGUI_INCLUDE_DIRS}")
    ENDIF (NOT MYGUI_FIND_QUIETLY)
ELSE (MYGUI_FOUND)
    IF (MYGUI_FIND_REQUIRED)
        MESSAGE(FATAL_ERROR "Could not find MYGUI")
    ENDIF (MYGUI_FIND_REQUIRED)
ENDIF (MYGUI_FOUND)

CMAKE_POLICY(POP)