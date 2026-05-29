#include "AppUserPaths.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

#include <QtGlobal>

namespace pvj::app {

QString userDataRoot()
{
#ifdef Q_OS_WIN
    QString base = qEnvironmentVariable("LOCALAPPDATA");
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    }
#elif defined(Q_OS_MACOS)
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#elif defined(Q_OS_LINUX)
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#else
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#endif
    if (base.isEmpty()) {
        base = QDir::homePath();
    }
    return QDir(base).filePath(QStringLiteral("PerformanieVJ"));
}

void configureUserStorage()
{
    const QString root = userDataRoot();
    QDir().mkpath(root);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, root);
}

} // namespace pvj::app
