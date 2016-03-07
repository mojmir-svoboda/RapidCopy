/* ========================================================================
    Project  Name			: Fast/Force copy file and directory
    Create					: 2016-02-28(Sun)
    Copyright				: Kengo Sawatsu
    Reference				:
    ======================================================================== */

#ifndef JOBDIALOG_H
#define JOBDIALOG_H

#include <QDialog>
#include <tlib.h>
#include <osl.h>
#include <cfg.h>
#include <QMessageBox>
//#include "mainwindow.h"
namespace Ui {
class JobDialog;
}

class JobDialog : public QDialog
{
    Q_OBJECT

public:
    explicit JobDialog(QWidget *parent = 0,Cfg *cfg_pt = NULL);
    ~JobDialog();

private:
    Ui::JobDialog *ui;
    bool AddJob();
    bool DelJob();

protected:
    Cfg *cfg;
    void *mainwindow;

private slots:
    void on_pushButton_Delete_clicked();
    void on_pushButton_Execute_clicked();
};

#endif // JOBDIALOG_H
