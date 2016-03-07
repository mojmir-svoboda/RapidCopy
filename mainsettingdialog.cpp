/* ========================================================================
Project  Name		: Fast/Force copy file and directory
Create				: 2016-02-28
Copyright			: Kengo Sawatsu
Summary             : 「Preference(一般設定)」 のダイアログ
Reference			:
======================================================================== */
#include "mainsettingdialog.h"
#include "ui_mainsettingdialog.h"
#include "mainwindow.h"

struct VerifyInfo VERIFYINFO_LIST [] = {
    {IDS_SHA1,TDigest::SHA1},
    {IDS_MD5,TDigest::MD5},
    {IDS_XX,TDigest::XX},
    {IDS_SHA2_256,TDigest::SHA2_256},
    {IDS_SHA2_512,TDigest::SHA2_512},
    {IDS_SHA3_256,TDigest::SHA3_256},
    {IDS_SHA3_512,TDigest::SHA3_512},
};

mainsettingDialog::mainsettingDialog(QWidget *parent,Cfg *cfg_pt) :
    QDialog(parent),
    ui(new Ui::mainsettingDialog)
{

    ui->setupUi(this);
    this->cfg = cfg_pt;
    //一般タブに初期設定

    //ベリファイモードリスト作成
    for (int i=0; VERIFYINFO_LIST[i].resId; i++) {
        ui->comboBox_VerifyMode->addItem((char*)GetLoadStrV(VERIFYINFO_LIST[i].resId));
    }

    //test
    ui->checkBox_rwstat->setEnabled(false);
    ui->checkBox_rwstat->setVisible(false);
    ui->checkBox_aio->setEnabled(false);
    ui->checkBox_aio->setVisible(false);
    GetData();
    ui->general_optionlist->setCurrentRow(0);

    ui->checkBox_Rdahead->setVisible(false);
}

mainsettingDialog::~mainsettingDialog()
{
    delete ui;
}

//EvCreate代わりのカレントコンフィグ取得->GUI設定ウインドウ
BOOL mainsettingDialog::GetData()
{
    //Defaults
    ui->lineEdit_Buffer->setText(QString::number(cfg->bufSize));
    ui->checkBox_NonStop->setChecked(cfg->ignoreErr ? true:false);
    ui->checkBox_Estimate->setChecked(cfg->estimateMode ? true:false);
    ui->checkBox_ACL->setChecked(cfg->enableAcl);
    ui->checkBox_Xattr->setChecked(cfg->enableXattr);
    ui->checkBox_Dotignore->setChecked(cfg->Dotignore_mode);
    ui->checkBox_Verify->setChecked(cfg->enableVerify);
    //ui->checkBox_F_NOCACHE->setChecked(cfg->enableF_NOCACHE);
    ui->checkBox_LTFS->setChecked(cfg->enableLTFS);

    ui->checkBox_Owdel->setChecked(cfg->enableOwdel);
    ui->checkBox_ShowExtend->setChecked(cfg->isExtendFilter);

    ui->checkBox_JobList->setChecked(cfg->isJobListMode);
    SetSpeedLevelLabel(cfg->speedLevel);

    //I/O settings
    ui->lineEdit_Iomax->setText(QString::number(cfg->maxTransSize));
    //ui->checkBox_Rdahead->setChecked(cfg->enableReadahead);

    //Copy/Move Options
    ui->checkBox_Samedir->setChecked(cfg->isSameDirRename);
    ui->checkBox_Nocreate->setChecked(cfg->skipEmptyDir);
    ui->checkBox_Symlink->setChecked(cfg->isReparse);
    ui->checkBox_Movemode->setChecked(cfg->enableMoveAttr);
    ui->checkBox_Moveone->setChecked(cfg->serialMove);
    ui->checkBox_Moveone_verify->setChecked(cfg->serialVerifyMove);
    ui->checkBox_Noutime->setChecked(cfg->isDisableutime);

    //Delete Options
    ui->checkBox_NSA->setChecked(cfg->enableNSA);
    ui->checkBox_Delfilter->setChecked(cfg->delDirWithFilter);

    //Log Settings
    ui->lineEdit_Srcdstnum->setText(QString::number(cfg->maxHistoryNext));
    ui->checkBox_Errlog->setChecked(cfg->isErrLog);

    ui->checkBox_Filelog->setChecked(cfg->fileLogMode);
    if(cfg->fileLogMode & Cfg::AUTO_FILELOG){
        ui->checkBox_FilelogCSV->setChecked(cfg->fileLogMode & Cfg::ADD_CSVFILELOG ? true : false);
    }
    ui->checkBox_Aclerr->setChecked(cfg->aclErrLog);
    ui->checkBox_Xattrerr->setChecked(cfg->streamErrLog);

    //Misc
    ui->checkBox_Dontwait->setChecked(cfg->forceStart ? true : false);
    ui->checkBox_Confirm_exe->setChecked(cfg->execConfirm);
    ui->checkBox_Taskminimize->setChecked(cfg->taskbarMode);
    switch(cfg->infoSpan){
        case 0:
            ui->radioButton_250ms->setChecked(true);
            break;
        case 1:
            ui->radioButton_500ms->setChecked(true);
            break;
        case 2:
            ui->radioButton_1000ms->setChecked(true);
            break;

        default:
            ui->radioButton_500ms->setChecked(true);
            break;
    }

    ui->checkBox_EnglishUI->setChecked(cfg->lcid == QLocale::Japanese ? false : true);
    ui->comboBox_VerifyMode->setCurrentIndex(cfg->usingMD5);
    ui->checkBox_rwstat->setChecked(cfg->rwstat);
    ui->checkBox_aio->setChecked(cfg->async);
    return	TRUE;
}

int mainsettingDialog::SetSpeedLevelLabel(int level)
{
    if (level == -1){
        level = ui->slider_Speed->value();
    }
    else{
        ui->slider_Speed->setValue(level);
    }

    char	buf[64];
    sprintf(buf,
            level == SPEED_FULL ?		(char*)GetLoadStrV(IDS_FULLSPEED_DISP) :
            level == SPEED_AUTO ?		(char*)GetLoadStrV(IDS_AUTOSLOW_DISP) :
            level == SPEED_SUSPEND ?	(char*)GetLoadStrV(IDS_SUSPEND_DISP) :
                                        (char*)GetLoadStrV(IDS_RATE_DISP),
            level);
    ui->label_Realspeed->setText(buf);
    return	level;
}

BOOL mainsettingDialog::SetData()
{
    //Defaults
    cfg->bufSize = ui->lineEdit_Buffer->text().toInt();
    cfg->ignoreErr = ui->checkBox_NonStop->checkState() ? 1 : 0;
    cfg->estimateMode = ui->checkBox_Estimate->checkState() ? 1 : 0;
    cfg->enableAcl = ui->checkBox_ACL->checkState();
    cfg->enableXattr = ui->checkBox_Xattr->checkState();

    cfg->enableVerify = ui->checkBox_Verify->checkState();
    //cfg->enableF_NOCACHE = ui->checkBox_F_NOCACHE->checkState();
    cfg->enableOwdel = ui->checkBox_Owdel->checkState();
    cfg->isExtendFilter = ui->checkBox_ShowExtend->checkState();
    cfg->isJobListMode = ui->checkBox_JobList->checkState();
    cfg->speedLevel = SetSpeedLevelLabel();

    //I/O settings
    cfg->maxTransSize = ui->lineEdit_Iomax->text().toInt();
    //cfg->enableReadahead = ui->checkBox_Rdahead->checkState();

    //Copy/Move Options
    cfg->isSameDirRename = ui->checkBox_Samedir->checkState();
    cfg->skipEmptyDir = ui->checkBox_Nocreate->checkState() ? 1 : 0;
    cfg->isReparse = ui->checkBox_Symlink->checkState();
    cfg->enableMoveAttr = ui->checkBox_Movemode->checkState();
    cfg->serialMove = ui->checkBox_Moveone->checkState();
    cfg->serialVerifyMove = ui->checkBox_Moveone_verify->checkState();
    cfg->enableLTFS = ui->checkBox_LTFS->checkState();
    cfg->Dotignore_mode = ui->checkBox_Dotignore->checkState();
    cfg->isDisableutime = ui->checkBox_Noutime->checkState();

    //Delete Options
    cfg->enableNSA = ui->checkBox_NSA->checkState();
    cfg->delDirWithFilter = ui->checkBox_Delfilter->checkState();

    //Log Settings
    cfg->maxHistoryNext = ui->lineEdit_Srcdstnum->text().toInt();
    cfg->isErrLog = ui->checkBox_Errlog->checkState();
    //廃止
    cfg->fileLogMode = ui->checkBox_Filelog->checkState() ? Cfg::AUTO_FILELOG : Cfg::NO_FILELOG;
    if(cfg->fileLogMode & Cfg::AUTO_FILELOG){
        cfg->fileLogMode |= ui->checkBox_FilelogCSV->isChecked() ? Cfg::ADD_CSVFILELOG : 0;
    }
    cfg->aclErrLog = ui->checkBox_Aclerr->checkState();
    cfg->streamErrLog = ui->checkBox_Xattrerr->checkState();

    //Misc
    cfg->forceStart = ui->checkBox_Dontwait->checkState() ? 1 : 0;
    cfg->execConfirm = ui->checkBox_Confirm_exe->checkState();
    cfg->taskbarMode = ui->checkBox_Taskminimize->checkState();
    if(ui->radioButton_250ms->isChecked()){
        cfg->infoSpan = 0;
    }
    else if(ui->radioButton_500ms->isChecked()){
        cfg->infoSpan = 1;
    }
    else{
        cfg->infoSpan = 2;
    }
    cfg->lcid = ui->checkBox_EnglishUI->checkState() ? QLocale::English : QLocale::Japanese;
    cfg->usingMD5 = ui->comboBox_VerifyMode->currentIndex();
    cfg->rwstat = ui->checkBox_rwstat->checkState();
    cfg->async = ui->checkBox_aio->checkState();
    return	TRUE;
}


void mainsettingDialog::on_buttonBox_accepted()
{
    //現在設定をcfgクラスに反映
    SetData();
    //定義ファイルに書き込む
    cfg->WriteIni();
}

void mainsettingDialog::on_buttonBox_rejected()
{
    //何もしない
}

void mainsettingDialog::on_general_optionlist_currentRowChanged(int changedrowindex)
{
//	qDebug("changed %d",changedrowindex);
}

void mainsettingDialog::on_slider_Speed_valueChanged(int value)
{
    SetSpeedLevelLabel(value);
}

void mainsettingDialog::on_checkBox_Filelog_stateChanged(int arg1)
{
    switch(arg1){

        case Qt::Unchecked:
                ui->checkBox_FilelogCSV->setChecked(false);
                ui->checkBox_FilelogCSV->setEnabled(false);
                break;
        case Qt::Checked:
        case Qt::PartiallyChecked:
                ui->checkBox_FilelogCSV->setEnabled(true);
                break;
        default:
                break;
    }
}
