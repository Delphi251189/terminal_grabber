#include "qterminalgrabber.h"

#include <util.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <QSocketNotifier>

struct QTerminalGrabberPrivate  {

    QTerminalGrabberPrivate()
        :masterFd(-1), pid(-1), notifier(nullptr)
    {

    }

    ~QTerminalGrabberPrivate()
    {

        if(masterFd >= 0){
            close(masterFd);
        }
    }

    int masterFd;
    pid_t pid;
    QSocketNotifier *notifier;

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
    ioctl(d->masterFd, TIOCSWINSZ, &ws);

}

bool QTerminalGrabber::startShell(const QString &program)
{

    if (d->masterFd >= 0)
        return false;

    struct winsize ws;
    ws.ws_row = 24;
    ws.ws_col = 80;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    d->pid = forkpty(&d->masterFd, nullptr, nullptr, &ws);

    if (d->pid < 0) {
        return false;
    }

    if (d->pid == 0) {
        // --- CHILD PROCESS ---

        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        setenv("LANG", "en_US.UTF-8", 1);

        QByteArray prog = program.isEmpty()
                ? QByteArray("/bin/zsh")
                : program.toUtf8();

        // execlp(prog.constData(), prog.constData(), nullptr);

        execlp(prog.constData(), prog.constData(), "-il", nullptr);

        _exit(1);
    }

    // --- PARENT PROCESS ---

    d->notifier = new QSocketNotifier(d->masterFd, QSocketNotifier::Read, this);

    connect(d->notifier, &QSocketNotifier::activated,
            this, [this]() {
        char buffer[4096];
        ssize_t n = ::read(d->masterFd, buffer, sizeof(buffer));
        if (n > 0) {
            emit readyRead(QByteArray(buffer, n));
        }
    });

    return true;
}

void QTerminalGrabber::sendInput(const QByteArray &data)
{
    if(d->masterFd >= 0)
    {
        write(d->masterFd, data.constData(), data.size());
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
