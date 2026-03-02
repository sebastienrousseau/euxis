# Install script for directory: /home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-build/libixwebsocket.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ixwebsocket" TYPE FILE FILES
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXBench.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXCancellationRequest.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXConnectionInfo.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXConnectionState.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXDNSLookup.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXExponentialBackoff.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXHttp.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXHttpClient.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXHttpServer.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXNetSystem.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXProgressCallback.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXSelectInterrupt.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXSelectInterruptFactory.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXSetThreadName.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXSocket.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXSocketConnect.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXSocketFactory.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXSocketServer.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXSocketTLSOptions.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXUdpSocket.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXUrlParser.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXUtf8Validator.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXUserAgent.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocket.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketCloseConstants.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketCloseInfo.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketErrorInfo.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketHandshake.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketHandshakeKeyGen.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketHttpHeaders.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketInitResult.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketMessage.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketMessageType.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketOpenInfo.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketPerMessageDeflate.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketPerMessageDeflateCodec.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketPerMessageDeflateOptions.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketSendInfo.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketServer.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketTransport.h"
    "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketVersion.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-config.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-config.cmake"
         "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-build/CMakeFiles/Export/dbc99e06a99e696141dafd40631f8060/ixwebsocket-config.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-config-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-config.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket" TYPE FILE FILES "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-build/CMakeFiles/Export/dbc99e06a99e696141dafd40631f8060/ixwebsocket-config.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^()$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket" TYPE FILE FILES "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-build/CMakeFiles/Export/dbc99e06a99e696141dafd40631f8060/ixwebsocket-config-noconfig.cmake")
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/seb/.euxis/euxis-cpp/build-cov/_deps/ixwebsocket-build/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
