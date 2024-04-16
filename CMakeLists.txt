cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

cmake_policy(SET CMP0015 NEW)
set(LIB_NAME "telnet_server")
PROJECT (${LIB_NAME})

set(CROSS_COMPILE  aarch64-xilinx-linux-)
set(CMAKE_C_COMPILER aarch64-xilinx-linux-gcc)
set(CMAKE_CXX_COMPILER aarch64-xilinx-linux-g++)

MESSAGE(STATUS "This is CROSS COMPILE=" ${CROSS_COMPILE})
MESSAGE(STATUS "CROSS COMPILE_C=" ${CMAKE_C_COMPILER})
MESSAGE(STATUS "CROSS COMPILE_CXX=" ${CMAKE_CXX_COMPILER})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -march=armv8-a -O0")

include_directories(	
	../SDK/include.20220614/
)
link_directories(
    ../SDK/lib.20220614/
)

ADD_EXECUTABLE (telnet_server telnet_server.cpp curl_handle.cpp cJSON.c)
target_link_libraries(telnet_server 
	pthread
	zzmf_dev.so
	curl
	#qcap.arm64.so
	#qvidsink.so
	
)



