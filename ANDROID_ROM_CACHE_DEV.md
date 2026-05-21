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
