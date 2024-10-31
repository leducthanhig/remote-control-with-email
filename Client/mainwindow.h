#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QMainWindow>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>
#include "ui_mainwindow.h"
#include "gmailapi.h"
#include "client.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void login();

private slots:
    void logout();

private:
    Ui::MainWindow *ui;
    QWebEngineView* view;
    QStackedWidget* stackedWidget;
    bool sleeping;

    void startSession();
    bool checkMailbox();
};

#endif // MAINWINDOW_H