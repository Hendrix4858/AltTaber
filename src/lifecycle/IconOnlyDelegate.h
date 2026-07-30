#ifndef WIN_SWITCHER_ICONONLYDELEGATE_H
#define WIN_SWITCHER_ICONONLYDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QIcon>
#include <QColor>

/// Icon Only Mode for QListWidget
class IconOnlyDelegate : public QStyledItemDelegate {
    QColor m_selectedColor;
    QColor m_hoverColor;
    int m_cornerRadius;
    QFont m_badgeFont;

public:
    explicit IconOnlyDelegate(QObject* parent = nullptr,
                              QColor selectedColor = QColor(80, 80, 80, 200),
                              QColor hoverColor = QColor(50, 50, 50, 100),
                              int radius = 8)
        : QStyledItemDelegate(parent), m_selectedColor(selectedColor), m_hoverColor(hoverColor),
          m_cornerRadius(radius), m_badgeFont(QStringLiteral("Microsoft YaHei")) {
        m_badgeFont.setPointSizeF(12.8);
        m_badgeFont.setBold(true);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};


#endif //WIN_SWITCHER_ICONONLYDELEGATE_H
