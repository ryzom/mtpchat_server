# Look in local CMakeModules, specified variable CMAKE_MODULE_PATH or environment variable CMAKE_MODULE_PATH
SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMakeModules ${CMAKE_MODULE_PATH} $ENV{CMAKE_MODULE_PATH})

IF(EXISTS ${CMAKE_SOURCE_DIR}/CMakeModules/.hg/hgrc)
  # Don't try to include common.cmake because it could have been remotely modified
  SET(REMOTE_CMAKE_MODULES_FOUND ON)
ELSE()
  SET(REMOTE_CMAKE_MODULES_FOUND OFF)

  # Try to include common.cmake
  INCLUDE(common OPTIONAL)
ENDIF()

# If forcing remote modules OR module not found OR found a Merucial repository, try to clone/update it
IF(WITH_REMOTE_CMAKE_MODULES OR NOT COMMON_MODULE_FOUND)
  FIND_PROGRAM(HG_EXECUTABLE hg
    PATHS
      /opt/local/bin
      "C:/Program Files/TortoiseHg"
      "C:/Program Files (x86)/TortoiseHg"
    )

  IF(HG_EXECUTABLE)
    SET(HG_REPOSITORY "http://hg.kervala.net/cmake_modules")

    IF(REMOTE_CMAKE_MODULES_FOUND)
      EXECUTE_PROCESS(COMMAND ${HG_EXECUTABLE} pull -u
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/CMakeModules
        ERROR_VARIABLE HG_ERRORS
        RESULT_VARIABLE HG_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      IF(HG_RESULT EQUAL 0)
        MESSAGE(STATUS "CMake modules successfully updated")
      ELSE()
        MESSAGE(FATAL_ERROR "Unable to pull CMake modules: ${HG_ERRORS}")
      ENDIF()
    ELSE()
      EXECUTE_PROCESS(COMMAND ${HG_EXECUTABLE} clone ${HG_REPOSITORY} CMakeModules
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        ERROR_VARIABLE HG_ERRORS
        RESULT_VARIABLE HG_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      IF(HG_RESULT EQUAL 0)
        MESSAGE(STATUS "CMake modules successfully cloned")
      ELSE()
        MESSAGE(FATAL_ERROR "Unable to clone CMake modules: ${HG_ERRORS}")
      ENDIF()
    ENDIF()

    # retry to include common.cmake
    INCLUDE(common)
  ELSE()
    MESSAGE(FATAL_ERROR "Unable to find Mercurial to clone CMake modules repository!")
  ENDIF()
ENDIF()
