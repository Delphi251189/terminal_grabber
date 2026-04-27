#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWebSocketServer>

#include "qterminalgrabber.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void keyPressEvent(QKeyEvent *event) override;
public slots:
    void initScreenGrabber();
    void initHttpServer();
    void onNewConnection();
    void onGrabberMessage(const QByteArray &data);
    void onSocketMessage(const QByteArray &data);
    void onSocketTextMessage(const QString &data);

private:
    Ui::MainWindow *ui;
    QTerminalGrabber *grabber;
    QWebSocketServer    *server;
    QWebSocket          *socket;
};

#endif // MAINWINDOW_H
