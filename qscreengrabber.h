#ifndef QSCREENGRABBER_H
#define QSCREENGRABBER_H

#include <QObject>
#include <QImage>
#include <QByteArray>
#include <QScreen>
#include <QGuiApplication>

class QScreenGrabber : public QObject
{
    Q_OBJECT
signals:
    void messageReady(const QByteArray &message);
public:
    enum PixelFormat : quint8
    {
        PF_RGB32 = 1,
        PF_RGB16 = 2,
        PF_GRAY8 = 3,
        PF_INDEX8 = 4
    };
    explicit QScreenGrabber(QObject *parent = nullptr);
    void setFrameSize(const QSize &sz);
    void setFramesPerSecond(int fps);
    void setQuality(int q);
protected:
    void timerEvent(QTimerEvent *e) override;
private:
    void grabFrame();
    void convertQuality();
    void buildMessage();
    void diffBlocks(const QRect &rect);
private:
    QSize           m_size;
    int             m_fps;
    int             m_quality;
    int             m_timerid;
    QImage          m_prev;
    QImage          m_curr;
    QByteArray      m_buffer;
    PixelFormat     m_format;
};

#endif // QSCREENGRABBER_H
