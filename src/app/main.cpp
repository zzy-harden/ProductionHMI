#include <QApplication>
#include <QLabel>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Production HMI");
    auto *label = new QLabel("Production HMI - Project Skeleton", &window);
    label->setMargin(20);
    window.show();

    return app.exec();
}
