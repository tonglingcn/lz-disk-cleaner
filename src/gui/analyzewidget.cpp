/*
 * Analyze Widget - Implementation
 * 磁盘分析界面 - 实现文件
 * 
 * 针对 Deepin V25 系统设计的磁盘分析组件
 */

#include "analyzewidget.h"
#include "../core/diskanalyzer.h"
#include "../utils/logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QProcess>
#include <QDebug>
#include <QApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QBrush>
#include <QColor>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>

// ============================================================================
// ScanThread Implementation
// ============================================================================

ScanThread::ScanThread(QObject *parent)
    : QThread(parent)
    , m_stopRequested(false)
{
}

void ScanThread::stop()
{
    QMutexLocker locker(&m_mutex);
    m_stopRequested = true;
}

void ScanThread::setScanCategories(const QList<ScanCategory> &categories)
{
    m_categories = categories;
}

void ScanThread::run()
{
    m_stopRequested = false;
    qint64 totalSize = 0;
    int totalFiles = 0;

    // 默认扫描所有类别
    if (m_categories.isEmpty()) {
        m_categories = {
            ScanCategory::USER_CACHE,
            ScanCategory::THUMBNAIL_CACHE,
            ScanCategory::APT_CACHE,
            ScanCategory::SYSTEM_LOGS,
            ScanCategory::JOURNAL_LOGS,
            ScanCategory::CRASH_REPORTS,
            ScanCategory::TEMP_FILES,
            ScanCategory::TRASH,
            ScanCategory::BROWSER_CACHE,
            ScanCategory::DEV_CACHE,
            ScanCategory::LINGLONG_APPS,
            ScanCategory::IMMUTABLE_SNAPSHOTS
        };
    }

    int totalCategories = m_categories.size();
    int currentCategory = 0;

    for (ScanCategory category : m_categories) {
        if (m_stopRequested) {
            break;
        }

        // 获取类别名称
        QString categoryName;
        switch (category) {
        case ScanCategory::USER_CACHE: categoryName = tr("用户缓存"); break;
        case ScanCategory::THUMBNAIL_CACHE: categoryName = tr("缩略图缓存"); break;
        case ScanCategory::APT_CACHE: categoryName = tr("APT包缓存"); break;
        case ScanCategory::SYSTEM_LOGS: categoryName = tr("系统日志"); break;
        case ScanCategory::JOURNAL_LOGS: categoryName = tr("Journald日志"); break;
        case ScanCategory::CRASH_REPORTS: categoryName = tr("崩溃报告"); break;
        case ScanCategory::TEMP_FILES: categoryName = tr("临时文件"); break;
        case ScanCategory::TRASH: categoryName = tr("回收站"); break;
        case ScanCategory::BROWSER_CACHE: categoryName = tr("浏览器缓存"); break;
        case ScanCategory::DEV_CACHE: categoryName = tr("开发工具缓存"); break;
        case ScanCategory::LINGLONG_APPS: categoryName = tr("玲珑应用"); break;
        case ScanCategory::IMMUTABLE_SNAPSHOTS: categoryName = tr("磐石系统快照"); break;
        default: categoryName = tr("未知"); break;
        }

        // 发送开始扫描该类别的进度（使用起始百分比）
        // 每个类别分配 (95% / totalCategories) 的进度范围
        int categoryProgressStart = (currentCategory * 95) / totalCategories;
        int categoryProgressEnd = ((currentCategory + 1) * 95) / totalCategories;
        
        // 发送扫描开始信号，进度为该类别的起始值
        emit scanProgress(categoryName, categoryProgressStart);

        // 扫描该类别（传入进度范围，用于内部更新）
        QList<ScanResult> results = scanCategory(category, categoryName, categoryProgressStart, categoryProgressEnd);
        emit categoryScanned(category, results);

        // 累计统计
        for (const ScanResult &result : results) {
            totalSize += result.size;
            totalFiles += result.fileCount;
        }

        currentCategory++;
    }

    emit scanFinished(totalSize, totalFiles);
}

// 计算目录大小
qint64 ScanThread::calculateDirSize(const QString &path)
{
    qint64 totalSize = 0;
    QDir dir(path);
    if (!dir.exists()) {
        return 0;
    }

    // 使用du命令快速获取大小
    QProcess process;
    process.start("du", QStringList() << "-sb" << path);
    if (process.waitForFinished(3000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QStringList parts = output.split('\t');
        if (!parts.isEmpty()) {
            totalSize = parts[0].toLongLong();
            return totalSize;
        }
    }

    // 如果du命令失败，使用递归方式
    QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo &fileInfo : fileList) {
        if (fileInfo.isDir()) {
            totalSize += calculateDirSize(fileInfo.absoluteFilePath());
        } else {
            totalSize += fileInfo.size();
        }
    }
    return totalSize;
}

// 获取目录用途说明
QString ScanThread::getDirectoryPurpose(const QString &path)
{
    QString dirName = QFileInfo(path).fileName().toLower();
    QString fullPath = path.toLower();
    
    // 应用数据目录
    if (dirName == "dingtalk" || fullPath.contains("dingtalk"))
        return tr("🔶 钉钉应用数据");
    if (dirName == "wechat" || dirName == "weixin" || fullPath.contains("wechat"))
        return tr("🟢 微信应用数据");
    if (dirName == "qq" || fullPath.contains("/qq/"))
        return tr("🐧 QQ应用数据");
    if (dirName == "tim" || fullPath.contains("tim"))
        return tr("💼 TIM应用数据");
    if (fullPath.contains("kingsoft") || fullPath.contains("wps"))
        return tr("📝 WPS Office缓存");
    
    // 浏览器缓存
    if (dirName.contains("chrome") || fullPath.contains("google-chrome"))
        return tr("🌐 Chrome浏览器缓存");
    if (dirName.contains("chromium"))
        return tr("🌐 Chromium浏览器缓存");
    if (dirName.contains("mozilla") || dirName.contains("firefox"))
        return tr("🦊 Firefox浏览器缓存");
    if (dirName.contains("yandex"))
        return tr("🌐 Yandex浏览器缓存");
    if (dirName.contains("falkon"))
        return tr("🌐 Falkon浏览器缓存");
    if (fullPath.contains("360.browser") || fullPath.contains("360browser"))
        return tr("🌐 360浏览器缓存");
    if (fullPath.contains("loongnix.lbrowser") || fullPath.contains("lbrowser"))
        return tr("🐉 龙芯浏览器缓存");
    if (fullPath.contains("qqbrowser"))
        return tr("🐧 QQ浏览器缓存");
    if (fullPath.contains("microsoft-edge") || fullPath.contains("edge"))
        return tr("🌊 Edge浏览器缓存");
    
    // 开发工具
    if (dirName == "pip" || fullPath.contains("pip"))
        return tr("🐍 Python包缓存");
    if (dirName == "npm" || dirName == ".npm")
        return tr("📦 NPM包缓存");
    if (dirName == "yarn" || fullPath.contains("yarn"))
        return tr("📦 Yarn包缓存");
    if (dirName.contains("go-build") || fullPath.contains("go/pkg"))
        return tr("🐹 Go模块缓存");
    if (dirName.contains("cargo") || fullPath.contains(".cargo"))
        return tr("🦀 Cargo包缓存");
    if (dirName.contains("maven") || dirName.contains(".m2"))
        return tr("☕ Maven仓库");
    if (dirName.contains("gradle"))
        return tr("📱 Gradle缓存");
    if (dirName == "ccache")
        return tr("⚡ CCache编译缓存");
    
    // 系统目录
    if (dirName == "thumbnails" || dirName.contains("thumb"))
        return tr("🖼️ 图片缩略图缓存");
    if (dirName == "fontconfig" || dirName.contains("font"))
        return tr("🔤 字体配置缓存");
    if (dirName == "dconf")
        return tr("⚙️ 系统配置缓存");
    if (dirName.contains("mesa") || dirName.contains("shader"))
        return tr("🎮 显卡着色器缓存");
    if (dirName.contains("gstreamer"))
        return tr("🎬 GStreamer媒体缓存");
    
    // 通用目录
    if (dirName == "cache" || dirName == ".cache")
        return tr("📂 应用缓存数据");
    if (dirName == "config" || dirName == ".config")
        return tr("⚙️ 应用配置文件");
    if (dirName == "data")
        return tr("📊 应用数据目录");
    if (dirName == "logs" || dirName.contains("log"))
        return tr("📝 日志文件目录");
    if (dirName == "temp" || dirName == "tmp")
        return tr("📄 临时文件目录");
    if (dirName == "backups" || dirName.contains("backup"))
        return tr("💾 备份文件目录");
    if (dirName == "downloads" || dirName == "Downloads")
        return tr("📥 下载文件目录");
    if (dirName == "documents" || dirName == "Documents")
        return tr("📄 文档文件目录");
    if (dirName == "desktop" || dirName == "Desktop")
        return tr("🖥️ 桌面文件目录");
    
    // 玲珑应用
    if (dirName.contains("linglong") || fullPath.contains("/var/lib/linglong"))
        return tr("🔷 玲珑应用数据");
    
    return QString(); // 返回空表示没有特定用途
}

// 获取应用图标
QString ScanThread::getAppIcon(const QString &appName)
{
    QString lowerName = appName.toLower();
    
    if (lowerName.contains("chrome"))
        return "🌐";
    if (lowerName.contains("firefox") || lowerName.contains("mozilla"))
        return "🦊";
    if (lowerName.contains("dingtalk"))
        return "🔶";
    if (lowerName.contains("wechat") || lowerName.contains("weixin"))
        return "🟢";
    if (lowerName.contains("qq"))
        return "🐧";
    if (lowerName.contains("tim"))
        return "💼";
    if (lowerName.contains("code") || lowerName.contains("vscode"))
        return "📝";
    if (lowerName.contains("thumbnail"))
        return "🖼️";
    if (lowerName.contains("pip"))
        return "🐍";
    if (lowerName.contains("npm"))
        return "📦";
    if (lowerName.contains("go"))
        return "🐹";
    if (lowerName.contains("cargo"))
        return "🦀";
    
    return "📁";
}

QList<ScanResult> ScanThread::scanCategory(ScanCategory category, const QString &categoryName,
                                             int progressStart, int progressEnd)
{
    m_currentCategoryName = categoryName;
    m_progressStart = progressStart;
    m_progressEnd = progressEnd;
    
    QString homePath = QDir::homePath();

    switch (category) {
    case ScanCategory::USER_CACHE: {
        QList<ScanResult> results;
        QString cachePath = homePath + "/.cache";
        QDir cacheDir(cachePath);
        
        if (cacheDir.exists()) {
            // 扫描主要缓存子目录 - 使用层级扫描
            QStringList cacheSubDirs = {
                "thumbnails", "mozilla", "google-chrome", "chromium",
                "deepin", "dconf", "fontconfig", "mesa_shader_cache",
                "nvidia", "qtshadercache", "gstreamer-1.0", "vmware"
            };
            
            // 收集所有需要扫描的目录
            QStringList allDirsToScan;
            
            // 先收集主要子目录
            for (const QString &subDir : cacheSubDirs) {
                QString fullPath = cachePath + "/" + subDir;
                QDir subDirPath(fullPath);
                if (subDirPath.exists()) {
                    allDirsToScan.append(subDir);
                }
            }
            
            // 收集其他未分类的缓存目录
            QFileInfoList otherEntries = cacheDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo &entry : otherEntries) {
                if (!cacheSubDirs.contains(entry.fileName())) {
                    allDirsToScan.append(entry.fileName());
                }
            }
            
            // 扫描子目录 - 使用层级扫描，并更新进度
            int totalItems = allDirsToScan.size();
            int currentItem = 0;
            
            for (const QString &subDir : allDirsToScan) {
                if (m_stopRequested) return results;
                
                QString fullPath = cachePath + "/" + subDir;
                QDir subDirPath(fullPath);
                if (subDirPath.exists()) {
                    QList<ScanResult> subResults = scanDirectoryWithChildren(
                        fullPath, category, subDir, 0, 
                        progressStart, progressEnd, currentItem, totalItems);
                    if (!subResults.isEmpty()) {
                        results.append(subResults);
                    }
                }
                currentItem++;
            }
            
            // 按大小排序
            std::sort(results.begin(), results.end(), 
                      [](const ScanResult &a, const ScanResult &b) {
                          return a.size > b.size;
                      });
        }
        return results;
    }

    case ScanCategory::THUMBNAIL_CACHE:
        return scanDirectory(homePath + "/.cache/thumbnails", category,
                            tr("缩略图缓存"), false);

    case ScanCategory::APT_CACHE:
        return scanDirectory("/var/cache/apt/archives", category,
                            tr("APT包缓存"), false);

    case ScanCategory::SYSTEM_LOGS:
        return scanDirectory("/var/log", category,
                            tr("系统日志"), true);

    case ScanCategory::JOURNAL_LOGS: {
        QList<ScanResult> results;
        QProcess process;
        process.start("journalctl", QStringList() << "--disk-usage");
        process.waitForFinished(5000);
        QString output = process.readAllStandardOutput();
        
        // 解析日志大小 - 格式如 "1.2G" 或 "500M"
        QRegularExpression re(R"((\d+(?:\.\d+)?)([GMK]?))", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = re.match(output);
        if (match.hasMatch()) {
            double value = match.captured(1).toDouble();
            QString unit = match.captured(2).toUpper();
            qint64 size = 0;
            
            if (unit == "G") size = static_cast<qint64>(value * 1024 * 1024 * 1024);
            else if (unit == "M") size = static_cast<qint64>(value * 1024 * 1024);
            else if (unit == "K") size = static_cast<qint64>(value * 1024);
            else size = static_cast<qint64>(value);
            
            ScanResult result;
            result.category = category;
            result.name = tr("Journald日志");
            result.path = "/var/log/journal";
            result.size = size;
            result.fileCount = 1;
            result.isDirectory = true;
            result.isDeletable = true;
            result.isDangerous = false;
            result.description = tr("系统服务日志，可安全清理旧日志");
            results.append(result);
        }
        return results;
    }

    case ScanCategory::CRASH_REPORTS:
        return scanDirectory("/var/crash", category,
                            tr("崩溃报告"), false);

    case ScanCategory::TEMP_FILES:
        return scanDirectory("/tmp", category,
                            tr("临时文件"), true);

    case ScanCategory::TRASH: {
        // 回收站结构：files/ 存放文件，info/ 存放元信息
        QList<ScanResult> results;
        QString trashPath = homePath + "/.local/share/Trash";
        QString filesPath = trashPath + "/files";
        QString infoPath = trashPath + "/info";
        
        int totalFileCount = 0;
        qint64 totalSize = 0;
        
        // 扫描 files 目录
        QDir filesDir(filesPath);
        if (filesDir.exists()) {
            int fileCount = 0;
            totalSize += getDirectorySize(filesPath, &fileCount);
            totalFileCount += fileCount;
        }
        
        // 扫描 info 目录
        QDir infoDir(infoPath);
        if (infoDir.exists()) {
            int fileCount = 0;
            totalSize += getDirectorySize(infoPath, &fileCount);
            totalFileCount += fileCount;
        }
        
        if (totalSize > 0 || totalFileCount > 0) {
            ScanResult result;
            result.category = category;
            result.name = tr("回收站");
            result.path = trashPath;  // 保存根路径用于清理
            result.size = totalSize;
            result.fileCount = totalFileCount;
            result.isDirectory = true;
            result.isDeletable = true;
            result.isDangerous = false;
            result.description = QString("%1 %2").arg(totalFileCount).arg(tr("个文件"));
            results.append(result);
        }
        return results;
    }

    case ScanCategory::BROWSER_CACHE: {
        QList<ScanResult> results;
        struct BrowserPath {
            QString path;
            QString displayName;
        };
        QList<BrowserPath> browserPaths = {
            {homePath + "/.cache/google-chrome", tr("Chrome缓存")},
            {homePath + "/.cache/chromium", tr("Chromium缓存")},
            {homePath + "/.cache/mozilla", tr("Firefox缓存")},
            {homePath + "/.cache/falkon", tr("Falkon缓存")},
            {homePath + "/.config/google-chrome/Default/Cache", tr("Chrome配置")},
            {homePath + "/.config/chromium/Default/Cache", tr("Chromium配置")},
            {homePath + "/.cache/yandex-browser", tr("Yandex浏览器")},
            // 360浏览器
            {homePath + "/.cache/com.360.browser", tr("360浏览器缓存")},
            {homePath + "/.config/com.360.browser/Default/Cache", tr("360浏览器配置")},
            // 龙芯浏览器
            {homePath + "/.cache/cn.loongnix.lbrowser", tr("龙芯浏览器缓存")},
            {homePath + "/.config/cn.loongnix.lbrowser/Default/Cache", tr("龙芯浏览器配置")},
            // QQ浏览器
            {homePath + "/.cache/qqbrowser", tr("QQ浏览器缓存")},
            {homePath + "/.config/qqbrowser/Default/Cache", tr("QQ浏览器配置")},
            // Edge浏览器
            {homePath + "/.cache/microsoft-edge", tr("Edge浏览器缓存")},
            {homePath + "/.config/microsoft-edge/Default/Cache", tr("Edge浏览器配置")}
        };
        
        int totalItems = browserPaths.size();
        int currentItem = 0;
        
        for (const BrowserPath &bp : browserPaths) {
            QDir dir(bp.path);
            if (dir.exists()) {
                QList<ScanResult> subResults = scanDirectoryWithChildren(
                    bp.path, category, bp.displayName, 0,
                    progressStart, progressEnd, currentItem, totalItems);
                if (!subResults.isEmpty()) {
                    results.append(subResults);
                }
            }
            currentItem++;
        }
        return results;
    }

    case ScanCategory::DEV_CACHE: {
        QList<ScanResult> results;
        struct DevPath {
            QString path;
            QString displayName;
        };
        QList<DevPath> devPaths = {
            {homePath + "/.cache/pip", tr("Python Pip")},
            {homePath + "/.cache/npm", tr("NPM")},
            {homePath + "/.cache/yarn", tr("Yarn")},
            {homePath + "/.cache/go-build", tr("Go编译")},
            {homePath + "/.cache/ccache", tr("CCache")},
            {homePath + "/.m2/repository", tr("Maven")},
            {homePath + "/.gradle/caches", tr("Gradle")},
            {homePath + "/.cargo/registry/cache", tr("Cargo")}
        };
        
        int totalItems = devPaths.size();
        int currentItem = 0;
        
        for (const DevPath &devPath : devPaths) {
            QDir dir(devPath.path);
            if (dir.exists()) {
                QList<ScanResult> subResults = scanDirectoryWithChildren(
                    devPath.path, category, devPath.displayName, 0,
                    progressStart, progressEnd, currentItem, totalItems);
                if (!subResults.isEmpty()) {
                    results.append(subResults);
                }
            }
            currentItem++;
        }
        return results;
    }

    case ScanCategory::LINGLONG_APPS: {
        QList<ScanResult> results;
        
        // 主要扫描 /var/lib/linglong/layers 目录
        QString layersPath = "/var/lib/linglong/layers";
        QDir layersDir(layersPath);
        
        if (layersDir.exists()) {
            // 获取所有子目录（应用层）
            QFileInfoList entries = layersDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            int totalItems = entries.size();
            int currentItem = 0;
            
            for (const QFileInfo &entry : entries) {
                if (m_stopRequested) return results;
                
                // 更新进度
                int percent = progressStart + (progressEnd - progressStart) * currentItem / totalItems;
                emit scanProgress(tr("%1 (%2/%3)").arg(categoryName).arg(currentItem + 1).arg(totalItems), percent);
                
                QString appId = entry.fileName();
                QString appPath = entry.absoluteFilePath();
                
                // 计算应用大小
                int fileCount = 0;
                qint64 appSize = getDirectorySize(appPath, &fileCount);
                
                // 只显示有实际大小的应用
                if (appSize > 0) {
                    // 尝试从 info.json 读取应用信息
                    QString appName = appId;  // 默认使用目录名
                    QString realAppId = appId;  // 真正的应用ID（用于卸载）
                    QString appVersion;
                    QString infoJsonPath = appPath + "/info.json";
                    
                    QFile infoFile(infoJsonPath);
                    if (infoFile.exists() && infoFile.open(QIODevice::ReadOnly)) {
                        QByteArray jsonData = infoFile.readAll();
                        infoFile.close();
                        
                        // 清理可能的BOM和空白字符
                        QString jsonStr = QString::fromUtf8(jsonData).trimmed();
                        
                        // 解析 id/appid 字段（用于 ll-cli uninstall）
                        // 有些文件使用 "id"，有些使用 "appid"（小写）
                        QRegularExpression idRe("\"id\"\\s*:\\s*\"([^\"]+)\"");
                        QRegularExpression appidRe("\"appid\"\\s*:\\s*\"([^\"]+)\"");
                        
                        QRegularExpressionMatch idMatch = idRe.match(jsonStr);
                        QRegularExpressionMatch appidMatch = appidRe.match(jsonStr);
                        
                        if (idMatch.hasMatch()) {
                            realAppId = idMatch.captured(1).trimmed();
                            LOG_INFO(QString("Found linglong appId (id): %1 for %2").arg(realAppId, appId));
                        } else if (appidMatch.hasMatch()) {
                            realAppId = appidMatch.captured(1).trimmed();
                            LOG_INFO(QString("Found linglong appId (appid): %1 for %2").arg(realAppId, appId));
                        } else {
                            LOG_WARNING(QString("Failed to parse appId from %1").arg(infoJsonPath));
                        }
                        
                        // 解析 name 字段（显示名称）
                        QRegularExpression nameRe("\"name\"\\s*:\\s*\"([^\"]+)\"");
                        QRegularExpressionMatch nameMatch = nameRe.match(jsonStr);
                        if (nameMatch.hasMatch()) {
                            appName = nameMatch.captured(1).trimmed();
                        }
                        
                        // 获取版本号
                        QRegularExpression verRe("\"version\"\\s*:\\s*\"([^\"]+)\"");
                        QRegularExpressionMatch verMatch = verRe.match(jsonStr);
                        if (verMatch.hasMatch()) {
                            appVersion = verMatch.captured(1).trimmed();
                        }
                    } else {
                        LOG_WARNING(QString("Failed to read info.json: %1").arg(infoJsonPath));
                    }
                    
                    // 使用应用ID作为显示名称（方便用户识别）
                    // 如果ID太长，截断显示
                    QString displayName = realAppId;
                    if (displayName.length() > 40) {
                        displayName = displayName.left(37) + "...";
                    }
                    
                    ScanResult result;
                    result.category = category;
                    result.name = displayName;  // 显示应用ID
                    result.appId = realAppId;   // 存储真正的应用ID用于卸载
                    result.path = appPath;
                    result.size = appSize;
                    result.fileCount = fileCount;
                    result.isDirectory = true;
                    result.isDeletable = true;
                    result.isDangerous = false;
                    
                    // 构建描述信息 - 显示应用名称和版本
                    if (!appVersion.isEmpty() && !appName.isEmpty()) {
                        result.description = tr("%1 v%2 | %3个文件").arg(appName, appVersion).arg(fileCount);
                    } else if (!appName.isEmpty()) {
                        result.description = tr("%1 | %2个文件").arg(appName).arg(fileCount);
                    } else {
                        result.description = tr("%1个文件").arg(fileCount);
                    }
                    result.purpose = tr("🔷 玲珑应用");
                    
                    // 对大于100MB的应用，扫描其子目录
                    if (appSize > 100 * 1024 * 1024) {
                        QDir appDir(appPath);
                        QFileInfoList subEntries = appDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
                        
                        // 按大小排序，只取前5个
                        QList<QPair<qint64, QFileInfo>> subDirs;
                        for (const QFileInfo &subEntry : subEntries) {
                            // 跳过 info.json 等文件，只扫描目录
                            if (!subEntry.isDir()) continue;
                            
                            int subCount = 0;
                            qint64 subSize = getDirectorySize(subEntry.absoluteFilePath(), &subCount);
                            if (subSize > 10 * 1024 * 1024) { // 只显示大于10MB的子目录
                                subDirs.append(qMakePair(subSize, subEntry));
                            }
                        }
                        
                        std::sort(subDirs.begin(), subDirs.end(), 
                                  [](const QPair<qint64, QFileInfo> &a, const QPair<qint64, QFileInfo> &b) {
                                      return a.first > b.first;
                                  });
                        
                        int maxSubs = qMin(5, subDirs.size());
                        for (int i = 0; i < maxSubs; ++i) {
                            QString subDirName = subDirs[i].second.fileName();
                            QString subDirPath = subDirs[i].second.absoluteFilePath();
                            
                            // 尝试读取子目录的 info.json 获取版本信息
                            QString subInfoPath = subDirPath + "/info.json";
                            QString subVersion;
                            QFile subInfoFile(subInfoPath);
                            if (subInfoFile.exists() && subInfoFile.open(QIODevice::ReadOnly)) {
                                QByteArray subJsonData = subInfoFile.readAll();
                                subInfoFile.close();
                                QRegularExpression verRe("\"version\"\\s*:\\s*\"([^\"]+)\"");
                                QRegularExpressionMatch verMatch = verRe.match(subJsonData);
                                if (verMatch.hasMatch()) {
                                    subVersion = verMatch.captured(1);
                                }
                            }
                            
                            ScanResult subResult;
                            subResult.category = category;
                            subResult.name = subVersion.isEmpty() ? subDirName : subVersion;
                            subResult.path = subDirPath;
                            subResult.size = subDirs[i].first;
                            subResult.fileCount = 0;
                            subResult.isDirectory = true;
                            subResult.isDeletable = false; // 子目录不单独删除
                            subResult.isDangerous = false;
                            subResult.description = tr("版本层: %1").arg(subDirName.left(16));
                            subResult.purpose = tr("📦 版本");
                            result.children.append(subResult);
                        }
                    }
                    
                    results.append(result);
                }
                
                currentItem++;
            }
        }
        
        // 按大小排序
        std::sort(results.begin(), results.end(), 
                  [](const ScanResult &a, const ScanResult &b) {
                      return a.size > b.size;
                  });
        
        return results;
    }

    case ScanCategory::IMMUTABLE_SNAPSHOTS: {
        QList<ScanResult> results;
        QProcess process;
        
        // 检查 deepin-immutable-ctl 是否存在
        process.start("which", QStringList() << "deepin-immutable-ctl");
        if (!process.waitForFinished(3000) || process.exitCode() != 0) {
            LOG_DEBUG("deepin-immutable-ctl not found, skipping immutable system scan");
            return results;
        }
        
        LOG_INFO("Detected immutable system tools, scanning...");
        
        // 1. 扫描 ostree 仓库
        QString ostreeRepoPath = "/ostree/repo";
        QDir ostreeRepoDir(ostreeRepoPath);
        if (ostreeRepoDir.exists()) {
            ScanResult ostreeResult;
            ostreeResult.category = category;
            ostreeResult.name = tr("OSTree 系统仓库");
            ostreeResult.path = ostreeRepoPath;
            ostreeResult.size = getDirectorySize(ostreeRepoPath);
            ostreeResult.fileCount = 1;
            ostreeResult.isDirectory = true;
            ostreeResult.isDeletable = false; // 不能直接删除
            ostreeResult.isDangerous = true;
            ostreeResult.description = tr("磐石系统核心仓库，包含系统镜像");
            results.append(ostreeResult);
            LOG_INFO(QString("OSTree repo size: %1").arg(ostreeResult.size));
        }
        
        // 2. 扫描部署目录
        QString deployPath = "/ostree/deploy";
        QDir deployDir(deployPath);
        if (deployDir.exists()) {
            ScanResult deployResult;
            deployResult.category = category;
            deployResult.name = tr("系统部署数据 (总览)");
            deployResult.path = deployPath;
            deployResult.size = getDirectorySize(deployPath);
            deployResult.fileCount = 1;
            deployResult.isDirectory = true;
            deployResult.isDeletable = false;
            deployResult.isDangerous = true;   // 总览项不可删除，标记为危险
            deployResult.description = tr("当前及历史系统部署版本数据（不可直接删除）");
            results.append(deployResult);
            LOG_INFO(QString("Deploy size: %1").arg(deployResult.size));
            
            // ========== 解析各部署项（支持 undeploy 删除旧版本）==========
            process.start("deepin-immutable-ctl", QStringList() << "admin" << "status" << "-d" << "all");
            if (process.waitForFinished(5000)) {
                QString rawOutput = QString::fromUtf8(process.readAllStandardOutput());
                LOG_INFO(QString("Deploy raw output (first 500): %1").arg(rawOutput.left(500)));
                
                QRegularExpression jsonDeployRe(R"(\{[^{}]*"deployments"\s*:\s*\[([^\]]*)\])");
                QRegularExpressionMatch jsonMatch = jsonDeployRe.match(rawOutput);
                
                int totalDeployCount = 0;
                QList<ScanResult> deployItems;
                
                if (jsonMatch.hasMatch()) {
                    QString deploymentsStr = jsonMatch.captured(1);
                    QRegularExpression itemRe(R"(\{([^}]*)\})");
                    auto iter = itemRe.globalMatch(deploymentsStr);
                    
                    while (iter.hasNext()) {
                        QRegularExpressionMatch itemMatch = iter.next();
                        QString itemStr = itemMatch.captured(1);
                        
                        int idx = totalDeployCount++;
                        
                        // 检测 booted 状态
                        bool booted = itemStr.contains(R"XXX("booted":true)XXX")
                                       || itemStr.contains("\"booted\"\\s*:\\s*true");

                        QString depId, depVer, depTs;
                        QRegularExpression idRe(R"XXX("id"\s*:\s*"([^"]+)")XXX");
                        QRegularExpression verRe(R"XXX("version"\s*:\s*"([^"]+)")XXX");
                        QRegularExpression tsRe(R"XXX("timestamp"\s*:\s*(\d+))XXX");
                        QRegularExpressionMatch m;
                        m = idRe.match(itemStr); if (m.hasMatch()) depId = m.captured(1).trimmed();
                        m = verRe.match(itemStr); if (m.hasMatch()) depVer = m.captured(1).trimmed();
                        m = tsRe.match(itemStr); if (m.hasMatch()) depTs = m.captured(1).trimmed();
                        
                        ScanResult depScanResult;
                        depScanResult.category = category;
                        depScanResult.deployIndex = idx;
                        depScanResult.path = QString("/ostree/deploy/deepin-%1").arg(idx);
                        depScanResult.isDirectory = true;
                        depScanResult.isDeletable = !booted;
                        depScanResult.isDangerous = !booted;
                        depScanResult.fileCount = 1;
                        
                        if (booted) {
                            depScanResult.name = tr("⭐ 部署 #%1 [当前使用]").arg(idx)
                                + (depVer.isEmpty() ? "" : (" - " + depVer));
                            depScanResult.description = tr("正在运行的系统部署，不可删除");
                            depScanResult.purpose = tr("🟢 当前运行");
                            depScanResult.size = 0;
                        } else {
                            depScanResult.name = tr("📦 历史部署 #%1").arg(idx)
                                + (depVer.isEmpty() ? "" : (" - " + depVer));
                            depScanResult.description = tr("旧版本部署，可通过 undeploy 释放空间")
                                + (depTs.isEmpty() ? "" : ("\n" + depTs));
                            depScanResult.purpose = tr("🔶 可删除");
                            
                            // 尝试获取单个部署大小
                            QString singleDeployPath = QString("/ostree/deploy/deepin/deploy/%1").arg(idx);
                            QDir sd(singleDeployPath);
                            if (sd.exists()) {
                                depScanResult.size = getDirectorySize(singleDeployPath);
                            } else {
                                qint64 totalDeploySize = getDirectorySize(deployPath);
                                depScanResult.size = totalDeploySize / qMax(totalDeployCount, 1);
                            }
                        }
                        
                        results.append(depScanResult);
                        LOG_INFO(QString("  Deploy #%1: booted=%2, size=%3")
                                 .arg(idx).arg(booted).arg(depScanResult.size));
                    }
                } else {
                    // 文本模式：检测 deploy-N 模式
                    QRegularExpression textDeployRe(R"(deploy-(\d+)[^0-9])");
                    auto textIter = textDeployRe.globalMatch(rawOutput);
                    int maxIdx = -1;
                    while (textIter.hasNext()) {
                        QRegularExpressionMatch tm = textIter.next();
                        int ti = tm.captured(1).toInt();
                        if (ti > maxIdx) maxIdx = ti;
                    }
                    
                    if (maxIdx >= 0) {
                        for (int i = 0; i <= maxIdx; ++i) {
                            ScanResult depScanResult;
                            depScanResult.category = category;
                            depScanResult.deployIndex = i;
                            depScanResult.path = QString("/ostree/deploy/deepin-%1").arg(i);
                            depScanResult.isDirectory = true;
                            depScanResult.fileCount = 1;
                            
                            if (i == maxIdx) {
                                depScanResult.isDeletable = false;
                                depScanResult.isDangerous = false;
                                depScanResult.name = tr("⭐ 部署 #%1 [当前使用]").arg(i);
                                depScanResult.description = tr("正在运行的系统部署，不可删除");
                                depScanResult.purpose = tr("🟢 当前运行");
                                depScanResult.size = 0;
                            } else {
                                depScanResult.isDeletable = true;
                                depScanResult.isDangerous = true;
                                depScanResult.name = tr("📦 历史部署 #%1").arg(i);
                                depScanResult.description = tr("旧版本部署，可通过 undeploy 释放空间");
                                depScanResult.purpose = tr("🔶 可删除");
                                
                                QString singleDeployPath = QString("/ostree/deploy/deepin/deploy/%1").arg(i);
                                QDir sd(singleDeployPath);
                                if (sd.exists()) {
                                    depScanResult.size = getDirectorySize(singleDeployPath);
                                }
                            }
                            results.append(depScanResult);
                        }
                    }
                }
            }
        }
        
        // 3. 扫描 debtree 目录
        QString debtreePath = "/ostree/debtree";
        QDir debtreeDir(debtreePath);
        if (debtreeDir.exists()) {
            ScanResult debtreeResult;
            debtreeResult.category = category;
            debtreeResult.name = tr("DEB 软件包树");
            debtreeResult.path = debtreePath;
            debtreeResult.size = getDirectorySize(debtreePath);
            debtreeResult.fileCount = 1;
            debtreeResult.isDirectory = true;
            debtreeResult.isDeletable = false;
            debtreeResult.isDangerous = true;
            debtreeResult.description = tr("通过 apt 安装的软件包数据");
            results.append(debtreeResult);
            LOG_INFO(QString("Debtree size: %1").arg(debtreeResult.size));
        }
        
        // 4. 扫描 persistent overlay 目录
        QString overlayPath = "/root/persistent/overlay";
        QDir overlayDir(overlayPath);
        if (overlayDir.exists()) {
            ScanResult overlayResult;
            overlayResult.category = category;
            overlayResult.name = tr("系统修改层 (Overlay)");
            overlayResult.path = overlayPath;
            overlayResult.size = getDirectorySize(overlayPath);
            overlayResult.fileCount = 1;
            overlayResult.isDirectory = true;
            overlayResult.isDeletable = false;
            overlayResult.isDangerous = true;
            overlayResult.description = tr("对系统目录的修改数据 (/usr, /opt, /etc)");
            results.append(overlayResult);
            LOG_INFO(QString("Overlay size: %1").arg(overlayResult.size));
        }
        
        // 5. 扫描 persistent ostree 目录
        QString persistentOstreePath = "/root/persistent/ostree";
        QDir persistentOstreeDir(persistentOstreePath);
        if (persistentOstreeDir.exists()) {
            ScanResult persistentResult;
            persistentResult.category = category;
            persistentResult.name = tr("持久化 OSTree 数据");
            persistentResult.path = persistentOstreePath;
            persistentResult.size = getDirectorySize(persistentOstreePath);
            persistentResult.fileCount = 1;
            persistentResult.isDirectory = true;
            persistentResult.isDeletable = false;
            persistentResult.isDangerous = true;
            persistentResult.description = tr("持久化的系统数据");
            results.append(persistentResult);
            LOG_INFO(QString("Persistent ostree size: %1").arg(persistentResult.size));
        }
        
        // 6. 扫描快照目录 (如果存在)
        QString snapshotPath = "/boot/deepin-snapshots";
        QDir snapshotDir(snapshotPath);
        if (snapshotDir.exists()) {
            QStringList subdirs = snapshotDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &subdir : subdirs) {
                QString fullPath = snapshotPath + "/" + subdir;
                ScanResult snapshotResult;
                snapshotResult.category = category;
                snapshotResult.name = tr("系统快照: %1").arg(subdir);
                snapshotResult.path = fullPath;
                snapshotResult.size = getDirectorySize(fullPath);
                snapshotResult.fileCount = 1;
                snapshotResult.isDirectory = true;
                snapshotResult.isDeletable = true;
                snapshotResult.isDangerous = true;
                snapshotResult.description = tr("磐石系统快照，可安全删除");
                results.append(snapshotResult);
            }
        }
        
        // 7. 获取快照列表 (通过命令)
        process.start("deepin-immutable-ctl", QStringList() << "snapshot" << "list");
        if (process.waitForFinished(5000)) {
            QString snapshotOutput = QString::fromUtf8(process.readAllStandardOutput());
            LOG_INFO(QString("Snapshot list output: %1").arg(snapshotOutput));
        }
        
        LOG_INFO(QString("Immutable system scan complete, found %1 items").arg(results.size()));
        return results;
    }

    default:
        return QList<ScanResult>();
    }
}

QList<ScanResult> ScanThread::scanDirectory(const QString &path, ScanCategory category,
                                            const QString &displayName, bool isDangerous)
{
    QList<ScanResult> results;

    QDir dir(path);
    if (!dir.exists()) {
        return results;
    }

    int fileCount = 0;
    qint64 totalSize = getDirectorySize(path, &fileCount);

    if (totalSize > 0 || fileCount > 0) {
        ScanResult result;
        result.category = category;
        result.name = displayName;
        result.path = path;
        result.size = totalSize;
        result.fileCount = fileCount;
        result.isDirectory = true;
        result.isDeletable = true;
        result.isDangerous = isDangerous;
        result.description = QString("%1 %2").arg(fileCount).arg(tr("个文件"));
        result.purpose = getDirectoryPurpose(path);

        results.append(result);
    }

    return results;
}

// 层级扫描目录 - 对大于阈值的目录深入扫描子目录
QList<ScanResult> ScanThread::scanDirectoryWithChildren(const QString &path, ScanCategory category,
                                                         const QString &displayName, int depth,
                                                         int progressStart, int progressEnd,
                                                         int currentItem, int totalItems)
{
    QList<ScanResult> results;
    
    // 限制递归深度，避免过深扫描
    if (depth > 2) {
        return results;
    }

    QDir dir(path);
    if (!dir.exists()) {
        return results;
    }

    // 更新进度（在扫描前）
    if (totalItems > 0) {
        int percent = progressStart + (progressEnd - progressStart) * currentItem / totalItems;
        emit scanProgress(tr("%1 (%2/%3)").arg(m_currentCategoryName).arg(currentItem + 1).arg(totalItems), percent);
    }

    int fileCount = 0;
    qint64 totalSize = getDirectorySize(path, &fileCount);

    if (totalSize <= 0) {
        return results;
    }

    // 创建主结果项
    ScanResult result;
    result.category = category;
    result.name = displayName;
    result.path = path;
    result.size = totalSize;
    result.fileCount = fileCount;
    result.isDirectory = true;
    result.isDeletable = true;
    result.isDangerous = false;
    result.purpose = getDirectoryPurpose(path);
    
    // 如果目录大于100MB且深度小于限制，扫描子目录
    const qint64 DEEP_SCAN_THRESHOLD = 100 * 1024 * 1024; // 100MB
    if (totalSize > DEEP_SCAN_THRESHOLD && depth < 2) {
        QFileInfoList subEntries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
        
        // 按大小排序，只取前5个最大的子目录
        QList<QPair<qint64, QFileInfo>> subDirs;
        for (const QFileInfo &entry : subEntries) {
            int subCount = 0;
            qint64 subSize = getDirectorySize(entry.absoluteFilePath(), &subCount);
            if (subSize > 10 * 1024 * 1024) { // 只显示大于10MB的子目录
                subDirs.append(qMakePair(subSize, entry));
            }
        }
        
        // 按大小降序排序
        std::sort(subDirs.begin(), subDirs.end(), 
                  [](const QPair<qint64, QFileInfo> &a, const QPair<qint64, QFileInfo> &b) {
                      return a.first > b.first;
                  });
        
        // 最多显示5个子目录
        int maxSubs = qMin(5, subDirs.size());
        for (int i = 0; i < maxSubs; ++i) {
            const QFileInfo &subInfo = subDirs[i].second;
            qint64 subSize = subDirs[i].first;
            
            // 递归扫描子目录
            QList<ScanResult> subResults = scanDirectoryWithChildren(
                subInfo.absoluteFilePath(), category, subInfo.fileName(), depth + 1,
                progressStart, progressEnd, currentItem, totalItems);
            
            if (!subResults.isEmpty()) {
                result.children.append(subResults.first());
            }
        }
        
        // 如果有更多子目录，添加一个"更多..."项
        if (subDirs.size() > maxSubs) {
            ScanResult moreResult;
            moreResult.category = category;
            moreResult.name = tr("... 还有 %1 个子目录").arg(subDirs.size() - maxSubs);
            moreResult.path = path;
            moreResult.size = 0;
            moreResult.fileCount = 0;
            moreResult.isDirectory = true;
            moreResult.isDeletable = false;
            moreResult.isDangerous = false;
            moreResult.purpose = tr("点击展开查看全部");
            result.children.append(moreResult);
        }
    }
    
    results.append(result);
    return results;
}

qint64 ScanThread::getDirectorySize(const QString &path, int *fileCount)
{
    qint64 totalSize = 0;
    int count = 0;

    QDir dir(path);
    if (!dir.exists()) {
        return 0;
    }

    // 使用du命令快速计算目录大小
    QProcess process;
    process.start("du", QStringList() << "-sb" << path);
    if (process.waitForFinished(10000)) {
        QString output = process.readAllStandardOutput();
        QStringList parts = output.split('\t');
        if (!parts.isEmpty()) {
            totalSize = parts[0].toLongLong();
        }
    }

    // 计算文件数量
    QProcess findProcess;
    findProcess.start("find", QStringList() << path << "-type" << "f");
    if (findProcess.waitForFinished(10000)) {
        QString output = findProcess.readAllStandardOutput();
        count = output.count('\n');
    }

    if (fileCount) {
        *fileCount = count;
    }

    return totalSize;
}

QString ScanThread::formatSize(qint64 bytes)
{
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;

    if (bytes >= GB) {
        return QString("%1 GB").arg(bytes / (double)GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(bytes / (double)MB, 0, 'f', 2);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(bytes / (double)KB, 0, 'f', 2);
    } else {
        return QString("%1 B").arg(bytes);
    }
}

// ============================================================================
// AnalyzeWidget Implementation
// ============================================================================

AnalyzeWidget::AnalyzeWidget(QWidget *parent)
    : QWidget(parent)
    , m_scanThread(nullptr)
    , m_totalSize(0)
    , m_totalFiles(0)
    , m_isScanning(false)
{
    LOG_INFO("AnalyzeWidget initializing");
    initUI();
    applyTheme();
}

AnalyzeWidget::~AnalyzeWidget()
{
    if (m_scanThread && m_scanThread->isRunning()) {
        m_scanThread->stop();
        m_scanThread->wait();
    }
    LOG_INFO("AnalyzeWidget destroyed");
}

void AnalyzeWidget::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 创建堆叠布局
    m_stackedLayout = new QStackedLayout();

    // 创建三个页面并添加到堆叠布局
    createInfoPage();
    m_stackedLayout->addWidget(m_infoPage);
    
    createProgressPage();
    m_stackedLayout->addWidget(m_progressPage);
    
    createResultPage();
    m_stackedLayout->addWidget(m_resultPage);

    mainLayout->addLayout(m_stackedLayout);

    // 默认显示信息页
    m_stackedLayout->setCurrentWidget(m_infoPage);
}

void AnalyzeWidget::createInfoPage()
{
    m_infoPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_infoPage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    // 检测深色主题
    bool darkMode = isDarkTheme();
    QString titleColor = darkMode ? "#e0e0e0" : "#2c3e50";
    QString descColor = darkMode ? "#a0a0a0" : "#7f8c8d";

    // 标题
    QLabel *titleLabel = new QLabel(tr("磁盘分析"), this);
    titleLabel->setStyleSheet(QString("font-size: 24px; font-weight: bold; color: %1;").arg(titleColor));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // 说明文字
    QLabel *descLabel = new QLabel(tr("扫描系统中的可清理项目，包括缓存、日志、临时文件等"), this);
    descLabel->setStyleSheet(QString("font-size: 14px; color: %1;").arg(descColor));
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    // 扫描类型网格
    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setSpacing(15);
    gridLayout->setContentsMargins(10, 10, 10, 10);

    struct CategoryInfo {
        QString name;
        QString icon;
        QString desc;
    };

    QList<CategoryInfo> categories = {
        {tr("用户缓存"), "🗂️", tr("应用程序缓存数据")},
        {tr("系统日志"), "📋", tr("系统和应用日志")},
        {tr("临时文件"), "📄", tr("系统临时文件")},
        {tr("回收站"), "🗑️", tr("已删除的文件")},
        {tr("浏览器缓存"), "🌐", tr("网页浏览缓存")},
        {tr("开发缓存"), "💻", tr("开发工具缓存")},
        {tr("玲珑应用"), "📦", tr("玲珑应用数据")},
        {tr("磐石快照"), "📸", tr("系统快照备份")}
    };

    // 根据主题设置颜色
    QString cardBgColor = darkMode ? "#3d3d3d" : "#ecf0f1";
    QString nameColor = darkMode ? "#ffffff" : "#2c3e50";
    
    int row = 0, col = 0;
    for (const CategoryInfo &cat : categories) {
        QFrame *frame = new QFrame(this);
        frame->setStyleSheet(
            "QFrame {"
            "   background-color: " + cardBgColor + ";"
            "   border-radius: 8px;"
            "   padding: 10px;"
            "}"
        );
        frame->setMinimumSize(160, 120);
        frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        QVBoxLayout *frameLayout = new QVBoxLayout(frame);
        frameLayout->setSpacing(4);  // 缩小间距
        frameLayout->setContentsMargins(10, 10, 10, 10);

        QLabel *iconLabel = new QLabel(cat.icon, frame);
        iconLabel->setStyleSheet("font-size: 32px;");
        iconLabel->setAlignment(Qt::AlignCenter);
        frameLayout->addWidget(iconLabel);

        QLabel *nameLabel = new QLabel(cat.name, frame);
        nameLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: " + nameColor + ";");
        nameLabel->setAlignment(Qt::AlignCenter);
        frameLayout->addWidget(nameLabel);

        QLabel *descLabel = new QLabel(cat.desc, frame);
        descLabel->setStyleSheet("font-size: 11px; color: " + descColor + ";");
        descLabel->setAlignment(Qt::AlignCenter);
        descLabel->setWordWrap(true);
        frameLayout->addWidget(descLabel);
        frameLayout->addStretch();

        gridLayout->addWidget(frame, row, col);

        col++;
        if (col >= 4) {
            col = 0;
            row++;
        }
    }

    layout->addLayout(gridLayout);

    // 开始扫描按钮
    m_startScanButton = new QPushButton(tr("🔍 开始扫描"), this);
    m_startScanButton->setMinimumSize(200, 50);
    m_startScanButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #3498db;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 8px;"
        "   font-size: 16px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #2980b9;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #21618c;"
        "}"
    );
    connect(m_startScanButton, &QPushButton::clicked, this, &AnalyzeWidget::onStartScanClicked);

    layout->addSpacing(20);
    layout->addWidget(m_startScanButton, 0, Qt::AlignCenter);
}

void AnalyzeWidget::createProgressPage()
{
    m_progressPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_progressPage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    // 检测深色主题
    bool darkMode = isDarkTheme();
    QString textColor = darkMode ? "#e0e0e0" : "#2c3e50";
    QString borderColor = darkMode ? "#555555" : "#bdc3c7";
    QString bgColor = darkMode ? "#3d3d3d" : "#ecf0f1";

    // 进度标签
    m_progressLabel = new QLabel(tr("正在扫描..."), this);
    m_progressLabel->setStyleSheet(QString("font-size: 18px; color: %1;").arg(textColor));
    m_progressLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_progressLabel);

    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMinimumWidth(400);
    m_progressBar->setMinimumHeight(25);
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "   border: 2px solid " + borderColor + ";"
        "   border-radius: 5px;"
        "   text-align: center;"
        "   background-color: " + bgColor + ";"
        "   color: " + textColor + ";"
        "}"
        "QProgressBar::chunk {"
        "   background-color: #3498db;"
        "   border-radius: 3px;"
        "}"
    );
    layout->addWidget(m_progressBar, 0, Qt::AlignCenter);

    // 停止按钮
    m_stopScanButton = new QPushButton(tr("停止扫描"), this);
    m_stopScanButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #e74c3c;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 5px;"
        "   padding: 8px 20px;"
        "   outline: none;"
        "}"
        "QPushButton:hover {"
        "   background-color: #c0392b;"
        "}"
        "QPushButton:focus {"
        "   outline: none;"
        "   border: none;"
        "}"
    );
    connect(m_stopScanButton, &QPushButton::clicked, this, &AnalyzeWidget::onStopScanClicked);
    layout->addWidget(m_stopScanButton, 0, Qt::AlignCenter);
}

void AnalyzeWidget::createResultPage()
{
    m_resultPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_resultPage);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 检测深色主题
    bool darkMode = isDarkTheme();
    QString titleColor = darkMode ? "#e0e0e0" : "#2c3e50";

    // 标题栏
    QHBoxLayout *headerLayout = new QHBoxLayout();
    
    QLabel *titleLabel = new QLabel(tr("扫描结果"), this);
    titleLabel->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(titleColor));
    headerLayout->addWidget(titleLabel);

    m_totalSizeLabel = new QLabel(tr("总计: 0 B"), this);
    m_totalSizeLabel->setStyleSheet("font-size: 14px; color: #27ae60; font-weight: bold;");
    headerLayout->addWidget(m_totalSizeLabel);

    headerLayout->addStretch();
    layout->addLayout(headerLayout);

    // 结果树 - 使用更多列展示详细信息
    m_resultTree = new QTreeWidget(this);
    m_resultTree->setHeaderLabels({tr("项目"), tr("大小"), tr("用途/描述"), tr("路径"), tr("状态")});
    m_resultTree->setColumnWidth(0, 280);  // 项目名称
    m_resultTree->setColumnWidth(1, 90);   // 大小
    m_resultTree->setColumnWidth(2, 150);  // 用途/描述
    m_resultTree->setColumnWidth(3, 200);  // 路径
    m_resultTree->setColumnWidth(4, 70);   // 状态
    // 样式由 applyTheme() 统一设置，支持深色主题
    m_resultTree->setAnimated(true);
    m_resultTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_resultTree->setAlternatingRowColors(true);
    m_resultTree->setUniformRowHeights(false);
    m_resultTree->header()->setStretchLastSection(true);
    m_resultTree->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    connect(m_resultTree, &QTreeWidget::itemChanged, this, &AnalyzeWidget::onTreeItemChanged);
    
    // 添加右键菜单支持
    m_resultTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_resultTree, &QTreeWidget::customContextMenuRequested, this, &AnalyzeWidget::onTreeContextMenu);
    
    layout->addWidget(m_resultTree);

    // 底部按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_rescanButton = new QPushButton(tr("🔄 重新扫描"), this);
    m_rescanButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #95a5a6;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 5px;"
        "   padding: 10px 20px;"
        "   font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #7f8c8d;"
        "}"
    );
    connect(m_rescanButton, &QPushButton::clicked, this, &AnalyzeWidget::onStartScanClicked);
    buttonLayout->addWidget(m_rescanButton);

    m_cleanupButton = new QPushButton(tr("🧹 清理选中项"), this);
    m_cleanupButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #27ae60;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 5px;"
        "   padding: 10px 25px;"
        "   font-size: 14px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #229954;"
        "}"
        "QPushButton:disabled {"
        "   background-color: #bdc3c7;"
        "}"
    );
    m_cleanupButton->setEnabled(false);
    connect(m_cleanupButton, &QPushButton::clicked, this, &AnalyzeWidget::onCleanupClicked);
    buttonLayout->addWidget(m_cleanupButton);

    layout->addLayout(buttonLayout);
}

void AnalyzeWidget::startScan()
{
    onStartScanClicked();
}

void AnalyzeWidget::stopScan()
{
    onStopScanClicked();
}

bool AnalyzeWidget::isScanning() const
{
    return m_isScanning;
}

void AnalyzeWidget::onStartScanClicked()
{
    LOG_INFO("Scan started");

    m_isScanning = true;
    m_scanResults.clear();
    m_totalSize = 0;
    m_totalFiles = 0;
    m_resultTree->clear();

    // 切换到进度页
    m_stackedLayout->setCurrentWidget(m_progressPage);
    m_progressLabel->setText(tr("正在扫描..."));
    m_progressBar->setValue(0);

    // 创建并启动扫描线程
    if (m_scanThread) {
        m_scanThread->stop();
        m_scanThread->wait();
        delete m_scanThread;
    }

    m_scanThread = new ScanThread(this);
    connect(m_scanThread, &ScanThread::scanProgress, this, &AnalyzeWidget::onScanProgress);
    connect(m_scanThread, &ScanThread::categoryScanned, this, &AnalyzeWidget::onCategoryScanned);
    connect(m_scanThread, &ScanThread::scanFinished, this, &AnalyzeWidget::onScanFinished);

    emit scanStarted();
    m_scanThread->start();
}

void AnalyzeWidget::onStopScanClicked()
{
    LOG_INFO("Scan stopped by user");

    if (m_scanThread && m_scanThread->isRunning()) {
        m_scanThread->stop();
        m_scanThread->wait();
    }

    m_isScanning = false;
    m_progressLabel->setText(tr("扫描已停止"));

    // 如果有结果则显示，否则返回信息页
    if (!m_scanResults.isEmpty()) {
        m_stackedLayout->setCurrentWidget(m_resultPage);
        updateResultTree();
    } else {
        m_stackedLayout->setCurrentWidget(m_infoPage);
    }
}

// 递归收集选中的项目 - 避免重复收集
// 分类节点（顶级节点）只用于遍历，不添加到 selectedItems
// 只收集实际的文件/目录路径
static void collectSelectedItems(QTreeWidgetItem *item, ScanCategory category, 
                                  QList<ScanResult> &selectedItems, bool parentChecked = false)
{
    bool isChecked = item->checkState(0) == Qt::Checked;
    
    // 检查是否是分类节点（顶级节点）- 分类节点的 data(0, UserRole) 存储的是类别枚举整数
    // 子项目的 data(0, UserRole) 存储的是路径字符串（以 '/' 开头）
    QString path = item->data(0, Qt::UserRole).toString();
    bool isCategoryNode = !path.startsWith('/') && !path.isEmpty();
    
    // 如果是子项目（非分类节点）且被勾选，添加到选中列表
    if (!isCategoryNode && (isChecked || parentChecked) && !item->text(0).contains("...")) {
        if (!path.isEmpty() && path.startsWith('/')) {
            ScanResult result;
            result.path = path;
            result.size = item->data(1, Qt::UserRole).toLongLong();
            // 获取不带图标的显示名称
            result.name = item->data(0, Qt::UserRole + 2).toString();
            if (result.name.isEmpty()) {
                result.name = item->text(0);
            }
            // 获取应用ID（用于玲珑应用卸载）
            result.appId = item->data(0, Qt::UserRole + 1).toString();
            result.category = category;
            // 获取部署编号（用于 undeploy 操作）
            int di = item->data(3, Qt::UserRole + 1).toInt();
            if (di >= 0) {
                result.deployIndex = di;
                LOG_INFO(QString("Collecting deploy item: %1, deployIndex=%2").arg(result.name).arg(di));
            }
            selectedItems.append(result);
        }
        
        // 如果当前子项目被勾选，不需要再递归处理子项
        if (isChecked) {
            return;
        }
    }
    
    // 递归处理子项
    for (int i = 0; i < item->childCount(); ++i) {
        collectSelectedItems(item->child(i), category, selectedItems, isChecked || parentChecked);
    }
}

void AnalyzeWidget::onCleanupClicked()
{
    // 收集选中的项目
    QList<ScanResult> selectedItems;

    for (int i = 0; i < m_resultTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *categoryItem = m_resultTree->topLevelItem(i);
        ScanCategory category = static_cast<ScanCategory>(
            categoryItem->data(0, Qt::UserRole).toInt());
        
        // 递归收集所有选中的项（包括子目录）
        collectSelectedItems(categoryItem, category, selectedItems, false);
    }

    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请选择要清理的项目"));
        return;
    }

    // 确认对话框
    qint64 totalSize = 0;
    for (const ScanResult &item : selectedItems) {
        totalSize += item.size;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("确认清理"),
        tr("确定要清理选中的 %1 个项目吗？\n\n将释放空间: %2")
            .arg(selectedItems.size())
            .arg(formatSize(totalSize)),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        emit cleanupRequested(selectedItems);
    }
}

void AnalyzeWidget::onScanProgress(const QString &currentItem, int percent)
{
    m_progressLabel->setText(tr("正在扫描: %1").arg(currentItem));
    m_progressBar->setValue(percent);
    QApplication::processEvents();
}

void AnalyzeWidget::onCategoryScanned(ScanCategory category, const QList<ScanResult> &results)
{
    if (!results.isEmpty()) {
        m_scanResults[category] = results;

        for (const ScanResult &result : results) {
            m_totalSize += result.size;
            m_totalFiles += result.fileCount;
        }
    }
}

void AnalyzeWidget::onScanFinished(qint64 totalSize, int totalFiles)
{
    LOG_INFO(QString("Scan finished, total size: %1, files: %2")
                .arg(totalSize).arg(totalFiles));

    m_isScanning = false;
    m_totalSize = totalSize;
    m_totalFiles = totalFiles;

    // 切换到结果页
    m_stackedLayout->setCurrentWidget(m_resultPage);
    updateResultTree();
    updateTotalSize();

    emit scanFinished(totalSize);
}

void AnalyzeWidget::removeCleanedItems(const QList<ScanResult> &cleanedItems)
{
    LOG_INFO(QString("Removing %1 cleaned items from result tree").arg(cleanedItems.size()));
    
    // 收集已清理项目的路径
    QSet<QString> cleanedPaths;
    for (const ScanResult &item : cleanedItems) {
        cleanedPaths.insert(item.path);
    }
    
    // 遍历树形控件，移除已清理的项目
    QList<QTreeWidgetItem*> itemsToRemove;
    
    for (int i = 0; i < m_resultTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *categoryItem = m_resultTree->topLevelItem(i);
        
        // 检查子项
        for (int j = categoryItem->childCount() - 1; j >= 0; --j) {
            QTreeWidgetItem *childItem = categoryItem->child(j);
            QString itemPath = childItem->data(0, Qt::UserRole).toString();
            
            if (cleanedPaths.contains(itemPath)) {
                categoryItem->removeChild(childItem);
                delete childItem;
            }
        }
        
        // 如果分类下没有子项了，标记移除
        if (categoryItem->childCount() == 0) {
            itemsToRemove.append(categoryItem);
        }
    }
    
    // 移除空的分类项
    for (QTreeWidgetItem *item : itemsToRemove) {
        int index = m_resultTree->indexOfTopLevelItem(item);
        if (index >= 0) {
            m_resultTree->takeTopLevelItem(index);
            delete item;
        }
    }
    
    // 更新总大小显示
    updateTotalSize();
    
    // 如果没有剩余项目，显示提示
    if (m_resultTree->topLevelItemCount() == 0) {
        m_resultTree->setHeaderLabel(tr("所有项目已清理完成"));
    }
    
    LOG_INFO("Result tree updated after cleanup");
}

void AnalyzeWidget::onTreeItemChanged(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    // 如果是父项被勾选，自动勾选所有子项
    if (item->childCount() > 0) {
        Qt::CheckState state = item->checkState(0);
        for (int i = 0; i < item->childCount(); ++i) {
            item->child(i)->setCheckState(0, state);
        }
    }

    updateTotalSize();
}

void AnalyzeWidget::updateResultTree()
{
    m_resultTree->clear();

    for (auto it = m_scanResults.begin(); it != m_scanResults.end(); ++it) {
        ScanCategory category = it.key();
        const QList<ScanResult> &results = it.value();

        addCategoryToTree(category, results);
    }

    m_resultTree->expandAll();
}

// 递归添加子目录到树
static void addResultToTreeItem(QTreeWidgetItem *parent, const ScanResult &result, 
                                 const std::function<QString(qint64)> &formatSize)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(parent);
    
    // 设置图标和名称
    QString icon = result.purpose.isEmpty() ? "📁" : result.purpose.left(2);
    if (!result.purpose.isEmpty() && result.purpose.contains(' ')) {
        icon = result.purpose.section(' ', 0, 0);
    }
    item->setText(0, icon + " " + result.name);
    
    // 设置大小
    item->setText(1, formatSize(result.size));
    item->setData(1, Qt::UserRole, result.size);
    
    // 设置用途/描述
    if (!result.purpose.isEmpty()) {
        QString purposeText = result.purpose;
        if (purposeText.contains(' ')) {
            purposeText = purposeText.section(' ', 1); // 去掉图标
        }
        item->setText(2, purposeText);
    } else if (!result.description.isEmpty()) {
        item->setText(2, result.description);
    }
    
    // 设置路径
    item->setText(3, result.path);
    item->setData(0, Qt::UserRole, result.path);
    
    // 存储应用ID（用于玲珑应用卸载）
    item->setData(0, Qt::UserRole + 1, result.appId);
    
    // 存储显示名称（不带图标）
    item->setData(0, Qt::UserRole + 2, result.name);
    
    // 存储部署编号（用于 undeploy 操作，-1 表示非部署项）
    if (result.deployIndex >= 0) {
        item->setData(3, Qt::UserRole + 1, result.deployIndex);
        LOG_INFO(QString("Storing deployIndex=%1 for item: %2").arg(result.deployIndex).arg(result.name));
    }
    
    // 设置状态 - 根据类别和危险程度显示不同状态
    if (result.isDangerous) {
        item->setText(4, "⚠️ " + QObject::tr("危险"));
        item->setForeground(4, QBrush(QColor("#e74c3c")));
    } else if (result.category == ScanCategory::USER_CACHE || 
               result.category == ScanCategory::BROWSER_CACHE ||
               result.category == ScanCategory::DEV_CACHE ||
               result.category == ScanCategory::TEMP_FILES ||
               result.category == ScanCategory::LINGLONG_APPS) {
        // 用户缓存、浏览器缓存、开发缓存、临时文件、玲珑应用 - 可能包含用户数据
        item->setText(4, "🔶 " + QObject::tr("注意"));
        item->setForeground(4, QBrush(QColor("#f39c12")));
    } else {
        item->setText(4, QObject::tr("安全"));
        item->setForeground(4, QBrush(QColor("#27ae60")));
    }
    
    // 设置勾选框
    item->setCheckState(0, Qt::Unchecked);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    
    // 设置工具提示
    if (!result.description.isEmpty()) {
        item->setToolTip(0, result.description);
    }
    
    // 如果大小超过500MB，高亮显示
    if (result.size > 500 * 1024 * 1024) {
        item->setBackground(1, QBrush(QColor("#ffeaa7")));
    }
    
    // 递归添加子目录
    for (const ScanResult &child : result.children) {
        addResultToTreeItem(item, child, formatSize);
    }
}

void AnalyzeWidget::addCategoryToTree(ScanCategory category, const QList<ScanResult> &results)
{
    qint64 categorySize = 0;
    for (const ScanResult &result : results) {
        categorySize += result.size;
    }

    if (categorySize == 0 && results.isEmpty()) {
        return;
    }

    // 创建分类节点
    QTreeWidgetItem *categoryItem = new QTreeWidgetItem(m_resultTree);
    QString icon = getCategoryIcon(category);
    categoryItem->setText(0, icon + " " + getCategoryName(category));
    categoryItem->setText(1, formatSize(categorySize));
    categoryItem->setText(2, tr("共 %1 项").arg(results.size()));
    categoryItem->setText(3, "");
    categoryItem->setText(4, results.isEmpty() ? tr("空") : tr("可清理"));
    categoryItem->setData(0, Qt::UserRole, static_cast<int>(category));
    categoryItem->setData(1, Qt::UserRole, categorySize);  // 存储分类总大小
    categoryItem->setCheckState(0, Qt::Unchecked);
    categoryItem->setFlags(categoryItem->flags() | Qt::ItemIsUserCheckable);
    
    // 分类节点加粗显示
    QFont font = categoryItem->font(0);
    font.setBold(true);
    font.setPointSize(11);
    categoryItem->setFont(0, font);
    categoryItem->setFont(1, font);

    // 超过100MB高亮显示
    if (categorySize > 100 * 1024 * 1024) {
        categoryItem->setBackground(1, QBrush(QColor("#fff3cd")));
    }

    // 添加子项
    for (const ScanResult &result : results) {
        addResultToTreeItem(categoryItem, result, [this](qint64 size) { return formatSize(size); });
    }
}

// 递归计算选中项的大小 - 避免重复计算
// 如果父节点被勾选，只计算父节点的大小，不计算子节点
// 如果父节点未被勾选，递归计算被勾选的子节点
static void calculateSelectedSizeRecursive(QTreeWidgetItem *item, qint64 &totalSize, bool parentChecked = false)
{
    bool isChecked = item->checkState(0) == Qt::Checked;
    
    if (isChecked || parentChecked) {
        // 如果当前节点被勾选，或者父节点被勾选，计算当前节点大小
        qint64 size = item->data(1, Qt::UserRole).toLongLong();
        if (size > 0) {
            totalSize += size;
        }
        
        // 如果当前节点被勾选，子节点不需要再计算（因为父节点已经代表了整体）
        if (isChecked) {
            return;
        }
    }
    
    // 递归处理子项
    for (int i = 0; i < item->childCount(); ++i) {
        calculateSelectedSizeRecursive(item->child(i), totalSize, isChecked || parentChecked);
    }
}

void AnalyzeWidget::updateTotalSize()
{
    qint64 selectedSize = 0;

    for (int i = 0; i < m_resultTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *categoryItem = m_resultTree->topLevelItem(i);
        calculateSelectedSizeRecursive(categoryItem, selectedSize, false);
    }

    m_totalSizeLabel->setText(tr("已选择: %1").arg(formatSize(selectedSize)));
    m_cleanupButton->setEnabled(selectedSize > 0);
}

QString AnalyzeWidget::getCategoryName(ScanCategory category) const
{
    switch (category) {
    case ScanCategory::USER_CACHE: return tr("用户缓存");
    case ScanCategory::THUMBNAIL_CACHE: return tr("缩略图缓存");
    case ScanCategory::APT_CACHE: return tr("APT包缓存");
    case ScanCategory::SYSTEM_LOGS: return tr("系统日志");
    case ScanCategory::JOURNAL_LOGS: return tr("Journald日志");
    case ScanCategory::CRASH_REPORTS: return tr("崩溃报告");
    case ScanCategory::TEMP_FILES: return tr("临时文件");
    case ScanCategory::TRASH: return tr("回收站");
    case ScanCategory::BROWSER_CACHE: return tr("浏览器缓存");
    case ScanCategory::DEV_CACHE: return tr("开发工具缓存");
    case ScanCategory::LINGLONG_APPS: return tr("玲珑应用");
    case ScanCategory::IMMUTABLE_SNAPSHOTS: return tr("磐石系统快照");
    default: return tr("未知");
    }
}

QString AnalyzeWidget::getCategoryIcon(ScanCategory category) const
{
    switch (category) {
    case ScanCategory::USER_CACHE: return "🗂️";
    case ScanCategory::THUMBNAIL_CACHE: return "🖼️";
    case ScanCategory::APT_CACHE: return "📦";
    case ScanCategory::SYSTEM_LOGS: return "📋";
    case ScanCategory::JOURNAL_LOGS: return "📝";
    case ScanCategory::CRASH_REPORTS: return "💥";
    case ScanCategory::TEMP_FILES: return "📄";
    case ScanCategory::TRASH: return "🗑️";
    case ScanCategory::BROWSER_CACHE: return "🌐";
    case ScanCategory::DEV_CACHE: return "💻";
    case ScanCategory::LINGLONG_APPS: return "🔷";
    case ScanCategory::IMMUTABLE_SNAPSHOTS: return "📸";
    default: return "📁";
    }
}

QString AnalyzeWidget::formatSize(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    const qint64 TB = 1024 * GB;

    if (bytes >= TB) {
        return QString("%1 TB").arg(bytes / (double)TB, 0, 'f', 2);
    } else if (bytes >= GB) {
        return QString("%1 GB").arg(bytes / (double)GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(bytes / (double)MB, 0, 'f', 2);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(bytes / (double)KB, 0, 'f', 2);
    } else {
        return QString("%1 B").arg(bytes);
    }
}

void AnalyzeWidget::onTreeContextMenu(const QPoint &pos)
{
    // 获取被右键点击的项
    QTreeWidgetItem *item = m_resultTree->itemAt(pos);
    if (!item) {
        return;
    }
    
    // 获取路径（存储在 UserRole 中）
    QString path = item->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) {
        // 尝试从第3列（路径列）获取
        path = item->text(3);
    }
    
    if (path.isEmpty()) {
        return;
    }
    
    // 创建右键菜单
    QMenu contextMenu(this);
    QAction *openFolderAction = contextMenu.addAction(tr("📂 打开所在文件夹"));
    QAction *copyPathAction = contextMenu.addAction(tr("📋 复制路径"));
    
    // 显示菜单并获取选中的动作
    QAction *selectedAction = contextMenu.exec(m_resultTree->viewport()->mapToGlobal(pos));
    
    if (selectedAction == openFolderAction) {
        // 打开文件夹
        QFileInfo fileInfo(path);
        QString dirPath = fileInfo.isDir() ? path : fileInfo.dir().absolutePath();
        
        LOG_INFO(QString("Opening folder: %1").arg(dirPath));
        QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    } else if (selectedAction == copyPathAction) {
        // 复制路径到剪贴板
        QApplication::clipboard()->setText(path);
        LOG_INFO(QString("Path copied to clipboard: %1").arg(path));
    }
}

bool AnalyzeWidget::isDarkTheme()
{
    QPalette palette = qApp->palette();
    QColor windowColor = palette.color(QPalette::Window);
    int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
    return brightness < 128;
}

void AnalyzeWidget::applyTheme()
{
    bool darkMode = isDarkTheme();

    if (darkMode) {
        // 深色主题样式
        m_resultTree->setStyleSheet(
            "QTreeWidget {"
            "   border: 1px solid #444;"
            "   border-radius: 5px;"
            "   background-color: #2d2d2d;"
            "   color: #e0e0e0;"
            "   font-size: 12px;"
            "   outline: none;"
            "}"
            "QTreeWidget::item {"
            "   padding: 6px;"
            "   border-bottom: 1px solid #3d3d3d;"
            "   color: #e0e0e0;"
            "   outline: none;"
            "}"
            "QTreeWidget::item:selected {"
            "   background-color: #3d5a80;"
            "   color: #ffffff;"
            "   border: none;"
            "   outline: none;"
            "}"
            "QTreeWidget::item:hover {"
            "   background-color: #3d3d3d;"
            "}"
            "QTreeWidget::item:focus {"
            "   outline: none;"
            "   border: none;"
            "}"
            "QHeaderView::section {"
            "   background-color: #3d3d3d;"
            "   color: #e0e0e0;"
            "   padding: 8px;"
            "   font-weight: bold;"
            "   border: none;"
            "   border-right: 1px solid #444;"
            "}"
            "QTreeWidget::indicator {"
            "   width: 16px;"
            "   height: 16px;"
            "}"
            "QTreeWidget::indicator:unchecked {"
            "   border: 2px solid #666;"
            "   background: #2d2d2d;"
            "   border-radius: 3px;"
            "}"
            "QTreeWidget::indicator:checked {"
            "   border: 2px solid #27ae60;"
            "   background: #27ae60;"
            "   border-radius: 3px;"
            "   image: url(:/icons/check.svg);"
            "}"
            "QTreeWidget::indicator:indeterminate {"
            "   border: 2px solid #3498db;"
            "   background: #3498db;"
            "   border-radius: 3px;"
            "   image: url(:/icons/check.svg);"
            "}"
        );
        setStyleSheet("AnalyzeWidget { background-color: #2d2d2d; }");
    } else {
        // 浅色主题样式
        m_resultTree->setStyleSheet(
            "QTreeWidget {"
            "   border: 1px solid #bdc3c7;"
            "   border-radius: 5px;"
            "   background-color: white;"
            "   font-size: 12px;"
            "   outline: none;"
            "}"
            "QTreeWidget::item {"
            "   padding: 6px;"
            "   border-bottom: 1px solid #ecf0f1;"
            "   outline: none;"
            "}"
            "QTreeWidget::item:selected {"
            "   background-color: #a8c9d1;"
            "   color: #2C3E50;"
            "   border: none;"
            "   outline: none;"
            "}"
            "QTreeWidget::item:hover {"
            "   background-color: #eaf2f8;"
            "}"
            "QTreeWidget::item:focus {"
            "   outline: none;"
            "   border: none;"
            "}"
            "QHeaderView::section {"
            "   background-color: #ecf0f1;"
            "   padding: 8px;"
            "   font-weight: bold;"
            "   border: none;"
            "   border-right: 1px solid #bdc3c7;"
            "}"
            "QTreeWidget::indicator {"
            "   width: 16px;"
            "   height: 16px;"
            "}"
            "QTreeWidget::indicator:unchecked {"
            "   border: 2px solid #95a5a6;"
            "   background: white;"
            "   border-radius: 3px;"
            "}"
            "QTreeWidget::indicator:checked {"
            "   border: 2px solid #27ae60;"
            "   background: #27ae60;"
            "   border-radius: 3px;"
            "   image: url(:/icons/check.svg);"
            "}"
            "QTreeWidget::indicator:indeterminate {"
            "   border: 2px solid #3498db;"
            "   background: #3498db;"
            "   border-radius: 3px;"
            "   image: url(:/icons/check.svg);"
            "}"
        );
        setStyleSheet("AnalyzeWidget { background-color: #F5F5F5; }");
    }
}
