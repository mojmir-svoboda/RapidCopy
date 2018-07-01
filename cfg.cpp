static char *cfg_id =
    "@(#)Copyright (C) 2004-2012 H.Shirouzu		cfg.cpp	ver2.10";
/* ========================================================================
    Project  Name		: Fast/Force copy file and directory
    Create				: 2004-09-15(Wed)
    Update				: 2012-06-17(Sun)
    port update         : 2016-02-28
    Copyright			: H.Shirouzu, Kengo Sawatsu
    Reference			:
    ======================================================================== */

#include "mainwindow.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/stat.h>
#include <errno.h>
#include <QCoreApplication>
#include <QDir>
#include <QtCore>
#include <QIODevice>
#include <qcoreapplication.h>
#include <qstandardpaths.h>
#include "osl.h"
#include "tmisc.h"
#include "tlib.h"
#include "wchar.h"
#include "unistd.h"
#include "cfg.h"
#include "qblowfish.h"

#define FASTCOPY_INI			"RapidCopy.ini"
#define FASTCOPY_PLIST			"RapidCopy.plist"
#define MAIN_SECTION			"main"
#define SRC_HISTORY				"src_history"
#define DST_HISTORY				"dst_history"
#define DEL_HISTORY				"del_history"
#define INC_HISTORY				"include_history"
#define EXC_HISTORY				"exclude_history"
#define FROMDATE_HISTORY		"fromdate_history"
#define TODATE_HISTORY			"todate_history"
#define MINSIZE_HISTORY			"minsize_history"
#define MAXSIZE_HISTORY			"maxsize_history"

#define MAX_HISTORY_KEY			"max_history"
#define COPYMODE_KEY			"default_copy_mode"
#define COPYFLAGS_KEY			"default_copy_flags"
#define SKIPEMPTYDIR_KEY		"skip_empty_dir"
#define FORCESTART_KEY			"force_start"
#define IGNORE_ERR_KEY			"ignore_error"
#define ESTIMATE_KEY			"estimate_mode"
#define DISKMODE_KEY			"disk_mode"
#define ISTOPLEVEL_KEY			"is_toplevel"
#define ISERRLOG_KEY			"is_errlog"
#define ISUTF8LOG_KEY			"is_utf8log"
#define FILELOGMODE_KEY			"filelog_mode"
#define ACLERRLOG_KEY			"aclerr_log"
#define STREAMERRLOG_KEY		"streamerr_log"
#define ISRUNASBUTTON_KEY		"is_runas_button"
#define ISSAMEDIRRENAME_KEY		"is_samedir_rename"
#define BUFSIZE_KEY				"bufsize"
#define MAXTRANSSIZE_KEY		"max_transize"
#define MAXAIONUM_KEY			"max_aionum"
#define MAXOPENFILES_KEY		"max_openfiles"
#define MAXATTRSIZE_KEY			"max_attrsize"
#define MAXDIRSIZE_KEY			"max_dirsize"
#define SHEXTAUTOCLOSE_KEY		"shext_autoclose"
#define SHEXTTASKTRAY_KEY		"shext_tasktray"
#define SHEXTNOCONFIRM_KEY		"shext_dd_noconfirm"
#define SHEXTNOCONFIRMDEL_KEY	"shext_right_noconfirm"
#define EXECCONRIM_KEY			"exec_confirm"
#define FORCESTART_KEY			"force_start"
#define LCID_KEY				"lcid"
#define LOGFILE_KEY				"logfile"
#define WAITTICK_KEY			"wait_tick"
#define ISAUTOSLOWIO_KEY		"is_autoslow_io2"
#define SPEEDLEVEL_KEY			"speed_level"
#define ALWAYSLOWIO_KEY			"alwaysLowIo"
#define DRIVEMAP_KEY			"driveMap"
#define OWDEL_KEY				"overwrite_del"
#define ACL_KEY					"acl"
#define STREAM_KEY				"stream"
#define XATTR_KEY				"xattr"
#define VERIFY_KEY				"verify"
#define VERIFY_ERRDEL_KEY		"verifyerrdel"
#define IGNORE_DOT_KEY			"dotignore"
#define USEMD5_KEY				"using_md5"
#define ISLISTING_KEY			"isListing"
#define JOBGROUP_KEY			"jobGroup"
#define NSA_KEY					"nsa_del"
#define DELDIR_KEY				"deldir_with_filter"
#define MOVEATTR_KEY			"move_attr"
#define SERIALMOVE_KEY			"serial_move"
#define SERIALVERIFYMOVE_KEY	"serial_verify_move"
#define REPARSE_KEY				"reparse2"
#define LINKDEST_KEY			"linkdest"
#define MAXLINKHASH_KEY			"max_linkhash"
//#define ALLOWCONTFSIZE_KEY	"allow_cont_fsize"
#define RECREATE_KEY			"recreate"
#define EXTENDFILTER_KEY		"extend_filter"
#define WINPOS_KEY				"win_pos"
#define TASKBARMODE_KEY			"taskbarMode"
#define INFOSPAN_KEY			"infoSpan"
#define MACPOS_KEY				"macwinpos"
#define MACSIZE_KEY				"macwinsize"

#define NONBUFMINSIZENTFS_KEY	"nonbuf_minsize_ntfs2"
#define NONBUFMINSIZEFAT_KEY	"nonbuf_minsize_fat"
#define ISREADOSBUF_KEY			"is_readosbuf"

#define FMT_JOB_KEY				"job_%d"
#define FMT_JOBLIST_KEY			"joblist_%d"
#define FMT_JOBLIST_TITLEKEY	"joblist_job_%d"
#define TITLE_KEY				"title"
#define CMD_KEY					"cmd"
#define ACCOUNT_KEY				"e-account"
#define PASSWORD_KEY			"e-password"
#define SMTPSERVER_KEY			"e-smtpserver"
#define PORT_KEY				"e-port"
#define TOMAILADDR_KEY			"e-toaddress"
#define FROMMAILADDR_KEY		"e-fromaddress"

#define SRC_KEY					"src"
#define DST_KEY					"dst"
#define FILTER_KEY				"filter"
#define INCLUDE_KEY				"include_filter"
#define EXCLUDE_KEY				"exclude_filter"
#define FROMDATE_KEY			"fromdate_filter"
#define TODATE_KEY				"todate_filter"
#define MINSIZE_KEY				"minsize_filter"
#define MAXSIZE_KEY				"maxsize_filter"
#define FMT_FINACT_KEY			"finaction_%d"
#define SOUND_KEY				"sound"
#define SHUTDOWNTIME_KEY		"shutdown_time"
#define FLAGS_KEY				"flags"
#define LTFS_KEY				"ltfs"
#define F_ODIRECT_KEY			"odirect"
#define RWSTAT_KEY				"rwstat"
#define ASYNCIO_KEY				"asyncio"
#define DATA_KEY				"data"
#define LATEST_URL				"latest_dlurl"
#define NOTE_VUP_KEY			"notify_vup"
#define DISABLE_UTIME_KEY		"disable_utime"
#define READAHEAD_KEY			"readahead"
#define JOBLISTMODE_KEY			"isJobListMode"
#define FORCELAUNCH_KEY			"forcelaunch"

#define DEFAULT_MAX_HISTORY		10
#define DEFAULT_COPYMODE		1
#define DEFAULT_COPYFLAGS		0
#define DEFAULT_EMPTYDIR		1
#define DEFAULT_FORCESTART		0
#define DEFAULT_BUFSIZE			256
#define DEFAULT_MAXTRANSSIZE	1
#define DEFAULT_MAXAIONUM		1
#define DEFAULT_MAXATTRSIZE		(1024 * 1024 * 1024)
#define DEFAULT_MAXDIRSIZE		(1024 * 1024 * 1024)
#define DEFAULT_MAXOPENFILES	256
#define DEFAULT_LINKHASH		300000
//#define DEFAULT_ALLOWCONTFSIZE	(1024 * 1024 * 1024)
#define DEFAULT_WAITTICK		10
#define JOB_MAX					1000
#define FINACT_MAX				1000
#define JOBLIST_MAX				256
#define FINACT_PORT_DEFAULT		"465"			//SMTP Over SSL
#define DEFAULT_FASTCOPYLOG		"RapidCopy.log"
#define DEFAULT_INFOSPAN		2
#define LTFS_MAXTRANSSIZE		1 //LTFSモード時のMAXTRANSSIZE

/*=========================================================================
  クラス ： Cfg
  概  要 ： コンフィグクラス
  説  明 ：
  注  意 ：
=========================================================================*/
Cfg::Cfg()
{
}

Cfg::~Cfg()
{

    userDirV.clear();
    free(errLogPathV);
    free(execDirV);
    free(execPathV);
    delete bf;
}

BOOL Cfg::Init(void *user_dir, void *virtual_dir)
{
    char	buf[MAX_PATH], path[MAX_PATH];

    strncpy(buf,
            QCoreApplication::applicationFilePath().toStdString().data(),
            QCoreApplication::applicationFilePath().toStdString().length() + STR_NULL_GUARD);

    // シンボリックリンクとかだと騙されるんでフルパス化
    if(realpath((const char*)buf,(char*)path) == NULL){
        return false;
    }

    execPathV = strdupV(path);
    execDirV = strdupV(QCoreApplication::applicationDirPath().toStdString().data());
    errLogPathV = NULL;
    userDirV.clear();

    userDirV = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + FASTCOPY;
    fileLogDirV = userDirV + "/" + (char*)GetLoadStrV(IDS_FILELOG_SUBDIR);

    QDir userdir(userDirV);

    // DocumentディレクトリにRapidCopyディレクトリなかったら作成
    if(!userdir.exists()){
        //ないので作っちゃえ
        userdir.mkpath(userDirV);
    }

    statDirV = userDirV + FASTCOPY_STATDIR;
    soundDirV = userDirV + FASTCOPY_SOUNDDIR;

    //RapidCopy独自SEフォルダの作成
    QDir sounddir(soundDirV);
    if(!sounddir.exists()){
        //ないので作っちゃえ
        sounddir.mkpath(soundDirV);
    }
    //RapidCopy独自SEフォルダ内にサウンドファイルをコピー
    for(int i=1;i <= SOUND_RESOURCE_NUM; i++){
        char buf[MAX_PATH];
        sprintf(buf,"%s%02d%s",SOUND_RESOURCE_NAMEBASE,i,".wav");
        QString sefile(buf);
        QFile	localsefile(sefile);

        sprintf(buf,"%s%s%02d%s",soundDirV.toLocal8Bit().data(),LOCAL_SOUNDFILENAME_NAMEBASE,i,".wav");
        QFile	existslocalsefile(buf);
        if(!existslocalsefile.exists()){
            localsefile.copy(QFileInfo(existslocalsefile).absoluteFilePath());
        }
    }

    QDir filelogdir(fileLogDirV);
    if(!filelogdir.exists()){
        //ないので作っちゃえ
        //rapidcopy開始時のログ機能延長とやってること被ってるけどまあいっか
        filelogdir.mkpath(fileLogDirV);
    }

    //AとCはデコードするけど、なんとなくBは生のままにしておく。気休め。
    share_key.append(QByteArray::fromBase64(SHARE_KEY_A_BASE64));
    share_key.append(SHARE_KEY_B);
    share_key.append(QByteArray::fromBase64(SHARE_KEY_C_BASE64));
    bf = new QBlowfish(share_key);
    //bf->setPaddingEnabled(true);
    return	TRUE;
}

BOOL Cfg::ReadIni(void *user_dir, void *virtual_dir)
{
    if (!Init(user_dir, virtual_dir)) return FALSE;

    int		i, j;
    char	key[100], *p;
    char	*buf = new char [MAX_HISTORY_CHAR_BUF];

    char	*section_array[] = {	SRC_HISTORY, DST_HISTORY, DEL_HISTORY,
                                    INC_HISTORY, EXC_HISTORY,
                                    FROMDATE_HISTORY, TODATE_HISTORY,
                                    MINSIZE_HISTORY, MAXSIZE_HISTORY };
    void	***history_array[] = {	&srcPathHistory, &dstPathHistory, &delPathHistory,
                                    &includeHistory, &excludeHistory,
                                    &fromDateHistory, &toDateHistory,
                                    &minSizeHistory, &maxSizeHistory };

    srcPathHistory	= NULL;
    dstPathHistory	= NULL;
    delPathHistory	= NULL;
    includeHistory	= NULL;
    excludeHistory	= NULL;

    jobArray = NULL;
    jobMax = 0;

    joblistArray = NULL;
    joblistMax = 0;

    finActArray = NULL;
    finActMax = 0;

    QSettings ini(userDirV + "/" + FASTCOPY_INI,QSettings::IniFormat);

    ini.beginGroup(MAIN_SECTION);

    bufSize			= ini.value(BUFSIZE_KEY,DEFAULT_BUFSIZE).toInt();
    maxTransSize	= ini.value(MAXTRANSSIZE_KEY, DEFAULT_MAXTRANSSIZE).toInt();
    maxAionum		= ini.value(MAXAIONUM_KEY, DEFAULT_MAXAIONUM).toInt();
    //enableReadahead = ini.value(READAHEAD_KEY,FALSE).toBool();

    maxOpenFiles	= ini.value(MAXOPENFILES_KEY, DEFAULT_MAXOPENFILES).toInt();
    maxAttrSize		= ini.value(MAXATTRSIZE_KEY, DEFAULT_MAXATTRSIZE).toInt();
    maxDirSize		= ini.value(MAXDIRSIZE_KEY, DEFAULT_MAXDIRSIZE).toInt();
    maxHistoryNext	= maxHistory = ini.value(MAX_HISTORY_KEY, DEFAULT_MAX_HISTORY).toInt();
    copyMode		= ini.value(COPYMODE_KEY, DEFAULT_COPYMODE).toInt();
    copyFlags		= ini.value(COPYFLAGS_KEY, DEFAULT_COPYFLAGS).toInt();
    skipEmptyDir	= ini.value(SKIPEMPTYDIR_KEY, DEFAULT_EMPTYDIR).toInt();
    forceStart		= ini.value(FORCESTART_KEY, DEFAULT_FORCESTART).toInt();
    ignoreErr		= ini.value(IGNORE_ERR_KEY, 1).toInt();
    estimateMode	= ini.value(ESTIMATE_KEY, 1).toInt();
    diskMode		= ini.value(DISKMODE_KEY, 0).toInt();
    isTopLevel		= ini.value(ISTOPLEVEL_KEY, FALSE).toBool();
    isErrLog		= ini.value(ISERRLOG_KEY, FALSE).toBool();
    fileLogMode		= ini.value(FILELOGMODE_KEY,0).toInt();
    aclErrLog		= ini.value(ACLERRLOG_KEY,FALSE).toBool();
    streamErrLog	= ini.value(STREAMERRLOG_KEY,FALSE).toBool();
    isRunasButton	= ini.value(ISRUNASBUTTON_KEY, FALSE).toBool();
    isSameDirRename	= ini.value(ISSAMEDIRRENAME_KEY, TRUE).toBool();
    execConfirm		= ini.value(EXECCONRIM_KEY, FALSE).toBool();
    lcid			= ini.value(LCID_KEY,QLocale::system().language()).toInt();
    waitTick		= ini.value(WAITTICK_KEY, DEFAULT_WAITTICK).toInt();
    isAutoSlowIo	= ini.value(ISAUTOSLOWIO_KEY, TRUE).toBool();
    speedLevel		= ini.value(SPEEDLEVEL_KEY, SPEED_FULL).toInt();
    alwaysLowIo		= ini.value(ALWAYSLOWIO_KEY, FALSE).toBool();
    enableOwdel		= ini.value(OWDEL_KEY, FALSE).toBool();
    enableAcl		= ini.value(ACL_KEY, FALSE).toBool();
    enableXattr		= ini.value(XATTR_KEY, FALSE).toBool();
    enableVerify	= ini.value(VERIFY_KEY, false).toBool();
    enableVerifyErrDel = ini.value(VERIFY_ERRDEL_KEY, true).toBool();
    Dotignore_mode  = ini.value(IGNORE_DOT_KEY, true).toBool();
    enableLTFS		= ini.value(LTFS_KEY, false).toBool();
    enableOdirect   = ini.value(F_ODIRECT_KEY,true).toBool();
    usingMD5		= ini.value(USEMD5_KEY,1).toInt();
    enableNSA		= ini.value(NSA_KEY, FALSE).toBool();
    delDirWithFilter= ini.value(DELDIR_KEY, FALSE).toBool();
    enableMoveAttr	= ini.value(MOVEATTR_KEY, FALSE).toBool();
    serialMove		= ini.value(SERIALMOVE_KEY, FALSE).toBool();
    serialVerifyMove = ini.value(SERIALVERIFYMOVE_KEY, FALSE).toBool();
    isReparse		= ini.value(REPARSE_KEY, TRUE).toBool();
    isLinkDest		= ini.value(LINKDEST_KEY, 0).toInt();
    maxLinkHash		= ini.value(MAXLINKHASH_KEY, DEFAULT_LINKHASH).toInt();
    isReCreate		= ini.value(RECREATE_KEY, 0).toInt();;
    isExtendFilter	= ini.value(EXTENDFILTER_KEY, FALSE).toBool();
    taskbarMode		= ini.value(TASKBARMODE_KEY, 0).toInt();
    infoSpan		= ini.value(INFOSPAN_KEY, DEFAULT_INFOSPAN).toInt();
    if (infoSpan < 0 || infoSpan > 2) infoSpan = DEFAULT_INFOSPAN;
    macpos = ini.value(MACPOS_KEY,QPoint(INVALID_POINTVAL,INVALID_POINTVAL)).toPoint();
    macsize = ini.value(MACSIZE_KEY,QSize(INVALID_SIZEVAL,INVALID_SIZEVAL)).toSize();
    rwstat = ini.value(RWSTAT_KEY,false).toBool();
    async = ini.value(ASYNCIO_KEY,false).toBool();
    isDisableutime = ini.value(DISABLE_UTIME_KEY,false).toBool();
    isJobListMode = ini.value(JOBLISTMODE_KEY,false).toBool();

    ini.endGroup();

    errLogPathV = strdupV(ini.value(LOGFILE_KEY,
                                    (QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                      + "/"
                                      + FASTCOPY
                                      + "/"
                                      + DEFAULT_FASTCOPYLOG)).toByteArray().data()
                                    );

    // History
    for (i=0; i < sizeof(section_array) / sizeof(char *); i++) {
        char	*&section = section_array[i];
        void	**&history = *history_array[i];

        ini.beginGroup(section);
        history = (void **)calloc(maxHistory, sizeof(char *));
        for (j=0; j < maxHistory + 30; j++){
            sprintf(key, "%d", j);
            if (j < maxHistory){
                history[j] = strdupV(ini.value(key,"").toString().toLocal8Bit().data());
            }
            // 元々key持ってなかったらスルー
            else if(!ini.contains(key));
            else
                break;
        }
        ini.endGroup();
    }

//Job

    for (i=0; i < JOB_MAX; i++) {
        Job	job;

        sprintf(buf, FMT_JOB_KEY, i);
        ini.beginGroup(buf);

        if(ini.value(TITLE_KEY,"").toString().isEmpty()){
            ini.endGroup();
            break;
        }
        job.title = strdupV(ini.value(TITLE_KEY,"").toString().toLocal8Bit().data());
        job.src = strdupV(ini.value(SRC_KEY,"").toString().toLocal8Bit().data());
        job.dst = strdupV(ini.value(DST_KEY,"").toString().toLocal8Bit().data());
        job.cmd = strdupV(ini.value(CMD_KEY,"").toString().toLocal8Bit().data());
        job.includeFilter = strdupV(ini.value(INCLUDE_KEY,"").toString().toLocal8Bit().data());
        job.excludeFilter = strdupV(ini.value(EXCLUDE_KEY,"").toString().toLocal8Bit().data());
        job.fromDateFilter = strdupV(ini.value(FROMDATE_KEY,"").toString().toLocal8Bit().data());
        job.toDateFilter = strdupV(ini.value(TODATE_KEY,"").toString().toLocal8Bit().data());
        job.minSizeFilter = strdupV(ini.value(MINSIZE_KEY,"").toString().toLocal8Bit().data());
        job.maxSizeFilter = strdupV(ini.value(MAXSIZE_KEY,"").toString().toLocal8Bit().data());
        job.estimateMode = ini.value(ESTIMATE_KEY, 1).toInt();
        job.diskMode = ini.value(DISKMODE_KEY, 0).toInt();
        job.ignoreErr = ini.value(IGNORE_ERR_KEY, 1).toInt();
        job.enableOwdel = ini.value(OWDEL_KEY, FALSE).toBool();
        job.enableAcl = ini.value(ACL_KEY, FALSE).toBool();
        job.enableXattr = ini.value(XATTR_KEY, TRUE).toBool();
        job.enableVerify = ini.value(VERIFY_KEY, TRUE).toBool();
        job.isFilter = ini.value(FILTER_KEY,FALSE).toBool();
        job.bufSize = ini.value(BUFSIZE_KEY,DEFAULT_BUFSIZE).toInt();
        job.Dotignore_mode = ini.value(IGNORE_DOT_KEY,TRUE).toBool();
        job.LTFS_mode = ini.value(LTFS_KEY, false).toBool();
        //job.enableF_NOCACHE = ini.value(F_NOCACHE_KEY,false).toBool();
        job.usingMD5 = ini.value(USEMD5_KEY,1).toInt();
        job.isListing = ini.value(ISLISTING_KEY,false).toBool();			//JobList機能専用
        job.flags = ini.value(FLAGS_KEY,(int)Job::SINGLE_JOB).toInt();			//joblistに関係ないジョブ
        AddJobV(&job);
        ini.endGroup();
    }

    //JobList
    for (i=0; i < JOBLIST_MAX; i++) {
        JobList	joblist;

        sprintf(buf, FMT_JOBLIST_KEY, i);
        ini.beginGroup(buf);

        if(ini.value(TITLE_KEY,"").toString().isEmpty()){
            ini.endGroup();
            break;
        }
        joblist.joblist_title = strdupV(ini.value(TITLE_KEY,"").toString().toLocal8Bit().data());
        joblist.forcelaunch = ini.value(FORCELAUNCH_KEY,true).toBool();
        for(int j=0; j < JOBLIST_JOBMAX;j++){
            sprintf(buf,FMT_JOBLIST_TITLEKEY,j);
            ini.beginGroup(buf);
            if(ini.value(TITLE_KEY,"").toString().isEmpty()){
                ini.endGroup();
                break;
            }
            joblist.jobname_pt[j] = strdupV(ini.value(TITLE_KEY,"").toString().toLocal8Bit().data());
            joblist.joblistentry++;
            ini.endGroup();
        }
        ini.endGroup();
        AddJobList(&joblist);
    }

    // FinAct
    for (i=0; i < FINACT_MAX; i++) {
        FinAct	act;

        sprintf(buf, FMT_FINACT_KEY, i);
        ini.beginGroup(buf);

        if(ini.value(TITLE_KEY,"").toString().isEmpty()){
            ini.endGroup();
            break;
        }
        act.title = strdupV(ini.value(TITLE_KEY,"").toString().toLocal8Bit().data());
        act.sound = strdupV(ini.value(SOUND_KEY,"").toString().toLocal8Bit().data());
        act.command = strdupV(ini.value(CMD_KEY,"").toString().toLocal8Bit().data());
        act.account = strdupV(ini.value(ACCOUNT_KEY,"").toString().toLocal8Bit().data());

        //復号化
        QByteArray decryptdata = decryptData(ini.value(PASSWORD_KEY,"").toByteArray());
        act.password = strdupV(decryptdata);

        act.smtpserver = strdupV(ini.value(SMTPSERVER_KEY,"").toString().toLocal8Bit().data());
        QString port = ini.value(PORT_KEY,FINACT_PORT_DEFAULT).toString();
        if(port.isEmpty()){
            act.port = strdupV(FINACT_PORT_DEFAULT);
        }
        else{
            act.port = strdupV(port.toStdString().c_str());
        }
        act.tomailaddr = strdupV(ini.value(TOMAILADDR_KEY,"").toString().toLocal8Bit().data());
        act.frommailaddr = strdupV(ini.value(FROMMAILADDR_KEY,"").toString().toLocal8Bit().data());
        act.flags = ini.value(FLAGS_KEY,DEFAULT_COPYFLAGS).toInt();

        act.shutdownTime = ini.value(SHUTDOWNTIME_KEY,0).toInt();
        AddFinActV(&act);
        ini.endGroup();
    }

    // ファイル名で存在が確認できなかったら、ファイル作成かな
    // 初回起動とか、意図的に設定ファイル消しちゃってる場合は書き込みするよ
    if(QFile(ini.fileName()).exists() == FALSE){
        WriteIni();
    }
    delete [] buf;
    return	TRUE;
}

BOOL Cfg::PostReadIni(void)
{
    //シャットダウン周りは未実装
    /*
    DWORD id_list[] = {
        IDS_FINACT_NORMAL, IDS_FINACT_SUSPEND, IDS_FINACT_HIBERNATE, IDS_FINACT_SHUTDOWN, 0
    };
    DWORD flags_list[] = {
        0,				   FinAct::SUSPEND,	   FinAct::HIBERNATE,	 FinAct::SHUTDOWN,	0
    };
    */

    DWORD id_list[] = {
        IDS_FINACT_NORMAL,0
    };
    DWORD flags_list[] = {
        0,0
    };

    for (int i=0; id_list[i]; i++) {

        if (i >= finActMax) {
            FinAct	act;
            act.flags = flags_list[i] | FinAct::BUILTIN;
            if (i >= 1) act.shutdownTime = 60;
            act.SetString(GetLoadStrV(id_list[i]),
                          BLANKARG,BLANKARG,BLANKARG,
                          BLANKARG,BLANKARG,BLANKARG,
                          BLANKARG,BLANKARG);

            AddFinActV(&act);
        }
        if(finActArray[i]->flags & FinAct::BUILTIN){
            free(finActArray[i]->title);
            finActArray[i]->title = strdupV(GetLoadStrV(id_list[i]));
        }
    }
    return	TRUE;
}


// 定義ファイルの書き換え関数
BOOL Cfg::WriteIni(void)
{
    QSettings ini(userDirV + "/" + FASTCOPY_INI,QSettings::IniFormat);

    ini.beginGroup(MAIN_SECTION);

    ini.setValue(BUFSIZE_KEY,bufSize);
    ini.setValue(MAXTRANSSIZE_KEY, maxTransSize);
    ini.setValue(MAXAIONUM_KEY, maxAionum);
    ini.setValue(MAX_HISTORY_KEY, maxHistoryNext);
    ini.setValue(COPYMODE_KEY, copyMode);
    ini.setValue(SKIPEMPTYDIR_KEY, skipEmptyDir);
    ini.setValue(IGNORE_ERR_KEY, ignoreErr);
    ini.setValue(ESTIMATE_KEY, estimateMode);
    ini.setValue(DISKMODE_KEY, diskMode);
    ini.setValue(ISTOPLEVEL_KEY, isTopLevel);
    ini.setValue(ISERRLOG_KEY, isErrLog);
    ini.setValue(FILELOGMODE_KEY, fileLogMode);
    ini.setValue(ACLERRLOG_KEY, aclErrLog);
    ini.setValue(STREAMERRLOG_KEY, streamErrLog);
    ini.setValue(ISSAMEDIRRENAME_KEY, isSameDirRename);
    ini.setValue(EXECCONRIM_KEY, execConfirm);
    ini.setValue(FORCESTART_KEY, forceStart);
    ini.setValue(LCID_KEY, lcid);
    ini.setValue(SPEEDLEVEL_KEY, speedLevel);
    ini.setValue(OWDEL_KEY, enableOwdel);
    ini.setValue(ACL_KEY, enableAcl);
    ini.setValue(XATTR_KEY, enableXattr);
    ini.setValue(IGNORE_DOT_KEY,Dotignore_mode);
    ini.setValue(LTFS_KEY,enableLTFS);
    ini.setValue(VERIFY_KEY, enableVerify);
    ini.setValue(VERIFY_ERRDEL_KEY, enableVerifyErrDel);
    ini.setValue(USEMD5_KEY, usingMD5);
    ini.setValue(NSA_KEY, enableNSA);
    ini.setValue(DELDIR_KEY, delDirWithFilter);
    ini.setValue(MOVEATTR_KEY, enableMoveAttr);
    ini.setValue(SERIALMOVE_KEY, serialMove);
    ini.setValue(SERIALVERIFYMOVE_KEY, serialVerifyMove);
    ini.setValue(REPARSE_KEY, isReparse);
    ini.setValue(EXTENDFILTER_KEY, isExtendFilter);
    ini.setValue(TASKBARMODE_KEY, taskbarMode);
    ini.setValue(INFOSPAN_KEY, infoSpan);
    ini.setValue(MACPOS_KEY,macpos);
    ini.setValue(MACSIZE_KEY,macsize);
    ini.setValue(RWSTAT_KEY,rwstat);
    ini.setValue(ASYNCIO_KEY,async);
    ini.setValue(DISABLE_UTIME_KEY,isDisableutime);
//  ini.setValue(READAHEAD_KEY,enableReadahead);
    ini.setValue(F_ODIRECT_KEY,enableOdirect);
    ini.setValue(JOBLISTMODE_KEY,isJobListMode);

    ini.endGroup();


    char	*section_array[] = {	SRC_HISTORY, DST_HISTORY, DEL_HISTORY,
                                    INC_HISTORY, EXC_HISTORY,
                                    FROMDATE_HISTORY, TODATE_HISTORY,
                                    MINSIZE_HISTORY, MAXSIZE_HISTORY };
    void	***history_array[] = {	&srcPathHistory, &dstPathHistory, &delPathHistory,
                                    &includeHistory, &excludeHistory,
                                    &fromDateHistory, &toDateHistory,
                                    &minSizeHistory, &maxSizeHistory };
    int		i, j;
    char	key[100];
    char	*buf = new char [MAX_HISTORY_CHAR_BUF];

    for (i=0; i < sizeof(section_array) / sizeof(char *); i++) {
        char	*&section = section_array[i];
        void	**&history = *history_array[i];
        ini.beginGroup(section);
        for (j=0; j < maxHistory; j++) {
            sprintf(key, "%d", j);

            if (j < maxHistoryNext){
                ini.setValue(key,(char*)history[j]);
            }
            else{
                ini.remove(key);
            }
        }
        //1セクション終わり
        ini.endGroup();
    }

// Job
    for (i=0; i < jobMax; i++) {
        sprintf(buf, FMT_JOB_KEY, i);
        Job *job = jobArray[i];

        ini.beginGroup(buf);

        VtoIniStr(job->title, buf);
        //ini.SetStr(TITLE_KEY,		buf);
        ini.setValue(TITLE_KEY,buf);

        VtoIniStr(job->src, buf);
        ini.setValue(SRC_KEY,buf);

        VtoIniStr(job->dst, buf);
        ini.setValue(DST_KEY,buf);

        VtoIniStr(job->cmd, buf);
        ini.setValue(CMD_KEY,buf);

        VtoIniStr(job->includeFilter, buf);
        ini.setValue(INCLUDE_KEY,buf);
        VtoIniStr(job->excludeFilter, buf);
        ini.setValue(EXCLUDE_KEY,buf);

        VtoIniStr(job->fromDateFilter,buf);
        ini.setValue(FROMDATE_KEY,buf);
        VtoIniStr(job->toDateFilter,buf);
        ini.setValue(TODATE_KEY,buf);

        VtoIniStr(job->minSizeFilter,buf);
        ini.setValue(MINSIZE_KEY,buf);
        VtoIniStr(job->maxSizeFilter,buf);
        ini.setValue(MAXSIZE_KEY,buf);

        ini.setValue(ESTIMATE_KEY,job->estimateMode);
        ini.setValue(DISKMODE_KEY,job->diskMode);
        ini.setValue(IGNORE_ERR_KEY,job->ignoreErr);
        ini.setValue(OWDEL_KEY,job->enableOwdel);
        ini.setValue(ACL_KEY,job->enableAcl);
        ini.setValue(STREAM_KEY,job->enableXattr);
        ini.setValue(VERIFY_KEY,job->enableVerify);
        ini.setValue(FILTER_KEY,job->isFilter);
        ini.setValue(BUFSIZE_KEY,job->bufSize);
        ini.setValue(IGNORE_DOT_KEY,job->Dotignore_mode);
        ini.setValue(LTFS_KEY,job->LTFS_mode);
        ini.setValue(USEMD5_KEY,job->usingMD5);
        ini.setValue(ISLISTING_KEY,job->isListing);
        ini.setValue(FLAGS_KEY,job->flags);

        ini.endGroup();
    }

// JobList
    for (i=0; i < joblistMax; i++) {
        sprintf(buf, FMT_JOBLIST_KEY, i);
        JobList *joblist = joblistArray[i];
        ini.beginGroup(buf);

        ini.setValue(TITLE_KEY,(char*)joblist->joblist_title);
        ini.setValue(FORCELAUNCH_KEY,joblist->forcelaunch);
        for(int j=0; j < joblist->joblistentry;j++){
            sprintf(buf,FMT_JOBLIST_TITLEKEY,j);
            ini.beginGroup(buf);
            ini.setValue(TITLE_KEY,(char*)joblist->jobname_pt[j]);
            ini.endGroup();
        }
        ini.endGroup();
    }

// FinAct

    for (i=0; i < finActMax; i++) {
        sprintf(buf, FMT_FINACT_KEY, i);
        FinAct *finAct = finActArray[i];

        ini.beginGroup(buf);
        VtoIniStr(finAct->title, buf);
        ini.setValue(TITLE_KEY,buf);
        VtoIniStr(finAct->sound, buf);
        ini.setValue(SOUND_KEY,buf);
        VtoIniStr(finAct->command, buf);
        ini.setValue(CMD_KEY,buf);
        VtoIniStr(finAct->account,buf);
        ini.setValue(ACCOUNT_KEY,buf);
        VtoIniStr(finAct->password,buf);
        QByteArray encryptdata = encryptData(buf);
        ini.setValue(PASSWORD_KEY,encryptdata);
        VtoIniStr(finAct->smtpserver,buf);
        ini.setValue(SMTPSERVER_KEY,buf);
        VtoIniStr(finAct->port,buf);
        ini.setValue(PORT_KEY,buf);
        VtoIniStr(finAct->tomailaddr,buf);
        ini.setValue(TOMAILADDR_KEY,buf);
        VtoIniStr(finAct->frommailaddr,buf);
        ini.setValue(FROMMAILADDR_KEY,buf);
        ini.setValue(SHUTDOWNTIME_KEY,	finAct->shutdownTime);
        ini.setValue(FLAGS_KEY,			finAct->flags);
        ini.endGroup();
    }
    delete [] buf;
    // QSettingクラスは後処理とかcloseとか不要みたいなので、無条件true返す
    return(true);

}

BOOL Cfg::EntryPathHistory(void *src, void *dst)
{
    void	*path_array[] = { src, dst };
    void	***history_array[] = { &srcPathHistory, &dstPathHistory };

    return	EntryHistory(path_array, history_array, 2);
}

BOOL Cfg::EntryDelPathHistory(void *del)
{
    void	*path_array[] = { del };
    void	***history_array[] = { &delPathHistory };

    return	EntryHistory(path_array, history_array, 1);
}

BOOL Cfg::EntryFilterHistory(void *inc, void *exc, void *from, void *to, void *min, void *max)
{
    void	*path_array[] = { inc, exc, from, to, min, max };
    void	***history_array[] = {
        &includeHistory, &excludeHistory, &fromDateHistory, &toDateHistory,
        &minSizeHistory, &maxSizeHistory
    };

    return	EntryHistory(path_array, history_array, 6);
}

BOOL Cfg::EntryHistory(void **path_array, void ****history_array, int max)
{
    BOOL	ret = TRUE;
    void	*target_path;

    for (int i=0; i < max; i++) {
        int		idx;
        void	*&path = path_array[i];
        void	**&history = *history_array[i];

        if (!path || strlenV(path) >= NEW_MAX_HISTORY_BUF || GetChar(path, 0) == 0) {
            ret = FALSE;
            continue;
        }
        for (idx=0; idx < maxHistory; idx++) {
            if (lstrcmpiV(path, history[idx]) == 0)
                break;
        }
        if (idx) {
            //既にヒストリ要素満杯？
            if (idx == maxHistory) {
                //先頭に差し込むパスをメモリ確保してクローン
                target_path = strdupV(path);
                //消える予定のケツのパスをfree
                free(history[--idx]);
            }
            else {
                //同じパス名なんだからわざわざメモリ確保する必要ないよに。アドレスだけこぴっとくべ
                target_path = history[idx];
            }
            //先頭に挿入するので、先頭から一個先のアドレス配列を全部一個分後ろに移動
            memmove(history + 1, history, idx * sizeof(void *));
            //先頭に今回のパス文字列のアドレスを挿入
            history[0] = target_path;
        }
    }
    return	ret;
}

BOOL Cfg::IniStrToV(char *inipath, void *path)
{
    strcpyV(path, inipath);

    return	TRUE;
}

BOOL Cfg::VtoIniStr(void *path, char *inipath)
{
    strcpyV(inipath, path);

    return	TRUE;
}

int Cfg::SearchJobV(void *title)
{
    for (int i=0; i < jobMax; i++) {
        if (lstrcmpiV(jobArray[i]->title, title) == 0)
            return	i;
    }
    return	-1;
}

BOOL Cfg::AddJobV(const Job *job)
{
    if (GetChar(job->title, 0) == 0
    || strlenV(job->src) >= NEW_MAX_HISTORY_BUF || GetChar(job->src, 0) == 0)
        return	FALSE;

    int idx = SearchJobV(job->title);

    if (idx >= 0) {
        delete jobArray[idx];
        jobArray[idx] = new Job(*job);
        return TRUE;
    }

#define ALLOC_JOB	100
    if ((jobMax % ALLOC_JOB) == 0)
        jobArray = (Job **)realloc(jobArray, sizeof(Job *) * (jobMax + ALLOC_JOB));

    for (idx=0; idx < jobMax; idx++) {
        if (lstrcmpiV(jobArray[idx]->title, job->title) > 0)
            break;
    }
    memmove(jobArray + idx + 1, jobArray + idx, sizeof(Job *) * (jobMax++ - idx));
    jobArray[idx] = new Job(*job);
    return	TRUE;
}

BOOL Cfg::DelJobV(void *title)
{
    int idx = SearchJobV(title);

    if(idx == -1){
        return	FALSE;
    }

    char	*buf = new char [MAX_HISTORY_CHAR_BUF];

    //windowsの場合はwriteiniでグループの消し込み面倒みてくれたけど、
    //Qtの場合はここで消し込まないとファイル上に残ったままになる
    QSettings ini(userDirV + "/" + FASTCOPY_INI,QSettings::IniFormat);

    for (int i=idx; i < jobMax; i++) {
        sprintf(buf, FMT_JOB_KEY, i);
        ini.beginGroup(buf);
        ini.remove("");
        ini.endGroup();
    }

    delete jobArray[idx];
    memmove(jobArray + idx, jobArray + idx + 1, sizeof(Job *) * (--jobMax - idx));

    delete [] buf;
    return	TRUE;
}

int Cfg::SearchJobList(void *title)
{
    for(int i=0; i < joblistMax; i++) {
        if(lstrcmpiV(joblistArray[i]->joblist_title, title) == 0){
            return	i;
        }
    }
    return	-1;
}

//指定されたジョブがジョブリスト登録されている個数を返却
//注意:ジョブリスト単位でカウントする。ジョブリスト中に複数個同一ジョブ名が存在しても+1。
int Cfg::SearchJobnuminAllJobList(void* job_title){
    int jobnum=0;
    for (int i=0; i < joblistMax; i++){
        for(int j=0;j < joblistArray[i]->joblistentry; j++){
            if(lstrcmpiV(job_title,(char*)joblistArray[i]->jobname_pt[j]) == 0){
                //ジョブリスト中に同一ジョブ名が複数あっても気にせず一個扱いとする
                jobnum++;
                break;
            }
        }
    }
    return jobnum;
}

BOOL Cfg::AddJobList(const JobList *joblist)
{
    if(!joblist->joblist_title){
        return	FALSE;
    }

    int idx = SearchJobList(joblist->joblist_title);

    if (idx >= 0) {
        delete joblistArray[idx];
        joblistArray[idx] = new JobList(*joblist);
        return TRUE;
    }

#define ALLOC_JOB	100
    if ((joblistMax % ALLOC_JOB) == 0)
        joblistArray = (JobList **)realloc(joblistArray, sizeof(JobList *) * (joblistMax + ALLOC_JOB));

    for (idx=0; idx < joblistMax; idx++) {
        if (lstrcmpiV(joblistArray[idx]->joblist_title, joblist->joblist_title) > 0)
            break;
    }
    memmove(joblistArray + idx + 1, joblistArray + idx, sizeof(JobList *) * (joblistMax++ - idx));
    joblistArray[idx] = new JobList(*joblist);
    return	TRUE;
}

int Cfg::AddJobtoJobList(int joblistindex,Job *job){

    //内部矛盾
    if(!joblistArray[joblistindex]){
        //qDebug() << "InternalError:index out of range" << joblistindex;
        return -1;
    }
    JobList *list_pt = joblistArray[joblistindex];
    //1ジョブリストのエントリ数オーバー
    if(list_pt->joblistentry + 1 >= JOBLIST_JOBMAX){
        return -1;
    }
    //追加したろかね
    list_pt->joblistentry++;
    list_pt->jobname_pt[list_pt->joblistentry] = (void*)job;
    //なんとなく自分が登録した位置をリターンしておくかあ
    return(list_pt->joblistentry);
}

BOOL Cfg::DelJobList(void *title)
{
    int idx = SearchJobList(title);
    if(idx == -1){
        return	FALSE;
    }

    char	*buf = new char [MAX_HISTORY_CHAR_BUF];

    //windowsの場合はwriteiniでグループの消し込み面倒みてくれたけど、
    //Qtの場合はここで消し込まないとファイル上に残ったままになる
    QSettings ini(userDirV + "/" + FASTCOPY_INI,QSettings::IniFormat);

    for(int i=idx; i < joblistMax; i++){
        sprintf(buf, FMT_JOBLIST_KEY, i);
        ini.beginGroup(buf);
        ini.remove("");
        ini.endGroup();
    }

    delete joblistArray[idx];
    memmove(joblistArray + idx, joblistArray + idx + 1, sizeof(JobList *) * (--joblistMax - idx));

    delete [] buf;
    return	TRUE;
}

int Cfg::DelJobinAllJobList(void *job_title){

    int deljobnum = 0;

    if(joblistMax){
        //全てのジョブリストをチェック
        for(int i=0; i < joblistMax;i++){
            JobList *list_pt = joblistArray[i];

            int j=0;
            //当該ジョブリスト内のジョブ全部チェック
            while(j < list_pt->joblistentry){
                //削除予定のJob名称とジョブリストのJob名称が同じ？
                if(lstrcmpiV((char*)list_pt->jobname_pt[j], job_title) == 0){
                    //削除確定
                    //消す予定のポインタが入ってるアドレスを、一個後ろの後続領域で上書き
                    memmove(&list_pt->jobname_pt[j],
                            &list_pt->jobname_pt[j+1],
                            sizeof(void *) * (JOBLIST_MAX - --list_pt->joblistentry));
                    //上書きしちゃったので、現在のjは既にnextになっている。再チェック
                    deljobnum++;
                    continue;
                }
                else{
                    //削除対象ではない、nextをチェック
                    j++;
                }
            }
        }
    }
    return deljobnum;
}

int Cfg::DelJobinJobList(void *job_title, void *joblist_title){
    int otherjobnum = 0;

    if(joblistMax){
        //全てのジョブリストをチェック
        for(int i=0; i < joblistMax;i++){
            JobList *list_pt = joblistArray[i];
            int j=0;
            //当該ジョブリスト内のジョブ全部チェック
            while(j < list_pt->joblistentry){
                //削除予定のJob名称とジョブリストのJob名称が同じ？
                if(lstrcmpiV((char*)list_pt->jobname_pt[j], job_title) == 0){
                    //削除対象の指定JobList？
                    if(lstrcmpiV((char*)list_pt->joblist_title, joblist_title) == 0){
                        //削除確定
                        //消す予定のポインタが入ってるアドレスを、一個後ろの後続領域で上書き
                        memmove(&list_pt->jobname_pt[j],
                                &list_pt->jobname_pt[j+1],
                                sizeof(void *) * (JOBLIST_MAX - --list_pt->joblistentry));
                        //上書きしちゃったので、現在のjは既にnextになっている。再チェック
                        continue;
                    }
                    else{
                        //削除対象じゃないジョブリストに当該ジョブが存在してる数
                        otherjobnum++;
                    }
                }
                //nextチェック
                j++;
            }
        }
    }
    return otherjobnum;
}

int Cfg::SearchFinActV(void *title, BOOL is_cmdline)
{
    for (int i=0; i < finActMax; i++) {
        if (lstrcmpiV(finActArray[i]->title, title) == 0)
            return	i;
    }

    //電源関連処理が実装される日まで未実装
    /*
    if (is_cmdline) {
        int	id_list[] = { IDS_STANDBY, IDS_HIBERNATE, IDS_SHUTDOWN };
        for (int i=0; i < 3; i++) {
            if (lstrcmpiV(GetLoadStrV(id_list[i]), title) == 0) return i + 1;
        }
    }
    */

    return	-1;
}

BOOL Cfg::AddFinActV(const FinAct *finAct)
{
    if (GetChar(finAct->title, 0) == 0)
        return	FALSE;

    int		idx = SearchFinActV(finAct->title);

    if (idx >= 0) {
        DWORD builtin_flag = finActArray[idx]->flags & FinAct::BUILTIN;
        delete finActArray[idx];
        finActArray[idx] = new FinAct(*finAct);
        finActArray[idx]->flags |= builtin_flag;
        return TRUE;
    }

#define ALLOC_FINACT	100
    if ((finActMax % ALLOC_FINACT) == 0)
        finActArray =
            (FinAct **)realloc(finActArray, sizeof(FinAct *) * (finActMax + ALLOC_FINACT));

    finActArray[finActMax++] = new FinAct(*finAct);
    return	TRUE;
}

BOOL Cfg::DelFinActV(void *title)
{
    int idx = SearchFinActV(title);
    if (idx == -1){
        return	FALSE;
    }

    //windowsの場合はwriteiniでグループの消し込み面倒みてくれたけど、
    //Qtの場合はここで後続のini内容を消し込まないとファイル上に残ったままになるだ
    QSettings ini(userDirV + "/" + FASTCOPY_INI,QSettings::IniFormat);

    char	*buf = new char [MAX_HISTORY_CHAR_BUF];

    for (int i=idx; i < finActMax; i++) {
        sprintf(buf, FMT_FINACT_KEY, i);

        ini.beginGroup(buf);
        ini.remove("");
        ini.endGroup();
    }

    delete finActArray[idx];
    memmove(finActArray + idx, finActArray + idx + 1, sizeof(FinAct *) * (--finActMax - idx));
    return	TRUE;
}

QByteArray Cfg::decryptData(QByteArray encyptdata){
    //復号化した結果のQByteArrayを返却。
    return(bf->decrypted(encyptdata));
}

QByteArray Cfg::encryptData(QByteArray orgdata){
    //暗号化した結果のQByteArrayを返却
    return(bf->encrypted(orgdata));
}
