cmake_minimum_required(VERSION 2.8) 
project(moka)                           # 项目名称，会生成PROJECT_SOURCE_DIR和PROJECT_BINARY_DIR两个环境变量

set(CMAKE_VERBOSE_MAKEFILE ON)          # 从makefile中构建更详细的输出

# -rdynamic: 将所有符号都加入到符号表中，便于使用dlopen或者backtrace追踪到符号
# -Wno-deprecated: 不要警告过时的特性
# -Wno-unused-function: 不要警告未使用函数
# -fpic 生成位置无关代码
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function") # 添加编译C++文件时要使用的默认编译标志

find_library(YAMLCPP yaml-cpp)          # 查找链接库yaml-cpp为库名称，若找到则存储在YAMLCPP变量中

set(LIB_SRC                             # 设置变量
  moka/log.cc
  moka/util.cc
  moka/config.cc
)

add_library(moka SHARED ${LIB_SRC})     # 添加名为libmoka.so的动态库，再指定生成库的源文件

# 添加测试文件
add_executable(test tests/test.cc)      # 添加可执行文件，test作为可执行文件的名称
add_dependencies(test moka)             # 使test依赖于moka(即moka改变，test就要重新编译)
target_link_libraries(test moka ${YAMLCPP})

add_executable(test_config tests/test_config.cc)      # 添加可执行文件，test作为可执行文件的名称
add_dependencies(test_config moka)             # 使test依赖于moka(即moka改变，test就要重新编译)
target_link_libraries(test_config moka ${YAMLCPP})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)  # 设置可执行文件的生成位置，这里设置为bin目录下
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)     # 设置库文件的生成位置(方便去找)