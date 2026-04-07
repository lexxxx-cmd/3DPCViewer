# 3DPCViewer

3DPCViewer是一个基于Qt和OpenSceneGraph的3D点云可视化工具，支持从ROS bag文件中读取和显示点云位姿图像等数据。

## 系统要求

- Windows 10/11
- Visual Studio 2019或更高版本
- CMake 3.16或更高版本
- Git

## 依赖项

项目依赖以下库：

- Qt 5.x（Core, Gui, Widgets, Concurrent）
- PCL (Point Cloud Library)
- OpenSceneGraph 3.6.5（osg, osgDB, osgGA, osgViewer, osgUtil, osgQt）
- Vcpkg（用于依赖管理）

## 安装步骤

### 1. 安装Vcpkg

首先，安装Vcpkg包管理器：

```bash
# 克隆Vcpkg仓库
git clone https://github.com/microsoft/vcpkg.git

# 运行安装脚本（Windows）
cd vcpkg
bootstrap-vcpkg.bat
cd ..

# 设置环境变量，将Vcpkg添加到系统路径
# 或者在命令行中指定Vcpkg路径
```

### 2. 安装依赖项

使用Vcpkg安装项目依赖：

```bash
# 安装OpenSceneGraph
vcpkg install osg osg-qt
# 安装Qt5
# 安装PCL
```

### 3. 克隆项目

```bash
git clone <项目仓库地址> 3DPCViewer
cd 3DPCViewer
```

## 构建步骤

### 1. 配置CMake

使用CMake生成Visual Studio解决方案：

```bash
# 创建构建目录
mkdir build
cd build

# 配置CMake，指定Vcpkg工具链文件
cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg路径>/scripts/buildsystems/vcpkg.cmake

# 或者使用GUI界面配置
# cmake-gui ..
```

### 2. 编译项目

使用Visual Studio打开生成的解决方案文件（3DPCViewer.sln），然后：

1. 选择适当的配置（Debug/Release）
2. 右键点击解决方案，选择"生成解决方案"

或者使用命令行编译：

```bash
# 使用MSBuild编译
msbuild 3DPCViewer.sln /p:Configuration=Release

# 或者使用CMake编译
cmake --build . --config Release
```

## 运行说明

编译完成后，可执行文件位于`build\Release`目录（或`build\Debug`目录，取决于编译配置）。

运行程序：

```bash
# 从构建目录运行
cd build\Release
3DPCViewer.exe

# 或者直接双击可执行文件
```

## 项目结构

```
3DPCViewer/
├── cmake/            # CMake配置文件
├── resources/        # 资源文件
├── src/              # 源代码
│   ├── core/         # 核心功能
│   ├── io/           # 数据读取相关
│   ├── node/         # 可视化节点
│   ├── ui/           # 用户界面
│   └── main.cpp      # 主入口
├── third_party/      # 第三方库
│   └── ros1bag_reader/  # ROS bag文件读取器
├── CMakeLists.txt    # 主CMake配置文件
├── vcpkg.json        # Vcpkg清单模式配置文件
├── vcpkg-configuration.json
└── README.md         # 项目说明
```
