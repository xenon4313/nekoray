#include "dialog_theme_select.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QPainter>
#include <QPainterPath>
#include <QFileInfo>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>

#include "main/NekoGui.hpp"

namespace {
class ClickableFrame : public QFrame {
public:
    explicit ClickableFrame(int id, QWidget *parent = nullptr)
        : QFrame(parent), themeId(id) {
        setCursor(Qt::PointingHandCursor);
    }

    std::function<void(int)> onClicked;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && onClicked) {
            onClicked(themeId);
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    int themeId;
};
}

ThemeSelectDialog::ThemeSelectDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Choosing a theme"));
    setModal(true);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFixedSize(780, 480);

    setStyleSheet(
        "QDialog {"
        "  background-color: #12151e;"
        "  color: #e4e7ec;"
        "  font-family: 'Segoe UI', -apple-system, sans-serif;"
        "}"
        "QLabel {"
        "  color: #e4e7ec;"
        "}"
        "QPushButton#closeBtn {"
        "  background: rgba(255, 255, 255, 20);"
        "  color: #f0f0f3;"
        "  border: 1px solid rgba(255, 255, 255, 40);"
        "  border-radius: 8px;"
        "  padding: 6px 20px;"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "}"
        "QPushButton#closeBtn:hover {"
        "  background: rgba(255, 255, 255, 35);"
        "}"
        "QPushButton#browseBtn {"
        "  background: #3b82f6;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 4px 10px;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "}"
        "QPushButton#browseBtn:hover {"
        "  background: #2563eb;"
        "}"
    );

    presets = {
        {1, tr("Taiga Aisaka"), tr("Original 1"), ":/neko/ver1.jpg", false},
        {2, tr("City Life"), tr("Original 2"), ":/neko/ver2.jpg", false},
        {3, tr("Kana Arima"), tr("Oshi no Ko"), ":/neko/ver3.jpg", false},
        {4, tr("Yuu Koito"), tr("Bloom Into You"), ":/neko/ver4.jpg", false},
        {0, tr("Custom"), tr(""), "", true}
    };

    setupUI();
    updateSelectionVisuals();
}

QPixmap ThemeSelectDialog::createRoundedThumbnail(const QString &imagePath, int w, int h, int radius) {
    QPixmap result(w, h);
    result.fill(Qt::transparent);

    QPixmap src(imagePath);
    if (src.isNull() && !imagePath.isEmpty()) {
        QFileInfo fi(imagePath);
        if (fi.exists()) src = QPixmap(fi.absoluteFilePath());
    }

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath clipPath;
    clipPath.addRoundedRect(0, 0, w, h, radius, radius);
    painter.setClipPath(clipPath);

    if (!src.isNull()) {
        // Aspect-fill (cover)
        double targetAspect = (double) w / (double) h;
        double bgAspect = (double) src.width() / (double) src.height();
        QRect srcRect;
        if (bgAspect > targetAspect) {
            int srcW = qRound(src.height() * targetAspect);
            int srcX = (src.width() - srcW) / 2;
            srcRect = QRect(srcX, 0, srcW, src.height());
        } else {
            int srcH = qRound(src.width() / targetAspect);
            int srcY = (src.height() - srcH) / 2;
            srcRect = QRect(0, srcY, src.width(), srcH);
        }
        painter.drawPixmap(QRect(0, 0, w, h), src, srcRect);
    } else {
        // Placeholder for custom theme without image yet
        painter.fillRect(QRect(0, 0, w, h), QColor(24, 28, 40));
        painter.setPen(QColor(120, 130, 150));
        QFont font = painter.font();
        font.setPixelSize(28);
        painter.setFont(font);
        painter.drawText(QRect(0, 0, w, h - 20), Qt::AlignCenter, "+");
        font.setPixelSize(11);
        painter.setFont(font);
        painter.drawText(QRect(0, h - 45, w, 30), Qt::AlignCenter, tr("Загрузить"));
    }

    return result;
}

void ThemeSelectDialog::setupUI() {
    auto *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(24, 20, 24, 20);
    mainLay->setSpacing(16);

    // Header
    auto *titleLabel = new QLabel(tr("Theme selection"), this);
    titleLabel->setStyleSheet("font-size: 19px; font-weight: 700; color: #ffffff;");
    auto *subLabel = new QLabel(tr("Select a preset theme or upload any custom image"), this);
    subLabel->setStyleSheet("font-size: 12px; color: #94a3b8;");

    mainLay->addWidget(titleLabel);
    mainLay->addWidget(subLabel);

    // Cards row
    auto *cardsRow = new QHBoxLayout();
    cardsRow->setSpacing(12);
    cardsRow->setContentsMargins(0, 8, 0, 8);

    for (const auto &p: presets) {
        auto *card = createCardWidget(p);
        cardWidgets.append(card);
        cardsRow->addWidget(card);
    }

    mainLay->addLayout(cardsRow);

    // Bottom info and Close button
    auto *bottomLay = new QHBoxLayout();
    auto *infoLabel = new QLabel(tr("Tip: Any aspect ratio is automatically adjusted, but 824x1280 is recommended"), this);
    infoLabel->setStyleSheet("font-size: 11px; color: #64748b;");

    auto *closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setObjectName("closeBtn");
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    bottomLay->addWidget(infoLabel, 1);
    bottomLay->addWidget(closeBtn, 0);

    mainLay->addLayout(bottomLay);
}

QFrame *ThemeSelectDialog::createCardWidget(const ThemePreset &preset) {
    auto *card = new ClickableFrame(preset.id, this);
    card->setObjectName(QStringLiteral("card_%1").arg(preset.id));
    card->setFixedSize(136, 280);

    card->onClicked = [this](int id) {
        onCardClicked(id);
    };

    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(5);
    lay->setAlignment(Qt::AlignTop);

    // Thumbnail
    int thumbW = 120;
    int thumbH = 175;
    auto *thumbLabel = new QLabel(card);
    thumbLabel->setFixedSize(thumbW, thumbH);
    thumbLabel->setAlignment(Qt::AlignCenter);

    QString imgPath = preset.path;
    if (preset.isCustom) {
        imgPath = NekoGui::dataStore->ui_simple_bg_custom;
        customThumbLabel = thumbLabel;
    }

    thumbLabel->setPixmap(createRoundedThumbnail(imgPath, thumbW, thumbH, 8));
    lay->addWidget(thumbLabel, 0, Qt::AlignCenter);

    // Title
    auto *title = new QLabel(preset.title, card);
    title->setStyleSheet("font-size: 13px; font-weight: 700; color: #f8fafc; margin-top: 2px;");
    title->setAlignment(Qt::AlignCenter);
    title->setWordWrap(true);
    lay->addWidget(title);

    // Subtitle / Browse button for custom
    if (preset.isCustom) {
        auto *browseBtn = new QPushButton(tr("Select..."), card);
        browseBtn->setObjectName("browseBtn");
        browseBtn->setCursor(Qt::PointingHandCursor);
        connect(browseBtn, &QPushButton::clicked, this, &ThemeSelectDialog::onBrowseCustomFile);
        lay->addWidget(browseBtn, 0, Qt::AlignCenter);
    } else {
        auto *sub = new QLabel(preset.subtitle, card);
        sub->setStyleSheet("font-size: 11px; color: #94a3b8;");
        sub->setAlignment(Qt::AlignCenter);
        lay->addWidget(sub);
    }

    // Active status badge
    auto *badge = new QLabel(card);
    badge->setObjectName("badge");
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedHeight(18);
    lay->addWidget(badge, 0, Qt::AlignCenter);

    return card;
}

void ThemeSelectDialog::updateSelectionVisuals() {
    int activeId = NekoGui::dataStore->ui_simple_bg;

    for (int i = 0; i < presets.size(); ++i) {
        const auto &p = presets[i];
        auto *card = cardWidgets[i];
        bool isActive = (p.id == activeId);

        auto *badge = card->findChild<QLabel *>("badge");
        if (badge) {
            if (isActive) {
                badge->setText(tr("✓ Active"));
                badge->setStyleSheet(
                    "background: rgba(59, 130, 246, 200);"
                    "color: #ffffff;"
                    "font-size: 10px;"
                    "font-weight: 700;"
                    "border-radius: 9px;"
                    "padding: 1px 8px;"
                );
            } else {
                badge->setText("");
                badge->setStyleSheet("background: transparent;");
            }
        }

        if (isActive) {
            card->setStyleSheet(
                QStringLiteral(
                    "QFrame#card_%1 {"
                    "  background: rgba(30, 48, 80, 210);"
                    "  border: 2px solid #3b82f6;"
                    "  border-radius: 12px;"
                    "}"
                    "QFrame#card_%1:hover {"
                    "  background: rgba(35, 58, 95, 230);"
                    "}"
                ).arg(p.id)
            );
        } else {
            card->setStyleSheet(
                QStringLiteral(
                    "QFrame#card_%1 {"
                    "  background: rgba(22, 26, 38, 180);"
                    "  border: 1px solid rgba(255, 255, 255, 28);"
                    "  border-radius: 12px;"
                    "}"
                    "QFrame#card_%1:hover {"
                    "  background: rgba(32, 38, 55, 210);"
                    "  border: 1px solid rgba(255, 255, 255, 60);"
                    "}"
                ).arg(p.id)
            );
        }
    }
}

void ThemeSelectDialog::onCardClicked(int id) {
    if (id == 0 && NekoGui::dataStore->ui_simple_bg_custom.isEmpty()) {
        onBrowseCustomFile();
        return;
    }

    NekoGui::dataStore->ui_simple_bg = id;
    NekoGui::dataStore->Save();
    updateSelectionVisuals();
    emit themeApplied();
}

void ThemeSelectDialog::onBrowseCustomFile() {
    QString filter = tr("Изображения (*.png *.jpg *.jpeg *.webp *.bmp);;Все файлы (*.*)");
    QString path = QFileDialog::getOpenFileName(this, tr("Select a background image"), QString(), filter);
    if (path.isEmpty()) return;

    NekoGui::dataStore->ui_simple_bg_custom = path;
    NekoGui::dataStore->ui_simple_bg = 0; // 0 = custom
    NekoGui::dataStore->Save();

    if (customThumbLabel) {
        customThumbLabel->setPixmap(createRoundedThumbnail(path, 120, 175, 8));
    }

    updateSelectionVisuals();
    emit themeApplied();
}
