/* ========================================================================
Project  Name		: Fast/Force copy file and directory
Create				: 2016-02-28
Copyright			: Kengo Sawatsu
Summary             : 「シングルジョブ管理」 のダイアログ
Reference			:
======================================================================== */
#include "jobdialog.h"
#include "ui_jobdialog.h"
#include "mainwindow.h"

JobDialog::JobDialog(QWidget *parent,Cfg *cfg_pt) :
    QDialog(parent),
    ui(new Ui::JobDialog)
{
    char buf[MAX_PATH];

    ui->setupUi(this);
    this->cfg = cfg_pt;
    mainwindow = (void*)parent;

    //ウインドウ位置微調整はさぼり
    for (int i=0; i < cfg->jobMax; i++){
        //ジョブ管理ダイアログで削除、修正更新できるのはシングルジョブだけ
        if(cfg->jobArray[i]->flags & Job::SINGLE_JOB){
            ui->comboBox_Job->addItem((char*)cfg->jobArray[i]->title);
        }
    }

    strcpy(buf,(char*)(((MainWindow *)mainwindow)->GetJobTitle().toLocal8Bit().data()));
    if (strlen(buf) > 0) {

        int idx = cfg->SearchJobV(buf);
        if(idx >= 0){
            ui->comboBox_Job->setCurrentIndex(idx);
        }
        else{
            ui->comboBox_Job->addItem(buf);
        }
    }
    ui->pushButton_Delete->setEnabled(cfg->jobMax ? true : false);
}

JobDialog::~JobDialog()
{
    delete ui;
}

bool JobDialog::AddJob(){

    char	title[MAX_PATH];
    int		jobidx;
    if(ui->comboBox_Job->currentText().isEmpty()){
        return	FALSE;
    }
    //保存しようとしているジョブ名がすでに存在かつ、ジョブリスト用ジョブの名称と重複してないかチェック
    jobidx = cfg->SearchJobV(ui->comboBox_Job->currentText().toLocal8Bit().data());
    if(jobidx != -1 && cfg->jobArray[jobidx]->flags & Job::JOBLIST_JOB){
        //ジョブリストモード用のジョブ名と重複してるのでリターンだなもし
        QMessageBox msgbox;
        msgbox.setStandardButtons(QMessageBox::Ok);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setText((char*)GetLoadStrV(IDS_SINGLEJOB_MODERROR));
        msgbox.exec();
        return FALSE;
    }

    strcpy(title,ui->comboBox_Job->currentText().toLocal8Bit().data());

    int src_len = ((MainWindow *)mainwindow)->GetSrclen() + STR_NULL_GUARD;
    if (src_len >= NEW_MAX_HISTORY_BUF) {
        QMessageBox msgbox;
        msgbox.setStandardButtons(QMessageBox::Ok);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setText("Source is too long (max.1052672 bytes)");
        msgbox.exec();
        return FALSE;
    }

    Job			job;
    int			idx = ((MainWindow *)mainwindow)->GetCopyModeIndex();
    CopyInfo	*info = ((MainWindow *)mainwindow)->GetCopyInfo();
    void		*src = new char [src_len];
    char		dst[MAX_PATH]="";
    char		inc[MAX_PATH]="", exc[MAX_PATH]="";
    char		from_date[MINI_BUF]="", to_date[MINI_BUF]="";
    char		min_size[MINI_BUF]="", max_size[MINI_BUF]="";

    //mainWindow側メソッドで設定
    ((MainWindow *)mainwindow)->GetJobfromMain(&job);

    QString src_string = (((MainWindow *)mainwindow)->GetSrc());

    strncpy((char*)src,src_string.toLocal8Bit().data(),src_len);
    if (info[idx].mode != FastCopy::DELETE_MODE) {
        QString dst_string = (((MainWindow *)mainwindow)->GetDst());
        strncpy(dst,dst_string.toLocal8Bit().data(),MAX_PATH);
        job.diskMode = ((MainWindow *)mainwindow)->GetDiskMode();
    }

    if (job.isFilter) {
        ((MainWindow *)mainwindow)->GetFilterfromMain(inc,exc,from_date,to_date,min_size,max_size);
    }
    job.SetString(title, src, dst, info[idx].cmdline_name, inc, exc,
        from_date, to_date, min_size, max_size);
    job.isListing =  false;
    job.flags |= Job::SINGLE_JOB;
    if (cfg->AddJobV(&job)) {
        cfg->WriteIni();
        ((MainWindow *)mainwindow)->SetJobTitle(title);
        QMessageBox msgbox;
        msgbox.setStandardButtons(QMessageBox::Ok);
        msgbox.setIcon(QMessageBox::Information);
        msgbox.setText((char*)GetLoadStrV(IDS_JOBACT_SUCCESS_INFO));
        msgbox.exec();
    }
    else{
        QMessageBox msgbox;
        msgbox.setStandardButtons(QMessageBox::Ok);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setText((char*)GetLoadStrV(IDS_JOBACT_ERROR_INFO));
        msgbox.setInformativeText((char*)GetLoadStrV(IDS_JOBACT_ERROR_DETAIL_INFO));
        msgbox.exec();
    }

    delete [] src;
    return	TRUE;
}

bool JobDialog::DelJob(){
    char	buf[MAX_PATH], msg[MAX_PATH];

    if(!ui->comboBox_Job->currentText().isEmpty()){

        strncpy(buf,ui->comboBox_Job->currentText().toLocal8Bit().data(),MAX_PATH);

        int idx = cfg->SearchJobV(buf);
        sprintfV(msg, GetLoadStrV(IDS_JOBNAME), buf);
        if (idx >= 0){
            QMessageBox msgbox;

            //保存しようとしているジョブ名がすでに存在かつ、ジョブリスト用ジョブの名称と重複してないかチェック
            if(cfg->jobArray[idx]->flags & Job::JOBLIST_JOB){
                //ジョブリストモード用のジョブ名と重複してるのでエラーリターン
                msgbox.setStandardButtons(QMessageBox::Ok);
                msgbox.setIcon(QMessageBox::Warning);
                msgbox.setText((char*)GetLoadStrV(IDS_SINGLEJOB_MODERROR));
                msgbox.exec();
                return false;
            }
            msgbox.setStandardButtons(QMessageBox::Ok|QMessageBox::Cancel);
            msgbox.setIcon(QMessageBox::Question);
            msgbox.setText((char*)GetLoadStrV(IDS_DELCONFIRM));
            msgbox.setInformativeText(msg);
            if(msgbox.exec() == QMessageBox::Ok){
                cfg->DelJobinAllJobList(buf);
                cfg->DelJobV(buf);
                cfg->WriteIni();
                ui->comboBox_Job->removeItem(ui->comboBox_Job->currentIndex());
                ui->comboBox_Job->setCurrentIndex(idx == 0 ? 0 : idx < cfg->jobMax ? idx : idx -1);
                ((MainWindow *)mainwindow)->DelJobTitle();
            }
        }
    }
    ui->pushButton_Delete->setChecked(cfg->jobMax ? true : false);
    return	TRUE;
}

void JobDialog::on_pushButton_Execute_clicked()
{
    if(AddJob()){
        //成功ならしれっとダイアログ閉じる
        this->close();
    }
}

void JobDialog::on_pushButton_Delete_clicked()
{
    DelJob();
}


