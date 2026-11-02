# 构建补充

在 MSVC 开发终端使用 README 的标准 CMake 命令；不提交本机绝对路径预设。BUILD_TESTING 默认开启，需安装对应 Qt Test 组件。

中文 MSVC 消息资源下，如果 Ninja 无法识别 showIncludes，可显式传入 `-DPIXELGUARD_MSVC_INCLUDES_PREFIX="注意: 包含文件:  "`；其他语言环境让 CMake 自动探测。

示例 OpenCV_DIR 只是占位路径，必须指向本机 OpenCVConfig.cmake 所在目录（vc15/vc16 等目录名由实际安装决定）。
