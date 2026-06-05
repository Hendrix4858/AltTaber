#include "utils/UwpHelper.h"
#include "utils/Util.h"
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDomDocument>
#include <appmodel.h>

namespace AppUtil {
    HWND getAppFrameWindow(HWND hwnd) {
        auto className = Util::getClassName(hwnd);
        if (className == AppFrameWindowClass) return hwnd;
        auto title = Util::getWindowTitle(hwnd);
        if (auto res = FindWindowW(LPCWSTR(AppFrameWindowClass.utf16()), LPCWSTR(title.utf16())))
            return res;
        qWarning() << "Failed to find ApplicationFrameWindow of " << title << hwnd;
        return nullptr;
    }

    HWND getAppCoreWindow(HWND hwnd) {
        auto className = Util::getClassName(hwnd);
        if (className == AppCoreWindowClass) return hwnd;
        const auto childList = Util::enumChildWindows(hwnd);
        const auto title = Util::getWindowTitle(hwnd);
        for (HWND child: childList) {
            if (Util::getClassName(child) == AppCoreWindowClass && Util::getWindowTitle(child) == title) {
                return child;
            }
        }

        if (auto res = FindWindowW(LPCWSTR(AppCoreWindowClass.utf16()), LPCWSTR(title.utf16())))
            return res;
        qWarning() << "Failed to find ApplicationCoreWindow of " << title << hwnd;
        return nullptr;
    }

    bool isAppFrameWindow(HWND hwnd) {
        return Util::getClassName(hwnd) == AppFrameWindowClass;
    }

    QString getLogoPathFromAppxManifest(const QString& manifestPath) {
        QFile file(manifestPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "Failed to open AppxManifest.xml" << manifestPath;
            return {};
        }

        QDomDocument doc;
        if (!doc.setContent(&file)) {
            file.close();
            qDebug() << "Failed to parse XML";
            return {};
        }
        file.close();

        static const QStringList LogoAttributes = {
            "Square44x44Logo",
            "Square30x30Logo",
            "SmallLogo"
        };

        QDomElement root = doc.documentElement();
        QDomElement appElement = root.firstChildElement("Applications").firstChildElement("Application");
        QDomElement visualElement = appElement.firstChildElement("uap:VisualElements");
        for (const auto& attr: LogoAttributes) {
            if (auto value = visualElement.attribute(attr); !value.isEmpty())
                return value;
        }

        QDomElement storeLogo = root.firstChildElement("Properties").firstChildElement("Logo");
        if (!storeLogo.isNull())
            return storeLogo.text();

        return {};
    }

    QIcon loadUWPLogo(const QString& logoPath) {
        QFileInfo fileInfo(logoPath);
        QDir dir = fileInfo.absoluteDir();
        QString wildcard = fileInfo.baseName() + "*." + fileInfo.suffix();

        if (!dir.exists()) {
            qDebug() << "Directory does not exist!";
            return {};
        }

        QStringList filters;
        filters << wildcard;

        QStringList matchingFiles = dir.entryList(filters, QDir::Files, QDir::Size);

        auto _ = std::remove_if(matchingFiles.begin(), matchingFiles.end(), [](const QString& fileName) {
            return fileName.contains("_contrast-black.") || fileName.contains("_contrast-white.");
        });

        if (!matchingFiles.isEmpty()) {
            qDebug() << "Found matching file:" << matchingFiles.first();
            QString logoFile = dir.absoluteFilePath(matchingFiles.first());
            return QIcon(logoFile);
        } else {
            qWarning() << "No matching files found!";
        }
        return {};
    }

    QIcon getAppIcon(const QString& path) {
        QFileInfo fileInfo(path);
        const auto dir = fileInfo.absolutePath() + "\\";
        const auto manifestPath = dir + AppManifest;
        const auto logoPath = dir + getLogoPathFromAppxManifest(manifestPath);
        return loadUWPLogo(logoPath);
    }

    bool comparePackageFullNames(const wchar_t* a, const wchar_t* b) {
        PACKAGE_VERSION versionA, versionB;
        UINT32 lengthA = 0, lengthB = 0;

        if (SUCCEEDED(PackageIdFromFullName(a, PACKAGE_INFORMATION_BASIC, &lengthA, nullptr)) &&
            SUCCEEDED(PackageIdFromFullName(b, PACKAGE_INFORMATION_BASIC, &lengthB, nullptr))) {
            std::vector<BYTE> bufferA(lengthA), bufferB(lengthB);
            auto* idA = reinterpret_cast<PACKAGE_ID*>(bufferA.data());
            auto* idB = reinterpret_cast<PACKAGE_ID*>(bufferB.data());

            if (SUCCEEDED(PackageIdFromFullName(a, PACKAGE_INFORMATION_BASIC, &lengthA, bufferA.data())) &&
                SUCCEEDED(PackageIdFromFullName(b, PACKAGE_INFORMATION_BASIC, &lengthB, bufferB.data()))) {
                versionA = idA->version;
                versionB = idB->version;

                if (versionA.Major != versionB.Major) return versionA.Major > versionB.Major;
                if (versionA.Minor != versionB.Minor) return versionA.Minor > versionB.Minor;
                if (versionA.Build != versionB.Build) return versionA.Build > versionB.Build;
                return versionA.Revision > versionB.Revision;
            }
        }

        return wcscmp(a, b) > 0;
    }

    QString getUWPInstallDirByAUMID(const QString& AUMID) {
        QString installPath;
        QString packageFamilyName = AUMID.split('!').first();

        UINT32 count = 0;
        UINT32 bufferLength = 0;
        LONG result = GetPackagesByPackageFamily(packageFamilyName.toStdWString().c_str(), &count, nullptr, &bufferLength, nullptr);
        if (result == ERROR_INSUFFICIENT_BUFFER) {
            std::vector<PWSTR> fullNames(count);
            std::vector<wchar_t> buffer(bufferLength);
            result = GetPackagesByPackageFamily(packageFamilyName.toStdWString().c_str(), &count, fullNames.data(), &bufferLength,
                                                buffer.data());

            if (result == ERROR_SUCCESS && count > 0) {
                std::sort(fullNames.begin(), fullNames.end(), comparePackageFullNames);

                for (UINT32 i = 0; i < count; ++i) {
                    UINT32 pathLength = 0;
                    result = GetPackagePathByFullName(fullNames[i], &pathLength, nullptr);

                    if (result == ERROR_INSUFFICIENT_BUFFER) {
                        std::vector<wchar_t> pathBuffer(pathLength);
                        result = GetPackagePathByFullName(fullNames[i], &pathLength, pathBuffer.data());

                        if (result == ERROR_SUCCESS) {
                            installPath = QString::fromWCharArray(pathBuffer.data());
                            break;
                        }
                    }
                }
            }
        }

        if (installPath.isEmpty()) {
            qWarning() << "Failed to find package for AUMID:" << AUMID;
        }

        return installPath;
    }

    QString getExecutableFromAppxManifest(const QString& manifestPath, const QString& id) {
        QString exe;

        QFile file(manifestPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Failed to open AppxManifest.xml";
            return exe;
        }

        QDomDocument doc;
        if (!doc.setContent(&file)) {
            qWarning() << "Failed to parse XML content";
            return exe;
        }
        file.close();

        QDomElement root = doc.documentElement();
        QDomNodeList applications = root.elementsByTagName("Application");

        for (int i = 0; i < applications.size(); ++i) {
            QDomElement appElement = applications.at(i).toElement();
            if (!appElement.isNull() && appElement.attribute("Id") == id) {
                exe = appElement.attribute("Executable");
                break;
            }
        }

        return exe;
    }

    QString getUwpExePathByAUMID(const QString& AUMID) {
        auto applicationId = AUMID.split('!').last();
        auto dir = getUWPInstallDirByAUMID(AUMID);
        auto manifest = dir + '\\' + AppManifest;
        auto exePath = getExecutableFromAppxManifest(manifest, applicationId);
        return dir + '\\' + exePath;
    }
}
