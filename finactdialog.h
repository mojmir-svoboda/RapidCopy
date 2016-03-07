/* ========================================================================
    Project  Name			: Fast/Force copy file and directory
    Create					: 2016-02-28(Sun)
    Copyright				: Kengo Sawatsu
    Reference				:
    ======================================================================== */

#ifndef FINACTDIALOG_H
#define FINACTDIALOG_H

#include <QDialog>
#include <QMediaPlayer>
#include <QFileInfoList>
#include <tlib.h>
#include <osl.h>
#include <cfg.h>

namespace Ui {
class FinActDialog;
}

class FinActDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FinActDialog(QWidget *parent = 0,Cfg *cfg_pt = NULL);
    ~FinActDialog();

protected:
    Cfg *cfg;
    QWidget *mainParent;
    QMediaPlayer *player;
    QFileInfoList list;

public:
    BOOL	Reflect(int idx);
    BOOL	AddFinAct();
    BOOL	DelFinAct();

private slots:
    void on_pushButton_Create_clicked();
    void on_pushButton_Del_clicked();
    void on_comboBox_Action_currentIndexChanged(int index);
    void on_pushButton_SoundFile_clicked();
    void on_pushButton_Play_clicked();
    void on_pushButton_Command_clicked();
    void on_checkBox_Standby_clicked(bool checked);
    void on_checkBox_Hibernation_clicked(bool checked);
    void on_checkBox_Shutdown_clicked(bool checked);
private:
    Ui::FinActDialog *ui;

};

#endif // FINACTDIALOG_H
