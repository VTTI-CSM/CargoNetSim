#include "RabbitMQConfigDialog.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("CargoNetSim");
    app.setOrganizationName("CargoNetSim");

    RabbitMQConfigDialog dialog;
    dialog.show();

    return app.exec();
}
