#include <QApplication>
#include <gtest/gtest.h>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    // Isolate test settings from real user config
    QCoreApplication::setOrganizationName("EuxisTest");
    QCoreApplication::setApplicationName("ETXTest");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
