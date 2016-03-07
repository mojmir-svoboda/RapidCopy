/* ========================================================================
Project  Name		: Fast/Force copy file and directory
Create				: 2016-02-28
Copyright			: Kengo Sawatsu
Summary             : 「終了時処理」 のダイアログ
Reference			:
======================================================================== */
#include "finactdialog.h"
#include "ui_finactdialog.h"
#include "mainwindow.h"
#include <QMessageBox>
#include <QMediaPlayer>
#include <QStandardPaths>

FinActDialog::FinActDialog(QWidget *parent,Cfg *cfg_pt) :
    QDialog(parent),
    ui(new Ui::FinActDialog)
{
    ui->setupUi(this);
    this->cfg = cfg_pt;
    this->mainParent = parent;
    player = new QMediaPlayer;

    for (int i=0; i < cfg->finActMax; i++) {
        ui->comboBox_Action->insertItem(i,(char*)cfg->finActArray[i]->title);
    }

    for (int i=0; i < 3; i++) {
        ui->comboBox_CommandTiming->insertItem(i,(char*)GetLoadStrV(IDS_FACMD_ALWAYS+i));
        ui->comboBox_MailTiming->insertItem(i,(char*)GetLoadStrV(IDS_FACMD_ALWAYS+i));
    }
    //電源関連処理が実装される日まで非表示実装
    ui->groupBox_Power->setVisible(false);
    //uiファイルデフォルトに従って自由にパスを設定、編集可能
    //ui->comboBox_SoundFile
    ui->comboBox_SoundFile->setEditable(true);
    //非表示にしたぶん空欄になってるので、リサイズ
    this->resize(this->sizeHint());

    Reflect(max(((MainWindow*)mainParent)->GetFinActIdx(),0));
}

FinActDialog::~FinActDialog()
{
    delete ui;
    delete player;
}

BOOL FinActDialog::Reflect(int idx)
{
    if (idx >= cfg->finActMax) return FALSE;

    FinAct *finAct = cfg->finActArray[idx];
    ui->comboBox_Action->setCurrentIndex(idx);
    ui->comboBox_SoundFile->setEditText((char*)finAct->sound);
    ui->lineEdit_Command->setText((char*)finAct->command);

    char	shutdown_time[100] = "60";
    BOOL	is_stop = finAct->shutdownTime >= 0;

    if (is_stop){
        sprintf(shutdown_time, "%d", finAct->shutdownTime);
    }
    ui->lineEdit_GraceSec->setText(shutdown_time);
    ui->checkBox_Standby->setChecked(is_stop && (finAct->flags & FinAct::SUSPEND) ? true : false);
    ui->checkBox_Hibernation->setChecked(is_stop && (finAct->flags & FinAct::HIBERNATE) ? true : false);
    ui->checkBox_Force->setChecked(is_stop && (finAct->flags & FinAct::FORCE) ? true : false);
    ui->checkBox_Shutdown->setChecked(is_stop && (finAct->flags & FinAct::SHUTDOWN) ? true : false);
    ui->checkBox_DontExecute->setChecked(is_stop && (finAct->flags & FinAct::ERR_SHUTDOWN) ? true : false);

    ui->lineEdit_Account->setText((char*)finAct->account);
    ui->lineEdit_Password->setText((char*)finAct->password);
    ui->lineEdit_SMTPServer->setText((char*)finAct->smtpserver);
    ui->lineEdit_Port->setText((char*)finAct->port);
    ui->lineEdit_ToMailAddr->setText((char*)finAct->tomailaddr);
    ui->lineEdit_FromMailAddr->setText((char*)finAct->frommailaddr);

    ui->checkBox_Error->setChecked((finAct->flags & FinAct::ERR_SOUND) ? true : false);
    ui->checkBox_Commandwait->setChecked((finAct->flags & FinAct::WAIT_CMD)  ? true : false);


    int	fidx =  (finAct->flags & FinAct::ERR_CMD) ? 2 :
                (finAct->flags & FinAct::NORMAL_CMD) ? 1 : 0;
    ui->comboBox_CommandTiming->setCurrentIndex(fidx);

    int midx = (finAct->flags & FinAct::ERR_MAIL) ? 2 :
               (finAct->flags & FinAct::NORMAL_MAIL) ? 1 : 0;
    ui->comboBox_MailTiming->setCurrentIndex(midx);

    ui->pushButton_Del->setEnabled((finAct->flags & FinAct::BUILTIN) ? false : true);
    return	TRUE;
}

BOOL FinActDialog::AddFinAct()
{
    FinAct	finAct;

    char	title[MAX_PATH];
    char	sound[MAX_PATH];
    char	command[MAX_PATH];

    if(strncpy(title,ui->comboBox_Action->currentText().toLocal8Bit().data(),MAX_PATH) <= 0
        || lstrcmpiV(title, FALSE_V) == 0){
        return	FALSE;
    }

    int idx = cfg->SearchFinActV(title);

    strncpy(sound,ui->comboBox_SoundFile->currentText().toLocal8Bit().data(),MAX_PATH);
    strncpy(command,ui->lineEdit_Command->text().toLocal8Bit().data(),MAX_PATH);
    finAct.SetString(title, sound, command,
                     ui->lineEdit_Account->text().toLocal8Bit().data(),
                     ui->lineEdit_Password->text().toLocal8Bit().data(),
                     ui->lineEdit_SMTPServer->text().toLocal8Bit().data(),
                     ui->lineEdit_Port->text().toLocal8Bit().data(),
                     ui->lineEdit_ToMailAddr->text().toLocal8Bit().data(),
                     ui->lineEdit_FromMailAddr->text().toLocal8Bit().data());

    if (GetChar(sound, 0)) {
        finAct.flags |= ui->checkBox_Error->isChecked() ? FinAct::ERR_SOUND : 0;
    }
    if (GetChar(command, 0)) {
        int	fidx = ui->comboBox_CommandTiming->currentIndex();
        finAct.flags |= (fidx == 1 ? FinAct::NORMAL_CMD : fidx == 2 ? FinAct::ERR_CMD : 0);
        finAct.flags |= ui->checkBox_Commandwait->isChecked() ? FinAct::WAIT_CMD : 0;
    }

    if(!ui->lineEdit_Account->text().isEmpty() &&
       !ui->lineEdit_Password->text().isEmpty() &&
       !ui->lineEdit_SMTPServer->text().isEmpty() &&
       !ui->lineEdit_Port->text().isEmpty() &&
       !ui->lineEdit_ToMailAddr->text().isEmpty() &&
       !ui->lineEdit_FromMailAddr->text().isEmpty()){
            int midx = ui->comboBox_MailTiming->currentIndex();
            finAct.flags |= (midx == 1 ? FinAct::NORMAL_MAIL : midx == 2 ? FinAct::ERR_MAIL : 0);
    }

    if(idx >= 1 && (cfg->finActArray[idx]->flags & FinAct::BUILTIN)){
        finAct.flags |= cfg->finActArray[idx]->flags &
            (FinAct::SUSPEND|FinAct::HIBERNATE|FinAct::SHUTDOWN);
    }
    else{
        finAct.flags |= ui->checkBox_Standby->isChecked() ? FinAct::SUSPEND : 0;
        finAct.flags |= ui->checkBox_Hibernation->isChecked() ? FinAct::HIBERNATE : 0;
        finAct.flags |= ui->checkBox_Shutdown->isChecked() ? FinAct::SHUTDOWN : 0;
    }

    if(finAct.flags & (FinAct::SUSPEND|FinAct::HIBERNATE|FinAct::SHUTDOWN)){
        finAct.flags |= ui->checkBox_Force->isChecked() ? FinAct::FORCE : 0;
        finAct.flags |= ui->checkBox_DontExecute->isChecked() ? FinAct::ERR_SHUTDOWN : 0;
        finAct.shutdownTime = 60;
        if(!ui->lineEdit_GraceSec->text().isEmpty()){
            finAct.shutdownTime = strtoulV(ui->lineEdit_GraceSec->text().toLocal8Bit().data(),0,10);
        }
    }

    if(cfg->AddFinActV(&finAct)){
        cfg->WriteIni();
        if(ui->comboBox_Action->count() < cfg->finActMax){
            ui->comboBox_Action->addItem((char*)title);
        }
        Reflect(cfg->SearchFinActV(title));
    }
    else{
        QMessageBox errorBox;
        errorBox.setText("Add FinAct Error");
        errorBox.setStandardButtons(QMessageBox::Ok);
        errorBox.setDefaultButton(QMessageBox::Ok);
        errorBox.setIcon(QMessageBox::Warning);
        errorBox.exec();
    }
    return	TRUE;
}

BOOL FinActDialog::DelFinAct()
{
    char buf[MAX_PATH],msg[MAX_PATH];

    if(strncpy(buf,ui->comboBox_Action->currentText().toLocal8Bit().data(),MAX_PATH) > 0){
        int idx = cfg->SearchFinActV(buf);
        sprintfV(msg, GetLoadStrV(IDS_FINACTNAME), buf);

        QMessageBox msgBox;


        if(cfg->finActArray[idx]->flags & FinAct::BUILTIN){
            msgBox.setText("Can't delete built-in Action");
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setDefaultButton(QMessageBox::Ok);
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.exec();
        }
        else{

            msgBox.setText((char*)GetLoadStrV(IDS_DELCONFIRM));
            msgBox.setStandardButtons(QMessageBox::Ok|QMessageBox::Cancel);
            msgBox.setIcon(QMessageBox::Question);
            if(msgBox.exec() == QMessageBox::Ok){
                cfg->DelFinActV(buf);
                cfg->WriteIni();
                ui->comboBox_Action->removeItem(idx);
                idx = idx == cfg->finActMax ? idx -1 : idx;
                ui->comboBox_Action->setCurrentIndex(idx);
                Reflect(idx);
            }
        }
    }
    return	TRUE;
}

void FinActDialog::on_pushButton_Create_clicked()
{
    AddFinAct();
    QMessageBox msgbox;
    msgbox.setStandardButtons(QMessageBox::Ok);
    msgbox.setIcon(QMessageBox::Information);
    msgbox.setText((char*)GetLoadStrV(IDS_FINACT_CRESAV_INFO));
    msgbox.exec();
}

void FinActDialog::on_pushButton_Del_clicked()
{
    DelFinAct();
}

void FinActDialog::on_comboBox_Action_currentIndexChanged(int index)
{
    Reflect(index);
}

void FinActDialog::on_pushButton_SoundFile_clicked()
{
    QFileDialog file_dialog(this);
    QString srcstr;

    srcstr = file_dialog.getOpenFileName(
                this,
                (char*)GetLoadStrV(IDS_OPENFILE),
                cfg->soundDirV,
                tr("Sound Files (*.wav)"));
    if(!srcstr.isEmpty()){
        ui->comboBox_SoundFile->setEditText(srcstr);
    }
}

void FinActDialog::on_pushButton_Play_clicked()
{

    QString target_filepath = ui->comboBox_SoundFile->currentText();
    QFileInfo soundfile_info(target_filepath);
    if(soundfile_info.isFile() && soundfile_info.suffix() == "wav"){

        player->setMedia(QUrl::fromLocalFile(target_filepath));
        player->play();
    }
    else{
        QMessageBox errorBox;
        errorBox.setText("please select Sound file(aiff,mp3,wav).");
        errorBox.setStandardButtons(QMessageBox::Ok);
        errorBox.setDefaultButton(QMessageBox::Ok);
        errorBox.setIcon(QMessageBox::Warning);
        errorBox.exec();
    }
}

void FinActDialog::on_pushButton_Command_clicked()
{
    QFileDialog file_dialog(this);
    QString srcstr;

    srcstr = file_dialog.getOpenFileName(
                this,
                (char*)GetLoadStrV(IDS_OPENFILE),
                QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                0,
                0,
                0);
    //nullじゃない？(キャンセルされてない？)
    if(!srcstr.isEmpty()){
        //設定してもよさそうね
        ui->comboBox_CommandTiming->setCurrentText(srcstr);
    }
}

//未使用未実装
void FinActDialog::on_checkBox_Standby_clicked(bool checked)
{
    if(checked){
        ui->checkBox_Hibernation->setChecked(false);
        ui->checkBox_Shutdown->setChecked(false);
    }
}

void FinActDialog::on_checkBox_Hibernation_clicked(bool checked)
{
    if(checked){
        ui->checkBox_Standby->setChecked(false);
        ui->checkBox_Shutdown->setChecked(false);
    }
}

void FinActDialog::on_checkBox_Shutdown_clicked(bool checked)
{
    if(checked){
        ui->checkBox_Standby->setChecked(false);
        ui->checkBox_Hibernation->setChecked(false);
    }
}
