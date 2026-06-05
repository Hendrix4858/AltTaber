#include "WindowGroupModel.h"
#include "widget.h"

WindowGroupModel::WindowGroupModel(QObject* parent)
    : QAbstractListModel(parent) {}

int WindowGroupModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_groups.size();
}

QVariant WindowGroupModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_groups.size())
        return {};

    const auto& group = m_groups.at(index.row());
    switch (role) {
    case Qt::DecorationRole:
        return group.icon;
    case Qt::UserRole:
        return QVariant::fromValue(group);
    default:
        return {};
    }
}

void WindowGroupModel::setGroups(const QList<WindowGroup>& groups) {
    beginResetModel();
    m_groups = groups;
    endResetModel();
}

const WindowGroup& WindowGroupModel::groupAt(int row) const {
    return m_groups.at(row);
}
