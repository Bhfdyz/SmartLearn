#ifndef AICHATWORKER_H
#define AICHATWORKER_H

#include <QObject>
#include <QString>
#include <QTcpSocket>

class AIChatWorker : public QObject
{
    Q_OBJECT

public:
    explicit AIChatWorker(QObject *parent = nullptr);
    ~AIChatWorker();

public slots:
    void sendRequest(const QString &jsonData, const QString &host, quint16 port);

signals:
    void requestFinished(const QByteArray &response);
    void requestError(const QString &error);

private:
    QTcpSocket *_socket;
};

#endif // AICHATWORKER_H
