cmake_minimum_required(VERSION 3.16)
project(lz-disk-cleaner VERSION 1.0.0 LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 配置
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
)

# ==================== 主程序 ====================

# 源文件
set(SOURCES
    src/main.cpp
    src/core/diskanalyzer.cpp
    src/core/diskcleaner.cpp
    src/core/systeminfo.cpp
    src/core/fileshredder.cpp
    src/gui/mainwindow.cpp
    src/gui/dashboardwidget.cpp
    src/gui/analyzewidget.cpp
    src/gui/resourceswidget.cpp
    src/gui/historychart.cpp
    src/gui/cleanupdialog.cpp
    src/gui/progressdialog.cpp
    src/gui/fileshredderwidget.cpp
    src/gui/systemslimmerwidget.cpp
    src/core/systemslimmer.cpp
    src/utils/logger.cpp
    src/utils/config.cpp
)

# 头文件
set(HEADERS
    src/core/diskanalyzer.h
    src/core/diskcleaner.h
    src/core/systeminfo.h
    src/core/fileshredder.h
    src/gui/mainwindow.h
    src/gui/dashboardwidget.h
    src/gui/analyzewidget.h
    src/gui/resourceswidget.h
    src/gui/historychart.h
    src/gui/cleanupdialog.h
    src/gui/progressdialog.h
    src/gui/fileshredderwidget.h
    src/gui/systemslimmerwidget.h
    src/core/systemslimmer.h
    src/utils/logger.h
    src/utils/config.h
)

# 资源文件
set(RESOURCES
    resources/resources.qrc
)

# 创建可执行文件
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# 链接 Qt 库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ==================== Helper 程序（需要提权的操作）====================

set(HELPER_SOURCES
    src/helper/main.cpp
)

add_executable(${PROJECT_NAME}-helper
    ${HELPER_SOURCES}
)

target_link_libraries(${PROJECT_NAME}-helper PRIVATE
    Qt6::Core
)

set_target_properties(${PROJECT_NAME}-helper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ==================== 安装配置 ====================

# 安装主程序
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# 安装 helper 程序
install(TARGETS ${PROJECT_NAME}-helper
    RUNTIME DESTINATION bin
)

# 安装 PolicyKit 策略文件
install(FILES src/helper/com.deepin.pkexec.disk-cleaner.policy
    DESTINATION /usr/share/polkit-1/actions
)