// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QtGui/QIntValidator>
#include <chrono>
#include <QDateTime>
#include <QDebug>

QString now()
{
    QDateTime date = QDateTime::currentDateTime();
    QString formattedTime = date.toString("dd.MM.yyyy hh:mm:ss");
    return formattedTime;
}

//==========OnOffLabel===========

OnOffLabel::OnOffLabel(QString img_on_path,
                       QString img_off_path,
                       QString tooltip_on,
                       QString tooltip_off,
                       int w,
                       int h,
                       QLabel *parent)
    : QLabel(parent)
    , img_on_path (img_on_path)
    , img_off_path(img_off_path)
    , tooltip_on(tooltip_on)
    , tooltip_off(tooltip_off)
    , w(w)
    , h(h)
{
    pix_on ->load(img_on_path);
    pix_off->load(img_off_path);
}

void OnOffLabel::setOn(bool b)
{
    QSharedPointer<QPixmap> p;
    QString t;
    if (b)
    {
        p = pix_on;
        t = tooltip_on;
    }
    else
    {
        p = pix_off;
        t = tooltip_off;
    }
    setPixmap(p->scaled(w,h,Qt::KeepAspectRatio));
    setToolTip(t);
}

//==========OnOffLabel===========
//==========MainWindow===========

void MainWindow::log(QString message)
{
    ui->plainTextEdit_logs->appendPlainText(now()+" "+message);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->horizontalLayout_4->setAlignment(Qt::AlignBottom);
    //Init config groupbox
    ui->lineEdit_client_port->setValidator( new QIntValidator(1, 65535, this) );
    ui->lineEdit_client_port->setText( QString::number(client_port) );
    ui->lineEdit_server_port->setValidator( new QIntValidator(1, 65535, this) );
    ui->lineEdit_server_port->setText( QString::number(server_port) );
    ui->lineEdit_server_password->setText(server_password);
    ui->plainTextEdit_logs->setReadOnly(true);
    statusBar()->addWidget(broker_status_label.get());
    statusBar()->addWidget(server_status_label.get());
    statusBar()->addWidget(client_status_label.get());
    server_status_label->setOff(true);
    client_status_label->setOff(true);

    keepalive_enabled = ui->checkBox_enable_keepalives->isChecked();

    this->update_ui();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::update_ui()
{
    ui->lineEdit_client_port->setDisabled(server_started);
    ui->lineEdit_server_port->setDisabled(server_started);
    ui->pushButton_start->setDisabled(server_started);
    ui->pushButton_stop->setEnabled(server_started);
    ui->lineEdit_server_password->setDisabled(server_started);
    broker_status_label->setOn(server_started);
}

void MainWindow::started()
{
    this->server_started = true;
    this->update_ui();
    connect(broker, &Broker::log, this, &MainWindow::log, Qt::QueuedConnection);
    connect(broker,   &Broker::client_connected,
            this, &MainWindow::client_connected);
    connect(broker,   &Broker::client_disconnected,
            this, &MainWindow::client_disconnected);
    connect(broker,   &Broker::server_connected,
            this, &MainWindow::server_connected);
    connect(broker,   &Broker::server_disconnected,
            this, &MainWindow::server_disconnected);
    this->log("Broker started");
}

void MainWindow::stopped()
{
    this->server_started = false;
    this->log("Broker stopped");
}

void MainWindow::on_pushButton_start_released()
{
    this->log("Starting broker");
    broker_thread.start();
    broker = new Broker(server_port,
                        client_port,
                        server_password);
    connect(&broker_thread, &QThread::finished, broker, [this](){ this->broker->deleteLater(); });
    broker->moveToThread(&broker_thread);
    emit broker->enableKeepalive(keepalive_enabled);
    emit broker->start();
    connect(broker, &Broker::started, this, &MainWindow::started, Qt::QueuedConnection);
    connect(broker, &Broker::stopped, this, &MainWindow::stopped, Qt::QueuedConnection);
}

void MainWindow::on_pushButton_stop_released()
{
    this->log("Stopping broker");
    broker_thread.exit();
    broker_thread.wait();
    broker = nullptr;
    this->server_started = false;
    this->update_ui();
    //Signals could be lost, set status manually
    client_status_label->setOff(true);
    server_status_label->setOff(true);
}


void MainWindow::on_lineEdit_client_port_textChanged(const QString &arg1)
{
    client_port = arg1.toInt();
}


void MainWindow::on_lineEdit_server_port_textChanged(const QString &arg1)
{
    server_port = arg1.toInt();
}

void MainWindow::server_connected(QString message)
{
    server_status_label->setOn(true);
}

void MainWindow::server_disconnected(QString message)
{
    server_status_label->setOff(true);
}

void MainWindow::client_connected(QString message)
{
    client_status_label->setOn(true);
}

void MainWindow::client_disconnected(QString message)
{
    client_status_label->setOff(true);
}

void MainWindow::on_lineEdit_server_password_textChanged(const QString &arg1)
{
    server_password = arg1;
}


void MainWindow::on_checkBox_enable_keepalives_stateChanged(int arg1)
{
    keepalive_enabled = arg1;
    if (server_started)
        broker->enableKeepalive(keepalive_enabled);
}

