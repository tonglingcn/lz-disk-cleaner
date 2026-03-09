#!/bin/bash

# deepin25 磁盘空间分析工具
# 用于分析系统磁盘占用情况，特别是磐石系统和玲珑应用
# 支持安全清理磁盘空间

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

# 全局变量
TOTAL_FREED_SPACE=0
CLEANUP_LOG="/tmp/disk_cleanup_$(date +%Y%m%d_%H%M%S).log"

# 打印带颜色的信息
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_header() {
    echo -e "${CYAN}==========================================${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}==========================================${NC}"
}

print_menu_item() {
    echo -e "  ${GREEN}$1)${NC} $2"
}

# 检查是否具有sudo权限
check_sudo() {
    if [ "$EUID" -ne 0 ]; then
        print_warning "某些功能需要sudo权限，请在需要时输入密码"
    fi
}

# 获取目录大小（人类可读格式）
get_dir_size() {
    local dir="$1"
    if [ -e "$dir" ]; then
        du -sh "$dir" 2>/dev/null | cut -f1
    else
        echo "0"
    fi
}

# 获取目录大小（字节数）
get_dir_size_bytes() {
    local dir="$1"
    if [ -e "$dir" ]; then
        du -sb "$dir" 2>/dev/null | cut -f1
    else
        echo "0"
    fi
}

# 确认操作
confirm_action() {
    local message="$1"
    local default="${2:-n}"
    
    echo ""
    echo -e "${YELLOW}⚠ $message${NC}"
    echo -ne "${CYAN}是否继续? [y/N]: ${NC}"
    read -r response
    
    case "$response" in
        [yY][eE][sS]|[yY]) 
            return 0
            ;;
        *)
            print_info "操作已取消"
            return 1
            ;;
    esac
}

# 记录清理日志
log_cleanup() {
    local action="$1"
    local size="$2"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "[$timestamp] $action - 释放空间: $size" >> "$CLEANUP_LOG"
}

# 获取目录用途说明
get_dir_purpose() {
    local dir_name=$(basename "$1")
    case "$dir_name" in
        "data")
            echo "应用数据目录"
            ;;
        "config")
            echo "配置文件目录"
            ;;
        "logs")
            echo "日志文件目录"
            ;;
        "backups")
            echo "备份文件目录"
            ;;
        "temp"|"tmp")
            echo "临时文件目录"
            ;;
        "cache")
            echo "缓存文件目录"
            ;;
        "Documents")
            echo "用户文档目录"
            ;;
        "Desktop")
            echo "桌面文件目录"
            ;;
        "Downloads")
            echo "下载文件目录"
            ;;
        ".config")
            echo "应用配置目录"
            ;;
        ".cache")
            echo "应用缓存目录"
            ;;
        ".local")
            echo "本地应用数据目录"
            ;;
        "dingtalk")
            echo "钉钉应用数据"
            ;;
        "wechat")
            echo "微信应用数据"
            ;;
        "weixin")
            echo "企业微信数据"
            ;;
        "qq")
            echo "QQ应用数据"
            ;;
        "tim")
            echo "TIM应用数据"
            ;;
        *)
            echo "其他数据"
            ;;
    esac
}

# 1. 磁盘整体占用分析
analyze_disk_usage() {
    print_header "1. 磁盘整体占用分析"
    
    print_info "执行 df -Th 命令分析磁盘使用情况..."
    df -Th | grep -E "(Filesystem|/dev/sd|/dev/nvme|persistent|home)" | awk '
    BEGIN {
        printf "%-20s %-10s %-10s %-10s %-10s %s\n", "文件系统", "类型", "总大小", "已使用", "可用", "挂载点"
        printf "-------------------------------------------------------------------\n"
    }
    {
        if (NR > 1) {
            printf "%-20s %-10s %-10s %-10s %-10s %s\n", $1, $2, $3, $4, $5, $7
        }
    }'
    
    echo ""
    print_info "分析 persistent 分区占用情况..."
    if [ -d "/persistent" ]; then
        echo "persistent 分区主要目录占用:"
        echo "-------------------------------------------------------------------"
        sudo du -sh /persistent/* 2>/dev/null | sort -hr | head -10 | while read size dir; do
            purpose=$(get_dir_purpose "$dir")
            printf "%-15s %-30s %s\n" "$size" "$(basename "$dir")" "$purpose"
        done
    else
        print_warning "未找到 persistent 分区"
    fi
    
    echo ""
    print_info "分析 home 目录占用情况 (按大小排序)..."
    if [ -d "$HOME" ]; then
        echo "用户主目录占用分析:"
        echo "====================================================================="
        
        # 获取一级目录并按大小排序
        sudo du -sh "$HOME"/* 2>/dev/null | sort -hr | head -15 | while read size dir; do
            if [ -d "$dir" ]; then
                dir_name=$(basename "$dir")
                purpose=$(get_dir_purpose "$dir")
                echo ""
                echo "[$size] $dir_name - $purpose"
                echo "---------------------------------------------------------------------"
                
                # 对于大目录，进一步分析子目录
                if [[ "$size" =~ ^[0-9]+(\.[0-9]+)?[G] ]]; then
                    # 如果目录大于1G，深入分析子目录
                    sudo du -sh "$dir"/* 2>/dev/null | sort -hr | head -5 | while read sub_size sub_dir; do
                        if [ -e "$sub_dir" ]; then
                            sub_name=$(basename "$sub_dir")
                            sub_purpose=$(get_dir_purpose "$sub_dir")
                            printf "  %-12s %-40s %s\n" "$sub_size" "$sub_name" "$sub_purpose"
                        fi
                    done
                fi
            fi
        done
        
        # 特殊应用数据目录分析
        echo ""
        echo "特殊应用数据目录分析:"
        echo "====================================================================="
        for app_path in ".config/dingtalk" ".config/wechat" ".config/weixin" ".config/qq" ".config/tim" ".cache"; do
            if [ -d "$HOME/$app_path" ]; then
                size=$(du -sh "$HOME/$app_path" 2>/dev/null | cut -f1)
                purpose=$(get_dir_purpose "$app_path")
                echo "[$size] $app_path - $purpose"
                
                # 深入分析大应用数据目录
                if [[ "$size" =~ ^[0-9]+(\.[0-9]+)?[G] ]]; then
                    echo "  子目录分析:"
                    sudo du -sh "$HOME/$app_path"/* 2>/dev/null | sort -hr | head -3 | while read sub_size sub_dir; do
                        sub_name=$(basename "$sub_dir")
                        printf "    %-12s %s\n" "$sub_size" "$sub_name"
                    done
                fi
            fi
        done
    fi
}

# 2. 磐石系统占用分析
analyze_immutable_system() {
    print_header "2. 磐石系统占用分析"
    
    print_info "检查磐石系统状态..."
    
    # 检查 deepin-immutable-ctl 命令是否存在
    if ! command -v deepin-immutable-ctl &> /dev/null; then
        print_error "未找到 deepin-immutable-ctl 命令，可能不是磐石系统"
        return 1
    fi
    
    # 获取所有部署
    print_info "获取所有部署信息..."
    if sudo deepin-immutable-ctl admin status -d all &> /dev/null; then
        echo "所有部署:"
        sudo deepin-immutable-ctl admin status -d all 2>/dev/null | grep -E "(部署|大小)" | head -20
    else
        print_error "无法获取部署信息"
    fi
    
    echo ""
    print_info "获取修改层信息..."
    if sudo deepin-immutable-ctl admin status -d modified &> /dev/null; then
        echo "修改层:"
        sudo deepin-immutable-ctl admin status -d modified 2>/dev/null
    else
        print_error "无法获取修改层信息"
    fi
    
    echo ""
    print_info "分析快照信息..."
    
    # 使用正确的命令获取快照列表
    if sudo deepin-immutable-ctl snapshot list &> /dev/null; then
        echo "快照列表:"
        sudo deepin-immutable-ctl snapshot list 2>/dev/null
        echo ""
    fi
    
    # 检查快照目录
    if [ -d "/boot/deepin-snapshots" ]; then
        echo "快照目录占用情况:"
        sudo du -sh /boot/deepin-snapshots/* 2>/dev/null | sort -hr | while read size dir; do
            snapshot_name=$(basename "$dir")
            printf "%-15s %s\n" "$size" "$snapshot_name"
        done
        
        # 快照数量
        snapshot_count=$(sudo ls /boot/deepin-snapshots/ 2>/dev/null | wc -l)
        echo ""
        print_info "快照总数: $snapshot_count"
    else
        print_warning "未找到快照目录 /boot/deepin-snapshots"
    fi
}

# 3. 玲珑应用分析
analyze_linglong_apps() {
    print_header "3. 玲珑应用分析"
    
    # 检查 ll-cli 命令
    if ! command -v ll-cli &> /dev/null; then
        print_error "未找到 ll-cli 命令，可能未安装玲珑"
        return 1
    fi
    
    print_info "获取玲珑应用列表..."
    
    # 获取应用列表
    echo "已安装的玲珑应用:"
    ll-cli list 2>/dev/null | grep -v "^Total" | while read app; do
        if [ -n "$app" ]; then
            echo "  - $app"
        fi
    done
    
    # 统计应用数量
    app_count=$(ll-cli list 2>/dev/null | grep -v "^Total" | grep -v "^$" | wc -l)
    echo ""
    print_info "玲珑应用总数: $app_count"
    
    echo ""
    print_info "分析玲珑应用磁盘占用..."
    
    if [ -d "/var/lib/linglong/layers" ]; then
        echo "玲珑应用层磁盘占用 (Top 15):"
        echo "====================================================================="
        cd /var/lib/linglong/layers
        
        # 使用指定的命令分析磁盘占用
        du -sh * 2>/dev/null | sort -hr | head -15 | while read size dir; do
            if [ -d "$dir" ]; then
                # 显示目录树结构
                echo ""
                echo "[$size] $dir"
                tree -L 1 "$dir" 2>/dev/null | head -10
            fi
        done
        
        echo ""
        print_info "统计 base 和 runtime..."
        
        # 使用 ll-cli list 统计 base 和 runtime
        echo "Base 和 Runtime 分析:"
        echo "====================================================================="
        
        # 从 ll-cli list 中查找 base
        echo "Base 列表:"
        ll-cli list 2>/dev/null | grep -i base | while read base; do
            if [ -n "$base" ]; then
                echo "  - $base"
            fi
        done
        
        # 从 ll-cli list 中查找 runtime
        echo ""
        echo "Runtime 列表:"
        ll-cli list 2>/dev/null | grep -i runtime | while read runtime; do
            if [ -n "$runtime" ]; then
                echo "  - $runtime"
            fi
        done
        
        # 统计数量
        base_count=$(ll-cli list 2>/dev/null | grep -i base | wc -l)
        runtime_count=$(ll-cli list 2>/dev/null | grep -i runtime | wc -l)
        
        echo ""
        echo "统计结果:"
        echo "  Base 数量: $base_count"
        echo "  Runtime 数量: $runtime_count"
        
        # 分析 base 和 runtime 的依赖关系
        if command -v jq &> /dev/null; then
            echo ""
            print_info "分析 base 和 runtime 依赖关系..."
            echo "应用依赖关系 (App <- Base):"
            echo "====================================================================="
            
            find . -name "info.json" -type f 2>/dev/null | while read info_file; do
                if [ -f "$info_file" ]; then
                    app_info=$(sudo cat "$info_file" 2>/dev/null | jq -r 'select(.kind == "app") | "\(.id // .appid) <- \(.base)"' 2>/dev/null)
                    if [ -n "$app_info" ]; then
                        echo "$app_info"
                    fi
                fi
            done | sort | uniq
        else
            print_warning "未找到 jq 命令，无法分析依赖关系"
        fi
        
        cd - > /dev/null
    else
        print_error "未找到玲珑应用层目录 /var/lib/linglong/layers"
    fi
}

# ============================================================
# 清理功能模块
# ============================================================

# 分析可清理项目
analyze_cleanable_items() {
    print_header "可清理项目分析"
    
    local total_cleanable=0
    echo ""
    
    # 1. 用户缓存
    if [ -d "$HOME/.cache" ]; then
        local cache_size=$(get_dir_size "$HOME/.cache")
        echo -e "${GREEN}[用户缓存]${NC} ~/.cache: ${YELLOW}$cache_size${NC}"
        echo "  - 包含应用程序缓存文件"
        echo "  - 清理后应用程序会自动重建缓存"
        echo ""
    fi
    
    # 2. 缩略图缓存
    if [ -d "$HOME/.cache/thumbnails" ]; then
        local thumb_size=$(get_dir_size "$HOME/.cache/thumbnails")
        echo -e "${GREEN}[缩略图缓存]${NC} ~/.cache/thumbnails: ${YELLOW}$thumb_size${NC}"
        echo "  - 图片和视频文件的缩略图缓存"
        echo "  - 清理后会自动重新生成"
        echo ""
    fi
    
    # 3. 包管理器缓存
    if [ -d "/var/cache/apt/archives" ]; then
        local apt_size=$(get_dir_size "/var/cache/apt/archives")
        echo -e "${GREEN}[APT缓存]${NC} /var/cache/apt/archives: ${YELLOW}$apt_size${NC}"
        echo "  - 已下载的deb包缓存"
        echo "  - 清理后不影响已安装的软件"
        echo ""
    fi
    
    # 4. 系统日志
    if command -v journalctl &> /dev/null; then
        local journal_size=$(journalctl --disk-usage 2>/dev/null | grep -oP '\d+\.\d+[GM]' || echo "未知")
        echo -e "${GREEN}[系统日志]${NC} Journal日志: ${YELLOW}$journal_size${NC}"
        echo "  - systemd日志文件"
        echo "  - 可安全清理旧日志"
        echo ""
    fi
    
    # 5. 临时文件
    if [ -d "/tmp" ]; then
        local tmp_size=$(get_dir_size "/tmp")
        echo -e "${GREEN}[临时文件]${NC} /tmp: ${YELLOW}$tmp_size${NC}"
        echo "  - 系统临时文件"
        echo "  - 重启后会自动清理"
        echo ""
    fi
    
    # 6. 用户临时文件
    if [ -d "$HOME/.local/share/Trash" ]; then
        local trash_size=$(get_dir_size "$HOME/.local/share/Trash")
        echo -e "${GREEN}[回收站]${NC} ~/.local/share/Trash: ${YELLOW}$trash_size${NC}"
        echo "  - 用户删除的文件"
        echo "  - 清理后无法恢复"
        echo ""
    fi
    
    # 7. 下载目录中的安装包
    if [ -d "$HOME/Downloads" ]; then
        local deb_count=$(find "$HOME/Downloads" -maxdepth 1 -name "*.deb" -type f 2>/dev/null | wc -l)
        local deb_size=$(find "$HOME/Downloads" -maxdepth 1 -name "*.deb" -type f -exec du -ch {} + 2>/dev/null | grep total$ | cut -f1)
        if [ "$deb_count" -gt 0 ]; then
            echo -e "${GREEN}[安装包]${NC} ~/Downloads/*.deb: ${YELLOW}$deb_size${NC} ($deb_count 个文件)"
            echo "  - 已下载的deb安装包"
            echo "  - 清理前请确认不再需要"
            echo ""
        fi
    fi
    
    # 8. 磐石系统快照
    if [ -d "/boot/deepin-snapshots" ]; then
        local snapshot_count=$(sudo ls /boot/deepin-snapshots/ 2>/dev/null | wc -l)
        local snapshot_total_size=$(get_dir_size "/boot/deepin-snapshots")
        echo -e "${GREEN}[系统快照]${NC} /boot/deepin-snapshots: ${YELLOW}$snapshot_total_size${NC} ($snapshot_count 个快照)"
        echo "  - 磐石系统快照"
        echo "  - 保留足够的快照用于系统恢复"
        echo ""
    fi
    
    # 9. 玲珑应用旧版本
    if [ -d "/var/lib/linglong/layers" ]; then
        local linglong_size=$(get_dir_size "/var/lib/linglong/layers")
        echo -e "${GREEN}[玲珑应用]${NC} /var/lib/linglong/layers: ${YELLOW}$linglong_size${NC}"
        echo "  - 玲珑应用和运行时环境"
        echo "  - 可清理不需要的应用"
        echo ""
    fi
    
    # 10. 浏览器缓存
    for browser_cache in "$HOME/.cache/google-chrome" "$HOME/.cache/chromium" "$HOME/.cache/mozilla" "$HOME/.cache/yandex"; do
        if [ -d "$browser_cache" ]; then
            local browser_name=$(basename "$browser_cache")
            local browser_size=$(get_dir_size "$browser_cache")
            echo -e "${GREEN}[浏览器缓存]${NC} $browser_name: ${YELLOW}$browser_size${NC}"
            echo "  - 浏览器缓存和临时文件"
            echo "  - 清理后网页加载可能变慢"
            echo ""
        fi
    done
    
    # 11. Python pip 缓存
    if [ -d "$HOME/.cache/pip" ]; then
        local pip_size=$(get_dir_size "$HOME/.cache/pip")
        echo -e "${GREEN}[Pip缓存]${NC} ~/.cache/pip: ${YELLOW}$pip_size${NC}"
        echo "  - Python包缓存"
        echo "  - 清理后不影响已安装的包"
        echo ""
    fi
    
    # 12. npm/yarn 缓存
    if [ -d "$HOME/.npm" ]; then
        local npm_size=$(get_dir_size "$HOME/.npm")
        echo -e "${GREEN}[NPM缓存]${NC} ~/.npm: ${YELLOW}$npm_size${NC}"
        echo "  - Node.js包缓存"
        echo ""
    fi
}

# 清理用户缓存
clean_user_cache() {
    print_header "清理用户缓存"
    
    local cache_dir="$HOME/.cache"
    if [ ! -d "$cache_dir" ]; then
        print_warning "缓存目录不存在"
        return 0
    fi
    
    local before_size=$(get_dir_size "$cache_dir")
    echo "当前缓存大小: $before_size"
    
    echo ""
    echo "缓存目录详情:"
    du -sh "$cache_dir"/* 2>/dev/null | sort -hr | head -10
    
    if ! confirm_action "即将清理用户缓存目录，某些应用可能需要重新生成缓存"; then
        return 0
    fi
    
    print_info "清理缓存..."
    
    # 排除一些重要的缓存目录
    local exclude_dirs=("fontconfig" "dconf" "gstreamer-1.0" "mesa_shader_cache" "glsl_shader")
    
    for item in "$cache_dir"/*; do
        if [ -e "$item" ]; then
            local item_name=$(basename "$item")
            local should_clean=true
            
            for exclude in "${exclude_dirs[@]}"; do
                if [ "$item_name" = "$exclude" ]; then
                    should_clean=false
                    print_info "保留: $item_name"
                    break
                fi
            done
            
            if [ "$should_clean" = true ]; then
                rm -rf "$item" 2>/dev/null && print_success "已清理: $item_name" || print_warning "无法清理: $item_name"
            fi
        fi
    done
    
    local after_size=$(get_dir_size "$cache_dir")
    echo ""
    print_success "缓存清理完成"
    print_info "清理前: $before_size -> 清理后: $after_size"
    log_cleanup "用户缓存" "$before_size"
}

# 清理缩略图缓存
clean_thumbnail_cache() {
    print_header "清理缩略图缓存"
    
    local thumb_dir="$HOME/.cache/thumbnails"
    if [ ! -d "$thumb_dir" ]; then
        print_warning "缩略图缓存目录不存在"
        return 0
    fi
    
    local before_size=$(get_dir_size "$thumb_dir")
    echo "当前缩略图缓存大小: $before_size"
    
    if ! confirm_action "即将清理缩略图缓存"; then
        return 0
    fi
    
    print_info "清理缩略图..."
    rm -rf "$thumb_dir"/* 2>/dev/null
    
    local after_size=$(get_dir_size "$thumb_dir")
    print_success "缩略图缓存清理完成: $before_size -> $after_size"
    log_cleanup "缩略图缓存" "$before_size"
}

# 清理APT缓存
clean_apt_cache() {
    print_header "清理APT包管理器缓存"
    
    if [ "$EUID" -ne 0 ]; then
        print_warning "需要sudo权限"
    fi
    
    local apt_cache="/var/cache/apt/archives"
    if [ ! -d "$apt_cache" ]; then
        print_warning "APT缓存目录不存在"
        return 0
    fi
    
    local before_size=$(sudo du -sh "$apt_cache" 2>/dev/null | cut -f1)
    echo "当前APT缓存大小: $before_size"
    
    echo ""
    echo "缓存的deb包:"
    ls -lh "$apt_cache"/*.deb 2>/dev/null | head -10
    
    if ! confirm_action "即将清理APT缓存（已安装的软件不受影响）"; then
        return 0
    fi
    
    print_info "清理APT缓存..."
    sudo apt-get clean
    sudo apt-get autoclean
    
    local after_size=$(sudo du -sh "$apt_cache" 2>/dev/null | cut -f1)
    print_success "APT缓存清理完成: $before_size -> $after_size"
    log_cleanup "APT缓存" "$before_size"
}

# 清理系统日志
clean_journal_logs() {
    print_header "清理系统日志"
    
    if ! command -v journalctl &> /dev/null; then
        print_warning "未找到journalctl命令"
        return 0
    fi
    
    echo "当前日志占用:"
    journalctl --disk-usage
    
    echo ""
    echo "清理选项:"
    echo "  1) 保留最近7天的日志"
    echo "  2) 保留最近14天的日志"
    echo "  3) 保留最近30天的日志"
    echo "  4) 保留最近100MB的日志"
    echo "  5) 取消"
    echo ""
    echo -ne "请选择 [1-5]: "
    read -r choice
    
    case $choice in
        1)
            if confirm_action "保留最近7天的日志"; then
                sudo journalctl --vacuum-time=7d
                print_success "日志清理完成"
            fi
            ;;
        2)
            if confirm_action "保留最近14天的日志"; then
                sudo journalctl --vacuum-time=14d
                print_success "日志清理完成"
            fi
            ;;
        3)
            if confirm_action "保留最近30天的日志"; then
                sudo journalctl --vacuum-time=30d
                print_success "日志清理完成"
            fi
            ;;
        4)
            if confirm_action "保留最近100MB的日志"; then
                sudo journalctl --vacuum-size=100M
                print_success "日志清理完成"
            fi
            ;;
        *)
            print_info "操作已取消"
            ;;
    esac
    
    echo ""
    echo "清理后日志占用:"
    journalctl --disk-usage
    log_cleanup "系统日志" "用户选择"
}

# 清理临时文件
clean_temp_files() {
    print_header "清理临时文件"
    
    local tmp_size=$(get_dir_size "/tmp")
    echo "/tmp 目录大小: $tmp_size"
    
    if [ -d "$HOME/.local/share/Trash" ]; then
        local trash_size=$(get_dir_size "$HOME/.local/share/Trash")
        echo "回收站大小: $trash_size"
    fi
    
    echo ""
    echo "清理选项:"
    echo "  1) 清理系统临时文件 (/tmp)"
    echo "  2) 清空回收站"
    echo "  3) 全部清理"
    echo "  4) 取消"
    echo ""
    echo -ne "请选择 [1-4]: "
    read -r choice
    
    case $choice in
        1|3)
            if confirm_action "清理系统临时文件 (可能影响正在运行的程序)"; then
                print_info "清理/tmp..."
                sudo rm -rf /tmp/* 2>/dev/null || true
                print_success "临时文件清理完成"
            fi
            ;;&
        2|3)
            if [ -d "$HOME/.local/share/Trash" ]; then
                if confirm_action "清空回收站 (文件将无法恢复)"; then
                    print_info "清空回收站..."
                    rm -rf "$HOME/.local/share/Trash"/* 2>/dev/null || true
                    print_success "回收站已清空"
                fi
            fi
            ;;
        *)
            print_info "操作已取消"
            ;;
    esac
    log_cleanup "临时文件" "用户选择"
}

# 清理下载目录中的安装包
clean_downloaded_packages() {
    print_header "清理下载的安装包"
    
    local downloads_dir="$HOME/Downloads"
    if [ ! -d "$downloads_dir" ]; then
        print_warning "下载目录不存在"
        return 0
    fi
    
    echo "查找下载目录中的安装包..."
    echo ""
    
    # 查找各种安装包
    local deb_files=$(find "$downloads_dir" -maxdepth 2 -name "*.deb" -type f 2>/dev/null)
    local appimage_files=$(find "$downloads_dir" -maxdepth 2 -name "*.AppImage" -type f 2>/dev/null)
    local snap_files=$(find "$downloads_dir" -maxdepth 2 -name "*.snap" -type f 2>/dev/null)
    local flatpak_files=$(find "$downloads_dir" -maxdepth 2 -name "*.flatpak" -type f 2>/dev/null)
    
    local all_packages=""
    [ -n "$deb_files" ] && all_packages="$deb_files"
    [ -n "$appimage_files" ] && all_packages="$all_packages$appimage_files"
    [ -n "$snap_files" ] && all_packages="$all_packages$snap_files"
    [ -n "$flatpak_files" ] && all_packages="$all_packages$flatpak_files"
    
    if [ -z "$all_packages" ]; then
        print_info "未找到安装包文件"
        return 0
    fi
    
    echo "找到以下安装包:"
    echo ""
    
    local total_size=0
    declare -a packages_array
    
    # 显示deb文件
    if [ -n "$deb_files" ]; then
        echo -e "${GREEN}DEB包:${NC}"
        while IFS= read -r file; do
            local size=$(du -sh "$file" 2>/dev/null | cut -f1)
            local filename=$(basename "$file")
            echo "  [$size] $filename"
            packages_array+=("$file")
        done <<< "$deb_files"
        echo ""
    fi
    
    # 显示AppImage文件
    if [ -n "$appimage_files" ]; then
        echo -e "${GREEN}AppImage包:${NC}"
        while IFS= read -r file; do
            local size=$(du -sh "$file" 2>/dev/null | cut -f1)
            local filename=$(basename "$file")
            echo "  [$size] $filename"
            packages_array+=("$file")
        done <<< "$appimage_files"
        echo ""
    fi
    
    # 显示snap文件
    if [ -n "$snap_files" ]; then
        echo -e "${GREEN}Snap包:${NC}"
        while IFS= read -r file; do
            local size=$(du -sh "$file" 2>/dev/null | cut -f1)
            local filename=$(basename "$file")
            echo "  [$size] $filename"
            packages_array+=("$file")
        done <<< "$snap_files"
        echo ""
    fi
    
    # 显示flatpak文件
    if [ -n "$flatpak_files" ]; then
        echo -e "${GREEN}Flatpak包:${NC}"
        while IFS= read -r file; do
            local size=$(du -sh "$file" 2>/dev/null | cut -f1)
            local filename=$(basename "$file")
            echo "  [$size] $filename"
            packages_array+=("$file")
        done <<< "$flatpak_files"
        echo ""
    fi
    
    echo "清理选项:"
    echo "  1) 清理全部安装包"
    echo "  2) 只清理DEB包"
    echo "  3) 只清理AppImage"
    echo "  4) 手动选择"
    echo "  5) 取消"
    echo ""
    echo -ne "请选择 [1-5]: "
    read -r choice
    
    case $choice in
        1)
            if confirm_action "删除所有安装包文件"; then
                for file in "${packages_array[@]}"; do
                    rm -f "$file" && print_success "已删除: $(basename "$file")"
                done
            fi
            ;;
        2)
            if confirm_action "删除所有DEB包"; then
                while IFS= read -r file; do
                    rm -f "$file" && print_success "已删除: $(basename "$file")"
                done <<< "$deb_files"
            fi
            ;;
        3)
            if confirm_action "删除所有AppImage"; then
                while IFS= read -r file; do
                    rm -f "$file" && print_success "已删除: $(basename "$file")"
                done <<< "$appimage_files"
            fi
            ;;
        4)
            print_info "请手动检查下载目录"
            ;;
        *)
            print_info "操作已取消"
            ;;
    esac
    log_cleanup "安装包" "用户选择"
}

# 清理磐石系统快照
clean_snapshots() {
    print_header "清理磐石系统快照"
    
    if ! command -v deepin-immutable-ctl &> /dev/null; then
        print_error "未找到 deepin-immutable-ctl 命令"
        return 1
    fi
    
    echo "当前快照列表:"
    sudo deepin-immutable-ctl snapshot list 2>/dev/null
    
    echo ""
    if [ -d "/boot/deepin-snapshots" ]; then
        echo "快照占用详情:"
        sudo du -sh /boot/deepin-snapshots/* 2>/dev/null | sort -hr
    fi
    
    echo ""
    print_warning "删除快照可能导致无法恢复到之前的系统状态"
    echo ""
    echo "清理选项:"
    echo "  1) 删除指定快照"
    echo "  2) 只保留最新的N个快照"
    echo "  3) 取消"
    echo ""
    echo -ne "请选择 [1-3]: "
    read -r choice
    
    case $choice in
        1)
            echo -ne "请输入要删除的快照编号: "
            read -r snapshot_id
            if [ -n "$snapshot_id" ]; then
                if confirm_action "删除快照 $snapshot_id"; then
                    sudo deepin-immutable-ctl snapshot delete "$snapshot_id"
                    print_success "快照 $snapshot_id 已删除"
                fi
            fi
            ;;
        2)
            echo -ne "保留最新的几个快照? [建议至少3个]: "
            read -r keep_count
            if [[ "$keep_count" =~ ^[0-9]+$ ]] && [ "$keep_count" -ge 1 ]; then
                if confirm_action "保留最新 $keep_count 个快照"; then
                    local total=$(sudo ls /boot/deepin-snapshots/ 2>/dev/null | wc -l)
                    if [ "$total" -gt "$keep_count" ]; then
                        print_info "将删除 $((total - keep_count)) 个旧快照..."
                        # 这里需要根据实际命令调整
                        print_warning "请使用 deepin-immutable-ctl snapshot delete 命令手动删除旧快照"
                    else
                        print_info "快照数量不超过 $keep_count，无需清理"
                    fi
                fi
            else
                print_error "无效的数量"
            fi
            ;;
        *)
            print_info "操作已取消"
            ;;
    esac
    log_cleanup "系统快照" "用户选择"
}

# 清理玲珑应用
clean_linglong_apps() {
    print_header "清理玲珑应用"
    
    if ! command -v ll-cli &> /dev/null; then
        print_error "未找到 ll-cli 命令"
        return 1
    fi
    
    echo "已安装的玲珑应用:"
    ll-cli list 2>/dev/null | grep -v "^Total"
    
    echo ""
    echo "玲珑应用占用详情:"
    if [ -d "/var/lib/linglong/layers" ]; then
        sudo du -sh /var/lib/linglong/layers/* 2>/dev/null | sort -hr | head -15
    fi
    
    echo ""
    echo "清理选项:"
    echo "  1) 卸载指定应用"
    echo "  2) 清理旧版本应用"
    echo "  3) 清理未使用的运行时"
    echo "  4) 取消"
    echo ""
    echo -ne "请选择 [1-4]: "
    read -r choice
    
    case $choice in
        1)
            echo ""
            echo -ne "请输入要卸载的应用ID (格式: appid/version): "
            read -r app_id
            if [ -n "$app_id" ]; then
                if confirm_action "卸载应用 $app_id"; then
                    ll-cli uninstall "$app_id"
                    print_success "应用 $app_id 已卸载"
                fi
            fi
            ;;
        2)
            print_info "检查旧版本应用..."
            # 显示所有版本
            echo ""
            print_warning "请手动检查并卸载不需要的旧版本应用"
            print_info "使用命令: ll-cli uninstall <appid/version>"
            ;;
        3)
            if confirm_action "清理未使用的运行时环境"; then
                # 这里需要分析依赖关系来确定哪些runtime未被使用
                print_info "分析运行时使用情况..."
                print_warning "建议手动检查runtime使用情况后再清理"
            fi
            ;;
        *)
            print_info "操作已取消"
            ;;
    esac
    log_cleanup "玲珑应用" "用户选择"
}

# 清理浏览器缓存
clean_browser_cache() {
    print_header "清理浏览器缓存"
    
    echo "检测到的浏览器缓存:"
    echo ""
    
    declare -a browser_caches
    declare -a browser_names
    
    if [ -d "$HOME/.cache/google-chrome" ]; then
        local size=$(get_dir_size "$HOME/.cache/google-chrome")
        echo "  [Chrome] $size"
        browser_caches+=("$HOME/.cache/google-chrome/Default/Cache")
        browser_caches+=("$HOME/.cache/google-chrome/Default/Code Cache")
        browser_names+=("Chrome")
    fi
    
    if [ -d "$HOME/.cache/chromium" ]; then
        local size=$(get_dir_size "$HOME/.cache/chromium")
        echo "  [Chromium] $size"
        browser_caches+=("$HOME/.cache/chromium/Default/Cache")
        browser_names+=("Chromium")
    fi
    
    if [ -d "$HOME/.cache/mozilla" ]; then
        local size=$(get_dir_size "$HOME/.cache/mozilla")
        echo "  [Firefox] $size"
        browser_caches+=("$HOME/.cache/mozilla/firefox")
        browser_names+=("Firefox")
    fi
    
    if [ ${#browser_caches[@]} -eq 0 ]; then
        print_info "未检测到浏览器缓存"
        return 0
    fi
    
    echo ""
    if ! confirm_action "清理浏览器缓存 (清理后首次打开网页可能较慢)"; then
        return 0
    fi
    
    for cache_dir in "${browser_caches[@]}"; do
        if [ -d "$cache_dir" ]; then
            rm -rf "$cache_dir"/* 2>/dev/null && print_success "已清理: $cache_dir"
        fi
    done
    log_cleanup "浏览器缓存" "用户选择"
}

# 清理开发工具缓存
clean_dev_cache() {
    print_header "清理开发工具缓存"
    
    echo "检测到的开发工具缓存:"
    echo ""
    
    # Python pip
    if [ -d "$HOME/.cache/pip" ]; then
        local pip_size=$(get_dir_size "$HOME/.cache/pip")
        echo "  [Pip] ~/.cache/pip: $pip_size"
    fi
    
    # npm
    if [ -d "$HOME/.npm" ]; then
        local npm_size=$(get_dir_size "$HOME/.npm")
        echo "  [NPM] ~/.npm: $npm_size"
    fi
    
    # yarn
    if [ -d "$HOME/.cache/yarn" ]; then
        local yarn_size=$(get_dir_size "$HOME/.cache/yarn")
        echo "  [Yarn] ~/.cache/yarn: $yarn_size"
    fi
    
    # Go modules
    if [ -d "$HOME/go/pkg" ]; then
        local go_size=$(get_dir_size "$HOME/go/pkg")
        echo "  [Go] ~/go/pkg: $go_size"
    fi
    
    # Cargo
    if [ -d "$HOME/.cargo/registry" ]; then
        local cargo_size=$(get_dir_size "$HOME/.cargo/registry")
        echo "  [Cargo] ~/.cargo/registry: $cargo_size"
    fi
    
    # Maven
    if [ -d "$HOME/.m2/repository" ]; then
        local maven_size=$(get_dir_size "$HOME/.m2/repository")
        echo "  [Maven] ~/.m2/repository: $maven_size"
    fi
    
    # Gradle
    if [ -d "$HOME/.gradle/caches" ]; then
        local gradle_size=$(get_dir_size "$HOME/.gradle/caches")
        echo "  [Gradle] ~/.gradle/caches: $gradle_size"
    fi
    
    echo ""
    echo "清理选项:"
    echo "  1) 清理Pip缓存"
    echo "  2) 清理NPM缓存"
    echo "  3) 清理Go模块缓存"
    echo "  4) 清理Cargo缓存"
    echo "  5) 清理所有开发工具缓存"
    echo "  6) 取消"
    echo ""
    echo -ne "请选择 [1-6]: "
    read -r choice
    
    case $choice in
        1)
            if [ -d "$HOME/.cache/pip" ]; then
                if confirm_action "清理Pip缓存"; then
                    rm -rf "$HOME/.cache/pip"/*
                    print_success "Pip缓存已清理"
                fi
            fi
            ;;
        2)
            if [ -d "$HOME/.npm" ]; then
                if confirm_action "清理NPM缓存"; then
                    rm -rf "$HOME/.npm"/*
                    print_success "NPM缓存已清理"
                fi
            fi
            ;;
        3)
            if [ -d "$HOME/go/pkg" ]; then
                if confirm_action "清理Go模块缓存"; then
                    go clean -modcache
                    print_success "Go模块缓存已清理"
                fi
            fi
            ;;
        4)
            if [ -d "$HOME/.cargo/registry" ]; then
                if confirm_action "清理Cargo缓存"; then
                    rm -rf "$HOME/.cargo/registry"/*
                    print_success "Cargo缓存已清理"
                fi
            fi
            ;;
        5)
            if confirm_action "清理所有开发工具缓存"; then
                [ -d "$HOME/.cache/pip" ] && rm -rf "$HOME/.cache/pip"/*
                [ -d "$HOME/.npm" ] && rm -rf "$HOME/.npm"/*
                [ -d "$HOME/.cache/yarn" ] && rm -rf "$HOME/.cache/yarn"/*
                [ -d "$HOME/go/pkg" ] && go clean -modcache 2>/dev/null
                [ -d "$HOME/.cargo/registry" ] && rm -rf "$HOME/.cargo/registry"/*
                print_success "所有开发工具缓存已清理"
            fi
            ;;
        *)
            print_info "操作已取消"
            ;;
    esac
    log_cleanup "开发工具缓存" "用户选择"
}

# 一键智能清理
smart_cleanup() {
    print_header "智能清理"
    
    print_info "分析可安全清理的项目..."
    echo ""
    
    local items_to_clean=()
    local total_size=0
    
    # 检查各项可清理内容
    echo "可安全清理的项目:"
    echo ""
    
    # 缩略图缓存（安全）
    if [ -d "$HOME/.cache/thumbnails" ]; then
        local thumb_size=$(get_dir_size "$HOME/.cache/thumbnails")
        echo "  ✓ 缩略图缓存: $thumb_size"
        items_to_clean+=("thumbnails")
    fi
    
    # APT缓存（安全）
    if [ -d "/var/cache/apt/archives" ] && [ "$(ls -A /var/cache/apt/archives/*.deb 2>/dev/null)" ]; then
        local apt_size=$(sudo du -sh /var/cache/apt/archives 2>/dev/null | cut -f1)
        echo "  ✓ APT包缓存: $apt_size"
        items_to_clean+=("apt")
    fi
    
    # 回收站（需确认）
    if [ -d "$HOME/.local/share/Trash/files" ] && [ "$(ls -A $HOME/.local/share/Trash/files 2>/dev/null)" ]; then
        local trash_size=$(get_dir_size "$HOME/.local/share/Trash")
        echo "  ✓ 回收站: $trash_size"
        items_to_clean+=("trash")
    fi
    
    # Pip缓存
    if [ -d "$HOME/.cache/pip" ] && [ "$(ls -A $HOME/.cache/pip 2>/dev/null)" ]; then
        local pip_size=$(get_dir_size "$HOME/.cache/pip")
        echo "  ✓ Pip缓存: $pip_size"
        items_to_clean+=("pip")
    fi
    
    if [ ${#items_to_clean[@]} -eq 0 ]; then
        print_success "系统很干净，无需清理"
        return 0
    fi
    
    echo ""
    if ! confirm_action "执行智能清理 (只清理安全的项目)"; then
        return 0
    fi
    
    print_info "开始智能清理..."
    echo ""
    
    for item in "${items_to_clean[@]}"; do
        case $item in
            thumbnails)
                rm -rf "$HOME/.cache/thumbnails"/* 2>/dev/null
                print_success "缩略图缓存已清理"
                ;;
            apt)
                sudo apt-get clean
                sudo apt-get autoclean
                print_success "APT缓存已清理"
                ;;
            trash)
                rm -rf "$HOME/.local/share/Trash"/* 2>/dev/null
                print_success "回收站已清空"
                ;;
            pip)
                rm -rf "$HOME/.cache/pip"/* 2>/dev/null
                print_success "Pip缓存已清理"
                ;;
        esac
    done
    
    echo ""
    print_success "智能清理完成！"
}

# 清理菜单
cleanup_menu() {
    while true; do
        echo ""
        print_header "磁盘清理工具"
        echo ""
        echo -e "${CYAN}请选择要清理的项目:${NC}"
        echo ""
        print_menu_item "1" "分析可清理项目"
        print_menu_item "2" "清理用户缓存 (~/.cache)"
        print_menu_item "3" "清理缩略图缓存"
        print_menu_item "4" "清理APT包管理器缓存"
        print_menu_item "5" "清理系统日志"
        print_menu_item "6" "清理临时文件和回收站"
        print_menu_item "7" "清理下载的安装包"
        print_menu_item "8" "清理磐石系统快照"
        print_menu_item "9" "清理玲珑应用"
        print_menu_item "10" "清理浏览器缓存"
        print_menu_item "11" "清理开发工具缓存"
        print_menu_item "12" "智能清理 (一键优化)"
        print_menu_item "0" "返回主菜单"
        echo ""
        echo -ne "${CYAN}请输入选项 [0-12]: ${NC}"
        read -r choice
        
        case $choice in
            1) analyze_cleanable_items ;;
            2) clean_user_cache ;;
            3) clean_thumbnail_cache ;;
            4) clean_apt_cache ;;
            5) clean_journal_logs ;;
            6) clean_temp_files ;;
            7) clean_downloaded_packages ;;
            8) clean_snapshots ;;
            9) clean_linglong_apps ;;
            10) clean_browser_cache ;;
            11) clean_dev_cache ;;
            12) smart_cleanup ;;
            0) return 0 ;;
            *) print_error "无效选项" ;;
        esac
    done
}

# 生成总结报告
generate_summary() {
    print_header "磁盘空间分析总结"
    
    print_info "分析完成！"
    echo ""
    echo -e "${GREEN}主要发现:${NC}"
    echo "  1. 使用 'df -h' 查看整体磁盘使用情况"
    echo "  2. 检查 /persistent 分区占用"
    echo "  3. 检查 $HOME 目录下应用数据"
    echo "  4. 磐石系统快照可能占用大量空间"
    echo "  5. 玲珑应用和运行时环境可能占用较多空间"
    echo ""
    echo -e "${YELLOW}建议:${NC}"
    echo "  • 定期清理不需要的快照和玲珑应用"
    echo "  • 使用清理功能释放磁盘空间"
    echo "  • 检查大文件和重复文件"
    echo ""
    
    if [ -f "$CLEANUP_LOG" ]; then
        echo -e "${CYAN}本次清理日志:${NC}"
        echo "  日志文件: $CLEANUP_LOG"
        echo ""
        echo "清理记录:"
        cat "$CLEANUP_LOG"
    fi
}

# 显示帮助信息
show_help() {
    echo "deepin25 磁盘空间分析工具"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -a, --analyze     仅执行磁盘分析"
    echo "  -c, --clean       直接进入清理菜单"
    echo "  -s, --smart       执行智能清理"
    echo "  -h, --help        显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  $0                # 交互式菜单"
    echo "  $0 -a             # 仅分析不清理"
    echo "  $0 -c             # 直接进入清理"
    echo "  $0 -s             # 智能清理"
}

# 主菜单
main_menu() {
    while true; do
        echo ""
        print_header "deepin25 磁盘空间分析工具"
        echo ""
        echo -e "${CYAN}请选择操作:${NC}"
        echo ""
        print_menu_item "1" "磁盘空间分析"
        print_menu_item "2" "磁盘清理工具"
        print_menu_item "3" "分析并清理 (完整流程)"
        print_menu_item "0" "退出"
        echo ""
        echo -ne "${CYAN}请输入选项 [0-3]: ${NC}"
        read -r choice
        
        case $choice in
            1)
                analyze_disk_usage
                analyze_immutable_system
                analyze_linglong_apps
                generate_summary
                ;;
            2)
                cleanup_menu
                ;;
            3)
                analyze_disk_usage
                analyze_immutable_system
                analyze_linglong_apps
                echo ""
                print_info "分析完成，是否进入清理菜单?"
                echo -ne "${CYAN}[Y/n]: ${NC}"
                read -r response
                if [[ ! "$response" =~ ^[nN] ]]; then
                    cleanup_menu
                fi
                generate_summary
                ;;
            0)
                print_success "感谢使用!"
                echo ""
                if [ -f "$CLEANUP_LOG" ]; then
                    echo "清理日志已保存到: $CLEANUP_LOG"
                fi
                exit 0
                ;;
            *)
                print_error "无效选项"
                ;;
        esac
    done
}

# 主函数
main() {
    # 解析命令行参数
    case "${1:-}" in
        -a|--analyze)
            check_sudo
            analyze_disk_usage
            analyze_immutable_system
            analyze_linglong_apps
            generate_summary
            ;;
        -c|--clean)
            check_sudo
            cleanup_menu
            ;;
        -s|--smart)
            check_sudo
            smart_cleanup
            ;;
        -h|--help)
            show_help
            ;;
        *)
            check_sudo
            main_menu
            ;;
    esac
}

# 脚本入口
main "$@"
