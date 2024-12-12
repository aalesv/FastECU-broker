#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QRandomGenerator>
#include "broker.h"
#include <QThread>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

//Custom label to put in status bar
class OnOffLabel : public QLabel
{
    Q_OBJECT
public:
    OnOffLabel(QString img_on_path,
               QString img_off_path,
               QString tooltip_on = "",
               QString tooltip_off = "",
               //width
               int w = 20,
               //height
               int h = 20,
               QLabel *parent=nullptr);
    ~OnOffLabel(){}

    void setOn(bool b);
    void setOff(bool b) {setOn(!b);}

private:
    int w; //width
    int h; //height
    QString tooltip_on;  //tooltip for 'on' state
    QString tooltip_off; //tooltip for 'off' state
    QString img_on_path; //image for 'on' state
    QString img_off_path;//image for 'off' state
    //smart pointers will be deleted automatically on destroy
    QSharedPointer<QPixmap> pix_on = QSharedPointer<QPixmap>(new QPixmap);
    QSharedPointer<QPixmap> pix_off = QSharedPointer<QPixmap>(new QPixmap);
};

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
    void server_connected(QString message);
    void server_disconnected(QString message);
    void client_connected(QString message);
    void client_disconnected(QString message);
    void started();
    void stopped();

    void on_lineEdit_server_password_textChanged(const QString &arg1);

    void on_checkBox_enable_keepalives_stateChanged(int arg1);

private:
    Ui::MainWindow *ui;
    bool server_started = false;
    int server_port = 33314;
    int client_port = 33315;
    bool keepalive_enabled = false;
    QThread broker_thread;
    QString server_password =
        QString::number(QRandomGenerator::global()->bounded(1111, 9999));
    Broker *broker = nullptr;
    QSharedPointer<OnOffLabel> broker_status_label =
        QSharedPointer<OnOffLabel>(
            new OnOffLabel("icons/broker_status_on.png",
                           "icons/broker_status_off.png",
                           "Broker started",
                           "Broker stopped"
            )
        );
    QSharedPointer<OnOffLabel> server_status_label =
        QSharedPointer<OnOffLabel>(
            new OnOffLabel("icons/server_status_connected.png",
                           "icons/server_status_disconnected.png",
                           "Server peer connected",
                           "Server peer disconnected"
                           )
            );
    QSharedPointer<OnOffLabel> client_status_label =
        QSharedPointer<OnOffLabel>(
            new OnOffLabel("icons/client_status_connected.png",
                           "icons/client_status_disconnected.png",
                           "Client peer connected",
                           "Client peer disconnected"
                           )
            );
};
#endif // MAINWINDOW_H
