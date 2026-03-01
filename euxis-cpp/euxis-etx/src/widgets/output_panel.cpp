#include <QPlainTextEdit>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QFont>
#include <QString>
#include <QColor>

namespace euxis::etx {

class OutputPanelWidget : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit OutputPanelWidget(QWidget* parent = nullptr)
        : QPlainTextEdit(parent)
    {
        setObjectName("output_panel");
        setReadOnly(true);

        QFont mono_font;
        mono_font.setFamily("Monospace");
        mono_font.setStyleHint(QFont::Monospace);
        mono_font.setPointSize(11);
        setFont(mono_font);

        setStyleSheet(
            "QPlainTextEdit { background: rgba(0,0,0,0.3); "
            "border: 1px solid rgba(255,255,255,0.08); border-radius: 6px; "
            "padding: 8px; color: #e0e0e0; }"
            "QScrollBar:vertical { background: rgba(255,255,255,0.02); width: 8px; }"
            "QScrollBar::handle:vertical { background: rgba(255,255,255,0.1); "
            "border-radius: 4px; min-height: 30px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
    }

    void write_line(const QString& line) {
        QTextCharFormat fmt;

        if (line.contains("ERROR") || line.contains("FAIL") ||
            line.contains(QString::fromUtf8("\xe2\x9c\x97"))) {
            fmt.setForeground(QColor("#f44336"));
        } else if (line.contains("WARNING") || line.contains("WARN")) {
            fmt.setForeground(QColor("#ffab00"));
        } else if (line.contains("PASS") ||
                   line.contains(QString::fromUtf8("\xe2\x9c\x93"))) {
            fmt.setForeground(QColor("#4caf50"));
        } else if (line.startsWith("#")) {
            fmt.setFontWeight(QFont::Bold);
            fmt.setForeground(QColor("#e0e0e0"));
        } else {
            fmt.setForeground(QColor("#c0c0c0"));
        }

        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(line + "\n", fmt);
        setTextCursor(cursor);
        ensureCursorVisible();
    }

    void write_status(const QString& message, const QColor& color) {
        QTextCharFormat fmt;
        fmt.setForeground(color);
        fmt.setFontWeight(QFont::Bold);

        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(message + "\n", fmt);
        setTextCursor(cursor);
        ensureCursorVisible();
    }

    void write_separator() {
        QTextCharFormat fmt;
        fmt.setForeground(QColor("#555555"));

        QString separator = QString(QChar(0x2500)).repeated(40);

        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(separator + "\n", fmt);
        setTextCursor(cursor);
        ensureCursorVisible();
    }
};

QWidget* create_output_panel_widget(QWidget* parent) {
    return new OutputPanelWidget(parent);
}

} // namespace euxis::etx

#include "output_panel.moc"
