#include "android.hpp"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>
#include <QUrl>

#if defined(__ANDROID__)
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#endif

/**
 * @brief 跳转系统设置, 授权"所有文件访问"(MANAGE_EXTERNAL_STORAGE, 仅安卓)
 * @note 安卓 11 (API 30) 以上才有此设置页; 授权后裸路径(/storage/emulated/0/...)才可读
 */
void grantAllFilesPermission () {
#if defined(__ANDROID__)
    // 安卓 11 (API 30) 以下没有该权限
    if (QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT") < 30)
        return;
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    // Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION)
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      QJniObject::fromString("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION").object());
    // intent.setData(Uri.parse("package:" + getPackageName()))
    QJniObject packageName = context.callObjectMethod("getPackageName", "()Ljava/lang/String;");
    QJniObject uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString("package:" + packageName.toString()).object());
    intent.callObjectMethod("setData", "(Landroid/net/Uri;)Landroid/content/Intent;", uri.object());
    // Intent.FLAG_ACTIVITY_NEW_TASK
    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", 0x10000000);
    context.callObjectMethod("startActivity", "(Landroid/content/Intent;)V", intent.object());
#endif
}

/**
 * @brief 持久化SAF目录授权
 * @param uri 系统文件夹选择器返回的 content:// tree URI
 * @note 不持久化的话, 设备重启后授权会丢失, 文件树又只能看到目录
 */
void persistAndroidTreeUri (const QString &uri) {
#if defined(__ANDROID__)
    if (!uri.startsWith("content://"))
        return;
    // android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION | FLAG_GRANT_WRITE_URI_PERMISSION
    constexpr jint readWriteFlags = 0x1 | 0x2;
    QJniObject androidUri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString(uri).object());
    // Qt 6.8+/6.9 的 context() 返回类型化 QtJniTypes::Context(JObject<ContextTag>),
    // 可隐式转成 QJniObject, 再走经典 API 调 ContentResolver
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject resolver = context.callObjectMethod("getContentResolver",
                                                   "()Landroid/content/ContentResolver;");
    resolver.callMethod<void>("takePersistableUriPermission",
                              "(Landroid/net/Uri;I)V", androidUri.object(), readWriteFlags);
#endif
}

/**
 * @brief 检查有无所有文件获取权限
 * @return 是否
 */
bool hasManageExternalStorage () {
#if defined(__ANDROID__)
    QJniObject result = QJniObject::callStaticObjectMethod(
        "android/os/Environment",
        "isExternalStorageManager",
        "()Z"
    );
    return result.toString() == "true";
#endif
    return false;
}

/**
 * @brief 复制Debug文本到剪贴板(仅安卓)
 */
void copyAndroidDebugText () {
    QApplication::clipboard()->setText(androidDebugText);
}

/**
 * @brief 获取文件夹访问权限(仅安卓)
 * @return 选择的目录: 安卓为 content:// tree URI, 桌面为裸路径; 取消为空
 * @note 调起系统文件夹选择器(SAF), 持久化授权后把 content:// 路径复制到剪贴板
 */
QString grantFolderPermission () {
    auto option = QFileDialog::Options();
    option |= QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks;
    const QString dir = QFileDialog::getExistingDirectory(nullptr, "选择文件夹以获取访问权限",
                                                          QDir::homePath(), option);
    if (dir.isEmpty())
        return {};
    persistAndroidTreeUri(dir); // 持久化授权, 避免设备重启后授权丢失
    QApplication::clipboard()->setText(dir);
    return dir;
}

/**
 * @brief 判断是否为 SAF 目录树 URI (content://.../tree/...)
 */
bool isSafTreeUri (const QString &uri) {
    return uri.startsWith("content://") && uri.contains("/tree/");
}

/**
 * @brief 枚举 SAF 目录树下的直接子项
 * @param treeUri content://.../tree/... URI
 * @param parentDocumentId 父项 document id (空 = 树根); 也可传子项 document URI 自动解析
 * @return 子项列表 (文件/文件夹), 权限缺失或非安卓时为空
 * @note 通过 DocumentsContract + ContentResolver 查询, 不走 QDir (QDir 不支持 content://)
 */
QVector<SafChild> safListChildren (const QString &treeUri, const QString &parentDocumentId) {
#if defined(__ANDROID__)
    QVector<SafChild> result;
    if (!isSafTreeUri(treeUri))
        return result;
    QJniObject tree = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString(treeUri).object());
    // 解析父级 document id: 支持空(树根) / 纯 id / 完整 document URI
    QString parentId = parentDocumentId;
    if (parentId.startsWith("content://")) {
        const int slash = parentId.lastIndexOf('/');
        parentId = (slash >= 0) ? parentId.mid(slash + 1) : parentId;
        parentId = QUrl::fromPercentEncoding(parentId.toUtf8());
    }
    if (parentId.isEmpty())
        parentId = QJniObject::callStaticObjectMethod(
            "android/provider/DocumentsContract", "getTreeDocumentId",
            "(Landroid/net/Uri;)Ljava/lang/String;", tree.object()).toString();
    if (parentId.isEmpty())
        return result;
    QJniObject childrenUri = QJniObject::callStaticObjectMethod(
        "android/provider/DocumentsContract", "buildChildDocumentsUriUsingTree",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;",
        tree.object(), QJniObject::fromString(parentId).object());
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject resolver = context.callObjectMethod("getContentResolver",
                                                   "()Landroid/content/ContentResolver;");
    // projection 传 null, 让 provider 返回默认列, 避免 C++ 侧构造 String[]
    QJniObject cursor = resolver.callObjectMethod(
        "query",
        "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;",
        childrenUri.object(), nullptr, nullptr, nullptr, nullptr);
    if (!cursor.isValid())
        return result;
    while (cursor.callMethod<jboolean>("moveToNext")) {
        SafChild child;
        child.documentId = cursor.callObjectMethod(
            "getString", "(I)Ljava/lang/String;",
            cursor.callMethod<jint>("getColumnIndex", "(Ljava/lang/String;)I",
                                    QJniObject::fromString("document_id").object())).toString();
        if (child.documentId.isEmpty())
            continue;
        child.name = cursor.callObjectMethod(
            "getString", "(I)Ljava/lang/String;",
            cursor.callMethod<jint>("getColumnIndex", "(Ljava/lang/String;)I",
                                    QJniObject::fromString("_display_name").object())).toString();
        if (child.name.isEmpty()) {
            const int slash = child.documentId.lastIndexOf('/');
            child.name = (slash >= 0) ? child.documentId.mid(slash + 1) : child.documentId;
        }
        const QString mime = cursor.callObjectMethod(
            "getString", "(I)Ljava/lang/String;",
            cursor.callMethod<jint>("getColumnIndex", "(Ljava/lang/String;)I",
                                    QJniObject::fromString("mime_type").object())).toString();
        child.isFolder = (mime == "vnd.android.document/directory");
        if (!child.isFolder)
            child.isPdf = (mime == "application/pdf")
                    || child.name.endsWith(".pdf", Qt::CaseInsensitive);
        child.uri = QJniObject::callStaticObjectMethod(
            "android/provider/DocumentsContract", "buildDocumentUriUsingTree",
            "(Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;",
            tree.object(), QJniObject::fromString(child.documentId).object()).toString();
        result.push_back(child);
    }
    cursor.callMethod<void>("close");
    return result;
#else
    return {};
#endif
}

/**
 * @brief 把 content:// 文档拷贝到本地缓存, 返回本地路径
 * @param filePath 文件路径; 非 content:// (桌面/裸路径) 原样返回
 * @param cacheDir 缓存目录; 空则用 QStandardPaths::CacheLocation
 * @return 本地缓存路径; 失败为空
 * @note SAF 每次读文件都走 ContentResolver, 拷贝到本地后 QPdfDocument 渲染不再反复跨进程
 */
QString safCachePdf (const QString &filePath, const QString &cacheDir) {
    if (!filePath.startsWith("content://"))
        return filePath;
    // 从 document URI 尾部还原原文件名: primary%3ADownload%2FZUCK.pdf → ZUCK.pdf
    QString name = QUrl::fromPercentEncoding(QUrl(filePath).fileName().toUtf8());
    const int slash = name.lastIndexOf('/');
    if (slash >= 0)
        name = name.mid(slash + 1);
    if (name.isEmpty())
        name = "chart.pdf";
    if (!name.endsWith(".pdf", Qt::CaseInsensitive))
        name += ".pdf";
    const QDir dir(cacheDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                                      : cacheDir);
    if (!dir.exists() && !dir.mkpath("."))
        return {};
    const QString dest = dir.filePath(name);
    if (QFile::exists(dest)) // 已缓存过, 直接用
        return dest;
    QFile src(filePath);
    if (!src.open(QIODevice::ReadOnly))
        return {};
    QFile dst(dest);
    if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        src.close();
        return {};
    }
    char buffer[64 * 1024];
    while (!src.atEnd()) {
        const qint64 n = src.read(buffer, sizeof(buffer));
        if (n <= 0 || dst.write(buffer, n) != n) {
            dst.remove();
            src.close();
            return {};
        }
    }
    src.close();
    dst.close();
    return dest;
}
