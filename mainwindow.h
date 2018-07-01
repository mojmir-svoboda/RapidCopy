/* ========================================================================
    Project  Name			: Fast/Force copy file and directory
    Create					: 2016-02-28(Sun)
    Copyright				: Kengo Sawatsu
    Reference				:
    ======================================================================== */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <sysexits.h>
#include <QMainWindow>
#include <mainsettingdialog.h>
#include <confirmdialog.h>
#include <aboutdialog.h>
#include <finactdialog.h>
#include <jobdialog.h>
#include <joblistrenamedialog.h>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QTimer>
#include <QThread>
//#include <QSystemSemaphore>
#include <QProcess>
#include <time.h>
#include <QFileDialog>
#include <QDropEvent>
#include <QActionGroup>
#include <QSystemTrayIcon>
#include <QCoreApplication>
#include <QTranslator>
#include <QListWidgetItem>
#include <QMenu>
#include <QLabel>
#include <QCommandLineParser>
#include <QDesktopServices>
#include <QMimeData>
#include <QTextBlock>
#include <QTreeView>
#include <semaphore.h>
#include <sys/utsname.h>
#include "cfg.h"

#define NIM_ADD 0
#define NIM_MODIFY 1
#define NIM_DELETE 2

#define FASTCOPYLOG_MUTEX	FASTCOPY_ELOG_MUTEX

#define UNICODE_PARAGRAPH_BYTES 3

//mac OS X では16384が開発機におけるFinderのD&Dでの入力maxパス数だった。
//しかし、16384をフルにQActionに放り込むと選択でぐーるぐるしちゃう&&Qt内部でエラーになっちゃうので、
//安全に倒して1024パスくらいを制限とする。
#define NEW_MAX_HISTORY_BUF	((MAX_PATH + UNICODE_PARAGRAPH_BYTES + STR_NULL_GUARD) * 1024)
#define MAX_HISTORY_BUF		8192
#define MAX_HISTORY_CHAR_BUF (MAX_HISTORY_BUF * 4)

#define MINI_BUF 128

#define FILELOG_MINSIZE	512 * 1024

struct CopyInfo {
    UINT				resId;
    char				*list_str;
    UINT				cmdline_resId;
    void				*cmdline_name;
    FastCopy::Mode		mode;
    FastCopy::OverWrite	overWrite;
};

#define MAX_NORMAL_FASTCOPY_ICON	4
#define FCNORMAL_ICON_IDX			0
#define FCWAIT_ICON_IDX				MAX_NORMAL_FASTCOPY_ICON
#define MAX_FASTCOPY_ICON			MAX_NORMAL_FASTCOPY_ICON + 1
#define ICON_RESOURCE_NAMEBASE			":/icon/icon/"
#define ICON_RESOURCE_EXTNAME			".png"
#define TRANSLATE_RESOURCE_NAMEBASE		":/qm"
#define HELP_RESOURCE_NAMEBASE_JA		":/help/help/index.html"
#define HELP_RESOURCE_NAMEBASE_EN		":/help/help/index_en.html"
#define HELP_RESOURCE_NAMEBASE_EMAIL	":/help/help/RapidCopyEmailSettingExample.png"
#define LOCAL_HELPFILENAME_JA			"help_ja.html"
#define LOCAL_HELPFILENAME_EN			"help_en.html"
#define LOCAL_HELPFILENAME_EMAIL		"RapidCopyEmailSettingExample.png"
#define LOCAL_LICENSEFILE_JP			"license_jp.txt"
#define LOCAL_LICENSEFILE_EN			"license_en.txt"


#define SOUND_RESOURCE_NUM				28			//:/sound/sound/RapidCopy_SE_XX num
#define SOUND_RESOURCE_NAMEBASE			":/sound/sound/RapidCopy_SE_"				// + "XXX.wav"
#define LOCAL_SOUNDFILENAME_NAMEBASE	"/RapidCopy_SE_"							// + "XXX.wav"
#define RAPIDCOPY_REC_DIVIDER			"-------------------------------------------------\n"
#define RAPIDCOPY_CSV_REC_DIVIDER		"//////////\n"

#define SPEED_FULL		11
#define SPEED_AUTO		10
#define SPEED_SUSPEND	0

#define REPORT_COMMAND_NUM 3					//保守資料取得アクション時に実行するコマンドの個数

#define FINACTCMD_STACKSIZE (1024 * 1024 * 1)	//後処理コマンド実行スレッドのスタックサイズ

// ファイルパスをdragしてくるとついてくるURI
#define URI_FILE_STR "file://"

#define JOBLISTNAME_MAX 512	                            //JOBLIST name maxlen(include null)
#define CMD_ARG_ERR EX_USAGE							//CLIモード実行で引数不正

class Command_Thread : public QThread{
    Q_OBJECT
public:
    Command_Thread(QString *command,int stack_size,QStringList *arg_v=NULL);
    void run();

private:
    int my_stacksize;					//自Thread StackSize
    QString *command;					//command文字列
    QStringList *arg_v;					//引数リスト
};

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public slots:

    bool	EvClicked(void);
    //QtではGUIの操作を行うのはメインスレッドじゃないとダメ。
    //しかし、他スレッドからpostEventしても他のスレッド自身のイベントディスパッチャに渡されてしまいフリーズ。
    //仕方ないのでGUI更新を行うイベント出口処理をピンポイントにSLOT化
    FastCopy::Confirm	ConfirmNotify(FastCopy::Confirm _confirm);

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    bool	EnableErrLogFile(bool on);
    int		CmdNameToComboIndex(void *cmd_name);

    bool	CommandLineExecV(int argc,QStringList argv);

    bool	SetMiniWindow(void);
    bool	SetWindowTitle();
    bool	IsRunasshow() {return isRunAsshow;}
    _int64	GetDateInfo(void *buf, bool is_end);
    _int64	GetSizeInfo(void *buf);
    bool	SetInfo(bool is_finish_status=FALSE);
    bool	SetStatusBarInfo(TransInfo *ti_ptr,bool is_finish_status);
    bool	SetJobListInfo(TransInfo *ti,bool is_finish_status);
    enum	SetHistMode { SETHIST_LIST, SETHIST_EDIT, SETHIST_CLEAR, SETHIST_INIT };
    void	SetComboBox(QComboBox *item, void **history, SetHistMode mode);
    void	SetComboBox_new(QPlainTextEdit *item, void **history, SetHistMode mode);
    void	SetPathHistory_new(SetHistMode mode, QObject *item);
    void	SetFilterHistory(SetHistMode mode, QComboBox *item=NULL);
    bool	TaskTray(int nimMode, QIcon*hSetIcon , char* tip);
    CopyInfo *GetCopyInfo() { return copyInfo; }
    void	SetFinAct(int idx);
    int		GetFinActIdx() { return finActIdx; }
    QString	GetJobTitle();
    void	SetJobTitle(void *title);
    void	DelJobTitle(void);
    int		GetSrclen();
    int		GetDstlen();
    QString	GetSrc();
    QString	GetDst();
    int		GetDiskMode();
    int		GetCopyModeIndex();
    void	GetJobfromMain(Job *job);
    void	GetFilterfromMain(char* inc,char* exc,char* from_date,
                                 char* to_date,char* min_size,char* max_size);
    FastCopy::Mode	GetCopyMode(void);
    FastCopy::OverWrite GetOverWriteMode(void);

    void	sighandler_mainwindow(int signum);

protected:
    enum AutoCloseLevel { NO_CLOSE, NOERR_CLOSE, FORCE_CLOSE };
    enum { NORMAL_EXEC=1, LISTING_EXEC=2, CMDLINE_EXEC=4 ,VERIFYONLY_EXEC=8};
    enum { RUNAS_IMMEDIATE=1, RUNAS_SHELLEXT=2, RUNAS_AUTOCLOSE=4 };

    FastCopy		fastCopy;
    FastCopy::Info	info;

    int				orgArgc;
    QStringList		orgArgv;
    Cfg				cfg;
    QIcon			*hMainIcon[MAX_FASTCOPY_ICON];
    CopyInfo		*copyInfo;
    int				finActIdx;
    int				doneRatePercent;
    int				lastTotalSec;
    int				calcTimes;
    bool			isAbort;

    AutoCloseLevel autoCloseLevel;
    bool		isTaskTray;
    int			speedLevel;
    bool		isMouseover;		//マウスオーバー判定フラグ true=マウスオーバー中 false=マウスオーバー無し

    bool		noConfirmDel;
    bool		noConfirmStop;
    UINT		diskMode;
    bool		isRegExp;
    bool		isShellExt;         //未使用未実装
    int			skipEmptyDir;
    int			forceStart;
    bool		dotignoremode;
    char		errLogPathV[MAX_PATH];
    char		fileLogPathV[MAX_PATH];
    char		fileCsvPathV[MAX_PATH];
    QString		fileLogDirV;
    char		statLogPathV[MAX_PATH];
    bool		isErrLog;
    int			fileLogMode;
    bool		isReparse;
    bool		isLinkDest;
    int			maxLinkHash;
    bool		isReCreate;
    bool		resultStatus;

    int			listBufOffset;
    int			csvfileBufOffset;
    int			errBufOffset;
    bool		isErrEditHide;
    UINT		curIconIndex;
    bool		isDelay;

    struct tm	startTm;
    QString		*pathLogBuf;
    QString		*pathLogcsvBuf;
    int			hErrLog;
    void		*hErrLogMutex;
    int			hFileLog;
    int			hcsvFileLog;			//csv出力専用ファイルハンドル
    int			hStatLog;
    TransInfo	ti;
    QString		mailsub;				//subject(件名)用文字列
    QString		mailbody;				//body(本文)用文字列
    bool		isCommandStart;			//CLIで起動したか？true=CLI,false=GUI

    // for detect autoslow status
    DWORD		timerCnt;
    DWORD		timerLast;
    QThread::Priority	curPriority;
    QTimer		mw_Timer;				//メインウインドウ用タイムアウトタイマ
    QTime		sys_time;				// システム起動時からの通算秒取得用
    int			mw_TimerId;				// mainwindowタイマID
    Command_Thread	*hCommandThread;	//後処理の外部コマンド実行用スレッド
    QString		jobtitle_static;		//カレントjob名用ワーク。JOBTITLE_STATICの代わり
    QSystemTrayIcon systray;            //最小化時のシステムトレイオブジェクト

    bool		isRunAsshow;			//スタート時、GUI表示する?
    bool		verify_before;			//GUIでのベリファイ専用モード指定時、変更前のverify有無記憶
    QPalette	default_palette;		//標準の色覚えておく用
    QMediaPlayer player;				//終了時処理でのサウンド再生用
    QProcess	command_process[REPORT_COMMAND_NUM];		//保守資料取得時起動するプロセス数
    QString		commands[REPORT_COMMAND_NUM];				//実行するコマンド文字列の配列
    QStringList	args[REPORT_COMMAND_NUM];					//実行コマンドに投入する引数
    QByteArray	p_data[REPORT_COMMAND_NUM];					//実行コマンドの標準出力を受け取るバッファ
    QDateTime	dateTime;				//保守資料取得実行時刻
    int			commands_donecnt;		//保守資料取得時、並列同時実行したプロセスの実行終了した数
    QMenu		src_HistoryMenu;		//src入力フィールドのヒストリ用メニュー
    QMenu		dst_HistoryMenu;		//dst入力フィールドのヒストリ用メニュー
    QLabel		status_label;			//statusBarに表示する文字列用領域

    //各種設定用ダイアログ
    mainsettingDialog *setupDlg;
    confirmDialog	  *confirmDlg;
    aboutDialog		  *aboutDlg;
    FinActDialog	  *finactDlg;
    JobDialog		  *jobDlg;
    JobListRenameDialog *joblistrenameDlg;

    //終了時処理のメニューアクション用
    QActionGroup	  *finact_Group;
    QAction			  *finAct_Menu;
    //ジョブ関連メニューアクション用
    QActionGroup	  *job_Group;		//jobの数に合わせてがんがる
    QList<QMenu*>	  job_Groups;		//JobGroupの数にあわせて作成されるメニュー群
    QMenu			  *joblist_menu;	//fix joblist menu;
    //srchistory
    QActionGroup	  *srchist_Group;
    //dsthistory
    QActionGroup	  *dsthist_Group;
    //QApplicationインスタンス
    QCoreApplication  *main;
    QTranslator		  *translater;

    QList<Job>		joblist;			//ジョブリストのワーク
    int				joblistcurrentidx;	//現在実行中のジョブリスト中のジョブ番号
    bool			joblistisstarting;	//true=ジョブリスト実行中 false=ジョブリスト実行してない
    bool			joblistiscancel;	//true=ジョブリストキャンセル要求 false = ジョブリストキャンセルしてない
    struct tm		jobliststarttm;		//ジョブリストがスタートした時の時刻
    int				joblistcurrentfileidx;//詳細ファイルログ枝番用
    int				joblist_joberrornum;//ジョブリスト中で失敗したジョブの数
                                        //0=いずれのジョブも失敗してない. 数=エラーになったジョブの数
    bool			joblist_cancelsignal;//ジョブリストの変更検知用フラグ
    bool			joblist_isnosave;	//ジョブリストがセーブ済みじゃないフラグ
    Job				joblist_prev_job;	//ジョブリスト操作中、直前まで操作していたジョブ内容のコピー

protected:
    bool	SetCopyModeList(void);
    bool	IsForeground();
    bool	IsSendingMail(bool is_prepare);
    bool	IsFirstJob();
    bool	IsLastJob();
    bool	SetupWindow();
    bool	event(QEvent *event);
    void	changeEvent(QEvent *event);
    void	timerEvent(QTimerEvent *timer_event);

    bool	ExecCopy(DWORD exec_flags);
    bool	ExecCopyCore(void);
    bool	EndCopy(void);
    bool	ExecFinalAction();
    bool	CancelCopy(void);

    void	SetItemEnable(bool is_delete);
    void	SetItemEnable_Verify(bool verify_before_update=true);
    void	SetExtendFilter(bool enable);
    void	ReflectFilterCheck(bool enabled);
    bool	SwapTarget(bool check_only=FALSE);
    bool	SwapTargetCore(const void *s, const void *d, void *out_s, void *out_d);
    void	UpdateMenu();
    void	UpdateJobList(bool req_current = false);
    void	EnableJobList(bool required_unlock);
    void	EnableJobMenus(bool isEnable);
    bool	CheckJobListmodified();
    void	SetJob(int idx,bool use_joblistwk=false,bool cancel_signal=false);
    bool	IsListing() { return (info.flags & FastCopy::LISTING_ONLY) ? true : FALSE; }
    bool	IsVerifyListing() { return (info.flags & FastCopy::LISTING_ONLY_VERIFY) ? true : false; }
    void	SetPriority(QThread::Priority new_class);
    int		UpdateSpeedLevel(BOOL is_timer=FALSE);
    void	SetListInfo();
    void	SetFileLogInfo();
    void	WriteStatInfo();
    bool	SetTaskTrayInfo(bool is_finish_status, double doneRate, int remain_h, int remain_m,
            int remain_s);
    bool	CalcInfo(double *doneRate, int *remain_sec, int *total_sec);
    void	EditPathLog(QString *buf,void* src,void * dst,bool is_delete_mode,char* inc,char* exc,int idx,bool csv_req);
    void	WriteLogHeader(int hFile, bool add_filelog=true,bool csv_title=false);
    bool	WriteLogFooter(int hFile,bool csv_title=false);
    bool	WriteErrLog(bool is_initerr=FALSE,int message_no=0);
    void	WriteMailLog();
    bool	StartFileLog();
    void	EndFileLog();
    bool	StartStatLog();
    void	EndStatLog();
    bool	InitEv();
    void	SetJobListmodifiedSignal(bool enable);
    bool	EvCreate();

    int		SetSpeedLevelLabel(int level);
    void	Show(int mode=SW_NORMAL);
    void	castPath(QPlainTextEdit *target,QString append_path,int append_path_count,bool replace_req);
    void	LaunchNextJob();
    bool	isJobListwithSilent();
    bool	UpdateJobListtoini();
    bool	DeleteJobListtoini(QString del_joblistname);

    void	dragEnterEvent(QDragEnterEvent *event);
    void	dropEvent(QDropEvent *event);
    bool	eventFilter(QObject *target, QEvent *event);
    void	closeEvent(QCloseEvent *event);

private slots:

    void jobListjob_Change_Detected();
    void on_actionUserDir_U_triggered();
    void on_actionOpenLog_triggered();
    void on_actionHelp_triggered();
    void on_actionSwap_Source_DestDir_triggered();
    void on_actionShow_Extended_Filter_toggled(bool arg1);
    void on_actionMain_Settings_triggered();
    void on_actionAuto_HDD_mode_triggered(bool checked);
    void on_actionSame_HDD_mode_triggered(bool checked);
    void on_actionDiff_HDD_mode_triggered(bool checked);
    void on_actionWindowPos_Fix_triggered(bool checked);
    void on_actionWindowSize_Fix_triggered(bool checked);
    void on_actionAbout_triggered();
    void on_actionRun_RapidCopy_triggered();
    void on_speed_Slider_valueChanged(int value);
    void actpostprocess_triggered(QAction *_action);
    void actpostprocessdlg_triggered();
    void job_triggered(QAction *_action);
    void hist_triggered(QAction *_action);
    void on_actionShow_SingleJob_triggered();
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void on_checkBox_Verify_clicked(bool checked);
    void on_Latest_URL_triggered();
    void on_actionSave_Report_at_Desktop_triggered();
    void process_stdout_output();
    void process_exit();
    void on_plainTextEdit_Src_textChanged();
    void on_actionOpenDetailLogDir_triggered();
    void on_actionQuit_triggered();
    void on_actionClear_Source_and_DestDir_triggered();
    void on_plainTextEdit_Src_blockCountChanged(int newBlockCount);
    void on_plainTextEdit_Dst_textChanged();
    void on_actionClose_Window_triggered();
    void on_comboBox_Mode_currentIndexChanged(int index);
    void on_pushButton_JobDelete_clicked();
    void on_pushButton_JobDeleteAll_clicked();
    void on_pushButton_JobListSave_clicked();
    void on_pushButton_JobListDelete_clicked();
    void on_pushButton_JobListRename_clicked();
    void on_comboBox_JobListName_activated(int index);
    void on_comboBox_JobListName_editTextChanged(const QString &arg1);
    void on_checkBox_JobListForceLaunch_clicked(bool checked);
    void on_listWidget_JobList_itemChanged(QListWidgetItem *item);
    void listWidget_JobList_rowsMoved(QModelIndex parent,int start,int end,QModelIndex dest,int row);
    void on_actionEnable_JobList_Mode_toggled(bool arg1);
    void on_listWidget_JobList_clicked(const QModelIndex &index);
    void on_checkBox_LTFS_stateChanged(int arg1);

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
