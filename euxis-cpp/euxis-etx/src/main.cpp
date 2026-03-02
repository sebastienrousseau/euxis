#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <euxis/etx/app.hpp>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Euxis ETX");
    QApplication::setOrganizationName("Euxis");
    QApplication::setApplicationVersion("0.0.3");

    // Load translations for system locale
    QTranslator translator;
    auto locale = QLocale::system().name();  // e.g. "fr_FR", "de_DE"
    if (translator.load("euxis_etx_" + locale, ":/translations")) {
        QApplication::installTranslator(&translator);
    } else {
        // Try language code only (e.g. "fr" from "fr_FR")
        auto lang = locale.left(2);
        if (translator.load("euxis_etx_" + lang, ":/translations")) {
            QApplication::installTranslator(&translator);
        }
    }

    euxis::etx::EuxisApp window;
    window.show();

    return app.exec();
}
