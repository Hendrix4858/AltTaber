#ifndef WINDOWGROUPMODEL_H
#define WINDOWGROUPMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QVariant>

struct WindowGroup;

class WindowGroupModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit WindowGroupModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void setGroups(const QList<WindowGroup>& groups);
    const WindowGroup& groupAt(int row) const;
    int groupCount() const { return m_groups.size(); }
    const QList<WindowGroup>& groups() const { return m_groups; }

private:
    QList<WindowGroup> m_groups;
};

#endif //WINDOWGROUPMODEL_H
