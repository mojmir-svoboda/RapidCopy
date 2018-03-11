static char *tmisc_id =
    "@(#)Copyright (C) 1996-2012 H.Shirouzu		tmisc.cpp	Ver0.99";
/* ========================================================================
    Project  Name			: Win32 Lightweight  Class Library Test
    Module Name				: Application Frame Class
    Create					: 1996-06-01(Sat)
    Update					: 2012-04-02(Mon)
    ported update           : 2016-02-28
    Copyright				: H.Shirouzu Kengo Sawatsu
    Reference				:
    ======================================================================== */

#include <sys/mman.h>
#include "tlib.h"
#include "resource.h"

static StringTable msgtxt [] = {
    {IDS_MKDIR,						QObject::tr("Create")},
    {IDS_RMDIR,						QObject::tr("Delete")},
    {IDS_SRC_SELECT,				QObject::tr("Select source (OK with CtrlKey for addition)")},
    {IDS_DST_SELECT,				QObject::tr("Select destination directory")},
    {IDS_SAMEDISK,					QObject::tr("Same HDD mode")},
    {IDS_DIFFDISK,					QObject::tr("Diff HDD mode")},
    {IDS_FIX_SAMEDISK,				QObject::tr("Fix Same HDD")},
    {IDS_FIX_DIFFDISK,				QObject::tr("Fix Diff HDD")},
    {IDS_VERIFY,					QObject::tr("Verify (Size/Date)")},
    {IDS_SHA1,						QObject::tr("SHA-1")},
    {IDS_MD5,						QObject::tr("MD5")},
    {IDS_XX,						QObject::tr("xxHash")},
    {IDS_SHA2_256,					QObject::tr("SHA-2(256bit)")},
    {IDS_SHA2_512,					QObject::tr("SHA-2(512bit)")},
    {IDS_SHA3_256,					QObject::tr("SHA-3(256bit)")},
    {IDS_SHA3_512,					QObject::tr("SHA-3(512bit)")},
    {IDS_SIZEVERIFY,				QObject::tr("Verify (Size)")},
    // blank17<->19
    {IDS_ALLSKIP,					QObject::tr("Diff (No Overwrite)")},
    {IDS_ATTRCMP,					QObject::tr("Diff (Size/Date)")},
    {IDS_UPDATECOPY,				QObject::tr("Diff (Newer)")},
    {IDS_FORCECOPY,					QObject::tr("Copy (Overwrite all)")},
    {IDS_SYNCCOPY,					QObject::tr("Sync (Size/Date) ")},
    {IDS_MUTUAL,					QObject::tr("Sync (MutualNew)")},
    {IDS_MOVEATTR,					QObject::tr("Move (Size/Date)")},
    {IDS_MOVEFORCE,					QObject::tr("Move (Overwrite all)")},
    {IDS_DELETE,					QObject::tr("Delete all")},
    {IDS_EXECUTE,					QObject::tr("Execute")},
    {IDS_CANCEL,					QObject::tr("Cancel...")},
    {IDS_BEHAVIOR,					QObject::tr("Behavior changes whether the end of DestDir character is '/' or not.\nPlease refer to help for details or DestDir field Tooltip.")},
    {IDS_UPDATE,					QObject::tr("Because installed shell extension is old, it will be updated.")},
    {IDS_FILESELECT,				QObject::tr("Select files...")},
    {IDS_DIRSELECT,					QObject::tr("Back to directory select")},
    {IDS_SAMEPATHERR,				QObject::tr("Source and DestDir are the identical.")},
    {IDS_PARENTPATHERR,				QObject::tr("It can't be moved from parent to child directory")},
    {IDS_MOVECONFIRM,				QObject::tr("Moving")},
    {IDS_SHELLEXT_MODIFY,			QObject::tr("Update")},
    {IDS_SHELLEXT_EXEC,				QObject::tr("Install")},
    {IDS_FASTCOPYURL,				QObject::tr("https://github.com/KengoSawa2/RapidCopy")},
    {IDS_FASTCOPYHELP,				QObject::tr("fastcopy.chm::/fastcopy_eng.htm")},
    {IDS_EXCEPTIONLOG,				QObject::tr("Exception occured.\nException info saved to \n%s\n\nPlease send this file to author.")},
    {IDS_SYNCCONFIRM,				QObject::tr("Synchronizing")},
    {IDS_BACKSLASHERR,				QObject::tr("'\\' is missing after ':'")},
    {IDS_DELETECONFIRM,				QObject::tr("Delete All Files/Dirs")},
    {IDS_ERRSTOP,					QObject::tr("Can't continue")},
    {IDS_CONFIRMFORCEEND,			QObject::tr("Do you want to force application termination?")},
    {IDS_STOPCONFIRM,				QObject::tr("Do you want to stop copying?")},
    {IDS_DELSTOPCONFIRM,			QObject::tr("Do you want to stop deleting?")},
    {IDS_DELCONFIRM,				QObject::tr("Deleting.")},
    {IDS_DUPCONFIRM,				QObject::tr("Creating duplicated files")},
    {IDS_NOFILTER_USAGE,			QObject::tr("Filter option isn't availale in move mode.")},
    {IDS_LISTCONFIRM,				QObject::tr("Do you want to stop listing?")},
    {IDS_JOBNAME,					QObject::tr("JobName: <%s>")},
    {IDS_JOBNOTFOUND,				QObject::tr("Illegal option: ""--job"" Specified Job can't be found\n")},
    {IDS_LISTING,					QObject::tr("Listing")},
    {IDS_FIXPOS_MSG,				QObject::tr("Do you want to save current window position?")},
    {IDS_FIXSIZE_MSG,				QObject::tr("Do you want to save current window size?")},
    {IDS_FINACTNAME,				QObject::tr("Post-Process Name: <%s>")},
    {IDS_FINACTMENU,				QObject::tr("Post-Process (%s)")},
    {IDS_FINACT_NORMAL,				QObject::tr("Normal")},
    {IDS_ELEVATE,					QObject::tr("&Elevate")},
    {IDS_SHUTDOWN_MSG,				QObject::tr("It will shut down\n(Auto executing will be done in %2dsec)")},
    {IDS_HIBERNATE_MSG,				QObject::tr("It will enter hibernation mode.\n(Auto executing will be done in %2dsec)")},
    {IDS_SUSPEND_MSG,				QObject::tr("It will enter standby mode.\n(Auto executing will be done in %2dsec)")},
    {IDS_FINACT_SUSPEND,			QObject::tr("Standby")},
    {IDS_FINACT_HIBERNATE,			QObject::tr("Hibernate")},
    {IDS_FINACT_SHUTDOWN,			QObject::tr("Shutdown")},
    {IDS_DATEFORMAT_MSG,			QObject::tr("Illegal date format.\nYYYYMMDD 		    ex) 20090101\n-numberW|D|h|m|s  ex) -52W, -1D")},
    {IDS_SIZEFORMAT_MSG,			QObject::tr("Illegal size format.\n number[K|M|G|T]  ex) 100, 1K, 100M")},
    {IDS_CMDNOTFOUND,				QObject::tr("Illegal option: ""--cmd""\n")},
    {IDS_VERIFYNOTFOUND,			QObject::tr("Illegal option: ""--verify""\n")},
    {IDS_FINACTNOTFOUND,			QObject::tr("Illegal option: ""--postproc""\n")},
    {IDS_DATEISOVER,				QObject::tr("RapidCopy expiration date is expired.\n Download latest trial version, or purchase of the product version.")},
    {IDS_OPENFILE,					QObject::tr("Open File")},
    {IDS_SOUNDFILEERR,				QObject::tr("please select Sound file(aiff,mp3,wav).")},
    {IDS_REPORTOUT,					QObject::tr("output the report file in the following path.\n %s \n Thank you sending to the author.")},
    {IDS_VERIFY_FILE_MISSING_ERR,	QObject::tr("verify target file is not found.")},
    {IDS_VERIFY_FILE_SIZE_ERR,		QObject::tr("verify target file Size error.")},
    {IDS_VERIFY_FILE_DATE_ERR,		QObject::tr("verify target file Date error.")},
    {IDS_VERIFY_DIR_MISSING_ERR,	QObject::tr("verify target dir is not found.")},
    {IDS_VERIFY_DIR_SIZE_ERR,		QObject::tr("verify target dir Size found.")},
    {IDS_VERIFY_DIR_DATE_ERR,		QObject::tr("verify target dir Date error.")},
    {IDS_REPORTING_NOT_CANCEL,		QObject::tr("Can not be terminated during the reporting function use.")},
    {IDS_UNDO,						QObject::tr("Undo(&U)")},
    {IDS_CUT,						QObject::tr("Cut(&T)")},
    {IDS_COPY,						QObject::tr("Copy(&C)")},
    {IDS_PASTE,						QObject::tr("Paste(&P)")},
    {IDS_DEL,						QObject::tr("Delete(&D)")},
    {IDS_SELECTALL,					QObject::tr("Select All(&A)")},
    {IDS_STATUS_WAITING_OTHER,		QObject::tr("Waiting other RapidCopy to finish")},
    {IDS_STATUS_PRESEARCHING,		QObject::tr("Presearching")},
    {IDS_STATUS_COPYING,			QObject::tr("Copying")},
    {IDS_STATUS_VERIFYING,			QObject::tr("Verifying")},
    {IDS_STATUS_LISTING,			QObject::tr("Listing")},
    {IDS_STATUS_DELETING,			QObject::tr("Deleting")},
    {IDS_XYARG_NOTFOUND,			QObject::tr("Illegal option: --xpos and --ypos must be specified at the same time.\n")},
    {IDS_CLIMODE_PROHIBIT,			QObject::tr("CLI mode is not Supported.")},
    {IDS_FASTCOPYURL_EN,			QObject::tr("https://github.com/KengoSawa2/RapidCopy")},
    //blank 115<->118
    {IDS_CMDCSVNOTFOUND,			QObject::tr("Illegal option: ""--csv""")},
    {IDS_FACMD_ALWAYS,				QObject::tr("error/no error")},
    {IDS_FACMD_NORMAL,				QObject::tr("no error")},
    {IDS_FACMD_ERROR,				QObject::tr("error")},
    {IDS_CMDCSVWITHFILELOG_ERROR,	QObject::tr("Illegal option: ""--csv"" must be enable ""--filelog""\n")},
    {IDS_CMDLTFSATTR_ERROR,			QObject::tr("Illegal option: ""--ltfs"" must be select ""--cmd [noe|siz|for|mov|del]""\n")},
    {IDS_PATH_INITERROR,			QObject::tr("Initialize Error (Invalid Source/Destination path).Check Path.")},
    {IDS_CONFIRM_REJECTED,			QObject::tr("Initialize Error (Confirm Rejected.)")},
    {IDS_VERIFY_ERROR_WARN_1,		QObject::tr("Verify Error detected. srcdigest=")},
    {IDS_VERIFY_ERROR_WARN_2,		QObject::tr(" dstdigest=")},
    {IDS_VERIFY_ERROR_WARN_3,		QObject::tr("\nIt might have also destruction occurred in other files.\nCheck the file contents of dst, Recommend ""overwrite"" re-run.\nPlease make sure there is no problem in following.\nDisk devices,FileSystems,drivers,cables....")},
    {IDS_MOVEDIR_FAILED,			QObject::tr("rmdir(move)failed:Check write failed file or hidden file exists.")},
    {IDS_NEWWINDOW_FAILED,			QObject::tr("""Open New Window"" is not possible during the operation of the RapidCopy.")},
    // blank132<->999
    {IDS_FASTCOPY,					QObject::tr("RapidCopy")},
    {IDS_HIST_CLEAR,		 		QObject::tr("Clear path")},
    {IDS_SIZESKIP,					QObject::tr("Diff (Size)")},
    // blank1004<->1079
    {IDS_CMD_NOEXIST_ONLY,			QObject::tr("noe")},
    {IDS_CMD_DIFF,					QObject::tr("dif")},
    {IDS_CMD_UPDATE,				QObject::tr("upd")},
    {IDS_CMD_FORCE_COPY,			QObject::tr("for")},
    {IDS_CMD_SYNC,					QObject::tr("syn")},
    {IDS_CMD_MUTUAL,				QObject::tr("mut")},
    {IDS_CMD_MOVE,					QObject::tr("mov")},
    {IDS_CMD_DELETE,				QObject::tr("del")},
    {IDS_CMD_OPT,					QObject::tr("cmd")},
    //{IDS_CMD_MOVEATTR,	    	QObject::tr("move_diff")},
    {IDS_BUFSIZE_OPT,				QObject::tr("bufsize")},
    {IDS_ERRSTOP_OPT,				QObject::tr("error_stop")},
    {IDS_OPENWIN_OPT,				QObject::tr("open_window")},
    {IDS_AUTOCLOSE_OPT,				QObject::tr("auto_close")},
    {IDS_FORCECLOSE_OPT,			QObject::tr("force_close")},
    {IDS_NOEXEC_OPT,				QObject::tr("no_exec")},
    {IDS_NOCONFIRMDEL_OPT,			QObject::tr("no_confirm_del")},
    {IDS_LOG_OPT,					QObject::tr("log")},
    {IDS_TO_OPT,					QObject::tr("to")},
    {IDS_ESTIMATE_OPT,				QObject::tr("estimate")},
    {IDS_JOB_OPT,					QObject::tr("job")},
    {IDS_AUTOSLOW_OPT,				QObject::tr("auto_slow")},
    {IDS_SPEED_OPT,					QObject::tr("speed")},
    //{IDS_UTF8LOG_OPT,				QObject::tr("utf8")},
    {IDS_CMD_VERIFY,				QObject::tr("ver")},
    {IDS_JOBLIST_OPT,				QObject::tr("joblist")},
    {IDS_CMD_SIZE,					QObject::tr("siz")},
    {IDS_CMD_SIZEVERIFY,			QObject::tr("ves")},
    // blank 1109<->1110
    {IDS_INCLUDE_OPT,				QObject::tr("include")},
    {IDS_EXCLUDE_OPT,				QObject::tr("exclude")},
    {IDS_FORCESTART_OPT,			QObject::tr("force_start")},
    {IDS_SKIPEMPTYDIR_OPT,			QObject::tr("skip_empty_dir")},
    {IDS_DISKMODE_OPT,				QObject::tr("disk_mode")},
    {IDS_DISKMODE_SAME,				QObject::tr("same")},
    {IDS_DISKMODE_DIFF,				QObject::tr("diff")},
    {IDS_DISKMODE_AUTO,				QObject::tr("auto")},
    {IDS_ACL_OPT,					QObject::tr("acl")},
    {IDS_STREAM_OPT,				QObject::tr("ea")},
    {IDS_OWDEL_OPT,					QObject::tr("overwrite_del")},
    {IDS_SPEED_FULL,				QObject::tr("full")},
    {IDS_SPEED_AUTOSLOW,			QObject::tr("autoslow")},
    {IDS_SPEED_SUSPEND,				QObject::tr("suspend")},
    {IDS_REPARSE_OPT,				QObject::tr("reparse")},
    {IDS_RUNAS_OPT,					QObject::tr("runas")},
    {IDS_USERDIR_MENU,				""},
    {IDS_LOGFILE_OPT,				QObject::tr("logfile")},
    {IDS_NOCONFIRMSTOP_OPT,			QObject::tr("no_confirm_stop")},
    {IDS_VERIFY_OPT,				QObject::tr("verify")},
    {IDS_USEROLDDIR_MENU,			""},
    {IDS_WIPEDEL_OPT,				QObject::tr("wipe_del")},
    {IDS_SRCFILE_OPT,				QObject::tr("srcfile")},
    {IDS_SRCFILEW_OPT,				""},
    {IDS_FINACT_OPT,				QObject::tr("postproc")},
    {IDS_NOTEPAD,					""},
    {IDS_LINKDEST_OPT,				QObject::tr("linkdest")},
    {IDS_RECREATE_OPT,				QObject::tr("recreate")},
    {IDS_FROMDATE_OPT,				QObject::tr("from_date")},
    {IDS_TODATE_OPT,				QObject::tr("to_date")},
    {IDS_MINSIZE_OPT,				QObject::tr("min_size")},
    {IDS_MAXSIZE_OPT,				QObject::tr("max_size")},
    {IDS_STANDBY,					QObject::tr("Standby")},
    {IDS_HIBERNATE,					QObject::tr("Hibernate")},
    {IDS_SHUTDOWN,					QObject::tr("Shutdown")},
    {IDS_FILELOG_OPT,				QObject::tr("filelog")},
    {IDS_TRUE,						QObject::tr("true")},
    {IDS_FALSE,						QObject::tr("false")},
    {IDS_FILELOGNAME,				QObject::tr("%04d%02d%02d-%02d%02d%02d-%d%s.log")},
    {IDS_FILELOG_SUBDIR,			QObject::tr("Log")},
    //{IDS_INSTALL_OPT,	   QObject::tr("install")},
    {IDS_VERIFYMD5_OPT,				QObject::tr("md5")},
    {IDS_VERIFYSHA1_OPT,			QObject::tr("sh1")},
    {IDS_VERIFYXX_OPT,				QObject::tr("xxh")},
    {IDS_VERIFYSHA2_256_OPT,		QObject::tr("s22")},
    {IDS_VERIFYSHA2_512_OPT,		QObject::tr("s25")},
    {IDS_VERIFYSHA3_256_OPT,		QObject::tr("s32")},
    {IDS_VERIFYSHA3_512_OPT,		QObject::tr("s35")},
    {IDS_FNOCACHE_OPT,				QObject::tr("fnocache")},
    {IDS_LTFS_OPT,					QObject::tr("ltfs")},
    {IDS_VERIFY_FALSE_OPT,			QObject::tr("fal")},
    {IDS_DOTIGNORE_OPT,				QObject::tr("nodot")},
    {IDS_X_OPT,						QObject::tr("xpos")},
    {IDS_Y_OPT,						QObject::tr("ypos")},
    {IDS_FILECSVNAME,				QObject::tr("%04d%02d%02d-%02d%02d%02d-%d%s.csv")},
    {IDS_FILECSVTITLE,				QObject::tr("OpCode1,OpCode2,OpCode3,Destination Path,Checksum,FinishTime,FileSize(Byte),Error\n")},
    {IDS_FILECSVEX,					QObject::tr("OpCode Reference:\nOpCode1:(A=Add R=Delete V=Verify E=Error)\nOpCode2:(N=Normal S=Symboliclink H=Hardlink )\nOpCode3:(F=File D=Folder(Directory))\n")},
    {IDS_FILECSV_OPT,				QObject::tr("filecsv")},
    // blank 1179<->1200
    {IDS_FULLSPEED_DISP,			QObject::tr("Full Speed")},
    {IDS_AUTOSLOW_DISP,				QObject::tr("Auto Slow")},
    {IDS_SUSPEND_DISP,				QObject::tr("Suspend")},
    {IDS_RATE_DISP,					QObject::tr("%d")},
    //blank 1205
    {IDS_DST_CASE_ERR,				QObject::tr("The case from the sensitive file system you are trying to copy a file system that is case-insensitive.\nChange the source file name or change the case sensitive file system.")},
    {IDS_FINACT_CRESAV_INFO,		QObject::tr("FinAct Saved/Created.")},
    {IDS_FINACT_DELETE_INFO,		QObject::tr("FinAct Deleted.")},
    {IDS_JOBACT_SUCCESS_INFO,		QObject::tr("Job Saved/Created.")},
    {IDS_JOBACT_ERROR_INFO,			QObject::tr("Job Add Error.")},
    {IDS_FINACT_WAIT_INFO,			QObject::tr("Wait for finish action...\n")},
    {IDS_GROWL_TITLE,				QObject::tr("RapidCopy Execution finished.")},
    {IDS_PATH_INPUT,				QObject::tr("Input %d Path.")},
    {IDS_PATH_ADDED,				QObject::tr("Added %d Path. Total %d Paths.")},
    {IDS_JOBACT_ERROR_DETAIL_INFO,	QObject::tr("Please make sure the source or destination is set.")},
    {IDS_SRCHEAP_ERROR,				QObject::tr("Source paths is too long. Reduce the paths.")},
    {IDS_SRCPATH_CLEAR,				QObject::tr("Source cleared.")},
    {IDS_DSTPATH_CLEAR,				QObject::tr("Dest cleared.")},
    {IDS_FINACT_SANDBOX_SOUND_ERR,	QObject::tr("Can't select the other sound of RapidCopy folder.")},
    {IDS_ABOUT_WINDOWTITLE,			QObject::tr("About %s")},
    {IDS_FINACT_EMPTYSTR,			QObject::tr("No sound.")},
    //blank 1222<->1228
    {IDS_SIZECHANGED_ERROR,			QObject::tr("Filesize change detected. Check file.")},
    {IDS_LTFS_REPLACE_WARN,			QObject::tr("Warning:LTFS prohibit character detected. Prohibit character Convert to ""_""")},
    {IDS_LTFS_REPLACE_ERROR,		QObject::tr("LTFS prohibit character Convert error. You need to do the conversion process.")},
    {IDS_JOBLISTACT_SUCCESS_INFO,	QObject::tr("JobList Created.")},
    {IDS_JOBLISTACT_ERROR_INFO,		QObject::tr("JobList Created Error.Plz Report.")},
    {IDS_JOB_DELETEREQ,				QObject::tr("Job will delete. Are you ok?")},
    {IDS_JOBLISTDEL_SUCCESS_INFO,	QObject::tr("JobList Deleted.")},
    {IDS_JOBLISTDEL_ERROR_INFO,		QObject::tr("JobList Deleted Error.")},
    {IDS_JOBLISTMAX_ERROR,			QObject::tr("JobList Entry Max detected. max = 256")},
    {IDS_JOBGROUPDEL_ERROR,			QObject::tr("JobGroup delete Error. plz report.")},
    {IDS_JOBLIST_PLACEHOLDER,		QObject::tr("input/select JobList name")},
    {IDS_JOBLIST_CANCEL,			QObject::tr("Cancel JobList")},
    {IDS_JOBLIST_EXECUTE,			QObject::tr("Execute JobList")},
    {IDS_JOBLIST_JOBSAVE,			QObject::tr("Job Overwrite.")},
    {IDS_JOBLISTNOTFOUND,			QObject::tr("Illegal option: ""--joblist"" Specified JobList can't be found\n")},
    {IDS_SINGLELIST_MENUNAME,		QObject::tr("Single Jobs")},
    {IDS_JOBLIST_CHANGENOTIFY,		QObject::tr(" is not saved. changing save?")},
    {IDS_JOBNAME_CHANGE,			QObject::tr("Job name changed.")},
    {IDS_JOBNAME_DUPERROR,			QObject::tr("The name is duplicate. Please renameing.")},
    {IDS_JOBLIST_MENUNAME,			QObject::tr("JobLists")},
    {IDS_SINGLEJOB_MODERROR,		QObject::tr("Can't Save/Delete JobList Job. Please renaming.")},
    {IDS_JOBLISTRENAME_TITLE,		QObject::tr("JobList Rename")},
    {IDS_JOBLIST_DELETEREQ,			QObject::tr("JobList will delete. Are you ok?")},
    {IDS_JOB_DELETEALLREQ,			QObject::tr("All Job will delete. Are you ok?")},
    {IDS_JOB_CHANGEDNOTIFY,			QObject::tr("job changed detected. If you want save(overwrite), press Option+S")},
    {IDS_LTFS_REPLACE_CHECK_ERR,    QObject::tr("LTFS prohibit character duplication detected. You should change source file/folder name.")},
    // 1255->blank
    // 1300<->1400 ツールチップテキスト用
    {TOOLTIP_TXTEDIT_SRC,			QObject::tr("Enter Copy source files and folders in the Source button or drag-and-drop.")},
    {TOOLTIP_TXTEDIT_DST,			QObject::tr("Enter Copy Destination folder in the DestDir button or drag-and-drop.\nPut '/' at the end, create a folder of the source folder name to the destination.\nDon't put a '/' at the end, copy the data directly below the source folder to the destination.")},
    // blank 1400<->2262
    {FINACT_MENUITEM,				QObject::tr("Add/Modify/Del...")},
    //システムコール用エラーメッセージ
    {RAPIDCOPY_ERRNO_EPERM,			QObject::tr("Operation not permitted. check file.")},
    {RAPIDCOPY_ERRNO_ENOENT,		QObject::tr("No such file or directory. check exists.")},
    {RAPIDCOPY_ERRNO_EINTR,			QObject::tr("Interrupted function call.")},
    {RAPIDCOPY_ERRNO_EIO,			QObject::tr("An I/O error occurred while (read/write) file system. check Device or file system.")},
    {RAPIDCOPY_ERRNO_ENXIO,			QObject::tr("A requested action cannot be performed by the device.")},
    {RAPIDCOPY_ERRNO_E2BIG,			QObject::tr("Arg list too long.")},
    {RAPIDCOPY_ERRNO_ENOEXEC,		QObject::tr("Exec format error.")},
    {RAPIDCOPY_ERRNO_EBADF,			QObject::tr("Bad file descripter. check file permission.")},
    {RAPIDCOPY_ERRNO_ENOMEM,		QObject::tr("Cannot allocate memory. please make sure sufficient amount of memory exists.")},
    {RAPIDCOPY_ERRNO_EACCES,		QObject::tr("Permission denied. check permission or system prohibit char usage.")},
    {RAPIDCOPY_ERRNO_EFAULT,		QObject::tr("(read/write) operation internal error was occured. Plaease Report.")},
    {RAPIDCOPY_ERRNO_ENOTBLK,		QObject::tr("Not a block device. check block device or file.")},
    {RAPIDCOPY_ERRNO_EBUSY,			QObject::tr("Resource busy. check the system resource,run again.")},
    {RAPIDCOPY_ERRNO_EEXIST,		QObject::tr("file or directory exists. Please running again by removing the target file as necessary.")},
    {RAPIDCOPY_ERRNO_EXDEV,			QObject::tr("A hard link to a file on another file system was attempted. hardlink operation has been canceled.")},
    {RAPIDCOPY_ERRNO_ENODEV,		QObject::tr("An attempt was made to apply an inappropriate function to a device. operation has been canceled.")},
    {RAPIDCOPY_ERRNO_ENOTDIR,		QObject::tr("A component of the path prefix is not a directory. check file path.")},
    {RAPIDCOPY_ERRNO_EISDIR,		QObject::tr("Directory for instructions intended for the file has been specified. check file.")},
    {RAPIDCOPY_ERRNO_EINVAL,		QObject::tr("Invalid argument was passed to a system call. Please report.")},
    {RAPIDCOPY_ERRNO_ENFILE,		QObject::tr("Too many open files in system. close unnecessary files or extend the number of files open.")},
    {RAPIDCOPY_ERRNO_EMFILE,		QObject::tr("Too many open files in RapidCopy. Please report.")},
    {RAPIDCOPY_ERRNO_EFBIG,			QObject::tr("File was greater than the maximum file size. check target file system file size limited.")},
    {RAPIDCOPY_ERRNO_ENOSPC,		QObject::tr("There is no free space remaining on the file system containing the file. check the free space.")},
    {RAPIDCOPY_ERRNO_EROFS,			QObject::tr("Tried to open a read-only file in writing. check permission or device.")},
    {RAPIDCOPY_ERRNO_EMLINK,		QObject::tr("Maximum allowable hard links to a single file has been exceeded. hardlink operation has been canceled.")},
    {RAPIDCOPY_ERRNO_EAGAIN,		QObject::tr("Resource temporarily unavailable.")},
    {RAPIDCOPY_ERRNO_ENOTSUP,		QObject::tr("Operation not supported at this file system.")},
    {RAPIDCOPY_ERRNO_ENETDOWN,		QObject::tr("Network is down. check network connection.")},
    {RAPIDCOPY_ERRNO_ENETUNREACH,	QObject::tr("Network is unreachable. check network connection.")},
    {RAPIDCOPY_ERRNO_ENETRESET,		QObject::tr("Network is reset. check network connection.")},
    {RAPIDCOPY_ERRNO_ECONNABORTED,	QObject::tr("Network connection is aborted. check network connection.")},
    {RAPIDCOPY_ERRNO_ECONNRESET,	QObject::tr("Network connection is reset. check network connection.")},
    {RAPIDCOPY_ERRNO_ENOBUFS,		QObject::tr("No socket buffer space available. check system socket buffer.")},
    {RAPIDCOPY_ERRNO_ETIMEDOUT,		QObject::tr("Time out detected. check network connection or target system.")},
    {RAPIDCOPY_ERRNO_ELOOP,			QObject::tr("Too many levels of symbolic links. check loop symbolic link or MAXSIMLINKS.")},
    {RAPIDCOPY_ERRNO_ENAMETOOLONG,	QObject::tr("File name too long. check file path.")},
    {RAPIDCOPY_ERRNO_EDQUOT,		QObject::tr("Detected a write to disk quota limit. Please check the quota.")},
    {RAPIDCOPY_ERRNO_ENOSYS,		QObject::tr("Call function is not supported on the system.")},
    {RAPIDCOPY_ERRNO_ENOATTR,		QObject::tr("EA data not found. check file have EA or file system type.")},
    {RAPIDCOPY_ERRNO_EOPNOTSUPP,	QObject::tr("Operation not supported at this file system.")},
    //blank 4xxxx
    {RAPIDCOPY_COUNTER_XATTR_ENOTSUP,	QObject::tr("Notice: EA has been lost.")},
    {RAPIDCOPY_COUNTER_ACL_EOPNOTSUPP,	QObject::tr("Notice: ACL has been lost.")},
    {RAPIDCOPY_COUNTER_UTIMES_EAGAIN,	QObject::tr("Failed update Date information. Please re-run with [Diff (Size/date)].")},
    {RAPIDCOPY_COUNTER_XATTR_ENOTSUP_EA,QObject::tr("Notice: EA has been lost. EA name is below. ")},
    {IDS_SENTINEL,		  			QObject::tr("Nothing to use!!!!!")}
};

bool THashObj::LinkHash(THashObj *top)
{
    if (priorHash)
        return FALSE;
    this->nextHash = top->nextHash;
    this->priorHash = top;
    top->nextHash->priorHash = this;
    top->nextHash = this;
    return true;
}

bool THashObj::UnlinkHash()
{
    if (!priorHash)
        return FALSE;
    priorHash->nextHash = nextHash;
    nextHash->priorHash = priorHash;
    priorHash = nextHash = NULL;
    return true;
}


THashTbl::THashTbl(int _hashNum, bool _isDeleteObj)
{
    hashTbl = NULL;
    registerNum = 0;
    isDeleteObj = _isDeleteObj;

    if ((hashNum = _hashNum) > 0) {
        Init(hashNum);
    }
}

THashTbl::~THashTbl()
{
    UnInit();
}

bool THashTbl::Init(int _hashNum)
{
    if ((hashTbl = new THashObj [hashNum = _hashNum]) == NULL) {
        return	FALSE;	// VC4's new don't occur exception
    }

    for (int i=0; i < hashNum; i++) {
        THashObj	*obj = hashTbl + i;
        obj->priorHash = obj->nextHash = obj;
    }
    registerNum = 0;
    return	true;
}

void THashTbl::UnInit()
{
    if (hashTbl) {
        if (isDeleteObj) {
            for (int i=0; i < hashNum && registerNum > 0; i++) {
                THashObj	*start = hashTbl + i;
                for (THashObj *obj=start->nextHash; obj != start; ) {
                    THashObj *next = obj->nextHash;
                    delete obj;
                    obj = next;
                    registerNum--;
                }
            }
        }
        delete [] hashTbl;
        hashTbl = NULL;
        registerNum = 0;
    }
}

void THashTbl::Register(THashObj *obj, u_int hash_id)
{
    obj->hashId = hash_id;

    if (obj->LinkHash(hashTbl + (hash_id % hashNum))) {
        registerNum++;
    }
}

void THashTbl::UnRegister(THashObj *obj)
{
    if (obj->UnlinkHash()) {
        registerNum--;
    }
}

THashObj *THashTbl::Search(const void *data, u_int hash_id)
{
    THashObj *top = hashTbl + (hash_id % hashNum);

    for (THashObj *obj=top->nextHash; obj != top; obj=obj->nextHash) {
        if (obj->hashId == hash_id && IsSameVal(obj, data)) {
            return obj;
        }
    }
    return	NULL;
}

/*=========================================================================
  クラス ： Condition
  概  要 ： 条件変数クラス
  説  明 ：
  注  意 ：
=========================================================================*/
Condition::Condition(void)
{
    hEvents = NULL;
}

Condition::~Condition(void)
{
    UnInitialize();
}

bool Condition::Initialize(int _max_threads)
{
    UnInitialize();

    max_threads = _max_threads;
    waitEvents = new WaitEvent [max_threads];
    hEvents	  = new QMutex [max_threads];
    for(int wait_id = 0; wait_id < max_threads; wait_id++){
        waitEvents[wait_id] = CLEAR_EVENT;
    }
    waitCnt = 0;
    return	true;
}

void Condition::UnInitialize(void)
{
    if (hEvents != NULL) {
        /* こうしたいけどQtの仕様上できないのよ。。
        while (--max_threads >= 0){
            hEvents[max_threads].unlock();
        }
        */
        // コピー実行もしくはリスティング2回目以降はQMutexはwait中、つまりlockされっぱなしとなる。これは正常な動作。
        // windowsの場合ならlock中のオブジェクトをclose,deleteしても問題ないが、QtだとQMutexクラスから標準出力に警告が出力されちゃう。。
        // 警告に実害はないけどunlock()してからdeleteしたい
        // が、QMutexはlock()を発行したスレッド上でunlock()しないといけないルールがある。
        // http://doc.qt.io/qt-5/qmutex.html#unlock
        //なので、ここではunlockできない。。
        delete [] hEvents;
        delete [] waitEvents;
        // こうしたいんだけどねー
        // delete [] thread_pts;
        hEvents = NULL;
        waitEvents = NULL;
    }
}

bool Condition::Wait(DWORD timeout)
{
    int		wait_id = 0;

    for (wait_id=0; wait_id < max_threads && waitEvents[wait_id] != CLEAR_EVENT; wait_id++)
        ;
    if (wait_id == max_threads) {	// 通常はありえない
        //MessageBox(0, "Detect too many wait threads", "TLib", MB_OK);
        return	FALSE;
    }
    waitEvents[wait_id] = WAIT_EVENT;
    waitCnt++;
    UnLock();


    bool status = hEvents[wait_id].tryLock(timeout);
    Lock();
    --waitCnt;
    waitEvents[wait_id] = CLEAR_EVENT;

    return(status);
}

void Condition::Notify(void)	// 現状では、眠っているスレッド全員を起こす
{
    if (waitCnt > 0) {
        for (int wait_id=0, done_cnt=0; wait_id < max_threads; wait_id++) {
            if (waitEvents[wait_id] == WAIT_EVENT) {
                hEvents[wait_id].unlock();
                waitEvents[wait_id] = DONE_EVENT;
                if (++done_cnt >= waitCnt)
                    break;
            }
        }
    }
}

/*=========================================================================
  クラス ： VBuf
  概  要 ： 仮想メモリ管理クラス
  説  明 ：
  注  意 ：
=========================================================================*/
VBuf::VBuf(int _size, int _max_size, VBuf *_borrowBuf)
{
    Init();

    if (_size || _max_size) AllocBuf(_size, _max_size, _borrowBuf);
}

VBuf::~VBuf()
{
    if (buf)
        FreeBuf();
}

void VBuf::Init(void)
{
    buf = NULL;
    borrowBuf = NULL;
    size = usedSize = maxSize = 0;
}

bool VBuf::AllocBuf(int _size, int _max_size, VBuf *_borrowBuf)
{
    if (_max_size == 0)
        _max_size = _size;
    maxSize = _max_size;
    borrowBuf = _borrowBuf;

    if (borrowBuf) {
        if (!borrowBuf->Buf() || borrowBuf->MaxSize() < borrowBuf->UsedSize() + maxSize)
            return	FALSE;
        buf = borrowBuf->Buf() + borrowBuf->UsedSize();
        borrowBuf->AddUsedSize(maxSize + PAGE_SIZE);
    }
    else {
    // 1page 分だけ余計に確保（buffer over flow 検出用）
        /* 元ソース
        if (!(buf = (BYTE *)::VirtualAlloc(NULL, maxSize + PAGE_SIZE, MEM_RESERVE, PAGE_READWRITE))) {
        */
        // Windowsの場合はMEM_RESERVE->MEM_COMMITの2段階でメモリ確保をかけるけど、mmapで即割り当て実行する
        // なので、メモリ確保はここ一回だけでいい。追加使用時のcommit処理をするGrowまでだましきっちゃえ
        if((buf = (BYTE *)(mmap(0,maxSize + PAGE_SIZE,PROT_READ | PROT_WRITE,MAP_PRIVATE | MAP_ANON,-1,0))) == MAP_FAILED){
            Init();
            return	FALSE;
        }
    }
    return	Grow(_size);
}

bool VBuf::LockBuf(void)
{
    return mlock(buf,size);
}

void VBuf::FreeBuf(void)
{
    if (buf) {
        if (borrowBuf) {
            munmap(buf,maxSize + PAGE_SIZE);
        }
        else {
            munmap(buf,maxSize + PAGE_SIZE);
        }
    }
    Init();
}

bool VBuf::Grow(int grow_size)
{
    if (size + grow_size > maxSize)
        return	FALSE;

    // 最初のAllocBuf内のmmapでメモリは確保済みなので、COMMIT処理は不要。
    //if (grow_size && !::VirtualAlloc(buf + size, grow_size, MEM_COMMIT, PAGE_READWRITE))
    //	return	FALSE;
    // 増えて使用できるようになった気分になって貰うために計算だけ実行
    size += grow_size;
    return	true;
}

char* GetLoadStrA(UINT resId)
{
    static TResHash	*hash;
    int	   i;

    if (hash == NULL) {
        hash = new TResHash(100);
    }

    TResHashObj	*obj;

    if ((obj = hash->Search(resId)) == NULL) {
        //TResHashObj自体がいらない気もするがリファインまでとりあえずこれで。
        for(i=0;msgtxt[i].resId != IDS_SENTINEL;i++){
            if(resId == msgtxt[i].resId){
                //動的翻訳したものを登録する
                obj = new TResHashObj(resId, strdup((QObject::tr(msgtxt[i].str.toLocal8Bit().data())).toLocal8Bit().data()));
                hash->Register(obj);
            }
        }
    }
    return	obj ? (char *)obj->val : NULL;
}

/*=========================================================================
    パス合成（ANSI 版）
=========================================================================*/
int MakePath(char *dest, const char *dir, const char *file)
{
    size_t	len;

    if ((len = strlen(dir)) == 0){
        return	sprintf(dest, "%s", file);
    }
    return		sprintf(dest, "%s%s%s", dir,dir[len-1] == '/' ? "" : "/", file);
}

WCHAR lGetCharIncA(const char **str)
{
    WCHAR	ch = *(*str)++;
    return	ch;
}

/*=========================================================================
    bin <-> hex
=========================================================================*/
static char  *hexstr   =  "0123456789abcdef";

int bin2hexstr(const BYTE *bindata, int len, char *buf)
{
    for (const BYTE *end=bindata+len; bindata < end; bindata++)
    {
        *buf++ = hexstr[*bindata >> 4];
        *buf++ = hexstr[*bindata & 0x0f];
    }
    *buf = 0;
    return	len * 2;
}
