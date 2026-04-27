#ifndef QSCREENVIEWER_H
#define QSCREENVIEWER_H


#include <QObject>
#include <QImage>

class QScreenViewer : public QObject
{
    Q_OBJECT
signals:
    void frameChanged(const QImage &frame);
public:
    enum PixelFormat : quint8
    {
        PF_RGB32 = 1,
        PF_RGB16 = 2,
        PF_GRAY8 = 3,
        PF_INDEX8 = 4
    };
    explicit QScreenViewer(QObject *parent = nullptr);
    void processMessage(const QByteArray &msg);
private:
    void handleFull(const char *data, int size);
    QImage::Format getFormat(PixelFormat f);
    void handleIncremental(const char *data, int size);
    void handleEnd();
private:
    QImage m_frame;
};

#endif // QSCREENVIEWER_H
