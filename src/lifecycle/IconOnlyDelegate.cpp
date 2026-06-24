#include "lifecycle/IconOnlyDelegate.h"
#include "WindowTypes.h"
#include "core/ThemeManager.h"
#include <QDebug>
#include <QStyle>

void IconOnlyDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const auto& colors = ThemeManager::current();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setPen(Qt::NoPen);
    if (option.state & QStyle::State_Selected) {
        painter->setBrush(colors.delegateSelected);
        painter->drawRoundedRect(option.rect, m_cornerRadius, m_cornerRadius);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setBrush(colors.delegateHover);
        painter->drawRoundedRect(option.rect, m_cornerRadius, m_cornerRadius);
    }

    auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    if (!icon.isNull()) {
        QRect aligned = QStyle::alignedRect(
            option.direction,
            Qt::AlignCenter,
            option.decorationSize,
            option.rect
        );
        QPixmap pixmap = icon.pixmap(option.decorationSize);

        if (pixmap.devicePixelRatio() != 1.0) {
            pixmap = QPixmap::fromImage(pixmap.toImage());
        }

        if (pixmap.size() != option.decorationSize) {
            pixmap = pixmap.scaled(
                option.decorationSize,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            );
        }

        pixmap.setDevicePixelRatio(1.0);

        painter->drawPixmap(aligned.topLeft(), pixmap);
    }

    auto windowCount = qvariant_cast<WindowGroup>(index.data(Qt::UserRole)).windows.size();
    if (windowCount > 1) {
        auto text = QString::number(windowCount);
        const auto badgeTextPadding = 8 * (text.size() - 1);
        constexpr auto badgeRadius = 12;
        auto badgeCenter = option.rect.topRight() + QPoint(-(badgeRadius + 3), badgeRadius + 3);
        auto badgeRect = QRect(badgeCenter + QPoint(-badgeRadius - badgeTextPadding, -badgeRadius), QSize(2 * badgeRadius + badgeTextPadding, 2 * badgeRadius));
        painter->setPen(Qt::NoPen);
        painter->setBrush(colors.badgeBg);
        painter->drawRoundedRect(badgeRect, badgeRadius, badgeRadius);

        QFont font{"Microsoft YaHei"};
        font.setPointSizeF(12.8);
        font.setBold(true);
        painter->setFont(font);
        painter->setPen(colors.badgeText);
        painter->drawText(badgeRect, Qt::AlignCenter, text);
    }
}

QSize IconOnlyDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(80, 80);
}
