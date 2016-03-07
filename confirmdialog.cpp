/* ========================================================================
Project  Name		: Fast/Force copy file and directory
Create				: 2016-02-28
Copyright			: Kengo Sawatsu
Summary             : 「実行前確認」 のダイアログ
Reference			:
======================================================================== */

#include "confirmdialog.h"
#include "ui_confirmdialog.h"

confirmDialog::confirmDialog(QWidget *parent,
FastCopy::Info *_info,
Cfg *_cfg,
const void *_title,
const void *_src,
const void *_dst,
bool _isShellext) :
    QDialog(parent),
    ui(new Ui::confirmDialog)
{
    ui->setupUi(this);
    exec_button = new QPushButton(tr("&Execute"));
    ui->buttonBox->addButton(exec_button,QDialogButtonBox::AcceptRole);

    info = _info;
    cfg = _cfg;
    title = _title;
    src = _src;
    dst = _dst;
    isShellExt = _isShellext;

    if(title != NULL){
        this->setWindowTitle((char*)title);
    }

    ui->textEdit_Src->setText((char*)src);

    if(dst != NULL){
        ui->textEdit_Dst->setText((char*)dst);
    }
    else{
        //deleteモード
        ui->label_Dst->hide();
        this->resize(this->width(),this->height() - ui->label_Dst->height());
        ui->textEdit_Dst->hide();
        this->resize(this->width(),this->height() - ui->textEdit_Dst->height());
    }

}

confirmDialog::~confirmDialog()
{
    delete ui;
    delete exec_button;
}
