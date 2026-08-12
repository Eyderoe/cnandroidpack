#ifndef CHARTNAVIGATION_ANDROID_HPP
#define CHARTNAVIGATION_ANDROID_HPP

#include <QDebug>


template <typename... Args>
QString debugAllToString (Args &&... args);

void grantAllFilesPermission ();
void persistAndroidTreeUri (const QString &uri);
void copyAndroidDebugText ();
void grantFolderPermission ();
bool hasManageExternalStorage ();


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
