#include <QApplication>
#include "installer_window.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    opendriver::installer::InstallerWindow window;
    window.show();
    
    return app.exec();
}
