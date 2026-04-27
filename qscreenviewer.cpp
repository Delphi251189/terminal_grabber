#include "qscreenviewer.h"
#include <cstring>

QScreenViewer::QScreenViewer(QObject *parent)
    : QObject(parent)
{
}

static inline quint16 read16(const char *p)
{
    return (quint8(p[0]) << 8) | quint8(p[1]);
}

QImage::Format QScreenViewer::getFormat(PixelFormat f)
{
    switch (f) {
    case PF_GRAY8: return QImage::Format_Grayscale8;
    case PF_INDEX8: return QImage::Format_Indexed8;
    case PF_RGB16: return QImage::Format_RGB16;
    case PF_RGB32: return QImage::Format_RGB32;
    default: return QImage::Format_Invalid;
    }
}

void QScreenViewer::processMessage(const QByteArray &msg)
{
    if (msg.size() < 10) { return; }

    const char *p = msg.constData();

    char type = p[0];
    switch (type)
    {
    case 'F':
        handleFull(p, msg.size());
        break;
    case 'I':
        handleIncremental(p, msg.size());
        break;
    case 'E':
        handleEnd();
        break;
    default:
        break;
    }
}

void QScreenViewer::handleFull(const char *p, int size)
{
    qint8  format = quint8(p[1]); p += 2;
    quint16 x = read16(p); p += 2;
    quint16 y = read16(p); p += 2;
    quint16 w = read16(p); p += 2;
    quint16 h = read16(p); p += 2;
    Q_UNUSED(x)
    Q_UNUSED(y)
    const char *payload = p;
    QImage img((const uchar*)payload, w, h, getFormat(PixelFormat(format)));
    m_frame = img.copy();
    emit frameChanged(m_frame);
}


void QScreenViewer::handleIncremental(const char *p, int size)
{
    if (m_frame.isNull()){  return; }

    qint8  format = quint8(p[1]); p += 2;
    quint16 x = read16(p); p += 2;
    quint16 y = read16(p); p += 2;
    quint16 w = read16(p); p += 2;
    quint16 h = read16(p); p += 2;

    Q_UNUSED(format)

    const char *payload = p;
    const int bytesPerPixel = m_frame.depth() / 8;
    for (int yy = 0; yy < h; ++yy)
    {
        uchar *dst = m_frame.scanLine(y + yy) + x * bytesPerPixel;
        const char *src = payload + yy * w * bytesPerPixel;
        memcpy(dst, src, w * bytesPerPixel);
    }
}

void QScreenViewer::handleEnd()
{
    emit frameChanged(m_frame);
}
