/* static char *fastcopy_id =
    "@(#)Copyright (C) 2004-2012 H.Shirouzu		fastcopy.h	Ver2.10"; */
/* ========================================================================
    Project  Name			: Fast Copy file and directory
    Create					: 2004-09-15(Wed)
    Update					: 2012-06-17(Sun)
    port update	            : 2016-02-25
    Copyright				: H.Shirouzu,Kengo Sawatsu
    Reference				:
    ======================================================================== */

#include <QThread>
#include <QTime>
#include <QMutex>
#include <QEvent>
#include <QDir>
#include <QSystemSemaphore>
#include <sysexits.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/acl.h>
#include <unistd.h>
#include <linux/magic.h>
#include <QDebug>

#include "tlib.h"
#include "resource.h"
#include "utility.h"
#include "regexp.h"

#define MIN_SECTOR_SIZE		(512)
#define OPT_SECTOR_SIZE		AFT_SECTOR_SIZE
#define AFT_SECTOR_SIZE		(4096)

#define MAX_BUF				(1024 * 1024 * 1024)
#define MAX_MAINBUF_MB		(2047)					// -1 はPAGE_SIZE*4のぶん
#define MAX_MAINBUF			(1024 * 1024 * MAX_MAINBUF_MB)

#define MIN_BUF				(1024 * 1024)
#define MAX_IOSIZE_MB		(1024)
#define MAX_IOSIZE			(1024 * 1024 * MAX_IOSIZE_MB)
#define BIGTRANS_ALIGN		(32 * 1024)
#define APP_MEMSIZE			(6 * 1024 * 1024)

//LTFS禁則文字
#define LTFS_PROHIBIT_SLASH		"/"
#define LTFS_PROHIBIT_COLON		":"
//LTFS禁則文字の自動変換先
#define LTFS_CONVERTED_AFTER	"_"

#define FASTCOPY			"RapidCopy"

#define MAX_BASE_BUF		( 512 * 1024 * 1024)
#define MIN_BASE_BUF		(  64 * 1024 * 1024)
#define RESERVE_BUF			( 128 * 1024 * 1024)

#define MIN_ATTR_BUF		(256 * 1024)
//#define MAX_ATTR_BUF		(128 * 1024 * 1024)
#define MIN_ATTRIDX_BUF		ALIGN_SIZE((MIN_ATTR_BUF / 4), PAGE_SIZE)
#define MAX_ATTRIDX_BUF(x)	ALIGN_SIZE(((x) / 32), PAGE_SIZE)

#define MAX_XATTR_DATA_SIZE (MAX_NTQUERY_BUF)

#define MIN_MKDIRQUEUE_BUF	(8 * 1024)
#define MAX_MKDIRQUEUE_BUF	(256 * 1024)
#define MIN_DSTDIREXT_BUF	(64 * 1024)
//#define MAX_DSTDIREXT_BUF	(2 * 1024 * 1024)
//課題:dirへのxattr分割不可問題解決までの暫定処置
//xattr最大の((128B + 128KB + 4B(LL)) * 1000) + 1024B(rep) + 8B (ACL)
#define MAX_DSTDIREXT_BUF	(132 * 1024 * 1024)
#define MIN_ERR_BUF			(64 * 1024)
#define MAX_ERR_BUF			(4 * 1024 * 1024)
#define MAX_LIST_BUF		(128 * 1024)
#define MIN_PUTLIST_BUF		(1 * 1024 * 1024)
#define MAX_PUTLIST_BUF		(4 * 1024 * 1024)

#define MIN_DIGEST_LIST		(1 * 1024 * 1024)
#define MAX_DIGEST_LIST		(8 * 1024 * 1024)
#define MIN_MOVEPATH_LIST	(1 * 1024 * 1024)
#define MAX_MOVEPATH_LIST	(8 * 1024 * 1024)

//#define MAX_NTQUERY_BUF		(1024 * 1024 * 10)

#define CV_WAIT_TICK		1000

#define FASTCOPY_MUTEX		"/RapidCopyRunMutex"  //コピー実行を別RapidCopyプロセスとの間で排他するときのセマフォ名
#define FASTCOPY_ELOG_MUTEX	"/RapidCopyELogMutex" //標準エラーログへの書き込みを排他するときのセマフォ名
#define FASTCOPYLOG_MUTEX_INSTANCE 1				// QSystemSemaphore指定、同時実行を許可するインスタンス数
#define FASTCOPY_MUTEX_INSTANCE 1					// FastCopy同時実行インスタンス数

#define FASTCOPY_ERROR_EVENT    0x01
#define FASTCOPY_STOP_EVENT	    0x02

#define FASTCOPY_MINLIMITIOSIZE         (1 * 1024 * 1024)           //read/writeの最小I/Oサイズ
#define FASTCOPY_LTFS_MAXTRANSSIZE      FASTCOPY_MINLIMITIOSIZE	    //LTFSモード時のread/write単位。
//1MB以上を指定すると原因不明のSIGBUSになるよ
//Linuxでどうなるかはしらぬ
#define FASTCOPY_STACKSIZE (1024 * 1024 * 8)        //各スレッドのスタックサイズ

#define FASTCOPY_FILTER_FLAGS_DIRENT 0				//FilterCheck時判断構造体はdirent
#define FASTCOPY_FILTER_FLAGS_STAT 1				//FilterCheck時判断構造体はstat

//コマンドライン実行時のリタンコード
#define FASTCOPY_OK EX_OK							//正常終了(エラーが一つもない)
#define FASTCOPY_ERROR_EXISTS (EX__MAX + 1)			//エラー終了(一つ以上のファイルまたはディレクトリでエラーが発生)

#define FASTCOPY_STATDIR  "/STAT"
#define FASTCOPY_SOUNDDIR "/sound"

#define FASTCOPY_SYMDIR_SKIP	-1					//dirへのシンボリックリンクskip指示

#define FASTCOPY_UTIMES_RETRYCOUNT	3				//UTIMES失敗時のリトライ回数
#define FASTCOPY_UTIMES_RETRYUTIMES	500000			//UTIMES失敗時のsleep時間(マイクロ秒) 0.5秒

#define F_NOCACHE 1                                 //ファイルシステムキャッシュしないよフラグ

struct FsTable {
    int	 fstype;								//FastCopy::FsType
    __SWORD_TYPE f_fstype;                      //statfsで得られるファイルシステムの種別ID
    QString fstypename_log;						//RapidCopyのLog上に出力するときの出力名称
};

class FastCopy_Thread;

struct TotalTrans {
    BOOL	isPreSearch;
    int		preDirs;
    int		preFiles;
    _int64	preTrans;
    int		readDirs;
    int		readFiles;
    int		readAclStream;	//1ファイルのxattrを1回以上読み取った or acl読み取った時に+1
                            //補足:ストリーム数が複数あっても+1だし、xattrとacl両方読み取っても+1だよ。
    _int64	readTrans;
    int		writeDirs;
    int		writeFiles;
    int		linkFiles;
    int		writeAclStream;
    _int64	writeTrans;
    int		verifyFiles;
    _int64	verifyTrans;
    int		deleteDirs;
    int		deleteFiles;
    _int64	deleteTrans;
    int		skipDirs;
    int		skipFiles;
    _int64	skipTrans;
    int		filterSrcSkips;
    int		filterDelSkips;
    int		filterDstSkips;
    int		errDirs;
    int		errFiles;
    _int64	errTrans;
    _int64	errStreamTrans;
    int		errAclStream;
    int		openRetry;
};

struct TransInfo {
    TotalTrans			total;
    DWORD				tickCount;
    BOOL				isSameDrv;
    DWORD				ignoreEvent;
    DWORD				waitTick;

    VBuf				*errBuf;
    QMutex				*errCs;
    VBuf				*listBuf;
    VBuf				*csvfileBuf;
    QMutex				*listCs;
    VBuf				*statBuf;
    QMutex				*statCs;

    char				curPath[MAX_PATH];
    int					phase;          //動作フェーズ状況通知フラグ
                                        //FastCopy::Phaseのenum参照のこと
};

struct FileStat {
    _int64		fileID;
    int			hFile;
    BYTE		*upperName;		// cFileName 終端+1を指す。
    struct timespec ftCreationTime;
    struct timespec ftLastAccessTime;
    struct timespec ftLastWriteTime;
    off_t		nFileSize;          //64bit環境しかやる気ない。8バイト期待。
    mode_t		dwFileAttributes;	//stat->st_modeの値
                                    //0の場合は xattr or acl
    DWORD		lastError;			//最後に発生したerrno
    int			renameCount;
    BOOL		isExists;
    BOOL		isCaseChanged;
    int			size;
    int			minSize;		// upperName 分を含めない
    DWORD		hashVal;		// upperName の hash値

    // for hashTable
    FileStat	*next;

    // extraData
    BYTE		*acl;	// acl_getで得たACLコンテキストへのポインタ
    BYTE		*ead;	// dirへのxattr適用時はxattr設定用電文の先頭位置を示すポインタ
                        // ファイルへのxattrの場合はこっちじゃないので注意。
    BYTE		*rep;	// reparse dataだったけどシンボリックリンクのパス入れる
    int			aclSize; //acl_getで得たACLコンテキストへのポインタのサイズ。8バイト固定
    int			eadSize; //dirへのxattr適用時はxattr設定用電文サイズ。ファイルの場合は未使用。
    int			repSize; //シンボリックリンク先のリンクパス名+1(\0保証)

    // md5/sha1 digest
    BYTE		digest[SHA3_512SIZE];
    dev_t		st_dev;			// hardlinkhash算出元データ
    ino_t		st_ino;			// hardlinkhash算出元データ
    nlink_t		st_nlink;		// hardlink有無チェックに使用
    BYTE		cFileName[4];	// 先頭4バイトだけ明示的。後続に相対パス文字列が格納されるため、
                                // 以降のオフセットにはメンバ追加してはいけない。
    //ここにパス入ってるのよ！

    //winだと4バイトずつに分かれてたので並べてポインタ被せてつかってたんね
    off_t	FileSize(){return nFileSize;}
    void	SetFileSize(off_t file_size){nFileSize = file_size;}
    void	SetLinkData(DWORD *data) { memcpy(digest, data, sizeof(DWORD) * 3); }
    DWORD	*GetLinkData() { return (DWORD *)digest; }
    //先頭8バイトにはめているので、nanotime領域を無視。
    _int64	CreateTime() { return *(_int64 *)&ftCreationTime;   }
    _int64	AccessTime() { return *(_int64 *)&ftLastAccessTime; }
    _int64	WriteTime()  { return *(_int64 *)&ftLastWriteTime; }
};



class StatHash {
    FileStat	**hashTbl;
    int			hashNum;
    int			HashNum(int data_num);

public:
    StatHash() {}
    int		RequireSize(int data_num) { return	sizeof(FileStat *) * HashNum(data_num); }
    BOOL	Init(FileStat **data, int _data_num, void *tbl_buf);
    FileStat *Search(void *upperName, DWORD hash_val);
};

struct DirStatTag {
    int			statNum;
    int			oldStatSize;
    FileStat	*statBuf;
    FileStat	**statIndex;
};

struct LinkObj : public THashObj {
    void	*path;
    DWORD	data[3];
    int		len;
    DWORD	nLinks;
    LinkObj(const void *_path, DWORD _nLinks, DWORD *_data, int len=-1) {
        if (len == -1) len = strlenV(_path) + 1;
        int alloc_len = len * CHAR_LEN_V;
        path = malloc(alloc_len);
        memcpy(path, _path, alloc_len);
        memcpy(data, _data, sizeof(data));
        nLinks = _nLinks;
    }
    ~LinkObj() { if (path) free(path); }
};

class TLinkHashTbl : public THashTbl {
public:
    virtual BOOL IsSameVal(THashObj *obj, const void *_data) {
        return	!memcmp(&((LinkObj *)obj)->data, _data, sizeof(((LinkObj *)obj)->data));
    }

public:
    TLinkHashTbl() : THashTbl() {}
    virtual ~TLinkHashTbl() {}
    virtual BOOL Init(int _num) { return THashTbl::Init(_num); }
    u_int	MakeHashId(DWORD volSerial, DWORD highIdx, DWORD lowIdx) {
        DWORD data[] = { volSerial, highIdx, lowIdx };
        return	MakeHashId(data);
    }
    u_int	MakeHashId(DWORD *data) { return MakeHash(data, sizeof(DWORD) * 3); }
};

class FastCopy: public QObject{

    Q_OBJECT

public:

    enum Mode { DIFFCP_MODE, SYNCCP_MODE, MOVE_MODE, MUTUAL_MODE, DELETE_MODE,VERIFY_MODE};
    enum OverWrite { BY_NAME, BY_ATTR, BY_LASTEST, BY_CONTENTS, BY_ALWAYS,BY_SIZE};
    //mode及びOverWriteの組み合わせについてはstruct CopyInfoを参照

    //RapidCopyが認識できるファイルシステム
    enum FsType { FSTYPE_NONE, FSTYPE_ADFS, FSTYPE_AFS, FSTYPE_CODA,
                  FSTYPE_EXT2,FSTYPE_EXT3,FSTYPE_EXT4,FSTYPE_BTRFS,
                  FSTYPE_F2FS,FSTYPE_ISOFS,FSTYPE_FAT,FSTYPE_NFS,
                  FSTYPE_REISERFS,FSTYPE_SMB,FSTYPE_CIFS,FSTYPE_FUSE,
                  FSTYPE_HFS,FSTYPE_JFS,FSTYPE_NTFS,FSTYPE_UDF,
                  FSTYPE_XFS};
    enum Flags {
        CREATE_OPENERR_FILE	=	0x00000001,
        USE_OSCACHE_READ	=	0x00000002,
        USE_OSCACHE_WRITE	=	0x00000004,
        PRE_SEARCH			=	0x00000008,
        SAMEDIR_RENAME		=	0x00000010,
        SKIP_EMPTYDIR		=	0x00000020,
        FIX_SAMEDISK		=	0x00000040,
        FIX_DIFFDISK		=	0x00000080,
        AUTOSLOW_IOLIMIT	=	0x00000100,
        WITH_ACL			=	0x00000200,
        // XATTR用にすげかえた
        // WITH_ALTSTREAM		=	0x00000400,
        WITH_XATTR			=	0x00000400,
        OVERWRITE_DELETE	=	0x00000800,
        OVERWRITE_DELETE_NSA=	0x00001000,
        FILE_REPARSE		=	0x00002000,
        DIR_REPARSE			=	0x00004000,
        COMPARE_CREATETIME	=	0x00008000,
        SERIAL_MOVE			=	0x00010000,
        SERIAL_VERIFY_MOVE	=	0x00020000,
        //USE_OSCACHE_READVERIFYは未使用
        USE_OSCACHE_READVERIFY=	0x00040000,
        RESTORE_HARDLINK	=	0x00080000,
        DISABLE_COMPARE_LIST=	0x00100000,
        DEL_BEFORE_CREATE	=	0x00200000,
        DELDIR_WITH_FILTER	=	0x00400000,
        LISTING				=	0x01000000,
        LISTING_ONLY_VERIFY	=	0x02000000,        //LISTINGなんだけどverifyonlyモード選択してる
        OVERWRITE_PARANOIA	=	0x04000000,
        VERIFY_FILE			=	0x08000000,
        LISTING_ONLY		=	0x10000000,
        REPORT_ACL_ERROR	=	0x20000000,
        REPORT_STREAM_ERROR	=	0x40000000,
        SKIP_DOTSTART_FILE	=	0x80000000,
    };
    //RapidCopy独自拡張の追加フラグ
    enum Flags_second {
        LTFS_MODE			= 0x00000001,			//LTFSコピーモード有効
        STAT_MODE			= 0x00000002,			//統計情報有効
        ASYNCIO_MODE		= 0x00000004,			//aio有効
        LAUNCH_CLI			= 0x00000008,			//CLIによる起動
                                                    //(当該bitが無い場合はGUI起動)
        DISABLE_UTIME		= 0x00000010,			//ファイルコピー先の時刻更新抑止
      //ENABLE_READAHEAD	= 0x00000020,			read時先読み最適化有効
        ENABLE_PREALLOCATE	= 0x00000040,			//write時フラグメント抑止有効
        CSV_FILEOUT			= 0x00000100,			//CSVファイル出力要求
        VERIFY_SHA1			= 0x01000000,			//SHA-1
        VERIFY_MD5			= 0x02000000,			//MD5
        VERIFY_XX			= 0x04000000,			//xxHash
        VERIFY_SHA2_256		= 0x00100000,			//SHA2-256
        VERIFY_SHA2_512		= 0x00200000,			//SHA2-512
        VERIFY_SHA3_256		= 0x00400000,			//SHA2-512
        VERIFY_SHA3_512		= 0x00800000,			//SHA2-512
    };

    enum SpecialWaitTick {SUSPEND_WAITTICK=0x7fffffff};

    struct Info {
        DWORD	ignoreEvent;	// (I/ )
        Mode	mode;			// (I/ )
        OverWrite overWrite;	// (I/ )
        int		flags;			// (I/ ) 動作モード第一フラグ
        unsigned int bufSize;	// (I/ )
        int		maxOpenFiles;	// (I/ )
        int		maxTransSize;	// (I/ )
        int		maxAttrSize;	// (I/ )
        int		maxDirSize;		// (I/ )
        int		maxLinkHash;	// (I/ ) Dest Hardlink 用 hash table サイズ
        void*	hNotifyWnd;		// (I/ ) MainWindowへのポインタ
        //int	lcid;			// (I/ )
        _int64	fromDateFilter;	// (I/ ) 最古日時フィルタ
        _int64	toDateFilter;	// (I/ ) 最新日時フィルタ
        _int64	minSizeFilter;	// (I/ ) 最低サイズフィルタ
        _int64	maxSizeFilter;	// (I/ ) 最大サイズフィルタ
        char	driveMap[64];	// (I/ ) 物理ドライブマップ
        BOOL	isRenameMode;	// (/O) ...「複製します」ダイアログタイトル用情報（暫定）
                                //           将来的に、情報が増えれば、メンバから切り離し
        int		flags_second;	// (I/ ) 動作モード第二フラグ
    };

    enum Notify { END_NOTIFY, LISTING_NOTIFY,STAT_NOTIFY,CLI_JOBLISTNOTIFY};
    struct Confirm {
        enum Result { CANCEL_RESULT, IGNORE_RESULT, CONTINUE_RESULT };
        const void	*message;		// (I/ )
        BOOL		allow_continue;	// (I/ )
        const void	*path;			// (I/ )
        DWORD		err_code;		// (I/ )
        Result		result;			// ( /O)
    };
    //fastcopyの動作フェーズフラグ
    enum Phase{NORUNNING,MUTEX_WAITING,PRESEARCHING,COPYING,DELETING,VERIFYING};

public:
    FastCopy(void);
    virtual ~FastCopy();

    BOOL RegisterInfo(const PathArray *_srcArray, const PathArray *_dstArray, Info *_info,
                      const PathArray *_includeArray=NULL, const PathArray *_excludeArray=NULL);
    BOOL TakeExclusivePriv(void);
    BOOL AllocBuf(void);
    BOOL Start(TransInfo *ti);
    BOOL End(void);
    BOOL IsStarting(void) { return hReadThread_c != NULL || hWriteThread_c != NULL; }
    BOOL Suspend(void);
    BOOL Resume(void);
    void Aborting(void);
    BOOL WriteErrLog(void *message, int len=-1);
    BOOL IsAborting(void) {return(isAbort); }
    void SetWaitTick(DWORD wait) { waitTick = wait; }
    DWORD GetWaitTick() { return waitTick; }
    BOOL GetTransInfo(TransInfo *ti, BOOL fullInfo=TRUE);
    BOOL GetPhaseInfo(TransInfo *ti);
    int  CompensentIO_size(int req_iosize,FastCopy::FsType src_fstype,FastCopy::FsType dst_fstype);
    void signal_handler(int signum);
    static bool ReadThread(void *fastCopyObj);
    static bool WriteThread(void *fastCopyObj);
    static bool DeleteThread(void *fastCopyObj);
    static bool RDigestThread(void *fastCopyObj);
    static bool WDigestThread(void *fastCopyObj);

    FsType	srcFsType;
    FsType	dstFsType;
    char	srcFsName[32];	//Sourceファイルシステム名の通称
    char	dstFsName[32];	//DestDirファイルシステム名の通称

protected:
    enum Command {
        WRITE_FILE, WRITE_BACKUP_FILE, WRITE_FILE_CONT, WRITE_ABORT, WRITE_BACKUP_ACL,
        WRITE_BACKUP_EADATA, WRITE_BACKUP_ALTSTREAM, WRITE_BACKUP_END, CASE_CHANGE,
        CREATE_HARDLINK, REMOVE_FILES, MKDIR, INTODIR, DELETE_FILES, RETURN_PARENT, REQ_EOF
    };
    enum PutListOpt {
        PL_NORMAL=0x1, PL_DIRECTORY=0x2, PL_HARDLINK=0x4, PL_REPARSE=0x8,
        PL_CASECHANGE=0x10, PL_DELETE=0x20,
        PL_COMPARE=0x10000, PL_ERRMSG=0x20000, PL_NOADD=0x40000,
    };
    enum LinkStatus { LINK_ERR=0, LINK_NONE=1, LINK_ALREADY=2, LINK_REGISTER=3 };
    enum ConfirmErrFlags { CEF_STOP=0x10000000, CEF_NOAPI=0x20000000,
                           CEF_NORMAL=PL_NORMAL,CEF_DIRECTORY=PL_DIRECTORY,CEF_HARDLINK=PL_HARDLINK,CEF_REPARASE=PL_REPARSE};

    struct ReqHeader : public TListObj {	// request header
        Command		command;
        int			bufSize;
        BYTE		*buf;
        int			reqSize;
        FileStat	stat;	// 可変長
    };
    struct ReqBuf {
        BYTE		*buf;
        ReqHeader	*req;
        int			bufSize;
        int			reqSize;
    };

    struct MoveObj {
        _int64		fileID;
        _int64		fileSize;
        enum		Status { START, DONE, ERR } status;
        DWORD		dwFileAttributes;
        BYTE		path[1];
    };
    struct DigestObj {
        _int64		fileID;
        _int64		fileSize;
        DWORD		dwFileAttributes;
        BYTE		digest[SHA3_512SIZE];
        int			pathLen;
        BYTE		path[1];
    };
    struct DigestCalc {
        _int64		fileID;
        _int64		fileSize;
        enum		Status { INIT, CONT, DONE, ERR, FIN };
        DWORD		status;
        int			dataSize;
        BYTE		digest[SHA3_512SIZE];
        BYTE		*data;
        BYTE		path[1]; // さらに dstSector境界後にデータが続く
    };

    struct RandomDataBuf {	// 上書き削除用
        BOOL	is_nsa;
        int		base_size;
        int		buf_size;
        BYTE	*buf[3];
    };

    class TReqList : public TList {	// リクエストキュー
    public:
        TReqList(void) {}
        ReqHeader *TopObj(void) { return (ReqHeader *)TList::TopObj(); }
        ReqHeader *NextObj(ReqHeader *obj) { return (ReqHeader *)TList::NextObj(obj); }
    };

    // 基本情報
    Info		info;		// オプション指定等
    StatHash	hash;
    PathArray	srcArray;
    PathArray	dstArray;

    void	*src;			// src パス格納用
    void	*dst;			// dst パス格納用
    void	*confirmDst;	// 上書き確認調査用
    void	*hardLinkDst;	// ハードリンク用
    int		srcBaseLen;		// src パスの固定部分の長さ
    int		dstBaseLen;		// dst パスの固定部分の長さ
    int		srcPrefixLen;	// \\?\ or \\?\UNC\ の長さ UNIX環境では使用しない
    int		dstPrefixLen;   // \\?\ or \\?\UNC\ の長さ UNIX環境では使用しない
    BOOL	isExtendDir;
    BOOL	isMetaSrc;
    BOOL	isListing;
    BOOL	isListingOnly;
    int		maxStatSize;	// max size of FileStat
    BOOL	enableAcl;
    BOOL	enableXattr;	// XATTR

    // セクタ情報など
    int		srcSectorSize;
    int		dstSectorSize;
    int		sectorSize;
    DWORD	maxReadSize;
    DWORD	maxDigestReadSize;
    DWORD	maxWriteSize;
    //大文字小文字分けて存在することを許すか？
    bool	srcCaseSense;		//true=大文字小文字を区別する false=大文字小文字を区別しない
    bool	dstCaseSense;

    BYTE	src_root[MAX_PATH];

    TotalTrans	total;		// ファイルコピー統計情報
    int phase;				// fastcopy動作状況フラグ
    // FastCopy::Phaseのenum参照のこと

    // filter
    enum		{ REG_FILTER=0x1, DATE_FILTER=0x2, SIZE_FILTER=0x4 };
    DWORD		filterMode;
    enum		{ FILE_EXP, DIR_EXP, MAX_FTYPE_EXP };
    enum		{ INC_EXP, EXC_EXP, MAX_KIND_EXP };
    //RegExpEx	regExp[MAX_FTYPE_EXP][MAX_KIND_EXP];
    RegExp_q	regExp[MAX_FTYPE_EXP][MAX_KIND_EXP];

    // バッファ
    VBuf	mainBuf;		// Read/Write 用 buffer
    VBuf	fileStatBuf;	// src file stat 用 buffer
    QHash<unsigned long,bool> caseHash; //case-sensitive->no case-sensitiveコピー判定用ハッシュ
    VBuf	dirStatBuf;		// src dir stat 用 buffer
    VBuf	dstStatBuf;		// dst dir/file stat 用 buffer
    VBuf	dstStatIdxBuf;	// dstStatBuf 内 entry の index sort 用
    VBuf	mkdirQueueBuf;
    VBuf	dstDirExtBuf;
    VBuf	srcDigestBuf;
    VBuf	dstDigestBuf;
    VBuf	errBuf;
    VBuf	listBuf;
    VBuf	csvfileBuf;		//csvファイル出力専用バッファ
    VBuf	statBuf;		//統計情報用バッファ
    //VBuf	ntQueryBuf;

    // データ転送キュー関連
    TReqList	readReqList;
    TReqList	writeReqList;
    TReqList	rDigestReqList;
    BYTE		*usedOffset;
    BYTE		*freeOffset;
    ReqHeader	*writeReq;
    _int64		nextFileID;
    _int64		errFileID;
    FileStat	**openFiles;
    int			openFilesCnt;

    // スレッド関連
    FastCopy_Thread		*hReadThread_c;
    FastCopy_Thread		*hWriteThread_c;
    FastCopy_Thread		*hRDigestThread_c;
    FastCopy_Thread		*hWDigestThread_c;

    //通算秒取得用
    QTime	*elapse_timer;

    int			nThreads;
    void *rapidcopy_semaphore;                        //RapidCopy同時実行排他制御用セマフォ

    Condition	cv;

    QMutex		errCs;
    QMutex		listCs;
    QMutex		statCs;

    //FastCopy::Suspend,FastCopy::Resume相当実装用の排他
    QMutex	suspend_Mutex;

    //for QRegExp mutex
    QMutex  regexp_Mutex;

    //時間情報
    DWORD	startTick;
    DWORD	endTick;
    DWORD	suspendTick;
    DWORD	waitTick;

    // モード・フラグ類
    BOOL	isAbort;
    BOOL	isSuspend;
    BOOL	isSameDrv;
    BOOL	isSameVol;
    BOOL	isRename;
    BOOL	isNonCSCopy;		//srcがcasesensitiveなのにdstがcasesensitiveじゃないフラグ
    enum	DstReqKind { DSTREQ_NONE=0, DSTREQ_READSTAT=1, DSTREQ_DIGEST=2 } dstAsyncRequest;
    BOOL	dstRequestResult;
    enum	RunMode { RUN_NORMAL, RUN_DIGESTREQ } runMode;
    void	*mainThread_pt;				//mainThreadを示すポインタ
    bool	isgetSemaphore;				//実行時にPOSIX semaphore確保したか？

    // ダイジェスト関連
    TDigest		srcDigest;
    TDigest		dstDigest;
    BYTE		srcDigestVal[SHA3_512SIZE];
    BYTE		dstDigestVal[SHA3_512SIZE];

    DataList	digestList;	// ハッシュ/Open記録
    BOOL IsUsingDigestList() {
        return (info.flags & VERIFY_FILE) && (info.flags & LISTING_ONLY) == 0;
    }
    enum		CheckDigestMode { CD_NOWAIT, CD_WAIT, CD_FINISH };
    DataList	wDigestList;

    // 移動関連
    DataList		moveList;		// 移動
    DataList::Head	*moveFinPtr;	// 書き込み終了ID位置
    TLinkHashTbl	hardLinkList;	// ハードリンク用リスト

    BOOL ReadThreadCore(void);
    BOOL DeleteThreadCore(void);
    BOOL WriteThreadCore(void);
    BOOL RDigestThreadCore(void);
    BOOL WDigestThreadCore(void);

    LinkStatus CheckHardLink(FileStat*,void* path, int, DWORD *data);
    BOOL RestoreHardLinkInfo(DWORD *link_data, void *path, int base_len);

    BOOL CheckAndCreateDestDir(int dst_len);
    BOOL FinishNotify(void);
    BOOL PreSearch(void);
    BOOL PreSearchProc(void *path, int prefix_len, int dir_len);
    BOOL PutMoveList(_int64 fileID, void *path, int path_len, _int64 file_size, DWORD attr,
                     MoveObj::Status status);
    BOOL FlushMoveList(BOOL is_finish=FALSE);
    BOOL ReadProc(int dir_len, BOOL confirm_dir=TRUE);
    BOOL GetDirExtData(ReqBuf *req_buf, FileStat *stat);
    BOOL IsOverWriteFile(FileStat *srcStat, FileStat *dstStat);
    int  MakeRenameName(void *new_fname, int count, void *org_fname);
    BOOL SetRenameCount(FileStat *stat);
    BOOL FilterCheck(const void *path, const void *fname, DWORD attr, _int64 write_time,
                     _int64 file_size,int flags);
    BOOL ReadDirEntry(int dir_len, BOOL confirm_dir);
    int CreateFileWithRetry(void *path, DWORD mode, DWORD share, int sa,
                            DWORD cr_mode, DWORD flg, void* hTempl, int retry_max=10);
    BOOL OpenFileProc(FileStat *stat, int dir_len);
    BOOL OpenFileBackupProc(FileStat *stat, int src_len);
    BOOL OpenFileBackupStreamLocal(FileStat *stat,VBuf *buf,int *in_len);
    BOOL OpenFileBackupAclLocal(FileStat *stat,VBuf *buf,int *in_len);
    BOOL ReadMultiFilesProc(int dir_len);
    BOOL CloseMultiFilesProc(int maxCnt=0);
    void *RestoreOpenFilePath(void *path, int idx, int dir_len);
    BOOL ReadFileWithReduce(int hFile, void *buf, DWORD size, DWORD *reads,
                            void *overwrap);
    BOOL ReadFileXattr(int hFile,char *xattrname, void *buf, DWORD size,DWORD *reads);
    BOOL ReadFileXattrWithReduce(int hFile,char *xattrname, void *buf, DWORD size,DWORD *reads,off_t file_size,_int64 remain_size);
    BOOL ReadFileProc(int start_idx, int *end_idx, int dir_len);
    BOOL DeleteProc(void *path, int dir_len,bool isfirst=false);
    BOOL DeleteDirProc(void *path, int dir_len, void *fname, FileStat *stat,bool isfirst=false);
    BOOL DeleteFileProc(void *path, int dir_len, void *fname, FileStat *stat);

    void SetupRandomDataBuf(void);
    void GenRandomName(void *path, int fname_len, int ext_len);
    BOOL RenameRandomFname(void *org_path, void *rename_path, int dir_len, int fname_len);
    BOOL WriteRandomData(void *path, FileStat *stat, BOOL skip_hardlink);

    BOOL IsSameContents(FileStat *srcStat,FileStat *dstStat);
    BOOL MakeDigest(void *path, VBuf *vbuf, TDigest *digest, BYTE *val, _int64 *fsize=NULL,FileStat *stat=NULL);

    void DstRequest(DstReqKind kind);
    BOOL WaitDstRequest(void);
    BOOL CheckDstRequest(void);
    BOOL ReadDstStat(void);
    BOOL MakeHashTable(void);
    void FreeDstStat(void);
    static int SortStatFunc(const void *stat1, const void *stat2);
    BOOL WriteProc(int dir_len);
    BOOL CaseAlignProc(int dir_len=-1);    //Linux環境では未使用関数
    BOOL WriteDirProc(int dir_len);
    BOOL ExecDirQueue(void);
    BOOL SetDirExtData(FileStat *stat);
    DigestCalc *GetDigestCalc(DigestObj *obj, int require_size);
    BOOL PutDigestCalc(DigestCalc *obj, DigestCalc::Status status);
    BOOL MakeDigestAsync(DigestObj *obj);
    BOOL CheckDigests(CheckDigestMode mode);
    BOOL WriteFileWithReduce(int hFile, void *buf, DWORD size, DWORD *written,
                            void* overwrap);
    BOOL WriteFileXattr(int hFile,char* xattr_name, void*buf,DWORD size,DWORD *written);
    BOOL WriteFileXattrWithReduce(int hFile,char* xattr_name, void*buf,DWORD size,DWORD *written,void *wk_xattrbuf,_int64 file_size,_int64 remain_size);
    BOOL WriteFileProc(int dst_len,int parent_fh=SYSCALL_ERR);
    BOOL WriteFileBackupProc(int fh, int dst_len);
    BOOL ChangeToWriteModeCore(BOOL is_finish=FALSE);
    BOOL ChangeToWriteMode(BOOL is_finish=FALSE);
    BOOL AllocReqBuf(int req_size, _int64 _data_size, ReqBuf *buf);
    BOOL PrepareReqBuf(int req_size, _int64 data_size, _int64 file_id, ReqBuf *buf);
    BOOL SendRequest(Command command, ReqBuf *buf=NULL, FileStat *stat=NULL);
    BOOL RecvRequest(void);
    void WriteReqDone(void);
    void SetErrFileID(_int64 file_id);
    BOOL SetFinishFileID(_int64 _file_id, MoveObj::Status status);

    BOOL InitSrcPath(int idx);
    BOOL InitDeletePath(int idx);
    BOOL InitDstPath(void);
    FsType GetFsType(const void *root_dir, bool *isCaseSensitive,char *fsname_pt);
    bool IsXattrSupport(FsType src,FsType dst);
    bool IsAclSupport(FsType src,FsType dst);
    bool IsNoCaseSensitiveCopy(bool srcCaseSense,bool dstCaseSense){
        //srcがCase-SensitiveかつdstがCase-Sensitiveじゃない？
        return srcCaseSense && !dstCaseSense ? true : false;
    }

    int GetSectorSize(const void *root_dir);
    BOOL IsSameDrive(const void *root1, const void *root2);
    BOOL PutList(void *path, DWORD opt, DWORD lastErr=0, BYTE *digest=NULL,qint64 file_size=SYSCALL_ERR,void* org_path=NULL);
    BOOL PutStat(void *info);

    inline BOOL IsParentOrSelfDirs(void *name) {
        return *(BYTE *)name == '.' && (!strcmpV(name, DOT_V) || !strcmpV(name, DOTDOT_V));
    }
    inline BOOL IsDotStartFile(void *name) {
        //先頭文字が.で始まるかつ後続文字がnull文字じゃない。
        return *(char *)name == '.' && (*(char *)name)+1 != '\0';
    }

    int FdatToFileStat(struct stat *fdat, FileStat *stat, BOOL is_usehash,char *f_name);
    Confirm::Result ConfirmErr(const char *message, const void *path=NULL, DWORD flags=0,const char *count_message=NULL,char *count_message2=NULL);
    BOOL ConvertExternalPath(const void *path, void *buf, int buf_len);
    void apath_to_path_file_a(char *path,char *dir,char *file);
    BOOL Wait(DWORD tick=0);
    BOOL WriteFileBackupAclLocal(int fh,ReqHeader *writereq);
};

//RapidCopyで実行される各種スレッドクラス
class FastCopy_Thread : public QThread{

public:
    FastCopy_Thread(FastCopy *_fastcopyPt,int _thread_type,int _stack_size);
    enum thread_type {RTHREAD,WTHREAD,RDTHREAD,WDTHREAD,STHREAD,DTHREAD};
    void run();

private:
    FastCopy *fastcopy_pt;									//実行したい関数を保持しているfastcopyのインスタンス
    int my_type;											//自Threadの目的. thread_type参照
    int my_stacksize;										//自Thread StackSize
};

struct FastCopyEvent : public QEvent
{
    enum {EventId = QEvent::User};
    explicit FastCopyEvent(unsigned int uMsg_)
        : QEvent(static_cast<Type>(EventId)),
          uMsg(uMsg_) {}

    const unsigned int uMsg;
};
