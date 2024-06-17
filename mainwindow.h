#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "broker.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_start_released();

    void on_pushButton_stop_released();
    void update_ui();
    void log(QString message);

    void on_lineEdit_client_port_textChanged(const QString &arg1);

    void on_lineEdit_server_port_textChanged(const QString &arg1);

private:
    Ui::MainWindow *ui;
    void resizeEvent(QResizeEvent *e);
    bool server_started = false;
    int server_port = 33314;
    int client_port = 33315;
    Broker *broker = nullptr;
};
#endif // MAINWINDOW_H
