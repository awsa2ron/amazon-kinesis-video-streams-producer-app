cmake_minimum_required(VERSION 2.8)

project(libz-download NONE)

find_program(MAKE_EXE NAMES make)

include(ExternalProject)
set(CONFIGURE_COMMAND ${CMAKE_BINARY_DIR}/libz/src/project_zlib/configure --prefix=${KinesisVideoProducerC_SOURCE_DIR}/open-source)
ExternalProject_Add(project_zlib
                    PREFIX          ${CMAKE_BINARY_DIR}/libz
                    GIT_REPOSITORY  https://github.com/madler/zlib.git
                    GIT_TAG         v1.2.9
                    CONFIGURE_COMMAND ${CONFIGURE_COMMAND} 
                    BUILD_COMMAND     ${MAKE_EXE}
                    BUILD_IN_SOURCE   TRUE
                    INSTALL_COMMAND   ${MAKE_EXE} install
                    TEST_COMMAND    ""
)