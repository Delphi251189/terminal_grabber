#include "qscreengrabber.h"
#include <QPainter>
#include <cstring>

QScreenGrabber::QScreenGrabber(QObject *parent)
    : QObject(parent), m_fps(5), m_quality(9), m_format(PF_RGB16), m_timerid(-1)
{

}

void QScreenGrabber::setFrameSize(const QSize &sz)
{
    m_size = sz;
}

void QScreenGrabber::setFramesPerSecond(int fps)
{
    m_fps = qBound(1, fps, 10);
    if (m_timerid > 0)
    {
        killTimer(m_timerid);
    }
    m_timerid = startTimer(1000 / m_fps, Qt::CoarseTimer);
}

void QScreenGrabber::setQuality(int q)
{
    m_quality = qBound(0, q, 10);
}

void QScreenGrabber::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_timerid)
    {
        return;
    }
    grabFrame();
    convertQuality();
    buildMessage();
    m_prev = m_curr;
}

void QScreenGrabber::grabFrame()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }
    QPixmap pix = screen->grabWindow(0);

    m_curr = pix.toImage().scaled(m_size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

void QScreenGrabber::convertQuality()
{
    if (m_quality > 9) return;

    if (m_quality >= 6) {
        m_curr = m_curr.convertToFormat(QImage::Format_RGB16);
        m_format = PF_RGB16;
    }
    else if (m_quality >= 3) {
        m_curr = m_curr.convertToFormat(QImage::Format_Indexed8);
        m_format = PF_INDEX8;
    }
    else {
        // grayscale
        m_curr = m_curr.convertToFormat(QImage::Format_Grayscale8);
        m_format = PF_GRAY8;
    }
}

void QScreenGrabber::buildMessage()
{
    // format
    // type 'F'|'I'|'E'     - 1 byte
    // reserved             - 1 byte
    // x                    - 2 byte
    // y                    - 2 byte
    // width                - 2 byte
    // height               - 2 byte
    // size                 - 4 byte
    // payload              - size bytes
    // checksum             - 2 bytes

    static const int PERIOD = 64;
    static const int BLOCK = 32;
    static int count = 0;


    if(count % PERIOD == 0 )
    {
        // SEND FULL FRAME
        quint16 w = m_curr.width();
        quint16 h = m_curr.height();
        m_buffer.clear();
        m_buffer.reserve(m_curr.sizeInBytes() + 16);
        m_buffer.append('F');
        m_buffer.append((char)m_format);
        m_buffer.append((char)0x00);
        m_buffer.append((char)0x00);
        m_buffer.append(w / 256);
        m_buffer.append(w % 256);
        m_buffer.append(h / 256);
        m_buffer.append(h % 256);
        m_buffer.append(reinterpret_cast<const char*>(m_curr.bits()), m_curr.sizeInBytes());
        m_buffer.append((char)0x00);
        m_buffer.append((char)0x00);
        emit messageReady(m_buffer);
    }
    else
    {
        for (int y = 0; y < m_curr.height(); y += BLOCK)
        {
            for (int x = 0; x < m_curr.width(); x += BLOCK)
            {
                diffBlocks(QRect(x, y, BLOCK, BLOCK));
            }
        }

        // emit END OF INCREMENTAL UPDATE MESSAGE
        // IF m_prev differs from m_curr with 48 chunks of 32x32 blocks
        // THE RECEIVER SIDE DOES'NT KNOW HOW MANY FRAMES ARE CHANGED

        m_buffer.clear();
        m_buffer.append('E');
        m_buffer.append(QByteArray::fromHex("00:00:00:00:00:00:00:00:00:00:00:00:00:00:00"));
        emit messageReady(m_buffer);
    }
    ++count;
}

void QScreenGrabber::diffBlocks(const QRect &rect)
{
    if (m_prev.isNull()) return;

    const int bytesPerPixel = m_curr.depth() / 8;

    for (int y = rect.top(); y < rect.bottom(); ++y)
    {
        const uchar *pa = m_curr.constScanLine(y);
        const uchar *pb = m_prev.constScanLine(y);
        const int x0 = rect.left();
        const int width = rect.width();
        const uchar *rowA = pa + x0 * bytesPerPixel;
        const uchar *rowB = pb + x0 * bytesPerPixel;
        const int bytes = width * bytesPerPixel;

        if (memcmp(rowA, rowB, bytes) != 0)
        {
            m_buffer.clear();
            m_buffer.reserve(rect.width() * rect.height() * bytesPerPixel + 16);
            m_buffer.append('I');
            m_buffer.append((char)m_format);
            quint16 x = rect.left();
            quint16 t = rect.top();
            quint16 w = rect.width();
            quint16 h = rect.height();

            m_buffer.append(x >> 8);
            m_buffer.append(x & 0xFF);

            m_buffer.append(t >> 8);
            m_buffer.append(t & 0xFF);

            m_buffer.append(w >> 8);
            m_buffer.append(w & 0xFF);

            m_buffer.append(h >> 8);
            m_buffer.append(h & 0xFF);

            // payload: full block
            for (int yy = rect.top(); yy < rect.bottom(); ++yy)
            {
                const uchar *line = m_curr.constScanLine(yy) + x * bytesPerPixel;
                m_buffer.append(reinterpret_cast<const char*>(line), w * bytesPerPixel);
            }
            m_buffer.append((char)0x00);
            m_buffer.append((char)0x00);
            emit messageReady(m_buffer);
            return;
        }
    }
}
