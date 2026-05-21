# Android ROM 缓存复制功能开发记录

## 功能概述

在 Android 平台上，启动模拟器前将 ROM 文件复制到内部缓存目录，使用缓存路径启动模拟器，退出后自动清理。

## 代码修改步骤

### 1. ProcessLauncher.h - 新增信号和方法声明

```cpp
// 新增信号（在 Q_OS_ANDROID 条件编译内）
signals:
    void copyProgress(float progress, QString fileName);
    void copyFinished();
    void copyError(QString message);

// 新增私有成员和方法
private:
    QString m_cached_rom_path;

    QString cacheRomPath(const QString& originalPath);
    bool copyFileWithProgress(const QString& src, const QString& dst);
    void cleanCache();
```

### 2. ProcessLauncher.cpp - 核心复制逻辑

**新增头文件：**
```cpp
#ifdef Q_OS_ANDROID
#include "platform/AndroidHelpers.h"
#include <QCoreApplication>
#endif
```

**onLaunchRequested 方法修改：**
- 在变量替换前插入 Android ROM 复制逻辑
- 空间检查 → 复制文件 → 发送进度信号 → 使用缓存路径替换变量

**afterRun 方法修改：**
- 添加 `cleanCache()` 调用

**新增方法实现：**
- `cacheRomPath()` - 计算缓存路径
- `copyFileWithProgress()` - 带进度的文件复制（256KB 缓冲区）
- `cleanCache()` - 清理缓存文件和空目录

### 3. Backend.cpp - 信号连接

```cpp
#ifdef Q_OS_ANDROID
QObject::connect(m_launcher, &ProcessLauncher::copyProgress,
                 m_api_public, &model::ApiObject::onCopyProgress);
QObject::connect(m_launcher, &ProcessLauncher::copyFinished,
                 m_api_public, &model::ApiObject::onCopyFinished);
QObject::connect(m_launcher, &ProcessLauncher::copyError,
                 m_api_public, &model::ApiObject::onCopyError);
#endif
```

### 4. Api.h - QML 属性声明

```cpp
// Q_PROPERTY
Q_PROPERTY(bool copying READ isCopying NOTIFY copyingChanged)
Q_PROPERTY(float copyProgress READ copyProgress NOTIFY copyProgressChanged)

// 信号
void copyingChanged();
void copyProgressChanged();
void copyError(QString msg);

// 槽函数
void onCopyProgress(float progress, QString fileName);
void onCopyFinished();
void onCopyError(QString msg);

// 成员变量
bool m_copying = false;
float m_copy_progress = 0.f;
```

### 5. Api.cpp - 槽函数实现

- `onCopyProgress()` - 更新进度，发射信号
- `onCopyFinished()` - 重置状态
- `onCopyError()` - 重置状态并发射错误信号

### 6. main.qml - 进度条 UI

**新增 Connections 处理：**
```qml
function onCopyError(msg) {
    genericMessage.setSource("dialogs/GenericOkDialog.qml",
        { "title": qsTr("Error"), "message": msg });
    genericMessage.focus = true;
}
```

**新增全屏遮罩进度条：**
- 居中显示 "Copying ROM..." 文本
- 进度条（宽度 400px，圆角，绿色 #4CAF50）
- 百分比显示
- z-index: 1000（最高层级）

### 7. 版本号修改

**.qmake.conf：**
```qmake
# 使用当前日期作为版本号
GIT_REVISION = $$system(date +%Y-%m-%d)
GIT_DATE = $$system(date +%Y-%m-%d)
```

**cmake/PegasusGitInfo.cmake：**
```cmake
string(TIMESTAMP PEGASUS_GIT_REVISION "%Y-%m-%d")
string(TIMESTAMP PEGASUS_GIT_DATE "%Y-%m-%d")
```

### 8. CI 配置修改

**.github/workflows/x11.yml：**
- 触发条件改为 `[workflow_dispatch]`（仅手动触发）

**.github/workflows/android.yml：**
- 修改 release 签名配置使用本地 `.github/aks.enc`
- 添加 release 创建步骤（softprops/action-gh-release@v2）

## 签名配置步骤

### 1. 生成 Keystore

```bash
keytool -genkey -v -keystore pegasus-release.jks \
  -keyalg RSA -keysize 2048 -validity 10000 \
  -alias pegasus \
  -storepass pegasus123 \
  -keypass pegasus123 \
  -dname "CN=Pegasus, OU=Dev, O=Home, L=City, ST=State, C=CN"
```

### 2. 加密 Keystore

```bash
openssl aes-256-cbc -k "pegasus_enc_key_2026" \
  -in pegasus-release.jks \
  -out .github/aks.enc
```

### 3. 配置 GitHub Secrets

在仓库 Settings → Secrets and variables → Actions 添加：

| 名称 | 值 |
|---|---|
| `AKS_ENC_KEY` | `pegasus_enc_key_2026` |
| `AKS_ALIAS` | `pegasus` |
| `AKS_STOREPASS` | `pegasus123` |
| `AKS_KEYPASS` | `pegasus123` |

## 缓存目录结构

```
/storage/emulated/0/
├── Roms/
│   └── PSP/xxx.iso              ← 原始 ROM
└── pegasus-frontend/
    └── copyRoms/                ← 缓存目录
        └── Roms/PSP/xxx.iso    ← 缓存副本（退出后删除）
```

## 数据流

```
用户点击游戏
    ↓
ProcessLauncher::onLaunchRequested()
    ↓
copyFileWithProgress()
    ├─ emit copyProgress(0.3, "xxx.iso")
    │      ↓
    │  ApiObject → QML 显示进度条
    ↓
emit copyFinished() → QML 隐藏进度条
    ↓
用缓存路径替换 {file.path}
    ↓
runProcess() → 启动模拟器
    ↓
模拟器退出 → afterRun() → cleanCache()
```

---

# 日志查看器功能

## 功能概述

在应用设置中添加日志查看页面，实时显示应用运行日志，方便调试。

## 代码修改步骤

### 1. LogModel.h/cpp - 日志数据模型

```cpp
class LogModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(int count READ count NOTIFY messagesChanged)

public:
    explicit LogModel(QObject* parent = nullptr);
    QStringList messages() const;
    int count() const;
    void addMessage(const QString& msg);

public slots:
    void clear();

signals:
    void messagesChanged();

private:
    QStringList m_messages;
};
```

### 2. ModelLogSink.h/cpp - 日志接收器

```cpp
class ModelLogSink : public LogSink {
public:
    explicit ModelLogSink(model::LogModel* model);
    void info(const QString& msg) override;
    void warning(const QString& msg) override;
    void error(const QString& msg) override;

private:
    model::LogModel* m_model;
};
```

### 3. Log.h/cpp - 添加 addSink 方法

```cpp
// Log.h
static void addSink(std::unique_ptr<LogSink> sink);

// Log.cpp
void Log::addSink(std::unique_ptr<LogSink> sink)
{
    m_sinks.emplace_back(std::move(sink));
}
```

### 4. Internal.h - 添加 LogModel 属性

```cpp
#include "LogModel.h"

QML_CONST_PROPERTY(model::LogModel, log)
```

### 5. Backend.cpp - 连接日志接收器

```cpp
#include "ModelLogSink.h"

// 在构造函数中
m_api_private = new model::Internal(args);
Log::addSink(std::make_unique<ModelLogSink>(m_api_private->logPtr()));
```

### 6. LogScreen.qml - 日志查看器 UI

- 使用 MenuScreen 作为基础
- 显示日志消息列表
- 支持清除日志按钮
- 自动滚动到最新消息
- 使用等宽字体显示

### 7. MainMenuPanel.qml - 添加菜单项

```qml
signal showLogScreen

PrimaryMenuItem {
    id: mbLogs
    text: qsTr("Logs") + api.tr
    onActivated: {
        focus = true;
        root.showLogScreen();
    }
    selected: focus
    KeyNavigation.down: scopeQuit
}
```

### 8. MenuLayer.qml - 加载日志页面

```qml
onShowLogScreen: root.openScreen("menu/logs/LogScreen.qml")
```

### 9. 构建文件更新

**internal.pri:**
```qmake
HEADERS += $$PWD/LogModel.h
SOURCES += $$PWD/LogModel.cpp
```

**model/CMakeLists.txt:**
```cmake
internal/LogModel.cpp
internal/LogModel.h
```

**backend.pro:**
```qmake
SOURCES += ModelLogSink.cpp
HEADERS += ModelLogSink.h
```

**frontend.qrc:**
```xml
<file>menu/logs/LogScreen.qml</file>
```

---

# Android 游戏进程退出检测

## 问题

Android 上使用 `startActivity()` 启动模拟器是异步的，Java 立即返回。模拟器退出时 Pegasus 无法感知，导致前端卡死等待。

## 解决方案

使用 `startActivityForResult()` + `onActivityResult()` 回调模式，模拟器退出时系统自动回调。

### 1. MainActivity.java

```java
private static final int REQUEST_LAUNCH_GAME = 1001;

// 启动时使用 startActivityForResult
m_self.startActivityForResult(intent, REQUEST_LAUNCH_GAME);

// 回调
@Override
protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    super.onActivityResult(requestCode, resultCode, data);
    if (requestCode == REQUEST_LAUNCH_GAME) {
        onGameProcessFinished();  // JNI 回调
    }
}

private static native void onGameProcessFinished();
```

### 2. AndroidHelpers.h - GameProcessNotifier 单例

```cpp
namespace android {
class GameProcessNotifier : public QObject {
    Q_OBJECT
public:
    static GameProcessNotifier* instance();
    void notifyFinished();
signals:
    void gameProcessFinished();
};
}
```

### 3. AndroidHelpers.cpp - JNI 动态注册

```cpp
static void nativeOnGameProcessFinished(JNIEnv *env, jclass clazz) {
    android::GameProcessNotifier::instance()->notifyFinished();
}

static JNINativeMethod methods[] = {
    {"onGameProcessFinished", "()V", (void*)nativeOnGameProcessFinished}
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    // 注册 native 方法
}
```

### 4. ProcessLauncher.cpp - 处理完成信号

```cpp
void ProcessLauncher::onAndroidGameProcessFinished() {
    afterRun();
    emit processFinished();
}
```

### 5. Backend.cpp - 连接信号

```cpp
QObject::connect(android::GameProcessNotifier::instance(),
    &android::GameProcessNotifier::gameProcessFinished,
    m_launcher, &ProcessLauncher::onAndroidGameProcessFinished);
```

---

# 游戏列表缓存功能

## 功能概述

首次启动时扫描所有 provider 构建游戏列表并缓存到 JSON 文件。后续启动直接从缓存加载，跳过全量扫描。缓存永久保存，仅在用户手动刷新时更新。

解决了 USB 外接存储设备重新挂载后 `lastModified` 时间戳变化导致缓存反复失效的问题。

## 架构

```
ProviderManager::run()
    ↓
GameCache::exists()
    ├─ 存在 → GameCache::load() → 返回缓存数据
    └─ 不存在 → 执行 provider 扫描 → GameCache::save()
```

## 文件

### GameCache.h/cpp

JSON 缓存实现，位于 `src/backend/providers/game_cache/`。

**存储格式**：`gamecache.json`（Compact JSON）

**JSON 结构：**
```json
{
  "games": [
    {
      "key": "title\0filepath",
      "title": "...", "sort_by": "...",
      "developers": "...", "publishers": "...",
      "assets": { "boxFront": ["..."] },
      "files": [{ "path": "...", "name": "...", "uri": "..." }],
      "collection_names": ["PSP", "Favorites"]
    }
  ],
  "collections": [
    {
      "name": "PSP", "short_name": "psp",
      "assets": { ... },
      "game_keys": ["title\0filepath"]
    }
  ]
}
```

**核心方法：**
```cpp
bool exists() const;
bool load(std::vector<Collection*>& collections, std::vector<Game*>& games);
void save(const std::vector<Collection*>&, const std::vector<Game*>&);
void invalidate();
```

### game_cache.pri

qmake 构建文件。

## 修改文件

### Assets.h

新增公开访问器用于序列化：
```cpp
const HashMap<AssetType, QStringList, EnumHash>& allAssets() const;
```

### ProviderManager.cpp

集成缓存逻辑：
- `default_cache_path()` - 返回 JSON 缓存路径
- run() 中：缓存存在 → 加载，不存在 → 扫描 → 保存

### providers/CMakeLists.txt / providers.pri

注册 GameCache 源文件。

## 缓存行为

- 缓存文件存在 → 直接加载，不做任何校验
- 缓存文件不存在 → 全量扫描 → 保存
- 用户手动刷新 → 重新扫描 → 覆盖保存
- 缓存损坏（JSON 解析失败）→ 回退到全量扫描

## 唯一键策略

游戏使用 `title + \0 + first_file_path` 作为唯一键，处理同名游戏场景。

## 线程安全

- GameCache 在 QtConcurrent 线程中使用
- 仅文件 I/O，无共享状态
- QObject 对象在主线程创建，信号通过 AutoConnection 自动排队

## 从 SQLite 迁移

旧版使用 `gamecache.db`（SQLite），新版使用 `gamecache.json`。旧文件不会自动删除，可手动移除。
