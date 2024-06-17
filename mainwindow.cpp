// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QtGui/QIntValidator>
#include <chrono>
#include <QDateTime>

QString now()
{
    QDateTime date = QDateTime::currentDateTime();
    QString formattedTime = date.toString("dd.MM.yyyy hh:mm:ss");
    return formattedTime;
}

void MainWindow::log(QString message)
{
    ui->plainTextEdit_logs->appendPlainText(now()+" "+message);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //Init config groupbox
    ui->lineEdit_client_port->setValidator( new QIntValidator(1, 65535, this) );
    ui->lineEdit_client_port->setText( QString::number(client_port) );
    ui->lineEdit_server_port->setValidator( new QIntValidator(1, 65535, this) );
    ui->lineEdit_server_port->setText( QString::number(server_port) );
    ui->plainTextEdit_logs->setReadOnly(true);
    this->update_ui();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    int width = e->size().width();
    int margin;
    QSize size;

    margin = ui->groupBox_server_config->geometry().x();
    size = ui->groupBox_server_config->size();
    size.setWidth(width - 2*margin);
    ui->groupBox_server_config->resize(size);

    margin = ui->plainTextEdit_logs->geometry().x();
    size = ui->plainTextEdit_logs->size();
    size.setWidth(width - 2*margin);

    int height = e->size().height();
    margin = ui->plainTextEdit_logs->geometry().y();
    //Height of status bar is about 20 by default
    size.setHeight(height - margin - 40);
    ui->plainTextEdit_logs->resize(size);
}

void MainWindow::update_ui()
{
    ui->lineEdit_client_port->setDisabled(this->server_started);
    ui->lineEdit_server_port->setDisabled(this->server_started);
    ui->pushButton_start->setDisabled(this->server_started);
    ui->pushButton_stop->setEnabled(this->server_started);

}

void MainWindow::on_pushButton_start_released()
{
    this->log("Starting server");
    broker = new Broker(server_port,
                        client_port,
                        this);
    connect(broker, &Broker::log, this, &MainWindow::log, Qt::QueuedConnection);
    this->server_started = true;
    this->update_ui();
}


void MainWindow::on_pushButton_stop_released()
{
    this->log("Stopping server");
    delete broker;
    this->server_started = false;
    this->update_ui();
}


void MainWindow::on_lineEdit_client_port_textChanged(const QString &arg1)
{
    client_port = arg1.toInt();
}


void MainWindow::on_lineEdit_server_port_textChanged(const QString &arg1)
{
    server_port = arg1.toInt();
}

