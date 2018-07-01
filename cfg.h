/* static char *cfg_id =
    "@(#)Copyright (C) 2005-2012 H.Shirouzu		cfg.h	Ver2.10"; */
/* ========================================================================
    Project  Name			: Fast/Force copy file and directory
    Create					: 2005-01-23(Sun)
    Update					: 2012-06-17(Sun)
    port update	            : 2016-02-25
    Copyright				: H.Shirouzu,Kengo Sawatsu
    Reference				:
    ======================================================================== */

#ifndef CFG_H
#define CFG_H
#include "resource.h"
#include "osl.h"
#include "qblowfish.h"
#include <QSettings>
#include <QPoint>
#include <QSize>
#include <QDate>

//メール送信機能でメールアカウントのパスワードを扱うため、簡易暗号化。
//悪意のある第三者にとりあえず平文で読まれない程度でしかないけど。
#define SHARE_KEY_A_BASE64 "U3Bmbm1qaW1fdEh1NXVlUnJDdC0="		//decode = Spfnmjim_tHu5ueRrCt-
#define SHARE_KEY_B "WHXYCaGFXEUTHHhhzScrbF"                    //no decode
#define SHARE_KEY_C_BASE64 "RDlnTUtzYzZtV2d6UzQ="               //decode = D9gMKsc6mWgzS4
                                                                //complete blowfish key string = "Spfnmjim_tHu5ueRrCt-WHXYCaGFXEUTHHhhzScrbFD9gMKsc6mWgzS4"
#define LICENSE_FILE_JP ":/license/license/License_JP.txt"
#define LICENSE_FILE_EN ":/license/license/License_EN.txt"

#define JOBLIST_JOBMAX 256	//一つのジョブリストにぶら下げることのできるジョブ数MAX

//ジョブリスト構造体
struct JobList{
    void* joblist_title;				//ジョブリスト名
    void* jobname_pt[JOBLIST_JOBMAX];	//ジョブリスト中のジョブ名称配列
    unsigned short joblistentry;		//ジョブリスト中のジョブエントリ数
    bool		   forcelaunch;			//致命的エラーでジョブリストストップしないフラグ
                                        //true = 止めない(次のジョブを起動する)
                                        //false = 止める(ジョブリスト実行を止める)
    void Init() {
        memset(this, 0, sizeof(JobList));
        forcelaunch = true;				//デフォルトでは止めない
    }
    void SetString(void *_title){
        joblist_title = strdupV(_title);
    }
    void AddJobtoJobList(void *job_title){
        jobname_pt[joblistentry] = strdupV(job_title);
        joblistentry++;
    }
    void Set(const JobList *joblist){
        memcpy(this, joblist, sizeof(JobList));
        SetString(joblist->joblist_title);
        for(int i=0; i<joblistentry; i++){
            jobname_pt[i] = strdupV(jobname_pt[i]);
        }
    }
    void UnSet() {
        free(joblist_title);
        for(int i=0; i<joblistentry; i++){
            free(jobname_pt[i]);
        }
        joblistentry = 0;
        Init();
    }
    JobList() {
        Init();
    }
    JobList(const JobList& joblist) {
        Init();
        Set(&joblist);
    }
    ~JobList(){UnSet();}
    /*
    void debug(){
        printf("joblist_title = %s\n",joblist_title);
        for(int i=0; i<joblistentry; i++){
            printf("joblist_job%d = %s\n",i,jobname_pt[i]);
            printf("joblist_job%d pt= %lx\n",i,jobname_pt[i]);
        }
        fflush(stdout);
    }
    */
};

struct Job {

    void	*title;
    void	*src;
    void	*dst;
    void	*cmd;
    int		bufSize;
    int		estimateMode;
    int		diskMode;
    int		ignoreErr;
    int		usingMD5;
    BOOL	enableOwdel;
    BOOL	enableAcl;
    BOOL	enableXattr;
    BOOL	enableVerify;
    BOOL	Dotignore_mode;
    BOOL	LTFS_mode;
    BOOL	isFilter;
    BOOL	isListing;
    void	*includeFilter;
    void	*excludeFilter;
    void	*fromDateFilter;
    void	*toDateFilter;
    void	*minSizeFilter;
    void	*maxSizeFilter;
    int		flags;

    enum	{SINGLE_JOB=0x1,JOBLIST_JOB=0x2};
    void Init() {
        memset(this, 0, sizeof(Job));
    }
    void SetString(void *_title, void *_src, void *_dst, void *_cmd,
                    void *_includeFilter, void *_excludeFilter,
                    void *_fromDateFilter, void *_toDateFilter,
                    void *_minSizeFilter, void *_maxSizeFilter) {
        title = strdupV(_title);
        src = strdupV(_src);
        dst = strdupV(_dst);
        cmd = strdupV(_cmd);
        includeFilter = strdupV(_includeFilter);
        excludeFilter = strdupV(_excludeFilter);
        fromDateFilter = strdupV(_fromDateFilter);
        toDateFilter = strdupV(_toDateFilter);
        minSizeFilter = strdupV(_minSizeFilter);
        maxSizeFilter = strdupV(_maxSizeFilter);
    }
    //他Jobとジョブ内容を比較する
    //note:ジョブ名(タイトル)は比較しない
    bool CompareJob(Job *compare_wkjob){
        //titleとflagsはチェックしない
        if(strcmp((char*)src,(char*)compare_wkjob->src) != 0 ||
           strcmp((char*)dst,(char*)compare_wkjob->dst) != 0 ||
           strcmp((char*)cmd,(char*)compare_wkjob->cmd) != 0 ||
           bufSize != compare_wkjob->bufSize ||
           estimateMode != compare_wkjob->estimateMode ||
           diskMode != compare_wkjob->diskMode ||
           ignoreErr != compare_wkjob->ignoreErr ||
           usingMD5 != compare_wkjob->usingMD5 ||
           enableOwdel != compare_wkjob->enableOwdel ||
           enableAcl != compare_wkjob->enableAcl ||
           enableXattr != compare_wkjob->enableXattr ||
           enableVerify != compare_wkjob->enableVerify ||
           Dotignore_mode != compare_wkjob->Dotignore_mode ||
           LTFS_mode != compare_wkjob->LTFS_mode ||
           isListing != compare_wkjob->isListing){
           //フィルター内容以外のなにがしかが違う！
           return false;
        }
        //フィルタ内容チェック
        //比較対象両方ともフィルタ使用
        if(isFilter && compare_wkjob->isFilter){
            //内容をそれぞれ比較
            if(strcmp((char*)includeFilter,(char*)compare_wkjob->includeFilter) != 0 ||
               strcmp((char*)excludeFilter,(char*)compare_wkjob->excludeFilter) != 0 ||
               strcmp((char*)fromDateFilter,(char*)compare_wkjob->fromDateFilter) != 0 ||
               strcmp((char*)toDateFilter,(char*)compare_wkjob->toDateFilter) != 0 ||
               strcmp((char*)minSizeFilter,(char*)compare_wkjob->minSizeFilter) != 0 ||
               strcmp((char*)maxSizeFilter,(char*)compare_wkjob->maxSizeFilter) != 0){
                //各種フィルタ指定の内容のなんらかしらが違うだべよ。
                return false;
            }
        }
        //なんにも引っかからなかったので同一だよ
        return true;
    }

    void Set(const Job *job) {
        memcpy(this, job, sizeof(Job));
        SetString(job->title, job->src, job->dst, job->cmd,
                    job->includeFilter, job->excludeFilter,
                    job->fromDateFilter, job->toDateFilter,
                    job->minSizeFilter, job->maxSizeFilter);
    }
    void UnSet() {
        //free(jobGroup);
        free(excludeFilter);
        free(includeFilter);
        free(fromDateFilter);
        free(toDateFilter);
        free(minSizeFilter);
        free(maxSizeFilter);
        free(cmd);
        free(dst);
        free(src);
        free(title);
        Init();
    }

    Job() {
        Init();
    }
    Job(const Job& job) {
        Init();
        Set(&job);
    }
    ~Job() { UnSet(); }

    /*
    void debug(){
        printf("jobaddr = %x",this);
        printf("title = %s\n",title);
        printf("src = %s\n",src);
        printf("dst = %s\n",dst);
        printf("cmd = %s\n",cmd);
        printf("bufSize = %d\n",bufSize);
        printf("estimateMode = %d\n",estimateMode);
        printf("diskMode = %d\n",diskMode);
        printf("ignoreErr = %d\n",ignoreErr);
        printf("usingMD5 = %d\n",usingMD5);
        printf("enableOwdel = %d\n",enableOwdel ? 1 : 0);
        printf("enableAcl = %d\n",enableAcl ? 1 : 0);
        printf("enableXattr = %d\n",enableXattr ? 1 : 0);
        printf("enableVerify = %d\n",enableVerify ? 1 : 0);
        printf("Dotignore_mode = %d\n",Dotignore_mode ? 1 : 0);
        printf("LTFS_mode = %d\n",LTFS_mode ? 1 : 0);
        if(isFilter){
            printf("includeFilter = %s\n",includeFilter);
            printf("excludeFilter = %s\n",excludeFilter);
            printf("fromDateFilter = %s\n",fromDateFilter);
            printf("toDateFilter = %s\n",toDateFilter);
            printf("minSizeFilter = %s\n",minSizeFilter);
            printf("maxSizeFilter = %s\n",maxSizeFilter);
        }
        printf("flags = %x\n",flags);
        fflush(stdout);
    }
    */
};

struct FinAct {
    void	*title;
    void	*sound;
    void	*command;
    //以下はメール送信機能用に追加
    void	*account;
    void	*password;
    void	*smtpserver;
    void	*port;
    void	*tomailaddr;
    void	*frommailaddr;
    QStringList *mailsets;

    int		shutdownTime;    //自動シャットダウン用の時間は未実装

    int		flags;
    enum {	BUILTIN=0x1, ERR_SOUND=0x2, ERR_CMD=0x4, ERR_SHUTDOWN=0x8, WAIT_CMD=0x10,
            FORCE=0x20, SUSPEND=0x40, HIBERNATE=0x80, SHUTDOWN=0x100, NORMAL_CMD=0x200,
            NORMAL_MAIL=0x1000,ERR_MAIL=0x2000};
    void Init() {
        memset(this, 0, sizeof(FinAct));
        shutdownTime = -1;
    }
    void SetString(void *_title, void *_sound, void *_command,
                   void *_account,void *_password,void *_smtpserver,void *_port,void *_tomailaddr,void *_frommailaddr){
        title = strdupV(_title);
        sound = strdupV(_sound);
        command = strdupV(_command);
        account = strdupV(_account);
        password = strdupV(_password);
        smtpserver = strdupV(_smtpserver);
        port = strdupV(_port);
        tomailaddr = strdupV(_tomailaddr);
        frommailaddr = strdupV(_frommailaddr);
    }
    void Set(const FinAct *finAct) {
        memcpy(this, finAct, sizeof(FinAct));
        SetString(finAct->title, finAct->sound, finAct->command,
                  finAct->account,finAct->password,finAct->smtpserver,finAct->port,finAct->tomailaddr,finAct->frommailaddr);
    }
    void UnSet() {
        free(command);
        free(sound);
        free(title);
        free(account);
        free(password);
        free(smtpserver);
        free(port);
        free(tomailaddr);
        free(frommailaddr);
        Init();
    }
    FinAct() {
        Init();
    }
    FinAct(const FinAct& action) {
        Init();
        Set(&action);
    }
    ~FinAct() { UnSet(); }
};

class Cfg {
protected:
    BOOL Init(void *user_dir, void *virtual_dir);
    BOOL IniStrToV(char *inipath, void *path);
    BOOL VtoIniStr(void *path, char *inipath);
    BOOL EntryHistory(void **path_array, void ****history_array, int max);

public:
    enum FileLogMode{NO_FILELOG			= 0x00000000,	//FILELOG出力なし
                     AUTO_FILELOG		= 0x00000001,	//カレント時刻名でFILELOG出力
                     FIX_FILELOG		= 0x00000002,	//CLI延長の固定ファイル名でFILELOG出力
                     ADD_CSVFILELOG	= 0x00000010};		//CSVファイル出力要求

    int		bufSize;
    int		maxTransSize;
    int		maxAionum;
    //bool	enableReadahead;
    int		maxOpenFiles;
    int		maxAttrSize;
    int		maxDirSize;
    int		maxHistory;
    int		maxHistoryNext;
    int		copyMode;
    int		copyFlags;
    int		skipEmptyDir;	// 0:no, 1:filter-mode only, 2:always
    int		forceStart;		// 0:delete only, 1:always(copy+delete), 2:always wait
    bool	enableLTFS;
    int		ignoreErr;
    int		estimateMode;
    int		diskMode;
    int		lcid;
    int		waitTick;
    int		speedLevel;
    BOOL	isAutoSlowIo;
    BOOL	alwaysLowIo;
    BOOL	enableOwdel;
    BOOL	enableAcl;
    BOOL	enableXattr;
    BOOL	enableVerify;
    BOOL    enableVerifyErrDel;
    BOOL	Dotignore_mode;
    int		usingMD5;
    BOOL	enableNSA;
    BOOL	delDirWithFilter;
    BOOL	enableMoveAttr;
    BOOL	serialMove;
    BOOL	serialVerifyMove;
    BOOL	isDisableutime;
    BOOL	isReparse;
    int		isLinkDest;
    int		maxLinkHash;
    //_int64	allowContFsize;
    BOOL	isReCreate;
    BOOL	isExtendFilter;
    bool	isJobListMode;
    int		taskbarMode; // 0: use tray, 1: use taskbar
    int		infoSpan;	// information update timing (0:250msec, 1:500msec, 2:1000msec)
    BOOL	isTopLevel;
    BOOL	isErrLog;
    BOOL    enableOdirect;
    int		fileLogMode;

    BOOL	aclErrLog;
    BOOL	streamErrLog;
    BOOL	isRunasButton;
    BOOL	isSameDirRename;
    BOOL	execConfirm;
    void	**srcPathHistory;
    void	**dstPathHistory;
    void	**delPathHistory;
    void	**includeHistory;
    void	**excludeHistory;
    void	**fromDateHistory;
    void	**toDateHistory;
    void	**minSizeHistory;
    void	**maxSizeHistory;
    void	*execPathV;
    void	*execDirV;

    QString	userDirV;
    QString statDirV;
    QString soundDirV;

    void	*errLogPathV;
    QString	fileLogDirV;
    Job		**jobArray;
    int		jobMax;

    JobList	**joblistArray;
    int		joblistMax;
    FinAct	**finActArray;
    int		finActMax;
    QPoint	macpos;
    QSize	macsize;
    bool	rwstat;
    bool	async;
    QByteArray share_key;		//秘密鍵をがっちゃんこする領域
    QBlowfish  *bf;

    Cfg();
    ~Cfg();
    BOOL ReadIni(void *user_dir, void *virtual_dir);
    BOOL PostReadIni(void);
    BOOL WriteIni(void);
    BOOL EntryPathHistory(void *src, void *dst);
    BOOL EntryDelPathHistory(void *del);
    BOOL EntryFilterHistory(void *inc, void *exc, void *from, void *to, void *min, void *max);

    int SearchJobV(void *title);
    BOOL AddJobV(const Job *job);
    BOOL DelJobV(void *title);

    int SearchJobList(void *title);
    int SearchJobnuminAllJobList(void* job_title);
    bool AddJobList(const JobList *joblist);
    int  AddJobtoJobList(int joblistindex,Job *job);
    bool DelJobList(void* title);
    int DelJobinAllJobList(void* job_title);
    int DelJobinJobList(void *job_title,void*joblist_title);

    int SearchFinActV(void *title, BOOL cmd_line=FALSE);
    BOOL AddFinActV(const FinAct *job);
    BOOL DelFinActV(void *title);

    QByteArray decryptData(QByteArray encyptdata);
    QByteArray encryptData(QByteArray decryptdata);
};

#define INVALID_POINTVAL	-10000
#define INVALID_SIZEVAL		-10000

#define IS_INVALID_POINT(ptx,pty) (ptx == INVALID_POINTVAL && pty == INVALID_POINTVAL)
#define IS_INVALID_SIZE(szx,szy) (szx == INVALID_SIZEVAL  && szy == INVALID_SIZEVAL)


#endif
