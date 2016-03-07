/* ========================================================================
    Project  Name			: Fast/Force copy file and directory
    Create					: 2016-02-28(Sun)
    Copyright				: Kengo Sawatsu
    Reference				:
    ======================================================================== */

#ifndef CONFIRMDIALOG_H
#define CONFIRMDIALOG_H

#include <QDialog>
#include <QPushButton>

//#include <mainwindow.h>
#include <fastcopy.h>
#include <cfg.h>

namespace Ui {
class confirmDialog;
}

class confirmDialog : public QDialog
{
    Q_OBJECT

public:
    //shellextは未実装
    explicit confirmDialog(QWidget *parent = 0,
                           FastCopy::Info *_info = NULL,
                           Cfg *_cfg = NULL,
                           const void *_title = NULL,
                           const void *_src = NULL,
                           const void *_dst = NULL,
                           bool _isShellext = false);
    ~confirmDialog();

private:
    Ui::confirmDialog *ui;
    QPushButton *exec_button;

    const void		*src;
    const void		*dst;
    const void		*title;
    Cfg				*cfg;
    FastCopy::Info	*info;
    BOOL			isShellExt;
};

#endif // CONFIRMDIALOG_H
