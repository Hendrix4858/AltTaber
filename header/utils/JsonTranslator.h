#ifndef WIN_SWITCHER_JSONTRANSLATOR_H
#define WIN_SWITCHER_JSONTRANSLATOR_H

#include <QTranslator>
#include <QHash>
#include <QString>

class JsonTranslator : public QTranslator {
public:
    bool load(const QString& filePath);
    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override;

private:
    QHash<QString, QHash<QString, QString>> m_data;
};

#endif
