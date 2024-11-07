#ifndef WORKER_H
#define WORKER_H

#include <functional>
#include <QtCore/QObject>

class Worker : public QObject {
    Q_OBJECT

public:
    void setTask(std::function<std::string()> func) {
        task = func;
    }

public slots:
    void startTask() {
        if (task) {
            std::string result = task();
            emit taskFinished(QString::fromStdString(result));
        }
    }

signals:
    void taskFinished(const QString& result);

private:
    std::function<std::string()> task;
};

#endif // WORKER_H