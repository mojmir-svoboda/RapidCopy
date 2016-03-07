/* ========================================================================
    Project  Name			: Fast/Force copy file and directory
    Create					: 2016-02-28(Sun)
    Copyright				: Kengo Sawatsu
    Reference				:
    ======================================================================== */

#ifndef JOBLISTRENAMEDIALOG_H
#define JOBLISTRENAMEDIALOG_H

#include <QDialog>

#include <tlib.h>
#include <osl.h>
#include <cfg.h>

namespace Ui {
class JobListRenameDialog;
}

class JobListRenameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit JobListRenameDialog(QWidget *parent = 0,Cfg *cfg_pt=NULL,QString before_name=NULL,QString *after_name=NULL);
    ~JobListRenameDialog();

private:
    Ui::JobListRenameDialog *ui;
    QString *joblistname_after;

protected:
    Cfg *cfg;
    void *mainwindow;
    QString joblistname_before;


private slots:
    void on_lineEdit_newJobListName_textChanged(const QString &arg1);
    void on_pushButton_Ok_clicked();
    void on_pushButton_Cancel_clicked();
};

#endif // JOBLISTRENAMEDIALOG_H
