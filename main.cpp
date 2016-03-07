/* ========================================================================
Project  Name		: Fast/Force copy file and directory
Create				: 2016-02-28
Copyright			: Kengo Sawatsu
Summary             : Main関数
Reference			:
======================================================================== */

#include <QApplication>
#include <QLoggingCategory>
#include "mainwindow.h"
#include <signal.h>

MainWindow *mainwindow_pt;

//シグナルハンドラ関数
//最小限の処理しかしてはいけない、再代入可能関数以外はコールしてはいけない
//printf()するとバグになるので注意
void mainsignal_handler(int signum){
    mainwindow_pt->sighandler_mainwindow(signum);
    return;
}

int main(int argc, char *argv[])
{

    struct sigaction sa_sigint;

    //FastCopyv2.11のANSI/UNICODE対応関数テーブル準備処理の名残
    TLibInit_Win32V();

    QApplication Qmain(argc, argv);
    //自力のVUPチェックをする場合はsslライブラリ関連警告標準出力に出てしまうので、それを抑止
    //参考:https://bugreports.qt.io/browse/QTBUG-43173
    QLoggingCategory::setFilterRules("qt.network.ssl.warning=false");

    MainWindow w;
    mainwindow_pt = &w;
    //シグナルハンドラ設定
    memset(&sa_sigint, 0, sizeof(sa_sigint));
    sa_sigint.sa_handler = mainsignal_handler;

    //abort()コールはシグナルキャッチしちゃダメよ
    //if (sigaction(SIGABRT, &sa_sigint, NULL) == SYSCALL_ERR) {
    //	qDebug() << "sigaction SIGINT Error.";
    //}

    //ハードウェア障害(メモリ故障など)
    if (sigaction(SIGBUS, &sa_sigint, NULL) == SYSCALL_ERR) {
        qDebug() << "sigaction SIGBUS Error.";
    }
    //ゼロ除算、小数点溢れ
    if (sigaction(SIGFPE, &sa_sigint, NULL) == SYSCALL_ERR) {
        qDebug() << "sigaction SIGFPE Error.";
    }
    //不正アセンブラコール？
    if (sigaction(SIGILL, &sa_sigint, NULL) == SYSCALL_ERR) {
        qDebug() << "sigaction SIGILL Error.";
    }
    //CLI起動時のCtrl+C
    if (sigaction(SIGINT, &sa_sigint, NULL) == SYSCALL_ERR) {
        qDebug() << "sigaction SIGINT Error.";
    }
    //kill -9(SIGKILL) 対策はやりたいけどできない。SIGSTOPもね
    /*
    if (sigaction(SIGKILL, &sa_sigint, NULL) == SYSCALL_ERR) {
        qDebug() << "sigaction SIGKILL Error.";
    }
    */
    //通常終了
    if (sigaction(SIGQUIT, &sa_sigint, NULL) == SYSCALL_ERR) {
        qDebug() << "sigaction SIGQUIT Error.";
    }
    //セグメンテーションフォルト
    if (sigaction(SIGSEGV, &sa_sigint, NULL) == SYSCALL_ERR) {
        qDebug() << "sigaction SIGSEGV Error.";
    }
    //killデフォルト動作
    if (sigaction(SIGTERM, &sa_sigint, NULL) == SYSCALL_ERR) {
        qDebug() << "sigaction SIGTERM Error.";
    }

    if(w.IsRunasshow()){
        w.show();
    }
    else{
        w.hide();
    }
    return Qmain.exec();
}
