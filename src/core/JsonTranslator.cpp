#include "core/JsonTranslator.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

bool JsonTranslator::load(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "JsonTranslator: cannot open" << filePath;
        return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JsonTranslator: parse error" << err.errorString();
        return false;
    }

    m_data.clear();
    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        const QString& context = it.key();
        QJsonObject ctxObj = it.value().toObject();
        QHash<QString, QString> map;
        for (auto cit = ctxObj.begin(); cit != ctxObj.end(); ++cit) {
            map[cit.key()] = cit.value().toString();
        }
        m_data[context] = map;
    }

    qDebug().noquote() << "JsonTranslator: loaded" << m_data.size() << "contexts";
    return true;
}

QString JsonTranslator::translate(const char *context, const char *sourceText,
                                   const char *disambiguation, int n) const {
    Q_UNUSED(disambiguation)
    Q_UNUSED(n)

    if (!context || !sourceText)
        return QString();

    auto ctxIt = m_data.find(QLatin1String(context));
    if (ctxIt == m_data.end())
        return QString();

    auto srcIt = ctxIt->find(QLatin1String(sourceText));
    if (srcIt == ctxIt->end())
        return QString();

    return srcIt.value();
}
