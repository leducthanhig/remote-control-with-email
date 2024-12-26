#ifndef WORKER_HPP
#define WORKER_HPP

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
            std::string result;
            try {
                result = task();
            }
            catch (const std::exception& e) {
                result = e.what();
            }
            emit taskFinished(QString::fromStdString(result));
        }
    }

signals:
    void taskFinished(const QString& result);

private:
    std::function<std::string()> task;
};

#endif // WORKER_HPP