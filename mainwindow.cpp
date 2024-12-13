// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QtGui/QIntValidator>
#include <chrono>
#include <QDateTime>
#include <QDebug>
#include <QSettings>

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
                       QWidget *parent)
    : OnOffLabel(img_on_path,
                 img_off_path,
                 tooltip_on,
                 tooltip_off,
                 20,
                 20,
                 parent)
{}

OnOffLabel::OnOffLabel(QString img_on_path,
                       QString img_off_path,
                       QString tooltip_on,
                       QString tooltip_off,
                       int w,
                       int h,
                       QWidget *parent)
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
    , config_file_watcher(new QFileSystemWatcher(this))
    , broker_status_label(new OnOffLabel("icons/broker_status_on.png",
                                         "icons/broker_status_off.png",
                                         "Broker started",
                                         "Broker stopped",
                                         this)
                          )
    , server_status_label(new OnOffLabel("icons/server_status_connected.png",
                                         "icons/server_status_disconnected.png",
                                         "Server peer connected",
                                         "Server peer disconnected",
                                         this)
                          )
    , client_status_label(new OnOffLabel("icons/client_status_connected.png",
                                         "icons/client_status_disconnected.png",
                                         "Client peer connected",
                                         "Client peer disconnected",
                                         this)
                          )
{
    read_config();
    setup_config_file_watcher();
    ui->setupUi(this);
    ui->horizontalLayout_4->setAlignment(Qt::AlignBottom);
    //Init config groupbox
    ui->lineEdit_client_port->setValidator( new QIntValidator(1, 65535, this) );
    ui->lineEdit_client_port->setText( QString::number(client_port) );
    ui->lineEdit_server_port->setValidator( new QIntValidator(1, 65535, this) );
    ui->lineEdit_server_port->setText( QString::number(server_port) );
    ui->lineEdit_server_password->setText(server_password);
    ui->plainTextEdit_logs->setReadOnly(true);
    statusBar()->addWidget(broker_status_label);
    statusBar()->addWidget(server_status_label);
    statusBar()->addWidget(client_status_label);
    server_status_label->setOff(true);
    client_status_label->setOff(true);

    this->update_ui();

    connect(this, &MainWindow::configChanged,
            this, &MainWindow::config_changed, Qt::QueuedConnection);
}

MainWindow::~MainWindow()
{
    on_pushButton_stop_released();
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
    qDebug() << Q_FUNC_INFO << QThread::currentThread();
    this->log("Starting broker");
    broker_thread.start();
    broker = new Broker(server_port,
                        client_port,
                        server_password);
    connect(&broker_thread, &QThread::finished, broker, [this](){ this->broker->deleteLater(); });
    broker->moveToThread(&broker_thread);
    connect(broker, &Broker::started, this, &MainWindow::started, Qt::QueuedConnection);
    connect(broker, &Broker::stopped, this, &MainWindow::stopped, Qt::QueuedConnection);
    emit broker->start();
    emit configChanged();
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
        emit broker->enableKeepalive(keepalive_enabled);
}

void MainWindow::read_config(void)
{
    read_config(config_file_name);
}

void MainWindow::read_config(QString filename)
{
    QSettings cfg(filename, QSettings::IniFormat);
    qDebug() << "Reading config file" << filename;

    keepalive_enabled = cfg.value("keepalive_enabled", keepalive_enabled).toBool();
    qDebug() << "keepalive_enabled" << keepalive_enabled;

    keepalive_interval = cfg.value("keepalive_interval", keepalive_interval).toInt();
    qDebug() << "keepalive_interval" << keepalive_interval;

    keepalive_missed_limit = cfg.value("keepalive_missed_limit", keepalive_missed_limit).toInt();
    qDebug() << "keepalive_missed_limit" << keepalive_missed_limit;

    emit configChanged();
}

void MainWindow::setup_config_file_watcher()
{
    config_file_watcher->addPath(config_file_name);
    config_file_watcher->addPath(".");
    //Reread file when it's changed
    connect(config_file_watcher, &QFileSystemWatcher::fileChanged, this, [&](const QString &path)
            {
                if (path == config_file_name && QFile::exists(path))
                    read_config(path);
        }, Qt::QueuedConnection);
    //Detect file addition
    connect(config_file_watcher, &QFileSystemWatcher::directoryChanged, this, [&](const QString &path)
            {
                //addPath returns false if file is present on the list
                if (QFile::exists(config_file_name) &&
                    config_file_watcher->addPath(config_file_name))
                {
                    read_config(config_file_name);
                }
            }, Qt::QueuedConnection);
}

void MainWindow::config_changed()
{
    if (broker != nullptr)
    {
        emit broker->setKeepaliveInterval(keepalive_interval);
        emit broker->setKeepaliveMissedLimit(keepalive_missed_limit);
        emit broker->enableKeepalive(keepalive_enabled);
    }
}
