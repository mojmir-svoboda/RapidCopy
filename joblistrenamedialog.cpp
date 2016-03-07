/* ========================================================================
Project  Name		: Fast/Force copy file and directory
Create				: 2016-02-28
Copyright			: Kengo Sawatsu
Summary             : 「ジョブリストのリネーム」専用ダイアログ
Reference			:
======================================================================== */

#include "joblistrenamedialog.h"
#include "ui_joblistrenamedialog.h"
#include "mainwindow.h"

JobListRenameDialog::JobListRenameDialog(QWidget *parent,Cfg *cfg_pt,QString before_name,QString* after_name) :
    QDialog(parent),
    ui(new Ui::JobListRenameDialog)
{
    ui->setupUi(this);
    cfg = cfg_pt;
    joblistname_before = before_name;
    ui->lineEdit_newJobListName->setText(joblistname_before);
    this->setWindowTitle((char*)GetLoadStrV(IDS_JOBLISTRENAME_TITLE));
    joblistname_after = after_name;
}

JobListRenameDialog::~JobListRenameDialog()
{
    delete ui;
}

void JobListRenameDialog::on_lineEdit_newJobListName_textChanged(const QString &arg1)
{
    if(cfg->SearchJobList(arg1.toLocal8Bit().data()) == -1){
        ui->pushButton_Ok->setEnabled(true);
    }
    else{
        ui->pushButton_Ok->setEnabled(false);
    }
    return;
}

void JobListRenameDialog::on_pushButton_Ok_clicked()
{
    //MainWindowへのリターン文字列(リネーム後文字列)セット
    joblistname_after->clear();
    joblistname_after->append(ui->lineEdit_newJobListName->text());
    //リターン
    this->accept();
}

void JobListRenameDialog::on_pushButton_Cancel_clicked()
{
    //内容をemptyにしてリターン
    joblistname_after->clear();
    this->reject();
}
