#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QKeyEvent>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), socket(nullptr)
{
    ui->setupUi(this);
    grabber = new QTerminalGrabber(this);
    grabber->startShell("/bin/bash");
    connect(grabber, &QTerminalGrabber::readyRead, this, &MainWindow::onGrabberMessage);

    server = new QWebSocketServer("terminal-ws", QWebSocketServer::NonSecureMode, this);
    server->listen(QHostAddress::Any, 12345);
    connect(server, &QWebSocketServer::newConnection , this, &MainWindow::onNewConnection);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    grabber->sendKey((Qt::Key)event->key(), event->modifiers());
}

void MainWindow::onNewConnection()
{
    socket = server->nextPendingConnection();
    if(!socket) { return; }

    connect(socket, &QWebSocket::binaryMessageReceived, this, &MainWindow::onSocketMessage);
    connect(socket, &QWebSocket::textMessageReceived, this, &MainWindow::onSocketTextMessage);
}

void MainWindow::onGrabberMessage(const QByteArray &data)
{
    if(socket)
    {
        socket->sendBinaryMessage(data);
    }
}

void MainWindow::onSocketMessage(const QByteArray &data)
{
    if(!grabber){ return; }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    QString action;
    if(err.error == QJsonParseError::NoError){
        auto obj  = doc.object();
        action = obj["action"].toString();
        if(action == QStringLiteral("resize"))
        {
            grabber->setTerminalSize(obj["rows"].toInt(), obj["cols"].toInt());
            return;
        }
    }
    grabber->sendInput(data);
}

void MainWindow::onSocketTextMessage(const QString &data)
{
    onSocketMessage(data.toUtf8());
}
