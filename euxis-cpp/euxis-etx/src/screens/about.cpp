#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QFrame>
#include <QWidget>
#include <QStackedWidget>
#include <QSysInfo>

namespace euxis::etx {

QWidget* create_about_screen(QWidget* parent) {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    // Back button
    auto* top_bar = new QHBoxLayout();
    auto* back_btn = new QPushButton("< Back", widget);
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setFixedWidth(100);
    top_bar->addWidget(back_btn);
    top_bar->addStretch();
    layout->addLayout(top_bar);

    QObject::connect(back_btn, &QPushButton::clicked, widget, [widget]() {
        if (auto* stack = qobject_cast<QStackedWidget*>(widget->parentWidget())) {
            stack->setCurrentIndex(1);
        }
    });

    // Logo
    auto* logo = new QLabel("E", widget);
    QFont logo_font;
    logo_font.setPointSize(72);
    logo_font.setBold(true);
    logo_font.setFamily("monospace");
    logo->setFont(logo_font);
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet("color: #ffffff; background: transparent;");
    layout->addWidget(logo);

    // Title
    auto* title = new QLabel("Euxis ETX", widget);
    QFont title_font;
    title_font.setPointSize(24);
    title_font.setBold(true);
    title->setFont(title_font);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // Subtitle
    auto* subtitle = new QLabel("Production-grade Agentic Platform", widget);
    QFont sub_font;
    sub_font.setPointSize(14);
    subtitle->setFont(sub_font);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("color: #888;");
    layout->addWidget(subtitle);

    layout->addSpacing(24);

    // Info card
    auto* card = new QFrame(widget);
    card->setObjectName("about_card");
    card->setFrameShape(QFrame::StyledPanel);
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setSpacing(10);

    auto add_info_row = [&](const QString& label, const QString& value) {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(label, card);
        lbl->setStyleSheet("color: #888; font-weight: bold;");
        lbl->setFixedWidth(180);
        row->addWidget(lbl);

        auto* val = new QLabel(value, card);
        val->setStyleSheet("color: #ccc;");
        row->addWidget(val);
        row->addStretch();
        card_layout->addLayout(row);
    };

    add_info_row("Version:", "0.0.3");
    add_info_row("Qt Version:", qVersion());
    add_info_row("Compiler:", QString("%1 %2").arg(
#if defined(__clang__)
        "Clang", __clang_version__
#elif defined(__GNUC__)
        "GCC", __VERSION__
#elif defined(_MSC_VER)
        "MSVC", QString::number(_MSC_VER)
#else
        "Unknown", "Unknown"
#endif
    ));
    add_info_row("Platform:", QSysInfo::prettyProductName());
    add_info_row("Architecture:", QSysInfo::currentCpuArchitecture());
    add_info_row("Kernel:", QSysInfo::kernelType() + " " + QSysInfo::kernelVersion());
    add_info_row("Packages:", "18+ (Python/Rust/TypeScript/C++)");
    add_info_row("Agents:", "42 agents, 8 squads");
    add_info_row("Security:", "WASM sandboxing, AES-256-GCM encryption");

    layout->addWidget(card);
    layout->addStretch();

    return widget;
}

} // namespace euxis::etx
