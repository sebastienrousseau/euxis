#include <QApplication>
#include <euxis/etx/app.hpp>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Euxis ETX");
    QApplication::setOrganizationName("Euxis");
    QApplication::setApplicationVersion("0.0.3");

    euxis::etx::EuxisApp window;
    window.show();

    return app.exec();
}
