/* ========================================================================
    Project  Name			: Fast/Force copy file and directory
    Create					: 2016-02-28(Sun)
    Copyright				: Kengo Sawatsu
    Reference				:
    ======================================================================== */

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <tlib.h>
#include <osl.h>
#include <cfg.h>

#define ABOUT_STATIC "%s %s\n\n%s"

namespace Ui {
class aboutDialog;
}

class aboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit aboutDialog(QWidget *parent = 0,Cfg *cfg_pt=NULL);
    ~aboutDialog();

private slots:
    void on_pushButton_tohp_clicked();

private:
    Ui::aboutDialog *ui;
};

#endif // ABOUTDIALOG_H
