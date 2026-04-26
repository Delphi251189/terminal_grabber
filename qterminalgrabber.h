#ifndef QTERMINALGRABBER_H
#define QTERMINALGRABBER_H

#include <QObject>


struct QTerminalGrabberPrivate;
class QTerminalGrabber : public QObject
{
    Q_OBJECT
signals:
    void readyRead(const QByteArray &data);
public:
    explicit QTerminalGrabber(QObject *parent = nullptr);
    virtual ~QTerminalGrabber();
    void setTerminalSize(int rows, int cols);
    bool startShell(const QString &program);
    void sendInput(const QByteArray &data);
    void sendKey(Qt::Key k, Qt::KeyboardModifiers mods = Qt::NoModifier);
private:
    QTerminalGrabberPrivate *d;
};


#endif // QTERMINALGRABBER_H
