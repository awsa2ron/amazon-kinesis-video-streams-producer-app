cmake_minimum_required(VERSION 3.10.2)

project(libz NONE)

find_program(MAKE_EXE NAMES make)

include(ExternalProject)
set(CONFIGURE_COMMAND ${CMAKE_BINARY_DIR}/libz/src/project_zlib/configure --prefix=${CMAKE_BINARY_DIR})
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