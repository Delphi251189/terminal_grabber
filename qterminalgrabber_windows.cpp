#include "qterminalgrabber.h"

#include <windows.h>
#include <QWinEventNotifier>
#include <QByteArray>
#include <QDebug>

struct QTerminalGrabberPrivate
{
    HPCON hPC = nullptr;

    HANDLE hInputWrite = NULL;
    HANDLE hOutputRead = NULL;

    PROCESS_INFORMATION pi{};

    QWinEventNotifier *notifier = nullptr;
};

static bool createPipePair(HANDLE &readPipe, HANDLE &writePipe)
{
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    return CreatePipe(&readPipe, &writePipe, &sa, 0);
}

QTerminalGrabber::QTerminalGrabber(QObject *parent)
    : QObject(parent), d(new QTerminalGrabberPrivate)
{
}

QTerminalGrabber::~QTerminalGrabber()
{
    if (d->notifier)
        d->notifier->deleteLater();

    if (d->hInputWrite)
        CloseHandle(d->hInputWrite);

    if (d->hOutputRead)
        CloseHandle(d->hOutputRead);

    if (d->hPC)
        ClosePseudoConsole(d->hPC);

    if (d->pi.hProcess)
        CloseHandle(d->pi.hProcess);

    if (d->pi.hThread)
        CloseHandle(d->pi.hThread);

    delete d;
}

bool QTerminalGrabber::startShell(const QString &program)
{
    if (d->hPC)
        return false;

    HANDLE inPipeRead, inPipeWrite;
    HANDLE outPipeRead, outPipeWrite;

    if (!createPipePair(inPipeRead, inPipeWrite))
        return false;

    if (!createPipePair(outPipeRead, outPipeWrite))
        return false;

    COORD size{80, 24};

    HRESULT hr = CreatePseudoConsole(size, inPipeRead, outPipeWrite, 0, &d->hPC);
    if (FAILED(hr)) {
        qWarning() << "CreatePseudoConsole failed";
        return false;
    }

    // We don't need these ends anymore
    CloseHandle(inPipeRead);
    CloseHandle(outPipeWrite);

    d->hInputWrite = inPipeWrite;
    d->hOutputRead = outPipeRead;

    // Enable UTF-8 in child
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);

    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);

    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)malloc(attrListSize);
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrListSize);

    UpdateProcThreadAttribute(
        si.lpAttributeList,
        0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
        d->hPC,
        sizeof(HPCON),
        NULL,
        NULL
    );

    QString cmd = program.isEmpty() ? "cmd.exe" : program;
    std::wstring wcmd = cmd.toStdWString();

    if (!CreateProcessW(NULL, (LPWSTR)wcmd.data(), NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &d->pi))
    {
        qWarning() << "CreateProcess failed";
        return false;
    }

    // Setup async read
    d->notifier = new QWinEventNotifier(d->hOutputRead, this);

    connect(d->notifier, &QWinEventNotifier::activated, this, [this]() {
        char buffer[4096];
        DWORD read = 0;

        if (ReadFile(d->hOutputRead, buffer, sizeof(buffer), &read, NULL) && read > 0) {
            emit readyRead(QByteArray(buffer, read));
        }
    });

    return true;
}

void QTerminalGrabber::sendInput(const QByteArray &data)
{
    if (!d->hInputWrite)
        return;

    DWORD written = 0;
    WriteFile(d->hInputWrite, data.data(), data.size(), &written, NULL);
}

void QTerminalGrabber::setTerminalSize(int rows, int cols)
{
    if (!d->hPC)
        return;

    ResizePseudoConsole(d->hPC, COORD{(SHORT)cols, (SHORT)rows});
}

void QTerminalGrabber::sendKey(Qt::Key key, Qt::KeyboardModifiers mods)
{
    QByteArray seq;

    // Ctrl handling
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
            char c = static_cast<char>(key);
            if (!(mods & Qt::ShiftModifier))
                c = QChar(c).toLower().toLatin1();
            seq.append(c);
        } else {
            return;
        }
    }

    sendInput(seq);
}
