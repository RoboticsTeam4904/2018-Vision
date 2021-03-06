cmake_minimum_required(VERSION 3.9)
project(2018_field)

set(CMAKE_CXX_STANDARD 11)

find_package( Threads )
find_package( OpenCV REQUIRED )
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_BOT_LOCALIZE "Include the botlocale module" ON)
option(BUILD_OBJ_TRACKING "Include object tracking" ON)

option(USING_GPU 1)
option(USING_CUDNN 1)
option(USING_OPENMP 0)

option(DOWNLOAD_WEIGHTS "Download the latest cube weights" OFF)

set(DARKNET_DEBUG 0)
set(DARKNET_LIBSO_NAME libdarknet.so)

set(DATA_DIR ${PROJECT_SOURCE_DIR}/data/)
set(PROV_CRATE_URL "https://crate.botprovoking.org/fieldac/")
set(PROV_CRATE_WEIGHTS_FOLDER "weights/")
set(PROV_CRATE_WEIGHTS_FILENAME "cube-full-v2.weights")
set(PROV_CRATE_WEIGHTS_FILEMD5 947de697d772a6d31790a3ae89626978)

if(UNIX AND NOT APPLE)
    set(NTCORE_SHARED_FILENAME "libntcore.so")
    set(NTCORE_SHARED_FILEMD5 3dc47662ddf949a2212fa0cd02265940)
    set(WPIUTIL_SHARED_FILENAME "libwpiutil.so")
    set(WPIUTIL_SHARED_FILEMD5 2e9e8f0634a247c4b559da18555ee8e6)
elseif(UNIX AND APPLE)
    set(NTCORE_SHARED_FILENAME "libntcore.dylib")
    set(NTCORE_SHARED_FILEMD5 "7fcbd6a85e1034ea2eb53baecc5d49e0")
    set(WPIUTIL_SHARED_FILENAME "libwpiutil.dylib")
    set(WPIUTIL_SHARED_FILEMD5 d22a2f7f0c7787cfa35caa7a704b0afe)
endif()

include(DownloadProject.cmake)

if(BUILD_OBJ_TRACKING)
    add_definitions(-DOBJ_TRACKING)

    file(GLOB NETWORK_SRC src/network/*.cpp)
    file(GLOB OBJ_TRK_SRC src/objecttracking/*.cpp)

    ### ALEXEYAB/DARKNET START ###
    download_project(PROJ darknet
            GIT_REPOSITORY https://github.com/RoboticsTeam4904/darknet
            GIT_TAG botprov-compat
            GIT_SHALLOW 1)

    include_directories(${darknet_SOURCE_DIR}/src/)
    ### ALEXEYAB/DARKNET END ###
endif()

if(BUILD_BOT_LOCALIZE)
    add_definitions(-DBOT_LOCALIZE)

    file(GLOB BOT_LCL_SRC src/botlocale/*.cpp)

    ### RPLIDAR SDK START ###
    download_project(PROJ rplidar
            URL https://download.slamtec.com/api/download/rplidar-sdk/1.5.7
            URL_MD5 e2a894e706f9ec6118967101c11d1dee
            DOWNLOAD_NAME rplidar-sdk.zip
            DOWNLOAD_NO_PROGRESS 1)

    set(rplidar_SDK_DIR ${rplidar_SOURCE_DIR}/sdk/sdk/)
    execute_process(COMMAND uname -s
            OUTPUT_VARIABLE RPLIDAR_PLATFORM
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(rplidar_OUTPUT_DIR ${rplidar_SOURCE_DIR}/sdk/output/${RPLIDAR_PLATFORM}/Release/)

    include_directories(${rplidar_SDK_DIR}/include/)
    link_directories(${rplidar_OUTPUT_DIR})
    ### RPLIDAR SDK END ###
endif()
file(GLOB BOT_TRK_SRC src/bottracking/*.cpp)
file(GLOB CORE_SRC src/*.cpp)


### NTCORE START ###
download_project(PROJ ntcore
        GIT_REPOSITORY https://github.com/wpilibsuite/ntcore
        GIT_TAG 3025a18
        GIT_SHALLOW 1)

# Download the shared objects from our
# artifact host as compiling them on the fly
# sucks. Same goes for #{wpiutil-shared}
download_project(PROJ ntcore-shared
        URL "${PROV_CRATE_URL}${NTCORE_SHARED_FILENAME}"
        URL_MD5 ${NTCORE_SHARED_FILEMD5}
        DOWNLOAD_NAME ${NTCORE_SHARED_FILENAME}
        DOWNLOAD_NO_EXTRACT 1
        DOWNLOAD_NO_PROGRESS 1)

include_directories(${ntcore_SOURCE_DIR}/src/main/native/include/)
include_directories(${ntcore_SOURCE_DIR}/src/main/native/cpp/)
### NTCORE END ###

### WPIUTIL START ###
download_project(PROJ wpiutil
        GIT_REPOSITORY https://github.com/wpilibsuite/wpiutil
        GIT_TAG 71d06a1
        GIT_SHALLOW 1)

download_project(PROJ wpiutil-shared
        URL "${PROV_CRATE_URL}${WPIUTIL_SHARED_FILENAME}"
        URL_MD5 ${WPIUTIL_SHARED_FILEMD5}
        DOWNLOAD_NAME ${WPIUTIL_SHARED_FILENAME}
        DOWNLOAD_NO_EXTRACT 1
        DOWNLOAD_NO_PROGRESS 1)

include_directories(${wpiutil_SOURCE_DIR}/src/main/native/include/)
### WPIUTIL END ###

### WEIGHTS START
if(DOWNLOAD_WEIGHTS)
    download_project(PROJ crate-weights
            URL "${PROV_CRATE_URL}${PROV_CRATE_WEIGHTS_FOLDER}${PROV_CRATE_WEIGHTS_FILENAME}"
            URL_MD5 ${PROV_CRATE_WEIGHTS_FILEMD5}
            DOWNLOAD_NAME ${PROV_CRATE_WEIGHTS_FILENAME}
            DOWNLOAD_NO_EXTRACT 1
            DOWNLOAD_NO_PROGRESS 1)
endif()
### WEIGHTS END ###

link_directories(${CMAKE_BINARY_DIR})

set(FINAL_SRC ${CORE_SRC} ${BOT_TRK_SRC})

if(BUILD_BOT_LOCALIZE)
    set(FINAL_SRC ${FINAL_SRC} ${BOT_LCL_SRC})
endif()

if(BUILD_OBJ_TRACKING)
    set(FINAL_SRC ${FINAL_SRC} ${OBJ_TRK_SRC} ${NETWORK_SRC})
endif()

add_executable(field ${FINAL_SRC})

if(BUILD_BOT_LOCALIZE)
    add_custom_command(TARGET field
            PRE_LINK
            COMMAND make build_sdk
            WORKING_DIRECTORY ${rplidar_SDK_DIR}
            COMMENT "Building ${rplidar_SDK_DIR}"
            BYPRODUCTS ${rplidar_OUTPUT_DIR}/librplidar_sdk.a)

    target_link_libraries(field rplidar_sdk)
endif()

if(BUILD_OBJ_TRACKING)
    add_custom_command(TARGET field
            PRE_LINK
            COMMAND make obj libdarknet.so GPU=${USING_GPU} CUDNN=${USING_CUDNN} OPENCV=1 TRACK_OPTFLOW=1 DEBUG=${DARKNET_DEBUG} OPENMP=${USING_OPENMP} LIBSO=1 LIBNAMESO=${DARKNET_LIBSO_NAME}
            COMMAND cp ${DARKNET_LIBSO_NAME} ${CMAKE_BINARY_DIR}/
            COMMENT "Building ${DARKNET_LIBSO_NAME}..."
            BYPRODUCTS ${CMAKE_BINARY_DIR}/${DARKNET_LIBSO_NAME}
            WORKING_DIRECTORY ${darknet_SOURCE_DIR})

    target_link_libraries(field darknet)
endif()

add_custom_command(TARGET field
        PRE_LINK
        COMMAND cp ntcore-shared-download/ntcore-shared-download-prefix/src/${NTCORE_SHARED_FILENAME} ${CMAKE_BINARY_DIR}/
        COMMENT "Copying ntcore..."
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_custom_command(TARGET field
        PRE_LINK
        COMMAND cp wpiutil-shared-download/wpiutil-shared-download-prefix/src/${WPIUTIL_SHARED_FILENAME} ${CMAKE_BINARY_DIR}/
        COMMENT "Copying wpiutil..."
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

if(${DOWNLOAD_WEIGHTS} EQUAL 1 AND NOT EXISTS "${DATA_DIR}${PROV_CRATE_WEIGHTS_FILENAME}")
    add_custom_command(TARGET field
            PRE_LINK
            COMMAND mv crate-weights-download/crate-weights-download-prefix/src/${PROV_CRATE_WEIGHTS_FILENAME} ${DATA_DIR}
            COMMENT "Moving weights ${PROV_CRATE_WEIGHTS_FILENAME}..."
            BYPRODUCTS ${DATA_DIR}${PROV_CRATE_WEIGHTS_FILENAME}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

target_link_libraries(field ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(field ntcore)
target_link_libraries(field ${OpenCV_LIBS})

