/* ========================================================================
Project  Name		: Fast/Force copy file and directory
Create				: 2016-02-28
Copyright			: Kengo Sawatsu
Summary             : 「RapidCopyについて」 のダイアログ
Reference			:
======================================================================== */

#include "aboutdialog.h"
#include "ui_aboutdialog.h"

#include <mainwindow.h>
#include <version.h>
#include <QDesktopServices>

aboutDialog::aboutDialog(QWidget *parent,Cfg *cfg_pt) :
    QDialog(parent),
    ui(new Ui::aboutDialog)
{
    char buf[MAX_PATH + STR_NULL_GUARD];

    ui->setupUi(this);
    sprintf(buf,ABOUT_STATIC,(char*)GetLoadStrV(IDS_FASTCOPY), GetVersionStr(), GetCopyrightStr());
    ui->textEdit_vandc->append(buf);

    sprintf(buf,(char*)GetLoadStrV(IDS_ABOUT_WINDOWTITLE),(char*)GetLoadStrV(IDS_FASTCOPY));
    this->setWindowTitle(buf);

    //CopyRightその2の部分表示
    sprintf(buf,"%s",(char*)GetVersionStr2());
    ui->textEdit_vandc->append((char*)buf);
    ui->textEdit_vandc->append("");

    //debugでbuild日付と時刻いれとこ
    sprintf(buf,"Build at %s %s\n",__DATE__,__TIME__);
    ui->textEdit_vandc->append((char*)buf);

    QFile license_file;
    if(QLocale::system().country() == QLocale::Japan){
        //日本語のライセンスよみこみ
        license_file.setFileName(LICENSE_FILE_JP);
    }
    else{
        //英語のライセンスよみこみ
        license_file.setFileName(LICENSE_FILE_EN);
    }
    license_file.open(QIODevice::ReadOnly);
    QTextStream text(&license_file);
    ui->textEdit_vandc->append(text.readAll());
    license_file.close();

    ui->textEdit_vandc->moveCursor(QTextCursor::Start);
    ui->textEdit_vandc->ensureCursorVisible();
    ui->pushButton_tohp->setText((char*)GetLoadStrV(IDS_FASTCOPYURL));
}

aboutDialog::~aboutDialog()
{
    delete ui;
}

void aboutDialog::on_pushButton_tohp_clicked()
{
    QDesktopServices::openUrl(QUrl((char*)GetLoadStrV(IDS_FASTCOPYURL),QUrl::TolerantMode));
}
