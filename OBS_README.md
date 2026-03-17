# OBS 打包说明

## 项目信息
- 项目名称: lz-disk-cleaner
- 版本: 1.1.0
- 源码地址: https://github.com/tonglingcn/lz-disk-cleaner

## 支持的发行版

### Debian/Ubuntu 系列
- Debian 12 (Bookworm)
- Debian 13 (Trixie)
- Ubuntu 22.04 (Jammy)
- Ubuntu 24.04 (Noble)
- Deepin V23
- Deepin V25
- UOS 20

### Fedora/openSUSE 系列
- Fedora 39
- Fedora 40
- openSUSE Leap 15.5
- openSUSE Tumbleweed

## 上传到 OBS 步骤

### 1. 安装 osc 客户端
```bash
# Debian/Ubuntu
sudo apt install osc

# Fedora
sudo dnf install osc

# openSUSE
sudo zypper install osc
```

### 2. 配置 osc
编辑 ~/.oscrc 文件:
```
[general]
apiurl = https://api.opensuse.org

[https://api.opensuse.org]
user = 你的用户名
pass = 你的密码
```

### 3. 创建 OBS 项目
```bash
# 创建项目 (在 OBS Web 界面操作更方便)
osc meta prj -e home:你的用户名:lz-disk-cleaner

# 或者使用命令行创建包
osc mkpac home:你的用户名:lz-disk-cleaner lz-disk-cleaner
```

### 4. 上传文件
```bash
# 进入 OBS 工作目录
cd /path/to/obs/home:你的用户名:lz-disk-cleaner/lz-disk-cleaner

# 复制必要文件
cp /home/deepin/Documents/lz-disk-cleaner_1.1.0.orig.tar.gz .
cp /home/deepin/Documents/deepin-disk-cleaner/lz-disk-cleaner.spec .
cp /home/deepin/Documents/deepin-disk-cleaner/_service .

# 复制 debian 目录 (用于 Deb系构建)
cp -r /home/deepin/Documents/deepin-disk-cleaner/debian .

# 添加并提交文件
osc addremove
osc commit -m "Initial import v1.1.0"
```

### 5. 配置构建目标
在 OBS Web 界面中，为项目添加 Repository:
- Debian_12
- Debian_13
- xUbuntu_22.04
- xUbuntu_24.04
- Fedora_39
- Fedora_40
- openSUSE_Leap_15.5
- openSUSE_Tumbleweed

## 文件列表

| 文件 | 用途 |
|------|------|
| lz-disk-cleaner_1.1.0.orig.tar.gz | 源码压缩包 |
| lz-disk-cleaner.spec | RPM 构建规范 |
| debian/* | Debian 包构建文件 |
| _service | OBS 服务配置 (可选) |

## 注意事项

1. Qt6 版本要求 >= 6.4，部分旧发行版可能不支持
2. 对于 Qt5 系统 (如 UOS)，需要修改 CMakeLists.txt 添加 Qt5 支持
3. polkit 策略文件需要根据发行版调整路径

## Debian 包构建依赖

在 OBS 中添加 Debian 构建时，需要确保以下依赖可用:
- qt6-base-dev (>= 6.4)
- qt6-base-dev-tools (>= 6.4)
- libqt6core6 (>= 6.4)
- libqt6gui6 (>= 6.4)
- libqt6widgets6 (>= 6.4)
- libqt6network6 (>= 6.4)
- libqt6svg6 (>= 6.4)
