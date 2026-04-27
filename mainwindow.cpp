#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QKeyEvent>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QHttpServer>
#include <QTcpServer>
#endif

#include "qscreengrabber.h"
#include "qscreenviewer.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), socket(nullptr)
{
    ui->setupUi(this);
    grabber = new QTerminalGrabber(this);
#if defined(Q_OS_OSX)
    grabber->startShell("/bin/zsh");
#elif defined(Q_OS_UNIX)
    grabber->startShell("/bin/bash");
#elif defined(Q_OS_WIN)
    grabber->startShell("cmd.exe");
#else

#endif
    connect(grabber, &QTerminalGrabber::readyRead, this, &MainWindow::onGrabberMessage);

    server = new QWebSocketServer("terminal-ws", QWebSocketServer::NonSecureMode, this);
    server->listen(QHostAddress::Any, 12345);
    connect(server, &QWebSocketServer::newConnection , this, &MainWindow::onNewConnection);

    initHttpServer();
    initScreenGrabber();


}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    grabber->sendKey((Qt::Key)event->key(), event->modifiers());
}

void MainWindow::initScreenGrabber()
{
    auto sg = new QScreenGrabber(this);
    auto sv = new QScreenViewer(this);

    connect(sg, &QScreenGrabber::messageReady, sv, &QScreenViewer::processMessage);
    connect(sv, &QScreenViewer::frameChanged, [&](const QImage &img){

        ui->label->setPixmap(QPixmap::fromImage(img));
    });

    sg->setFrameSize(QSize(800,600));
    sg->setQuality(5);
    sg->setFramesPerSecond(5);


}

void MainWindow::initHttpServer()
{
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    auto server = new QHttpServer(this);

    server->route("/", [] () {
        QFile f(":/index.html");
        f.open(QIODevice::ReadOnly);
        const auto str = f.readAll();
        f.close();
        return QString::fromUtf8(str);
    });

    auto tcpserver = new QTcpServer();
    if (!tcpserver->listen(QHostAddress::Any, 8000))
    {
        delete tcpserver;
        delete server;
    }
    server->bind(tcpserver);

#endif
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

    if(data.startsWith("{") && data.endsWith("}"))
    {
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
    }
    grabber->sendInput(data);
}

void MainWindow::onSocketTextMessage(const QString &data)
{
    onSocketMessage(data.toUtf8());
}
