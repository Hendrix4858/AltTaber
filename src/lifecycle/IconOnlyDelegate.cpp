#include "lifecycle/IconOnlyDelegate.h"
#include "WindowTypes.h"
#include "core/ThemeManager.h"

void IconOnlyDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const auto& colors = ThemeManager::current();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    if (option.state & QStyle::State_Selected) {
        painter->setBrush(colors.delegateSelected);
        painter->drawRoundedRect(option.rect, radius, radius);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setBrush(colors.delegateHover);
        painter->drawRoundedRect(option.rect, radius, radius);
    }

    auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    if (!icon.isNull()) {
        QRect iconRect{{}, option.decorationSize};
        iconRect.moveCenter(option.rect.center());
        icon.paint(painter, iconRect);
    }

    auto num = qvariant_cast<WindowGroup>(index.data(Qt::UserRole)).windows.size();
    if (num > 1) {
        auto text = QString::number(num);
        const auto extraWidth = 8 * (text.size() - 1);
        constexpr auto R = 12;
        auto badgeCenter = option.rect.topRight() + QPoint(-(R + 3), R + 3);
        auto badgeRect = QRect(badgeCenter + QPoint(-R - extraWidth, -R), QSize(2 * R + extraWidth, 2 * R));
        painter->setPen(Qt::NoPen);
        painter->setBrush(colors.badgeBg);
        painter->drawRoundedRect(badgeRect, R, R);

        QFont font{"Microsoft YaHei"};
        font.setPointSizeF(12.8);
        font.setBold(true);
        painter->setFont(font);
        painter->setPen(colors.badgeText);
        painter->drawText(badgeRect, Qt::AlignCenter, text);
    }
}
