add_definitions(--std=c++11)

add_executable(xvrrun xvrrun.cpp axembed.c patcher.cpp ${PROJECT_SOURCE_DIR}/xvrrun.res)

add_executable(xvrrunhost xvrrunhost.cpp)