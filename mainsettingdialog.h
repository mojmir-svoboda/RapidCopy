/* ========================================================================
    Project  Name			: Fast/Force copy file and directory
    Create					: 2016-02-28(Sun)
    Copyright				: Kengo Sawatsu
    Reference				:
    ======================================================================== */

#ifndef MAINSETTINGDIALOG_H
#define MAINSETTINGDIALOG_H

#include <QDialog>

#include <tlib.h>
#include <osl.h>
#include <cfg.h>

struct VerifyInfo {
    UINT				resId;
    TDigest::Type		type;
};

namespace Ui {
    class mainsettingDialog;
}

class mainsettingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit mainsettingDialog(QWidget *parent = 0,Cfg *cfg_pt = NULL);
    ~mainsettingDialog();

private:
    Ui::mainsettingDialog *ui;
    int SetSpeedLevelLabel(int level = -1);

protected:
    Cfg *cfg;
    VerifyInfo *verifyInfo;

private slots:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();
    void on_general_optionlist_currentRowChanged(int changedrowindex);
    BOOL SetData();
    BOOL GetData();

    void on_slider_Speed_valueChanged(int value);
    void on_checkBox_Filelog_stateChanged(int arg1);
    void on_lineEdit_Iomax_textChanged(const QString &arg1);
    void on_lineEdit_aionum_textChanged(const QString &arg1);
};

#endif // MAINSETTINGDIALOG_H
