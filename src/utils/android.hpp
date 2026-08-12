#ifndef CHARTNAVIGATION_ANDROID_HPP
#define CHARTNAVIGATION_ANDROID_HPP

#include <QDebug>
#include <QString>
#include <QVector>


template <typename... Args>
QString debugAllToString (Args &&... args);

void grantAllFilesPermission ();
void persistAndroidTreeUri (const QString &uri);
void copyAndroidDebugText ();
QString grantFolderPermission ();
bool hasManageExternalStorage ();

struct SafChild {
    QString uri;        // content://.../document/<documentId>
    QString documentId; // 未编码的 document id
    QString name;       // 显示名
    bool isFolder{};
    bool isPdf{};
};

bool isSafTreeUri (const QString &uri);
QVector<SafChild> safListChildren (const QString &treeUri, const QString &parentDocumentId = {});
QString safCachePdf (const QString &filePath, const QString &cacheDir = {});


inline QString androidDebugText;
inline QMutex androidDebugMutex;
inline bool androidMasterAccess{hasManageExternalStorage()};


template <typename... Args>
QString debugAllToString (Args &&... args) {
    QMutexLocker locker(&androidDebugMutex);
    {
        QDebug dbg(&androidDebugText);
        dbg.nospace().noquote();
        (dbg << ... << std::forward<Args>(args));
    }
    return androidDebugText;
}

#endif //CHARTNAVIGATION_ANDROID_HPP
