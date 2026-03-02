#include <euxis/etx/config.hpp>

#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QWidget>
#include <QTimer>

namespace euxis::etx {

class TipBarWidget : public QWidget {
    Q_OBJECT

public:
    explicit TipBarWidget(ETXConfig* config, QWidget* parent = nullptr)
        : QWidget(parent)
        , config_(config)
        , current_index_(0)
    {
        setObjectName("tip_bar");
        setFixedHeight(28);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 0, 16, 0);

        // Tip icon
        auto* icon = new QLabel("*", this);
        icon->setFixedWidth(16);
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet("color: #ffab00; background: transparent; border: none; "
                            "font-weight: bold;");
        layout->addWidget(icon);

        // Tip text
        tip_label_ = new QLabel(this);
        tip_label_->setObjectName("tip_label");
        tip_label_->setStyleSheet(
            "color: #777; background: transparent; border: none; font-size: 11px;");
        layout->addWidget(tip_label_);
        layout->addStretch();

        tips_ = {
            tr("Press F3 to cycle themes"),
            tr("Use Ctrl+K to open the command palette"),
            tr("Press F5 to refresh the fleet registry"),
            tr("Ctrl+Q quits the application"),
            tr("Click on any agent card to see details"),
            tr("Use the search bar to filter agents"),
            tr("Press / to quickly open the command palette"),
        };

        update_tip();

        // Rotate tips every 8 seconds
        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &TipBarWidget::rotate_tip);
        timer->start(8000);
    }

private slots:
    void rotate_tip() {
        current_index_ = (current_index_ + 1) % tips_.size();
        update_tip();
    }

private:
    void update_tip() {
        if (!config_->show_tips()) {
            setVisible(false);
            return;
        }
        setVisible(true);
        tip_label_->setText(tips_[current_index_]);
    }

    ETXConfig* config_;
    QLabel* tip_label_;
    QStringList tips_;
    int current_index_;
};

QWidget* create_tip_bar_widget(ETXConfig* config, QWidget* parent) {
    return new TipBarWidget(config, parent);
}

} // namespace euxis::etx

#include "tip_bar.moc"
