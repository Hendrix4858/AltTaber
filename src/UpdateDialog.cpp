// You may need to build the project (run Qt uic code generator) to get "ui_UpdateDialog.h" resolved

#include "UpdateDialog.h"
#include <QCommandLineParser>
#include "ui_UpdateDialog.h"
#include <QDebug>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QDir>
#include "utils/SystemTray.h"
#include "utils/ThemeManager.h"
#include <QProcess>
#include <QElapsedTimer>

UpdateDialog::UpdateDialog(QWidget* parent) : QDialog(parent), ui(new Ui::UpdateDialog) {
    QElapsedTimer t;
    t.start();
    ui->setupUi(this);
    {
        HWND h = (HWND)winId();
        auto ex = GetWindowLongW(h, GWL_EXSTYLE);
        SetWindowLongW(h, GWL_EXSTYLE, ex | WS_EX_NOACTIVATE);
    }
    setWindowTitle("AltTaber Updater[GitHub]");
    qDebug() << QSslSocket::sslLibraryBuildVersionString() << QSslSocket::supportsSsl();

    applyThemeStyle();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &UpdateDialog::applyThemeStyle);

    manager.setTransferTimeout(10000); // 10s -> Operation canceled
    ui->progressBar->hide();
    connect(ui->btn_recheck, &QPushButton::clicked, this, &UpdateDialog::fetchGithubReleaseInfo);
    connect(ui->btn_update, &QPushButton::clicked, this, [this] {
        ui->btn_update->setEnabled(false);
        auto url = relInfo.downloadUrl;
        archive.fileName = QUrl(url).fileName();
        download(url, qApp->applicationDirPath() + '/' + archive.fileName);
    });
    qInfo() << "UpdateDialog initialized in" << t.elapsed() << "ms";
    connect(this, &UpdateDialog::downloadSucceed, this, [this](const QString& filePath) {
        qInfo() << "Download succeed" << filePath;
        if (filePath.endsWith(".zip")) {
            auto relDir = QDir(qApp->applicationDirPath() + '/' + archive.extractDir);
            relDir.removeRecursively();
            QProcess expandProcess;
            expandProcess.start("powershell", QStringList()
                << "-Command"
                << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
                    .arg(filePath).arg(relDir.absolutePath()));
            expandProcess.waitForFinished();
            if (expandProcess.exitCode() == 0 && relDir.exists()) {
                auto entryInfos = relDir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
                if (entryInfos.size() == 1 && entryInfos.first().isDir()) {
                    relDir.cd(entryInfos.first().fileName());
                }
                qDebug() << "release extract dir:" << relDir.absolutePath();
                const auto batPath = writeBat(relDir.absolutePath());
                QDesktopServices::openUrl(QUrl::fromLocalFile(batPath));
                qApp->quit();
            } else {
                ui->textBrowser->setMarkdown("## Extract failed");
            }
        } else {
            // 其他格式可能需要7z.exe支持
            qWarning() << "Unsupported file type" << filePath;
            ui->textBrowser->setMarkdown("## Unsupported file type❎");
        }
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
    ui->textBrowser->setMarkdown("## Fetching...");
    const QString ApiUrl = QString("https://api.github.com/repos/%1/%2/releases/latest").arg(Owner, Repo);
    QNetworkRequest request(ApiUrl);
    auto* reply = manager.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Failed to fetch update info" << reply->errorString();
            ui->textBrowser->setMarkdown("## Failed to fetch update info❎\n" + reply->errorString());
            sysTray().showMessage("Failed to fetch update info", reply->errorString());
            return;
        }
        const auto data = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        const auto obj = doc.object();

        relInfo.ver = normalizeVersion(obj["tag_name"].toString());
        relInfo.description = obj["body"].toString();
        relInfo.publishTime = toLocalTime(obj["published_at"].toString());

        if (const auto assets = obj["assets"].toArray(); !assets.isEmpty())
            relInfo.downloadUrl = assets.first()["browser_download_url"].toString();

        qInfo() << "Update info fetched" << relInfo.ver << relInfo.downloadUrl;
        ui->label_newVer->setText(QString("New Version: v%1 (%2)").arg(relInfo.ver.toString(), relInfo.publishTime));
        ui->label_ver->setText(QString("Current Version: v%1").arg(version.toString()));

        bool needUpdate = relInfo.ver > version;
        // 如果要支持网络图片，必须重写`QTextBrowser::loadResource`，默认是作为本地文件处理
        ui->textBrowser->setMarkdown(needUpdate ? relInfo.description : "# ✅Everything is up-to-date");
        ui->btn_update->setEnabled(needUpdate);
    });
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
            ui->textBrowser->setMarkdown("## Download failed❎ [" + reply->url().host() + "]\n" + reply->errorString());
            sysTray().showMessage("Download failed", reply->errorString());
        } else {
            ui->textBrowser->setMarkdown("## Download success✅");
            sysTray().showMessage("Download Status", "Success!");
            emit downloadSucceed(downloadStatus.file.fileName());
        }
    });
}

QString UpdateDialog::writeBat(const QString& sourceDir, const QString& targetDir) const {
    QFile file(qApp->applicationDirPath() + "/copy.bat");
    if (file.open(QFile::WriteOnly | QFile::Text)) {
        QTextStream text(&file);
        text << "@timeout /t 1 /NOBREAK" << '\n';
        text << "@cd /d %~dp0" << '\n'; //切换到bat目录，否则为qt的exe目录
        text << "@echo ##Copying files, please wait......\n";
        text << QString("xcopy \"%1\" \"%2\" /E /H /Y\n").arg(QDir::toNativeSeparators(sourceDir), QDir::toNativeSeparators(targetDir));
        text << "@echo -------------------------------------------------------\n";
        text << "@echo ##Update SUCCESSFUL(Maybe)\n";
        text << "@echo -------------------------------------------------------\n";
        // text << "@pause\n";
        text << QString("@del \"%1\"\n").arg(archive.fileName);
        text << QString("@rd /S /Q \"%1\"\n").arg(archive.extractDir);
        text << QString("@start \"\" \"%1\" --verify-update \"%2->%3\"\n").arg(QFile(qApp->applicationFilePath()).fileName())
                                                                          .arg(version.toString(), relInfo.ver.toString());
        text << "@del %0";
    }
    return file.fileName();
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
                sysTray().showMessage("Verify Update", "Update Success to v" + vTo.toString());
            } else if (vCur.normalized() == vFrom.normalized()) {
                qWarning() << "Update failed, version not change" << vFrom << vTo << vCur;
                sysTray().showMessage("Verify Update", "Update failed, version not change", QSystemTrayIcon::Critical);
            } else {
                qWarning() << "Update may failed, version changed but not to target" << vFrom << vTo << vCur;
                sysTray().showMessage("Verify Update", "Update may failed, version changed but not to target", QSystemTrayIcon::Warning);
            }
        } else
            qWarning() << "Invalid version change" << parser.value(versionUpdate);
    }
}

void UpdateDialog::showEvent(QShowEvent*) {
    if (!downloadStatus.reply)
        fetchGithubReleaseInfo();
}
