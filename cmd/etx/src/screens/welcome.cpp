#include <euxis/etx/app.hpp>

#include <QVBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QFont>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>

namespace euxis::etx {

class WelcomeScreen : public QWidget {
    Q_OBJECT

public:
    explicit WelcomeScreen(EuxisApp* app, QWidget* parent = nullptr)
        : QWidget(parent), app_(app)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setAlignment(Qt::AlignCenter);
        layout->setSpacing(24);

        // Logo "E"
        auto* logo = new QLabel("E", this);
        logo->setObjectName("welcome_logo");
        QFont logo_font;
        logo_font.setPointSize(120);
        logo_font.setBold(true);
        logo_font.setFamily("monospace");
        logo->setFont(logo_font);
        logo->setAlignment(Qt::AlignCenter);
        logo->setStyleSheet("color: #ffffff; background: transparent;");
        layout->addWidget(logo);

        // Title
        auto* title = new QLabel(tr("Welcome to Euxis ETX"), this);
        title->setObjectName("welcome_title");
        QFont title_font;
        title_font.setPointSize(28);
        title_font.setBold(true);
        title->setFont(title_font);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("color: #e0e0e0; background: transparent;");
        layout->addWidget(title);

        // Subtitle
        auto* subtitle = new QLabel(tr("Production-grade Agentic Platform"), this);
        subtitle->setObjectName("welcome_subtitle");
        QFont sub_font;
        sub_font.setPointSize(14);
        subtitle->setFont(sub_font);
        subtitle->setAlignment(Qt::AlignCenter);
        subtitle->setStyleSheet("color: #888888; background: transparent;");
        layout->addWidget(subtitle);

        // Spacer
        layout->addSpacing(40);

        // Continue hint
        auto* hint = new QLabel(tr("Press Enter to continue"), this);
        hint->setObjectName("welcome_hint");
        QFont hint_font;
        hint_font.setPointSize(12);
        hint_font.setItalic(true);
        hint->setFont(hint_font);
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet("color: #666666; background: transparent;");
        layout->addWidget(hint);

        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            app_->show_dashboard();
        } else {
            QWidget::keyPressEvent(event);
        }
    }

private:
    EuxisApp* app_;
};

QWidget* create_welcome_screen(EuxisApp* app, QWidget* parent) {
    return new WelcomeScreen(app, parent);
}

} // namespace euxis::etx

#include "welcome.moc"
