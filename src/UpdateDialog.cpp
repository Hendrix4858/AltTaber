// You may need to build the project (run Qt uic code generator) to get "ui_UpdateDialog.h" resolved

#include "UpdateDialog.h"
#include <windows.h>
#include <QCommandLineParser>
#include "ui_UpdateDialog.h"
#include <QDebug>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>

#include "lifecycle/SystemTray.h"
#include "core/ThemeManager.h"
#include "core/QuitReason.h"
#include <QProcess>
#include <QFileInfo>
#include <QEvent>
#include <QTimer>

UpdateDialog::UpdateDialog(QWidget* parent) : QDialog(parent), ui(new Ui::UpdateDialog) {
    QElapsedTimer t;
    t.start();
    ui->setupUi(this);
    {
        HWND h = (HWND)winId();
        auto ex = GetWindowLongW(h, GWL_EXSTYLE);
        SetWindowLongW(h, GWL_EXSTYLE, ex | WS_EX_NOACTIVATE);
    }
    retranslateTexts();
    qDebug() << QSslSocket::sslLibraryBuildVersionString() << QSslSocket::supportsSsl();

    applyThemeStyle();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &UpdateDialog::applyThemeStyle);

    manager.setTransferTimeout(10000); // 10s -> Operation canceled
    ui->progressBar->hide();
    connect(ui->btn_recheck, &QPushButton::clicked, this, &UpdateDialog::fetchGithubReleaseInfo);
    connect(ui->ckPreRelease, &QCheckBox::toggled, this, [this](bool checked) {
        m_includePreRelease = checked;
        if (!m_cachedReleases.isEmpty()) {
            QJsonObject bestObj;
            QVersionNumber bestVer;
            for (const auto& val : m_cachedReleases) {
                auto obj = val.toObject();
                if (!m_includePreRelease && obj["prerelease"].toBool())
                    continue;
                auto ver = normalizeVersion(obj["tag_name"].toString());
                if (!bestObj.isEmpty() && ver <= bestVer)
                    continue;
                bestObj = obj;
                bestVer = ver;
            }
            if (!bestObj.isEmpty()) {
                applyRelease(bestObj);
            } else {
                m_phase = Phase::FetchError;
                m_errorCache = tr("No matching release found");
                retranslateTexts();
            }
        }
    });
    connect(ui->btn_update, &QPushButton::clicked, this, [this] {
        ui->btn_update->setEnabled(false);
        auto url = relInfo.downloadUrl;
        auto fileName = QUrl(url).fileName();
        download(url, qApp->applicationDirPath() + '/' + fileName);
    });
    qInfo() << "UpdateDialog initialized in" << t.elapsed() << "ms";
    connect(this, &UpdateDialog::downloadSucceed, this, [this](const QString& filePath) {
        qInfo() << "Download succeed" << filePath;
        if (!filePath.endsWith(".exe", Qt::CaseInsensitive)) {
            qWarning() << "Downloaded file is not an executable, skip launch:" << filePath;
            m_phase = Phase::DownloadFailed;
            m_errorCache = tr("Asset type mismatch: expected installer, got `%1`.").arg(QFileInfo(filePath).suffix());
            retranslateTexts();
            sysTray().showMessage(tr("Download failed"), tr("Asset type mismatch: not an installer executable"));
            return;
        }
        QProcess::startDetached(filePath, {"/VERYSILENT", "/SUPPRESSMSGBOXES",
                                           "/CLOSEAPPLICATIONS", "/AUTOUPDATE"});
        QuitReason::markIntentional();
        m_phase = Phase::Installing;
        ui->progressBar->show();
        ui->progressBar->setMaximum(0);
        ui->progressBar->setValue(0);
        retranslateTexts();
        QTimer::singleShot(800, qApp, &QApplication::quit);
    });
}

UpdateDialog::~UpdateDialog() {
    delete ui;
    qDebug() << "UpdateDialog destroyed";
}

void UpdateDialog::applyThemeStyle() {
    const auto& c = ThemeManager::current();
    setStyleSheet(QString("QDialog { background-color: %1; color: %2; }")
                  .arg(c.bgColor.name(), c.textColor.name()));
    ui->textBrowser->setStyleSheet(QString(
        "QTextBrowser {"
        "  font: 10pt \"Microsoft YaHei\";"
        "  padding: 5px; border-radius: 5px;"
        "  background-color: %1; color: %2;"
        "}"
    ).arg(c.updateBg.name(), c.updateText.name()));
    ui->label_newVer->setStyleSheet(QString("color: %1;").arg(c.textColor.name()));
    ui->label_ver->setStyleSheet(QString("color: %1;").arg(c.disabledText.name()));
    const QString btnStyle = QString(
        "QPushButton { padding: 4px 16px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; }"
        "QPushButton:hover { background-color: %4; }"
        "QPushButton:disabled { color: %5; }"
    ).arg(c.borderColor.name(), c.inputBg.name(), c.textColor.name(),
          c.highlightColor.name(), c.disabledText.name());
    ui->btn_recheck->setStyleSheet(btnStyle);
    ui->btn_update->setStyleSheet(btnStyle);
}

void UpdateDialog::fetchGithubReleaseInfo() {
    m_phase = Phase::Fetching;
    ui->progressBar->show();
    ui->progressBar->setMaximum(0);
    ui->progressBar->setValue(0);
    retranslateTexts();
    const QString ApiUrl = QString("https://api.github.com/repos/%1/%2/releases").arg(Owner, Repo);
    QNetworkRequest request(ApiUrl);
    auto* reply = manager.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Failed to fetch update info" << reply->errorString();
            qWarning() << "GitHub API status:"
                       << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                       << reply->readAll();
            ui->progressBar->hide();
            m_phase = Phase::FetchError;
            m_errorCache = reply->errorString();
            retranslateTexts();
            sysTray().showMessage(tr("Failed to fetch update info"), reply->errorString());
            return;
        }
        const auto data = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        m_cachedReleases = doc.array();

        QJsonObject bestObj;
        QVersionNumber bestVer;
        for (const auto& val : m_cachedReleases) {
            auto obj = val.toObject();
            if (!m_includePreRelease && obj["prerelease"].toBool())
                continue;
            auto ver = normalizeVersion(obj["tag_name"].toString());
            if (!bestObj.isEmpty() && ver <= bestVer)
                continue;
            bestObj = obj;
            bestVer = ver;
        }
        if (!bestObj.isEmpty()) {
            applyRelease(bestObj);
        } else {
            ui->progressBar->hide();
            m_phase = Phase::FetchError;
            m_errorCache = tr("No matching release found");
            retranslateTexts();
        }
    });
}

QJsonObject UpdateDialog::selectInstallerAsset(const QJsonArray& assets) const {
#ifndef APP_ARCH
#  error "APP_ARCH not defined — update CMakeLists.txt"
#endif
    QString archSuffix = QStringLiteral(APP_ARCH);
    QString archTag = QStringLiteral("-%1-Setup.exe").arg(archSuffix);

    // 1) 精确匹配: 文件名以 -{arch}-Setup.exe 结尾
    for (const auto& a : assets) {
        auto o = a.toObject();
        if (o["name"].toString().endsWith(archTag, Qt::CaseInsensitive))
            return o;
    }

    // 2) 容错: 任意 Setup.exe
    for (const auto& a : assets) {
        auto o = a.toObject();
        if (o["name"].toString().contains("Setup.exe", Qt::CaseInsensitive))
            return o;
    }

    // 3) 容错: 任意 .exe
    for (const auto& a : assets) {
        auto o = a.toObject();
        if (o["name"].toString().endsWith(".exe", Qt::CaseInsensitive))
            return o;
    }

    // 4) 最后手段: 第一个资产
    if (!assets.isEmpty())
        return assets.first().toObject();

    return {};
}

void UpdateDialog::applyRelease(const QJsonObject& obj) {
    relInfo.ver = normalizeVersion(obj["tag_name"].toString());
    relInfo.description = obj["body"].toString();
    relInfo.publishTime = toLocalTime(obj["published_at"].toString());

    auto asset = selectInstallerAsset(obj["assets"].toArray());
    relInfo.downloadUrl = asset["browser_download_url"].toString();

    qInfo() << "[Update] Selected asset:" << asset["name"].toString();
    qInfo() << "Update info applied" << relInfo.ver << relInfo.downloadUrl;
    ui->progressBar->hide();

    bool needUpdate = relInfo.ver > version;
    m_phase = needUpdate ? Phase::HasUpdate : Phase::UpToDate;
    retranslateTexts();
    ui->btn_update->setEnabled(needUpdate);
}

void UpdateDialog::download(const QString& url, const QString& savePath) {
    QNetworkRequest request(url);
    auto* reply = manager.get(request);
    ui->progressBar->show();
    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(0); // unknown size

    QFile::remove(savePath);
    downloadStatus.file.setFileName(savePath);
    downloadStatus.file.open(QIODevice::WriteOnly);
    downloadStatus.success = false;
    downloadStatus.reply = reply;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        downloadStatus.file.write(reply->readAll());
    });

    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal) {
        qDebug() << "Download progress" << bytesReceived << bytesTotal;
        if (bytesReceived == bytesTotal)
            downloadStatus.success = true;

        ui->progressBar->setMaximum(bytesTotal);
        ui->progressBar->setValue(bytesReceived);
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        downloadStatus.reply = nullptr;
        downloadStatus.file.close(); // close才刷新缓冲区，写入磁盘，所以必须在解压之前！不然解压概率性失败...(QZip: EndOfDirectory not found)
        ui->progressBar->hide();
        ui->btn_update->setEnabled(true);
        if (!downloadStatus.success || reply->error() != QNetworkReply::NoError) {
            qWarning() << "Download failed" << reply->errorString();
            m_phase = Phase::DownloadFailed;
            m_errorCache = reply->errorString();
            retranslateTexts();
            sysTray().showMessage(tr("Download failed"), reply->errorString());
        } else {
            m_phase = Phase::DownloadSucceed;
            retranslateTexts();
            sysTray().showMessage(tr("Download Status"), tr("Success!"));
            emit downloadSucceed(downloadStatus.file.fileName());
        }
    });
}

QVersionNumber UpdateDialog::normalizeVersion(const QString& ver) {
    auto v = ver;
    if (v.startsWith('v')) v.removeFirst();
    while (v.count('.') > 2 && v.endsWith(".0")) // 移除多余的0，保留3位
        v.chop(2);
    return QVersionNumber::fromString(v);
}

QString UpdateDialog::toLocalTime(const QString& isoTime) {
    const auto dateTime = QDateTime::fromString(isoTime, Qt::ISODate);
    return dateTime.toLocalTime().toString("yyyy.MM.dd HH:mm");
}

void UpdateDialog::verifyUpdate(const QCoreApplication& app) {
    QCommandLineParser parser;
    // 用于验证更新成功与否，应由更新程序(.bat)调用，用法：`--verify-update 1.0->2.0`
    QCommandLineOption versionUpdate("verify-update", "verify update, e.g. `1.0->2.0`", "versionChange");
    // the `valueName` needs to be set if the option expects a value.
    parser.addOption(versionUpdate);
    parser.process(app);
    if (parser.isSet(versionUpdate)) {
        // Qt 好像不支持`-v 1.0.1 1.0.2`这样多values，只能`-v 1.0.1 -v 1.0.2`
        auto versions = parser.value(versionUpdate).split("->");
        if (versions.size() == 2) {
            auto vFrom = QVersionNumber::fromString(versions.first());
            auto vTo = QVersionNumber::fromString(versions.last());
            qDebug() << "vFrom" << vFrom << "vTo" << vTo << "compare" << QVersionNumber::compare(vFrom, vTo);

            auto vCur = QVersionNumber::fromString(QCoreApplication::applicationVersion());
            if (vCur.normalized() == vTo.normalized()) {
                qInfo() << "Update success";
                sysTray().showMessage(tr("Verify Update"), tr("Update Success to v%1").arg(vTo.toString()));
            } else if (vCur.normalized() == vFrom.normalized()) {
                qWarning() << "Update failed, version not change" << vFrom << vTo << vCur;
                sysTray().showMessage(tr("Verify Update"), tr("Update failed, version not change"), QSystemTrayIcon::Critical);
            } else {
                qWarning() << "Update may failed, version changed but not to target" << vFrom << vTo << vCur;
                sysTray().showMessage(tr("Verify Update"), tr("Update may failed, version changed but not to target"), QSystemTrayIcon::Warning);
            }
        } else
            qWarning() << "Invalid version change" << parser.value(versionUpdate);
    }
}

void UpdateDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateTexts();
    QDialog::changeEvent(event);
}

void UpdateDialog::retranslateTexts() {
    setWindowTitle(tr("AltTaber Updater"));
    ui->btn_recheck->setText(tr(" Check again "));
    ui->btn_update->setText(tr("Update"));
    ui->ckPreRelease->setText(tr("Include pre-releases"));

    switch (m_phase) {
    case Phase::Initial:
        ui->textBrowser->setMarkdown(tr("## Fetching..."));
        break;
    case Phase::Fetching:
        ui->textBrowser->setMarkdown(tr("## Fetching update..."));
        break;
    case Phase::FetchError:
        ui->textBrowser->setMarkdown(tr("## Failed to fetch update info❎\n%1").arg(m_errorCache));
        break;
    case Phase::UpToDate:
        ui->label_newVer->setText(tr("New Version: v%1 (%2)").arg(relInfo.ver.toString(), relInfo.publishTime));
        ui->label_ver->setText(tr("Current Version: v%1").arg(version.toString()));
        ui->textBrowser->setMarkdown(tr("# ✅Everything is up-to-date"));
        break;
    case Phase::HasUpdate:
        ui->label_newVer->setText(tr("New Version: v%1 (%2)").arg(relInfo.ver.toString(), relInfo.publishTime));
        ui->label_ver->setText(tr("Current Version: v%1").arg(version.toString()));
        ui->textBrowser->setMarkdown(
            relInfo.description.isEmpty()
                ? tr("No release notes available.")
                : relInfo.description);
        break;
    case Phase::DownloadFailed:
        ui->textBrowser->setMarkdown(tr("## Download failed❎\n%1").arg(m_errorCache));
        break;
    case Phase::DownloadSucceed:
        ui->textBrowser->setMarkdown(tr("## Download success✅"));
        break;
    case Phase::Installing:
        ui->textBrowser->setMarkdown(tr("## Installing..."));
        break;
    }
}

void UpdateDialog::showEvent(QShowEvent*) {
    if (!downloadStatus.reply)
        fetchGithubReleaseInfo();
}
