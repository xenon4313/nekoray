#pragma once

#include <QDialog>
#include <QList>
#include <QPixmap>

class QPushButton;
class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class QFrame;

struct ThemePreset {
    int id; // 1..4 for presets, 0 for custom
    QString title;
    QString subtitle;
    QString path;
    bool isCustom;
};

class ThemeSelectDialog : public QDialog {
    Q_OBJECT

public:
    explicit ThemeSelectDialog(QWidget *parent = nullptr);

signals:
    void themeApplied();

private slots:
    void onCardClicked(int id);
    void onBrowseCustomFile();

private:
    void setupUI();
    void updateSelectionVisuals();
    QFrame *createCardWidget(const ThemePreset &preset);
    QPixmap createRoundedThumbnail(const QString &imagePath, int w, int h, int radius);

    QList<QFrame *> cardWidgets;
    QList<ThemePreset> presets;
    QLabel *customThumbLabel = nullptr;
    QLabel *customPathLabel = nullptr;
};
