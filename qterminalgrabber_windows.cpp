#include "qterminalgrabber.h"

#ifdef Q_OS_WIN

#include <windows.h>
#include <QByteArray>
#include <QMetaObject>
#include <QDebug>

#include <atomic>

// winpty
#include "winpty.h"

static void printWinptyError(winpty_error_ptr_t err)
{
    if (!err)
        return;

    qDebug() << "winpty error:" << QString::fromWCharArray(winpty_error_msg(err));
}

struct QTerminalGrabberPrivate
{
    winpty_t *pty = nullptr;
    winpty_config_t *config = nullptr;
    winpty_spawn_config_t *spawnConfig = nullptr;

    HANDLE hIn = NULL;
    HANDLE hOut = NULL;

    HANDLE hThread = NULL;
    std::atomic<bool> running{false};

    QTerminalGrabber *q = nullptr;
};

// ---------------- Reader Thread ----------------

static DWORD WINAPI ReaderThread(LPVOID param)
{
    QTerminalGrabberPrivate *d = (QTerminalGrabberPrivate*)param;

    char buffer[4096];
    DWORD read = 0;

    while (d->running)
    {
        if (ReadFile(d->hOut, buffer, sizeof(buffer), &read, NULL) && read > 0)
        {
            QByteArray data(buffer, read);

            QMetaObject::invokeMethod(
                d->q,
                "readyRead",
                Qt::QueuedConnection,
                Q_ARG(QByteArray, data)
            );
        }
        else
        {
            break;
        }
    }

    return 0;
}

// ---------------- ctor / dtor ----------------

QTerminalGrabber::QTerminalGrabber(QObject *parent)
    : QObject(parent)
{
    d = new QTerminalGrabberPrivate;
    d->q = this;
}

QTerminalGrabber::~QTerminalGrabber()
{
    d->running = false;

    if (d->hOut)
        CloseHandle(d->hOut);

    if (d->hThread)
        WaitForSingleObject(d->hThread, INFINITE);

    if (d->hIn)
        CloseHandle(d->hIn);

    if (d->pty)
        winpty_free(d->pty);

    if (d->spawnConfig)
        winpty_spawn_config_free(d->spawnConfig);

    if (d->config)
        winpty_config_free(d->config);

    delete d;
}

// ---------------- startShell ----------------

bool QTerminalGrabber::startShell(const QString &program)
{
    winpty_error_ptr_t err = nullptr;

    // 1. Create config
    d->config = winpty_config_new(0, &err);
    if (!d->config) {
        qWarning() << "winpty_config_new failed";
        return false;
    }

    winpty_config_set_initial_size(d->config, 80, 24);

    // 2. Open pty
    d->pty = winpty_open(d->config, &err);
    if (!d->pty) {
        qWarning() << "winpty_open failed";
        return false;
    }

    // 3. Prepare command
    QString cmd = program.isEmpty() ? "cmd.exe" : program;

    std::wstring wcmd = cmd.toStdWString();

    // 4. Spawn config
    d->spawnConfig = winpty_spawn_config_new(
        WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN,
        wcmd.c_str(),
        NULL,
        NULL,
        NULL,
        &err
    );

    if (!d->spawnConfig) {
        printWinptyError(err);
        return false;
    }

    HANDLE childProcess = NULL;
    HANDLE childThread = NULL;

    BOOL ok = winpty_spawn(d->pty, d->spawnConfig, &childProcess, &childThread, NULL, &err );

    if (!ok) {
        qWarning() << "winpty_spawn failed";
        printWinptyError(err);
        return false;
    }

    if (childProcess)
        CloseHandle(childProcess);
    if (childThread)
        CloseHandle(childThread);

    // 5. Get pipes
    LPCWSTR conin = winpty_conin_name(d->pty);
    LPCWSTR conout = winpty_conout_name(d->pty);

    d->hIn = CreateFileW(conin, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    d->hOut = CreateFileW(conout, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (d->hIn == INVALID_HANDLE_VALUE || d->hOut == INVALID_HANDLE_VALUE) {
        qWarning() << "Failed to open winpty pipes";
        return false;
    }

    // 6. Start reader thread
    d->running = true;

    d->hThread = CreateThread(
        NULL,
        0,
        ReaderThread,
        d,
        0,
        NULL
    );

    return true;
}

// ---------------- sendInput ----------------

void QTerminalGrabber::sendInput(const QByteArray &data)
{
    if (!d->hIn)
        return;

    DWORD written = 0;
    WriteFile(d->hIn, data.data(), (DWORD)data.size(), &written, NULL);
}

// ---------------- resize ----------------

void QTerminalGrabber::setTerminalSize(int rows, int cols)
{
    if (!d->pty)
        return;

    winpty_set_size(d->pty, cols, rows, NULL);
}

// ---------------- sendKey ----------------

void QTerminalGrabber::sendKey(Qt::Key key, Qt::KeyboardModifiers mods)
{
    QByteArray seq;

    if (mods & Qt::ControlModifier) {
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            char ctrl = (key - Qt::Key_A) + 1;
            sendInput(QByteArray(1, ctrl));
            return;
        }
    }

    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        seq = "\r";
        break;
    case Qt::Key_Backspace:
        seq = "\x7f";
        break;
    case Qt::Key_Tab:
        seq = "\t";
        break;
    case Qt::Key_Left:
        seq = "\x1b[D";
        break;
    case Qt::Key_Right:
        seq = "\x1b[C";
        break;
    case Qt::Key_Up:
        seq = "\x1b[A";
        break;
    case Qt::Key_Down:
        seq = "\x1b[B";
        break;
    case Qt::Key_Delete:
        seq = "\x1b[3~";
        break;
    default:
        if (key >= Qt::Key_Space && key <= Qt::Key_AsciiTilde) {
            char c = (char)key;
            if (!(mods & Qt::ShiftModifier))
                c = (char)tolower(c);
            seq.append(c);
        } else {
            return;
        }
    }

    sendInput(seq);
}

#endif
