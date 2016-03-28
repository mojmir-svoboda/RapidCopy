/* ========================================================================
Project  Name		: Fast/Force copy file and directory
Create				: 2016-02-28
Copyright			: Kengo Sawatsu
Summary             : メイン画面
Reference			:
======================================================================== */
#include <fcntl.h>
#include <QScrollBar>
#include <QToolTip>
#include <smtp.h>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "version.h"

#define FASTCOPY_TIMER_TICK 250

//コピーモードと動作モード対照表
struct CopyInfo COPYINFO_LIST [] = {
    {IDS_ALLSKIP,		0,IDS_CMD_NOEXIST_ONLY,	0, FastCopy::DIFFCP_MODE,	FastCopy::BY_NAME},
    {IDS_ATTRCMP,		0,IDS_CMD_DIFF,			0, FastCopy::DIFFCP_MODE,	FastCopy::BY_ATTR},
    {IDS_SIZESKIP,		0,IDS_CMD_SIZE,			0, FastCopy::DIFFCP_MODE,	FastCopy::BY_SIZE},
    {IDS_UPDATECOPY,	0,IDS_CMD_UPDATE,		0, FastCopy::DIFFCP_MODE,	FastCopy::BY_LASTEST},
    {IDS_FORCECOPY,		0,IDS_CMD_FORCE_COPY,	0, FastCopy::DIFFCP_MODE,	FastCopy::BY_ALWAYS},
    {IDS_SYNCCOPY,		0,IDS_CMD_SYNC,			0, FastCopy::SYNCCP_MODE,	FastCopy::BY_ATTR},
//  {IDS_VERIFY,		0,IDS_CMD_VERIFY,		0, FastCopy::VERIFY_MODE,	FastCopy::BY_ATTR},
    {IDS_SIZEVERIFY,	0,IDS_CMD_SIZEVERIFY,	0, FastCopy::VERIFY_MODE,	FastCopy::BY_SIZE},
//	{IDS_MUTUAL,		0,IDS_CMD_MUTUAL,		0, FastCopy::MUTUAL_MODE,	FastCopy::BY_LASTEST},
    {IDS_MOVEATTR,		0,IDS_CMD_MOVE,			0, FastCopy::MOVE_MODE,		FastCopy::BY_ATTR},
    {IDS_MOVEFORCE,		0,IDS_CMD_MOVE,			0, FastCopy::MOVE_MODE,		FastCopy::BY_ALWAYS},
    {IDS_DELETE,		0,IDS_CMD_DELETE,		0, FastCopy::DELETE_MODE,	FastCopy::BY_ALWAYS},
    {0,					0,0,					0, (FastCopy::Mode)0,		(FastCopy::OverWrite)0}
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    //共通システム通算秒取得開始
    sys_time.start();

    finact_Group = new QActionGroup(this);
    finAct_Menu = new QAction(this);

    srchist_Group = new QActionGroup(this);
    dsthist_Group = new QActionGroup(this);

    //job表示用領域初期化
    jobtitle_static = "";

    char	*user_dir = NULL;
    char	*virtual_dir = NULL;

    //CLI引数取得
    orgArgv = QCoreApplication::arguments();
    orgArgc = orgArgv.count();

    // cfgからの読み取り
    if(!cfg.ReadIni(user_dir, virtual_dir)){
        QMessageBox msgbox;
        msgbox.setStandardButtons(QMessageBox::Ok);
        msgbox.setText("Can't initialize...");
        msgbox.setWindowTitle(FASTCOPY);
        msgbox.exec();
        return;
    }

    // アプリケーションのlocale確認と設定
    if(cfg.lcid == QLocale::Japanese){

        translater =  new QTranslator();
        main = QApplication::instance();
        QString str = "RapidCopy_" + QLocale(QLocale::Japanese, QLocale::Japan).name();
        translater->load(str,
                         TRANSLATE_RESOURCE_NAMEBASE);
        main->installTranslator(translater);
    }
    //elseは無条件に英語だよ。

    ui->setupUi(this);

    //ローカルヘルプの暫定対処。htmlのコピー作成する。
    //将来課題:Qt Assistantでオンラインヘルプちゃんと作るべきだよなあ。
    //qrcから直接読み取れればよかったのになあ。。。
    QString helpfile = HELP_RESOURCE_NAMEBASE_JA;
    QString helpfile_en = HELP_RESOURCE_NAMEBASE_EN;
    QString helpfile_mail = HELP_RESOURCE_NAMEBASE_EMAIL;
    QFile localhtml(helpfile);
    QFile localhtml_en(helpfile_en);
    QFile localemail_png(helpfile_mail);
    QFile locallicense_jp(LICENSE_FILE_JP);
    QFile locallicense_en(LICENSE_FILE_EN);

    QFile existshtml(cfg.userDirV + "/" + LOCAL_HELPFILENAME_JA);
    QFile existshtml_en(cfg.userDirV + "/" + LOCAL_HELPFILENAME_EN);
    QFile existsmail(cfg.userDirV + "/" + LOCAL_HELPFILENAME_EMAIL);
    QFile existslicense_jp(cfg.userDirV + "/" + LOCAL_LICENSEFILE_JP);
    QFile existslicense_en(cfg.userDirV + "/" + LOCAL_LICENSEFILE_EN);

    //まだ存在していないか、既に存在してるヘルプファイルとサイズが異なれば上書きする。
    //変更したけど1バイトも変更してないって事もあるだろうけど、まあまずないでしょ。
    if(!existshtml.exists() || localhtml.size() != existshtml.size()){
        //削除しないとコピー成功しないのね
        existshtml.remove();
        localhtml.copy(QFileInfo(existshtml).absoluteFilePath());
    }
    if(!existshtml_en.exists() || localhtml_en.size() != existshtml_en.size()){
        existshtml_en.remove();
        localhtml_en.copy(QFileInfo(existshtml_en).absoluteFilePath());
    }
    if(!existsmail.exists()	|| localemail_png.size() != existsmail.size()){
        existsmail.remove();
        localemail_png.copy(QFileInfo(existsmail).absoluteFilePath());
    }
    if(!existslicense_jp.exists() || locallicense_jp.size() != existslicense_jp.size()){
        existslicense_jp.remove();
        locallicense_jp.copy(QFileInfo(existslicense_jp).absoluteFilePath());
    }
    if(!existslicense_en.exists() || locallicense_en.size() != existslicense_en.size()){
        existslicense_en.remove();
        locallicense_en.copy(QFileInfo(existslicense_en).absoluteFilePath());
    }

    cfg.PostReadIni();

    isErrLog = cfg.isErrLog;
    fileLogMode = cfg.fileLogMode;
    isReparse = cfg.isReparse;
    isLinkDest = cfg.isLinkDest;
    isReCreate = cfg.isReCreate;
    maxLinkHash = cfg.maxLinkHash;
    forceStart = cfg.forceStart;
    dotignoremode = cfg.Dotignore_mode;
    isTaskTray = FALSE;
    noConfirmDel = noConfirmStop = FALSE;
    skipEmptyDir = cfg.skipEmptyDir;
    diskMode = cfg.diskMode;
    isErrEditHide = FALSE;
    resultStatus = true;
    strcpyV(errLogPathV, cfg.errLogPathV);
    *fileLogPathV = 0;
    *fileCsvPathV = 0;
    fileLogDirV = cfg.fileLogDirV;
    hFileLog = SYSCALL_ERR;
    hcsvFileLog = SYSCALL_ERR;
    hStatLog = SYSCALL_ERR;
    copyInfo = NULL;
    finActIdx = 0;
    doneRatePercent = -1;
    lastTotalSec = 0;
    calcTimes = 0;
    errBufOffset = 0;
    listBufOffset = 0;
    csvfileBufOffset = 0;
    timerCnt = 0;
    timerLast = 0;
    mailsub.clear();
    mailbody.clear();
    //curPriority = ::GetPriorityClass(::GetCurrentProcess());
    // メインスレッドのスレッド優先度取得
    curPriority = QThread::currentThread()->priority();

    autoCloseLevel = NO_CLOSE;
    curIconIndex = 0;
    pathLogBuf = NULL;
    pathLogcsvBuf = NULL;
    isDelay = FALSE;
    isAbort = FALSE;
    hErrLog = SYSCALL_ERR;
    hErrLogMutex = NULL;
    memset(&ti, 0, sizeof(ti));

    //SIGNAL/SLOT初期化
    InitEv();
    //GUIオブジェクト初期化
    EvCreate();
}

//windows最小化、最大化時イベント出口
void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if(event->type() == QEvent::WindowStateChange){
        if(isMinimized()){
            TaskTray(NIM_ADD, hMainIcon[isDelay ? FCWAIT_ICON_IDX : FCNORMAL_ICON_IDX], FASTCOPY);
            //qDebug() << "minimized?";
        }
        else{
            //qDebug("taskbarMode = %d",cfg.taskbarMode);
            //if (cfg.taskbarMode){
                TaskTray(NIM_DELETE,NULL,NULL);
            //}
            //qDebug() << "normal or maximum?";
        }
    }
}

//汎用イベント出口
bool MainWindow::event(QEvent *event)
{
    // FastCopyEvent::EventId+XXがWM_APP+XX
    if (event->type() ==
        static_cast<QEvent::Type>(FastCopyEvent::EventId)){
        FastCopyEvent *fastcopyEvent = static_cast<FastCopyEvent*>(event);
        //Q_ASSERT(fastcopyEvent);

        switch(fastcopyEvent->uMsg){
        //post元からの要求イベント種別チェック
            case FastCopy::END_NOTIFY:
                /* debug
                printf(fastcopyEvent->message.toLocal8Bit().data(),"%s\n");
                fflush(stdout);
                */
                EndCopy();
                //コピー終了確定でキャンセルボタン押せなくする
                ui->pushButton_cancel_exec->hide();
                //verifyモード以外は実行ボタン復活
                if(GetCopyMode() != FastCopy::VERIFY_MODE){
                    ui->pushButton_Exe->show();
                }
                //Listingボタン復活
                ui->pushButton_Listing->show();
                //同時実行阻止ボタン隠す
                ui->pushButton_Atonce->hide();

                //ジョブリストモード有効？
                if(ui->actionEnable_JobList_Mode->isChecked()){
                    //ジョブリストモードで実行中？
                    if(joblistisstarting){
                        LaunchNextJob();
                    }
                    //ジョブリストモード有効なんだけど単体実行した？
                    else{
                        //ジョブリストモード中から選んでる状態じゃないことにする。
                        //お試し実行扱い
                        //でもこれだと、試しに変更して一回実行してみる->いい感じだなーこのまま保存しよっと。->え、できないの？
                        //になっちゃうよなあ。
                        //ui->listWidget_JobList->currentItem()->setSelected(false);

                        //CheckJobListModifiedするとなぜか空。。。なんで？
                        CheckJobListmodified();
                    }
                }
                break;
            case FastCopy::LISTING_NOTIFY:

                if (IsListing()){
                    SetListInfo();
                }
                else{
                    SetFileLogInfo();
                }
                break;
            case FastCopy::STAT_NOTIFY:
                WriteStatInfo();
                break;
            default:
                break;
        }
        return true;
    }
    return QMainWindow::event(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    //レポート出力中は終了禁止
    //課題:レポート処理自体が沈み込んだりしたらやばいけど、まあ殆どないでしょう。たぶん。

    if(!ui->actionSave_Report_at_Desktop->isEnabled()){
        QMessageBox msgbox;
        msgbox.setStandardButtons(QMessageBox::Ok);
        msgbox.setText((const char *)GetLoadStrV(IDS_REPORTING_NOT_CANCEL));
        msgbox.setWindowTitle(FASTCOPY);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.exec();
        event->ignore();
        return;
    }
    //コピーエンジン動作中じゃない？
    if (!fastCopy.IsStarting()) {
        event->accept();
    }
    //アボート状態？
    else if (fastCopy.IsAborting()) {

        QMessageBox::StandardButton cancel_btn;
        cancel_btn = QMessageBox::question(this,
                                 FASTCOPY,
                                 (const char *)GetLoadStrV(IDS_CONFIRMFORCEEND),
                                 QMessageBox::Yes|QMessageBox::No,
                                 QMessageBox::NoButton);

        if(cancel_btn == QMessageBox::Yes){
            /* GUIオブジェクト破棄->終了・・・？ */
            event->accept();
        }
        else{
            event->ignore();
            // なんもしない
        }
    }
    else {
        CancelCopy();
        autoCloseLevel = NOERR_CLOSE;
        /*
        if(ret){
            event->accept();
        }
        else{
            event->ignore();
        }
        */
        event->ignore();
    }
}

void MainWindow::timerEvent(QTimerEvent *timer_event){

    timerCnt++;
    if (isDelay) {
        SetStatusBarInfo(NULL,false);
        if (fastCopy.TakeExclusivePriv()) {

            QObject::killTimer(mw_TimerId);
            mw_TimerId = 0;
            isDelay = FALSE;
            ui->pushButton_Atonce->hide();
            ExecCopyCore();
        }
    }
    else {
        if (timerCnt & 0x1) UpdateSpeedLevel(TRUE); // check 500msec

        if (timerCnt == 1 || ((timerCnt >> cfg.infoSpan) << cfg.infoSpan) == timerCnt) {
            SetInfo();
        }
    }

    return;
}

BOOL MainWindow::SetMiniWindow(void)
{
    isErrEditHide = TRUE;
    return	TRUE;
}

BOOL MainWindow::SetupWindow()
{
    static BOOL once = FALSE;

    if (once)
        return TRUE;
    once = TRUE;
    if (cfg.isTopLevel){
        this->setWindowFlags(Qt::WindowStaysOnTopHint|Qt::X11BypassWindowManagerHint);
    }

    GetCopyMode() == FastCopy::DELETE_MODE ? SetItemEnable(true) :
                        GetCopyMode() == FastCopy::VERIFY_MODE ? SetItemEnable_Verify(true) :
                        SetItemEnable(false);

    SetMiniWindow();
    return	TRUE;
}


MainWindow::~MainWindow()
{

    delete ui;
}

void MainWindow::SetPathHistory_new(SetHistMode mode, QObject *item)
{
    if (GetCopyMode() == FastCopy::DELETE_MODE) {
        if(item == ui->plainTextEdit_Src){
            SetComboBox_new((QPlainTextEdit *)item,cfg.delPathHistory,mode);
        }
    }
    else {
        if(item == ui->plainTextEdit_Src){
            SetComboBox_new((QPlainTextEdit *)item,cfg.srcPathHistory,mode);
        }
        if(item == ui->plainTextEdit_Dst){
            SetComboBox_new((QPlainTextEdit *)item,cfg.dstPathHistory,mode);
        }
    }
}

void MainWindow::SetFilterHistory(SetHistMode mode, QComboBox *item)
{
    if (!item || item == ui->comboBox_include)  SetComboBox(ui->comboBox_include,  cfg.includeHistory,  mode);
    if (!item || item == ui->comboBox_exclude)  SetComboBox(ui->comboBox_exclude,  cfg.excludeHistory,  mode);
    if (!item || item == ui->comboBox_FromDate) SetComboBox(ui->comboBox_FromDate, cfg.fromDateHistory, mode);
    if (!item || item == ui->comboBox_ToDate)   SetComboBox(ui->comboBox_ToDate,   cfg.toDateHistory,   mode);
    if (!item || item == ui->comboBox_MinSize)  SetComboBox(ui->comboBox_MinSize,  cfg.minSizeHistory,  mode);
    if (!item || item == ui->comboBox_MaxSize)  SetComboBox(ui->comboBox_MaxSize,  cfg.maxSizeHistory,  mode);
}

void MainWindow::SetComboBox(QComboBox *item, void **history, SetHistMode mode)
{
    DWORD	len = 0;
    char	*wbuf = NULL;

    // backup editbox
    if (mode == SETHIST_LIST || mode == SETHIST_CLEAR) {
        len = strlen(item->currentText().toLocal8Bit().data()) + STR_NULL_GUARD;
        wbuf = new char[len];
        strncpy(wbuf,
                item->currentText().toStdString().c_str(),
                len);
    }
    // clear listbox & editbox
    item->clear();
    // set listbox
    if (mode == SETHIST_LIST || mode == SETHIST_INIT) {
        for (int i=0; i < cfg.maxHistory; i++) {
            if (GetChar(history[i], 0)){
                item->addItem(QString::fromUtf8((char*)history[i]));
            }
        }
    }
    // set editbox
    if (mode == SETHIST_EDIT) {
        if (cfg.maxHistory > 0 /* && ::IsWindowEnabled(GetDlgItem(item) */)
            item->addItem(QString::fromUtf8((char*)history[0]));
    }
    // restore editbox
    if (mode == SETHIST_LIST || mode == SETHIST_CLEAR) {
        item->setEditText(QString::fromUtf8(wbuf));
    }
    delete [] wbuf;
}

void MainWindow::SetComboBox_new(QPlainTextEdit *item, void **history, SetHistMode mode)
{
    DWORD	len = 0;
    char	*wbuf = NULL;
    QActionGroup *act_grp=NULL;
    QMenu		 *hist_menu=NULL;

    // backup editbox
    if (mode == SETHIST_LIST || mode == SETHIST_CLEAR) {
        //qDebug("add Item 0 %s",item->currentText().toLocal8Bit().data());
        len = strlen(item->toPlainText().toLocal8Bit().data()) + STR_NULL_GUARD;
        wbuf = new char[len];
        strncpy(wbuf,
                item->toPlainText().toStdString().c_str(),
                len);
    }
    //currentのtextをクリア
    //on_plainTextEdit_Src_textChanged will called
    item->clear();

    //qDebug() << "count=" << srchist_Group->actions().count();
    //履歴box内のアクションリスト(履歴)を全部クリア
    if(item == ui->plainTextEdit_Src){
        act_grp = srchist_Group;
        hist_menu = &src_HistoryMenu;
    }
    else{
        act_grp = dsthist_Group;
        hist_menu = &dst_HistoryMenu;
    }
    for(int i=act_grp->actions().count() - 1; i >= 0; i--){
        //qDebug() << "remove_tar=" << i << ":" << srchist_Group->actions().at(i)->text();
        hist_menu->removeAction(act_grp->actions().at(i));
        delete act_grp->actions().at(i);
        //qDebug() << "delete srchistActions:" << i << ":" << srchist_Group->actions().at(i)->text();
    }

    // set listbox
    if (mode == SETHIST_LIST || mode == SETHIST_INIT) {

        //先頭のアクションとしてクリアする専用のアクションを固定追加
        QAction *clearact = new QAction(act_grp);

        //クリア用文字列はboldとItalicで強調する
        QFont clearfont = item->font();
        clearfont.setItalic(true);
        clearfont.setBold(true);
        clearact->setFont(clearfont);

        clearact->setText((char*)GetLoadStrV(IDS_HIST_CLEAR));
        //qDebug() << "init:" << clearact->iconText();
        //IconTextに空""を入れたいんだけど、クラス仕様により許されないみたい。
        //hist_triggered側での認識文字として"."を使う。
        clearact->setIconText(DOT_V);

        for (int i=0; i < cfg.maxHistory; i++) {

            if (GetChar(history[i], 0)){

                //Actionの表示名はあくまで表示用のdispを登録しておく。
                //実際のフルパスとして使用するのはIconTextのほうよ
                QAction *initact = new QAction(act_grp);
                //QActionに複数大量のパスが含まれると、srchistをpopupした時に画面を全部埋めてしまう。
                //なので、textは妥当なサイズに切り詰めた省略文字を登録しておき、
                //フルパスとして使う実際のデータはIcontextの方を使用する。
                QString disp = (char*)history[i];			//表示専用のワークQString
                QFont font = item->font();
                QFontMetrics m(font);

                //パス内のSeparatorそのままだと見難いので、表示専用に空白に置換
                disp.replace(QChar::ParagraphSeparator," ");
                //大元のパスを表示した時の横合計pixelを取得
                int hitsorywidth = m.width(disp);
                //debug
                //qDebug() << "disp width=" << width;
                //qDebug() << "disp size=" << disp.size();
                //パス文字列合計がメインウインドウの横幅pixelの2倍以内に収まる？
                if(this->width() * 2 < hitsorywidth){
                    //収まらないのかあ
                    //pixelベースでカレントウインドウの横幅2倍以内分までに文字数を省略する
                    //メインウインドウの横幅px*2 / (本来の文字列表示に必要な横幅px / 合計文字数) = 一文字辺りのpx平均値
                    disp.truncate(this->width() * 2 / (hitsorywidth / disp.size()));
                    //縮めはしちゃったけど、後続あるので...表記を追加
                    disp.append("...");

                    //qDebug() << "trunc str=" << disp;
                }
                //収まりそうだからそのまま表示しちゃえばいいよね。
                else{
                    //qDebug() << "nontruncate:" << disp;
                }
                //action表示用の見た目textを登録
                initact->setText(disp);
                //icontextにデータとして使うフルパスを格納
                initact->setIconText((char*)history[i]);
                //qDebug() << "setdataindex=" << i << "data=" << initact->iconText();
            }
        }
        hist_menu->addActions(act_grp->actions());
    }
    //set editbox
    //SETHIST_EDITは廃止
    /*
    if (mode == SETHIST_EDIT) {
        if (cfg.maxHistory > 0 && ::IsWindowEnabled(GetDlgItem(item)){

            //item->addItem((char*)history[0]);
            //item->addItem(QString::fromUtf8((char*)history[0]));
            QAction *editact = new QAction((char*)history[0],srchist_Group);
        }
    }
    */
    // restore editbox
    if (mode == SETHIST_LIST || mode == SETHIST_CLEAR) {
        /*
        SetDlgItemTextV(item, wbuf);
        */
        //on_plainTextEdit_Src_textChanged will called
        item->setPlainText(QString::fromUtf8(wbuf));
    }
    delete [] wbuf;
}

BOOL MainWindow::SetCopyModeList(void)
{
    int	idx = cfg.copyMode;
    QString selectedname;
    if(copyInfo == NULL) {		// 初回コピーモードリスト作成
        for (int i=0; COPYINFO_LIST[i].resId; i++) {
            COPYINFO_LIST[i].list_str = (char*)GetLoadStrV(COPYINFO_LIST[i].resId);
            COPYINFO_LIST[i].cmdline_name = GetLoadStrV(COPYINFO_LIST[i].cmdline_resId);
        }
        copyInfo = new CopyInfo[sizeof(COPYINFO_LIST) / sizeof(CopyInfo)];
    }
    else{
        //カレントの選択肢を退避
        idx = ui->comboBox_Mode->currentIndex();
        //カレントindexから、現在のモード名称取得しておく
        selectedname = ui->comboBox_Mode->itemText(idx);

        //modeコンボボックスの項目リセット
        ui->comboBox_Mode->clear();
    }

    CopyInfo *ci = copyInfo;

    for (int i=0; COPYINFO_LIST[i].resId; i++) {
        if ((cfg.enableMoveAttr && COPYINFO_LIST[i].resId == IDS_MOVEFORCE
        )|| (!cfg.enableMoveAttr && COPYINFO_LIST[i].resId == IDS_MOVEATTR)) {
            continue;
        }
        //LTFSチェック時は日付更新不可のため日付で判定するコピーモードを全て登録しないようにしとく
        if(ui->checkBox_LTFS->isChecked() &&
            (COPYINFO_LIST[i].resId == IDS_ATTRCMP ||
             COPYINFO_LIST[i].resId == IDS_UPDATECOPY ||
             COPYINFO_LIST[i].resId == IDS_SYNCCOPY ||
             COPYINFO_LIST[i].resId == IDS_MOVEATTR
             /*COPYINFO_LIST[i].resId == IDS_VERIFY*/)){
            continue;
        }
        else if(!ui->checkBox_LTFS->isChecked() &&
                (COPYINFO_LIST[i].resId == IDS_SIZESKIP
                 /*COPYINFO_LIST[i].resId == IDS_SIZEVERIFY*/)){
            continue;
        }
        *ci = COPYINFO_LIST[i];
        //mode指定用のコンボボックス選択肢を追加。
        //on_comboBox_Mode_currentIndexChanged will called
        ui->comboBox_Mode->addItem(ci->list_str);
        ci++;
    }
    memset(ci, 0, sizeof(CopyInfo));	// terminator

    //改めて選択しなおしするぞ

    //現在のコピーモードで選択可能なモード名称から選択前の名称と一致するやつあるか検索する
    ci = copyInfo;
    int returnindex = 0;
    //現在選べるモードぶんぐーるぐる
    while(ci->resId){
        //もともと選んでたやつと名称一致する？
        if(selectedname.compare(ci->list_str) == 0){
            //当たりなのでこいつを選択して抜ける
            ui->comboBox_Mode->setCurrentIndex(returnindex);
            break;
        }
        returnindex++;
        ci++;
    }
    //再設定できるはずだったのに誰も名称一致しなかった = 当該モードは選択できない状態だに
    if(!ci->cmdline_resId){
        //仕方ないのでデフォルト値にしとく
        ui->comboBox_Mode->setCurrentIndex(cfg.copyMode);
    }
    return	TRUE;
}

BOOL MainWindow::CancelCopy()
{

    //タイマキャンセル済み？
    if(mw_TimerId != 0){
        QObject::killTimer(mw_TimerId);
        mw_TimerId = 0;
    }

    if (!isDelay) fastCopy.Suspend();

    /* 本当に止めていいかどうか聞く */
    QMessageBox::StandardButton ret;
    ret = QMessageBox::question(this,
                             FASTCOPY,
                             (const char *)GetLoadStrV(IsListing() ? IDS_LISTCONFIRM :
                              info.mode == FastCopy::DELETE_MODE ? IDS_DELSTOPCONFIRM : IDS_STOPCONFIRM),
                             QMessageBox::Yes|QMessageBox::No,
                             QMessageBox::NoButton);

    if (isDelay) {
        if (ret == QMessageBox::Yes) {
            isDelay = FALSE;
            EndCopy();
        }
        else {
            mw_TimerId = QObject::startTimer(FASTCOPY_TIMER_TICK,Qt::CoarseTimer);
        }
    }
    else {
        fastCopy.Resume();

        if (ret == QMessageBox::Yes) {
            isAbort = TRUE;
            fastCopy.Aborting();
        }
        else {
            mw_TimerId = QObject::startTimer(FASTCOPY_TIMER_TICK,Qt::CoarseTimer);
        }
    }

    return	ret == QMessageBox::Yes ? TRUE : FALSE;
}


BOOL MainWindow::IsForeground()
{

    //HWND hForeWnd = GetForegroundWindow();
    QWidget* hForeWnd = QApplication::activeWindow();

    //return	hForeWnd && hWnd && (hForeWnd == hWnd || ::IsChild(hWnd, hForeWnd)) ? TRUE : FALSE;
    return hForeWnd ? true : false;
}

BOOL MainWindow::EvCreate()
{
    char	wk_char[256];		// int->ASCII変換用のワーク

    //Topボタンをon/offボタンにする
    ui->pushButton_Top->setCheckable(true);

    //Window前面固定貼り付け？チェックと設定
    ui->pushButton_Top->setChecked(cfg.isTopLevel);

    Qt::WindowFlags flags = this->windowFlags();

    if(ui->pushButton_Top->isChecked()){
        flags |= Qt::X11BypassWindowManagerHint;
        flags |= Qt::WindowStaysOnTopHint;
        this->setWindowFlags(flags);
        //this->show();
    }

    //何故か内部値のautocompleteのデフォルトがノンケースセンシティブなので封印。、なんでデフォルト意識しないなのか。。
    ui->comboBox_include->setAutoCompletionCaseSensitivity(Qt::CaseSensitive);
    //ui->comboBox_include->setAutoCompletion(false);
    ui->comboBox_exclude->setAutoCompletionCaseSensitivity(Qt::CaseSensitive);

    //アイコンロード
    int i;
    for (i=0; i < MAX_FASTCOPY_ICON; i++) {
        QString iconname = QString("%1%2%3%4").arg(
                                   ICON_RESOURCE_NAMEBASE,
                                   FASTCOPY,
                                   QString::number(i),
                                   ICON_RESOURCE_EXTNAME);
        hMainIcon[i] = new QIcon(iconname);
    }
    //Linux版の場合はWindowIcon自前でセット
    this->setWindowIcon(*hMainIcon[FCNORMAL_ICON_IDX]);

    // メッセージセット
    ui->textEdit_STATUS->setText((char*)GetLoadStrV(IDS_BEHAVIOR));
    sprintf(wk_char,"%d",cfg.bufSize);
    ui->lineEdit_BUFF->setText(wk_char);
    default_palette = ui->textEdit_PATH->palette();

    SetCopyModeList();
    UpdateMenu();

    ui->checkBox_Nonstop->setChecked(cfg.ignoreErr);
    ui->checkBox_Estimate->setChecked(cfg.estimateMode);

    verify_before = cfg.enableVerify;
    ui->checkBox_Verify->setChecked(cfg.enableVerify);

    ui->checkBox_Acl->setChecked(cfg.enableAcl);
    ui->checkBox_xattr->setChecked(cfg.enableXattr);

    ui->checkBox_owdel->setChecked(cfg.enableOwdel);

    ui->checkBox_LTFS->setChecked(cfg.enableLTFS);
    speedLevel = cfg.speedLevel;
    SetSpeedLevelLabel(speedLevel);

    //作者曰く:
    //SetPathHistoryはFastCopyのhistory動的作成（＝見ていないときは空）には理由があって、
    //WindowsのCOMBOBOXはリサイズするとEDIT部がhistory（に先頭一致するエントリ）で
    //勝手に文字列補完されてしまうので、これを避けるためだったりします。
    //とのこと。設定できるタイミングで勝手にぶっ込んでいいかも？

    ui->plainTextEdit_Dst->setWordWrapMode(QTextOption::NoWrap);
    ui->plainTextEdit_Dst->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    ui->pushButton_Dsthist->setMenu(&dst_HistoryMenu);

    SetPathHistory_new(SETHIST_INIT,ui->plainTextEdit_Dst);

    ui->plainTextEdit_Src->setWordWrapMode(QTextOption::NoWrap);
    ui->plainTextEdit_Src->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    ui->pushButton_Srchist->setMenu(&src_HistoryMenu);
    //pushbutton
//	ui->pushButton_Srchist->setPopupMode(QToolButton::MenuButtonPopup);
    SetPathHistory_new(SETHIST_INIT,ui->plainTextEdit_Src);

    SetFilterHistory(SETHIST_LIST);

    //statusbar
    //デフォルトだと右下にGUIオブジェクトを配置しちゃうのを変更
    ui->statusBar->setSizeGripEnabled(false);
    //statusBarの表示方向を右から左に変更？
    //http://www.qtcentre.org/threads/18430-status-bar-dispaly
    ui->statusBar->setLayoutDirection(Qt::RightToLeft);
    //statusBarの表示文字列位置を左はしから5pixel右方向に移動
    ui->statusBar->setContentsMargins(5,0,0,0);
    //statusBar自身のメソッドは一時的な表示用なので、labelを永久登録して
    //label上の表示文字列を変更することで状態表示を行う
    ui->statusBar->addPermanentWidget(&status_label);

    //デフォルトOFFで隠しておく
    ui->filter_widget->hide();
    ui->exfilter_widget->hide();

    //cfgで常時表示有効ならメニュー項目にチェックつける
    //フィルタ
    ui->actionShow_Extended_Filter->setChecked(cfg.isExtendFilter);

    if (isTaskTray) {
        TaskTray(NIM_ADD, hMainIcon[FCNORMAL_ICON_IDX], FASTCOPY);
    }

    //SRC及びDSTへのD&D対応のためのイベントハンドル登録
    this->setAcceptDrops(true);

    //初期状態ではキャンセルボタンは存在するけどかくしておくのよ
    ui->pushButton_cancel_exec->hide();

    ui->progressBar_exec->setRange(0,100);
    ui->progressBar_exec->setValue(0);

    // ProgressBarは初期状態では隠しておく
    ui->progress_widget->hide();
    this->resize(this->width(),this->height() - ui->progress_widget->height());

    //エラー情報表示用領域は起動時は隠しておく
    ui->err_widget->hide();
    this->resize(this->width(),this->height() - ui->err_widget->height());
    isErrEditHide = true;
    ui->checkBox_owdel->hide();
    //強制同時実行用のボタンは初期状態では隠しておく
    ui->pushButton_Atonce->hide();

    if(!IS_INVALID_POINT(cfg.macpos.x(),cfg.macpos.y())){
        this->move(cfg.macpos);
        ui->actionWindowPos_Fix->setChecked(true);
    }
    if(IS_INVALID_SIZE(cfg.macsize.width(),cfg.macsize.height())){
        //初期状態でのベターなサイズはこんくらいなので強制resize
        //課題：エラーウィジェット発生の繰り返しでサイズ徐々に小さくなっちゃうバグあり。
        //yが540以上あるとずれなくなる。ナンデー？！
        this->resize(410,570);
    }
    else{
        this->resize(cfg.macsize);
        ui->actionWindowSize_Fix->setChecked(true);
    }

    //ジョブリスト関連初期化
    ui->comboBox_JobListName->setMaxCount(JOBLISTNAME_MAX - 1);
    joblistisstarting = false;
    joblist_joberrornum = 0;
    joblist_cancelsignal = true;
    if(cfg.isJobListMode){
        //Pro版の場合はcfgで設定する
        ui->actionEnable_JobList_Mode->setChecked(cfg.isJobListMode);
    }
    else{
        ui->central_batch_widget->hide();
    }

    //Shell Extension機能は未実装だよ
    ui->actionShell_Extension->setVisible(false);

    //コマンドラインモード
    //要素先頭にはアプリケーション名入ってるので、先頭要素は除外
    if (orgArgc > 1) {

        //Log記録用に起動要因を記憶
        isCommandStart = true;

        if (!CommandLineExecV(orgArgc, orgArgv) && (autoCloseLevel >= NOERR_CLOSE)){
            if(IsForeground() && QApplication::queryKeyboardModifiers() & Qt::ShiftModifier){
                autoCloseLevel = NO_CLOSE;
            }
            else{
                // 正の整数 = エラーが一個でも発生した場合はエラーファイルの個数
                // 0 = 正常終了
                this->close();
                if(ti.total.errFiles || ti.total.errDirs){
                    exit(FASTCOPY_ERROR_EXISTS);
                }
                else{
                    exit(FASTCOPY_OK);
                }
            }
        }
    }
    else{
        //Log記録用に起動要因を記憶
        isCommandStart =  false;
        this->Show();
        isRunAsshow = true;
    }
    return	TRUE;
}


BOOL MainWindow::SwapTargetCore(const void *s, const void *d, void *out_s, void *out_d)
{
    char	*src_fname = NULL;//, *dst_fname = NULL;
    BOOL	isSrcLastBS = GetChar(s, strlenV(s) - 1) == '/'; // 95系は無視...
    BOOL	isDstLastBS = GetChar(d, strlenV(d) - 1) == '/';
    BOOL	isSrcRoot = FALSE;

    VBuf	buf(MAX_PATHLEN_V * CHAR_LEN_V);
    VBuf	srcBuf(MAX_PATHLEN_V * CHAR_LEN_V);

    QFileInfo Qs((char*)s);
    QFileInfo Qd((char*)d);
    QString wk_s_str(Qs.absoluteFilePath());
    QString wk_d_str(Qd.absoluteFilePath());
    QString file_name;

    if (!buf.Buf() || !srcBuf.Buf()) return	FALSE;

    if (isSrcLastBS) {
        strcpy((char*)s, (char*)buf.Buf());
        if (strcmpV(s, buf.Buf()) == 0) {
            isSrcRoot = TRUE;
        }
        else {
            strcpyV(srcBuf.Buf(), s);
            SetChar(srcBuf.Buf(), strlenV(srcBuf.Buf()) -1, 0);
            s = srcBuf.Buf();
        }
    }

    struct stat attr;
    int rc = 0;

    BOOL	 isSrcDir = isSrcLastBS || (rc = lstat((char*)s,&attr)) == SYSCALL_ERR
              || (((attr.st_mode & S_IFMT) == S_IFDIR) && (!cfg.isReparse || (attr.st_mode & S_IFMT) != S_IFLNK));

    if (isSrcDir && !isDstLastBS) {	// dst に '\\' がない場合
        strcpyV(out_d, s);
        strcpyV(out_s, d);
        goto END;
    }

    if (!isDstLastBS) {	// dst 末尾に '\\' を付与
        MakePathV(buf.Buf(), d, EMPTY_STR_V);
        d = buf.Buf();
    }

    //srcを出力予定のout_d領域にコピー
    if(snprintf((char*)out_d,
                MAX_PATHLEN_V + STR_NULL_GUARD
                ,"%s",
                wk_s_str.toLocal8Bit().data()) <= 0){
        return false;
    }
    //dstを出力予定のout_s領域にコピー
    if(snprintf((char*)out_s,
                MAX_PATHLEN_V + STR_NULL_GUARD,
                "%s",
                wk_d_str.toLocal8Bit().data()) <= 0){
        return false;
    }

    //srcの元領域から末尾ファイル名だけピンポイントに抽出
    file_name = QFileInfo(wk_s_str).fileName();

    if(!(file_name.isEmpty())){ // a:\aaa\bbb  d:\ccc\ddd\ --> d:\ccc\ddd\bbb a:\aaa\ にする
        //出力予定のout_sのけつにファイル名を付与
        strcpyV(MakeAddr(out_s, strlenV(out_s)), file_name.toLocal8Bit().data());
        //出力予定のout_dのけつのファイル名をけずる
        src_fname = (char*)out_d + strlen((char*)out_d) - strlen(file_name.toLocal8Bit().data());
        SetChar(src_fname, 0, 0);
        goto END;
    }

    else if(isSrcRoot){	// a:\  d:\xxx\  -> d:\xxx a:\ にする
        strcpy((char*)out_s, (char*)buf.Buf());
        if(strcmpV(out_s, buf.Buf()) && !isSrcRoot){
            SetChar(out_s, strlenV(out_s) -1, 0);
        }
        goto END;
    }else{
        return	FALSE;
    }

END:
    PathArray	srcArray;
    if(!srcArray.RegisterPath(out_s)){
        return	FALSE;
    }
    srcArray.GetMultiPath(out_s, MAX_PATHLEN_V);
    return	TRUE;
}

BOOL MainWindow::SwapTarget(BOOL check_only)
{

    int		src_len = GetSrclen() + STR_NULL_GUARD;
    int		dst_len = GetDstlen() + STR_NULL_GUARD;

    if (src_len == 0 && dst_len == 0){
        return FALSE;
    }
    //大量のパスを放り込まれたとき、計算でフリーズしかねないのでで事前に弾く！！
    //srcがMAX_PATH以上の文字数?
    else if(src_len > MAX_PATH+STR_NULL_GUARD){
        //qDebug() << "oosugi return.";
        //MAX_PATH以上のパス入力=複数パスなので交換不能なので即リターン
        return false;
    }
    //MAX_PATH以下のパス長なんだけど・・・。
    else if(src_len < MAX_PATH+STR_NULL_GUARD){
        //改行を含んでいる？
        if(GetSrc().contains(QChar::ParagraphSeparator)){
            //複数のパス入力時は交換不能だよ
            return false;
        }
    }

    void		*src = new char [MAX_PATHLEN_V + STR_NULL_GUARD];
    void		*dst = new char [MAX_PATHLEN_V + STR_NULL_GUARD];
    PathArray	srcArray, dstArray;
    BOOL		ret = FALSE;

    if (src && dst && (snprintf((char*)src,
                       MAX_PATHLEN_V + STR_NULL_GUARD,
                       "%s",
                       GetSrc().toLocal8Bit().data()) + STR_NULL_GUARD) == src_len
                       &&
                       (snprintf((char*)dst,
                       MAX_PATHLEN_V + STR_NULL_GUARD,
                       "%s",
                       GetDst().toLocal8Bit().data()) + STR_NULL_GUARD) == dst_len){
        if(srcArray.RegisterMultiPath(src) <= 1 && dstArray.RegisterPath(dst) <= 1
                && !(srcArray.Num() == 0 && dstArray.Num() == 0)) {
            ret = TRUE;
            if(!check_only){
                if(srcArray.Num() == 0){
                    dstArray.GetMultiPath(src, MAX_PATHLEN_V);
                    SetChar(dst, 0, 0);
                }
                else if(dstArray.Num() == 0){
                    SetChar(src, 0, 0);
                    strcpyV(dst, srcArray.Path(0));
                }
                else{
                    ret = SwapTargetCore(srcArray.Path(0), dstArray.Path(0), src, dst);
                }
                if(ret){
                    ui->plainTextEdit_Src->setPlainText((char*)src);
                    ui->plainTextEdit_Dst->setPlainText((char*)dst);
                }
            }
        }
    }

    delete [] dst;
    delete [] src;
    return	ret;
}

// MainWindowのGUI->clicked()シグナルを受け取るスロット関数共通入り口
BOOL MainWindow::EvClicked()
{
    bool start_error;
    // Execute or listing
    if(ui->pushButton_Exe == QObject::sender() || ui->pushButton_Listing == QObject::sender()){

        int exec_flag = ui->pushButton_Exe == QObject::sender() ? NORMAL_EXEC :
                         LISTING_EXEC | (GetCopyMode() == FastCopy::VERIFY_MODE ? VERIFYONLY_EXEC : 0);
        if (!fastCopy.IsStarting() && !isDelay) {

            //ジョブリストモード実行中にリスティングまたは実行でコピー実行する場合は
            //ジョブ内容変更検知用シグナルをキャンセルする
            if(ui->actionEnable_JobList_Mode->isChecked() && ui->listWidget_JobList->currentRow() != -1){
                joblist_cancelsignal = true;
                //変更中を示すフォントを強制で元に戻す。
                QListWidgetItem *item = ui->listWidget_JobList->item(ui->listWidget_JobList->currentRow());
                QFont font = item->font();
                font.setItalic(false);
                font.setBold(false);
                item->setFont(font);
                //カレントジョブ変更未保存フラグをリセット
                //(変更検知させない)
                joblist_isnosave = false;
            }
            //コピー開始
            start_error = ExecCopy(exec_flag);

            if(start_error == true){
                //実行ボタンを隠す
                ui->pushButton_Exe->hide();
                //Listingボタンを隠す
                ui->pushButton_Listing->hide();

                //キャンセルボタンの座標をExeの位置に移動
                ui->pushButton_cancel_exec->move(
                            ui->pushButton_Exe->pos().x(),
                            ui->pushButton_Exe->pos().y());
                // キャンセルボタンを表示する
                ui->pushButton_cancel_exec->show();
            }
            //コピー開始失敗
            else{
                //ジョブリストモードで実行中？
                if(joblistisstarting){

                    joblist_joberrornum++;
                    ti.total.errFiles++;
                    //Display Error.
                    SetJobListInfo(&ti,true);
                    if(IsSendingMail(false)){
                        WriteMailLog();
                    }
                    if(IsLastJob()){
                        ExecFinalAction();
                    }
                    LaunchNextJob();
                }
            }
        }
        else if(CancelCopy()){
            autoCloseLevel = NO_CLOSE;
        }
        return true;
    }

    //ジョブリストにジョブ追加？
    else if(ui->pushButton_JobExe == QObject::sender() || ui->pushButton_JobList == QObject::sender()){

        int	 idx = (int)ui->comboBox_Mode->currentIndex();
        bool is_delete_mode = copyInfo[idx].mode == FastCopy::DELETE_MODE;
        int exec_flag = ui->pushButton_JobExe == QObject::sender() ? NORMAL_EXEC :
                         LISTING_EXEC | (GetCopyMode() == FastCopy::VERIFY_MODE ? VERIFYONLY_EXEC : 0);
        //deleteモード時はdstの中身気にしないよ
        if(ui->plainTextEdit_Src->toPlainText().isEmpty()
                || (!is_delete_mode && ui->plainTextEdit_Dst->toPlainText().isEmpty())){
            //through return
            return true;
        }
        else if(joblist.count() > JOBLIST_JOBMAX){
            //max over error
            QMessageBox msgbox;
            msgbox.setText((char*)GetLoadStrV(IDS_JOBLISTMAX_ERROR));
            msgbox.setStandardButtons(QMessageBox::Ok);
            msgbox.setIcon(QMessageBox::Warning);
            msgbox.exec();
            return true;
        }
        //ワークのジョブ名称を作成(Job_20160229_120000111)
        QDateTime dt = QDateTime::currentDateTime();
        QString jobname_date = ui->pushButton_JobExe == QObject::sender() ? "Job_" : "Listup_";
        jobname_date.append(dt.toString("yyyyMMdd_hhmmsszzz"));

        //ジョブリスト名が空欄状態ならワークのジョブリスト名称を作成
        if(!joblist.count() && ui->comboBox_JobListName->currentText().isEmpty()){
            ui->comboBox_JobListName->setCurrentText("JobList_" + dt.toString("yyyyMMdd_hhmmss"));
        }
        Job	wk_job;
        //ジョブ内容の一部を現在の画面設定から取得
        GetJobfromMain(&wk_job);
        //フィルタ類の内容を取得
        wk_job.SetString(jobname_date.toLocal8Bit().data(),
                ui->plainTextEdit_Src->toPlainText().toLocal8Bit().data(),
                copyInfo[ui->comboBox_Mode->currentIndex()].mode != FastCopy::DELETE_MODE ?
                 ui->plainTextEdit_Dst->toPlainText().toLocal8Bit().data() : (void*)"",
                copyInfo[ui->comboBox_Mode->currentIndex()].cmdline_name,
                wk_job.isFilter ? ui->comboBox_include->currentText().toLocal8Bit().data() : (void*)"",
                wk_job.isFilter ? ui->comboBox_exclude->currentText().toLocal8Bit().data() : (void*)"",
                wk_job.isFilter ? ui->comboBox_FromDate->currentText().toLocal8Bit().data() : (void*)"",
                wk_job.isFilter ? ui->comboBox_ToDate->currentText().toLocal8Bit().data() : (void*)"",
                wk_job.isFilter ? ui->comboBox_MinSize->currentText().toLocal8Bit().data() : (void*)"",
                wk_job.isFilter ? ui->comboBox_MaxSize->currentText().toLocal8Bit().data() : (void*)"");
        wk_job.isListing = exec_flag & LISTING_EXEC ? true : false;
        wk_job.flags |= Job::JOBLIST_JOB;
        //ワークのジョブをワークジョブリストに追加
        joblist.append(wk_job);
        QListWidgetItem *item = new QListWidgetItem((char*)joblist.last().title);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        ui->listWidget_JobList->addItem(item);
        UpdateJobListtoini();
    }

    // Cancel
    else if(ui->pushButton_cancel_exec == QObject::sender()){

        CancelCopy();
        autoCloseLevel = NO_CLOSE;
        return	TRUE;
    }

    // コピー強制実行
    else if(ui->pushButton_Atonce == QObject::sender()){

        isDelay = FALSE;
        //実行待ちタイマをキャンセル
        QObject::killTimer(mw_TimerId);
        mw_TimerId = 0;
        //強制実行しちゃうのでAt onceボタン隠す
        ui->pushButton_Atonce->hide();
        //Copy強制実行しちゃうので、実行権は取れたことにする
        ExecCopyCore();
        return	TRUE;
    }

    // Srcボタン
    else if(ui->pushButton_Src == QObject::sender()){

        QFileDialog src_dialog(this);
        QStringList src_strlist;
        QString append_str;
        bool ctrl_pressed;

        src_dialog.setFileMode(QFileDialog::Directory);
        src_dialog.setOption(QFileDialog::DontUseNativeDialog,true);

        //ファイルダイアログでフォルダとファイルを同時選択するためのおまじない。
        QListView *l = src_dialog.findChild<QListView*>("listView");
        if(l){
            l->setSelectionMode(QAbstractItemView::MultiSelection);
        }
        QTreeView *t = src_dialog.findChild<QTreeView*>();
        if(t){
            t->setSelectionMode(QAbstractItemView::MultiSelection);
        }
        //キャンセルしてたらおしまい
        if(!src_dialog.exec()){
            return true;
        }
        src_strlist = src_dialog.selectedFiles();
        ctrl_pressed = QApplication::queryKeyboardModifiers() & Qt::ControlModifier;
        if(!src_strlist.isEmpty()){
            if(ctrl_pressed){
                append_str.append(QChar::ParagraphSeparator);
            }
            for(int i=0; i < src_strlist.count();i++){
                append_str.append(src_strlist.at(i));
                if(i+1 < src_strlist.count()){
                    append_str.append(QChar::ParagraphSeparator);
                }
            }
            //フィールドへの入力と整形、表示依頼
            castPath(ui->plainTextEdit_Src,append_str,src_strlist.count(),ctrl_pressed);
            CheckJobListmodified();
        }
    }
    // Dstボタン
    else if(ui->pushButton_Dst == QObject::sender()){

        QFileDialog dst_dialog(this);
        QString dststr;

        dststr = dst_dialog.getExistingDirectory(
                    this,
                    (char*)GetLoadStrV(IDS_DST_SELECT),
                    QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                    QFileDialog::ShowDirsOnly);

        if(!dststr.isNull()){
            ui->plainTextEdit_Dst->setPlainText(dststr + "/");
            CheckJobListmodified();
        }

        return true;
    }

    //help
    else if(ui->pushButton_help == QObject::sender()){
        on_actionHelp_triggered();
        return true;
    }
    //Top
    else if(ui->pushButton_Top == QObject::sender()){
        Qt::WindowFlags flags = this->windowFlags();
        if(ui->pushButton_Top->isChecked()){
            //最前面固定すると、何故かウインドウのタイトルバー消えちゃってウインドウ位置異動できないなあ。。
            //仕方ないけどとりあえず仕様
            flags |= Qt::X11BypassWindowManagerHint;
            flags |= Qt::WindowStaysOnTopHint;
            this->setWindowFlags(flags);
            this->show();
        }
        else{
            flags &= ~Qt::X11BypassWindowManagerHint;
            flags &= ~Qt::WindowStaysOnTopHint;
            this->setWindowFlags(flags);
            this->show();
        }
        cfg.isTopLevel = ui->pushButton_Top->isChecked();
        cfg.WriteIni();
    }
    //JoblistStart
    else if(ui->pushButton_JobListStart == QObject::sender()){

        if(joblist.count() && !joblistisstarting){

            joblistcurrentidx = 0;
            joblist_joberrornum = 0;
            SetJob(joblistcurrentidx,true,true);

            ui->listWidget_JobList->clearFocus();

            //リスト上のジョブ名をいっかい全部引き上げる
            QList<QString> wk_strings;
            for(int i=0;i < ui->listWidget_JobList->count();i++){
                //一回文字列だけとりかえす
                wk_strings.append(ui->listWidget_JobList->item(i)->text());
            }
            //Icon表示リセットのため一回全部クリア
            ui->listWidget_JobList->clear();
            //ジョブ名Itemを再登録
            ui->listWidget_JobList->addItems(wk_strings);
            //なんらかのアイテムをクリックしてるとジョブリストの進行状況わかりにくいので
            //選択してたら強制解除
            ui->listWidget_JobList->clearSelection();

            joblistisstarting = true;
            joblistiscancel = false;
            //ジョブリスト周りの機能を触れないようにする。
            EnableJobList(false);
            EnableJobMenus(false);
            //JObListExecuteボタンをキャンセルボタンに意味かえる
            ui->pushButton_JobListStart->setText((char*)GetLoadStrV(IDS_JOBLIST_CANCEL));
            //JobListの実行時は他のRapidCopyインスタンス待ちは強制無効化する。
            forceStart = 1;
            joblist[joblistcurrentidx].isListing ?
                        ui->pushButton_Listing->click() : ui->pushButton_Exe->click();
        }
        else if(joblistisstarting){
            //キャンセルフラグを立ててnextジョブの起動抑止する
            joblistiscancel = true;
            //カレントのJobを停止させる
            ui->pushButton_cancel_exec->click();
        }
    }

    return	FALSE;
}

//エラー発生時の応答用スロット出口
FastCopy::Confirm MainWindow::ConfirmNotify(FastCopy::Confirm confirm){

    QMessageBox errorBox;				//エラー発生応答用ダイアログ
    bool		silent = isJobListwithSilent();
                                        //ジョブリストモードかつ次のジョブの強制起動の場合はダイアログ表示しない?

    bool		jobliststop = (joblistisstarting && !ui->checkBox_JobListForceLaunch->isChecked());
                                        //ジョブリストモードかつ次のジョブ起動しないで全体ストップ？
    int ret;                            //ダイアログ選択値
    //クリティカルエラー
    if(!confirm.allow_continue){
        //オリジナルでは表示されるけどグレーアウトする仕様なんだよねえ。
        //結果的にユーザが取れる行動は同じだからいいんだけど、。
        errorBox.setText(tr("Critical Error Occured. Can't continue..."));
        errorBox.setInformativeText(tr("Detail: ") + (QString)((char*)confirm.message));
        errorBox.setStandardButtons(QMessageBox::Ok);
        errorBox.setDefaultButton(QMessageBox::Ok);
        errorBox.setIcon(QMessageBox::Critical);
    }
    //
    else{
        errorBox.setText(tr("Error Occured. Continue?"));
        //errorBox.setInformativeText(tr("Continue?"));
        errorBox.setInformativeText(tr("Detail: ") + (QString)((char*)confirm.message));
        errorBox.setStandardButtons(QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::Cancel);
        errorBox.setDefaultButton(QMessageBox::Cancel);
        errorBox.setIcon(QMessageBox::Warning);
    }
    if(silent){
        //ジョブリストモードで次を強制実行する場合は無条件にok押したことにする。
        //じゃないと先に進まないじゃんね
        ret = QMessageBox::Ok;
    }
    else{
        ret = errorBox.exec();
    }

    switch(ret){
        //続行？
        case QMessageBox::Yes:
                confirm.result = FastCopy::Confirm::CONTINUE_RESULT;
                break;
        //全部続行？
        case QMessageBox::YesToAll:
                confirm.result = FastCopy::Confirm::IGNORE_RESULT;
                break;
        //クリティカルエラー？
        case QMessageBox::Ok:
                //次ジョブ強制起動が無効？
                if(jobliststop){
                    //次のジョブは起動せずに中断するよ。。
                    joblistiscancel = true;
                }
        //コピー実行をストップ
        case QMessageBox::Cancel:
             confirm.result = FastCopy::Confirm::CANCEL_RESULT;
             break;
        default:
             ui->textEdit_ERR->setText("Internal Error.");
             break;
    }
    return confirm;
}


//FastCopyの1コピー実行終了時の後始末処理関数
BOOL MainWindow::EndCopy(void)
{
    SetPriority(QThread::NormalPriority);

    //タイマキャンセル済み？
    if(mw_TimerId != 0){
        QObject::killTimer(mw_TimerId);
        mw_TimerId = 0;
    }

    BOOL	is_starting = fastCopy.IsStarting();

    if(is_starting){
        SetInfo(TRUE);
        if(ti.total.errFiles == 0 && ti.total.errDirs == 0){
            resultStatus = true;
        }
        else{
            resultStatus = false;
            if(joblistisstarting){
                joblist_joberrornum++;
            }
        }

        //終了時処理でメール送信予定あり？
        if(IsSendingMail(true)){
            //Mail送信用データ準備
            WriteMailLog();
        }

        if(isErrLog) WriteErrLog();
        if(fileLogMode != Cfg::NO_FILELOG)EndFileLog();

        EndStatLog();

        //一時的にコピー実行ボタン押せないようにしとく。
        IsListing() ?
         ui->pushButton_Listing->setEnabled(false) : ui->pushButton_Exe->setEnabled(false);

        //コピーエンジン側の終了処理
        fastCopy.End();

        //終わったんでまた押せるよにする。
        IsListing() ?
         ui->pushButton_Listing->setEnabled(true) : ui->pushButton_Exe->setEnabled(true);

        //srcのパス履歴とdstのパス履歴を最新に更新する
        //オリジナルではボタンのドロップダウン時に動的にロードしなおししているが、
        //mac版では1job終了時にGUIに反映とする。
        SetPathHistory_new(SETHIST_LIST,ui->plainTextEdit_Dst);
        SetPathHistory_new(SETHIST_LIST,ui->plainTextEdit_Src);
        SetFilterHistory(SETHIST_LIST);
    }
    else {

        //statusのtextエリアにキャンセルしたよを表示
        ui->textEdit_STATUS->append(" ---- Cancelled. ----");

        //statusbarinfoを強制リセット
        SetStatusBarInfo(&ti,true);

        //オリジナルのfastcopyではここでGUI表示させる必要がないが、
        //同時実行排他処理が別スレッドになった関係で、泣く泣くここで表示させる。
        //コピー終了確定でキャンセルボタン押せなくする
        ui->pushButton_cancel_exec->hide();
        //同時実行阻止ボタン隠す
        ui->pushButton_Atonce->hide();
        //実行ボタン復活
        ui->pushButton_Exe->show();
        //Listingボタン復活
        ui->pushButton_Listing->show();
    }

    delete pathLogBuf;
    pathLogBuf = NULL;
    delete pathLogcsvBuf;
    pathLogcsvBuf = NULL;

    BOOL	is_auto_close = false;

    //ジョブリストモードでの実行？
    if(joblistisstarting){
        //ジョブリスト中の最後のジョブ？
        if(IsLastJob()){
            //自動クローズの条件として、ジョブリスト中のジョブでエラーが一個も起きてない、も考慮するよ。
            is_auto_close = autoCloseLevel == FORCE_CLOSE
                                || (autoCloseLevel == NOERR_CLOSE
                                && (!is_starting ||
                                    (ti.total.errFiles == 0 && ti.total.errDirs == 0 && joblist_joberrornum == 0)));
        }
    }
    //普通のコピー実行
    else{
        is_auto_close = autoCloseLevel == FORCE_CLOSE
                                || (autoCloseLevel == NOERR_CLOSE
                                    && (!is_starting || (ti.total.errFiles == 0 &&
                                        ti.total.errDirs == 0 && errBufOffset == 0)));
    }
    //ジョブリストモード終わってる？または最後のジョブ？
    if(!joblistisstarting || (joblistisstarting && IsLastJob())){
        //Listingは対象外なんだけど、verify専用モード実行はListing扱いじゃないので後処理実行対象
        if(!IsListing() || IsVerifyListing() || joblistisstarting){

            if(is_starting && !isAbort) {
                //後処理を実行
                ExecFinalAction();
            }
            //自動終了する？
            if(is_auto_close){
                this->close();
                if((ti.total.errFiles || ti.total.errDirs) || joblist_joberrornum > 0){
                    exit(FASTCOPY_ERROR_EXISTS);
                }
                else{
                    exit(FASTCOPY_OK);
                }
            }
            autoCloseLevel = NO_CLOSE;
        }
    }
    UpdateMenu();
    IsListing() ?
     ui->pushButton_Listing->setFocus() :
     ui->pushButton_Exe->setFocus();

    if (isTaskTray) {
        TaskTray(NIM_MODIFY, hMainIcon[curIconIndex = FCNORMAL_ICON_IDX], FASTCOPY);
    }
    return	TRUE;
}

//Threadプライオリティ変更
void MainWindow::SetPriority(QThread::Priority new_class)
{
    QThread *my_Thread = QThread::currentThread();
    //API仕様の説明見てもいまいち意味あるんだかわからんけど、とりあえず。
    //ぶっちゃけosの仕様次第だよねー。
    if (curPriority != new_class) {
        my_Thread->setPriority(new_class);
        curPriority = new_class;
    }
}

FastCopy::Mode MainWindow::GetCopyMode(void)
{
    if(!copyInfo)return FastCopy::DIFFCP_MODE;	// 通常はありえない。
    return		copyInfo[ui->comboBox_Mode->currentIndex()].mode;
}

FastCopy::OverWrite MainWindow::GetOverWriteMode(void)
{
    if (!copyInfo) return FastCopy::BY_ATTR;	// 通常はありえない。
    return		copyInfo[ui->comboBox_Mode->currentIndex()].overWrite;
}

//シグナルハンドラ処理
void MainWindow::sighandler_mainwindow(int signum){

    char* headmsg = "Aborted by signal. catched signo(ASCII)='";
    char* footmsg = "'\n";
    //signo=0をASCII文字の'A'に補正して出力するために'A'の16進数をオフセット。
    //ex:SIGHUP=1なので、64+1 = 0x41となり、これをwriteするので'A'がlogに出力される。
    //printf系が使用できない苦肉の策。。
    int sigasciinum = signum + 64;
    //リトルエンディアンなので先頭1バイトにint値入ってるはず
    char* asciipt = (char*)&sigasciinum;
    //排他周りの状況がどうなっているかはわからない。
    //とにかく原因を探りたいので標準エラーログ有効なら強引にwriteしちゃう
    //中身が多少ぐしゃってもダウン要因知りたいでしょ
    //ファイル開いてなかったらopen
    if(hErrLog == SYSCALL_ERR){
        hErrLog = open(errLogPathV,O_RDWR | O_CREAT,0777);
    }
    if(hErrLog != SYSCALL_ERR){

        lseek(hErrLog,0,SEEK_END);
        //文字列終端は書き込まない
        write(hErrLog,headmsg,41);
        //signal番号に40を足してASCII文字に変換。
        write(hErrLog,asciipt,1);
        write(hErrLog,footmsg,2);
        fsync(hErrLog);
        ::close(hErrLog);
    }
    //標準ファイルログ書き出し用排他を確保していたら解放
    if(hErrLogMutex != NULL){
        sem_post((sem_t *)hErrLogMutex);
    }
    //FastCopyクラスの後始末コール
    fastCopy.signal_handler(signum);
}

void MainWindow::SetExtendFilter(bool enable)
{
    if(enable && ui->exfilter_widget->isHidden()){
        ui->exfilter_widget->show();
        this->resize(this->width(),this->height() + ui->exfilter_widget->height());
    }
    else if(!enable && ui->exfilter_widget->isVisible()){
        ui->exfilter_widget->hide();
        this->resize(this->width(),this->height() - ui->exfilter_widget->height());
    }
}

void MainWindow::ReflectFilterCheck(BOOL enabled)
{
    ui->comboBox_include->setEnabled(enabled);
    ui->comboBox_exclude->setEnabled(enabled);
    ui->comboBox_FromDate->setEnabled(enabled);
    ui->comboBox_ToDate->setEnabled(enabled);
    ui->comboBox_MinSize->setEnabled(enabled);
    ui->comboBox_MaxSize->setEnabled(enabled);

    if(enabled && ui->filter_widget->isHidden()){
        ui->filter_widget->show();
        this->resize(this->width(),this->height() + ui->filter_widget->height());
    }
    else if(!enabled && ui->filter_widget->isVisible()){
        ui->filter_widget->hide();
        this->resize(this->width(),this->height() - ui->filter_widget->height());
    }
    SetExtendFilter(enabled);
}

_int64 MainWindow::GetDateInfo(void *buf, BOOL is_end)
{
    const void	*p = NULL;
    time_t		ft;
    struct tm st;
    _int64		&t = *(_int64 *)&ft;
    _int64		val;

    val = strtolV(buf, &p, 10);

    if (val > 0 && !strchrV(buf, '+')) {	// absolute
        if (val < 16010102) return -1;
        memset(&st, 0, sizeof(st));
        st.tm_year = (((WORD) (val / 10000)) - TM_YEAR_OFFSET);
        st.tm_mon = ((WORD)((val / 100) % 100) - TM_MONTH_OFFSET);
        st.tm_mday		= (WORD) (val % 100);
        if (is_end) {
            st.tm_hour			= 23;
            st.tm_min			= 59;
            st.tm_sec			= 59;
            //st.wMilliseconds	= 999;
        }

        ft = mktime(&st);
    }
    else if (p && GetChar(p, 0)) {
        //現在時刻取得
        time(&ft);
        switch (GetChar(p, 0)) {
        case 'W': val *= 60 * 60 * 24 * 7;	break;
        case 'D': val *= 60 * 60 * 24;		break;
        case 'h': val *= 60 * 60;			break;
        case 'm': val *= 60;				break;
        case 's':							break;
        default: return	-1;
        }
        //t += val * 10000000;
        t += val;
    }
    else return -1;

    return	t;
}

_int64 MainWindow::GetSizeInfo(void *buf)
{
    const void	*p=NULL;
    _int64		val;

    if ((val = strtolV(buf, &p, 0)) < 0) return -2;

    if (val == 0 && p == buf) {	// 変換すべき数字が無い
        for (int i=0; GetChar(p, i); i++) {
            if (!strchr(" \t\n", GetChar(p, i))) return -2;
        }
        return	-1;
    }
    int c = p ? GetChar(p, 0) : ' ';

    switch (toupper(c)) {
    case 'T': val *= (_int64)1024 * 1024 * 1024 * 1024;	break;
    case 'G': val *= (_int64)1024 * 1024 * 1024;		break;
    case 'M': val *= (_int64)1024 * 1024;				break;
    case 'K': val *= (_int64)1024;						break;
    case ' ': case 0:	break;
    default: return	-2;
    }

    return	val;
}

BOOL MainWindow::ExecCopy(DWORD exec_flags)
{
    int		idx = (int)ui->comboBox_Mode->currentIndex();
    BOOL	is_delete_mode = copyInfo[idx].mode == FastCopy::DELETE_MODE;

    BOOL	is_filter = ui->actionShow_Extended_Filter->isChecked();

    BOOL	is_listing = (exec_flags & LISTING_EXEC) ? TRUE : FALSE;
    BOOL	is_verifyonlylisting = (exec_flags & VERIFYONLY_EXEC) ? true : false;
    //BOOL	is_initerr_logging = (noConfirmStop && !is_listing) ? TRUE : FALSE;
    BOOL	is_initerr_logging = true;
    int		initerr_messageno;
    BOOL	is_fore = IsForeground();

    //クリティカルエラー時のmessagebox先に用意
    QMessageBox msgbox;
    msgbox.setStandardButtons(QMessageBox::Ok);
    msgbox.setIcon(QMessageBox::Critical);
    msgbox.setWindowTitle(FASTCOPY);

    info.ignoreEvent	= (ui->checkBox_Nonstop->checkState() ? FASTCOPY_ERROR_EVENT : 0) |
                            (noConfirmStop ? FASTCOPY_STOP_EVENT : 0);

    info.mode			= copyInfo[idx].mode;
    info.overWrite		= copyInfo[idx].overWrite;
    //info.lcid			= cfg.lcid;

    // 起動時の各種オプション初期値設定
    info.flags			= cfg.copyFlags
        | (is_listing ? FastCopy::LISTING_ONLY : 0)
        | (!is_listing && fileLogMode != Cfg::NO_FILELOG ? FastCopy::LISTING : 0)
        | (ui->checkBox_Acl->checkState() ? FastCopy::WITH_ACL : 0)
        | (ui->checkBox_xattr->checkState() ? FastCopy::WITH_XATTR : 0)
        | (dotignoremode ? FastCopy::SKIP_DOTSTART_FILE : 0)
        | (cfg.aclErrLog ? FastCopy::REPORT_ACL_ERROR : 0)
        | (cfg.streamErrLog ? FastCopy::REPORT_STREAM_ERROR : 0)
                                                                    //listingモードあるいは、listingモードなんだけどVERIFYONLYモード
        | (!is_delete_mode && ui->checkBox_Estimate->checkState() && (!is_listing || (is_listing && is_verifyonlylisting)) ?
            FastCopy::PRE_SEARCH : 0)
        | (!is_listing && ui->checkBox_Verify->checkState() ? FastCopy::VERIFY_FILE : 0)
        | (is_delete_mode && ui->checkBox_owdel->isChecked() ? cfg.enableNSA ?
            FastCopy::OVERWRITE_DELETE_NSA : FastCopy::OVERWRITE_DELETE : 0)

        | (is_delete_mode && cfg.delDirWithFilter ? FastCopy::DELDIR_WITH_FILTER : 0)

        | (cfg.isSameDirRename ? FastCopy::SAMEDIR_RENAME : 0)
        | (cfg.isAutoSlowIo ? FastCopy::AUTOSLOW_IOLIMIT : 0)
        | (skipEmptyDir && is_filter ? FastCopy::SKIP_EMPTYDIR : 0)
        | (!isReparse
            && info.mode != FastCopy::MOVE_MODE
            && info.mode != FastCopy::DELETE_MODE ?
            (FastCopy::FILE_REPARSE|FastCopy::DIR_REPARSE) : 0)
        | (((is_listing && is_fore && QApplication::queryKeyboardModifiers() & Qt::ControlModifier) || (info.mode == FastCopy::VERIFY_MODE))
            ? FastCopy::VERIFY_FILE : 0)
        | (info.mode == FastCopy::MOVE_MODE && cfg.serialMove ? FastCopy::SERIAL_MOVE : 0)
        | (info.mode == FastCopy::MOVE_MODE && cfg.serialVerifyMove ?
            FastCopy::SERIAL_VERIFY_MOVE : 0)
        | (isLinkDest ? FastCopy::RESTORE_HARDLINK : 0)
        | (isReCreate ? FastCopy::DEL_BEFORE_CREATE : 0)
        | (diskMode == 0 ? 0 : diskMode == 1 ? FastCopy::FIX_SAMEDISK : FastCopy::FIX_DIFFDISK)
        | (is_verifyonlylisting ? FastCopy::LISTING_ONLY_VERIFY : 0);

    //第二フラグ
    info.flags_second = cfg.copyFlags | (ui->checkBox_LTFS->isChecked() ? FastCopy::LTFS_MODE : 0)
                                      | (cfg.rwstat ? FastCopy::STAT_MODE : 0)
                                      | (cfg.async ? FastCopy::ASYNCIO_MODE : 0)
                                      | (isCommandStart ? FastCopy::LAUNCH_CLI : 0)
                                      | (cfg.isDisableutime ? FastCopy::DISABLE_UTIME : 0)
//                                    | (cfg.enableReadahead ? FastCopy::ENABLE_READAHEAD : 0)
                                      | (fileLogMode & Cfg::ADD_CSVFILELOG ? FastCopy::CSV_FILEOUT : 0);

    switch(cfg.usingMD5){

        case TDigest::SHA1:
            info.flags_second |= FastCopy::VERIFY_SHA1;
            break;
        case TDigest::MD5:
            info.flags_second |= FastCopy::VERIFY_MD5;
            break;
        case TDigest::XX:
            info.flags_second |= FastCopy::VERIFY_XX;
            break;
        case TDigest::SHA2_256:
            info.flags_second |= FastCopy::VERIFY_SHA2_256;
            break;
        case TDigest::SHA2_512:
            info.flags_second |= FastCopy::VERIFY_SHA2_512;
            break;
        case TDigest::SHA3_256:
            info.flags_second |= FastCopy::VERIFY_SHA3_256;
            break;
        case TDigest::SHA3_512:
            info.flags_second |= FastCopy::VERIFY_SHA3_512;
            break;

        default:
            info.flags_second |= FastCopy::VERIFY_MD5;
            break;
    }

    //mainBufの最大値チェック。とりあえず2GB。
    if(ui->lineEdit_BUFF->text().toInt() >= MAX_MAINBUF_MB){
        info.bufSize = (unsigned int)MAX_MAINBUF;
    }
    else{
        info.bufSize = ui->lineEdit_BUFF->text().toInt() * 1024 * 1024;
    }

    //info.maxTransSizeは最大で1GBまで。勝手に補正。
    if(cfg.maxTransSize > MAX_IOSIZE_MB){
        info.maxTransSize = MAX_IOSIZE;
    }
    else{
        info.maxTransSize	= cfg.maxTransSize * 1024 * 1024;
    }

    // 対象ファイルシステムごとにI/Oサイズをなんとなく制限する
    info.maxTransSize = fastCopy.CompensentIO_size(info.maxTransSize,fastCopy.srcFsType,fastCopy.dstFsType);

    if(info.flags_second & FastCopy::LTFS_MODE){
        //LTFSモード時はread/write単位は1MB固定に変更
        info.maxTransSize = FASTCOPY_LTFS_MAXTRANSSIZE;
        //xattr付与されてたらオフる。(LTFSではxattrは1エントリしか保存できない)
        info.flags = info.flags &~ FastCopy::WITH_XATTR;
        //ACLも保存できないので強制オフ
        info.flags = info.flags &~ FastCopy::WITH_ACL;

        //LTFS上の時刻情報はアテにならないので時刻復元をオフ
        info.flags_second |= FastCopy::DISABLE_UTIME;
    }

    info.maxOpenFiles	= cfg.maxOpenFiles;
    info.maxAttrSize	= cfg.maxAttrSize;
    info.maxDirSize		= cfg.maxDirSize;
    info.maxLinkHash	= maxLinkHash;

    // MainWindowへのイベントハンドルセット
    info.hNotifyWnd = this;

    info.fromDateFilter	= 0;
    info.toDateFilter	= 0;
    info.minSizeFilter	= -1;
    info.maxSizeFilter	= -1;

    errBufOffset		= 0;
    listBufOffset		= 0;
    lastTotalSec		= 0;
    calcTimes			= 0;

    memset(&ti, 0, sizeof(ti));
    isAbort = FALSE;

    resultStatus = true;

    if(joblistisstarting && IsFirstJob()){
        time_t wk_t;
        time(&wk_t);
        localtime_r(&wk_t,&jobliststarttm);
    }

    int		src_len = GetSrclen() + STR_NULL_GUARD; //+\0
    int		dst_len = GetDstlen() + STR_NULL_GUARD; //+\0

    if (src_len <= 1 || (!is_delete_mode && dst_len <= 1)) {
        return	FALSE;
    }

    char	*src = new(std::nothrow) char [src_len], dst[MAX_PATH + STR_NULL_GUARD] = "";
    //srcパス分のヒープ確保失敗(まず発生しないのでロギングしない)
    if(src == NULL){
        msgbox.setText((char*)GetLoadStrV(IDS_SRCHEAP_ERROR));
        msgbox.exec();
        return false;
    }

    BOOL	ret = TRUE;

    BOOL	exec_confirm = cfg.execConfirm;

    if(!exec_confirm){
        exec_confirm = is_delete_mode ? !noConfirmDel : cfg.execConfirm;
        //ジョブリストモードでの実行中の場合はコンフィグ設定に関わらずダイアログ生成強制無効
        if(joblistisstarting){
            exec_confirm = false;
        }
    }

    //srcとdstにパス格納
    memcpy(src,GetSrc().toLocal8Bit().data(),src_len);
    memcpy(dst,GetDst().toLocal8Bit().data(),dst_len);

    // 各種表示用の画面初期化
    ui->textEdit_PATH->setText("");
    //前回結果の背景色・文字色変更をリセット
    ui->textEdit_PATH->setPalette(default_palette);
    ui->textEdit_STATUS->setText("");

    // presearch有効ならプログレスバーを表示
    if(info.flags & FastCopy::PRE_SEARCH){
        ui->progressBar_exec->setValue(0);
        if(ui->progress_widget->isHidden()){
            this->resize(this->width(),this->height() + ui->progress_widget->height());
            ui->progress_widget->show();
        }
    }
    //presearchなしの場合は表示不能なので隠す
    else{
        if(ui->progress_widget->isVisible()){
            this->resize(this->width(),this->height() - ui->progress_widget->height());
            ui->progress_widget->hide();
        }
    }

    ui->Label_Errstatic_FD->setText("");

    PathArray	srcArray, dstArray, incArray, excArray;
    srcArray.RegisterMultiPath(src);
    if (!is_delete_mode)
        dstArray.RegisterPath(dst);

    //フィルタ
    QString	from_date,to_date;
    QString	min_size,max_size;
    char	*inc = NULL, *exc = NULL;
    if (is_filter) {

        DWORD	inc_len = strlen(ui->comboBox_include->currentText().toLocal8Bit().data()) + STR_NULL_GUARD;
        DWORD	exc_len = strlen(ui->comboBox_exclude->currentText().toLocal8Bit().data()) + STR_NULL_GUARD;

        inc = new char [inc_len];
        exc = new char [exc_len];

        memcpy(inc,ui->comboBox_include->currentText().toStdString().c_str(),inc_len);
        memcpy(exc,ui->comboBox_exclude->currentText().toStdString().c_str(),exc_len);
        incArray.RegisterMultiPath(inc,SEMICOLON_V);
        excArray.RegisterMultiPath(exc,SEMICOLON_V);

        if(!ui->comboBox_FromDate->currentText().isEmpty()){
            from_date = ui->comboBox_FromDate->currentText();
            info.fromDateFilter = GetDateInfo(
                                    from_date.toLocal8Bit().data(),
                                    FALSE);
        }
        if(!ui->comboBox_ToDate->currentText().isEmpty()){
            to_date = ui->comboBox_ToDate->currentText();
            info.toDateFilter = GetDateInfo(
                                    to_date.toLocal8Bit().data(),
                                    TRUE);
        }
        if(!ui->comboBox_MinSize->currentText().isEmpty()){
            min_size = ui->comboBox_MinSize->currentText();
            info.minSizeFilter  = GetSizeInfo(min_size.toLocal8Bit().data());
        }
        if(!ui->comboBox_MaxSize->currentText().isEmpty()){
            max_size = ui->comboBox_MaxSize->currentText();
            info.maxSizeFilter  = GetSizeInfo(max_size.toLocal8Bit().data());
        }
        if (info.fromDateFilter == -1 || info.toDateFilter  == -1) {
            msgbox.setText((char*)GetLoadStrV(IDS_DATEFORMAT_MSG));
            initerr_messageno = IDS_DATEFORMAT_MSG;
            if(!isJobListwithSilent()){
                msgbox.exec();
            }
            ret = FALSE;
        }
        if (info.minSizeFilter  == -2 || info.maxSizeFilter == -2) {
            msgbox.setText((char*)GetLoadStrV(IDS_SIZEFORMAT_MSG));
            initerr_messageno = IDS_SIZEFORMAT_MSG;
            if(!isJobListwithSilent()){
                msgbox.exec();
            }
            ret = FALSE;
        }
    }

    //ここでくりてぃかるえらー起きてもログにのこらないのよねえ。。
    // 確認用ファイル一覧
    if(!ret
        || !(ret = fastCopy.RegisterInfo(&srcArray, &dstArray, &info, &incArray, &excArray))){
        ui->textEdit_STATUS->setText((char*)GetLoadStrV(IDS_PATH_INITERROR));
        initerr_messageno = IDS_PATH_INITERROR;
    }

    int	src_list_len = srcArray.GetMultiPathLen();
    void *src_list = new char [src_list_len];

    if(ret && exec_confirm && (exec_flags & LISTING_EXEC) == 0){
        srcArray.GetMultiPath(src_list, src_list_len);
        void	*title =
                    info.mode == FastCopy::MOVE_MODE   ? GetLoadStrV(IDS_MOVECONFIRM) :
                    info.mode == FastCopy::SYNCCP_MODE ? GetLoadStrV(IDS_SYNCCONFIRM) :
                    //Deleteモード時のwindowtitle追加
                    info.mode == FastCopy::DELETE_MODE ? GetLoadStrV(IDS_DELETECONFIRM) :
                    info.isRenameMode ? GetLoadStrV(IDS_DUPCONFIRM): NULL;

        confirmDlg = new confirmDialog(this,&info,&cfg,title,src_list, is_delete_mode ? NULL : dst);

        switch(confirmDlg->exec()){
        case QDialog::Accepted:
            break;

        case QDialog::Rejected:
        default:
            ret = FALSE;
            initerr_messageno = IDS_CONFIRM_REJECTED;
            if (!is_delete_mode)
                autoCloseLevel = NO_CLOSE;
            break;
        }
        delete confirmDlg;
    }

    if(ret){
        if(is_delete_mode ? cfg.EntryDelPathHistory(src) : cfg.EntryPathHistory(src, dst)){
            SetPathHistory_new(SETHIST_LIST,ui->plainTextEdit_Dst);
            SetPathHistory_new(SETHIST_LIST,ui->plainTextEdit_Src);
        }
        if(is_filter){
            cfg.EntryFilterHistory(inc,
                                               exc,
                                   from_date.toLocal8Bit().data(),
                                   to_date.toLocal8Bit().data(),
                                   min_size.toLocal8Bit().data(),
                                   max_size.toLocal8Bit().data());
            SetFilterHistory(SETHIST_LIST);
        }
        cfg.WriteIni();
    }
    if((ret || is_initerr_logging) && (pathLogBuf = new QString())){

        EditPathLog(pathLogBuf,src,dst,is_delete_mode,inc,exc,idx,false);

        if(info.flags_second & FastCopy::CSV_FILEOUT && (pathLogcsvBuf = new QString())){
            EditPathLog(pathLogcsvBuf,src,dst,is_delete_mode,inc,exc,idx,true);
        }
    }

    delete [] exc;
    delete [] inc;
    delete [] src_list;
    delete [] src;

    if(ret){
        //折り返し設定する。Listだけ作成のときは折り返さない。そうじゃない場合は折り返す。
        //見た目のためにそうしてるっぽいけど、見やすさと合わせてあとで要調整かな。
        if(IsListing() == true){
            //NoWrap指定しちゃうと、textエリアに長い文字列入ると、フォーム側で強制改行されるなあ。
            //excelに貼るときとかは不便だから、その手の処理は無効にしておこ。
            ui->textEdit_PATH->setLineWrapMode(QTextEdit::NoWrap);
        }
        else{
            ui->textEdit_PATH->setLineWrapMode(QTextEdit::WidgetWidth);
        }

        if(ui->err_widget->isVisible()){
            //qDebug("2662 before width = %d height = %d err_widget_height = %d",this->width(),this->height(),ui->err_widget->height());
            this->resize(this->width(),this->height() - ui->err_widget->height());
            //qDebug("2664 after width = %d height = %d err_widget_height = %d",this->width(),this->height(),ui->err_widget->height());
            ui->err_widget->hide();
            /*
            qDebug("2662 before width = %d height = %d err_widget_height = %d",this->width(),this->height(),ui->err_widget->height());
            ui->err_widget->hide();
            this->resize(this->width(),this->height() - ui->err_widget->height());
            qDebug("2664 after width = %d height = %d err_widget_height = %d",this->width(),this->height(),ui->err_widget->height());
            */
            isErrEditHide = true;
        }

        //エラー発生時のみ表示されるエラーログの内容をリセット
        ui->textEdit_ERR->setText("");

        //他のRapidCopyとの同時実行排他取りに行く
        if(forceStart == 1 || (forceStart == 0 && is_delete_mode) || fastCopy.TakeExclusivePriv()){
            if(forceStart == 1 && !is_delete_mode){
                fastCopy.TakeExclusivePriv();
            }
            ret = ExecCopyCore();
        }
        else {
            isDelay = TRUE;

            //一回目で待たされちゃうなら、以降はイベントタイマで定期的にチェックやな。
            mw_TimerId = QObject::startTimer(FASTCOPY_TIMER_TICK,Qt::CoarseTimer);
            ui->pushButton_Atonce->show();

            //現在タスクトレイに入ってる？
            if(isTaskTray){
                //タスクトレイのiconをwaitのiconにする。
                TaskTray(NIM_MODIFY, hMainIcon[FCWAIT_ICON_IDX], FASTCOPY);
            }
        }
        if (ret) {
            //コマンドラインモード起動時はGUIクリック出口なくボタン隠し操作できないので、旧来位置で処理する。
            if(exec_flags & CMDLINE_EXEC){
                ui->pushButton_Listing->hide();
                ui->pushButton_Exe->hide();
                ui->pushButton_cancel_exec->show();
            }
        }
    }

    if(!ret){
        if (pathLogBuf) {
            if (is_initerr_logging) {
                WriteErrLog(TRUE,initerr_messageno);
            }
            delete pathLogBuf;
            pathLogBuf = NULL;
            if(info.flags_second & FastCopy::CSV_FILEOUT && pathLogcsvBuf){
                delete pathLogcsvBuf;
                pathLogcsvBuf = NULL;
            }
        }
    }

    return	ret;
}

BOOL MainWindow::ExecCopyCore(void)
{
    time_t wk_t;					//ワーク用のtime_t

    time(&wk_t);					//time_t取得
    //startTmに時刻格納
    localtime_r(&wk_t,&startTm);
    //JobListモードでの実行時は最初のジョブ開始時刻を記憶しとく
    if(joblistisstarting && IsFirstJob()){
        jobliststarttm = startTm;
    }

    StartFileLog();
    StartStatLog();

    //コピー開始
    BOOL ret = fastCopy.Start(&ti);
    if(ret){
        timerCnt = timerLast = 0;
        mw_TimerId = QObject::startTimer(FASTCOPY_TIMER_TICK,Qt::CoarseTimer);
        UpdateSpeedLevel();
    }
    return	ret;
}

//コピー終了時の終了時処理
BOOL MainWindow::ExecFinalAction()
{

    if (finActIdx < 0) return FALSE;

    FinAct	*act = cfg.finActArray[finActIdx];
    BOOL	is_err = ti.total.errFiles || ti.total.errDirs;

    int	flg = (act->flags & FinAct::ERR_CMD)	? FinAct::ERR_CMD	:
              (act->flags & FinAct::NORMAL_CMD) ? FinAct::NORMAL_CMD : 0;

    if (GetChar(act->command, 0) &&
        (flg == 0 || (flg == FinAct::NORMAL_CMD && !is_err) || (flg == FinAct::ERR_CMD && is_err))) {
        PathArray	pathArray;
        BOOL		is_wait = (act->flags & FinAct::WAIT_CMD) ? TRUE : FALSE;

        pathArray.SetMode(PathArray::ALLOW_SAME | PathArray::NO_REMOVE_QUOTE);
        pathArray.RegisterMultiPath(act->command,SEMICOLON_V);

        for (int i=0; i < pathArray.Num(); i++) {
            PathArray	path;
            PathArray	param;

            path.SetMode(PathArray::ALLOW_SAME | PathArray::NO_REMOVE_QUOTE);
            path.RegisterMultiPath(pathArray.Path(i), (void*)" ");

            param.SetMode(PathArray::ALLOW_SAME);
            for (int j=1; j < path.Num(); j++) param.RegisterPath(path.Path(j));

            char *param_str = new char [param.GetMultiPathLen(" ")];
            param.GetMultiPath(param_str, param.GetMultiPathLen(), " ");

            //外部process起動と実行
            //課題:コマンド実行にはいまのところ2つの課題があるど。
            //		1.コマンドパスや引数に空白が指定できない(エスケープシーケンスできないので自力作り込みが必要)
            //		2.コマンドパスや引数に";"を含むとコマンドの区切りと誤認される。(srcやdstの方式にすればいいけど後回し)
            QProcess post_process;

            QStringList parameterlist;
            QString		parameterstr((char*)param_str);

            QString		cmd_str((char*)path.Path(0));
            parameterlist = parameterstr.split(QRegExp("\\s"),QString::SkipEmptyParts);

            //外部プロセスの起動はwindows版と違ってターミナルで起動する術がないので、daemon的な動作になっちゃうよ。。
            //課題だけど、仕様で押し通しちゃうしかないかなあ。。
            if(is_wait){

                //コマンド待ち終了後の表示用にカレントテキスト退避
                QString prev_text(ui->textEdit_PATH->toPlainText());
                ui->textEdit_PATH->append((char*)GetLoadStrV(IDS_FINACT_WAIT_INFO));
                this->setEnabled(false);
                //コマンドに引数存在する？
                if(!parameterlist.at(0).isEmpty()){
                    //引数リスト付きで実行ね
                    hCommandThread = new Command_Thread(&cmd_str,FINACTCMD_STACKSIZE,&parameterlist);
                }
                else{
                    //引数無しで実行ね
                    hCommandThread = new Command_Thread(&cmd_str,FINACTCMD_STACKSIZE);
                }
                timespec sleepValue = {0,INTERVAL_MS};
                hCommandThread->start();
                while(1){
                    QCoreApplication::processEvents(QEventLoop::AllEvents);
                    if(hCommandThread->isFinished()){
                        break;
                    }
                    nanosleep(&sleepValue,NULL);
                }
                delete hCommandThread;
                this->setEnabled(true);
                //コマンド待ち以前の内容を復元
                ui->textEdit_PATH->setPlainText(prev_text);
            }
            else{
                //非同期実行
                if(!parameterstr.isEmpty()){
                    post_process.startDetached((char*)path.Path(0),parameterlist);
                }
                else{
                    post_process.startDetached((char*)path.Path(0));
                }
            }
            delete [] param_str;
        }
    }

    //サウンド鳴らすのはwindowsと仕様がちがう。コマンド実行後に鳴らすよ。
    if (GetChar(act->sound, 0) && (!(act->flags & FinAct::ERR_SOUND) || is_err)) {
        player.setMedia(QUrl::fromLocalFile((char*)act->sound));
        player.play();
    }

    //メール送信する？
    if(IsSendingMail(false)){

        Smtp *smtp = new Smtp((char*)act->account,(char*)act->password,(char*)act->smtpserver,atoi((char*)act->port));
        smtp->sendMail((char*)act->frommailaddr,(char*)act->tomailaddr,mailsub,mailbody);

        mailsub.clear();
        mailbody.clear();
    }

    return	TRUE;
}


void MainWindow::EditPathLog(QString *buf, void* src,void* dst,bool is_delete_mode,char* inc,char* exc,int idx,bool req_csv){
    char log_wkbuf[MAX_PATH * 4];	//Log出力用のワークバッファ
                                    //サイズは適当なので溢れに注意してStringのPathLogBufに移し替えてね
    int len = 0;					//log_wkbuf内のオフセット

    char *username_pt = getenv("USER");	//カレントユーザ名
    struct utsname uts_t;
    memset(&uts_t,0,sizeof(uts_t));

    uname(&uts_t);						//エラーは無視

    //ユーザ名
    len += sprintf(log_wkbuf + len,
                    req_csv ? "<username>,%s\n" : "<username> %s\n",
                    username_pt);
    //ホスト名(netBIOS名じゃないよ)
    len += sprintf(log_wkbuf + len,
                    req_csv ? "<hostname>,%s\n" : "<hostname> %s\n",
                    uts_t.nodename);
    //srcのファイルシステムタイプ出力
    len += sprintf(log_wkbuf + len,
                    req_csv ? "<srcFsType>,%s" : "<srcFsType> %s",
                    fastCopy.srcFsName);
    //dstのファイルシステムタイプ出力
    if(!is_delete_mode){
        len += sprintf(log_wkbuf + len,
                        req_csv ? "\n<dstFsType>,%s" : "\n<dstFsType> %s",
                        fastCopy.dstFsName);
    }
    if(inc && GetChar(inc, 0)){
        len += sprintf(log_wkbuf + len,
                        req_csv ? "\n<Include>,%s" : "\n<Include> %s",
                        (char *)inc);
    }
    if(exc && GetChar(exc, 0)){
        len += sprintf(log_wkbuf + len,
                        req_csv ? "\n<Exclude>,%s" : "\n<Exclude> %s",
                        (char *)exc);
    }
    len += sprintf(log_wkbuf + len,
                    req_csv ? "\n<Command>,%s" : "\n<Command> %s",
                    copyInfo[idx].list_str);

    if (info.flags & (FastCopy::WITH_ACL|FastCopy::WITH_XATTR|FastCopy::OVERWRITE_DELETE
                     |FastCopy::OVERWRITE_DELETE_NSA|FastCopy::VERIFY_FILE|FastCopy::SKIP_DOTSTART_FILE)
         || info.flags_second & FastCopy::LTFS_MODE
         || IsListing()) {
        len += sprintf(log_wkbuf + len, " (with");
        if (!is_delete_mode && (info.flags & FastCopy::VERIFY_FILE)){
            len += sprintf(log_wkbuf + len, " Verify[");
            if(info.flags_second & FastCopy::VERIFY_SHA1)
                len += sprintf(log_wkbuf + len,(char*)GetLoadStrV(IDS_SHA1));
            else if(info.flags_second & FastCopy::VERIFY_MD5)
                len += sprintf(log_wkbuf + len,(char*)GetLoadStrV(IDS_MD5));
            else if(info.flags_second & FastCopy::VERIFY_XX)
                len += sprintf(log_wkbuf + len,(char*)GetLoadStrV(IDS_XX));
            else if(info.flags_second & FastCopy::VERIFY_SHA2_256)
                len += sprintf(log_wkbuf + len,(char*)GetLoadStrV(IDS_SHA2_256));
            else if(info.flags_second & FastCopy::VERIFY_SHA2_512)
                len += sprintf(log_wkbuf + len,(char*)GetLoadStrV(IDS_SHA2_512));
            else if(info.flags_second & FastCopy::VERIFY_SHA3_256)
                len += sprintf(log_wkbuf + len,(char*)GetLoadStrV(IDS_SHA3_256));
            else if(info.flags_second & FastCopy::VERIFY_SHA3_512)
                len += sprintf(log_wkbuf + len,(char*)GetLoadStrV(IDS_SHA3_512));
            //else ありえない
            len += sprintf(log_wkbuf + len, "]");
        }
        if (!is_delete_mode && (info.flags & FastCopy::WITH_ACL))
            len += sprintf(log_wkbuf + len, " ACL");
        if (!is_delete_mode && (info.flags & FastCopy::WITH_XATTR))
            len += sprintf(log_wkbuf + len, " XATTR");
        if (!is_delete_mode && (info.flags & FastCopy::SKIP_DOTSTART_FILE))
            len += sprintf(log_wkbuf + len, " SKIP_DOT");
        if (is_delete_mode && (info.flags & FastCopy::OVERWRITE_DELETE))
            len += sprintf(log_wkbuf + len, " OverWrite");
        if (is_delete_mode && (info.flags & FastCopy::OVERWRITE_DELETE_NSA))
            len += sprintf(log_wkbuf + len, " NSA");
        //deleteモードでLTFS有効にする意味あるのか微妙だけどとりあえず。。
        if (info.flags_second & FastCopy::LTFS_MODE)
            len += sprintf(log_wkbuf + len, " LTFS");
        if (info.flags_second & FastCopy::LAUNCH_CLI)
            len += sprintf(log_wkbuf + len, " CLI");
        if (IsListing())
            len += sprintf(log_wkbuf + len, " LISTING");
        len += sprintf(log_wkbuf + len, ")");
    }
    len += sprintf(log_wkbuf + len , "\n");
    buf->append(log_wkbuf);

    QStringList srcpaths = QString::fromLocal8Bit((char*)src).split(QChar::ParagraphSeparator);
    for(int i=0; i<srcpaths.count();i++){
        sprintf(log_wkbuf,
                req_csv ? "<Source%d>,\"%s\"\n" : "<Source%d> %s\n",
                i+1,
                srcpaths.at(i).toLocal8Bit().data());
        //sourceは何個くるんだかわからないので都度QStringに出力
        buf->append(log_wkbuf);
    }

    if (!is_delete_mode) {
        len += sprintf(log_wkbuf,
                       req_csv ? "<DestDir>,\"%s\"\n" : "<DestDir> %s\n",
                       (char *)dst);
        buf->append(log_wkbuf);
    }
}

void MainWindow::WriteLogHeader(int hFile, BOOL add_filelog,bool csv_title)
{
    char	buf[1024];

    lseek(hFile,0,SEEK_END);

    DWORD len = sprintf(buf, !csv_title ? RAPIDCOPY_REC_DIVIDER : RAPIDCOPY_CSV_REC_DIVIDER);

    //ビルド日を出力
    len += sprintf(buf + len,
                   "RapidCopy(%s%s) start at %d/%02d/%02d %02d:%02d:%02d\nRapidCopy Build at %s %s\n\n",
                    GetVersionStr(),GetVerAdminStr(),
                    startTm.tm_year + TM_YEAR_OFFSET, startTm.tm_mon + TM_MONTH_OFFSET, startTm.tm_mday,
                    startTm.tm_hour, startTm.tm_min, startTm.tm_sec,
                    __DATE__,
                    __TIME__);

    len = write(hFile,buf,len);
    if(csv_title){
        len = write(hFile,pathLogcsvBuf->toLocal8Bit().data(),strlen(pathLogcsvBuf->toLocal8Bit().data()));
    }
    else{
        len = write(hFile,pathLogBuf->toLocal8Bit().data(),strlen(pathLogBuf->toLocal8Bit().data()));
    }

    if(add_filelog && *fileLogPathV){
        len = sprintf(buf, "<FileLog> %s\n",(char *)fileLogPathV);
        len = write(hFile,buf,len);
    }

    //ジョブリストモード有効ならジョブ名出力
    if(joblistisstarting){
        //output Job information.
        char *title = (char*)joblist.at(joblistcurrentidx).title;
        len = sprintf(buf, "<Job> %s\n",(char *)title);
        write(hFile,buf,len);
    }

    if(finActIdx >= 1){
        char	*title = (char*)cfg.finActArray[finActIdx]->title;
        len = sprintf(buf, "<PostPrc> %s\n\n",(char *)title);
        write(hFile,buf,len);
    }
    else{
        write(hFile,"\n",1);
    }

    //csvタイトル見出し要求あり？
    if(csv_title){
        write(hcsvFileLog,(char*)GetLoadStrV(IDS_FILECSVTITLE),strlen((char*)GetLoadStrV(IDS_FILECSVTITLE)));
    }
}

BOOL MainWindow::WriteLogFooter(int hFile,bool csv_title)
{
    DWORD	len = 0;
    char	buf[1024];

    lseek(hFile,0,SEEK_END);

    //csvタイトル見出し要求あり？
    if(csv_title){
        write(hcsvFileLog,(char*)GetLoadStrV(IDS_FILECSVTITLE),strlen((char*)GetLoadStrV(IDS_FILECSVTITLE)));
        write(hcsvFileLog,(char*)GetLoadStrV(IDS_FILECSVEX),strlen((char*)GetLoadStrV(IDS_FILECSVEX)));
    }

    if (errBufOffset == 0 && ti.total.errFiles == 0 && ti.total.errDirs == 0 && hFile!= hcsvFileLog){
        len += sprintf(buf + len, "%s No Errors\n", hFile == hFileLog ? "\n" : "");
    }

    len += sprintf(buf + len, "\n");
    len += snprintf(buf + len,sizeof(buf) - len,(char*)ui->textEdit_STATUS->toPlainText().toLocal8Bit().data());
    len += sprintf(buf + len,"\n\nResult : ");

    len += snprintf(buf + len,sizeof(buf) - len,(char*)ui->Label_Errstatic_FD->text().toLocal8Bit().data());
    len += sprintf(buf + len, "%s", "\n\n");

    write(hFile,buf,len);

    return	TRUE;
}

BOOL MainWindow::WriteErrLog(BOOL is_initerr,int message_no)
{
    time_t wk_t;					//ワーク用のtime_t

    EnableErrLogFile(TRUE);

    if(is_initerr){
        time(&wk_t);					//time_t取得
        localtime_r(&wk_t,&startTm);	//startTmに時刻格納
    }

    WriteLogHeader(hErrLog,true,false);

    if(errBufOffset){
        char *werr = (char *)ti.errBuf->Buf();
        write(hErrLog,werr,strlen(werr));
    }
    else if(is_initerr){
        char *msg = (char*)GetLoadStrV(message_no);
        if(msg){
            write(hErrLog,msg,strlen(msg));
            write(hErrLog,"\n",1);
        }
    }

    if(!is_initerr) WriteLogFooter(hErrLog);

    EnableErrLogFile(FALSE);
    return	TRUE;
}

void MainWindow::WriteMailLog(){

    char	buf_subject[512];
    char	buf[1024];
    struct tm stm = joblistisstarting ? jobliststarttm : startTm;

    if(joblist.empty() || IsFirstJob()){
        //mail subject header
        sprintf(buf_subject,
            "RapidCopy Report. start at %d/%02d/%02d %02d:%02d:%02d",
            stm.tm_year + TM_YEAR_OFFSET, stm.tm_mon + TM_MONTH_OFFSET, stm.tm_mday,
            stm.tm_hour, stm.tm_min, stm.tm_sec);
        mailsub.append((char*)buf_subject);

        //mail body header
        sprintf(buf, "-------------------------------------------------\n"
        "RapidCopy(%s%s) start at %d/%02d/%02d %02d:%02d:%02d\nBuild at %s %s\n\n",
        //QSysInfo::MacintoshVersion,username,
        GetVersionStr(), GetVerAdminStr(),
        stm.tm_year + TM_YEAR_OFFSET, stm.tm_mon + TM_MONTH_OFFSET, stm.tm_mday,
        stm.tm_hour, stm.tm_min, startTm.tm_sec,__DATE__,__TIME__);

        mailbody.append(buf);
        //JobListmode disabled?
        if(joblist.empty()){
            //src and dst output.
            mailbody.append(pathLogBuf);
        }
        else{
            //JobListName output
            sprintf(buf,"<JobListName> %s\n",
                    (char*)ui->comboBox_JobListName->currentText().toLocal8Bit().data());
            mailbody.append(buf);
        }
        if (*fileLogPathV) {
            sprintf(buf, "<FileLog> %s\n",(char *)fileLogPathV);
            mailbody.append((char*)buf);
        }
        if (finActIdx >= 1) {
            char	*title = (char *)cfg.finActArray[finActIdx]->title;
            sprintf(buf, "<PostPrc> %s\n\n",(char *)title);
            mailbody.append(buf);
        }
        else {
            sprintf(buf,"\n");
            mailbody.append(buf);
        }
    }

    //ジョブリストモードじゃない？
    if(!joblistisstarting){
        //errortext
        if (errBufOffset) {
            mailbody.append((char*)ti.errBuf->Buf());
        }

        //footer
        if (errBufOffset == 0 && ti.total.errFiles == 0 && ti.total.errDirs == 0){
            sprintf(buf, "\n No Errors\n");
            mailbody.append(buf);
        }
        sprintf(buf,"\n");
        mailbody.append(buf);

        mailbody.append(ui->textEdit_STATUS->toPlainText());
        sprintf(buf,"\n\nResult : ");
        mailbody.append(buf);
        mailbody.append(ui->Label_Errstatic_FD->text());
        sprintf(buf,"\n\n");
        mailbody.append(buf);
    }
    else{
        //ジョブリストモード
        //当該ジョブの成否を保存
        sprintf(buf,"<%s> %s\n",
                (char*)joblist.at(joblistcurrentidx).title,
                (ti.total.errFiles || ti.total.errDirs) ? "Error" : "No Error");
        mailbody.append(buf);

        //ジョブリスト中の最後のジョブ？
        if(IsLastJob()){
            //ジョブリスト全体の中のジョブで一つでもエラーがある？
            if(joblist_joberrornum){
                sprintf(buf,"\nNo Error:%d Error:%d",
                        joblist.count() - joblist_joberrornum,
                        joblist_joberrornum);
            }
            //全部成功なのね
            else{
                sprintf(buf,"\nNo Error(s).");
            }
            mailbody.append(buf);
        }
    }
    return;
}

bool MainWindow::IsSendingMail(bool is_prepare){
    FinAct	*act = cfg.finActArray[finActIdx];
    int	mflg = (act->flags & FinAct::ERR_MAIL)	? FinAct::ERR_MAIL   :
              (act->flags & FinAct::NORMAL_MAIL) ? FinAct::NORMAL_MAIL : 0;

    if(GetChar(act->account,0) &&
       GetChar(act->password,0) &&
       GetChar(act->smtpserver,0) &&
       GetChar(act->port,0) &&
       GetChar(act->tomailaddr,0) &&
       GetChar(act->frommailaddr,0)){
        if(joblistisstarting){
            if(is_prepare){
                return true;
            }
            else if(mflg == 0 || (mflg == FinAct::NORMAL_MAIL && joblist_joberrornum == 0) || (mflg == FinAct::ERR_MAIL && joblist_joberrornum != 0)){
                return true;
            }
        }
        //ジョブリストモードじゃない
        else if((mflg == 0 || (mflg == FinAct::NORMAL_MAIL && resultStatus) || (mflg == FinAct::ERR_MAIL && !resultStatus))){
            return true;
        }
    }
    return false;
}

bool MainWindow::IsFirstJob(){
    if(joblistcurrentidx == 0){
        return true;
    }
    else{
        return false;
    }
}

bool MainWindow::IsLastJob(){
    if((joblistcurrentidx + 1) == joblist.count()){
        return true;
    }
    else {
        return false;
    }
}

int MainWindow::UpdateSpeedLevel(BOOL is_timer)
{
    DWORD	waitCnt = fastCopy.GetWaitTick();
    DWORD	newWaitCnt = waitCnt;

    //LTFSモード時はspeedlevel設定要求はすべて無視する
    if(info.flags_second & FastCopy::LTFS_MODE){
        //SPEED_FULLを強制的に設定
        fastCopy.SetWaitTick(0);
        //qDebug() << "LTFS_MODE SPEED_FULL";
        return 0;
    }

    if (speedLevel == SPEED_FULL) {
        newWaitCnt = 0;
    }
    else if (speedLevel == SPEED_AUTO) {
        if(isActiveWindow()){
            newWaitCnt = 0;
        }
        else if(!isActiveWindow() || !isMouseover){
            newWaitCnt = cfg.waitTick;
            timerLast = timerCnt;
        }
        else if (is_timer && newWaitCnt > 0 && (timerCnt - timerLast) >= 10) {
            newWaitCnt -= 1;
            timerLast = timerCnt;
        }
    }
    else if (speedLevel == SPEED_SUSPEND) {
        newWaitCnt = FastCopy::SUSPEND_WAITTICK;
    }
    else {
        //開発機(corei7(sandy)2Ghz + 200MB/sec程度の転送速度を基準として重み付け。
        //Gbe転送速度理論値(120MB/sec)に対する誤差と、高速デバイスに対する誤差のちょうど中間点くらいかなー
        //というのが200MB/sec基準の根拠だよ。
        static DWORD	waitArray[]	= { 1000, 450, 300, 220, 160, 130, 100, 85, 70 };
        newWaitCnt = waitArray[speedLevel - 1];
    }

    fastCopy.SetWaitTick(newWaitCnt);

    if (!is_timer && fastCopy.IsStarting()) {

        SetPriority((speedLevel != SPEED_FULL || cfg.alwaysLowIo) ?
                        //IDLE_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
                        QThread::IdlePriority : QThread::NormalPriority);

        if (speedLevel == SPEED_SUSPEND) {
            fastCopy.Suspend();
        }
        else {
            fastCopy.Resume();
        }
    }

    return	newWaitCnt;
}

BOOL MainWindow::EnableErrLogFile(BOOL on)
{
    if(on && hErrLog == SYSCALL_ERR){

        // ErrLogFileの生成と書き込みは他プロセスで同時起動しているFastCopyと同時に行っちゃう可能性がある。
        // そのため、OSの名前空間に印とって排他かける。
        hErrLogMutex = sem_open(FASTCOPYLOG_MUTEX,O_CREAT,S_IRUSR|S_IWUSR,FASTCOPYLOG_MUTEX_INSTANCE);
        if(hErrLogMutex == SEM_FAILED){
            qDebug() << "sem_open(CREATE FASTCOPYLOG_MUTEX) :" << errno;
        }
        //排他取れるかなー？先に開いてるやついたら無限待ち
        if(sem_wait((sem_t*)hErrLogMutex) == 0){
            hErrLog = open(errLogPathV,O_RDWR | O_CREAT,0777);
        }
        else{
            qDebug() << "sem_wait() failed. errno=" << errno;
        }
    }
    else if(!on && hErrLog != SYSCALL_ERR){
        ::close(hErrLog);
        hErrLog = SYSCALL_ERR;

        //プロセス間排他開放
        if(sem_post((sem_t*)hErrLogMutex) == -1){
            qDebug() << "sem_post(hErrLogMutex) failed.";
        }
        hErrLogMutex = NULL;
    }
    return	TRUE;
}

int GetArgOpt(void *arg, int default_value)
{

    if (GetChar(arg, 0) == 0) {
        return	default_value;
    }

    if (lstrcmpiV(arg, TRUE_V) == 0)
        return	TRUE;

    if (lstrcmpiV(arg, FALSE_V) == 0)
        return	FALSE;

    if (GetChar(arg, 0) >= '0' && GetChar(arg, 0) <= '9' /* || GetChar(arg, 0) == '-' */)
        return	strtolV(arg, NULL, 0);

    return	default_value;
}

int MainWindow::CmdNameToComboIndex(void *cmd_name)
{
    for (int i=0; copyInfo[i].cmdline_name; i++) {
        if (lstrcmpiV(cmd_name, copyInfo[i].cmdline_name) == 0)
            return	i;
    }
    return	-1;
}

BOOL MakeFileToPathArray(void *path_file, PathArray *path)
{
    struct stat wk_stat;
    off_t	size;
    off_t	trans;
    int	hFile = open((char*)path_file,O_RDONLY,0644);
    ::fstat(hFile,&wk_stat);
    size = wk_stat.st_size;

    char	*buf = NULL;
    BOOL	ret = FALSE;
    if (hFile != SYSCALL_ERR) {
        if ((buf = (char *)malloc(size + 2))) {
            if ((trans = read(hFile,buf,size)) != SYSCALL_ERR && size == trans) {
                buf[size] = buf[size+1] = 0;
                if (path->RegisterMultiPath(buf, NEWLINE_STR_V) > 0) ret = TRUE;
            }
            free(buf);
        }
        close(hFile);
    }

    return	ret;
}

int inline strcmpiEx(void *s1, void *s2, int *len)
{
    *len = strlenV(s2);
    return	strnicmpV(s1, s2, *len);
}

BOOL MainWindow::CommandLineExecV(int argc, QStringList argv)
{
    int		job_idx = -1;
    int		joblist_idx = -1;

    BOOL	is_openwin			= FALSE;
    BOOL	is_noexec			= FALSE;
    BOOL	is_delete			= FALSE;
    bool	is_verifymode		= false;
    BOOL	is_estimate			= cfg.estimateMode;
    int		filter_mode			= 0;
    enum	{ NORMAL_FILTER=0x01, EXTEND_FILTER=0x02 };
    int		exec_flag = CMDLINE_EXEC;
    int		argerr = EXIT_SUCCESS;
    int		xval = -1;
    int		yval = -1;

    void	*CMD_STR			= GetLoadStrV(IDS_CMD_OPT);
    void	*BUFSIZE_STR		= GetLoadStrV(IDS_BUFSIZE_OPT);
    void	*LOG_STR			= GetLoadStrV(IDS_LOG_OPT);
    void	*LOGFILE_STR		= GetLoadStrV(IDS_LOGFILE_OPT);
    void	*FILELOG_STR		= GetLoadStrV(IDS_FILELOG_OPT);
    void	*REPARSE_STR		= GetLoadStrV(IDS_REPARSE_OPT);
    void	*FORCESTART_STR		= GetLoadStrV(IDS_FORCESTART_OPT);
    void	*SKIPEMPTYDIR_STR	= GetLoadStrV(IDS_SKIPEMPTYDIR_OPT);
    void	*ERRSTOP_STR		= GetLoadStrV(IDS_ERRSTOP_OPT);
    void	*ESTIMATE_STR		= GetLoadStrV(IDS_ESTIMATE_OPT);
    void	*VERIFY_STR			= GetLoadStrV(IDS_VERIFY_OPT);
    void	*SPEED_STR			= GetLoadStrV(IDS_SPEED_OPT);
    void	*SPEED_FULL_STR		= GetLoadStrV(IDS_SPEED_FULL);
    void	*SPEED_AUTOSLOW_STR	= GetLoadStrV(IDS_SPEED_AUTOSLOW);
    void	*SPEED_SUSPEND_STR	= GetLoadStrV(IDS_SPEED_SUSPEND);
    void	*DISKMODE_STR		= GetLoadStrV(IDS_DISKMODE_OPT);
    void	*DISKMODE_SAME_STR	= GetLoadStrV(IDS_DISKMODE_SAME);
    void	*DISKMODE_DIFF_STR	= GetLoadStrV(IDS_DISKMODE_DIFF);
    void	*OPENWIN_STR		= GetLoadStrV(IDS_OPENWIN_OPT);
    void	*AUTOCLOSE_STR		= GetLoadStrV(IDS_AUTOCLOSE_OPT);
    void	*FORCECLOSE_STR		= GetLoadStrV(IDS_FORCECLOSE_OPT);
    void	*NOEXEC_STR			= GetLoadStrV(IDS_NOEXEC_OPT);
    void	*NOCONFIRMDEL_STR	= GetLoadStrV(IDS_NOCONFIRMDEL_OPT);
    void	*NOCONFIRMSTOP_STR	= GetLoadStrV(IDS_NOCONFIRMSTOP_OPT);
    void	*INCLUDE_STR		= GetLoadStrV(IDS_INCLUDE_OPT);
    void	*EXCLUDE_STR		= GetLoadStrV(IDS_EXCLUDE_OPT);
    void	*FROMDATE_STR		= GetLoadStrV(IDS_FROMDATE_OPT);
    void	*TODATE_STR			= GetLoadStrV(IDS_TODATE_OPT);
    void	*MINSIZE_STR		= GetLoadStrV(IDS_MINSIZE_OPT);
    void	*MAXSIZE_STR		= GetLoadStrV(IDS_MAXSIZE_OPT);
    void	*JOB_STR			= GetLoadStrV(IDS_JOB_OPT);
    void	*JOBLIST_STR		= GetLoadStrV(IDS_JOBLIST_OPT);
    void	*WIPEDEL_STR		= GetLoadStrV(IDS_WIPEDEL_OPT);
    void	*ACL_STR			= GetLoadStrV(IDS_ACL_OPT);
    void	*STREAM_STR			= GetLoadStrV(IDS_STREAM_OPT);
    void	*LINKDEST_STR		= GetLoadStrV(IDS_LINKDEST_OPT);
    void	*RECREATE_STR		= GetLoadStrV(IDS_RECREATE_OPT);
    void	*SRCFILE_STR		= GetLoadStrV(IDS_SRCFILE_OPT);
    void	*FINACT_STR			= GetLoadStrV(IDS_FINACT_OPT);
    void	*TO_STR				= GetLoadStrV(IDS_TO_OPT);
    void	*LTFS_STR			= GetLoadStrV(IDS_LTFS_OPT);
    void	*DOTIGNORE_STR		= GetLoadStrV(IDS_DOTIGNORE_OPT);
    void	*X_STR				= GetLoadStrV(IDS_X_OPT);
    void	*Y_STR				= GetLoadStrV(IDS_Y_OPT);
    void	*CSV_STR			= GetLoadStrV(IDS_FILECSV_OPT);
    void	*dst_path	= NULL;
    PathArray pathArray;

    // 実行ファイル名は skip
    argc--;
    //argv.removeAt(0);

    QCommandLineParser parser;

    parser.addHelpOption();

    parser.addPositionalArgument("path1 path2 ...",QObject::tr("Source path to copy."));

    QCommandLineOption cmdOpt(QStringList() << (char*)CMD_STR,
                              QObject::tr("Specify operation mode.(Default: dif)"),
                              "noe|dif|siz|upd|for|syn|ves|mov|del",
                              "dif");
    QCommandLineOption jobOpt(QStringList() << (char*)JOB_STR,
                              QObject::tr("Specify the job that is already registered."),
                              "jobname");
    QCommandLineOption joblistOpt(QStringList() << (char*)JOBLIST_STR,
                              QObject::tr("Specify the joblist that is already registered."),
                              "joblistname");
    QCommandLineOption bufOpt(QStringList() << (char*)BUFSIZE_STR,
                              QObject::tr("Specify the size(MB) of the main buffer for Read/Write opration."),
                              "num",
                              "1");
    QCommandLineOption filelogOpt(QStringList() << (char*)FILELOG_STR,
                              QObject::tr("Write to the detailed filelog(detail of copy/delete files with checksum) It is stored ""YYYY/MM/DD-hh:mm:ss-N.log"" in RapidCopy/Log directory."),
                              "true|filename",
                              "");

    QCommandLineOption logfileOpt(QStringList() << (char*)LOGFILE_STR,
                              QObject::tr("Specify the filename of standard logfile."),
                              "filename",
                              "");
    QCommandLineOption logOpt(QStringList() << (char*)LOG_STR,
                              QObject::tr("Write the operation/errors information to the standard logfile(RapidCopy.log)."),
                              "true|false",
                              "");
    QCommandLineOption reparseOpt(QStringList() << (char*)REPARSE_STR,
                              QObject::tr("Copy symlink as link."),
                              "true|false",
                              "");
    QCommandLineOption forcestartOpt(QStringList() << (char*)FORCESTART_STR,
                              QObject::tr("Start at once without waiting for the finish of other FastCopy executing."),
                              "true|false",
                              "");
    QCommandLineOption skipemptydirOpt(QStringList() << (char*)SKIPEMPTYDIR_STR,
                              QObject::tr("Skip to create empty directories when /include or /exclude option is used."),
                              "true|false",
                              "");
    QCommandLineOption iserrstopOpt(QStringList() << (char*)ERRSTOP_STR,
                              QObject::tr("Show error dialog (and operation is interrupted), if an error occurred."),
                              "true|false",
                              "");

    QCommandLineOption isestimateOpt(QStringList() << (char*)ESTIMATE_STR,
                              QObject::tr("Estimate complete time."));
    QCommandLineOption isverifyOpt(QStringList() << (char*)VERIFY_STR,
                              QObject::tr("Verify written files data."),
                              "md5|sh1|xxh|s22|s25|s32|s35|fal",
                              "");
    QCommandLineOption speedOpt(QStringList() << (char*)SPEED_STR,
                              QObject::tr("Specify speed control level."),
                              "full|autoslow|1-9|suspend",
                              "");
    /* wipedelと効能同じ。nsa有効にしたかったのかなあ？封印しちゃう
    QCommandLineOption owdelOpt(QStringList() << (char*)OWDEL_STR,
                              QObject::tr("Rename filename and wipe(overwrite Random data) before deleting."),
                              "true|false",
                              "");
    */
    QCommandLineOption wipedelOpt(QStringList() << (char*)WIPEDEL_STR,
                              QObject::tr("Rename filename and wipe(overwrite Random data) before deleting."),
                              "true|false",
                              "");
    QCommandLineOption aclOpt(QStringList() << (char*)ACL_STR,
                              QObject::tr("Copy ACL (only HFS+)."),
                              "true|false",
                              "");
    QCommandLineOption xattrOpt(QStringList() << (char*)STREAM_STR,
                              QObject::tr("Copy EA(Extend Attribute)."),
                              "true|false",
                              "");
    QCommandLineOption linkdestOpt(QStringList() << (char*)LINKDEST_STR,
                              QObject::tr("Reproduce hardlink as much as possible."),
                              "true|false",
                              "1");
    QCommandLineOption recreateOpt(QStringList() << (char*)RECREATE_STR,
                              QObject::tr("Change updating behavior ""overwrite the target"" to ""delete and recreate the target"". (If /linkdest option is enabled, this option is enabled by default.) If you want always to enable, write [main] recreate=1 in RapidCopy.ini"),
                              "true",
                              "");
    QCommandLineOption srcfileOpt(QStringList() << (char*)SRCFILE_STR,
                              QObject::tr("Specify source files by textfile. User is able to describe 1 filename per line."),
                              "filename",
                              "");
    QCommandLineOption openwinOpt(QStringList() << (char*)OPENWIN_STR,
                              QObject::tr("Don't stored in the task tray."));

    QCommandLineOption autocloseOpt(QStringList() << (char*)AUTOCLOSE_STR,
                              QObject::tr("Close automatically after execution is finished with no errors."));
    QCommandLineOption forcecloseOpt(QStringList() << (char*)FORCECLOSE_STR,
                              QObject::tr("Close automatically and forcedly after execution is finished."));

    QCommandLineOption noexecOpt(QStringList() << (char*)NOEXEC_STR,
                              QObject::tr("Don't start to execute."));
    QCommandLineOption diskmodeOpt(QStringList() << (char*)DISKMODE_STR,
                              QObject::tr("Specify Auto/Same/Diff HDD mode. (default: auto)"),
                              "auto|same|diff",
                              "");
    QCommandLineOption includeOpt(QStringList() << (char*)INCLUDE_STR,
                              QObject::tr("Specify include filter."),
                              "pattern",
                              "");
    QCommandLineOption excludeOpt(QStringList() << (char*)EXCLUDE_STR,
                              QObject::tr("Specify exclude filter."),
                              "pattern",
                              "");
    QCommandLineOption fromdateOpt(QStringList() << (char*)FROMDATE_STR,
                              QObject::tr("Specify oldest timestamp filter."),
                              "pattern",
                              "");
    QCommandLineOption todateOpt(QStringList() << (char*)TODATE_STR,
                              QObject::tr("Specify newest timestamp filter."),
                              "pattern",
                              "");
    QCommandLineOption minsizeOpt(QStringList() << (char*)MINSIZE_STR,
                              QObject::tr("Specify minimum size filter."),
                              "pattern",
                              "");
    QCommandLineOption maxsizeOpt(QStringList() << (char*)MAXSIZE_STR,
                              QObject::tr("Specify maximum size filter."),
                              "pattern",
                              "");
    QCommandLineOption noconfirmdelOpt(QStringList() << (char*)NOCONFIRMDEL_STR,
                              QObject::tr("Don't confirm before deleting."));
    QCommandLineOption noconfirmstopOpt(QStringList() << (char*)NOCONFIRMSTOP_STR,
                              QObject::tr("Don't Show error dialog, Even if critical errors occurred."));
    QCommandLineOption finactOpt(QStringList() << (char*)FINACT_STR,
                              QObject::tr("Specify post-process action name."),
                              "finactname|false",
                              "");
    QCommandLineOption toOpt(QStringList() << (char*)TO_STR,
                              QObject::tr("Destination Directory.(If delete mode is specified, then ""to"" option isn't used."),
                              "filename",
                              "");
    QCommandLineOption ltfsOpt(QStringList() << (char*)LTFS_STR,
                              QObject::tr("LTFS mode."),
                              "true|false");
    QCommandLineOption dotOpt(QStringList() << (char*)DOTIGNORE_STR,
                              QObject::tr("Ignore hidden files and folders that begin from the ""."""),
                              "true|false");
    QCommandLineOption xOpt(QStringList() << (char*)X_STR,
                              QObject::tr("Specifies the window x position at startup. Must specify with --ypos."),
                              "xpos",
                              "0");
    QCommandLineOption yOpt(QStringList() << (char*)Y_STR,
                              QObject::tr("Specifies the window y position at startup. Must specify with --xpos."),
                              "ypos",
                              "0");
    QCommandLineOption csvfilelogOpt(QStringList() << (char*)CSV_STR,
                              QObject::tr("Write to the csv file(based on deteiled file log)It is stored ""YYYY/MM/DD-hh:mm:ss-N.csv"" in RapidCopy/Log directory."),
                              "true|filename",
                              "");

    parser.addOption(cmdOpt);
    parser.addOption(jobOpt);
    parser.addOption(joblistOpt);
    parser.addOption(bufOpt);
    parser.addOption(filelogOpt);
    parser.addOption(csvfilelogOpt);
    parser.addOption(logfileOpt);
    parser.addOption(logOpt);
    parser.addOption(reparseOpt);
    parser.addOption(forcestartOpt);
    parser.addOption(skipemptydirOpt);
    parser.addOption(iserrstopOpt);
    parser.addOption(isestimateOpt);
    parser.addOption(isverifyOpt);
    parser.addOption(speedOpt);
    parser.addOption(wipedelOpt);
    parser.addOption(aclOpt);
    parser.addOption(xattrOpt);
    parser.addOption(linkdestOpt);
    parser.addOption(recreateOpt);
    parser.addOption(srcfileOpt);
    parser.addOption(openwinOpt);
    parser.addOption(autocloseOpt);
    parser.addOption(forcecloseOpt);
    parser.addOption(noexecOpt);
    parser.addOption(diskmodeOpt);
    parser.addOption(includeOpt);
    parser.addOption(excludeOpt);
    parser.addOption(fromdateOpt);
    parser.addOption(todateOpt);
    parser.addOption(minsizeOpt);
    parser.addOption(maxsizeOpt);
    parser.addOption(noconfirmdelOpt);
    parser.addOption(noconfirmstopOpt);
    parser.addOption(finactOpt);
    parser.addOption(toOpt);
    parser.addOption(ltfsOpt);
    parser.addOption(dotOpt);
    parser.addOption(xOpt);
    parser.addOption(yOpt);

    parser.parse(argv);
    //unknownOptionNames()はparseかprocess()してからじゃないと実行できないぞ
    if(!parser.unknownOptionNames().isEmpty()){
        QString error_str(QObject::tr("illgal option -- "));
        for(int i=0;i<parser.unknownOptionNames().count();i++){
            error_str.append(" \"" + parser.unknownOptionNames().at(i) + "\"");
        }
        error_str.append("\n");
        QTextStream(stderr) << error_str;
        argerr = CMD_ARG_ERR;
        goto end;
    }
    //process()は不正引数が含まれてると勝手にexit()しちゃうのでparse()して自分で後処理の方が一般的かねえ。
    parser.process(argv);

    //ltfsオプションはcmd指定をあとで判定する必要があるので、特別に先に処理
    if(parser.isSet(ltfsOpt)){
        if(GetArgOpt(parser.value(ltfsOpt).toLocal8Bit().data(),true)){
            //LTFS専用のFastCopy::BY_SIZEを有効化
            ui->checkBox_LTFS->setChecked(true);
            if(GetOverWriteMode() == FastCopy::BY_ATTR || GetOverWriteMode() == FastCopy::BY_LASTEST){
                //LTFSには日付ベース適用不可なのでエラー
                QTextStream(stderr) << (char*)GetLoadStrV(IDS_CMDLTFSATTR_ERROR);
                QTextStream(stderr) << parser.helpText();
                argerr = CMD_ARG_ERR;
                goto end;
            }
        }
        else{
            ui->checkBox_LTFS->setChecked(false);
        }
    }

    if(parser.isSet(cmdOpt)){
        int idx = CmdNameToComboIndex(parser.value(cmdOpt).toLocal8Bit().data());
        if(idx == -1){
            QTextStream(stderr) << (char*)GetLoadStrV(IDS_CMDNOTFOUND);
            QTextStream(stderr) << parser.helpText();
            argerr = CMD_ARG_ERR;
            goto end;
        }

        ui->comboBox_Mode->setCurrentIndex(idx);
        //GUIの辻褄合わせはSLOT任せ
        //verify専用モード?
        if(GetCopyMode() == FastCopy::VERIFY_MODE){
            //ベリファイ強制ON
            exec_flag |= LISTING_EXEC;
            exec_flag |= VERIFYONLY_EXEC;
        }
    }
    if(parser.isSet(jobOpt)){
        job_idx = cfg.SearchJobV(parser.value(jobOpt).toLocal8Bit().data());
        if(job_idx == -1){
            QTextStream(stderr) << (char*)GetLoadStrV(IDS_JOBNOTFOUND);
            argerr = CMD_ARG_ERR;
            goto end;
        }
        SetJob(job_idx);
    }
    if(parser.isSet(bufOpt)){
        ui->lineEdit_BUFF->setText(parser.value(bufOpt));
    }
    if(parser.isSet(filelogOpt)){
        if(parser.value(filelogOpt).compare((char*)GetLoadStrV(IDS_TRUE)) == 0){
            fileLogMode = Cfg::AUTO_FILELOG;
        }
        else if(parser.value(filelogOpt).compare((char*)GetLoadStrV(IDS_FALSE)) == 0){
            //出力しない場合は何も出せないので無条件に0
            fileLogMode = Cfg::NO_FILELOG;
        }
        else{
            strcpyV(fileLogPathV,parser.value(filelogOpt).toLocal8Bit().data());
            fileLogMode = Cfg::FIX_FILELOG;
        }
    }
    if (parser.isSet(logfileOpt)){
        strcpyV(errLogPathV,parser.value(logfileOpt).toLocal8Bit().data());
    }
    if(parser.isSet(logOpt)){
        GetArgOpt(parser.value(logOpt).toLocal8Bit().data(),true) ? isErrLog=true : isErrLog=false;
    }
    if(parser.isSet(reparseOpt)){
        GetArgOpt(parser.value(reparseOpt).toLocal8Bit().data(),true) ? isReparse=true : isReparse=false;
    }
    if(parser.isSet(forcestartOpt)){
        GetArgOpt(parser.value(forcestartOpt).toLocal8Bit().data(),true) ? forceStart=true : forceStart=false;
    }
    if(parser.isSet(skipemptydirOpt)){
        GetArgOpt(parser.value(skipemptydirOpt).toLocal8Bit().data(),true) ? skipEmptyDir=true : skipEmptyDir=false;
    }
    if(parser.isSet(iserrstopOpt)){
        GetArgOpt(parser.value(iserrstopOpt).toLocal8Bit().data(),true) ?
        ui->checkBox_Nonstop->setChecked(false) : ui->checkBox_Nonstop->setChecked(true);
    }
    if(parser.isSet(isestimateOpt)){
        is_estimate = true;
    }
    if(parser.isSet(isverifyOpt)){
        QString verifyopt = parser.value(isverifyOpt);
        if(verifyopt.compare((char*)GetLoadStrV(IDS_VERIFY_FALSE_OPT)) == 0){
            //ベリファイ抑止
            ui->checkBox_Verify->setChecked(false);
        }
        else{
            cfg.usingMD5 =   verifyopt.compare((char*)GetLoadStrV(IDS_VERIFYMD5_OPT)) == 0 ? TDigest::MD5 :
                             verifyopt.compare((char*)GetLoadStrV(IDS_VERIFYSHA1_OPT)) == 0 ? TDigest::SHA1 :
                             verifyopt.compare((char*)GetLoadStrV(IDS_VERIFYXX_OPT)) == 0 ? TDigest::XX :
                             verifyopt.compare((char*)GetLoadStrV(IDS_VERIFYSHA2_256_OPT)) == 0 ? TDigest::SHA2_256 :
                             verifyopt.compare((char*)GetLoadStrV(IDS_VERIFYSHA2_512_OPT)) == 0 ? TDigest::SHA2_512 :
                             verifyopt.compare((char*)GetLoadStrV(IDS_VERIFYSHA3_256_OPT)) == 0 ? TDigest::SHA3_256 :
                             verifyopt.compare((char*)GetLoadStrV(IDS_VERIFYSHA3_512_OPT)) == 0 ? TDigest::SHA3_512 :
                              -1;
            if(cfg.usingMD5 == -1){
                QTextStream(stderr) << (char*)GetLoadStrV(IDS_VERIFYNOTFOUND);
                QTextStream(stderr) << parser.helpText();
                argerr = CMD_ARG_ERR;
                goto end;
            }
            ui->checkBox_Verify->setChecked(true);
        }
    }
    if(parser.isSet(speedOpt)){
        speedLevel = lstrcmpiV(parser.value(speedOpt).toLocal8Bit().data(), SPEED_FULL_STR) == 0 ? SPEED_FULL :
            lstrcmpiV(parser.value(speedOpt).toLocal8Bit().data(), SPEED_AUTOSLOW_STR) == 0 ? SPEED_AUTO :
            lstrcmpiV(parser.value(speedOpt).toLocal8Bit().data(), SPEED_SUSPEND_STR) == 0 ? SPEED_SUSPEND :
            GetArgOpt(parser.value(speedOpt).toLocal8Bit().data(), SPEED_FULL);
        speedLevel = min(speedLevel, SPEED_FULL);
        SetSpeedLevelLabel(speedLevel);
    }
    if(parser.isSet(wipedelOpt)){
        bool owdel = GetArgOpt(parser.value(wipedelOpt).toLocal8Bit().data(),true) ? true : false;
        ui->checkBox_owdel->setChecked(owdel);
    }
    if(parser.isSet(aclOpt)){
        GetArgOpt(parser.value(aclOpt).toLocal8Bit().data(),true) ?
        ui->checkBox_Acl->setChecked(true) : ui->checkBox_Acl->setChecked(false);
    }
    if(parser.isSet(xattrOpt)){
        GetArgOpt(parser.value(xattrOpt).toLocal8Bit().data(),true) ?
        ui->checkBox_xattr->setChecked(true) : ui->checkBox_xattr->setChecked(false);
    }
    if(parser.isSet(linkdestOpt)){
        int	val = GetArgOpt(parser.value(linkdestOpt).toLocal8Bit().data(), TRUE);
        if (val > 1 || val < 0) {
            if (val < 300000) {
                QTextStream(stderr) << QObject::tr("Too small(<300000) hashtable for linkdest.\n");
                argerr = CMD_ARG_ERR;
                goto end;
            }
            maxLinkHash = val;
            isLinkDest = TRUE;
        } else {
            isLinkDest = val ? true : false;	// TRUE or FALSE
        }
    }
    if(parser.isSet(recreateOpt)){
        GetArgOpt(parser.value(recreateOpt).toLocal8Bit().data(),true) ?
        isReCreate = true : isReCreate = false;
    }
    if(parser.isSet(srcfileOpt)){
        if (!MakeFileToPathArray(parser.value(srcfileOpt).toLocal8Bit().data(), &pathArray)) {
            QTextStream(stderr) << QObject::tr("Can't open:") << parser.value(srcfileOpt);
            argerr = CMD_ARG_ERR;
            goto end;
        }
    }
    if(parser.isSet(openwinOpt)){
        is_openwin = true;
    }
    if(parser.isSet(autocloseOpt) && autoCloseLevel != FORCE_CLOSE){
        autoCloseLevel = NOERR_CLOSE;
    }
    if(parser.isSet(forcecloseOpt)){
        autoCloseLevel = FORCE_CLOSE;
    }
    if(parser.isSet(noexecOpt)){
        is_noexec = true;
    }
    if(parser.isSet(diskmodeOpt)){
        if(lstrcmpiV(parser.value(diskmodeOpt).toLocal8Bit().data(), DISKMODE_SAME_STR) == 0){
            diskMode = 1;
        }
        else if (lstrcmpiV(parser.value(diskmodeOpt).toLocal8Bit().data(), DISKMODE_DIFF_STR) == 0){
            diskMode = 2;
        }
        else{
            diskMode = 0;
        }
    }
    if(parser.isSet(includeOpt)){
        filter_mode |= NORMAL_FILTER;
        ui->comboBox_include->setCurrentText(parser.value(includeOpt));
    }
    if(parser.isSet(excludeOpt)){
        filter_mode |= NORMAL_FILTER;
        ui->comboBox_exclude->setCurrentText(parser.value(excludeOpt));
    }
    if(parser.isSet(fromdateOpt)){
        filter_mode |= EXTEND_FILTER;
        ui->comboBox_FromDate->setCurrentText(parser.value(fromdateOpt));
    }
    if(parser.isSet(todateOpt)){
        filter_mode |= EXTEND_FILTER;
        ui->comboBox_ToDate->setCurrentText(parser.value(todateOpt));
    }
    if(parser.isSet(minsizeOpt)){
        filter_mode |= EXTEND_FILTER;
        ui->comboBox_MinSize->setCurrentText(parser.value(minsizeOpt));
    }
    if(parser.isSet(maxsizeOpt)){
        filter_mode |= EXTEND_FILTER;
        ui->comboBox_MaxSize->setCurrentText(parser.value(maxsizeOpt));
    }
    if(parser.isSet(noconfirmdelOpt)){
        noConfirmDel = true;
    }
    if(parser.isSet(noconfirmstopOpt)){
        noConfirmStop = true;
    }
    if(parser.isSet(finactOpt)){
        if (lstrcmpiV(parser.value(finactOpt).toLocal8Bit().data(),FALSE_V) == 0){
            SetFinAct(-1);
        }
        else {
            int idx = cfg.SearchFinActV(parser.value(finactOpt).toLocal8Bit().data(),true);
            if (idx >= 0){
                SetFinAct(idx);
            }
            else{
                QTextStream(stderr) << (char*)GetLoadStrV(IDS_FINACTNOTFOUND);
                argerr = CMD_ARG_ERR;
                goto end;
            }
        }
    }
    if(parser.isSet(toOpt)){
        ui->plainTextEdit_Dst->setPlainText(parser.value(toOpt).toLocal8Bit().data());
        dst_path = parser.value(toOpt).toLocal8Bit().data();
    }
    if(parser.isSet(dotOpt)){
        GetArgOpt(parser.value(dotOpt).toLocal8Bit().data(),true) ?
            dotignoremode = true : dotignoremode = false;
    }
    //「新しいウインドウを開く」延長で開始した場合はxposとyposを元ウインドウからちょっとずらした所から開始する
    if(parser.isSet(xOpt)){
        xval = GetArgOpt(parser.value(xOpt).toLocal8Bit().data(), 0);
    }
    if(parser.isSet(yOpt)){
        yval = GetArgOpt(parser.value(yOpt).toLocal8Bit().data(), 0);
    }

    //xとy両方指定している
    if(xval != -1 && yval != -1){
        //ウインドウ位置移動
        this->move(xval,yval);
    }
    else if(xval == -1 && yval == -1){
        //xとy両方指定していないので、なにもしない
    }
    else{
        //片方だけ指定しているのでエラー
        QTextStream(stderr) << (char*)GetLoadStrV(IDS_XYARG_NOTFOUND);
        QTextStream(stderr) << parser.helpText();
        argerr = CMD_ARG_ERR;
        goto end;
    }
    if(parser.isSet(csvfilelogOpt)){
        if(parser.value(csvfilelogOpt).compare((char*)GetLoadStrV(IDS_TRUE)) == 0){
            //CSV出力は詳細ファイルログ有効じゃないと使えない。詳細ファイルログが有効かどうかチェック
            if((fileLogMode & Cfg::AUTO_FILELOG || fileLogMode & Cfg::FIX_FILELOG)){
                //詳細ファイルログ有効だったのでCSV出力有効にするに
                fileLogMode |= Cfg::ADD_CSVFILELOG;
            }
            else{
                //詳細ファイルログ有効じゃないのにcsv出力要求してる？
                QTextStream(stderr) << (char*)GetLoadStrV(IDS_CMDCSVWITHFILELOG_ERROR);
                QTextStream(stderr) << parser.helpText();
                argerr = CMD_ARG_ERR;
                goto end;
            }
        }
        else if(parser.value(csvfilelogOpt).compare((char*)GetLoadStrV(IDS_FALSE)) == 0){
            //CSV出力ビットを落とす
            fileLogMode &= ~Cfg::ADD_CSVFILELOG;
        }
        else{
            //trueでもfalseでもないのを指定してる
            QTextStream(stderr) << (char*)GetLoadStrV(IDS_CMDCSVWITHFILELOG_ERROR);
            QTextStream(stderr) << parser.helpText();
            argerr = CMD_ARG_ERR;
            goto end;
        }
    }
    if(parser.isSet(joblistOpt)){
        joblist_idx = cfg.SearchJobList(parser.value(joblistOpt).toLocal8Bit().data());
        if (joblist_idx == -1) {
            QTextStream(stderr) << (char*)GetLoadStrV(IDS_JOBLISTNOTFOUND);
            argerr = CMD_ARG_ERR;
            goto end;
        }
        ui->actionEnable_JobList_Mode->setChecked(true);
        ui->comboBox_JobListName->setCurrentIndex(joblist_idx);
        on_comboBox_JobListName_activated(joblist_idx);
        if(!is_noexec){
            //click event post(working after Qmain.exec());
            ui->pushButton_JobListStart->click();
        }
    }

end:
    //ここまでのコマンド解析でエラー一個でもあれば終了
    if(argerr == CMD_ARG_ERR){
        //必要なエラーメッセージは手前で出しておいてね
        this->close();
        exit(argerr);
    }

    is_delete = GetCopyMode() == FastCopy::DELETE_MODE;
    is_verifymode = GetCopyMode() == FastCopy::VERIFY_MODE;

    if(job_idx == -1){
        if(!dst_path){
            ui->plainTextEdit_Dst->clear();
        }
        ui->plainTextEdit_Src->clear();
    }

    QStringList src = parser.positionalArguments();
    for(int i=0;i<src.count();i++){
        pathArray.RegisterPath(src.at(i).toLocal8Bit().data());
    }
    int		path_len = pathArray.GetMultiPathLen();
    void	*path = new char [path_len];
    if(pathArray.GetMultiPath(path, path_len)){
        ui->plainTextEdit_Src->setPlainText((char*)path);
        //no-execでセットするだけだと行ごとの色分け上手くいかないなあ。
        //まあばれないし、中でenterすれば問題ないから突っ込まれるまでほっとくか。。
        //castPath(ui->plainTextEdit_Src);
        //plainTextEdit_blockCountChanged(src.count());
    }
    delete [] path;

    if(((filter_mode & EXTEND_FILTER) || (filter_mode & NORMAL_FILTER))){
        //isExtendFilter = TRUE;
        //SetExtendFilter();
        ui->actionShow_Extended_Filter->setChecked(true);
    }

    if(GetSrc().isEmpty() || (!is_delete && GetDst().isEmpty())){
        is_noexec = true;
    }

    if(is_verifymode){
        //ベリファイモード専用GUIに設定しなおす
        SetItemEnable_Verify(false);
    }
    else{
        SetItemEnable(is_delete);
    }
    if(diskMode){
        UpdateMenu();
    }
    if(!is_delete && (is_estimate ||
                (!isShellExt && !is_noexec && cfg.estimateMode != is_estimate))){
            ui->checkBox_Estimate->setChecked(is_estimate);
    }
    if (is_openwin || is_noexec) {
        this->Show();
        isRunAsshow = true;
    }
    else {
        isRunAsshow = false;
        if (cfg.taskbarMode) {
            Show(SW_MINIMIZE);
        }
        else {

            TaskTray(NIM_ADD, hMainIcon[FCNORMAL_ICON_IDX], FASTCOPY);
        }
    }

    return	(joblist_idx != -1  || is_noexec) ? TRUE : ExecCopy(exec_flag);
}

void MainWindow::Show(int mode)
{
    if (mode != SW_HIDE && mode != SW_MINIMIZE) {
        SetupWindow();
    }
    else{
        this->hide();
    }
}

//引数targetのQPlainTextEditを良い感じの改行成形する
void MainWindow::castPath(QPlainTextEdit *target,QString append_path,int append_path_count,bool append_req){

    //現在のフィールド内のパス数を取得
    int rowcount = target->blockCount();
    char wbuf[1024];
    //フィールド内の既存パスにパスを追加？
    if(append_req){
        rowcount = rowcount + append_path_count;
        target->setPlainText(target->toPlainText() + append_path);
    }
    else{
        rowcount = append_path_count;
        target->clear();
        target->setPlainText(append_path);
    }

    if(append_req){
        sprintf(wbuf,(char*)GetLoadStrV(IDS_PATH_ADDED),append_path_count,rowcount);
        status_label.setText(wbuf);
    }
    else{
        sprintf(wbuf,(char*)GetLoadStrV(IDS_PATH_INPUT),append_path_count);
        status_label.setText(wbuf);
    }

    QFontMetrics m (ui->plainTextEdit_Src->font());

    int RowHeightpx = ((m.height() * (rowcount + 1)) + 30);	//1は最終行の;ないやつぶん。
                                                            //10はfont以外のQplainTextEditの端っこ部分のなんとなく
    //横幅を動的に調節・・・・しなくてもいいかもなーとりあえず残してるけど
    //最大高をいい感じに設定
    ui->plainTextEdit_Src->setMaximumHeight(RowHeightpx);

    //パス追加要求？
    if(append_req){
        //追加する場合、スクロールバーで隠れるとなにを追加したのかがわかりにくい。
        //カーソルを末尾に強制移動
        ui->plainTextEdit_Src->textCursor().movePosition(QTextCursor::End);
        //スクロールバーを末尾に強制移動
        ui->plainTextEdit_Src->verticalScrollBar()->setValue(ui->plainTextEdit_Src->verticalScrollBar()->maximum());
    }
}

void MainWindow::LaunchNextJob(){
    //ジョブリストがキャンセルされてない？かつ次のジョブが存在する？
    if(!joblistiscancel && ++joblistcurrentidx < joblist.count()){
        SetJob(joblistcurrentidx,true,true);
        joblist[joblistcurrentidx].isListing ?
            ui->pushButton_Listing->click() : ui->pushButton_Exe->click();
    }
    else{
        //ジョブリスト実行がキャンセルされたか、全部終わったかな
        joblistisstarting = false;
        joblistiscancel = false;
        joblist_joberrornum = 0;
        ui->pushButton_JobListStart->setText((char*)GetLoadStrV(IDS_JOBLIST_EXECUTE));
        //joblist実行時のRapidCopyインスタンス待ち強制無効化を	元に戻す
        forceStart = cfg.forceStart;
        //joblist操作用ウィジェットの操作ロック解除
        EnableJobList(true);
        EnableJobMenus(true);
    }
    return;
}

//ジョブリスト実行時、ダイアログ生成有無確認
bool MainWindow::isJobListwithSilent(){
    return(joblistisstarting && ui->checkBox_JobListForceLaunch->isChecked());
}

bool MainWindow::UpdateJobListtoini(){

    bool ret = true;
    JobList wk_list;
    wk_list.joblist_title = strdupV(ui->comboBox_JobListName->currentText().toLocal8Bit().data());
    wk_list.forcelaunch = ui->checkBox_JobListForceLaunch->isChecked();

    //JobListを全削除する
    cfg.DelJobList(wk_list.joblist_title);
    for(int i=0;i<joblist.count();i++){
        cfg.AddJobV(&joblist.at(i));
        wk_list.jobname_pt[i] = strdupV(joblist.at(i).title);
        wk_list.joblistentry++;
    }
    if(cfg.AddJobList(&wk_list)){
        cfg.WriteIni();
        //セーブした時はComboBoxもListWidgetもカレント状態をそのままキープしておきたいので
        //Itemだけ追加にしておくか
        //既に存在するジョブリストのデータ上書きかどうかチェック
        if(cfg.SearchJobList(ui->comboBox_JobListName->currentText().toLocal8Bit().data()) == -1){
            //上書きじゃなかったので、ジョブリストコンボボックスに作成したジョブリスト名を追加
            ui->comboBox_JobListName->addItem((char*)wk_list.joblist_title);
        }
        UpdateMenu();
        UpdateJobList(true);
    }
    else{
        ret = false;
    }
    return(ret);
}

bool MainWindow::DeleteJobListtoini(QString del_joblistname){
    bool ret = false;

    if(cfg.DelJobList(del_joblistname.toLocal8Bit().data())){
        //ジョブリスト内の各ジョブの削除で他のリストに所属してないジョブがいなければ削除
        for(int i=0;i<joblist.count();i++){
            if(cfg.DelJobinJobList(joblist.at(i).title,
                                   ui->comboBox_JobListName->currentText().toLocal8Bit().data())
                                    == 0){
                //他ジョブリスト上の存在数0なのでJob自体も削除する。
                cfg.DelJobV(joblist.at(i).title);
            }
        }
        cfg.WriteIni();
        ret = true;
    }
    else{
        ret = false;
    }
    return ret;
}

BOOL MainWindow::TaskTray(int nimMode, QIcon *hSetIcon,char* tip)
{
    isTaskTray = nimMode == NIM_DELETE ? FALSE : TRUE;

    if(cfg.taskbarMode){
        if(!hSetIcon) hSetIcon = hMainIcon[FCNORMAL_ICON_IDX];
        if(systray.isVisible()){
            systray.hide();
        }
    }
    else{

        if(nimMode == NIM_DELETE){
            if(systray.isVisible()){
                systray.hide();
            }
        }
        else{
            systray.setIcon(*hSetIcon);
            if(tip){
                systray.setToolTip((char*)tip);
                //this->setToolTip((char*)tip);
            }
            if(!systray.isVisible()){
                systray.show();
            }
        }

    }
    return(true);
}

BOOL MainWindow::StartFileLog()
{
    char	logDir[MAX_PATH] = "";
    char	wbuf[MAX_PATH * 2]   = "";
    int flags = O_RDWR | O_CREAT;
    struct stat wk_stat;

    if (fileLogMode == Cfg::NO_FILELOG){
        return FALSE;
    }

    if (fileLogMode & Cfg::AUTO_FILELOG || strchrV(fileLogPathV, '/') == 0) {

        MakePathV(logDir, cfg.userDirV.toStdString().c_str(), GetLoadStrV(IDS_FILELOG_SUBDIR));

        //出力先のディレクトリが存在する？
        if(stat((const char*)&logDir,&wk_stat) == SYSCALL_ERR
            && errno == ENOENT){
            //ないのね
            //再帰的にdir作るの面倒なので、QDir任せ
            QString logdir_str(logDir);
            QDir Qlogdir(logdir_str);
            Qlogdir.mkpath(logdir_str);
        }

        if (fileLogMode & Cfg::FIX_FILELOG && *fileLogPathV) {
            strcpyV(wbuf, fileLogPathV);
            MakePathV(fileLogPathV, logDir, wbuf);
            //CSV出力も同時に行うかチェック
            if(fileLogMode & Cfg::ADD_CSVFILELOG && *fileCsvPathV){
                strcpyV(wbuf,fileCsvPathV);
                MakePathV(fileCsvPathV, logDir, wbuf);
            }
        }
    }

    if (fileLogMode & Cfg::AUTO_FILELOG || *fileLogPathV == 0){
        //JobList実行時は最初のジョブ開始時刻を強制
        struct tm st = joblistisstarting ? jobliststarttm : startTm;

        for (int i=0; i < 100; i++) {

            //ジョブリストモードで実行中かつ、現在実行中のジョブは最初のジョブじゃない？
            if(joblistisstarting && !IsFirstJob()){
                //ファイルを最初に開いたジョブ枝番のファイルに合わせる
                i = joblistcurrentfileidx;
            }
            sprintfV(wbuf, GetLoadStrV(IDS_FILELOGNAME),
                st.tm_year + TM_YEAR_OFFSET,
                st.tm_mon + TM_MONTH_OFFSET,
                st.tm_mday,
                st.tm_hour,
                st.tm_min,
                st.tm_sec,
                (joblistisstarting && !IsFirstJob()) ? joblistcurrentfileidx : i,
                joblistisstarting ? (ui->comboBox_JobListName->currentText().prepend("-").toLocal8Bit().data()) : "");
            MakePathV(fileLogPathV, logDir, wbuf);
            //既に存在していたら別の枝番付与するためにもう一周。
            //10ms内で大量に同時に走ったりはしないでしょう的な意味で100なんだべな。
            //statによるチェック後のopenだとスレッドセーフじゃない、。
            //最初からO_EXCL指定すればよかったんじゃん:(
            //ファイルオープンする。すでに同一ファイルが存在する場合は失敗
            hFileLog = open(fileLogPathV,
                            (joblistisstarting && !IsFirstJob()) ? flags : flags | O_EXCL,
                            0777);
            //open失敗？
            if(hFileLog == SYSCALL_ERR){
                //失敗理由 = 既にファイル存在する？
                if(errno == EEXIST){
                    //他のRapidCopyに先をこされたかぁ。次で成功するはずなんでcontinue
                    continue;
                }
                else{
                    //致命的失敗発生
                    break;
                }
            }
            else{
                //成功した
                if((joblistisstarting && IsFirstJob())){
                    //nextジョブ以降に同じファイルを開き続ける必要があるので
                    //作成したファイル枝番を記憶
                    joblistcurrentfileidx = i;
                }
                //JobListモード時の複数ジョブリスト同時実行競合よけのために開いたファイル枝番を退避
                if(fileLogMode & Cfg::ADD_CSVFILELOG){
                    sprintfV(wbuf, GetLoadStrV(IDS_FILECSVNAME),
                        st.tm_year + TM_YEAR_OFFSET,
                        st.tm_mon + TM_MONTH_OFFSET,
                        st.tm_mday,
                        st.tm_hour,
                        st.tm_min,
                        st.tm_sec,
                        joblistisstarting ? joblistcurrentfileidx : i,
                        joblistisstarting ? (ui->comboBox_JobListName->currentText().prepend("-").toLocal8Bit().data()) : "");
                    MakePathV(fileCsvPathV, logDir, wbuf);
                    hcsvFileLog = open(fileCsvPathV,O_RDWR | O_CREAT,0777);
                    //エラーチェックさぼる。ファイル作成の競合は手前のhFileLogで弾かれてるので絶対に起きないはず。
                    //その他のエラーはまあどうでもいいから無視しちゃお。。
                }
                break;
            }
        }
    }
    else {
        hFileLog = open(fileLogPathV,O_RDWR | O_CREAT,0777);
        if(fileLogMode & Cfg::ADD_CSVFILELOG){
            //FIXの場合はfilelog指定文字列 + ".csv"を強制付与して動かす。
            strncpy(fileCsvPathV,fileLogPathV,sizeof(fileLogPathV));
            strncat(fileCsvPathV,".csv",sizeof(fileLogPathV));
            hcsvFileLog = open(fileCsvPathV,O_RDWR | O_CREAT,0777);
        }
    }

    if(hFileLog == SYSCALL_ERR) return FALSE;
    if(fileLogMode & Cfg::ADD_CSVFILELOG && hcsvFileLog == SYSCALL_ERR) return false;

    WriteLogHeader(hFileLog,false);
    //CSVファイル出力予定ありなら出力
    if(hcsvFileLog != SYSCALL_ERR){
        WriteLogHeader(hcsvFileLog,false,true);
    }
    return	TRUE;
}

void MainWindow::EndFileLog()
{
    if (hFileLog != SYSCALL_ERR) {
        //Listingのデータは既にすべて出力済み
        //macでは重複出力しても問題ないけど、無駄にwrite呼ぶの気持ち悪いので回避
        if(!IsListing()){
            SetFileLogInfo();
        }
        WriteLogFooter(hFileLog);
        ::close(hFileLog);
        if(hcsvFileLog != SYSCALL_ERR){
            WriteLogFooter(hcsvFileLog,true);
            ::close(hcsvFileLog);
        }
    }

    if (!(fileLogMode & Cfg::FIX_FILELOG)) {
        *fileLogPathV = 0;
        //csv file path delete.
        if(fileLogMode & Cfg::ADD_CSVFILELOG && *fileCsvPathV){
            *fileCsvPathV = 0;
        }
    }
    hFileLog = SYSCALL_ERR;
    hcsvFileLog = SYSCALL_ERR;
}

//デバッグ用read/write統計情報専用Log取得
bool MainWindow::StartStatLog(){
    struct tm st = startTm;
    char	logDir[MAX_PATH] = "";
    char	wbuf[MAX_PATH]   = "";

    if(!cfg.rwstat) return false;

    MakePathV(logDir, cfg.statDirV.toStdString().c_str(), EMPTY_STR_V);
    sprintfV(wbuf, GetLoadStrV(IDS_FILELOGNAME),
        st.tm_year + TM_YEAR_OFFSET, st.tm_mon + TM_MONTH_OFFSET, st.tm_mday, st.tm_hour,
        st.tm_min, st.tm_sec, 0);
    MakePathV(statLogPathV, logDir, wbuf);

    hStatLog = open(statLogPathV,O_RDWR | O_CREAT,0777);
    if(hStatLog == SYSCALL_ERR) return false;

    return true;
}

void MainWindow::EndStatLog(){

    if(hStatLog != SYSCALL_ERR){
        ::close(hStatLog);
    }
    *statLogPathV = 0;
    hStatLog = SYSCALL_ERR;
}

void MainWindow::SetFileLogInfo()
{
    DWORD	size = ti.listBuf->UsedSize();
    lseek(hFileLog,0,SEEK_END);
    write(hFileLog,ti.listBuf->Buf(),size);
    ti.listBuf->SetUsedSize(0);
    if(hcsvFileLog != SYSCALL_ERR && ti.csvfileBuf->UsedSize()){
        size = ti.csvfileBuf->UsedSize();
        lseek(hcsvFileLog,0,SEEK_END);
        write(hcsvFileLog,ti.csvfileBuf->Buf(),size);
        csvfileBufOffset += ti.csvfileBuf->UsedSize();
        ti.csvfileBuf->SetUsedSize(0);
    }
}

void MainWindow::WriteStatInfo()
{
    ti.statCs->lock();

    DWORD	size = ti.statBuf->UsedSize();

    lseek(hStatLog,0,SEEK_END);
    write(hStatLog,ti.statBuf->Buf(),size);
    ti.statBuf->SetUsedSize(0);

    ti.statCs->unlock();
}

void MainWindow::SetListInfo()
{
    size_t	size = ti.listBuf->UsedSize();

    QTextCursor cur = ui->textEdit_PATH->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertText((char*)ti.listBuf->Buf());
    //Listing時もlogに出力
    if(hFileLog != SYSCALL_ERR){
        if(ti.listBuf->UsedSize()){
            write(hFileLog,ti.listBuf->Buf(),size);
        }
        //csv出力要求ありならcsvも出力
        if(hcsvFileLog != SYSCALL_ERR && ti.csvfileBuf->UsedSize()){
            size = ti.csvfileBuf->UsedSize();
            write(hcsvFileLog,ti.csvfileBuf->Buf(),size);
            csvfileBufOffset += ti.csvfileBuf->UsedSize();
            ti.csvfileBuf->SetUsedSize(0);
        }
    }

    listBufOffset += ti.listBuf->UsedSize();
    ti.listBuf->SetUsedSize(0);
}

BOOL MainWindow::SetTaskTrayInfo(BOOL is_finish_status, double doneRate,
                int remain_h, int remain_m, int remain_s)
{
    int		len = 0;
    char	buf[1024] = "";

    if(!cfg.taskbarMode){
        if(info.mode == FastCopy::DELETE_MODE){
            len += sprintf(buf + len, IsListing() ?
                "RapidCopy (%.1fMB %dfiles %02d:%02d:%02d)" :
                "RapidCopy (%.1fMB %dfiles %02d:%02d:%02d %.2fMB/s)",
                (double)ti.total.deleteTrans / (1024 * 1024),
                ti.total.deleteFiles,
                ti.tickCount / 1000 / 3600,
                ((ti.tickCount / 1000) % 3600) / 60,
                ((ti.tickCount / 1000) % 60),
                (double)ti.total.deleteFiles * 1000 / ti.tickCount);
        }
        else if(ti.total.isPreSearch){
            len += sprintf(buf + len, " Estimating (Total %.1f MB/%d files/%d sec)",
                (double)ti.total.preTrans / (1024 * 1024),
                ti.total.preFiles,
                (int)(ti.tickCount / 1000));
        }
        else{
            if((info.flags & FastCopy::PRE_SEARCH) && !ti.total.isPreSearch && !is_finish_status
                && doneRate >= 0.0001){
                len += sprintf(buf + len, "%d%% (Remain %02d:%02d:%02d) ",
                    doneRatePercent, remain_h, remain_m, remain_s);
            }
            len += sprintf(buf + len, IsListing() ?
                "RapidCopy (%.1fMB %dfiles %02d:%02d:%02d)" :
                "RapidCopy (%.1fMB %dfiles %02d:%02d:%02d %.2fMB/s)",
                (double)ti.total.writeTrans / (1024 * 1024),
                ti.total.writeFiles,
                ti.tickCount / 1000 / 3600,
                ((ti.tickCount / 1000) % 3600) / 60,
                ((ti.tickCount / 1000) % 60),
                (double)ti.total.writeTrans / ti.tickCount / 1024 * 1000 / 1024
                );
        }
        if(is_finish_status){
            strcpy(buf + len, " Finished");
        }
    }

    curIconIndex = is_finish_status ? 0 : (curIconIndex + 1) % MAX_NORMAL_FASTCOPY_ICON;
    TaskTray(NIM_MODIFY, hMainIcon[curIconIndex], buf);
    return	TRUE;
}

inline double SizeToCoef(const double &ave_size)
{
#define AVE_WATERMARK_MIN (4.0   * 1024)
#define AVE_WATERMARK_MID (64.0  * 1024)

    if (ave_size < AVE_WATERMARK_MIN) return 2.0;
    double ret = AVE_WATERMARK_MID / ave_size;
    if (ret >= 2.0) return 2.0;
    if (ret <= 0.1) return 0.1;
    return ret;
}

BOOL MainWindow::CalcInfo(double *doneRate, int *remain_sec, int *total_sec)
{
    _int64	preFiles, preTrans, doneFiles, doneTrans;
    double	realDoneRate, coef, real_coef;

    calcTimes++;

    if (info.mode == FastCopy::DELETE_MODE) {
        preFiles  = ti.total.preFiles    + ti.total.preDirs;
        doneFiles = ti.total.deleteFiles + ti.total.deleteDirs + ti.total.skipDirs
                     + ti.total.errDirs + ti.total.skipFiles + ti.total.errFiles;
        realDoneRate = *doneRate = doneFiles / (preFiles + 0.01);
        lastTotalSec = (int)(ti.tickCount / realDoneRate / 1000);
    }
    else {

        if(info.mode != FastCopy::VERIFY_MODE){
            preFiles = (ti.total.preFiles /* + ti.total.preDirs*/) * 2;
            preTrans = (ti.total.preTrans * 2);

            doneFiles = (ti.total.writeFiles + ti.total.readFiles /*+ ti.total.readDirs*/
                        /*+ ti.total.skipDirs*/ + (ti.total.skipFiles + ti.total.errFiles) * 2);

            doneTrans = ti.total.writeTrans + ti.total.readTrans
                        + (ti.total.skipTrans + ti.total.errTrans) * 2;

            if ((info.flags & FastCopy::VERIFY_FILE)) {
                int vcoef =  ti.isSameDrv ? 1 : 2;
                preFiles  += ti.total.preFiles * vcoef;
                preTrans  += ti.total.preTrans * vcoef;
                doneFiles += (ti.total.verifyFiles + ti.total.skipFiles + ti.total.errFiles) * vcoef;
                doneTrans += (ti.total.verifyTrans + ti.total.skipTrans + ti.total.errTrans) * vcoef;
            }
        }
        //ベリファイモード
        else{
            preFiles = (ti.total.preFiles) * 2;
            preTrans = (ti.total.preTrans * 2);

            doneFiles = (ti.total.writeFiles + ti.total.readFiles
                        + (ti.total.skipFiles + ti.total.errFiles) * 2);
            doneTrans = ti.total.writeTrans + ti.total.readTrans
                        + (ti.total.skipTrans + ti.total.errTrans);

            if ((info.flags & FastCopy::VERIFY_FILE)) {
                int vcoef =  1;
                preFiles  += ti.total.preFiles * vcoef;
                preTrans  += ti.total.preTrans * vcoef;
                doneFiles += (ti.total.verifyFiles + ti.total.skipFiles + ti.total.errFiles) * vcoef;
                doneTrans += ((ti.total.verifyTrans / 2) + ti.total.skipTrans + ti.total.errTrans) * vcoef;
            }
        }
        if (doneFiles > preFiles) preFiles = doneFiles;
        if (doneTrans > preTrans) preTrans = doneTrans;

        double doneTransRate = doneTrans / (preTrans  + 0.01);
        double doneFileRate  = doneFiles / (preFiles  + 0.01);
        double aveSize 	  = preTrans  / (preFiles  + 0.01);
//		double doneAveSize   = doneTrans / (doneFiles + 0.01);

        coef 	 = SizeToCoef(aveSize);
//		real_coef = coef * coef / SizeToCoef(doneAveSize);
        real_coef = coef;

        if (coef 	 > 100.0) coef 	 = 100.0;
        if (real_coef > 100.0) real_coef = 100.0;
        *doneRate    = (doneFileRate *	  coef + doneTransRate) / (	 coef + 1);
//		realDoneRate = (doneFileRate * real_coef + doneTransRate) / (real_coef + 1);
        realDoneRate = *doneRate;

        if (realDoneRate < 0.01) realDoneRate = 0.001;

        if (calcTimes < 10
        || !(calcTimes % (realDoneRate < 0.70 ? 10 : (11 - int(realDoneRate * 10))))) {
            lastTotalSec = (int)(ti.tickCount / realDoneRate / 1000);
//			Debug("recalc(%d) ", (realDoneRate < 0.70 ? 10 : (11 - int(realDoneRate * 10))));
        }
//		else	Debug("nocalc(%d) ", (realDoneRate < 0.70 ? 10 : (11 - int(realDoneRate * 10))));
    }

    *total_sec = lastTotalSec;
    if ((*remain_sec = *total_sec - (ti.tickCount / 1000)) < 0) *remain_sec = 0;

//	Debug("rate=%2d(%2d) coef=%2.1f(%2.1f) rmain=%5d total=%5d\n", int(*doneRate * 100),
//		int(realDoneRate*100), coef, real_coef, *remain_sec, *total_sec);

    return	TRUE;
}

BOOL MainWindow::SetInfo(BOOL is_finish_status)
{
    char	buf[1024];
    int	len = 0;
    double	doneRate;
    int	remain_sec, total_sec, remain_h, remain_m, remain_s;

    doneRatePercent = -1;

    fastCopy.GetTransInfo(&ti, is_finish_status || !isTaskTray);
    if (ti.tickCount == 0) {
        ti.tickCount++;
    }

    if((IsListing() && ti.listBuf->UsedSize() > 0)
        || ((info.flags & FastCopy::LISTING) && ti.listBuf->UsedSize() >= FILELOG_MINSIZE)){
        ti.listCs->lock();
        if(IsListing()){
            SetListInfo();
        }
        else{
            SetFileLogInfo();
        }
        ti.listCs->unlock();
    }
    if(info.flags_second & FastCopy::STAT_MODE){
        WriteStatInfo();
    }

    if((info.flags & FastCopy::PRE_SEARCH) && !ti.total.isPreSearch) {
        CalcInfo(&doneRate, &remain_sec, &total_sec);
        remain_h = remain_sec / 3600;
        remain_m = (remain_sec % 3600) / 60;
        remain_s = remain_sec % 60;
        doneRatePercent = (int)(doneRate * 100);
        // ProgressBarに進捗を反映
        // 終了延長の場合は強制的に100%表示にする
        if(!is_finish_status){
            // 処理途中なので更新するだけ
            ui->progressBar_exec->setValue(doneRatePercent);
        }
        else{
            // 終了なので強制的に100%にする
            ui->progressBar_exec->setValue(100);
        }
        SetWindowTitle();
    }

    if(isTaskTray){
        SetTaskTrayInfo(is_finish_status, doneRate, remain_h, remain_m, remain_s);
        //タスクトレイ内で最小化しているかつ、終了してないなら即リターン
        if(isTaskTray && !is_finish_status) return TRUE;
    }

    if((info.flags & FastCopy::PRE_SEARCH) && !ti.total.isPreSearch && !is_finish_status
    && doneRate >= 0.0001) {
        len += sprintf(buf + len, " -- Remain %02d:%02d:%02d (%d%%) --\n",
                remain_h, remain_m, remain_s, doneRatePercent);
    }
    if(ti.total.isPreSearch){
        len += sprintf(buf + len,
            " ---- Estimating ... ----\n"
            "PreTrans = %.1fMB\nPreFiles  = %d (%d)\n"
            "PreTime  = %.2fsec\nPreRate  = %.2ffiles/s",
            (double)ti.total.preTrans / (1024 * 1024),
            ti.total.preFiles, ti.total.preDirs,
            (double)ti.tickCount / 1000,
            (double)ti.total.preFiles * 1000 / ti.tickCount);
    }
    else if(info.mode == FastCopy::DELETE_MODE){
        len += sprintf(buf + len,
            IsListing() ?
            "TotalDel   = %.1f MB\n"
            "DelFiles   = %d (%d)\n"
            "TotalTime = %02d:%02d:%02d\n" :
            (info.flags & (FastCopy::OVERWRITE_DELETE|FastCopy::OVERWRITE_DELETE_NSA)) ?
            "TotalDel   = %.1f MB\n"
            "DelFiles   = %d (%d)\n"
            "TotalTime = %02d:%02d:%02d\n"
            "FileRate   = %.2f files/s\n"
            "OverWrite = %.1f MB/s" :
            "TotalDel   = %.1f MB\n"
            "DelFiles   = %d (%d)\n"
            "TotalTime = %02d:%02d:%02d\n"
            "FileRate   = %.2f files/s",
            (double)ti.total.deleteTrans / (1024 * 1024),
            ti.total.deleteFiles, ti.total.deleteDirs,
            ti.tickCount / 1000 / 3600,
            ((ti.tickCount / 1000) % 3600) / 60,
            ((ti.tickCount / 1000) % 60),
            (double)ti.total.deleteFiles * 1000 / ti.tickCount,
            (double)ti.total.writeTrans / ((double)ti.tickCount / 1000) / (1024 * 1024));
    }
    else{

        if(IsListing()){

            //VERIFYONLYモード時はTotalSizeとTotalFilesは表示しない。
            //元々のListingでも特に意味ない表示っぽいが、後方互換目的で残しておく。
            if(!IsVerifyListing()){
                len += sprintf(buf + len,
                    (info.flags & FastCopy::RESTORE_HARDLINK) ?
                    "TotalSize = %.1f MB\n"
                    "TotalFiles = %d/%d (%d)\n" :
                    "TotalSize = %.1f MB\n"
                    "TotalFiles = %d (%d)\n"
                    , (double)ti.total.writeTrans / (1024 * 1024)
                    , ti.total.writeFiles
                    , (info.flags & FastCopy::RESTORE_HARDLINK) ?
                      ti.total.linkFiles :
                      ti.total.writeDirs
                    , ti.total.writeDirs);
            }
        }
        else{
            len += sprintf(buf + len,
                (info.flags & FastCopy::RESTORE_HARDLINK) ?
                "TotalRead = %.1f MB\n"
                "TotalWrite = %.1f MB\n"
                "TotalFiles = %d/%d (%d)\n" :
                "TotalRead = %.1f MB\n"
                "TotalWrite = %.1f MB\n"
                "TotalFiles = %d (%d)\n"
                , (double)ti.total.readTrans / (1024 * 1024)
                , (double)ti.total.writeTrans / (1024 * 1024)
                , ti.total.writeFiles
                , (info.flags & FastCopy::RESTORE_HARDLINK) ?
                  ti.total.linkFiles :
                  ti.total.writeDirs
                , ti.total.writeDirs);
        }

        //VERIFYONLYモード時はskipFiles表示しない。
        //if (ti.total.skipFiles || ti.total.skipDirs) {
        if(!IsVerifyListing() && (ti.total.skipFiles || ti.total.skipDirs)){
            len += sprintf(buf + len,
                "TotalSkip = %.1f MB\n"
                "SkipFiles = %d (%d)\n"
                , (double)ti.total.skipTrans / (1024 * 1024)
                , ti.total.skipFiles, ti.total.skipDirs);
        }
        if(ti.total.deleteFiles || ti.total.deleteDirs){
            len += sprintf(buf + len,
                "TotalDel  = %.1f MB\n"
                "DelFiles  = %d (%d)\n"
                , (double)ti.total.deleteTrans / (1024 * 1024)
                , ti.total.deleteFiles, ti.total.deleteDirs);
        }

        len += sprintf(buf + len, IsListing() ?
            "TotalTime = %02d:%02d:%02d\n" :
            "TotalTime = %02d:%02d:%02d\n"
            "TransRate = %.2f MB/s\n"
            "FileRate  = %.2f files/s"
            , ti.tickCount / 1000 / 3600
            , ((ti.tickCount / 1000) % 3600) / 60
            , ((ti.tickCount / 1000) % 60)
            , (double)ti.total.writeTrans / ti.tickCount / 1024 * 1000 / 1024
            , (double)ti.total.writeFiles * 1000 / ti.tickCount);

        if (info.flags & FastCopy::VERIFY_FILE) {
            len += sprintf(buf + len,
                "\nVerifyRead= %.1f MB\nVerifyFiles= %d"
                , (double)ti.total.verifyTrans / (1024 * 1024)
                , ti.total.verifyFiles);
        }
    }

    if (IsListing() && is_finish_status) {
        //VERIFY_ONLYモード時は別表示
        if(!IsVerifyListing()){
            len += sprintf(buf + len, "\n -- Listing%s Done --",
                    (info.flags & FastCopy::VERIFY_FILE) ? " (verify)" : "");
        }
        //VERIFY_ONLY
        else{
            len += sprintf(buf + len, "\n -- Verify Done --");
        }
    }
    ui->textEdit_STATUS->setText(buf);

    //元々の領域の色覚えておく
    QPalette p = ui->textEdit_PATH->palette();

    if(IsListing()){
        if(is_finish_status){
            QTextCursor cur = ui->textEdit_PATH->textCursor();
            cur.movePosition(QTextCursor::End);
            sprintf(buf, "Finished. (ErrorFiles : %d  ErrorDirs : %d)",
                ti.total.errFiles, ti.total.errDirs);
            cur.insertText(buf);
            cur.movePosition(QTextCursor::End);
            ui->textEdit_PATH->setTextCursor(cur);
        }
        //色変更
        if(ti.total.errFiles || ti.total.errDirs){
            p.setColor(QPalette::Base,QColor("tomato"));
        }
        else{
            p.setColor(QPalette::Base,QColor("lightgreen"));
        }
    }
    else if(is_finish_status){
        sprintf(buf, ti.total.errFiles || ti.total.errDirs ?
            "Finished. (ErrorFiles : %d  ErrorDirs : %d)" : "Finished.",
            ti.total.errFiles, ti.total.errDirs);
        ui->textEdit_PATH->setText(buf);

        if(ti.total.errFiles || ti.total.errDirs){
            p.setColor(QPalette::Base,QColor("tomato"));
        }
        else{
            p.setColor(QPalette::Base,QColor("lightgreen"));
        }
        SetWindowTitle();
    }
    else{
        ui->textEdit_PATH->setText(ti.total.isPreSearch ? EMPTY_STR_V : ti.curPath);
        if(ti.total.errFiles || ti.total.errDirs){
            p.setColor(QPalette::Base,QColor("orange"));
        }
        else{
            p.setColor(QPalette::Base,QColor("paleturquoise"));
        }
    }
    ui->textEdit_PATH->setPalette(p);
    //JobListの進行状況表示更新
    if(joblistisstarting){
        SetJobListInfo(&ti,is_finish_status);
    }

    ui->label_samedrv->setText(info.mode == FastCopy::DELETE_MODE ? EMPTY_STR_V : ti.isSameDrv ?
        (char*)GetLoadStrV(IDS_SAMEDISK) : (char*)GetLoadStrV(IDS_DIFFDISK));

    //エラー時継続有効な場合？
    if((info.ignoreEvent ^ ti.ignoreEvent) & FASTCOPY_ERROR_EVENT){
        //エラー時継続のチェックボックスを再度有効にする？なんでやる必要あるんだろか・・。？なぞ
        ui->checkBox_Nonstop->setChecked((
                    (info.ignoreEvent = ti.ignoreEvent) & FASTCOPY_ERROR_EVENT) ? 1 : 0);
    }

    if(isErrEditHide && ti.errBuf->UsedSize() > 0){
        //qDebug("4670 before width = %d height = %d err_widget_height = %d",this->width(),this->height(),ui->err_widget->height());
        this->resize(this->width(),this->height() + ui->err_widget->height());
        //qDebug("4672 after width = %d height = %d err_widget_height = %d",this->width(),this->height(),ui->err_widget->height());
        ui->err_widget->show();
        isErrEditHide = false;
    }
    if(ti.errBuf->UsedSize() > errBufOffset || errBufOffset == MAX_ERR_BUF || is_finish_status){
        if(ti.errBuf->UsedSize() > errBufOffset && errBufOffset != MAX_ERR_BUF){
            ti.errCs->lock();
            QTextCursor cur = ui->textEdit_ERR->textCursor();
            cur.movePosition(QTextCursor::End);
            cur.insertText((const char*)(ti.errBuf->Buf() + errBufOffset));
            errBufOffset = ti.errBuf->UsedSize();
            ti.errCs->unlock();
        }
        sprintf(buf, "(ErrFiles : %d / ErrDirs : %d)", ti.total.errFiles, ti.total.errDirs);
        ui->Label_Errstatic_FD->setText(buf);
    }
    //statusbarの表示情報
    SetStatusBarInfo(&ti,is_finish_status);
    return	TRUE;
}

bool MainWindow::SetStatusBarInfo(TransInfo *ti_ptr,bool is_finish_status){

    //既に問い合わせ済みじゃなければ自分でphaseだけ取得
    if(ti_ptr == NULL){
        fastCopy.GetPhaseInfo(&ti);
        ti_ptr = &ti;
    }
    //現在の進行状態表示処理
    if(!is_finish_status){
        //現在の処理状態文字列をとりあえず取得
        QString statusstr = status_label.text();
        int dotcount = statusstr.count(".");
        //過去の文字列いらないのでいったん消す
        statusstr.clear();
        switch(ti_ptr->phase){
            case FastCopy::NORUNNING:
                dotcount = 0;
                break;
            case FastCopy::MUTEX_WAITING:
                statusstr.append((char*)GetLoadStrV(IDS_STATUS_WAITING_OTHER));
                break;
            case FastCopy::PRESEARCHING:
                statusstr.append((char*)GetLoadStrV(IDS_STATUS_PRESEARCHING));
                break;
            case FastCopy::COPYING:
                if(IsListing()){
                    if(info.flags & FastCopy::VERIFY_FILE){
                        statusstr.append((char*)GetLoadStrV(IDS_STATUS_VERIFYING));
                    }
                    else{
                        statusstr.append((char*)GetLoadStrV(IDS_STATUS_LISTING));
                    }
                }
                else{
                    statusstr.append((char*)GetLoadStrV(IDS_STATUS_COPYING));
                }
                break;
            case FastCopy::DELETING:
                statusstr.append((char*)GetLoadStrV(IDS_STATUS_DELETING));
                break;
            case FastCopy::VERIFYING:
                statusstr.append((char*)GetLoadStrV(IDS_STATUS_VERIFYING));
                break;
            default:
                break;
        }
        if(statusstr.isEmpty()){
            status_label.clear();
        }
        else{
            if(++dotcount > 3){
                //.なしに戻すので.付与しない
            }
            else{
                //.をけつ文字列に付与
                for(int i=0;i<dotcount;i++){
                    statusstr.append(".");
                }
            }
            //最終組立文字列をLabelに適用
            status_label.setText(statusstr);
        }
    }
    else{
        //MainWindowから終了のお知らせ来てるので強制的に初期状態
        status_label.clear();
    }
    return true;
}

bool MainWindow::SetJobListInfo(TransInfo *ti,bool is_finish_status){

    disconnect(ui->listWidget_JobList,SIGNAL(itemChanged(QListWidgetItem*)),
               this,SLOT(on_listWidget_JobList_itemChanged(QListWidgetItem*)));

    QListWidgetItem* currentItem = ui->listWidget_JobList->item(joblistcurrentidx);
    if(is_finish_status){
        if(ti->total.errFiles || ti->total.errDirs){
            currentItem->setBackgroundColor("tomato");
            currentItem->setIcon(QIcon(":/icon/icon/delete_sign_filled.png"));
        }
        else{
            currentItem->setBackgroundColor(("lightgreen"));
            currentItem->setIcon(QIcon(":/icon/icon/checkmark.png"));
        }
    }
    else{
        if(ti->total.errFiles || ti->total.errDirs){
            currentItem->setBackgroundColor("orange");
            currentItem->setIcon(QIcon(":/icon/icon/delete_sign_filled.png"));
        }
        else{
            currentItem->setBackgroundColor("paleturquoise");
        }
    }

    connect(ui->listWidget_JobList,SIGNAL(itemChanged(QListWidgetItem*)),
            this,SLOT(on_listWidget_JobList_itemChanged(QListWidgetItem*)));
    return true;
}

BOOL MainWindow::SetWindowTitle()
{
    static void	*title;
    static void	*version;
    static void	*admin;

    if(!title){
        title	= (char *)GetLoadStrV(IDS_FASTCOPY);
        version	= (void *)GetVersionStr();
        admin	= (void *)GetVerAdminStr();
    }

    WCHAR	_buf[4096];
    void	*buf = _buf;
    int		len = 0;

    void	*FMT_HYPHEN  = (void *)" -%s";
    void	*FMT_PAREN   = (void *)" [%s]";
    void	*FMT_PERCENT = (void *)"(%d%%) ";
    void	*FMT_STR 	 = (void *)"%s";
    void	*FMT_STRSTR  = (void *)" %s%s";

    if((info.flags & FastCopy::PRE_SEARCH) && !ti.total.isPreSearch && doneRatePercent >= 0
        && fastCopy.IsStarting()){
        len += sprintfV(MakeAddr(buf, len), FMT_PERCENT, doneRatePercent);
    }

    char	job[MAX_PATH];
    void	*fin = finActIdx > 0 ? cfg.finActArray[finActIdx]->title : (void *)"";

    strncpy(job,jobtitle_static.toLocal8Bit().data(),MAX_PATH);

    len += sprintfV(MakeAddr(buf, len), FMT_STR, title);

    if(GetChar(job, 0)){
        len += sprintfV(MakeAddr(buf, len), FMT_PAREN, job);
    }

    if(GetChar(fin, 0)){
        len += sprintfV(MakeAddr(buf, len), FMT_HYPHEN, fin);
    }

    len += sprintfV(MakeAddr(buf, len), FMT_STRSTR,
            GetChar(job, 0) || GetChar(fin, 0) ? "" : version, admin);

    this->setWindowTitle((char*)buf);
    return(true);
}

void MainWindow::SetItemEnable(BOOL is_delete)
{

    ui->pushButton_Dst->setEnabled(!is_delete);
    ui->plainTextEdit_Dst->setEnabled(!is_delete);
    ui->checkBox_Estimate->setHidden(is_delete);
    ui->checkBox_Verify->setHidden(is_delete);

    //verifyオンリーモードからの以降時、変更前まで記憶していたtrue/falseを復元する
    ui->checkBox_Verify->setEnabled(true);
    ui->checkBox_Verify->setChecked(verify_before);

    ui->checkBox_Acl->setEnabled(!is_delete);
    ui->checkBox_xattr->setEnabled(!is_delete);
    ui->checkBox_owdel->setEnabled(is_delete);

    ui->checkBox_Acl->setHidden(is_delete);
    ui->checkBox_xattr->setHidden(is_delete);
    ui->checkBox_owdel->setHidden(!is_delete);

    ui->pushButton_Exe->setHidden(false);
    ui->pushButton_JobList->setHidden(false);
    if(ui->pushButton_JobExe->isEnabled()){
        ui->pushButton_JobExe->setHidden(false);
    }
    SetPathHistory_new(SETHIST_LIST,ui->plainTextEdit_Src);
}

void MainWindow::SetItemEnable_Verify(BOOL verify_before_update)
{
    //Verify強制する前のチェックステータスを退避
    if(verify_before_update){
        verify_before = ui->checkBox_Verify->isChecked();
    }
    //veriry専用モードではベリファイは強制でON
    ui->checkBox_Verify->setChecked(true);
    ui->checkBox_Verify->setEnabled(false);
    ui->checkBox_Verify->setHidden(false);

    ui->checkBox_Acl->setEnabled(false);
    ui->checkBox_xattr->setEnabled(false);
    ui->checkBox_owdel->setEnabled(false);

    ui->checkBox_Acl->setHidden(true);
    ui->checkBox_xattr->setHidden(true);
    ui->checkBox_owdel->setHidden(true);

    ui->pushButton_Exe->setHidden(true);
    if(ui->pushButton_JobExe->isEnabled()){
        ui->pushButton_JobExe->setHidden(true);
    }
}

void MainWindow::UpdateMenu(void)
{
    char	buf[MAX_PATH];
    int		i;

    for(int i=0;i < job_Groups.count();i++){
        job_Groups[i]->clear();
        ui->menuJobMng->removeAction(job_Groups[i]->menuAction());
    }
    job_Groups.clear();

    //jobのメニュー項目をcfgから読み込んで設定。
    //Other job(Do not belongs Joblist)
    QMenu *fixjobgroup_menu = new QMenu((char*)GetLoadStrV(IDS_SINGLELIST_MENUNAME));
    QObject::connect(fixjobgroup_menu,SIGNAL(triggered(QAction*)),
                     this,SLOT(job_triggered(QAction*)));

    for(int i=0; i < cfg.jobMax; i++){
        if(cfg.SearchJobnuminAllJobList(cfg.jobArray[i]->title) == 0){

            QAction *jobfixact_new = new QAction((char*)cfg.jobArray[i]->title,this);
            jobfixact_new->setCheckable(true);
            fixjobgroup_menu->addAction(jobfixact_new);
            if(lstrcmpiV(buf,cfg.jobArray[i]->title)){
                jobfixact_new->setChecked(false);
            }
            else{
                jobfixact_new->setChecked(true);
            }
        }
    }
    ui->menuJobMng->addMenu(fixjobgroup_menu);
    job_Groups.append(fixjobgroup_menu);

    joblist_menu = new QMenu((char*)GetLoadStrV(IDS_JOBLIST_MENUNAME),this);
    job_Groups.append(joblist_menu);

    //joblist
    for(i=0; i < cfg.joblistMax; i++){

        QMenu *jobgroup_menu = new QMenu((char*)cfg.joblistArray[i]->joblist_title);
        QObject::connect(jobgroup_menu,SIGNAL(triggered(QAction*)),
                             this,SLOT(job_triggered(QAction*)));

        for(int j=0;j < cfg.joblistArray[i]->joblistentry; j++){
            QAction *jobact_new = new QAction((char*)cfg.joblistArray[i]->jobname_pt[j],this);
            jobact_new->setCheckable(true);
            jobgroup_menu->addAction(jobact_new);
            if(lstrcmpiV(jobtitle_static.toLocal8Bit().data(),(char*)cfg.joblistArray[i]->jobname_pt[j])){
                //一致しない
                jobact_new->setChecked(false);
            }
            else{
                //だれかしらは一致する
                jobact_new->setChecked(true);
            }
        }
        joblist_menu->addMenu(jobgroup_menu);
        job_Groups.append(jobgroup_menu);
    }

    //シングルジョブリストのメニュー画面アクション前にジョブリスト一覧メニューを追加
    ui->menuJobMng->insertMenu(ui->actionShow_SingleJob,joblist_menu);
    //ジョブリストモードとシングルジョブモードをカテゴライズするためにセパレータ追加
    ui->menuJobMng->insertSeparator(ui->actionShow_SingleJob);

    switch(diskMode){
        case 0:
            ui->actionAuto_HDD_mode->setChecked(true);
            ui->actionSame_HDD_mode->setChecked(false);
            ui->actionDiff_HDD_mode->setChecked(false);
            break;

        case 1:

            ui->actionAuto_HDD_mode->setChecked(false);
            ui->actionSame_HDD_mode->setChecked(true);
            ui->actionDiff_HDD_mode->setChecked(false);
            break;

        case 2:
            ui->actionAuto_HDD_mode->setChecked(false);
            ui->actionSame_HDD_mode->setChecked(false);
            ui->actionDiff_HDD_mode->setChecked(true);
            break;

        default:
            ui->actionAuto_HDD_mode->setChecked(true);
            ui->actionSame_HDD_mode->setChecked(false);
            ui->actionDiff_HDD_mode->setChecked(false);
            break;
    }

    sprintfV(buf, GetLoadStrV(IDS_FINACTMENU),
        finActIdx >= 0 ? cfg.finActArray[finActIdx]->title : EMPTY_STR_V);

    ui->menuPost_Process->setTitle(buf);

    //登録済みのfinact関連アクションを一回全リセット
    //初回登録していない？
    if(finact_Group->actions().count() != 0){
        //登録済みのQActionをケツから順に削除
        for(i=finact_Group->actions().count()-1; i >= 0;i--){
            QAction *postact_old = finact_Group->actions().at(i);
            finact_Group->removeAction(postact_old);
            ui->menuPost_Process->removeAction(postact_old);
        }
    }

    //post-processのメニュー項目をcfgから読み込んで設定。
    for(i=0; i < cfg.finActMax; i++){
        QAction *postact_new = new QAction((char*)cfg.finActArray[i]->title,this);
        postact_new->setCheckable(true);
        finact_Group->addAction(postact_new);
        //ui->menuPost_Process->addAction()
        //::CheckMenuItem(hSubMenu, i, MF_BYPOSITION|(i == finActIdx ? MF_CHECKED : MF_UNCHECKED));
        if(i == finActIdx){
            postact_new->setChecked(true);
        }
    }
    ui->menuPost_Process->addActions(finact_Group->actions());
    ui->menuPost_Process->addSeparator();
    finAct_Menu->setText((char*)GetLoadStrV(FINACT_MENUITEM));
    ui->menuPost_Process->addAction(finAct_Menu);

    if(!fastCopy.IsStarting() && !isDelay){
        ui->label_samedrv->setText(diskMode == 0 ? "" : diskMode == 1 ?
            (char*)GetLoadStrV(IDS_FIX_SAMEDISK) : (char*)GetLoadStrV(IDS_FIX_DIFFDISK));
    }
    SetWindowTitle();
}

void MainWindow::UpdateJobList(bool req_current){
    QString currentText;
    if(req_current){
        currentText = ui->comboBox_JobListName->currentText();
    }
    //JobListをロードし直すので一旦内容を全クリア
    ui->comboBox_JobListName->clear();

    //JobListクラスからジョブリストをロード
    for(int i=0; i<cfg.joblistMax;i++){
        ui->comboBox_JobListName->addItem((char*)cfg.joblistArray[i]->joblist_title);
    }
    if(req_current){
        ui->comboBox_JobListName->setCurrentText(currentText);
    }
    else{
        ui->comboBox_JobListName->clearEditText();
    }
    //カレントテキストの入力例を表示
    ui->comboBox_JobListName->lineEdit()->setPlaceholderText((char*)GetLoadStrV(IDS_JOBLIST_PLACEHOLDER));
}

//JobList関連GUIでユーザ操作のロック/アンロックを行う
//required_unlock = true(操作可能にする/アンロック) false=(操作不能とする/ロック)
void MainWindow::EnableJobList(bool required_unlock){

    if(required_unlock){
        ui->listWidget_JobList->setDragDropMode(QAbstractItemView::InternalMove);
        //List内でのクリック->ジョブ名編集有効化
        //ui->listWidget_JobList->setUpdatesEnabled();
        ui->listWidget_JobList->setEditTriggers(QAbstractItemView::SelectedClicked
                                                | QAbstractItemView::DoubleClicked);

        disconnect(ui->listWidget_JobList,SIGNAL(itemChanged(QListWidgetItem*)),
                   this,SLOT(on_listWidget_JobList_itemChanged(QListWidgetItem*)));
        for(int i=0;i < ui->listWidget_JobList->count();i++){
            ui->listWidget_JobList->item(i)->setFlags(ui->listWidget_JobList->item(i)->flags()
                                                        | Qt::ItemIsEditable
                                                        | Qt::ItemIsDropEnabled);
        }
        connect(ui->listWidget_JobList,SIGNAL(itemChanged(QListWidgetItem*)),
                this,SLOT(on_listWidget_JobList_itemChanged(QListWidgetItem*)));

        ui->actionEnable_JobList_Mode->setEnabled(required_unlock);
    }
    else{
        ui->listWidget_JobList->setDragDropMode(QAbstractItemView::NoDragDrop);
        ui->listWidget_JobList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }

    //JobList manage
    ui->pushButton_JobListSave->setEnabled(required_unlock);
    ui->pushButton_JobListDelete->setEnabled(required_unlock);
    ui->pushButton_JobListRename->setEnabled(required_unlock);
    ui->comboBox_JobListName->setEnabled(required_unlock);
    ui->checkBox_JobListForceLaunch->setEnabled(required_unlock);
    //Job edit
    ui->pushButton_JobExe->setEnabled(required_unlock);
    ui->pushButton_JobList->setEnabled(required_unlock);
    ui->pushButton_JobDelete->setEnabled(required_unlock);
    ui->pushButton_JobDeleteAll->setEnabled(required_unlock);
    //homu
    ui->menuJobMng->setEnabled(required_unlock);

    //listwidget操作不能にする、見た目がアレなのがなあ。。
    ui->listWidget_JobList->setEnabled(required_unlock);


    //変更検知用の各種シグナルを有効、無効化
    SetJobListmodifiedSignal(required_unlock);
    joblist_cancelsignal = false;
    joblist_isnosave = false;
}

void MainWindow::EnableJobMenus(bool isEnable){
    //ジョブ周りのメニューをロック/アンロック
    ui->menuJobMng->setEnabled(isEnable);
}

//ジョブリスト機能のカレントジョブの内容と現在の画面構成に変化がないかチェック、
//変化している場合にはxxxする
bool MainWindow::CheckJobListmodified(){

    bool ret = false;
    bool is_blank;

    is_blank = ui->plainTextEdit_Src->toPlainText().isEmpty();
    if(is_blank && copyInfo[ui->comboBox_Mode->currentIndex()].mode != FastCopy::DELETE_MODE
        && ui->plainTextEdit_Dst->toPlainText().isEmpty()){
        is_blank = true;
    }

    //ジョブリスト中のジョブの内容変更検知する？

    //srcとdstが空白ではない?
    //JobListモード有効??
    //ジョブリスト中のジョブを選択中
    //ジョブリストモード実行中ではない
    if(!is_blank
        && ui->actionEnable_JobList_Mode->isChecked()
        && ui->listWidget_JobList->currentRow() != -1
        && !joblistisstarting){

        int jobidx = ui->listWidget_JobList->currentRow();
        Job *joblist_job = &joblist[jobidx];

        joblist_prev_job.Init();
        GetJobfromMain(&joblist_prev_job);

        joblist_prev_job.SetString(joblist_job->title,
                         ui->plainTextEdit_Src->toPlainText().toLocal8Bit().data(),
                         copyInfo[ui->comboBox_Mode->currentIndex()].mode != FastCopy::DELETE_MODE ?
                          ui->plainTextEdit_Dst->toPlainText().toLocal8Bit().data() : (void*)"",
                         copyInfo[ui->comboBox_Mode->currentIndex()].cmdline_name,
                         joblist_prev_job.isFilter ? ui->comboBox_include->currentText().toLocal8Bit().data() : (void*)"",
                         joblist_prev_job.isFilter ? ui->comboBox_exclude->currentText().toLocal8Bit().data() : (void*)"",
                         joblist_prev_job.isFilter ? ui->comboBox_FromDate->currentText().toLocal8Bit().data() : (void*)"",
                         joblist_prev_job.isFilter ? ui->comboBox_ToDate->currentText().toLocal8Bit().data() : (void*)"",
                         joblist_prev_job.isFilter ? ui->comboBox_MinSize->currentText().toLocal8Bit().data() : (void*)"",
                         joblist_prev_job.isFilter ? ui->comboBox_MaxSize->currentText().toLocal8Bit().data() : (void*)"");
        joblist_prev_job.isListing = joblist_job->isListing;
        joblist_prev_job.flags = joblist_job->flags;

        //差がないかチェック
        if(!joblist_job->CompareJob(&joblist_prev_job)){

            status_label.setText((char*)GetLoadStrV(IDS_JOB_CHANGEDNOTIFY));
            //フォント変更して、ジョブ内容保存されてないよを知らせる
            QListWidgetItem *item = ui->listWidget_JobList->item(ui->listWidget_JobList->currentRow());
            QFont font = item->font();
            font.setItalic(true);
            font.setBold(true);
            item->setFont(font);
            joblist_isnosave = true;
        }
        else{
            joblist_isnosave = false;
        }
        ret = true;
    }
    else{
        //qDebug() << "CheckJobListmodified called But no change.";
    }
    return ret;
}

void MainWindow::SetJob(int idx,bool use_joblistwk,bool cancel_signal)
{
    Job		*job;
    //とりあえずでっちあげ
    if(use_joblistwk){
        job = &joblist[idx];
        //job->debug();
        //qDebug() << "SetJob title = " << (char*)job->title;
    }
    else{
        job = cfg.jobArray[idx];
    }

    //ジョブリスト機能有効時に選択された場合は本関数内でセットした際に発生するシグナルを
    //一時的に無視
    //ユーザ任意操作以外の入力に対してシグナルが反応する必要がないので。
    if(cancel_signal){
        //SetJobする前に警告出さなくてよいか確認
        if(joblist_isnosave){
            //保存しといた直前のジョブで名称サーチ、index特定
            for(int i=0;i < joblist.count();i++){
                //直前状態のジョブタイトル名と一致する？
                if(lstrcmpiV(joblist_prev_job.title,joblist.at(i).title) == 0){
                    //保存した方が良いか聞いてみる
                    QString ask_msg = QString::fromLocal8Bit((char*)joblist_prev_job.title);
                    ask_msg.prepend("\"");
                    ask_msg.append("\"");
                    ask_msg.append((char*)GetLoadStrV(IDS_JOBLIST_CHANGENOTIFY));
                    QMessageBox msgbox;
                    msgbox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                    msgbox.setIcon(QMessageBox::Question);
                    msgbox.setText(ask_msg);
                    if(msgbox.exec() == QMessageBox::Yes){
                        //保存するってさ
                        joblist.removeAt(i);
                        joblist.insert(i,joblist_prev_job);
                        UpdateJobListtoini();
                        //removeで当該ジョブを削除してしまったので、
                        //job_ptを付け直す
                        job = &joblist[idx];
                    }
                    else{
                        joblist_prev_job.Init();
                    }
                    joblist_isnosave = false;
                    break;
                }
            }
        }
        joblist_cancelsignal = true;
    }

    ui->checkBox_LTFS->setChecked(job->LTFS_mode);
    idx = CmdNameToComboIndex(job->cmd);

    if(idx == -1){
        return;
    }

    jobtitle_static = jobtitle_static.fromLocal8Bit((char*)job->title);
    ui->comboBox_Mode->setCurrentIndex(idx);

    ui->lineEdit_BUFF->setText(QString::number(job->bufSize));
    ui->checkBox_Estimate->setChecked(job->estimateMode);
    if(GetCopyMode() == FastCopy::VERIFY_MODE){
        ui->checkBox_Verify->setChecked(true);
    }
    else{
        ui->checkBox_Verify->setChecked(job->enableVerify);
    }
    ui->checkBox_Nonstop->setChecked(job->ignoreErr);
    ui->checkBox_owdel->setChecked(job->enableOwdel);
    ui->checkBox_Acl->setChecked(job->enableAcl);
    ui->checkBox_xattr->setChecked(job->enableXattr);
    //ui->checkBox_dot->setChecked(job->Dotignore_mode);
    ui->plainTextEdit_Src->setPlainText((char*)job->src);
    //シグナルキャンセルするとSrcの色分け処理走らないので自分でsignal関数呼ぶ。。
    if(cancel_signal){
        on_plainTextEdit_Src_blockCountChanged(ui->plainTextEdit_Src->blockCount());
    }
    ui->plainTextEdit_Dst->setPlainText((char*)job->dst);

    //filter
    //filterはジョブ保存時に開いてたけど、いずれも設定してなかったらわざわざフィルタは表示しない
    if (job->isFilter
        && (strlen((char*)job->includeFilter) ||
            strlen((char*)job->excludeFilter) ||
            strlen((char*)job->fromDateFilter)||
            strlen((char*)job->toDateFilter)  ||
            strlen((char*)job->minSizeFilter) ||
            strlen((char*)job->maxSizeFilter))){
        ui->actionShow_Extended_Filter->setChecked(job->isFilter);
        //ReflectFilterCheck(true);
        ui->comboBox_include->setCurrentText((char*)job->includeFilter);
        ui->comboBox_exclude->setCurrentText((char*)job->excludeFilter);
        ui->comboBox_FromDate->setCurrentText((char*)job->fromDateFilter);
        ui->comboBox_ToDate->setCurrentText((char*)job->toDateFilter);
        ui->comboBox_MinSize->setCurrentText((char*)job->minSizeFilter);
        ui->comboBox_MaxSize->setCurrentText((char*)job->maxSizeFilter);
    }
    else{
        ui->actionShow_Extended_Filter->setChecked(cfg.isExtendFilter);
        ui->comboBox_include->clearEditText();
        ui->comboBox_exclude->clearEditText();
        ui->comboBox_FromDate->clearEditText();
        ui->comboBox_ToDate->clearEditText();
        ui->comboBox_MinSize->clearEditText();
        ui->comboBox_MaxSize->clearEditText();
    }
    ui->textEdit_PATH->clear();
    ui->textEdit_ERR->clear();
    diskMode = job->diskMode;
    UpdateMenu();
    SetMiniWindow();

    //シグナルの無視を解除
    if(cancel_signal){
        joblist_cancelsignal = false;
    }
}

void MainWindow::SetFinAct(int idx)
{
    if (idx < cfg.finActMax) {
        finActIdx = idx;
        UpdateMenu();
    }
}

QString MainWindow::GetJobTitle(){
    return(jobtitle_static);
}

void MainWindow::SetJobTitle(void *_title){
    jobtitle_static = jobtitle_static.fromLocal8Bit((char*)_title);
}

void MainWindow::DelJobTitle(){
    jobtitle_static.clear();
}

int MainWindow::GetSrclen(){
    //QPlainTextEditの改行はU+2029のParagraphSeparatorと\nが入ってる。。
    //改行コードを取り除いた文字列長をリターン
    return(strlen(GetSrc().toLocal8Bit().data()));
}

int MainWindow::GetDstlen(){
    //QPlainTextEditの改行はU+2029のParagraphSeparatorと\nが入ってる。。
    //改行コードを取り除いた文字列長をリターン
    return(strlen(GetDst().toLocal8Bit().data()));
}

QString MainWindow::GetSrc(){
    QTextCursor cursor = ui->plainTextEdit_Src->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End,QTextCursor::KeepAnchor);
    QString text = cursor.selectedText();
    return(text);
}

QString MainWindow::GetDst(){
    QTextCursor cursor = ui->plainTextEdit_Dst->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End,QTextCursor::KeepAnchor);
    QString text = cursor.selectedText();
    return(text);
}

int MainWindow::GetDiskMode(){
    return(diskMode);
}

int MainWindow::GetCopyModeIndex(){
    return(ui->comboBox_Mode->currentIndex());
}

void MainWindow::GetJobfromMain(Job *job){

    job->bufSize = ui->lineEdit_BUFF->text().toInt();
    job->estimateMode = ui->checkBox_Estimate->isChecked();
    job->ignoreErr = ui->checkBox_Nonstop->isChecked();
    job->enableOwdel = ui->checkBox_owdel->isChecked();
    job->enableAcl = ui->checkBox_Acl->isChecked();
    job->enableXattr = ui->checkBox_xattr->isChecked();
    job->enableVerify = ui->checkBox_Verify->isChecked();
    job->isFilter = ui->actionShow_Extended_Filter->isChecked();
    job->Dotignore_mode = cfg.Dotignore_mode;
    job->LTFS_mode = ui->checkBox_LTFS->isChecked();
}

void MainWindow::GetFilterfromMain(char* inc,char* exc,char* from_date,
                                   char* to_date,char* min_size,char* max_size){
    strncpy(inc,ui->comboBox_include->currentText().toLocal8Bit().data(),MAX_PATH);
    strncpy(exc,ui->comboBox_exclude->currentText().toLocal8Bit().data(),MAX_PATH);
    strncpy(from_date,ui->comboBox_FromDate->currentText().toLocal8Bit().data(),MINI_BUF);
    strncpy(to_date,ui->comboBox_ToDate->currentText().toLocal8Bit().data(),MINI_BUF);
    strncpy(min_size,ui->comboBox_MinSize->currentText().toLocal8Bit().data(),MINI_BUF);
    strncpy(max_size,ui->comboBox_MaxSize->currentText().toLocal8Bit().data(),MINI_BUF);

}
// Signal/SLOT初期化
bool MainWindow::InitEv(){

    bool ret = true;

    // clicked()シグナル関連初期化
    // Src
    if(QObject::connect(ui->pushButton_Src,SIGNAL(clicked()),
                        this,SLOT(EvClicked())) == false){
        ret = false;
        goto end;
    }

    // Dst
    if(QObject::connect(ui->pushButton_Dst,SIGNAL(clicked()),
                        this,SLOT(EvClicked())) == false){
        ret = false;
        goto end;
    }

    // Execute
    if(QObject::connect(ui->pushButton_Exe,SIGNAL(clicked()),
                        this,SLOT(EvClicked())) == false){
        ret = false;
        goto end;
    }

    // Listing
    if(QObject::connect(ui->pushButton_Listing,SIGNAL(clicked()),
                        this,SLOT(EvClicked())) == false){
        ret = false;
        goto end;
    }

    // Cancel
    if(QObject::connect(ui->pushButton_cancel_exec,SIGNAL(clicked()),
                        this,SLOT(EvClicked())) == false){
        ret = false;
        goto end;
    }

    // Atonce
    if(QObject::connect(ui->pushButton_Atonce,SIGNAL(clicked()),
                        this,SLOT(EvClicked())) == false){
        ret = false;
        goto end;
    }
    // help
    if(QObject::connect(ui->pushButton_help,SIGNAL(clicked()),
                        this,SLOT(EvClicked())) == false){
        ret = false;
        goto end;
    }
    // Top
    if(QObject::connect(ui->pushButton_Top,SIGNAL(clicked()),
                        this,SLOT(EvClicked())) == false){
        ret = false;
        goto end;
    }

    // activated() or triggeredシグナル関連初期化

    // srchist
    if(QObject::connect(srchist_Group,
                     SIGNAL(triggered(QAction *)),
                     this,
                     SLOT(hist_triggered(QAction *))) == false){
        ret = false;
        goto end;
    }

    //dsthist
    if(QObject::connect(dsthist_Group,
                     SIGNAL(triggered(QAction *)),
                     this,
                     SLOT(hist_triggered(QAction *))) == false){
        ret = false;
        goto end;
    }

    //post-process action
    if(QObject::connect(finact_Group,
                     //SIGNAL(triggered(QAction*)),
                     SIGNAL(triggered(QAction *)),
                     this,
                     SLOT(actpostprocess_triggered(QAction *))) == false){
        ret = false;
        goto end;
    }

    //post-processdlg action
    if(QObject::connect(finAct_Menu,
                     //SIGNAL(triggered(QAction*)),
                     SIGNAL(triggered()),
                     this,
                     SLOT(actpostprocessdlg_triggered())) == false){
        ret = false;
        goto end;
    }

    //JobList add and Listing
    connect(ui->pushButton_JobExe,SIGNAL(clicked(bool)),
            this,SLOT(EvClicked()));
    connect(ui->pushButton_JobList,SIGNAL(clicked(bool)),
            this,SLOT(EvClicked()));

    //systemTray
    connect(&systray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

    connect(ui->pushButton_JobListStart,SIGNAL(clicked()),
            this,SLOT(EvClicked()));

    //Joblist内のJob順番のD&D検知用にrowchangedシグナルをきゃっち！
    connect(ui->listWidget_JobList->model(),SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
            this,SLOT(listWidget_JobList_rowsMoved(QModelIndex,int,int,QModelIndex,int)));
    // キー検出用にイベントフィルタ登録
    ui->plainTextEdit_Src->installEventFilter(this);
    ui->plainTextEdit_Dst->installEventFilter(this);

    //JobList
    //QComboBox上でEnter押すとなぜかクラッシュするのでEnterキー抑止用
    ui->comboBox_JobListName->installEventFilter(this);
    ui->listWidget_JobList->installEventFilter(this);

    ui->lineEdit_BUFF->installEventFilter(this);
    ui->comboBox_include->installEventFilter(this);
    ui->comboBox_exclude->installEventFilter(this);
    ui->comboBox_FromDate->installEventFilter(this);
    ui->comboBox_ToDate->installEventFilter(this);
    ui->comboBox_MinSize->installEventFilter(this);
    ui->comboBox_MaxSize->installEventFilter(this);

    //メイン画面全体のイベントフックを登録。CTRLキーの押下検出用
    installEventFilter(this->window());

end:
    return ret;
}

void MainWindow::SetJobListmodifiedSignal(bool enable){
    //有効
    if(enable){
        //JobList変更差分検出用シグナル再登録
        connect(ui->comboBox_include,SIGNAL(editTextChanged(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        connect(ui->comboBox_exclude,SIGNAL(editTextChanged(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        connect(ui->comboBox_FromDate,SIGNAL(editTextChanged(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        connect(ui->comboBox_ToDate,SIGNAL(editTextChanged(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        connect(ui->comboBox_MinSize,SIGNAL(editTextChanged(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        connect(ui->comboBox_MaxSize,SIGNAL(editTextChanged(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        connect(ui->lineEdit_BUFF,SIGNAL(textEdited(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        connect(ui->checkBox_Estimate,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
        connect(ui->checkBox_Nonstop,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
        connect(ui->checkBox_owdel,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
        connect(ui->checkBox_Acl,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
        connect(ui->checkBox_xattr,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
        connect(ui->checkBox_Verify,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
        connect(ui->actionShow_Extended_Filter,SIGNAL(triggered()),
                this,SLOT(jobListjob_Change_Detected()));
        //DotIgnoreはコンフィグダイアログ変更時に手動でコール
        connect(ui->checkBox_LTFS,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
    }
    //無効
    else{
        disconnect(ui->comboBox_include,SIGNAL(editTextChanged(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        disconnect(ui->comboBox_exclude,SIGNAL(editTextChanged(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        disconnect(ui->comboBox_FromDate,SIGNAL(editTextChanged(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        disconnect(ui->comboBox_ToDate,SIGNAL(editTextChanged(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        disconnect(ui->comboBox_MinSize,SIGNAL(editTextChanged(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        disconnect(ui->comboBox_MaxSize,SIGNAL(editTextChanged(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        disconnect(ui->lineEdit_BUFF,SIGNAL(textEdited(QString)),
                this,SLOT(jobListjob_Change_Detected()));
        disconnect(ui->checkBox_Estimate,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
        disconnect(ui->checkBox_Nonstop,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
        disconnect(ui->checkBox_owdel,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
        disconnect(ui->checkBox_Acl,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
        disconnect(ui->checkBox_xattr,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
        disconnect(ui->checkBox_Verify,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
        disconnect(ui->actionShow_Extended_Filter,SIGNAL(triggered()),
                this,SLOT(jobListjob_Change_Detected()));
        //DotIgnoreはコンフィグダイアログ変更時に手動でコール
        disconnect(ui->checkBox_LTFS,SIGNAL(clicked()),
                this,SLOT(jobListjob_Change_Detected()));
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event){
    //drag開始時点でURL形式のデータ(Finderによる選択はMIMEのURL形式でくる)
    // かつ、drag発生元イベントはRapidCopyのアプリケーション範囲外？
    if(event->mimeData()->hasUrls() && event->source() == 0){
        //MainWindowアクティブ化
        raise();
        activateWindow();
        event->acceptProposedAction();
    }
}


// Drop Event Handler
void MainWindow::dropEvent(QDropEvent *event){

    //気休め程度に先に検知するようにしたけど、うーん。なんか決め手にかける。
    bool ctrl_pressed = QApplication::queryKeyboardModifiers() & Qt::ControlModifier;

    //D&D元の判定
    //送られてきたデータがMIMEのURL形式データ？
    //かつDrag元はRapidCopy内のGUIオブジェクト以外？(Finder想定ね)
    //かつ、drop先座標がSrcまたはDstのフィールド？
    if(event->mimeData()->hasUrls() && event->source() == 0
        && (ui->plainTextEdit_Src->rect().contains(ui->plainTextEdit_Src->mapFromGlobal(QCursor::pos()))
            || ui->plainTextEdit_Dst->rect().contains(ui->plainTextEdit_Dst->mapFromGlobal(QCursor::pos())))){

        QString append_str;		 //最終的にsrcを置き換えるまたは追加する用ワーク文字列

        //FinderからのD&DはQUrlのリストで送られてくる
        QList<QUrl> drop_str_list = event->mimeData()->urls();

        //複数フォルダをD&Dした？
        if(drop_str_list.size() > 1){
            //一個ずつ処理してくか
            for(int i=0;i < drop_str_list.size();i++){
                //ワークにURIをファイルパスに変換した内容を設定
                QString drop_str_current(drop_str_list.at(i).toLocalFile());
                //qDebug() << drop_str_current;
                //対象のパスが指すファイルがディレクトリ?

                if(QFileInfo(drop_str_current).isDir() && drop_str_current.endsWith("/")){
                    //ケツの'/'を除去
                    drop_str_current.replace(drop_str_current.size() - 1,1,"\0");
                }
                append_str.append(drop_str_current);
                if(i != drop_str_list.size() -1){
                    //改行コードを付与
                    append_str.append(QChar::ParagraphSeparator);
                }
                else{
                    //ケツのパス名には改行いらんので追加しない
                }
            }
        }
        else{
            //特に気にせず先頭の文字列だけ追加
            append_str.append(drop_str_list[0].toLocalFile());

            if(QFileInfo(append_str).isDir() && append_str.endsWith("/")){
                append_str.replace(append_str.size() - 1,1,"\0");
            }
        }
        // new SrcPlaintextedit
        if(ui->plainTextEdit_Src->rect().contains(ui->plainTextEdit_Src->mapFromGlobal(QCursor::pos()))){
            //Ctrl(Cmd)おしっぱ？
            if(ctrl_pressed){
                //追加文字列の先頭に改行を挿入
                append_str.prepend(QChar::ParagraphSeparator);
            }
            else{
                //今回分のパスでsrcフィールドを置換予定なので何もしない
            }
            //内容の追加、置換と成形を依頼する
            castPath(ui->plainTextEdit_Src,append_str,drop_str_list.size(),ctrl_pressed);
            CheckJobListmodified();
        }
        // 新Dst
        else if(ui->plainTextEdit_Dst->rect().contains(ui->plainTextEdit_Dst->mapFromGlobal(QCursor::pos()))){
            //Cmdキー押下検知中だろうとなんだろうと要素先頭のパスで置き換え
            QFileInfo dstdir(drop_str_list[0].toLocalFile());
            if(dstdir.exists() && dstdir.isDir()){
                ui->plainTextEdit_Dst->setPlainText(drop_str_list[0].toLocalFile() + "/");
                CheckJobListmodified();
            }
        }
    }
}

bool MainWindow::eventFilter(QObject *target, QEvent *event){

    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

    if(event->type() == QEvent::ToolTip){
        if(target == ui->plainTextEdit_Src){
            QToolTip::showText(ui->plainTextEdit_Src->mapToGlobal(QPoint(0,10)),(char*)GetLoadStrV(TOOLTIP_TXTEDIT_SRC));
        }
        else if(target == ui->plainTextEdit_Dst){
            //Dstにマウスオーバーした時の入力ヒント表示
            //20は対象Widgetの左上からのpixel絶対座標。ヒントが下に出るようにpx値を微調整。。
            QToolTip::showText(ui->plainTextEdit_Dst->mapToGlobal(QPoint(0,10)),(char*)GetLoadStrV(TOOLTIP_TXTEDIT_DST));
        }
    }

    //イベント対象がDstフィールドかつイベントタイプはキー押下？
    if(event->type() == QEvent::KeyPress){
        if(keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return){
            if(target == ui->plainTextEdit_Dst){
                //plainTextEditは改行前提のクラスだけど、dstは一行しか許さないので改行イベントは無視！
                return true;
            }
            //JobListNameのコンボボックス上でEnterキー押すとなぜかActivatedシグナル飛んでクラッシュするので
            //EnterKeyを無視
            if(target == ui->comboBox_JobListName){
                return true;
            }
        }
        else if(keyEvent->key() == Qt::Key_Tab){
            if(target == ui->plainTextEdit_Src){
                //Tabキーの入力を無視してヒストリにフォーカスを移動する。
                //ファイル名にtabは使わないはずやし
                ui->pushButton_Srchist->setFocus();
                //Tab入力を無視してリターン
                return true;
            }
            else if(target == ui->plainTextEdit_Dst){
                ui->pushButton_Dsthist->setFocus();
                return true;
            }
        }
        else if(target == ui->listWidget_JobList){
            if(keyEvent->modifiers() & Qt::AltModifier && !joblistisstarting){
                int currentRow = ui->listWidget_JobList->currentRow();
                //一個上のジョブと位置入れ替え＆範囲外チェック
                if(keyEvent->key() == Qt::Key_Up && currentRow - 1 >= 0){
                    QListWidgetItem* currentItem = ui->listWidget_JobList->takeItem(currentRow);
                    ui->listWidget_JobList->insertItem(currentRow - 1,currentItem);
                    ui->listWidget_JobList->setCurrentItem(currentItem);
                    joblist.swap(currentRow,currentRow - 1);
                    UpdateJobListtoini();
                    on_listWidget_JobList_clicked(ui->listWidget_JobList->currentIndex());
                }
                //一個下のジョブと位置入れ替え＆範囲外チェック
                else if(keyEvent->key() == Qt::Key_Down && currentRow + 1 != joblist.count()){
                    QListWidgetItem* currentItem = ui->listWidget_JobList->takeItem(currentRow);

                    ui->listWidget_JobList->insertItem(currentRow + 1,currentItem);
                    ui->listWidget_JobList->setCurrentItem(currentItem);
                    joblist.swap(currentRow,currentRow + 1);
                    UpdateJobListtoini();
                    on_listWidget_JobList_clicked(ui->listWidget_JobList->currentIndex());
                }
                return true;
            }
            else{
                int currentRow = ui->listWidget_JobList->currentRow();
                if(keyEvent->key() == Qt::Key_Up && currentRow - 1 >= 0){
                    ui->listWidget_JobList->setCurrentRow(currentRow - 1);
                    on_listWidget_JobList_clicked(ui->listWidget_JobList->currentIndex());
                    return true;
                }
                else if(keyEvent->key() == Qt::Key_Down && currentRow + 1 != joblist.count()){
                    ui->listWidget_JobList->setCurrentRow(currentRow + 1);
                    on_listWidget_JobList_clicked(ui->listWidget_JobList->currentIndex());
                    return true;
                }
            }
            return false;
        }
    }

    if(keyEvent->modifiers() & Qt::ControlModifier){
        // CTRLおしっぱ？
        if(keyEvent->key() == Qt::Key_Control){
            // Listing表示に"+V"付与
            if(!ui->pushButton_Listing->text().endsWith("+V")){
                ui->pushButton_Listing->setText(ui->pushButton_Listing->text() + "+V");
                return false;
            }
        }

        //Cmd + k検出でディスクモード切り替え
        if(keyEvent->modifiers() & Qt::ControlModifier
            && keyEvent->key() == Qt::Key_K){
            // from IDR_DISKMODE
            diskMode = (diskMode + 1) % 3;
            UpdateMenu();
        }
        return false;
    }
    if(keyEvent->modifiers() & Qt::AltModifier){
        //編集状態でAlt(Option)+Sを押下すると文字入って保存されちゃう対策
        //
        //plainTextEdit_Src
        //plainTextEdit_Dst
        //comboBox_JobListName
        //listWidget_JobList
        //lineEdit_BUFF
        //comboBox_include
        //comboBox_exclude
        //comboBox_FromDate
        //comboBox_ToDate
        //comboBox_MinSize
        //comboBox_MaxSize
        if(keyEvent->key() == Qt::Key_S && ui->listWidget_JobList->currentRow() != -1){
            int currentRow = ui->listWidget_JobList->currentRow();
            //ジョブ名とジョブグループ名と実行モードは再利用
            QString title = ((char*)joblist[currentRow].title);
            bool islisting = joblist[currentRow].isListing;

            Job wk_job;
            GetJobfromMain(&wk_job);
            wk_job.SetString(title.toLocal8Bit().data(),
                    ui->plainTextEdit_Src->toPlainText().toLocal8Bit().data(),
                    copyInfo[ui->comboBox_Mode->currentIndex()].mode != FastCopy::DELETE_MODE ?
                     ui->plainTextEdit_Dst->toPlainText().toLocal8Bit().data() : (void*)"",
                    copyInfo[ui->comboBox_Mode->currentIndex()].cmdline_name,
                    wk_job.isFilter ? ui->comboBox_include->currentText().toLocal8Bit().data() : (void*)"",
                    wk_job.isFilter ? ui->comboBox_exclude->currentText().toLocal8Bit().data() : (void*)"",
                    wk_job.isFilter ? ui->comboBox_FromDate->currentText().toLocal8Bit().data() : (void*)"",
                    wk_job.isFilter ? ui->comboBox_ToDate->currentText().toLocal8Bit().data() : (void*)"",
                    wk_job.isFilter ? ui->comboBox_MinSize->currentText().toLocal8Bit().data() : (void*)"",
                    wk_job.isFilter ? ui->comboBox_MaxSize->currentText().toLocal8Bit().data() : (void*)"");
                    wk_job.isListing = islisting;
            joblist.removeAt(currentRow);
            joblist.insert(currentRow,wk_job);
            UpdateJobListtoini();
            status_label.setText((char*)GetLoadStrV(IDS_JOBLIST_JOBSAVE));
            //fontを通常に戻す
            QListWidgetItem *item = ui->listWidget_JobList->item(currentRow);
            QFont font = item->font();
            font.setItalic(false);
            font.setBold(false);
            item->setFont(font);
            joblist_isnosave = false;
            //Option+Sで入力される文字を入力させない
            return true;
        }
    }

    //送られてきたイベントはキー離れた？
    if(event->type() == QEvent::KeyRelease){

        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        // CTRLを離した？
        if(keyEvent->key() == Qt::Key_Control){
            // Listing表示から"+V"を取り除く
            if(ui->pushButton_Listing->text().endsWith("+V")){
                ui->pushButton_Listing->setText(
                 ui->pushButton_Listing->text().remove(
                  ui->pushButton_Listing->text().length()-2,ui->pushButton_Listing->text().length()));
                return false;
            }
        }
    }
    if(event->type() == QEvent::Enter){
        isMouseover = true;
    }
    else if(event->type() == QEvent::Leave){
        isMouseover = false;
    }
    return false;
}

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason){
    switch(reason){

        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
        case QSystemTrayIcon::MiddleClick:
             TaskTray(NIM_DELETE,NULL,NULL);
             this->showNormal();
             break;
        default:
             break;
    }
}

int MainWindow::SetSpeedLevelLabel(int level)
{
    if(level == -1){
        level = ui->speed_Slider->value();
    }
    else{
        ui->speed_Slider->setValue(level);
    }

    char	buf[64];
    sprintf(buf,
            level == SPEED_FULL ?		(char*)GetLoadStrV(IDS_FULLSPEED_DISP) :
            level == SPEED_AUTO ?		(char*)GetLoadStrV(IDS_AUTOSLOW_DISP) :
            level == SPEED_SUSPEND ?	(char*)GetLoadStrV(IDS_SUSPEND_DISP) :
                                        (char*)GetLoadStrV(IDS_RATE_DISP),
            level);
    ui->label_Slider->setText(buf);
    return	level;
}

void MainWindow::jobListjob_Change_Detected(){
    if(!joblist_cancelsignal){
        CheckJobListmodified();
    }
    return;
}

void MainWindow::on_actionUserDir_U_triggered()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(cfg.userDirV));
    QApplication::postEvent(this,new QKeyEvent(QEvent::KeyRelease,Qt::Key_Control,Qt::ControlModifier));
}

void MainWindow::on_actionOpenLog_triggered()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile((char*)cfg.errLogPathV));
    QApplication::postEvent(this,new QKeyEvent(QEvent::KeyRelease,Qt::Key_Control,Qt::ControlModifier));
}

void MainWindow::on_actionOpenDetailLogDir_triggered()
{
    QString detaillogdir_str(fileLogDirV);
    QDir detaillogdir(detaillogdir_str);
    //詳細ファイルログディレクトリがまだ存在してないなら作成する
    if(!detaillogdir.exists()){
        detaillogdir.mkpath(detaillogdir_str);
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(fileLogDirV));
    QApplication::postEvent(this,new QKeyEvent(QEvent::KeyRelease,Qt::Key_Control,Qt::ControlModifier));
}

void MainWindow::on_actionHelp_triggered()
{
    if(cfg.lcid == QLocale::Japanese){
        QDesktopServices::openUrl(QUrl::fromLocalFile(cfg.userDirV + "/" + LOCAL_HELPFILENAME_JA));
    }
    else{
        QDesktopServices::openUrl(QUrl::fromLocalFile(cfg.userDirV + "/" + LOCAL_HELPFILENAME_EN));
    }
}

void MainWindow::on_actionSwap_Source_DestDir_triggered()
{
    SwapTarget();
}

void MainWindow::on_actionShow_Extended_Filter_toggled(bool arg1)
{
    ReflectFilterCheck(arg1);
}

void MainWindow::on_actionMain_Settings_triggered()
{
    setupDlg = new mainsettingDialog(this,&cfg);

    if(setupDlg->exec() == QDialog::Accepted){

        //ジョブリストモード有効時、SetCopyModeList()延長で空のコンボボックス見ちゃうので
        //ジョブリストモード時のみ、余計なシグナル発生させないように蓋する。。
        if(ui->actionEnable_JobList_Mode->isChecked()){
            disconnect(ui->comboBox_Mode,SIGNAL(currentIndexChanged(int)),
                       this,SLOT(on_comboBox_Mode_currentIndexChanged(int)));
            SetJobListmodifiedSignal(false);

        }
        SetCopyModeList();
        if(ui->actionEnable_JobList_Mode->isChecked()){
            connect(ui->comboBox_Mode,SIGNAL(currentIndexChanged(int)),
                    this,SLOT(on_comboBox_Mode_currentIndexChanged(int)));
            SetJobListmodifiedSignal(true);
        }
        ui->lineEdit_BUFF->setText(QString::number(cfg.bufSize));
        ui->checkBox_Nonstop->setChecked(cfg.ignoreErr);
        ui->checkBox_Estimate->setChecked(cfg.estimateMode);
        ui->checkBox_Verify->setChecked(cfg.enableVerify);
        speedLevel = cfg.speedLevel;
        SetSpeedLevelLabel(speedLevel);
        UpdateSpeedLevel();
        ui->checkBox_owdel->setChecked(cfg.enableOwdel);
        ui->checkBox_Acl->setChecked(cfg.enableAcl);
        ui->checkBox_xattr->setChecked(cfg.enableXattr);
        isErrLog = cfg.isErrLog;
        fileLogMode = cfg.fileLogMode;
        isReparse = cfg.isReparse;
        skipEmptyDir = cfg.skipEmptyDir;
        forceStart = cfg.forceStart;
        ui->actionShow_Extended_Filter->setChecked(cfg.isExtendFilter);
        ui->actionEnable_JobList_Mode->setChecked(cfg.isJobListMode);
        ui->checkBox_LTFS->setChecked(cfg.enableLTFS);
        if(dotignoremode != cfg.Dotignore_mode){
            dotignoremode = cfg.Dotignore_mode;
            //ジョブリストモード有効時のみ.無視変更をチェック&セーブ促警告
            if(ui->actionEnable_JobList_Mode->isChecked()){
                CheckJobListmodified();
            }
        }
    }
    delete setupDlg;
}

void MainWindow::on_actionAuto_HDD_mode_triggered(bool checked)
{
    if(checked){
        diskMode = 0;
    }
    UpdateMenu();
}

void MainWindow::on_actionSame_HDD_mode_triggered(bool checked)
{
    if(checked){
        diskMode = 1;
    }
    UpdateMenu();
}

void MainWindow::on_actionDiff_HDD_mode_triggered(bool checked)
{
    if(checked){
        diskMode = 2;
    }
    UpdateMenu();
}

void MainWindow::on_actionWindowPos_Fix_triggered(bool checked)
{
    if(checked && IS_INVALID_POINT(cfg.macpos.x(),cfg.macpos.y())){

        QMessageBox msgbox;
        msgbox.setText((const char*)GetLoadStrV(IDS_FIXPOS_MSG));
        msgbox.setWindowTitle(FASTCOPY);
        msgbox.setStandardButtons(QMessageBox::Ok|QMessageBox::Cancel);
        msgbox.setDefaultButton(QMessageBox::Ok);
        if(msgbox.exec() == QMessageBox::Ok){
            cfg.macpos = this->pos();
            cfg.WriteIni();
        }
        else{
            //cancelしたのでチェックさせない
            ui->actionWindowPos_Fix->setChecked(false);
        }
    }
    else {
        cfg.macpos.setX(INVALID_POINTVAL);
        cfg.macpos.setY(INVALID_POINTVAL);
        cfg.WriteIni();
    }
}

void MainWindow::on_actionWindowSize_Fix_triggered(bool checked)
{
    if(checked && IS_INVALID_SIZE(cfg.macsize.width(),cfg.macsize.height())){

        QMessageBox msgbox;
        msgbox.setText((const char *)GetLoadStrV(IDS_FIXSIZE_MSG));
        msgbox.setStandardButtons(QMessageBox::Ok|QMessageBox::Cancel);
        msgbox.setDefaultButton(QMessageBox::Ok);
        msgbox.setWindowTitle(FASTCOPY);


        if(msgbox.exec() == QMessageBox::Ok){
            cfg.macsize.setWidth(this->width());
            cfg.macsize.setHeight(this->height() - (isErrEditHide ? ui->err_widget->height() : 0));
            cfg.WriteIni();
        }
        else{
            //cancelしたのでチェックさせない
            ui->actionWindowSize_Fix->setChecked(false);
        }
    }
    else{
        cfg.macsize.setWidth(INVALID_SIZEVAL);
        cfg.macsize.setHeight(INVALID_SIZEVAL);
        cfg.WriteIni();
    }
}

void MainWindow::on_actionAbout_triggered()
{
    aboutDlg = new aboutDialog(this,&cfg);
    aboutDlg->exec();
    delete aboutDlg;
}

//
void MainWindow::on_actionRun_RapidCopy_triggered()
{
    //コピーエンジンの動作中に他のRapidCopyを起動すると現在のRapidCopyで開いているfdが引き継がれてしまい、
    //マウントやアンマウントで問題となってしまう。
    //仕方ないのでコピーエンジン動作中は新しいウインドウを開く、をできないようにする。
    if(!fastCopy.IsStarting()){
        QString rapidcopy_str = QCoreApplication::applicationDirPath() + "/" + QCoreApplication::applicationName();
        QProcess *other_process = new QProcess();
        QStringList windowpos;

        //新しくウインドウ開く時はCLI側に現在の開始位置 + 30px移動したところで開始するようにコマンド引数渡す
        //execしちゃうので、ファイルハンドルを引き継いでしまう。
        windowpos << "--xpos" << QString::number(this->pos().x() + 30) << "--ypos" << QString::number(this->pos().y() + 30);
        other_process->start(rapidcopy_str,windowpos);

        QApplication::postEvent(this,new QKeyEvent(QEvent::KeyRelease,Qt::Key_Control,Qt::ControlModifier));
    }
    else{
        QMessageBox msgbox;
        msgbox.setStandardButtons(QMessageBox::Ok);
        msgbox.setText((char*)GetLoadStrV(IDS_NEWWINDOW_FAILED));
        msgbox.setWindowTitle(FASTCOPY);
        msgbox.exec();
    }
}

void MainWindow::on_speed_Slider_valueChanged(int value)
{
    speedLevel = SetSpeedLevelLabel(value);
    UpdateSpeedLevel();
}

void MainWindow::actpostprocess_triggered(QAction *_action)
{
    SetFinAct(finact_Group->actions().indexOf(_action));
}

void MainWindow::actpostprocessdlg_triggered()
{
    finactDlg = new FinActDialog(this,&cfg);
    finactDlg->exec();
    SetFinAct(finActIdx < cfg.finActMax ? finActIdx : 0);
    delete finactDlg;
}

void MainWindow::job_triggered(QAction *_action)
{
    int idx = cfg.SearchJobV(_action->text().toLocal8Bit().data());
    if(cfg.jobArray[idx]->flags & Job::SINGLE_JOB){
        SetJob(idx);
    }
    else if(cfg.jobArray[idx]->flags & Job::JOBLIST_JOB){
        SetJob(idx,false,true);
    }
    return;
}

void MainWindow::hist_triggered(QAction *_action)
{
    //iconTextにリセット指示の"."が入力されてきた？
    bool is_reset_req = _action->iconText().compare(DOT_V) == 0 ? true : false;

    //SIGNAL送信元判定
    //src？
    if(srchist_Group == QObject::sender()){
        //パス区切りの単位を";"からQChar::ParagraphSeparatorに変更したので
        //そのままロードすればok
        //text()で出てくるのは表示専用の省略形なので使っちゃだめよ
        if(is_reset_req){
            //中身リセット
            ui->plainTextEdit_Src->clear();
            status_label.setText((char*)GetLoadStrV(IDS_SRCPATH_CLEAR));
        }
        else{
            //履歴データのケツにはParagraphSeparatorつけてないので、データカウント+1は水増し
            castPath(ui->plainTextEdit_Src,
                     _action->iconText(),
                     _action->iconText().count(QChar::ParagraphSeparator) + 1,
                     false);
            CheckJobListmodified();
        }
    }
    //dst
    else if(dsthist_Group == QObject::sender()){
        //dstは複数パスゆるさないのでそのままはっつければおｋ
        if(is_reset_req){
            ui->plainTextEdit_Dst->clear();
            status_label.setText((char*)GetLoadStrV(IDS_DSTPATH_CLEAR));
        }
        else{
            ui->plainTextEdit_Dst->setPlainText(_action->iconText());
            CheckJobListmodified();
        }
    }
}

void MainWindow::on_actionShow_SingleJob_triggered(){
    jobDlg = new JobDialog(this,&cfg);
    jobDlg->exec();
    UpdateMenu();
    delete jobDlg;
}


void MainWindow::on_checkBox_Verify_clicked(bool checked){
    verify_before = checked;
}

void MainWindow::on_Latest_URL_triggered(){
    //cfgが日本語設定なら日本語URLページに飛ばす
    if(cfg.lcid == QLocale::Japanese){
        QDesktopServices::openUrl(QUrl((char*)GetLoadStrV(IDS_FASTCOPYURL),QUrl::TolerantMode));
    }
    //そうじゃなければ英語ページ
    else{
        QDesktopServices::openUrl(QUrl((char*)GetLoadStrV(IDS_FASTCOPYURL_EN),QUrl::TolerantMode));
    }
}

void MainWindow::on_actionSave_Report_at_Desktop_triggered()
{
    //レポート機能を実行不可能に変更
    ui->actionSave_Report_at_Desktop->setEnabled(false);
    //実行した瞬間の時刻取得
    dateTime = QDateTime::currentDateTime();
    commands_donecnt = 0;

    commands[0] = "lsof";
    commands[1] = "uname";
    commands[2] = "top";

    //args[0] << "";		 lsofは引数不要
    args[1] << "-a";		 //uname引数
    args[2] << "-n" << "1" << "-b";	 //top引数

    int i;

    QFileInfo env_dir(cfg.userDirV);
    if(env_dir.exists() && env_dir.isDir() && env_dir.isWritable()){
        for(i=0;i < REPORT_COMMAND_NUM;i++){

            QObject::connect(&command_process[i],SIGNAL(readyReadStandardOutput()), this, SLOT(process_stdout_output()));
            QObject::connect(&command_process[i],SIGNAL(finished(int, QProcess::ExitStatus)),
                             this,SLOT(process_exit()));
            if(args[i].isEmpty()){
                command_process[i].start(commands[i]);
            }
            else{
                command_process[i].start(commands[i],args[i]);
            }
        }
    }
}

//保守資料取得時コマンドプロセスread出口
void MainWindow::process_stdout_output(){
    //on_actionSave_Report_at_Desktop_triggeredで設定したコマンドの内、
    //最後のzipコマンド以外の標準出力のreadを順不同で実行
    for(int i=0; i<REPORT_COMMAND_NUM; i++){
        //プロセス特定
        if(sender() == &command_process[i]){
            //対象プロセスに対応するQByteArrayバッファに今回分の標準出力内容を加算
            p_data[i] += command_process[i].readAllStandardOutput();
        }
    }
}
//保守資料取得時コマンドプロセスexit出口
void MainWindow::process_exit(){

    for(int i=0;i < REPORT_COMMAND_NUM;i++){
        //プロセス特定
        if(sender() == &command_process[i]){
            //出力用ファイル名生成
            QFile env_file(cfg.userDirV +
                            "/" +
                            commands[i] +
                            "_" +
                            dateTime.date().toString("yyyyMMdd_") +
                            dateTime.time().toString("hhmmss") +
                            ".txt");
            env_file.open(QIODevice::WriteOnly);
            env_file.write(p_data[i]);
            //ファイルクローズ
            env_file.close();
            //バッファクリア
            p_data[i].clear();
            //signal/slot解除
            QObject::disconnect(&command_process[i],SIGNAL(readyReadStandardOutput()), this, SLOT(process_stdout_output()));
            QObject::disconnect(&command_process[i],SIGNAL(finished(int, QProcess::ExitStatus)),
                                this, SLOT(process_exit()));
        }
    }
    commands_donecnt++;
    //保守コマンド全部終了した？
    if(commands_donecnt == REPORT_COMMAND_NUM){
        //zipコマンド実行よ
        char	buf[MAX_PATH * 2];	  //最終zip出力先フルパス名
        QProcess zip_process;
        QStringList zip_args;
        zip_args << "-r";
        zip_args << "-q";

        zip_args.append(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) +
                       "/" +
                       "Report_" +
                       dateTime.date().toString("yyyyMMdd_") +
                       dateTime.time().toString("hhmmss") +
                       ".zip");
        zip_args << cfg.userDirV;
        zip_process.execute("zip",zip_args);

        //コマンドで生成したファイルを削除しておく
        for(int i=0; i<REPORT_COMMAND_NUM; i++){
            QFile delete_file(cfg.userDirV +
                               "/" +
                               commands[i] +
                               "_" +
                               dateTime.date().toString("yyyyMMdd_") +
                               dateTime.time().toString("hhmmss") +
                               ".txt");
            delete_file.remove();
        }

        //レポートまとめ処理終わったよ通知
        QMessageBox msgbox;
        msgbox.setIcon(QMessageBox::Information);
        msgbox.setStandardButtons(QMessageBox::Ok);
        sprintfV(buf,GetLoadStrV(IDS_REPORTOUT),zip_args.at(2).toLocal8Bit().data());
        msgbox.setText(buf);
        msgbox.setWindowTitle(FASTCOPY);
        msgbox.exec();

        //レポート機能を実行可能に変更
        ui->actionSave_Report_at_Desktop->setEnabled(true);
    }
}



void MainWindow::on_plainTextEdit_Src_textChanged()
{
    ui->actionSwap_Source_DestDir->setEnabled(SwapTarget(true));
    if(!joblist_cancelsignal){
        CheckJobListmodified();
    }
}

void MainWindow::on_actionQuit_triggered()
{
    this->on_actionClose_Window_triggered();
}

void MainWindow::on_actionClear_Source_and_DestDir_triggered()
{
    ui->plainTextEdit_Src->clear();
    ui->plainTextEdit_Dst->clear();
}

void MainWindow::on_plainTextEdit_Src_blockCountChanged(int newBlockCount)
{

    //setBlockFormatするとon_plainTextEdit_Src_textChanged()コールされちゃう。。
    //仕方ないのでこいつを一時的にdisconnect
    disconnect(ui->plainTextEdit_Src,SIGNAL(textChanged()),
               this,SLOT(on_plainTextEdit_Src_textChanged()));

    for(int i=0;i < newBlockCount; i++){
        //対象行(ブロック)を行番号でサーチ
        //QTextBlock block = ui->plainTextEdit_Src->document()->findBlockByLineNumber(i);
        QTextBlock block = ui->plainTextEdit_Src->document()->findBlockByLineNumber(i);
        //ブロックフォーマットを取得
        QTextBlockFormat fmt = block.blockFormat();
        //奇数行？
        if((i % 2) == 0){
            fmt.setBackground(QColor("white"));
        }
        //偶数
        else{
            //fmt.setBackground(default_palette.background());
            //もともとはFinderに合わせてaliceblueだったけど、駄目モニタ対応で少し濃いlightcyanに変更
            //http://www.w3.org/TR/SVG/types.html#ColorKeywords
            //pick合わせした独自カラーに変更
            fmt.setBackground(QColor(226,241,255));
        }
        //対象ブロックのカーソル取得
        QTextCursor cursor(block);
        //対象ブロックのカーソルに色変更したブロックフォーマットを適用
        cursor.setBlockFormat(fmt);
    }
    //再接続
    connect(ui->plainTextEdit_Src,SIGNAL(textChanged()),
            this,SLOT(on_plainTextEdit_Src_textChanged()));
}


void MainWindow::on_plainTextEdit_Dst_textChanged()
{
    if(!joblist_cancelsignal){
        CheckJobListmodified();
    }
}

void MainWindow::on_actionClose_Window_triggered()
{
    this->close();
}

void MainWindow::on_comboBox_Mode_currentIndexChanged(int index){
    BOOL	is_delete = GetCopyMode() == FastCopy::DELETE_MODE;
    if(is_delete){
        //SetDlgItemTextV(SRC_COMBO, EMPTY_STR_V);
        //isNetPlaceSrc = FALSE;
    }
    is_delete ? SetItemEnable(true) :
        GetCopyMode() == FastCopy::VERIFY_MODE ? SetItemEnable_Verify(true) :
        SetItemEnable(false);
    // 作者からのヒントにより
    // DROPDOWN,CLOSEUPイベントでのリスト登録処理は行わない。
    // ExecCopy()延長とEnd()延長で更新するだけでいいや
    if(!joblist_cancelsignal){
        CheckJobListmodified();
    }
    return;
}

void MainWindow::on_pushButton_JobDelete_clicked(){

    int currentRow = ui->listWidget_JobList->currentRow();
    if(currentRow != -1){
        QMessageBox msgbox;
        msgbox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgbox.setIcon(QMessageBox::Question);
        msgbox.setText((char*)GetLoadStrV(IDS_JOB_DELETEREQ));
        if(msgbox.exec() == QMessageBox::Yes){
            QListWidgetItem* currentItem = ui->listWidget_JobList->takeItem(currentRow);
            //ジョブリスト内のジョブ削除対象が他のリストに所属してないジョブ？
            if(cfg.DelJobinJobList(currentItem->text().toLocal8Bit().data(),
                               ui->comboBox_JobListName->currentText().toLocal8Bit().data())
                                == 0){
                //他ジョブリスト上の存在数0なのでJob自体も削除する。
                cfg.DelJobV(currentItem->text().toLocal8Bit().data());
            }
            ui->listWidget_JobList->removeItemWidget(currentItem);
            joblist.removeAt(currentRow);
            UpdateJobListtoini();
        }
    }
}

void MainWindow::on_pushButton_JobDeleteAll_clicked(){
    QMessageBox msgbox;
    msgbox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgbox.setIcon(QMessageBox::Question);
    msgbox.setText((char*)GetLoadStrV(IDS_JOB_DELETEALLREQ));
    if(msgbox.exec() == QMessageBox::Yes){

        for(int i=0;i<joblist.count();i++){
            if(cfg.DelJobinJobList(joblist.at(i).title,
                                   ui->comboBox_JobListName->currentText().toLocal8Bit().data())
                                    == 0){
                cfg.DelJobV(joblist.at(i).title);
            }
        }
        ui->listWidget_JobList->clear();
        joblist.clear();
        ui->listWidget_JobList->clearFocus();
        UpdateJobListtoini();
    }
}

void MainWindow::on_pushButton_JobListSave_clicked(){
    //今表示しているジョブリスト名称とジョブ一覧をcfgにsaveする
    if(joblist.count() && !ui->comboBox_JobListName->currentText().isEmpty()){
        QMessageBox msgbox;
        if(UpdateJobListtoini()){
            msgbox.setStandardButtons(QMessageBox::Ok);
            msgbox.setIcon(QMessageBox::Information);
            msgbox.setText((char*)GetLoadStrV(IDS_JOBLISTACT_SUCCESS_INFO));
        }
        else{
            msgbox.setStandardButtons(QMessageBox::Ok);
            msgbox.setIcon(QMessageBox::Warning);
            msgbox.setText((char*)GetLoadStrV(IDS_JOBLISTACT_ERROR_INFO));
        }
        msgbox.exec();
    }
}

void MainWindow::on_pushButton_JobListDelete_clicked(){
    //空チェック
    if(ui->comboBox_JobListName->currentText().isEmpty()){
        return;
    }

    QMessageBox msgbox_inq;
    msgbox_inq.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgbox_inq.setIcon(QMessageBox::Question);
    msgbox_inq.setText((char*)GetLoadStrV(IDS_JOBLIST_DELETEREQ));

    //削除ダイアログ確認
    if(msgbox_inq.exec() == QMessageBox::No){
        //cancel
        return;
    }
    //ジョブリスト削除実行
    //成功のときはメッセージいちいち出さない、2回出るのうっとおしいでしょ
    if(!DeleteJobListtoini(ui->comboBox_JobListName->currentText())){
        QMessageBox msgbox_info;
        msgbox_info.setStandardButtons(QMessageBox::Ok);
        msgbox_info.setIcon(QMessageBox::Warning);
        msgbox_info.setText((char*)GetLoadStrV(IDS_JOBLISTDEL_ERROR_INFO));
        //削除失敗時だけ表示
        msgbox_info.exec();
    }
    UpdateJobList();
    //delete時はJobオーダー表示とワークQList<Job>をクリア
    ui->listWidget_JobList->clear();
    joblist.clear();
    //メニュー表示再更新
    UpdateMenu();
    return;
}

void MainWindow::on_pushButton_JobListRename_clicked()
{
    if(ui->comboBox_JobListName->currentText().isEmpty()){
        return;
    }

    QString rename_text;
    JobListRenameDialog *renamedialog = new JobListRenameDialog(this,
                                                                &cfg,
                                                                ui->comboBox_JobListName->currentText(),
                                                                &rename_text);
    switch(renamedialog->exec()){
        case QDialog::Accepted:
            if(!DeleteJobListtoini(ui->comboBox_JobListName->currentText())){
                QMessageBox msgbox_info;
                msgbox_info.setStandardButtons(QMessageBox::Ok);
                msgbox_info.setIcon(QMessageBox::Warning);
                msgbox_info.setText((char*)GetLoadStrV(IDS_JOBLISTDEL_ERROR_INFO));
                //削除失敗時だけ表示
                msgbox_info.exec();
                break;
            }
            //削除成功したので新しい名前で再作成
            //新しい名前を設定
            ui->comboBox_JobListName->setCurrentText(rename_text);
            UpdateJobListtoini();
            break;
        case QDialog::Rejected:
        default:
            break;
    }
    return;
}

void MainWindow::on_comboBox_JobListName_activated(int index){
    //ジョブリスト表示リセット
    ui->listWidget_JobList->clear();
    //メインウインドウ内でがちゃる用のワークジョブ配列をクリア
    joblist.clear();
    //選択したジョブリストからジョブをロード,ジョブリスト表示反映とワークジョブリストに同期
    //ジョブリスト数分でぐーるぐる
    //一個のジョブリスト中にあるジョブのかずぶんぐーるぐる
    for(int j=0; j < cfg.joblistArray[index]->joblistentry; j++){
        //Jobのindex特定するよー
        int jobidx = cfg.SearchJobV(cfg.joblistArray[index]->jobname_pt[j]);
        if(jobidx >= 0){
            joblist.append(*cfg.jobArray[jobidx]);
            QListWidgetItem *item = new QListWidgetItem((char*)cfg.joblistArray[index]->jobname_pt[j]);
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            ui->listWidget_JobList->addItem(item);
            //debug
            //joblist[j].debug();
        }
    }
    //強制起動有無を反映
    ui->checkBox_JobListForceLaunch->setChecked(cfg.joblistArray[index]->forcelaunch);
}

void MainWindow::on_comboBox_JobListName_editTextChanged(const QString &arg1)
{
    int idx = cfg.SearchJobList(arg1.toLocal8Bit().data());
    if(idx == -1){
        //対象のジョブリスト名は存在しない
        ui->pushButton_JobListSave->setEnabled(true);
        ui->pushButton_JobListDelete->setEnabled(false);

        ui->pushButton_JobListRename->setEnabled(false);
    }
    else{
        //対象のジョブリストが存在する
        ui->pushButton_JobListSave->setEnabled(false);
        ui->pushButton_JobListRename->setEnabled(true);
        ui->pushButton_JobListDelete->setEnabled(true);
    }
}

void MainWindow::on_checkBox_JobListForceLaunch_clicked(bool checked)
{
    //空チェック
    if(!ui->comboBox_JobListName->currentText().isEmpty()){
        //joblistインデックス特定
        int index = cfg.SearchJobList(ui->comboBox_JobListName->currentText().toLocal8Bit().data());
        //対象ジョブリスト名特定できた？かつ変更が確定？
        if(index != -1 && cfg.joblistArray[index]->forcelaunch != checked){
            cfg.joblistArray[index]->forcelaunch = checked;
            UpdateJobListtoini();
        }
    }
}

void MainWindow::on_listWidget_JobList_itemChanged(QListWidgetItem *item){
    //listWidgetのsetBackgroundColor()などでitemChangedイベント発生してしまうので、
    //ジョブリストモードで実行中の場合には当該イベント無視する
    //lstrcmpiVによる判定はitem->setText対策
    if(!joblistisstarting &&
        lstrcmpiV(item->text().toLocal8Bit().data(),(char*)joblist[item->listWidget()->currentIndex().row()].title) != 0){
        Job	*job_pt = &joblist[item->listWidget()->currentIndex().row()];
        if(cfg.SearchJobV(item->text().toLocal8Bit().data()) != -1){
            //強制的にもとにもどす、このsetTextにより本関数が再度呼ばれちゃうのがなんだかな。。
            item->setText((char*)job_pt->title);
            QMessageBox msgbox;
            msgbox.setStandardButtons(QMessageBox::Ok);
            msgbox.setText((char*)GetLoadStrV(IDS_JOBNAME_DUPERROR));
            msgbox.setIcon(QMessageBox::Warning);
            msgbox.exec();
            return;
        }
        //対象Jobリストからジョブを削除したけど、他のジョブリストに当該ジョブが存在していない？
        if(cfg.DelJobinJobList(job_pt->title,
                               ui->comboBox_JobListName->currentText().toLocal8Bit().data())
                                == 0){
            //他ジョブリスト上の存在数0なのでJob自体も削除する。
            cfg.DelJobV(job_pt->title);
        }
        //とりあえず確保してた空間を解放
        free(job_pt->title);
        //割り当てし直し
        job_pt->title = strdupV(item->text().toLocal8Bit().data());
        status_label.setText((char*)GetLoadStrV(IDS_JOBNAME_CHANGE));
        UpdateJobListtoini();
    }
    return;
}

void MainWindow::listWidget_JobList_rowsMoved(QModelIndex parent, int start, int end, QModelIndex dest, int row){

    int targetindex = row - 1;

    if(start < targetindex){
        //move front to back
        Job insert = joblist.takeAt(start);
        joblist.insert(targetindex,insert);
    }
    else{
        //move back to front
        Job insert = joblist.takeAt(start);
        joblist.insert(targetindex + 1,insert);
    }
    UpdateJobListtoini();
}

void MainWindow::on_actionEnable_JobList_Mode_toggled(bool arg1){
    if(arg1){
        UpdateJobList();
        EnableJobList(arg1);
        EnableJobMenus(arg1);
        ui->central_batch_widget->show();
        this->resize(this->width() + ui->central_batch_widget->width(),this->height());
        ui->pushButton_JobListStart->show();
    }
    else{
        EnableJobList(arg1);
        EnableJobMenus(true);
        ui->comboBox_JobListName->clear();
        ui->listWidget_JobList->clear();
        joblist.clear();
        ui->pushButton_JobListStart->hide();
        ui->central_batch_widget->hide();
        this->resize(this->width() - ui->central_batch_widget->width(),this->height());
    }
}

void MainWindow::on_listWidget_JobList_clicked(const QModelIndex &index){
    SetJob(index.row(),true,true);
    status_label.clear();

    disconnect(ui->listWidget_JobList,SIGNAL(itemChanged(QListWidgetItem*)),
               this,SLOT(on_listWidget_JobList_itemChanged(QListWidgetItem*)));
    //クリックで遷移してきたら、とりあえず全アイテムのフォントを変更無しの標準フォントに戻しておく
    for(int i=0;i < ui->listWidget_JobList->count();i++){
        QListWidgetItem *item = ui->listWidget_JobList->item(i);
        QFont font = item->font();
        font.setItalic(false);
        font.setBold(false);
        item->setFont(font);
    }
    //コネクトし直し
    connect(ui->listWidget_JobList,SIGNAL(itemChanged(QListWidgetItem*)),
           this,SLOT(on_listWidget_JobList_itemChanged(QListWidgetItem*)));
}



void MainWindow::on_checkBox_LTFS_stateChanged(int arg1)
{
    switch(arg1){
        case Qt::Unchecked:
        case Qt::Checked:
            //SetCopyModeList延長で余計なシグナルがいっぱい動いちゃうので
            //ピンポイントにシグナル発生しないようにする。。
            if(ui->actionEnable_JobList_Mode->isChecked()){
                disconnect(ui->comboBox_Mode,SIGNAL(currentIndexChanged(int)),
                           this,SLOT(on_comboBox_Mode_currentIndexChanged(int)));
                SetJobListmodifiedSignal(false);
            }

            SetCopyModeList();
            //SIGNAL再接続
            if(ui->actionEnable_JobList_Mode->isChecked()){
                connect(ui->comboBox_Mode,SIGNAL(currentIndexChanged(int)),
                        this,SLOT(on_comboBox_Mode_currentIndexChanged(int)));
                SetJobListmodifiedSignal(true);
            }
            break;

        case Qt::PartiallyChecked:
        default:
            break;
    }
}

/*=========================================================================
  クラス ： Command_Thread
  概  要 ： 終了時処理の延長で任意コマンド実行するときのコマンド実行用スレッド
=========================================================================*/
Command_Thread::Command_Thread(QString *_command,int _stack_size,QStringList *_arg_v)
{
    this->my_stacksize = _stack_size;
    command = _command;
    arg_v = _arg_v;
}
void Command_Thread::run(){

    QProcess post_process;
    if(arg_v == NULL){
        post_process.execute(*command);
    }
    else{
        post_process.execute(*command,*arg_v);
    }
}
