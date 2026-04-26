#include "qterminalgrabber.h"

#include <pty.h>
#include <unistd.h>
#include <signal.h>
#include <QSocketNotifier>
#include <QDebug>
#include <stdlib.h>

struct QTerminalGrabberPrivate  {

    QTerminalGrabberPrivate()
        :m_masterFd(-1), m_pid(-1), m_notifier(nullptr)
    {

    }

    ~QTerminalGrabberPrivate()
    {

        if(m_masterFd >= 0){
            close(m_masterFd);
        }
    }

    int m_masterFd;
    pid_t m_pid;
    QSocketNotifier *m_notifier;

};

QTerminalGrabber::QTerminalGrabber(QObject *parent)
    :QObject(parent), d(new QTerminalGrabberPrivate)
{
}

QTerminalGrabber::~QTerminalGrabber()
{
    delete d;
}

void QTerminalGrabber::setTerminalSize(int rows, int cols) {

    struct winsize ws;
    ws.ws_row = rows;
    ws.ws_col = cols;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    ioctl(d->m_masterFd, TIOCSWINSZ, &ws);
}

bool QTerminalGrabber::startShell(const QString &program)
{
    int slaveFd;

    if (openpty(&d->m_masterFd, &slaveFd, nullptr, nullptr, nullptr) < 0){
        return false;
    }
    d->m_pid = fork();

    setenv("TERM", "xterm-256color", 1);
    setenv("COLORTERM", "truecolor", 1);

    if (d->m_pid == 0)
    {
        setsid();
        dup2(slaveFd, STDIN_FILENO);
        dup2(slaveFd, STDOUT_FILENO);
        dup2(slaveFd, STDERR_FILENO);
        close(d->m_masterFd);
        close(slaveFd);
        execlp(program.toUtf8().constData(), program.toUtf8().constData(), nullptr);
        _exit(1);
    }

    close(slaveFd);
    d->m_notifier = new QSocketNotifier(d->m_masterFd, QSocketNotifier::Read, this);
    connect(d->m_notifier, &QSocketNotifier::activated, this, [this]() {
        char buf[4096];
        int n = read(d->m_masterFd, buf, sizeof(buf));
        if (n > 0){
            const auto ba = QByteArray(buf, n);
            emit readyRead(ba);
        }
    });
    return true;
}

void QTerminalGrabber::sendInput(const QByteArray &data)
{
    if(d->m_masterFd >= 0)
    {
        write(d->m_masterFd, data.constData(), data.size());
    }
}

void QTerminalGrabber::sendKey(Qt::Key k, Qt::KeyboardModifiers mods)
{
    QByteArray seq;

    switch (k)
    {
    case Qt::Key_Return:
    case Qt::Key_Enter: seq = "\n"; break;
    case Qt::Key_Backspace: seq = "\x7f"; break;
    case Qt::Key_Tab: seq = "\t"; break;
    case Qt::Key_Left: seq = "\x1b[D"; break;
    case Qt::Key_Right: seq = "\x1b[C"; break;
    case Qt::Key_Up: seq = "\x1b[A"; break;
    case Qt::Key_Down: seq = "\x1b[B"; break;
    case Qt::Key_Home: seq = "\x1b[H"; break;
    case Qt::Key_End: seq = "\x1b[F"; break;
    case Qt::Key_Delete: seq = "\x1b[3~"; break;
    case Qt::Key_Insert: seq = "\x1b[2~"; break;
    case Qt::Key_PageUp: seq = "\x1b[5~"; break;
    case Qt::Key_PageDown: seq = "\x1b[6~"; break;
    default:
        // Printable characters
        if (k >= Qt::Key_Space && k <= Qt::Key_AsciiTilde)
        {
            char c = static_cast<char>(k);
            if (mods & Qt::ShiftModifier)
                seq.append(c);
            else
                seq.append(QChar(c).toLower().toLatin1());
            break;
        }
        else
        {
            return;
        }


    }
    sendInput(seq);
}
