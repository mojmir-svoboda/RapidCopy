static char *fastcopy_id =
    "@(#)Copyright (C) 2004-2012 H.Shirouzu		fastcopy.cpp	ver2.10";
/* ========================================================================
    Project  Name			: Fast Copy file and directory
    Create					: 2004-09-15(Wed)
    Update					: 2012-06-17(Sun)
    ported update           : 2016-02-28
    Copyright				: H.Shirouzu,Kengo Sawatsu
    Reference				:
    Summary                 : FastCopyコアクラス
    ======================================================================== */

#include "fastcopy.h"
#include <QApplication>
#include <QElapsedTimer>
#include <dirent.h>
#include <fcntl.h>
#include <semaphore.h>
#include <aio.h>
#include <linux/magic.h>

const char *FMT_RENAME_V;		// ".%03d"
const char *FMT_PUTLIST_V;		// listing出力用
const char *FMT_PUTCSV_V;		//csvデータ出力用
const char *FMT_PUTERRCSV_V;	//csvエラーメッセージ出力用
const char *FMT_STRNL_V;
const char *FMT_REDUCEMSG_V;
const char *PLSTR_LINK_V;
const char *PLSTR_REPARSE_V;
const char *PLSTR_REPDIR_V;
const char *PLSTR_COMPARE_V;
const char *PLSTR_CASECHANGE_V;

#define MAX_XATTR		1000
#define REDUCE_SIZE		(1024 * 1024)

// scandirのdirent用判定
#define IsDir3(dt) (dt == DT_DIR)
#define IsFile3(dt) (dt == DT_REG)
#define IsSlnk3(dt) (dt == DT_LNK)
#define IsUNKNOWN(dt) (dt == DT_UNKNOWN)

#define IsReparse_dirent(dt) (dt & DT_LNK)

//stat用
//IsDirはunixのstat準拠に変更
#define IsDir(attr)  ((attr & S_IFMT) == S_IFDIR)
#define IsFile(attr) ((attr & S_IFMT) == S_IFREG)
#define IsSlnk(attr) ((attr & S_IFMT) == S_IFLNK)

//#define IsReparse(attr) (attr & FILE_ATTRIBUTE_REPARSE_POINT)
//IsReparseをシンボリックリンク判定に流用
#define IsReparse(st_mode) (IsSlnk(st_mode))
//delete,moveでの使用
#define IsNoReparseDir(st_mode) (!S_ISLNK(st_mode) && IsDir(st_mode))

//ThreadSuspend代替マクロ
#define CheckSuspend(){suspend_Mutex.lock();suspend_Mutex.unlock();}

//ファイルシステムタイプとファイルシステム名対照表
static FsTable fstable [] = {
    {FastCopy::FSTYPE_NONE,			0,                  FSLOGNAME_UNKNOWN},
    {FastCopy::FSTYPE_ADFS,			ADFS_SUPER_MAGIC,	FSLOGNAME_ADFS},
    {FastCopy::FSTYPE_AFS,			AFS_SUPER_MAGIC,	FSLOGNAME_AFS},
    {FastCopy::FSTYPE_CODA,			CODA_SUPER_MAGIC,	FSLOGNAME_CODA},
    {FastCopy::FSTYPE_EXT2,			EXT2_SUPER_MAGIC,	FSLOGNAME_EXT2},
    {FastCopy::FSTYPE_EXT3,		    EXT3_SUPER_MAGIC,	FSLOGNAME_EXT3},
    {FastCopy::FSTYPE_EXT4,			EXT4_SUPER_MAGIC,	FSLOGNAME_EXT4},
    {FastCopy::FSTYPE_BTRFS,		BTRFS_SUPER_MAGIC,	FSLOGNAME_BTRFS},
    {FastCopy::FSTYPE_F2FS,			F2FS_SUPER_MAGIC,	FSLOGNAME_F2FS},
    {FastCopy::FSTYPE_ISOFS,		ISOFS_SUPER_MAGIC,	FSLOGNAME_ISO},
    {FastCopy::FSTYPE_FAT,          MSDOS_SUPER_MAGIC,	FSLOGNAME_FAT},
    {FastCopy::FSTYPE_NFS,			NFS_SUPER_MAGIC,	FSLOGNAME_NFS},
    {FastCopy::FSTYPE_REISERFS,		REISERFS_SUPER_MAGIC,FSLOGNAME_REISERFS},
    {FastCopy::FSTYPE_SMB,			SMB_SUPER_MAGIC,	FSLOGNAME_SMBFS},
    {FastCopy::FSTYPE_CIFS,			CIFS_MAGIC_NUMBER,	FSLOGNAME_CIFS},
    {FastCopy::FSTYPE_FUSE,			FUSE_SUPER_MAGIC,	FSLOGNAME_FUSE},
    {FastCopy::FSTYPE_HFS,			HFS_SUPER_MAGIC,	FSLOGNAME_HFS},
    {FastCopy::FSTYPE_JFS,			JFS_SUPER_MAGIC,	FSLOGNAME_JFS},
    {FastCopy::FSTYPE_NTFS,			NTFS_SB_MAGIC,	    FSLOGNAME_NTFS},
    {FastCopy::FSTYPE_UDF,			UDF_SUPER_MAGIC,	FSLOGNAME_UDF},
    {FastCopy::FSTYPE_XFS,			XFS_SUPER_MAGIC,	FSLOGNAME_XFS},
    {-1,-1,""}
};


/*=========================================================================
  クラス ： FastCopy
  概  要 ： マルチスレッドなファイルコピー管理クラス
  説  明 ：
  注  意 ：
=========================================================================*/
FastCopy::FastCopy()
{
    hReadThread_c = hWriteThread_c = hRDigestThread_c = hWDigestThread_c = NULL;

    rapidcopy_semaphore = sem_open(FASTCOPY_MUTEX,O_CREAT,S_IRUSR|S_IWUSR,FASTCOPY_MUTEX_INSTANCE);
    if(rapidcopy_semaphore == SEM_FAILED){
        //qDebug() << "sem_open(CREATE) :" << errno;
    }

    src = (void*) new char [MAX_PATHLEN_V + MAX_PATH];
    dst = new char [MAX_PATHLEN_V + MAX_PATH];
    confirmDst = new char [MAX_PATHLEN_V + MAX_PATH];
    FMT_INT_STR_V		= "%d";
    FMT_RENAME_V		= "%.*s(%d)%s";
    FMT_PUTLIST_V		= "%c %s%s%s%s\n";
    FMT_PUTCSV_V		= "%s,\"%s\",%s,%s,%s\n";
    FMT_PUTERRCSV_V		= "%s,\"%s\",,%s,,%s\n";
    FMT_STRNL_V			= "%s\n";
    FMT_REDUCEMSG_V		= "Reduce MaxIO(%c) size (%dMB -> %dMB)";
    PLSTR_LINK_V		= " =>";
    PLSTR_REPARSE_V 	= " ->";
    PLSTR_REPDIR_V  	= "/ ->";
    PLSTR_CASECHANGE_V	= " (CaseChanged)";
    PLSTR_COMPARE_V		= " !!";

    maxStatSize = (MAX_PATH * CHAR_LEN_V) * 2 + offsetof(FileStat, cFileName) + 8;
    waitTick = 0;
    hardLinkDst = NULL;
    elapse_timer = new QTime;
    elapse_timer->start();
    mainThread_pt = QThread::currentThreadId();
    //実行フェーズを「実行前」に変更
    phase = NORUNNING;
}

FastCopy::~FastCopy()
{
    delete [] confirmDst;
    delete [] dst;
    delete [] src;
    //実行待ちしているRapidCopyが一人も存在しない？
    //課題：システムからセマフォを削除するタイミングは
    //「他のRapidCopyがコピー処理を行っていない」かつ「他のRapidCopyが起動していない」が条件となる。
    //けど、「他のRapidCopyが起動していない」を把握する手段がいまのところない。。
    //お行儀悪いけど、とりあえず消さないで放置。
    delete elapse_timer;
}

//機能:指定されたファイルのファイルシステム種別を返す、
//     ついでに対象ファイルシステムがcase-sensitiveかどうかチェックする。
//返り値:FastCopy::FsTypeを参照
FastCopy::FsType FastCopy::GetFsType(const void *root_dir,bool *isCaseSensitive,char *fsname_pt)
{
    struct statfs		fs_t;
    FastCopy::FsType	ret_type = FSTYPE_NONE;

    //課題:FAT32などのnoncase-sensitiveなファイルシステムを検出する手段がないので
    //case-sensitive->noncase-sensitiveへのコピーで一部ファイルが失われる可能性がある。
    //例:コピー元がaaa.TXTとAAA.txtなものをFAT32にコピーすると、どちらかが失われる。。
    //macならpathconf(_PC_CASE_SENSITIVE)で事前に確認できたのになあ。
    //常にケースセンシティブとして扱う
    *isCaseSensitive = true;

    //指定パスをとりあえず調べてみっかー。
    if (statfs((const char*)root_dir,&fs_t) == SYSCALL_ERR) {
        //失敗で強制停止
        ConfirmErr("GetFsType:statfs()", root_dir, CEF_STOP);
        return ret_type;
    }
    for(int i=0;fstable[i].fstype != -1;i++){
        if(fstable[i].f_fstype == (fs_t.f_type)){

            ret_type = (FastCopy::FsType)fstable[i].fstype;
            strncpy(fsname_pt,fstable[i].fstypename_log.toLocal8Bit().data(),sizeof(srcFsName));
            //一回あたれば他に当たる必要ないのでbreak
            break;
        }
    }
    //どのファイルシステムとも一致しない？
    if(ret_type == FSTYPE_NONE){
        //知らないファイルシステムだな
        strcpy(fsname_pt,FSLOGNAME_UNKNOWN);
    }
    return ret_type;
}

//機能:対象ファイルシステムへのread/writeサイズ要求を補正する。
//返り値:補正後のread/write単位(Byte)
int FastCopy::CompensentIO_size(int req_iosize,FastCopy::FsType src_fstype,FastCopy::FsType dst_fstype){

    int return_io_size = req_iosize;

    //コピー元
    switch(src_fstype){
        //こいつらは大きなread/write放り投げてもたぶん大丈夫
        case FSTYPE_NTFS:
        case FSTYPE_SMB:
        case FSTYPE_HFS:
        case FSTYPE_NFS:
        case FSTYPE_UDF:
        case FSTYPE_EXT2:
        case FSTYPE_EXT3:
        case FSTYPE_EXT4:
        case FSTYPE_BTRFS:
        case FSTYPE_F2FS:
        case FSTYPE_REISERFS:
        case FSTYPE_CIFS:
        case FSTYPE_XFS:
            break;

        //1MBに強制ダウン対象
        case FSTYPE_NONE:		//その他把握しようもないやつ。
        case FSTYPE_ISOFS:
        case FSTYPE_FAT:		//古いしなんとなく
        case FSTYPE_ADFS:
        case FSTYPE_AFS:
        case FSTYPE_CODA:
        case FSTYPE_FUSE:
        case FSTYPE_JFS:
        default:				//バグ
            //古いのと実装が予想できないやつらは強制的に1MBにダウン
            return_io_size = FASTCOPY_MINLIMITIOSIZE;
            break;
    }
    //コピー先
    switch(dst_fstype){
        case FSTYPE_NTFS:
        case FSTYPE_SMB:
        case FSTYPE_HFS:
        case FSTYPE_NFS:
        case FSTYPE_UDF:
        case FSTYPE_EXT2:
        case FSTYPE_EXT3:
        case FSTYPE_EXT4:
        case FSTYPE_BTRFS:
        case FSTYPE_F2FS:
        case FSTYPE_REISERFS:
        case FSTYPE_CIFS:
        case FSTYPE_XFS:
            break;
        //1MBに強制ダウン対象
        case FSTYPE_NONE:		//その他把握しようもないやつ。
        case FSTYPE_ISOFS:
        case FSTYPE_FAT:		//古いしなんとなく
        case FSTYPE_ADFS:
        case FSTYPE_AFS:
        case FSTYPE_CODA:
        case FSTYPE_FUSE:
        case FSTYPE_JFS:
        default:				//バグ
            //古いのと実装が予想できないやつらは強制的に1MBにダウン
            return_io_size = FASTCOPY_MINLIMITIOSIZE;
            break;
    }
    //qDebug() << "return_io_size = " << return_io_size;
    return return_io_size;
}

//機能:Xattrコピー機能が動作可能か判定する
//返り値:true:xattrコピー有効なファイルシステム同士である。
//		false:xattrコピーが有効なファイルシステム同士ではない。
bool FastCopy::IsXattrSupport(FastCopy::FsType src_fstype,FastCopy::FsType dst_fstype)
{
    bool ret = true;
    //srcのファイルシステムタイプチェック
    switch(src_fstype){
        case FSTYPE_EXT2:
        case FSTYPE_EXT3:
        case FSTYPE_EXT4:
        case FSTYPE_JFS:
        case FSTYPE_HFS:
        case FSTYPE_REISERFS:
        case FSTYPE_XFS:
        //CIFSに対してのxattrは対象システムのプロトコルスタックによって？
        //変換されることを期待する。(だめもと)
        case FSTYPE_SMB:
            break;
        //上記以外はxattrサポートないはずなので余計なことしないよ
        default:
            return false;
    }
    //dstのファイルシステムタイプチェック
    switch(dst_fstype){
        case FSTYPE_EXT2:
        case FSTYPE_EXT3:
        case FSTYPE_EXT4:
        case FSTYPE_JFS:
        case FSTYPE_HFS:
        case FSTYPE_REISERFS:
        case FSTYPE_XFS:
        case FSTYPE_SMB:
            break;
        default:
            return false;
    }
    return ret;
}

//機能:ACLコピー機能が動作可能か判定する
//返り値: true:ACLコピー有効なファイルシステム同士である。
//		 false:ACLコピーが有効なファイルシステム同士ではない。
bool FastCopy::IsAclSupport(FastCopy::FsType src_fstype,FastCopy::FsType dst_fstype)
{
    bool ret = true;

    //srcのファイルシステムタイプチェック
    switch(src_fstype){
        case FSTYPE_EXT2:
        case FSTYPE_EXT3:
        case FSTYPE_EXT4:
        case FSTYPE_JFS:
        case FSTYPE_HFS:
        case FSTYPE_REISERFS:
        case FSTYPE_XFS:
        case FSTYPE_NFS:            //v4 aclだけだけどね
        //CIFSに対してのACLはプロトコルスタックによって？
        //変換されることを期待する。(だめもと)
        case FSTYPE_SMB:
            break;
        //上記以外はaclサポートないはずなので余計なことしないよ
        default:
            return false;
    }
    //dstのファイルシステムタイプチェック
    switch(src_fstype){
        case FSTYPE_EXT2:
        case FSTYPE_EXT3:
        case FSTYPE_EXT4:
        case FSTYPE_JFS:
        case FSTYPE_HFS:
        case FSTYPE_REISERFS:
        case FSTYPE_XFS:
        case FSTYPE_NFS:
        case FSTYPE_SMB:
            break;
        //上記以外はaclサポートないはずなので余計なことしないよ
        default:
            return false;
    }
    return ret;
}

//機能:ファイルシステムごとの最小ioサイズ設定値を受け取る。
//返り値:ファイルシステム固有の1ブロックの長さ(単位:byte)
int FastCopy::GetSectorSize(const void *root_dir)
{
    //DWORD	spc, bps, fc, cl;

    struct statfs	fs_t;

    if (statfs((const char*)root_dir,&fs_t) == SYSCALL_ERR) {
        ConfirmErr("GetSectorSize:statfs", root_dir, CEF_STOP);
        //ERRNO_OUT(errno,"GetSectorSize:statfs");
        return	OPT_SECTOR_SIZE;
    }

    return ((int)fs_t.f_bsize);
}

//機能:同じ論理デバイスかどうかをチェックする。
//返り値: true:同一の論理デバイスである。 false:同一の論理デバイスではない。
BOOL FastCopy::IsSameDrive(const void *root1, const void *root2)
{

    struct stat stat_1;							//for root1 stat
    struct stat stat_2;							//for root2 stat

    //2つのパスにstat出す
    if((stat((const char*)root1,&stat_1) == SYSCALL_ERR)
        || (stat((const char*)root2,&stat_2) == SYSCALL_ERR)){
        //どっちかをstatで調査できなかった。
        ConfirmErr("IsSameDrive:stat",NULL);
        return(false);
    }

    //同じdev？
    if(stat_1.st_dev == stat_2.st_dev){
        //同じだね
        return true;
    }
    else{
        //違うみたいだね
        return false;
    }
}

BOOL FastCopy::InitDstPath(void)
{
    int		attr;
    char	wbuf[MAX_PATH+STR_NULL_GUARD];
    void	*buf = wbuf;
    const void	*org_path = dstArray.Path(0), *dst_root;
    struct stat wk_stat;			//stat結果受け取り用

    //realpathでパスが有効かどうかチェック
    if(realpath((const char *)org_path,(char*)dst) == NULL){
        ConfirmErr("InitDstPath:realpath()",org_path,CEF_STOP);
        return false;
    }

    // realpathで正規化すると、ケツの'/'とられちゃう。
    //とりあえずstat出してみるか
    attr = stat((const char *)dst,&wk_stat);

    if(attr == SYSCALL_ERR){
        if(errno == ENOENT){
            //dstが存在しないため、調査の必要がない
            info.overWrite = BY_ALWAYS;
        }
        else{
            ConfirmErr("InitDstPath:stat()",(char*) dst, CEF_STOP);
            return false;
        }
    }
    else{
        //directory?
        if(IsDir(wk_stat.st_mode)){
            // 入力したdstのオリジナルパスの末尾に"/"はついていたか？
            // UTF-8以外の事は今のところなにも考えてないぞ
            if(*((char*)org_path + (strlen((char*)org_path) - 1)) == '/'){
                //qDebug("strlen = %d",(strlen((char*)org_path) - 1));
                /* dstに"/"を付与しなおし */
                strcat((char*)dst,"/");
            }
        }
    }

    strcpy((char*)buf,(char*)dst);

    dstArray.RegisterPath(buf);

    dst_root = dstArray.Path(dstArray.Num() -1);

    strcpyV(buf, dst);
    MakePathV(dst, buf, EMPTY_STR_V);
    // src自体をコピーするか（dst末尾に / がついている or 複数指定）
    isExtendDir = strcmpV(buf, dst) == 0 || srcArray.Num() > 1 ? TRUE : FALSE;

    //UNCのためのprefix("¥¥")不要なので0設定
    dstPrefixLen = 0;
    dstBaseLen = strlenV(dst);

    //各種ファイルシステム情報取得
    // セクタサイズ取得
    dstSectorSize = GetSectorSize(dst_root);
    //ファイルシステムの取得
    dstFsType = GetFsType(dst_root,&dstCaseSense,dstFsName);
    //Linuxの場合は無条件にpreallocate有効
    info.flags_second |= ENABLE_PREALLOCATE;
    // 差分コピー用dst先ファイル確認
    strcpyV(confirmDst, dst);


    return	TRUE;
}

BOOL FastCopy::InitSrcPath(int idx)
{
    int			attr;
    BYTE		src_root_cur[MAX_PATH];
    WCHAR		wbuf[MAX_PATH];
    void		*buf = wbuf, *fname = NULL;
    const void	*dst_root = dstArray.Path(dstArray.Num() -1);
    const void	*org_path = srcArray.Path(idx);

    DWORD		cef_flg = IsStarting() ? 0 : CEF_STOP;	//JobListモード時は強制停止しない
    struct stat wk_stat;								//stat結果受け取り用
    char	wk_pathname[MAX_PATH + STR_NULL_GUARD];		// パス名のワーク
    char	wk_filename[MAX_PATH + STR_NULL_GUARD];		// ファイル名のワーク
    void	*fullpath = NULL;				//realpathで受け取る正規化後のフルパス(要解放)

    //絶対パスチェック
    fullpath = realpath((const char *)org_path,NULL);

    //それっぽくエラー処理
    if(fullpath == NULL){
        //srcの複数入力の2個目以降で、既にコピーが実行されている場合はとりあえず続行させる。
        //課題:InitSrcPathで先頭のsrc以外がエラーになると制御の都合上InitSrcPathが2回呼ばれるので、
        //見た目がちょっとかっちょ悪い。。
        ConfirmErr("InitSrcPath:realpath()", org_path,cef_flg);
        //途中エラーの時、その他エラーが一個も発生してないとエラー検出できずに色変化しないので
        //とりあえずdirとしてエラー加算しておく。
        total.errDirs++;
        return false;
    }

    //フルパスをパス名と末尾ファイル名の二つに分離
    apath_to_path_file_a((char*)fullpath,&wk_pathname[0],&wk_filename[0]);

    strcpy((char*)src,(char*)fullpath);

    strcpy((char*)src_root_cur,(char*)src);

    isMetaSrc = FALSE;

    attr = stat((const char *)src,&wk_stat);

    if(attr == SYSCALL_ERR){
        isMetaSrc = TRUE;
    }
    else if(IsDir(wk_stat.st_mode)){

        strcpyV(buf, src);
        MakePathV(src, buf, EMPTY_STR_V); // パス合成で'/'を強制付与
        //srcの指定複数またはdstのけつに'/'が指定されてる？ */
        if (isExtendDir){
            SetChar(src, strlenV(src) - 1, 0);	// 末尾に '/' を付けるのをやめる
                                                // (Sourceのディレクトリごとコピーする)
        }
        else{
            isMetaSrc = TRUE;					// 末尾に '/' を付けたままにしとく
                                                // (Sourceの中身をコピーする)
        }
    }

    //確認用dst生成のため、src末尾のフォルダ名をこぴっておく。
    //GetFullPathNameの機能を自力で補完。
    fname = wk_filename;

    strcpy((char*)buf,(char*)src);

    //UNCのためのprefixは不要なので無条件0設定する
    srcPrefixLen = 0;

    // 確認用dst生成
    // Sourceのディレクトリごとコピーする？
    if(isExtendDir){
        //dst確認先にsourceのフォルダ名をくっつける。
        strcpyV(MakeAddr(confirmDst, dstBaseLen), fname);
    }
    else{
        //sourceのディレクトリごとコピーしない。dst先にそのままコピーする
    }

    // 同一パスでないことの確認
    if (lstrcmpiV(buf, confirmDst) == 0) {
        if (info.mode != DIFFCP_MODE || (info.flags & SAMEDIR_RENAME) == 0) {

            ConfirmErr((char*)GetLoadStrV(IDS_SAMEPATHERR), MakeAddr(confirmDst, dstBaseLen),
                       CEF_STOP|CEF_NOAPI|CEF_DIRECTORY);
            return	FALSE;
        }

        //FindFirstFile用の末尾'*'はいらないのよん。
        //strcpyV(MakeAddr(confirmDst, dstBaseLen), ASTERISK_V);
        //"コピー元と同一フォルダへのコピーはリネームして動作継続"機能有効
        isRename = TRUE;
    }
    else{
        isRename = FALSE;
    }

    if (info.mode == MOVE_MODE && IsNoReparseDir(wk_stat.st_mode)) {	// 親から子への移動は認めない
        int	end_offset = 0;
        if (GetChar(fname, 0) == '*' || attr == 0xffffffff) {
            SetChar(fname, 0, 0);
            end_offset = 1;
        }
        int	len = ::strlenV((char*)buf);
        if (strnicmpV(buf, confirmDst, len) == 0) {
            DWORD ch = GetChar(confirmDst, len - end_offset);
            if (ch == 0 || ch == '/') {
                ConfirmErr((char*)GetLoadStrV(IDS_PARENTPATHERR), MakeAddr(buf, srcPrefixLen),
                           CEF_STOP|CEF_NOAPI|CEF_DIRECTORY);
                return	FALSE;
            }
        }
    }

    //srcの指定複数またはdstのけつに'/'が指定されてる？
    //またはコピー元パスの総数は1かつファイル指定？
    if(isExtendDir || (srcArray.Num() == 1 && IsFile(wk_stat.st_mode))){
        // コピー開始地点は一個上のディレクトリからにするため、末尾ファイル名を潰す
        // ex:/Volumes/src/src_2/ -> /Volumes/src/
        SetChar(buf,(strlenV(buf) - strlenV(wk_filename)),0);
    }
    //コピー開始地点の文字列長設定
    srcBaseLen = strlenV(buf);

    // src ファイルシステム情報取得
    if (lstrcmpiV(src_root_cur, src_root)) {
        srcSectorSize = GetSectorSize(src_root_cur);
        srcFsType = GetFsType(src_root_cur,&srcCaseSense,srcFsName);

        sectorSize = max(srcSectorSize, dstSectorSize);		// 大きいほうに合わせる
        sectorSize = max(sectorSize, MIN_SECTOR_SIZE);
        // 同一物理ドライブかどうかの調査
        if (info.flags & FIX_SAMEDISK){
            //同じ物理ドライブなのね
            isSameDrv = TRUE;
        }
        else if (info.flags & FIX_DIFFDISK){
            //違う物理ドライブのつもりでいくのね
            isSameDrv = FALSE;
        }
        else{
            //自分で確認するのね
            isSameDrv = IsSameDrive(src_root_cur, dst_root);
        }
        if (info.mode == MOVE_MODE){
            isSameVol = strcmpV(src_root_cur, dst_root);
        }
    }

    // ACLコピー機能対応チェック
    enableAcl = (info.flags & WITH_ACL) && IsAclSupport(srcFsType,dstFsType);
    // XATTRコピー機能対応チェック
    enableXattr = (info.flags & WITH_XATTR) && IsXattrSupport(srcFsType,dstFsType);

    //case-sensitiveなsrcからnon-case-sensitiveなdstにコピろうとしてないかチェック
    isNonCSCopy = IsNoCaseSensitiveCopy(srcCaseSense,dstCaseSense);

    strcpyV(src_root, src_root_cur);

    // 最大転送サイズ
    DWORD tmpSize = isSameDrv ? info.bufSize : info.bufSize / 4;
    maxReadSize = min(tmpSize, maxReadSize);
    maxReadSize = max(MIN_BUF, maxReadSize);
    maxReadSize = maxReadSize / BIGTRANS_ALIGN * BIGTRANS_ALIGN;
    maxWriteSize = min(maxReadSize, maxWriteSize);
    maxDigestReadSize = min(maxReadSize, maxDigestReadSize);

    if(fullpath != NULL){
        free(fullpath);
    }
    return	TRUE;

}

BOOL FastCopy::InitDeletePath(int idx)
{
    int			attr;
    char		wbuf[MAX_PATH];
    void		*buf = wbuf;
    const void	*org_path = srcArray.Path(idx);
    BYTE		dst_root[MAX_PATH];
    DWORD		cef_flg = IsStarting() ? 0 : CEF_STOP;
    struct		stat	wk_stat;				//stat結果受け取り用
    char		wk_pathname[MAX_PATH + STR_NULL_GUARD];	// パス名のワーク
    char		wk_filename[MAX_PATH + STR_NULL_GUARD];	// ファイル名のワーク
    void		*fullpath = NULL;				//realpathで受け取る正規化後のフルパス(要解放)

    fullpath = realpath((const char *)org_path,NULL);

    //それっぽくエラー処理
    if(fullpath == NULL){
        return	ConfirmErr("InitDeletePath:realpath()", org_path, cef_flg), FALSE;
    }

    apath_to_path_file_a((char*)fullpath,&wk_pathname[0],&wk_filename[0]);

    strcpy((char*)dst,(char*)fullpath);

    attr = stat((char*)dst,&wk_stat);

    if(attr != SYSCALL_ERR){

        strcpy((char*)dst_root,(char*)dst);

        if (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA)) {
            dstSectorSize = GetSectorSize(dst_root);
            dstFsType = GetFsType(dst_root,&dstCaseSense,dstFsName);
            //nbMinSize = dstFsType == FSTYPE_NTFS ? info.nbMinSizeNtfs : info.nbMinSizeFat;
        }
    }

    isMetaSrc = FALSE;

    if (attr == SYSCALL_ERR) {
        isMetaSrc = TRUE;
    }
    else if (IsDir(wk_stat.st_mode)) {
        strcpyV(buf, dst);
        MakePathV(dst, buf, EMPTY_STR_V);
        isMetaSrc = TRUE;
    }

    dstPrefixLen = 0;
    dstBaseLen = strlenV(dst);

    if (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA)) {
        strcpyV(confirmDst, dst);	// for renaming before deleting

        // 最大転送サイズ
        maxWriteSize = min((DWORD)info.bufSize, maxWriteSize);
        maxWriteSize = max(MIN_BUF, maxWriteSize);
    }
    
    if(fullpath != NULL){
        free(fullpath);
    }

    return	TRUE;
}

BOOL FastCopy::RegisterInfo(const PathArray *_srcArray, const PathArray *_dstArray, Info *_info,
    const PathArray *_includeArray, const PathArray *_excludeArray)
{;
    info = *_info;

    isAbort = FALSE;
    isRename = FALSE;
    isMetaSrc = FALSE;
    filterMode = 0;
    isListingOnly = (info.flags & LISTING_ONLY) ? TRUE : FALSE;
    isListing = (info.flags & LISTING) || isListingOnly ? TRUE : FALSE;
    endTick = 0;

    SetChar(src_root, 0, 0);

    //Listing時は事前予測強制OFFだが、VERIFYONLYモードでは特別に有効とする。
    if (isListingOnly && info.mode != VERIFY_MODE) info.flags &= ~PRE_SEARCH;

    // 最大転送サイズ上限（InitSrcPath で再設定）
    maxReadSize = maxWriteSize = maxDigestReadSize = info.maxTransSize;

    // filter
    PathArray	pathArray[] = { *_includeArray, *_excludeArray };
    void		*path = NULL;

    //regexpクラス準備
    for (int kind=0; kind < MAX_KIND_EXP; kind++) {
        for (int ftype=0; ftype < MAX_FTYPE_EXP; ftype++){
            //INC_EXPは何も指定ないときはファイル・ディレクトリ共にすべて許可指定
            if(kind == INC_EXP){
                regExp[kind][ftype].Init("*");
            }
            //EXC_EXP
            else{
                regExp[kind][ftype].Init();
            }
        }
        for (int idx=0; idx < pathArray[kind].Num(); idx++) {
            int		len = lstrlenV(path = pathArray[kind].Path(idx)), targ = FILE_EXP;
            if(GetChar(path, len - 1) == '\\'){
                SetChar(path, len - 1, 0);
                targ = DIR_EXP;
            }
            if (!regExp[kind][targ].RegisterWildCard(path, RegExp_q::CASE_SENSE)){
                return	ConfirmErr("Bad or Too long wildcard string",
                            path, CEF_STOP|CEF_NOAPI), FALSE;
            }
        }
        regExp[kind][FILE_EXP].RemoveDuplicateEntry();
        regExp[kind][DIR_EXP].RemoveDuplicateEntry();
    }


    if(path) filterMode |= REG_FILTER;
    if(info.fromDateFilter != 0  || info.toDateFilter  != 0)  filterMode |= DATE_FILTER;
    if(info.minSizeFilter  != -1 || info.maxSizeFilter != -1) filterMode |= SIZE_FILTER;

    if (!isListingOnly &&
        (info.mode != DELETE_MODE || (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))) &&
        (info.bufSize < MIN_BUF * 2))
        return	ConfirmErr("Too large or small Main Buffer.", NULL, CEF_STOP|CEF_NOAPI), FALSE;

    if((info.flags & (DIR_REPARSE|FILE_REPARSE)) && (info.mode == MOVE_MODE || info.mode == DELETE_MODE)){
        return	ConfirmErr("Illegal Flags (symlink)", NULL, CEF_STOP|CEF_NOAPI), FALSE;
    }

    // command
    if(info.mode == DELETE_MODE){
        srcArray = *_srcArray;
        if (InitDeletePath(0) == FALSE)
            return	FALSE;
    }
    else{
        srcArray = *_srcArray;
        dstArray = *_dstArray;

        //DriveMap必要なし
        //driveMng.SetDriveMap(info.driveMap);

        if (InitDstPath() == FALSE)
            return	FALSE;
        if (InitSrcPath(0) == FALSE)
            return	FALSE;
    }
    _info->isRenameMode = isRename;

    CheckSuspend();
    return	!isAbort;
}

BOOL FastCopy::TakeExclusivePriv(void)
{
    bool ret;
    //同時実行制御用プロセス間排他の取得
    if(sem_trywait((sem_t*)rapidcopy_semaphore) == 0){
        //qDebug() << "sem_trywait() success.";
        ret = true;
        isgetSemaphore = true;
    }
    else{
        ret = false;
        isgetSemaphore = false;
        phase = MUTEX_WAITING;
    }
    return ret;
}

BOOL FastCopy::AllocBuf(void)
{
    int		allocSize = isListingOnly ? MAX_LIST_BUF : info.bufSize + PAGE_SIZE * 4;
    BOOL	need_mainbuf = info.mode != DELETE_MODE ||
                    ((info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA)) && !isListingOnly);

    // メインリングバッファ確保
    if (need_mainbuf && mainBuf.AllocBuf(allocSize) == FALSE) {
        return	ConfirmErr("Can't alloc memory(mainBuf)", NULL, CEF_STOP), FALSE;
    }
    usedOffset = freeOffset = mainBuf.Buf();	// リングバッファ用オフセット初期化

    if (errBuf.AllocBuf(MIN_ERR_BUF, MAX_ERR_BUF) == FALSE) {
        return	ConfirmErr("Can't alloc memory(errBuf)", NULL, CEF_STOP), FALSE;
    }
    if (isListing) {
        if (listBuf.AllocBuf(MIN_PUTLIST_BUF, MAX_PUTLIST_BUF) == FALSE){
            return	ConfirmErr("Can't alloc memory(listBuf)", NULL, CEF_STOP), FALSE;
        }
        //listBuf確保成功
        else{
            //CSVファイル出力要求あり？
            if(info.flags_second & FastCopy::CSV_FILEOUT){
                //csv出力用バッファ確保
                if(!csvfileBuf.AllocBuf(MIN_PUTLIST_BUF,MAX_PUTLIST_BUF)){
                    return	ConfirmErr("Can't alloc memory(csvfileBuf)", NULL, CEF_STOP), FALSE;
                }
            }
        }
    }
    if (info.mode == DELETE_MODE) {
        if (need_mainbuf) SetupRandomDataBuf();
        return	TRUE;
    }
    if(info.flags_second & FastCopy::STAT_MODE){
        if(statBuf.AllocBuf(MIN_PUTLIST_BUF,MAX_PUTLIST_BUF) == false){
            return ConfirmErr("Can't alloc memory(statBuf)", NULL, CEF_STOP), FALSE;
        }
    }

    openFiles = new FileStat *[info.maxOpenFiles + MAX_XATTR]; /* for xattr */
    openFilesCnt = 0;

    if (info.flags & RESTORE_HARDLINK) {
        hardLinkDst = new char [MAX_PATHLEN_V + MAX_PATH];
        memcpy(hardLinkDst, dst, dstBaseLen * CHAR_LEN_V);
        if (!hardLinkList.Init(info.maxLinkHash) || !hardLinkDst)
            return	ConfirmErr("Can't alloc memory(hardlink)", NULL, CEF_STOP), FALSE;
    }

    if (IsUsingDigestList()) {
        digestList.Init(MIN_DIGEST_LIST, MAX_DIGEST_LIST, MIN_DIGEST_LIST);
        int require_size = (maxReadSize + PAGE_SIZE) * 4;
        if (require_size > allocSize) require_size = allocSize;

        maxDigestReadSize = (require_size / 4 - PAGE_SIZE) / BIGTRANS_ALIGN * BIGTRANS_ALIGN;
        maxDigestReadSize = max(MIN_BUF, maxDigestReadSize);

        if (!wDigestList.Init(MIN_BUF, require_size, PAGE_SIZE))
            return	ConfirmErr("Can't alloc memory(digest)", NULL, CEF_STOP), FALSE;
    }


    if (info.flags & VERIFY_FILE) {

        int digestreq = info.flags_second & VERIFY_XX ? TDigest::XX :
                         info.flags_second & VERIFY_SHA1 ? TDigest::SHA1 :
                         info.flags_second & VERIFY_SHA2_256 ? TDigest::SHA2_256 :
                         info.flags_second & VERIFY_SHA2_512 ? TDigest::SHA2_512 :
                         info.flags_second & VERIFY_SHA3_256 ? TDigest::SHA3_256 :
                         info.flags_second & VERIFY_SHA3_512 ? TDigest::SHA3_512 : TDigest::MD5;

        srcDigest.Init(TDigest::Type(digestreq));
        dstDigest.Init(TDigest::Type(digestreq));

        if (isListingOnly) {
            srcDigestBuf.AllocBuf(0, maxReadSize);
            dstDigestBuf.AllocBuf(0, maxReadSize);
        }
    }

    if (info.mode == MOVE_MODE) {
        if (!moveList.Init(MIN_MOVEPATH_LIST, MAX_MOVEPATH_LIST, MIN_MOVEPATH_LIST))
            return	ConfirmErr("Can't alloc memory(moveList)", NULL, CEF_STOP), FALSE;
    }
    //ntQueryバッファは廃止。メインバッファに分割コピーしてるよ。
    // src/dst dir-entry/attr 用バッファ確保
    dirStatBuf.AllocBuf(MIN_ATTR_BUF, info.maxDirSize);
    if(info.flags & SKIP_EMPTYDIR)
        mkdirQueueBuf.AllocBuf(MIN_MKDIRQUEUE_BUF, MAX_MKDIRQUEUE_BUF);
    dstDirExtBuf.AllocBuf(MIN_DSTDIREXT_BUF, MAX_DSTDIREXT_BUF);

    fileStatBuf.AllocBuf(MIN_ATTR_BUF, info.maxAttrSize);
    dstStatBuf.AllocBuf(MIN_ATTR_BUF, info.maxAttrSize);
    dstStatIdxBuf.AllocBuf(MIN_ATTRIDX_BUF, MAX_ATTRIDX_BUF(info.maxAttrSize));

    if(!fileStatBuf.Buf() || !dirStatBuf.Buf() || !dstStatBuf.Buf() || !dstStatIdxBuf.Buf()
       ||(info.flags & SKIP_EMPTYDIR) && !mkdirQueueBuf.Buf() || !dstDirExtBuf.Buf()){
        return	ConfirmErr("Can't alloc memory(misc buf)", NULL, CEF_STOP), FALSE;
    }
    return	TRUE;
}

BOOL FastCopy::Start(TransInfo *ti)
{

    memset(&total, 0, sizeof(total));
    if(info.flags & PRE_SEARCH)
        total.isPreSearch = TRUE;

    isAbort = FALSE;
    writeReq = NULL;
    isSuspend = FALSE;
    dstAsyncRequest = DSTREQ_NONE;
    nextFileID = 1;
    errFileID = 0;
    openFiles = NULL;
    moveFinPtr = NULL;
    runMode = RUN_NORMAL;
    nThreads = info.mode == DELETE_MODE ? 1 : IsUsingDigestList() ? 4 : 2;
    hardLinkDst = NULL;

    cv.Initialize(nThreads);
    readReqList.Init();
    writeReqList.Init();
    rDigestReqList.Init();

    if (AllocBuf() == FALSE) goto ERR;

    //開始起点時刻(ミリ秒)の取得
    startTick = elapse_timer->elapsed();
    if (ti) GetTransInfo(ti, FALSE);

    //モードに応じて各スレッドを起動
    if (info.mode == DELETE_MODE) {

        hReadThread_c = new FastCopy_Thread(this,FastCopy_Thread::DTHREAD,FASTCOPY_STACKSIZE);
        hReadThread_c->setObjectName("DeleteThread");
        hReadThread_c->start();

        return	TRUE;
    }

    hWriteThread_c = new FastCopy_Thread(this,FastCopy_Thread::WTHREAD,FASTCOPY_STACKSIZE);
    hWriteThread_c->setObjectName("WriteThread");
    hWriteThread_c->start();
    hReadThread_c = new FastCopy_Thread(this,FastCopy_Thread::RTHREAD,FASTCOPY_STACKSIZE);
    hReadThread_c->setObjectName("ReadThread");
    hReadThread_c->start();
    //ハッシュチェック有効？
    if(IsUsingDigestList()){

        //digest用スレッドを生成
        hRDigestThread_c = new FastCopy_Thread(this,FastCopy_Thread::RDTHREAD,FASTCOPY_STACKSIZE);
        hRDigestThread_c->setObjectName("ReadDigestThread");
        hRDigestThread_c->start();

        hWDigestThread_c = new FastCopy_Thread(this,FastCopy_Thread::WDTHREAD,FASTCOPY_STACKSIZE);
        hWDigestThread_c->setObjectName("WriteDigestThread");
        hWDigestThread_c->start();
    }
    return	TRUE;

ERR:
    End();
    return	FALSE;
}



/*=========================================================================
  関  数 ： ReadThread
  概  要 ： Read 処理
  説  明 ：
  注  意 ：
=========================================================================*/
bool FastCopy::ReadThread(void *fastCopyObj)
{
    return	((FastCopy *)fastCopyObj)->ReadThreadCore();
}

BOOL FastCopy::ReadThreadCore(void)
{
    BOOL	isSameDrvOld;
    int		done_cnt = 0;

    //事前時間予測チェックonならpresearch呼び出し
    if (info.flags & PRE_SEARCH){
        //実行フェーズを「予測中」に設定
        phase = PRESEARCHING;
        PreSearch();
    }
    //現在のフェーズを「コピー中」に変更
    phase = COPYING;

    for (int i=0; i < srcArray.Num(); i++) {
        if (InitSrcPath(i)) {
            if (done_cnt >= 1 && isSameDrvOld != isSameDrv) {
                ChangeToWriteMode();
            }
            ReadProc(srcBaseLen, info.overWrite == BY_ALWAYS ? FALSE : TRUE);
            isSameDrvOld = isSameDrv;
            done_cnt++;
        }
        CheckSuspend();
        if (isAbort)
            break;
    }
    SendRequest(REQ_EOF);

    if (isSameDrv) {
        ChangeToWriteMode(TRUE);
    }
    CheckSuspend();
    if (info.mode == MOVE_MODE && !isAbort) {
        FlushMoveList(TRUE);
    }
    //無限待ち
    hWriteThread_c->wait();

    if(IsUsingDigestList()){
        hRDigestThread_c->wait();
        hWDigestThread_c->wait();
    }

    // Thread関連オブジェクトの破棄(closeHandle()相当の処理ね)
    // 処理上の意味あんまりはないけど、最後に作成した奴から破棄する
    if(IsUsingDigestList()){

        delete hWDigestThread_c;
        delete hRDigestThread_c;
        // null初期化
        hWDigestThread_c = NULL;
        hRDigestThread_c = NULL;
    }

    delete hWriteThread_c;
    hWriteThread_c = NULL;

    // ReadThread本人はイベントディスパッチャ延長でメインスレッドから消し込む
    FinishNotify();

    return	TRUE;
}

BOOL FastCopy::PreSearch(void)
{
    BOOL	is_delete = info.mode == DELETE_MODE;
    BOOL	(FastCopy::*InitPath)(int) = is_delete ?
                &FastCopy::InitDeletePath : &FastCopy::InitSrcPath;
    void	*&path = is_delete ? dst : src;
    int		&prefix_len = is_delete ? dstPrefixLen : srcPrefixLen;
    int		&base_len = is_delete ? dstBaseLen : srcBaseLen;
    BOOL	ret = TRUE;

    for (int i=0; i < srcArray.Num(); i++) {
        if ((this->*InitPath)(i)) {
            if (!PreSearchProc(path, prefix_len, base_len))
                ret = FALSE;
        }
        CheckSuspend();
        if (isAbort)
            break;
    }
    total.isPreSearch = FALSE;
    startTick = elapse_timer->elapsed();
    CheckSuspend();
    return	ret && !isAbort;
}

BOOL FastCopy::FilterCheck(const void *path, const void *fname, DWORD attr, _int64 write_time,_int64 file_size,int flags)
{
    int	targ;
    if(flags == FASTCOPY_FILTER_FLAGS_DIRENT){
        targ = IsDir3(attr) ? DIR_EXP : FILE_EXP;
    }
    else if(FASTCOPY_FILTER_FLAGS_STAT == FASTCOPY_FILTER_FLAGS_STAT){
        targ = IsDir(attr) ? DIR_EXP : FILE_EXP;
    }

    if(targ == FILE_EXP){

        if(write_time < info.fromDateFilter
           || (info.toDateFilter && write_time > info.toDateFilter)){
            return	FALSE;
        }
        if (file_size < info.minSizeFilter
           || (info.maxSizeFilter != -1 && file_size > info.maxSizeFilter)){
            return	FALSE;
        }
    }

    if((filterMode & REG_FILTER) == 0)
        return	TRUE;

    regexp_Mutex.lock();
    if(regExp[EXC_EXP][targ].IsMatch(fname)){
        regexp_Mutex.unlock();
        return	FALSE;
    }

    if(regExp[INC_EXP][targ].IsMatch(fname)){
        regexp_Mutex.unlock();
        return	TRUE;
    }
    regexp_Mutex.unlock();

    return FALSE;
}

BOOL FastCopy::PreSearchProc(void *path, int prefix_len, int dir_len)
{

    BOOL ret = TRUE;
    int rc = SYSCALL_ERR;
    struct stat fdat;
    struct dirent ** namelist = NULL;	// 当該ディレクトリのファイルエントリリスト
    int cdir_fnum = 0;					// 当該ディレクトリのファイルエントリ数

    char	wk_fullpath[MAX_PATH + STR_NULL_GUARD];	// ファイルのパス合成用ワーク
    char	wk_pathname[MAX_PATH + STR_NULL_GUARD];	// パス名のワーク
                                                    // 兼 DT_UNKNOWN時のパス合成ワーク
    char	wk_filename[MAX_PATH + STR_NULL_GUARD];	// ファイル名のワーク

    //case-sensitive->noncase-sensitive?
    //if(isNonCSCopy){
    QHash<QString,bool> pre_caseHash;		//課題:ファイルがあほみたいに多い時メモリバカ食いだけど大丈夫かなあ。。
    //}

    if (waitTick) Wait(1);

    // directory かつ ReadProcから初回の呼び出し、かつsrcのフォルダごとコピーする？
    if(srcBaseLen == dir_len && isExtendDir){

        //対象がシンボリックリンクでも中身をコピー？
        if(info.flags & DIR_REPARSE || info.flags & FILE_REPARSE){
            //リンク先の情報を取得
            rc = stat((char*)path,&fdat);
        }
        else{
            rc = lstat((char*)path,&fdat);
        }
        if(rc != SYSCALL_ERR && !IsDir(fdat.st_mode)){
            total.preFiles++;
            total.preTrans += fdat.st_size;
            goto end;
        }
        // FindFirstFile()にdir指定した時一回目の特殊な挙動吸収するよ
        //パスをパスとファイル名に分離
        apath_to_path_file_a((char*)path,&wk_pathname[0],&wk_filename[0]);
        strcat((char*)path,"/");
        dir_len = dir_len + strlen(wk_filename) + 1;
    }

    //まずは指定ディレクトリ一覧とるで
    cdir_fnum = scandir((const char*)path,&namelist,NULL,alphasort);

    if(cdir_fnum == SYSCALL_ERR){
        //読み込み元ディレクトリが読めない場合は続行不能
        ret = false;
        goto end;
    }

    //取得できたファイル数でループ
    for(int i=0;i<cdir_fnum;i++){

        if (IsParentOrSelfDirs((void*)&namelist[i]->d_name[0])){
            //"."と".."は除外
            continue;
        }

        //"."から始まるファイルをコピーしないオプションonかつ.から始まるファイル？
        else if(info.flags & SKIP_DOTSTART_FILE && IsDotStartFile((void*)&namelist[i]->d_name[0])){
            //ユーザが意図的にチェックしたオプションなので、
            //各種skip数とかの統計には加算しないぞー。
            continue;
        }

        //再帰関数延長でpath書き換えされる前にカレントPATHをローカル変数に退避
        strcpyV(wk_fullpath,path);
        //filterチェック判定を先にやりたいが故に涙のlstat発行
        //再帰用のは使わないので、別のを利用するど
        strcpy(wk_pathname,wk_fullpath);
        strcat(wk_pathname,(const char*)&namelist[i]->d_name[0]);

        rc = lstat(wk_pathname,&fdat);

        if(rc == SYSCALL_ERR){
            continue;
        }

        //フィルターチェック
        if((dir_len != srcBaseLen || isMetaSrc
                || !IsDir(fdat.st_mode))
                        && !FilterCheck(path,
                                        namelist[i]->d_name,
                                        fdat.st_mode,
                                        fdat.st_size,
                                        fdat.st_mtim.tv_sec,
                                        //fdat.st_mtimespec.tv_sec,
                                        FASTCOPY_FILTER_FLAGS_STAT)){
            continue;
        }
        //case-sensitive->noncase-sensitive?
        if(isNonCSCopy){
            //まずは当該ディレクトリのファイル名をQString化
            QString upperString = namelist[i]->d_name;
            //全部大文字化
            upperString = upperString.toUpper();
            //既に大文字化した奴が存在する？
            if(pre_caseHash.value(upperString) == true){
                //dst側に同時に存在できないからだめ！強制停止するど
                //てぬき
                if(IsDir3(namelist[i]->d_type)){
                    total.errDirs++;
                }
                else{
                    total.errFiles++;
                }
                //強制停止メッセージ出力
                ConfirmErr((char*)GetLoadStrV(IDS_DST_CASE_ERR),wk_pathname,CEF_NOAPI|CEF_STOP|CEF_NORMAL);
                ret = false;
                //終了
                break;
            }
            else{
                //大文字化した内容をhashのキーとして放り込む
                pre_caseHash[upperString] = true;
            }
        }
        // ディレクトリ＆ファイル情報の蓄積
        if(IsDir3(namelist[i]->d_type)){
            //ディレクトリだね
            total.preDirs++;

            //シンボリックリンクじゃない？またはリンクは実体としてコピー？
            int len = sprintfV(MakeAddr(path, dir_len), FMT_CAT_ASTER_V,namelist[i]->d_name);
            // 再帰コール！
            ret = PreSearchProc(path, prefix_len, dir_len + len);
            // 書き換え直しー
            strcpyV(path,wk_fullpath);
        }
        else if(IsFile3(namelist[i]->d_type)){
            // ファイルだね
            total.preFiles++;
            total.preTrans += fdat.st_size;
        }
        //特殊なファイルシステム向けなので判定はできるだけ後にして、平常時の処理稼ぐ、。
        else if(IsUNKNOWN(namelist[i]->d_type)
                || IsSlnk3(namelist[i]->d_type)){

            if(IsDir(fdat.st_mode)){
                //ディレクトリだね
                total.preDirs++;
                int len = sprintfV(MakeAddr(path, dir_len), FMT_CAT_ASTER_V,namelist[i]->d_name);
                //再帰コール
                ret = PreSearchProc(path, prefix_len, dir_len + len);
                // 書き換え直しー
                strcpyV(path,wk_fullpath);
            }
            else if(IsFile(fdat.st_mode)){
                total.preFiles++;
                total.preTrans += fdat.st_size;
            }
            else if(IsSlnk(fdat.st_mode)){
                //シンボリックリンク先の実体についてstat出して調べる
                rc = stat(wk_pathname,&fdat);
                //symbolic link先の実体確認しにいったらエラーになっちゃった？
                if(rc == SYSCALL_ERR){
                    //気にせず次を処理
                    //どちらにリンクしてたのかわからないのでpreFiles.preDirsともに加算しない。
                    continue;
                }
                //シンボリックリンクの宛先はディレクトリだった？
                else if(IsDir(fdat.st_mode)){
                    total.preDirs++;
                    //リンク先を実体としてコピーする？
                    if(info.flags & DIR_REPARSE){
                        //リンク先のファイルさぐらないといかんので更に探る
                        int len = sprintfV(MakeAddr(path, dir_len), FMT_CAT_ASTER_V,namelist[i]->d_name);
                        // 再帰コール！
                        ret = PreSearchProc(path, prefix_len, dir_len + len);
                        // 書き換え直しー
                        strcpyV(path,wk_fullpath);
                    }
                    //リンクはリンクとしてコピーする？
                    else{
                        //リンクは単なるリンクとして処理するのでリンク先は探らない
                        //qDebug("presearchproc:symbolic link file passed.(dir) path = %s%s",(char*)path,(char*)namelist[i]->d_name);
                    }
                }
                //シンボリックリンク宛先は通常のファイルだった？
                else if(IsFile(fdat.st_mode)){

                    total.preFiles++;
                    //リンクを実体としてコピーする？
                    if(info.flags & FILE_REPARSE){
                        //シンボリックリンク宛先のファイルサイズを予測に加算
                        total.preTrans += fdat.st_size;
                    }
                    else{
                        //リンクは単なるリンクとして処理するのでリンク先は探らない
                        //qDebug("presearchproc:symbolic link file passed.(file) path = %s%s",(char*)path,(char*)namelist[i]->d_name);
                    }
                }
                //その他は対象外
                else{
                    //ConfirmErr("PreSearchProc(symlink->stat() not support file.)", wk_pathname);
                }
            }
            else{
                //ConfirmErr("PreSearchProc:stat() not support file.)", wk_pathname);
            }
        }
        // not support scandir File types
        else{
            //ConfirmErr("PreSearchProc:scandir() not support file.)", wk_pathname);
        }
    }

end:
    // scandir確保領域の解放
    for(int i=0;i < cdir_fnum;i++){
        free(namelist[i]);
    }
    free(namelist);

    //debug
    //qDebug("pretranse file = %lu, dir = %lu, size = %lu",
    //	   total.preFiles,
    //	   total.preDirs,
    //	   total.preTrans);
    CheckSuspend();
    return	ret && !isAbort;
}

BOOL FastCopy::PutList(void *path, DWORD opt, DWORD lastErr, BYTE *digest,qint64 file_size,void *org_path)
{

    time_t wk_t;						//ファイルコピーまたはベリファイ完了時時刻出力用ワーク
    struct tm endTm;

    if (!listBuf.Buf()) return FALSE;

    listCs.lock();

    int require_size = MAX_PATHLEN_V + 1024;

    if (listBuf.RemainSize() < require_size) {
        if (listBuf.Size() < listBuf.MaxSize()) {
            listBuf.Grow(MIN_PUTLIST_BUF);
        }
    }
    //csv出力用バッファの拡張
    if (info.flags_second & CSV_FILEOUT && csvfileBuf.RemainSize() < require_size) {
        if (csvfileBuf.Size() < csvfileBuf.MaxSize()) {
            csvfileBuf.Grow(MIN_PUTLIST_BUF);
        }
    }
    //listing用バッファまたはcsvファイル出力用バッファが減ってきてたら出力要求をメインウインドウにポスト
    if(listBuf.RemainSize() < require_size
        || (info.flags_second & CSV_FILEOUT && csvfileBuf.RemainSize() < require_size)){
        QApplication::postEvent((QObject*)info.hNotifyWnd,
                                new FastCopyEvent(LISTING_NOTIFY));
    }

    time(&wk_t);				//time_t取得
    localtime_r(&wk_t,&endTm);	//startTmに時刻格納
    char	wbuf[512];
    char	datebuf[512];
    int	len = 0;
    char	int64buf[256];		//fileサイズの文字列変換ワーク
    if(file_size != SYSCALL_ERR){
        sprintf(int64buf,"%lld",file_size);
    }

    if (listBuf.RemainSize() >= require_size) {
        if (opt & PL_ERRMSG) {
            len = sprintfV(listBuf.Buf() + listBuf.UsedSize(), FMT_STRNL_V, path);
        }
        else {
            if (digest) {
                    memcpy(wbuf, " :", 2);
                    bin2hexstr(digest, dstDigest.GetDigestSize(), wbuf + 2);
            }
            sprintf(datebuf," : %d/%02d/%02d %02d:%02d:%02d",
                    endTm.tm_year + TM_YEAR_OFFSET, endTm.tm_mon + TM_MONTH_OFFSET, endTm.tm_mday,
                    endTm.tm_hour, endTm.tm_min, endTm.tm_sec);

            len = sprintfV(listBuf.Buf() + listBuf.UsedSize(), FMT_PUTLIST_V,
                            (opt & PL_NOADD) ? ' ' : (opt & PL_DELETE) ? '-' : '+',
                            path,
                            (opt & PL_DIRECTORY) && (opt & PL_REPARSE) ? (DWORD)PLSTR_REPDIR_V :
                            (opt & PL_DIRECTORY) ? (DWORD)BACK_SLASH_V :
                            (opt & PL_REPARSE) ? (DWORD)PLSTR_REPARSE_V :
                            (opt & PL_HARDLINK) ? (DWORD)PLSTR_LINK_V :
                            (opt & PL_CASECHANGE) ? (DWORD)PLSTR_CASECHANGE_V :
                            (opt & PL_COMPARE) || lastErr ? (DWORD)PLSTR_COMPARE_V : (DWORD)EMPTY_STR_V,
                            (char*)digest ? wbuf : EMPTY_STR_V,
                            datebuf); //コピー完了時時刻を入れるよ
                                      //正確に言うとPutList時の時刻だけど、。
        }
        listBuf.AddUsedSize(len * CHAR_LEN_V);
    }
    //csv出力用
    if(info.flags_second & CSV_FILEOUT && csvfileBuf.RemainSize() >= require_size){

        //digest値事前算出
        if(digest){
            bin2hexstr(digest, dstDigest.GetDigestSize(),wbuf);
        }

        sprintf(datebuf,"%d/%02d/%02d %02d:%02d:%02d",
                endTm.tm_year + TM_YEAR_OFFSET, endTm.tm_mon + TM_MONTH_OFFSET, endTm.tm_mday,
                endTm.tm_hour, endTm.tm_min, endTm.tm_sec);

        char	pl_optbuf[16] = "";
        //CSV出力時のOpCode説明についてはIDS_FILECSVEX参照
        sprintf(pl_optbuf,
                "%s,%s,%s",
                (opt & PL_ERRMSG) ? "E" : (opt & PL_DELETE) ? "R" : info.flags & LISTING_ONLY_VERIFY ? "V" : "A",
                (opt & PL_REPARSE) ? "S" : (opt & PL_HARDLINK) ? "H" : "N",
                (opt & PL_DIRECTORY) ? "D" : "F");

        if(opt & PL_ERRMSG){
            //CSVエラー出力の場合は出力用メッセージを二次加工して出力
            QString wk_trim((char*)path);

            //エラーメッセージとは別にパス文字列を渡してきた？
            if(org_path != NULL){
                //エラー内容だけをErrorメッセージ部に出力するので、パス文字列を削除
                wk_trim.remove((char*)org_path);
            }
            len = sprintfV(csvfileBuf.Buf() + csvfileBuf.UsedSize(),
                            FMT_PUTERRCSV_V,
                            pl_optbuf,
                            org_path ? (char*)org_path : "",
                            datebuf,
                            wk_trim.simplified().toLocal8Bit().data());
        }
        else{
            len = sprintfV(csvfileBuf.Buf() + csvfileBuf.UsedSize(),
                            FMT_PUTCSV_V,
                            pl_optbuf,
                            path,
                            (char*)digest ? wbuf : EMPTY_STR_V,
                            datebuf,
                            (file_size == SYSCALL_ERR) ? EMPTY_STR_V : int64buf);	//ファイルサイズ
        }
        csvfileBuf.AddUsedSize(len * CHAR_LEN_V);
    }

    listCs.unlock();
    return	TRUE;
}

//統計情報取得要求関数
BOOL FastCopy::PutStat(void* data){

    if (!statBuf.Buf()) return FALSE;

    statCs.lock();

    int require_size = MAX_PATHLEN_V + 1024;

    if (statBuf.RemainSize() < require_size) {
        if (statBuf.Size() < statBuf.MaxSize()) {
            statBuf.Grow(MIN_PUTLIST_BUF);
        }
        if (statBuf.RemainSize() < require_size) {
            QApplication::postEvent((QObject*)info.hNotifyWnd,
                            new FastCopyEvent(STAT_NOTIFY));
        }
    }

    if (statBuf.RemainSize() >= require_size) {
        int	len = 0;

        len = sprintfV(statBuf.Buf() + statBuf.UsedSize(),"%s\n",(char*)data);
        statBuf.AddUsedSize(len * CHAR_LEN_V);
    }
    statCs.unlock();
    return	TRUE;
}

BOOL FastCopy::MakeDigest(void *path, VBuf *vbuf, TDigest *digest, BYTE *val, _int64 *fsize,FileStat *stat)
{
    int	flg = O_RDONLY;

    //win32のmodeフラグをF_NOCACHE指定指示に流用
    int		mode = ((info.flags & USE_OSCACHE_READVERIFY) ? 0 : F_NOCACHE);

    DWORD	share	= 0;
    BOOL	ret		= FALSE;

    memset(val, 0, digest->GetDigestSize());

    int	hFile = CreateFileWithRetry(path, mode, share, 0, 0, flg, 0, 5);

    digest->Reset();

    if (hFile == SYSCALL_ERR){
        return	FALSE;
    }
    //読み込んだデータのキャッシュ破棄指示
    posix_fadvise(hFile,0,0,POSIX_FADV_DONTNEED);

    if ((DWORD)vbuf->Size() >= maxReadSize || vbuf->Grow(maxReadSize)) {
        struct stat	bhi;

        if ((fstat(hFile,&bhi) == 0)) {

            _int64	remain_size = bhi.st_size;
            if (fsize) {
                *fsize = remain_size;
            }
            CheckSuspend();
            while (remain_size > 0 && !isAbort) {
                DWORD	trans_size = 0;
                if (ReadFileWithReduce(hFile, vbuf->Buf(), maxReadSize, &trans_size, NULL)
                && trans_size > 0){
                    digest->Update(vbuf->Buf(), trans_size);
                    remain_size -= trans_size;
                    total.verifyTrans += trans_size;
                }
                else{
                    break;
                }
            }
            if (remain_size == 0) {
                ret = digest->GetVal(val);
            }
        }
        //fstatのエラー表示処理追加
        else{
            //ERRNO_OUT(errno,"MakeDigest");
            ConfirmErr("MakeDigest:fstat()",(char*)path,CEF_NORMAL);
        }
    }

    //ファイルクローズ
    if(close(hFile) == SYSCALL_ERR){
        ConfirmErr("MakeDigest:close()",(char*)path,CEF_NORMAL);
    }
    return	ret;
}

void MakeVerifyStr(char *buf, BYTE *digest1, BYTE *digest2, DWORD digest_len)
{

    char	*p = buf + sprintf(buf, "Verify Error src:");
    p += bin2hexstr(digest1, digest_len, p);
    p += sprintf(p, " dst:");
    p += bin2hexstr(digest2, digest_len, p);
    p += sprintf(p, " ");

}

BOOL FastCopy::IsSameContents(FileStat *srcStat,FileStat *dstStat)
{

    BOOL	src_ret;
    BOOL	dst_ret;

    if(!isSameDrv){
       //ここでdstreqにpostして、WriteThreadでのdstReadとdstDigest計算を開始
       DstRequest(DSTREQ_DIGEST);
    }
    src_ret = MakeDigest(src, &srcDigestBuf, &srcDigest, srcDigestVal,NULL,srcStat);
    //同一ドライブモードの場合はスレッド分ける意味ないのでカレントスレッドで計算。
    //このWaitDstRequest()でDstRequest()でポストしたwriteスレッドMakeDigest()との待ち合わせを行ってる。
    dst_ret = isSameDrv ? MakeDigest(confirmDst, &dstDigestBuf, &dstDigest, dstDigestVal,NULL,dstStat) : WaitDstRequest();
    BOOL ret = src_ret && dst_ret && memcmp(srcDigestVal, dstDigestVal,
                srcDigest.GetDigestSize()) == 0 ? TRUE : FALSE;
    if(ret){
        total.verifyFiles++;
        if((info.flags & DISABLE_COMPARE_LIST) == 0){
            //通常表示
            PutList(MakeAddr(confirmDst, dstPrefixLen), PL_NOADD, 0, srcDigestVal,srcStat->FileSize());
        }
    }
    else{
        total.errFiles++;
//		PutList(MakeAddr(src,		srcPrefixLen), PL_COMPARE|PL_NOADD, 0, srcDigestVal);
//		PutList(MakeAddr(confirmDst, dstPrefixLen), PL_COMPARE|PL_NOADD, 0, dstDigestVal);
        if (src_ret && dst_ret) {
            QString str;
            QByteArray srcdigest((char*)srcDigestVal,dstDigest.GetDigestSize());
            QByteArray dstdigest((char*)dstDigestVal,dstDigest.GetDigestSize());
            str.append((char*)GetLoadStrV(IDS_VERIFY_ERROR_WARN_1));
            str.append(srcdigest.toHex());
            str.append((char*)GetLoadStrV(IDS_VERIFY_ERROR_WARN_2));
            str.append(dstdigest.toHex());
            str.append((char*)GetLoadStrV(IDS_VERIFY_ERROR_WARN_3));
            ConfirmErr(str.toLocal8Bit().data(), MakeAddr(confirmDst, dstPrefixLen), CEF_NOAPI|CEF_NORMAL);
            //ベリファイエラー検出した場合、dstのファイルを強制削除する。
            //再度のコピーでスキップされないようにすることで事故防止。
            switch(info.mode){
                case DIFFCP_MODE:
                case SYNCCP_MODE:
                case MOVE_MODE:
                    unlink((char*)confirmDst);		//エラーは無視
                    break;
                    //その他のモードの場合は削除しない。
                case VERIFY_MODE:
                case DELETE_MODE:
                case MUTUAL_MODE:
                default:
                    break;
            }
        }
        else if (!src_ret) {
            ConfirmErr("Can't get src digest", MakeAddr(src, srcPrefixLen), CEF_NOAPI|CEF_NORMAL);
            //ConfirmErr("Can't get src digest", MakeAddr(src, srcPrefixLen)、);
        }
        else if (!dst_ret) {
            ConfirmErr("Can't get dst digest", MakeAddr(confirmDst, dstPrefixLen), CEF_NOAPI|CEF_NORMAL);
        }
    }

    return	ret;
}

FastCopy::LinkStatus FastCopy::CheckHardLink(FileStat *stat,void* path, int len,DWORD *data)
{
    LinkStatus	ret = LINK_ERR;

    if(stat->st_nlink >= 2){
        DWORD	_data[3];
        if (!data) data = _data;

        //面倒だから頭からこぴっちまうか
        memcpy(&data[0],&stat->st_dev,sizeof(stat->st_dev));
        if(sizeof(_data[1]))
        memcpy(&data[1],&stat->st_ino,sizeof(stat->st_ino));
        //_data[2]にはコピー済みよ

        UINT	hash_id = hardLinkList.MakeHashId(data);
        LinkObj	*obj;

        if (!(obj = (LinkObj *)hardLinkList.Search(data, hash_id))) {
            if ((obj = new LinkObj(MakeAddr(path, srcBaseLen),stat->st_nlink, data, len))) {
                hardLinkList.Register(obj, hash_id);
                ret = LINK_REGISTER;
            }
            else {
                ConfirmErr("Can't malloc(CheckHardLink)", path, CEF_STOP);
            }
        }
        else {
            ret = LINK_ALREADY;
//			DBGWriteW(L"CheckHardLink %08x.%08x.%08x already %s (%s) %d\n", data[0], data[1],
//			data[2], MakeAddr(path, srcBaseLen), obj->path, bhi.nNumberOfLinks);
        }
    }
    else {
        ret = LINK_NONE;
    }

    return	ret;
}

BOOL FastCopy::ReadProc(int dir_len, BOOL confirm_dir)
{
    BOOL		ret = TRUE;
    FileStat	*srcStat, *statEnd;
    FileStat	*dstStat = NULL;
    int			new_dir_len, curDirStatSize;
    int			confirm_len = dir_len + (dstBaseLen - srcBaseLen);
    BOOL		is_rename_local = isRename;
    BOOL		confirm_dir_local = confirm_dir || is_rename_local;

    isRename = FALSE;	// top level のみ効果を出す

    if(waitTick) Wait(1);

    // カレントのサイズを保存
    curDirStatSize = dirStatBuf.UsedSize();

    if(confirm_dir_local && !isSameDrv) DstRequest(DSTREQ_READSTAT);

    // カレントディレクトリエントリを先にすべて読み取る

    ret = ReadDirEntry(dir_len, confirm_dir_local);

    CheckSuspend();
    if(confirm_dir_local && (isSameDrv ? ReadDstStat() : WaitDstRequest()) == FALSE
        || isAbort || !ret){
        return	FALSE;
    }

    // ファイルを先に処理
    statEnd = (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize());
    for(srcStat = (FileStat *)fileStatBuf.Buf(); srcStat < statEnd;
            srcStat = (FileStat *)((BYTE *)srcStat + srcStat->size)) {
        if(confirm_dir_local){
            dstStat = hash.Search(srcStat->upperName, srcStat->hashVal);
        }
        int		path_len = 0;
        srcStat->fileID = nextFileID++;

        if(info.mode == MOVE_MODE || (info.flags & RESTORE_HARDLINK)){
            path_len = dir_len + sprintfV(MakeAddr(src, dir_len), FMT_STR_V, srcStat->cFileName) + STR_NULL_GUARD;
        }

        if(dstStat){
            if(is_rename_local){
                SetRenameCount(srcStat);
            }
            else{
                dstStat->isExists = TRUE;
                //srcStat->isCaseChanged = strcmpV(srcStat->cFileName, dstStat->cFileName);

                //上書き対象じゃないかチェック
                if (!IsOverWriteFile(srcStat, dstStat) &&
                    (IsReparse(srcStat->dwFileAttributes) == IsReparse(dstStat->dwFileAttributes)
                    || (info.flags & FILE_REPARSE))) {
/* 比較モード */
                    if (isListingOnly && (info.flags & VERIFY_FILE)) {
                        strcpyV(MakeAddr(confirmDst, confirm_len), srcStat->cFileName);
                        strcpyV(MakeAddr(src, dir_len), srcStat->cFileName);
                        CheckSuspend();
                        if (!IsSameContents(srcStat, dstStat) && !isAbort) {
//							PutList(MakeAddr(confirmDst, dstPrefixLen), PL_COMPARE|PL_NOADD);
                        }
                    }
/* 比較モード */
                    if (info.mode == MOVE_MODE) {
                         PutMoveList(srcStat->fileID, src, path_len, srcStat->FileSize(),
                            srcStat->dwFileAttributes, MoveObj::DONE);
                    }
                    total.skipFiles++;
                    total.skipTrans += dstStat->FileSize();
                    if (info.flags & RESTORE_HARDLINK) {
                        CheckHardLink(srcStat,src,path_len,NULL);
                    }
                    continue;
                }
                //上書き対象じゃない時の処理ここまで
            }
        }
        total.readFiles++;

#define MOVE_OPEN_MAX 10
#define WAIT_OPEN_MAX 10
        if(!(ret = OpenFileProc(srcStat, dir_len)) == FALSE
            || srcFsType == FSTYPE_SMB
            || info.mode == MOVE_MODE && openFilesCnt >= MOVE_OPEN_MAX
            || waitTick && openFilesCnt >= WAIT_OPEN_MAX
            || openFilesCnt >= info.maxOpenFiles) {
            ReadMultiFilesProc(dir_len);
            CloseMultiFilesProc();
        }
        CheckSuspend();
        if(isAbort)break;
    }

    ReadMultiFilesProc(dir_len);
    CloseMultiFilesProc();
    CheckSuspend();
    if(isAbort)
        goto END;

    statEnd = (FileStat *)(dirStatBuf.Buf() + dirStatBuf.UsedSize());

    // ディレクトリの存在確認
    if(confirm_dir_local){
        for(srcStat = (FileStat *)(dirStatBuf.Buf() + curDirStatSize); srcStat < statEnd;
                srcStat = (FileStat *)((BYTE *)srcStat + srcStat->size)){

            if((dstStat = hash.Search(srcStat->upperName, srcStat->hashVal)) != NULL){
                //srcStat->isCaseChanged = strcmpV(srcStat->cFileName, dstStat->cFileName);
                if(is_rename_local){
                    SetRenameCount(srcStat);
                }
                else{
                    srcStat->isExists = dstStat->isExists = TRUE;
                    //課題:ここでシンボリックリンクチェックして同じならskip処理いれないとdirへのシンボリックリンク上手く処理できなさげ？
                    //dirのmdateで判定するけど、シンボリックリンクなのをちゃんと確認しないと大誤爆ありえるはず
                    //上書き対象じゃないかチェック
                    //dirのシンボリックリンクチェック処理追加ここから
                    if (!IsOverWriteFile(srcStat, dstStat) &&
                        (IsReparse(srcStat->dwFileAttributes) == IsReparse(dstStat->dwFileAttributes)
                         || (info.flags & DIR_REPARSE))) {

                        //dirへのシンボリックリンクのベリファイやlistingはしないよん
                        //moveモード時の処理は不要、なはず。
                        total.skipDirs++;
                        //あとのdir作成処理をwritethreadにrequestするところで判定する。
                        //MAX_PATH以上のありえない値を入れてフラグ代わり。
                        srcStat->repSize = FASTCOPY_SYMDIR_SKIP;
                        //次のdirstat処理してちょ
                        continue;
                    }
                    //dirのシンボリックリンクチェック処理追加ここまで
                }
            }
            else{
                srcStat->isExists = FALSE;
            }
        }
    }

    // SYNCモードの場合、コピー元に無いファイルを削除
    if(confirm_dir_local && info.mode == SYNCCP_MODE){
        int	max = dstStatIdxBuf.UsedSize() / sizeof(FileStat *);
        for(int i=0; i < max; i++){
            if((dstStat = ((FileStat **)dstStatIdxBuf.Buf())[i])->isExists)
                continue;

            if(!FilterCheck(confirmDst, dstStat->cFileName, dstStat->dwFileAttributes,
                 dstStat->WriteTime(), dstStat->FileSize(),FASTCOPY_FILTER_FLAGS_STAT)) {
                total.filterDstSkips++;
                continue;
            }
            //"."から始まるファイルをコピーしないオプションonかつ.から始まるファイル？
            if(info.flags & SKIP_DOTSTART_FILE && IsDotStartFile(dstStat->cFileName)){
                //ユーザが意図的にチェックしたオプションなので、
                //各種skip数とかの統計には加算しないぞー。
                continue;
            }

            if(isSameDrv){
                if(IsDir(dstStat->dwFileAttributes)){
                    ret = DeleteDirProc(confirmDst, confirm_len, dstStat->cFileName, dstStat);
                }
                else{
                    ret = DeleteFileProc(confirmDst, confirm_len, dstStat->cFileName, dstStat);
                }
            }
            else{
                SendRequest(DELETE_FILES, 0, dstStat);
            }
            CheckSuspend();
            if (isAbort)
                goto END;
        }
    }

    for(srcStat = (FileStat *)(dirStatBuf.Buf() + curDirStatSize);
         srcStat < statEnd; srcStat = (FileStat *)((BYTE *)srcStat + srcStat->size)) {
        BOOL	is_reparse = IsReparse(srcStat->dwFileAttributes)
                                && (info.flags & DIR_REPARSE) == 0;
        int		cur_skips = total.filterSrcSkips;
        total.readDirs++;
        ReqBuf	req_buf = {0};
        new_dir_len = dir_len + sprintfV(MakeAddr(src, dir_len), FMT_STR_V, srcStat->cFileName);
        srcStat->fileID = nextFileID++;

        if (confirm_dir && srcStat->isExists)
            sprintfV(MakeAddr(confirmDst, confirm_len), FMT_CAT_ASTER_V, srcStat->cFileName);

        if (!isListingOnly) {
            //ディレクトリへのシンボリックリンクskip確定してないか最初にチェック
            if (srcStat->repSize != FASTCOPY_SYMDIR_SKIP && (enableAcl || is_reparse || enableXattr)) {
                GetDirExtData(&req_buf, srcStat);
            }
            //シンボリックリンクコピー時でskip確定の場合は対象シンボリックリンクをwritethreadにrequestしない
            if (is_reparse
                 && srcStat->rep == NULL
                 && (!confirm_dir || !srcStat->isExists || srcStat->repSize == FASTCOPY_SYMDIR_SKIP)) {
                //skipね
                continue;
            }
        }

        sprintfV(MakeAddr(src, new_dir_len), FMT_CAT_ASTER_V, "");
        CheckSuspend();
        if (SendRequest(confirm_dir && srcStat->isExists ? INTODIR : MKDIR,
                req_buf.buf ? &req_buf : 0, srcStat), isAbort)
            goto END;

        if (!is_reparse) {
            CheckSuspend();
            if (ReadProc(new_dir_len + 1, confirm_dir && srcStat->isExists), isAbort)
                goto END;
            CheckSuspend();
            if (SendRequest(RETURN_PARENT), isAbort)
                goto END;
        }

        if (info.mode == MOVE_MODE) {
            SetChar(src, new_dir_len, 0);
            if (cur_skips == total.filterSrcSkips) {
                PutMoveList(srcStat->fileID, src, new_dir_len + 1, 0, srcStat->dwFileAttributes,
                /*srcStat->isExists ? MoveObj::DONE : MoveObj::START*/ MoveObj::DONE);
            }
        }
    }

END:
    // カレントの dir用Buf サイズを復元
    dirStatBuf.SetUsedSize(curDirStatSize);
    CheckSuspend();
    return	ret && !isAbort;
}

BOOL FastCopy::PutMoveList(_int64 fileID, void *path, int path_len, _int64 file_size, DWORD attr,
    MoveObj::Status status)
{
    int	path_size = path_len * CHAR_LEN_V;

    moveList.Lock();
    DataList::Head	*head = moveList.Alloc(NULL, 0, sizeof(MoveObj) + path_size);

    if (head) {
        MoveObj	*data = (MoveObj *)head->data;
        data->fileID = fileID;
        data->fileSize = file_size;
        data->dwFileAttributes = attr;
        data->status = status;
        memcpy(data->path, path, path_size);
    }
    if (moveList.IsWait()) {
        moveList.Notify();
    }
    moveList.UnLock();

    if (!head) {
        ConfirmErr("Can't alloc memory(moveList)", NULL, CEF_STOP);
    }
    else {
        FlushMoveList(FALSE);
    }

    return	head ? TRUE : FALSE;
}

BOOL FastCopy::FlushMoveList(BOOL is_finish)
{
    BOOL	is_nowait = FALSE;
    BOOL	is_nextexit = FALSE;
    BOOL	require_sleep = FALSE;

    if (!is_finish) {
        if (moveList.RemainSize() > moveList.MinMargin()) {	// Lock不要
            if ((info.flags & SERIAL_MOVE) == 0) {
                return	TRUE;
            }
            is_nowait = TRUE;
        }
    }

    while (!isAbort && moveList.Num() > 0) {
        DWORD			done_cnt = 0;
        DataList::Head	*head;

        moveList.Lock();
        if (require_sleep) moveList.Wait(CV_WAIT_TICK);

        while ((head = moveList.Fetch()) && !isAbort) {
            MoveObj	*data = (MoveObj *)head->data;

            if (data->status == MoveObj::START) {
                break;
            }
            if (data->status == MoveObj::DONE) {

                BOOL	is_reparse = IsReparse(data->dwFileAttributes);
                moveList.UnLock();
                if(IsDir(data->dwFileAttributes)){
                    if(!isListingOnly && rmdir((char*)data->path) == SYSCALL_ERR){
                        //ムーブ完了したはずなんだけど、コピー元ディレクトリの削除に失敗した。
                        //ここにたどり着いちゃう理由は以下のようにいろいろな理由があるだよ。。
                        //1:.から始まる隠しファイルを無視してmoveかけたので、コピー元に隠しファイルがあったので失敗
                        //2:WriteThreadでの書き込みでエラーになったのでコピー元ファイル削除対象外になった。
                        //3:2はクリアしたけどWDigestThreadでチェックサムエラーになり、コピー元ファイル削除対象外になった。
                        //ユーザにとっちゃ意味わからんので、細かく説明するか。。
                        total.errDirs++;
                        //削除失敗の理由はディレクトリになんらかのファイルが残っているから？
                        if(errno == ENOTEMPTY){
                            ConfirmErr((char*)GetLoadStrV(IDS_MOVEDIR_FAILED),data->path,CEF_DIRECTORY|CEF_NOAPI);
                        }
                        //削除失敗の理由はその他、よーわからんので説明しようがないな。。
                        else{
                            ConfirmErr("rmdir(move):",data->path,CEF_DIRECTORY);
                        }
                    }
                    else{
                        total.deleteDirs++;
                        if(isListing){
                            PutList(data->path,
                                PL_DIRECTORY|PL_DELETE|(is_reparse ? PL_REPARSE : 0));
                        }
                    }
                }
                else {

                    if(!isListingOnly && unlink((const char*)data->path) == SYSCALL_ERR) {
                        total.errFiles++;
                        ConfirmErr("DeleteFile(move)",data->path,CEF_NORMAL);
                    }
                    else {
                        total.deleteFiles++;
                        total.deleteTrans += data->fileSize;
                        if (isListing) {
                            PutList(data->path,
                                PL_DELETE|(is_reparse ? PL_REPARSE : 0),0,NULL,data->fileSize);
                        }
                    }
                }
                done_cnt++;

                if (waitTick) Wait((waitTick + 9) / 10);

                moveList.Lock();
            }

            if (head == moveFinPtr) {
                moveFinPtr = NULL;
            }
            moveList.Get();
        }
        moveList.UnLock();

        CheckSuspend();
        if (moveList.Num() == 0 || is_nowait || isAbort
        || !is_finish && moveList.RemainSize() > moveList.MinMargin()
                && (!isSameDrv || is_nextexit)) {
            break;
        }

        cv.Lock();
        if ((info.flags & VERIFY_FILE) && runMode != RUN_DIGESTREQ) {
            runMode = RUN_DIGESTREQ;
            cv.Notify();
        }
        if (isSameDrv) {
            ChangeToWriteModeCore();
            is_nextexit = TRUE;
        }
        else {
            require_sleep = TRUE;
        }
        cv.UnLock();
        CheckSuspend();
    }
    CheckSuspend();
    return	!isAbort;
}


//xattrおよびaclコピー処理関数入り口(dir単位)
BOOL FastCopy::GetDirExtData(ReqBuf *req_buf, FileStat *stat)
{
    int		fh = SYSCALL_ERR;
    int		flg = O_RDONLY;
    DWORD	size;
    BOOL	ret = TRUE;
    BOOL	is_reparse = IsReparse(stat->dwFileAttributes) && (info.flags & DIR_REPARSE) == 0;
    int		used_size_save = dirStatBuf.UsedSize();

    ssize_t xattr_namelen = 0;   // xattr namelen
    ssize_t xattr_nameoffset = 0;
    ssize_t xattr_datalen = 0;   // xattr datalen
    unsigned char* begin_ead;	 // stat->eadの開始位置記憶用

    acl_t dir_acl;				 //当該ディレクトリのACL Contextへのポインタ
    char* xattr_name = NULL;

    if(is_reparse) {
        size = MAX_PATH + STR_NULL_GUARD;
        if (dirStatBuf.RemainSize() <= (int)size + maxStatSize
        && dirStatBuf.Grow(ALIGN_SIZE(size + maxStatSize, MIN_ATTR_BUF)) == FALSE) {
            ret = false;
            ConfirmErr("Can't alloc memory(dirStatBuf)", NULL, CEF_STOP|CEF_NOAPI);
        }
        else if ((size = readlink((char*)src, (char*)dirStatBuf.Buf() + dirStatBuf.UsedSize(), size))
                    == SYSCALL_ERR) {
            ret = false;
            total.errDirs++;
            ConfirmErr("GetDirExtData(readlink)", MakeAddr(src, srcPrefixLen),CEF_DIRECTORY|CEF_REPARASE);
        }
        else {

            stat->rep = dirStatBuf.Buf() + dirStatBuf.UsedSize();
            //readlink()はけつに\0入れてくれないので自分で入れる。
            *(stat->rep + size) = '\0';
            //repsizeは'\0'分を含むサイズにするのよ。
            stat->repSize = size + STR_NULL_GUARD;
            dirStatBuf.AddUsedSize(stat->repSize);
        }
        //dirへのシンボリックリンクコピーでは
        //シンボリックリンク自身のACL及びXATTRはコピーしない。
        //ファイルと方針合わせる
        goto end;
    }

    if((fh = open((char*)src,flg,0777)) == SYSCALL_ERR){
        //ここでのエラーはエラーとしてカウントしない。
        ConfirmErr("GetDirExtData:open()",(char*)src,CEF_DIRECTORY);
        return false;
    }

    if(ret && enableAcl){

        //DirのACLポインタ取得
        dir_acl	= acl_get_fd(fh);
        //取得失敗？
        if(dir_acl == NULL){
            //aclがついてないdirはENOENTリターンで判断するしかない
            //対象ファイルシステムがACLサポート無し以外のエラー？かつACLエラー報告有効？
            if(errno != ENOENT && info.flags & REPORT_ACL_ERROR){
                if(errno == EOPNOTSUPP || errno == ENOTSUP){
                    ConfirmErr("GetDirExtData:acl_get_fd_np()",
                                MakeAddr(src, srcPrefixLen),
                                CEF_DIRECTORY,
                                (char*)GetLoadStrV(RAPIDCOPY_COUNTER_ACL_EOPNOTSUPP));
                }
                else{
                    ConfirmErr("GetDirExtData:acl_get_fd_np()",
                                MakeAddr(src, srcPrefixLen),
                                CEF_DIRECTORY);
                }
            }
            goto xattr;
        }
        unsigned char *&data = stat->acl;
        data = dirStatBuf.Buf() + dirStatBuf.UsedSize();
        //バッファ空きあるのかね
        dirStatBuf.AddUsedSize(sizeof(dir_acl));
        if (dirStatBuf.RemainSize() <= maxStatSize
        && !dirStatBuf.Grow(ALIGN_SIZE(maxStatSize + sizeof(dir_acl), MIN_ATTR_BUF))) {
            ConfirmErr("Can't alloc memory(dirStat(acl))",MakeAddr(src, srcPrefixLen),CEF_STOP|CEF_NOAPI);
            ret = false;
            //ACLコンテキスト解放
            acl_free((void *)dir_acl);
            goto xattr;
        }
        //acl contextへのアドレスをコピー
        memcpy(data,&dir_acl,sizeof(dir_acl));
        //aclポインタ長を加算
        stat->aclSize += sizeof(dir_acl);
    }

xattr:

    if(ret && enableXattr){

        //dirstatにdirのxattrを蓄積
        //OpenFileBackupStreamLocalのパクリだけど、xattrにdataのLLを入れるのがちょい違うのよ。
        //詳細はsetdirextdataの延長構造設計をみれ
        //xattrのStringlength取得
        xattr_namelen = flistxattr(fh,NULL,0);
        if(xattr_namelen == SYSCALL_ERR){
            //対象ファイルシステムがxattr未サポート以外でエラー？かつxattrエラー報告オプション立ってる？
            if(info.flags & REPORT_STREAM_ERROR){
                ConfirmErr("GetDirExtData:flistxattr(checklength)",
                            MakeAddr(src, srcPrefixLen),
                            CEF_DIRECTORY,
                            (char*)GetLoadStrV(RAPIDCOPY_COUNTER_XATTR_ENOTSUP));
            }
            //以降のxattrでENOTSUP発生した時のエラー処理不要なのでサボり。
            goto end;
        }
        else if(xattr_namelen == 0){
            //xattrなしは成功扱い。けつにとばす
            goto end;
        }
        xattr_name = (char*)dirStatBuf.Buf() + dirStatBuf.UsedSize();
        dirStatBuf.AddUsedSize(xattr_namelen);
        //バッファにあきあるんかな？
        if (dirStatBuf.RemainSize() <= xattr_namelen
            && !dirStatBuf.Grow(ALIGN_SIZE(xattr_namelen, MIN_ATTR_BUF))) {
            ConfirmErr("Can't alloc memory(dirStat(flistxattr))",MakeAddr(src, srcPrefixLen),CEF_STOP|CEF_NOAPI);
            goto end;
        }

        //dirstatBufにxattrのnamelistをコピー。
        xattr_namelen = flistxattr(fh,xattr_name,xattr_namelen);
        if(xattr_namelen == SYSCALL_ERR){
            //xattrエラー報告オプション立ってる？
            if(info.flags & REPORT_STREAM_ERROR){
                ConfirmErr("GetDirExtData:flistxattr()",
                            MakeAddr(src, srcPrefixLen),
                            CEF_DIRECTORY,
                            (char*)GetLoadStrV(RAPIDCOPY_COUNTER_XATTR_ENOTSUP));
            }
            ret = false;
            goto end;
        }

        //xattrデータ実体の数だけループ。
        while(xattr_nameoffset != xattr_namelen){

            //まずはxattrデータ実体の長さチェック
            xattr_datalen = fgetxattr(fh,xattr_name + xattr_nameoffset,NULL,0);
            //失敗？
            if(xattr_datalen == SYSCALL_ERR){
                //xattrエラー報告オプション立ってる？
                if(info.flags & REPORT_STREAM_ERROR){
                    ConfirmErr("GetDirExtData:fgetxattr()",
                                MakeAddr(src, srcPrefixLen),
                                CEF_DIRECTORY,
                                (char*)GetLoadStrV(RAPIDCOPY_COUNTER_XATTR_ENOTSUP_EA),
                                xattr_name + xattr_nameoffset);
                }
                goto end;
            }

            //1エントリの総データ長を事前計算
            //1エントリの電文構造は以下の通り
            //0---------------------------------------- 128--------------------------------------------132-----------------132+128KBのけつ
            //[xattr名称文字列(可変サイズ,\0保証有,MAX128Byte)][データ部長(int,自身のデータ長含まない,最大128KB)][データ本体(MAX128KB)]
            //1エントリの最大データ長=131204
            //これが最大1000エントリ。。
            int	 entry_len = strlen(xattr_name + xattr_nameoffset) + //xattr String
                                    CHAR_LEN_V +					 //¥0
                                    sizeof(int) +					 //xattr LL(LL自身の長さは含まない)
                                    xattr_datalen;					 //xattr data
            unsigned char *&data = stat->ead;

            data = dirStatBuf.Buf() + dirStatBuf.UsedSize();
            //バッファ空きあるのかね
            dirStatBuf.AddUsedSize(entry_len);
            if (dirStatBuf.RemainSize() <= entry_len
            && !dirStatBuf.Grow(ALIGN_SIZE(entry_len, MIN_ATTR_BUF))) {
                ConfirmErr("Can't alloc memory(dirStat(xattr))",MakeAddr(src, srcPrefixLen), CEF_STOP|CEF_NOAPI);
                goto end;
            }
            //stat->eadの開始位置記憶するために開始位置をワークに退避
            if(xattr_nameoffset == 0){
                begin_ead = data;
            }

            //xattr String部分をコピー
            memcpy(data,
                   xattr_name + xattr_nameoffset,
                   strlen(xattr_name + xattr_nameoffset) + CHAR_LEN_V);
            //ワーク変数にLLを計算、格納
            int xattr_ll_and_datalen = xattr_datalen;
            //xattr data長をxattr Stringの後ろ4バイトにコピー
            memcpy(data + strlen(xattr_name + xattr_nameoffset) + CHAR_LEN_V,
                   &xattr_ll_and_datalen,									//LL(not include LL self)
                   sizeof(int));
            //xattr data本体をコピー
            //xattr データ実体をLLの後ろにコピー
            xattr_datalen = fgetxattr(fh,
                            xattr_name + xattr_nameoffset,
                            //cFileName + xattr String + \0 の後続にコピー
                            data + (strlen(xattr_name + xattr_nameoffset) + CHAR_LEN_V + sizeof(int)),
                            xattr_datalen);
            //事前に測ってるのになんらかの理由で入らなかった？
            if(xattr_datalen == SYSCALL_ERR){
                if(info.flags & REPORT_STREAM_ERROR){

                    ConfirmErr("GetDirExtData:fgetxattr2",
                               (char*)src,
                               CEF_DIRECTORY,
                               (char*)GetLoadStrV(RAPIDCOPY_COUNTER_XATTR_ENOTSUP_EA),
                               xattr_name + xattr_nameoffset);
                }
                //もう確保しおわっちゃった後のエラーなので、1エントリ分をなかったことにする
                data -= entry_len;
                dirStatBuf.AddUsedSize(-entry_len);
                goto end;
            }
            //無事に格納できたので、xattr部の合計長に1エントリ長を加算
            stat->eadSize += entry_len;

            //xattr String配列のオフセット更新(\0の長さも含むよ) */
            xattr_nameoffset = xattr_nameoffset
                                + strlen(xattr_name + xattr_nameoffset)
                                + CHAR_LEN_V;
        }
    }
end:
    //is_reparse時はそもそもopenしないのでクローズ不要
    if(fh != SYSCALL_ERR){
        if(close(fh) == SYSCALL_ERR){
            ConfirmErr("GetDirExtData:close()",(char*)src,CEF_DIRECTORY);
        }
    }

    if (ret && (size = stat->aclSize + stat->eadSize + stat->repSize) > 0) {
        if ((ret = PrepareReqBuf(offsetof(ReqHeader, stat) + stat->minSize, size, stat->fileID,
                req_buf))) {
            //dirへの巨大なxattr来た時、クラッシュ回避のためメインバッファに入りきらないxattrが来たら無視する。
            //下記、課題の項参照。
            //xattrのサイズが実際に確保したメインバッファのI/O単位サイズに収まらない？
            if(stat->eadSize > (req_buf->bufSize - (stat->aclSize + stat->repSize))){
                //当該のxattrコピー処理をスキップする
                //(acl及びrepは必ず格納可能なので継続)
                stat->ead = NULL;
                stat->eadSize = 0;
            }
            BYTE	*data = req_buf->buf;
            if (stat->acl) {
                memcpy(data, stat->acl, stat->aclSize);
                stat->acl = data;
                data += stat->aclSize;
            }
            if (stat->ead) {
                //覚えておいたead先頭位置からbufにコピー
                //課題:このとき、eadに巨大な要求があった場合でもメインバッファ確保できた分に縮小される。
                //(ex:メインバッファが2MBなら1MB分しか確保できない)
                //本来ならxattr分割して処理したいが、未実装。。
                //OS Xではdirに巨大なxattrを設定されることはないので問題にはならないんだけどね。。
                memcpy(data, begin_ead, stat->eadSize);
                stat->ead = data;
                data += stat->eadSize;
            }
            if (stat->rep) {
                memcpy(data, stat->rep, stat->repSize);
                stat->rep = data;
                data += stat->repSize;
            }
        }
    }
    if(ret){
        total.readAclStream++;
    }
    else{
        total.errAclStream++;
    }
    dirStatBuf.SetUsedSize(used_size_save);
    CheckSuspend();
    return	ret && !isAbort;
}

int FastCopy::MakeRenameName(void *buf, int count, void *fname)
{
    void	*ext = strrchrV(fname, '.');
    int		body_limit = ext ? DiffLen(ext, fname) : MAX_PATH;

    return	sprintfV(buf, FMT_RENAME_V, body_limit, fname, count, ext ? ext : EMPTY_STR_V);
}

BOOL FastCopy::SetRenameCount(FileStat *stat)
{
    while(1){
        WCHAR	new_name[MAX_PATH];
        int		len = MakeRenameName(new_name, ++stat->renameCount, stat->upperName) + 1;
        DWORD	hash_val = MakeHash(new_name, len * CHAR_LEN_V);
        if (hash.Search(new_name, hash_val) == NULL)
            break;
    }
    return	TRUE;
}

/*
    上書き対象可否判定関数
    返り値true=上書き対象である
    返り値false=上書き対象ではない
*/
BOOL FastCopy::IsOverWriteFile(FileStat *srcStat, FileStat *dstStat)
{
    if(info.overWrite == BY_NAME){
        return	FALSE;
    }

    if(info.overWrite == BY_SIZE){
        //サイズが等しければ上書き対象としない
        if(dstStat->FileSize() == srcStat->FileSize()){
            return	false;
        }
        //else
        return true;
    }

    if(info.overWrite == BY_ATTR){
        // サイズが等しく、かつ...
        if (dstStat->FileSize() == srcStat->FileSize()) {

            // 最終ファイル更新時刻が等しい
            // Macの場合はst_ctime,st_birthtimeのいずれも同値保証できない。
            // utimes,futimesで更新不能。
            // 保証可能な日付で同一判定する
            if (dstStat->WriteTime() == srcStat->WriteTime() &&
                (info.flags & COMPARE_CREATETIME) == 0){
                return	FALSE;
            }

            // どちらかが NTFS でない場合（ネットワークドライブを含む）

            // 下のudf対策判定はwin+ntfsを基準とした判定なので、後回しとする。
            // Linux環境では組み合わせ論膨大すぎる。。
            //if (srcFsType != FSTYPE_NTFS || dstFsType != FSTYPE_NTFS) {
                // 片方が NTFS でない場合、1msec 未満の誤差は許容した上で、比較（UDF 対策）
                /*
                if (dstStat->WriteTime() + 10000 >= srcStat->WriteTime() &&
                    dstStat->WriteTime() - 10000 <= srcStat->WriteTime() &&
                    ((info.flags & COMPARE_CREATETIME) == 0
                ||	dstStat->CreateTime() + 10000 >= srcStat->CreateTime() &&
                    dstStat->CreateTime() - 10000 <= srcStat->CreateTime())) {
                    return	FALSE;
                }
                */
                // src か dst のタイムスタンプの最小単位が 1秒以上（FAT/SAMBA 等）かつ
                /*
                if ((srcStat->WriteTime() % 10000000) == 0
                ||  (dstStat->WriteTime() % 10000000) == 0) {
                    // タイムスタンプの差が 2 秒以内なら、
                    // 同一タイムスタンプとみなして、上書きしない
                    if (dstStat->WriteTime() + 20000000 >= srcStat->WriteTime() &&
                        dstStat->WriteTime() - 20000000 <= srcStat->WriteTime() &&
                        ((info.flags & COMPARE_CREATETIME) == 0
                    ||	dstStat->CreateTime() + 10000000 >= srcStat->CreateTime() &&
                        dstStat->CreateTime() - 10000000 <= srcStat->CreateTime()))
                        return	FALSE;

                }
                */
            //}
            //時刻更新判定処理はいっぺんまとめて見直ししないとやばす。
            //事実かどうかは別として以下のようなまとめあり
            //http://mlog.euqset.org/archives/samba-jp/20691.html
            //FAT32 2.000000000 2s
            //ext3 1.000000000 1s
            //NTFS 0.000000100 100ns
            //XFS 0.000000001 1ns
            //ext4 0.000000001 1ns

            //で、澤津調べによると
            //HFS(+)   1.000000000 1s
            //LTFS 	???????????
            //CodexVFS ???????????
            //exFAT    0.00100000 10ms
            //NFS 	 ???????????
            //ReFS     0.000000100 100ns

            //とりあえずFAT32だけ対策するけど、。。
            if(srcFsType == FSTYPE_FAT || dstFsType == FSTYPE_FAT){
                //タイムスタンプ差が2秒以内なら同じ日付として扱う
                //FAT32の偶数秒単位スタンプに丸められた可能性あり？
                if(dstStat->WriteTime() + 2 >= srcStat->WriteTime()
                    && dstStat->WriteTime() - 2 <= srcStat->WriteTime()){
                    return false;
                }
            }
        }
        return	TRUE;
    }

    if (info.overWrite == BY_LASTEST) {
        // 更新日付が dst と同じか古い場合は更新しない
        if (dstStat->WriteTime() >= srcStat->WriteTime() &&
            ((info.flags & COMPARE_CREATETIME) == 0
        ||	dstStat->CreateTime()  >= srcStat->CreateTime())) {
            return	FALSE;
        }

        // Linux環境では組み合わせ論膨大すぎる。。
        // どちらかが NTFS でない場合（ネットワークドライブを含む）
        /*
        if (srcFsType != FSTYPE_NTFS || dstFsType != FSTYPE_NTFS) {
            // 片方が NTFS でない場合、1msec 未満の誤差は許容した上で、比較（UDF 対策）
            if (dstStat->WriteTime() + 10000 >= srcStat->WriteTime() &&
                ((info.flags & COMPARE_CREATETIME) == 0 ||
                dstStat->CreateTime()  + 10000 >= srcStat->CreateTime())) {
                return	FALSE;
            }
            // src か dst のタイムスタンプの最小単位が 1秒以上（FAT/SAMBA 等）かつ
            if ((srcStat->WriteTime() % 10000000) == 0
            ||	(dstStat->WriteTime() % 10000000) == 0) {
                // タイムスタンプの差に 2 秒のマージンを付けた上で、
                // 更新日付が dst と同じか古い場合は、上書きしない
                if (dstStat->WriteTime() + 20000000 >= srcStat->WriteTime() &&
                    ((info.flags & COMPARE_CREATETIME) == 0
                ||	dstStat->CreateTime() + 20000000 >= srcStat->CreateTime()))
                    return	FALSE;
            }
        }
        */
        return	TRUE;
    }
    if (info.overWrite == BY_ALWAYS){
        return	TRUE;
    }
    return	ConfirmErr("Illegal overwrite mode", 0, CEF_STOP|CEF_NOAPI), FALSE;
}

BOOL FastCopy::ReadDirEntry(int dir_len, BOOL confirm_dir)
{
    BOOL	ret = TRUE;
    int		len;
    struct	stat	fdat;
    struct	dirent ** namelist = NULL;				// 当該ディレクトリのファイルエントリリスト
    int		src_fnum = 0;							// 当該ディレクトリのファイルエントリ数のワーク
    char	wk_fullpath[MAX_PATH + STR_NULL_GUARD];	// ファイルのパス合成用ワーク
    char	wk_pathname[MAX_PATH + STR_NULL_GUARD];	// パス名のワーク
                                                    // 兼 DT_UNKNOWN時のパス合成ワーク
    char	wk_filename[MAX_PATH + STR_NULL_GUARD];	// ファイル名のワーク
    int wk_rtn = SYSCALL_ERR;
    FileStat *stat_pt;								//case-sensitive->noncase-sensitive?重複コピーチェック用ワーク

    fileStatBuf.SetUsedSize(0);
    //case-sensitive->noncase-sensitive?
    if(isNonCSCopy){
        //filestatBufのハッシュ重複検出用のhashの中身をclaer();
        //中身の大きさがどこまで耐えられるかよくわからんのでreserve(先に予約)したいんだけど、。
        //・presearchがあればある程度予測はできるけど、1フォルダに10万なんてのもありえる。
        //・そもそもpresearchしないコピーだと予測しようもない
        //というわけで、諦めるしかない。多分大丈夫と信じる。
        //qDebug() << "caseHashcapacity=" << caseHash.capacity();
        caseHash.clear();
    }

    //リンクは実体としてコピー？
    if(info.flags & DIR_REPARSE || info.flags & FILE_REPARSE){
        wk_rtn = stat((const char*)src,&fdat);
    }
    else{
        //リンクとしてコピー
        wk_rtn = lstat((const char*)src,&fdat);
    }
    if(wk_rtn == SYSCALL_ERR){

        ret = false;
        //失敗なら加算してリターン
        //課題：これ、エラー時にファイルなんだかディレクトリなんだか判別つかないなあ。
        //とりあえずファイルってことにしとくか。
        total.errFiles++;
        ConfirmErr("ReadDirEntry:(l)stat", MakeAddr(src, srcPrefixLen),CEF_NORMAL);
        goto end;
    }
    //stat/lstat成功
    else{
        //正規化済みのパスをパスとファイル名に分離
        apath_to_path_file_a((char*)src,&wk_pathname[0],&wk_filename[0]);

        /* 単体ファイル？*/
        if(!IsDir(fdat.st_mode)){
            //ファイル名が'.'または'..'？
            if (IsParentOrSelfDirs((void*)&wk_filename[0])){
                //"."と".."は除外
                goto end;
            }
            //".から始まるファイルをコピーしないオプションonかつ.から始まるファイル？
            else if(info.flags & SKIP_DOTSTART_FILE && IsDotStartFile((void*)&wk_filename[0])){
                //ユーザが意図的にチェックしたオプションなので、
                //各種skip数とかの統計には加算しない。
                goto end;
            }

            len = FdatToFileStat(&fdat, (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize()),
                                    confirm_dir,wk_filename);
            fileStatBuf.AddUsedSize(len);
            if (fileStatBuf.RemainSize() <= maxStatSize && !fileStatBuf.Grow(MIN_ATTR_BUF)) {
                ret = false;
                ConfirmErr("Can't alloc memory(fileStatBuf)", NULL, CEF_STOP);
            }
            //既存のディレクトリ対象処理と辻褄合わせるため、srcからファイル名を取り除く
            strcpy((char*)src,&wk_pathname[0]);
            // '/'たしとく
            strcat((char*)src,"/");

            //ファイル一個だけなので、以降の処理を無視してケツに飛ばす。
            goto end;
        }
        /* directory かつ ReadProcから初回の呼び出し、かつsrcのフォルダごとコピーする？ */
        if(srcBaseLen == dir_len && isExtendDir){

            // FindFirstFileはdir単体を指定すると中のファイルエントリを調査せずにdirエントリだけ返す謎仕様
            // FindFirstFileの挙動エミュレートのために、dir情報だけ登録してリターンしちゃうぞ

            len = FdatToFileStat(&fdat, (FileStat *)(dirStatBuf.Buf() + dirStatBuf.UsedSize()),
                                    confirm_dir,wk_filename);
            dirStatBuf.AddUsedSize(len);
            if (dirStatBuf.RemainSize() <= maxStatSize && !dirStatBuf.Grow(MIN_ATTR_BUF)) {
                ConfirmErr("Can't alloc memory(dirStatBuf)", NULL, CEF_STOP);
            }
            // けつに飛ばす
            goto end;
        }

        //シンボリックリンク?
        else if(IsSlnk(fdat.st_mode)){
            //リンク先確認しないとどっちに登録したらいいかわからん。。。
            struct stat fdat_target;
            wk_rtn = stat((char*)src,&fdat_target);
            if(wk_rtn == SYSCALL_ERR){
                //シンボリックリンク宛先が存在しない？
                if(errno == ENOENT){
                    //宛先が存在しなくてもシンボリックリンクはコピーするぞー
                    //ERRQT_OUT(errno,"stat(symbolic link) single. ENOENT continue. name = ",(char*)src);
                    //ファイルに入れる
                    len = FdatToFileStat(&fdat, (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize()),
                             confirm_dir,wk_filename);
                    fileStatBuf.AddUsedSize(len);
                    if (fileStatBuf.RemainSize() <= maxStatSize && !fileStatBuf.Grow(MIN_ATTR_BUF)) {
                        ret = false;
                        ConfirmErr("Can't alloc memory(fileStatBuf single)", NULL, CEF_STOP);
                    }
                }
                else{
                    ret = false;
                    //宛先存在せず以外はerror
                    ConfirmErr("stat(symbolic link)", (char*)src);
                }
            }
            else if(IsDir(fdat_target.st_mode)){
                //ディレクトリへのシンボリックリンクやな
                len = FdatToFileStat(&fdat, (FileStat *)(dirStatBuf.Buf() + dirStatBuf.UsedSize()),
                        confirm_dir,wk_filename);
                dirStatBuf.AddUsedSize(len);
                if (dirStatBuf.RemainSize() <= maxStatSize && !dirStatBuf.Grow(MIN_ATTR_BUF)) {
                    ret = false;
                    ConfirmErr("Can't alloc memory(dirStatBuf single)", (char*)src, CEF_STOP);
                }
            }
            else if(IsFile(fdat_target.st_mode)){
                //ファイルへのシンボリックリンクやな
                len = FdatToFileStat(&fdat, (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize()),
                         confirm_dir,wk_filename);
                fileStatBuf.AddUsedSize(len);
                if (fileStatBuf.RemainSize() <= maxStatSize && !fileStatBuf.Grow(MIN_ATTR_BUF)) {
                    ret = false;
                    ConfirmErr("Can't alloc memory(fileStatBuf single)", (char*)src, CEF_STOP);
                }
            }
            else{
                //ブロックデバイス・スペシャルファイルとかその他色々。
                //要エラー処理
                ret = false;
                //ファイル単体コピーでコピー不能
                ConfirmErr("No Supported File Type.", (char*)src);
            }
            //既存のディレクトリ対象処理と辻褄合わせるため、srcからファイル名を取り除く
            strcpy((char*)src,&wk_pathname[0]);
            // '/'たしとく
            strcat((char*)src,"/");
            //ファイル単体処理なんで、後続処理無視して終了。
            goto end;
        }
    }

    //まずは指定ディレクトリ一覧とるで
    src_fnum = scandir((const char*)src,&namelist,NULL,alphasort);
    //scandir失敗してる？
    if(src_fnum == SYSCALL_ERR){
        // 読み込み元ディレクトリが読めない場合は続行不能
        // 一個もファイルやディレクトリが存在しない、はエラーじゃないぞ！
        ret = false;
        total.errDirs++;
        ConfirmErr("ReadDirEntry:scandir()", MakeAddr(src, srcPrefixLen),CEF_DIRECTORY);
        goto end;
    }

    //取得できたファイル数でループ
    for(int i=0;i<src_fnum;i++){

        if (IsParentOrSelfDirs((void*)&namelist[i]->d_name[0])){
            //"."と".."は除外
            //.から始まる隠しファイルはコピーしないオプションの有無確認とか、無視判定も将来的には要追加だぞー
            continue;
        }
        //".から始まるファイルをコピーしないオプションonかつ.から始まるファイル？ */
        else if(info.flags & SKIP_DOTSTART_FILE && IsDotStartFile((void*)&namelist[i]->d_name[0])){
            //ユーザが意図的にチェックしたオプションなので、
            //各種skip数とかの統計には加算しないぞー。
            continue;
        }

        //stat出すためにパス仮合成
        strcpy(wk_fullpath,(const char*)src);
        //合成後の結果がMAX_PATHを越える場合は強制打ち切りする。
        //MAX_PATH+1のパスを生成することでENAMETOOLONG発生を誘導
        strncat(wk_fullpath,(const char*)&namelist[i]->d_name[0],(MAX_PATH - strlen(wk_fullpath)));

        //リンクは実体としてコピー？
        if(info.flags & DIR_REPARSE || info.flags & FILE_REPARSE){
            wk_rtn = stat((const char*)wk_fullpath,&fdat);
        }
        //リンクとしてコピー
        else{
            wk_rtn = lstat((const char*)wk_fullpath,&fdat);
        }
        if(wk_rtn == SYSCALL_ERR){
            //失敗時はdirとdir以外でざっくりカウント
            if(IsDir3(namelist[i]->d_type)){
                total.errDirs++;
            }
            else{
                total.errFiles++;
            }
            ConfirmErr("ReadDirEntry:(l)stat",
                        wk_fullpath,
                        IsDir3(namelist[i]->d_type) ? CEF_DIRECTORY : CEF_NORMAL);
            //他のファイルは上手く行くかもしれないので続行
            continue;
        }

        //ファイルフィルタリング
        if ((dir_len != srcBaseLen || isMetaSrc || !IsDir(fdat.st_mode))
                && !FilterCheck(src,
                                namelist[i]->d_name,
                                fdat.st_mode,
                                fdat.st_mtim.tv_sec,
                                fdat.st_size,
                                FASTCOPY_FILTER_FLAGS_STAT)){
            //qDebug("filter checked?");
            total.filterSrcSkips++;
            continue;
        }
        // ディレクトリ＆ファイル情報の蓄積
        if (IsDir(fdat.st_mode)) {
            //ディレクトリやな
            len = FdatToFileStat(&fdat, (FileStat *)(dirStatBuf.Buf() + dirStatBuf.UsedSize()),
                    confirm_dir,&namelist[i]->d_name[0]);
            stat_pt = (FileStat *)(dirStatBuf.Buf() + dirStatBuf.UsedSize());
            dirStatBuf.AddUsedSize(len);
            if (dirStatBuf.RemainSize() <= maxStatSize && !dirStatBuf.Grow(MIN_ATTR_BUF)) {
                ConfirmErr("Can't alloc memory(dirStatBuf)", NULL, CEF_STOP);
                break;
            }
        }
        else if(IsFile(fdat.st_mode)){
            //ファイルやな
            len = FdatToFileStat(&fdat, (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize()),
                     confirm_dir,&namelist[i]->d_name[0]);
            stat_pt = (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize());
            fileStatBuf.AddUsedSize(len);
            if (fileStatBuf.RemainSize() <= maxStatSize && !fileStatBuf.Grow(MIN_ATTR_BUF)) {
                ConfirmErr("Can't alloc memory(fileStatBuf)", NULL, CEF_STOP);
                break;
            }
        }
        //
        else if(IsSlnk(fdat.st_mode)){
            //リンク先確認しないとどっちに登録したらいいかわからん。。。
            struct stat fdat_target;
            wk_rtn = stat((const char*)wk_fullpath,&fdat_target);
            if(wk_rtn == SYSCALL_ERR){
                //シンボリックリンク宛先が存在しない？
                if(errno == ENOENT){
                    //宛先が存在しなくてもシンボリックリンクはコピーするぞー
                    //debug
                    //ERRQT_OUT(errno,"stat(symbolic link) ENOENT continue.name = ",&namelist[i]->d_name[0]);
                    len = FdatToFileStat(&fdat, (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize()),
                             confirm_dir,&namelist[i]->d_name[0]);
                    stat_pt = (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize());
                    fileStatBuf.AddUsedSize(len);
                    if(fileStatBuf.RemainSize() <= maxStatSize && !fileStatBuf.Grow(MIN_ATTR_BUF)) {
                        ConfirmErr("Can't alloc memory(fileStatBuf)", NULL, CEF_STOP);
                        break;
                    }
                }
                else{
                    //宛先存在せず以外はさすがにダメ
                    //debug
                    ConfirmErr("ReadDirEntry:symboliclink->stat()", wk_fullpath);
                    continue;
                }
            }
            else if(IsDir(fdat_target.st_mode)){
                //ディレクトリへのシンボリックリンクやな
                len = FdatToFileStat(&fdat, (FileStat *)(dirStatBuf.Buf() + dirStatBuf.UsedSize()),
                        confirm_dir,&namelist[i]->d_name[0]);
                stat_pt = (FileStat *)(dirStatBuf.Buf() + dirStatBuf.UsedSize());
                dirStatBuf.AddUsedSize(len);
                if (dirStatBuf.RemainSize() <= maxStatSize && !dirStatBuf.Grow(MIN_ATTR_BUF)) {
                    ConfirmErr("Can't alloc memory(dirStatBuf)", NULL, CEF_STOP);
                    break;
                }
            }
            else if(IsFile(fdat_target.st_mode)){
                //ファイルへのシンボリックリンクやな
                len = FdatToFileStat(&fdat, (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize()),
                         confirm_dir,&namelist[i]->d_name[0]);
                stat_pt = (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize());
                fileStatBuf.AddUsedSize(len);
                if (fileStatBuf.RemainSize() <= maxStatSize && !fileStatBuf.Grow(MIN_ATTR_BUF)) {
                    ConfirmErr("Can't alloc memory(fileStatBuf)", NULL, CEF_STOP);
                    break;
                }
            }
            else{
                //ブロックデバイス・スペシャルファイルとか色々。
                //qDebug("Other not supported.(symbolic link) ReadDirEntry path = %s",wk_fullpath);
                ConfirmErr("ReadDirEntry:No Supported File Type.(symbolic link)", wk_fullpath);
                continue;
            }

        }
        //ブロックデバイス・スペシャルファイルとかその他もろもろのファイル種別。
        else{
            //qDebug("Other not supported. ReadDirEntry() path = %s",wk_fullpath);
            ConfirmErr("ReadDirEntry:No Supported File Type.", wk_fullpath);
            continue;
        }
        //case-sensitive->noncase-sensitive?
        if(isNonCSCopy){
            //同一フォルダ階層に大文字小文字以外全く同じファイルまたはディレクトリが存在する？
            if(caseHash.value(stat_pt->hashVal) == true){
                //書き込み先ファイルシステムに存在を許されないよエラー出力
                if((unsigned char*)stat_pt == (fileStatBuf.Buf() + fileStatBuf.UsedSize() - len)){
                    total.errFiles++;
                    ConfirmErr((char*)GetLoadStrV(IDS_DST_CASE_ERR),(char*)wk_fullpath,CEF_NOAPI|CEF_STOP|CEF_NORMAL);
                }
                else{
                    //dir用だべな
                    total.errDirs++;
                    ConfirmErr((char*)GetLoadStrV(IDS_DST_CASE_ERR),(char*)wk_fullpath,CEF_NOAPI|CEF_STOP|CEF_DIRECTORY);
                }
                //抜けて強制停止
                break;
                //以下エラー処理頑張ってみようと思ったけど、やっぱり意味ないので未実装。
                //ファイルであれディレクトリであれ、コピー元のどっちかをコピーできないのが確定した時点で
                //プログラム側にできることがなにもない。さっさと止めてユーザによるrenameまたは整理を期待すべき。
            }
            else{
                //対象のファイルをupperした値
                caseHash[stat_pt->hashVal] = true;
            }
        }
    }

end:
    //scandir確保領域の解放
    for(int i=0;i < src_fnum;i++){
        free(namelist[i]);
    }
    free(namelist);
    CheckSuspend();
    return	ret && !isAbort;
}


BOOL FastCopy::OpenFileProc(FileStat *stat, int dir_len)
{
    DWORD	name_len = strlenV(stat->cFileName);
    memcpy(MakeAddr(src, dir_len), stat->cFileName, ((name_len + 1) * CHAR_LEN_V));
    openFiles[openFilesCnt++] = stat;

    if (waitTick) Wait((waitTick + 9) / 10);

    if (isListingOnly)	return	TRUE;

    BOOL	ret = TRUE;

    BOOL	is_backup = enableAcl || enableXattr;

    BOOL	is_reparse = IsReparse(stat->dwFileAttributes) && (info.flags & FILE_REPARSE) == 0;
    BOOL	is_open = stat->FileSize() > 0 || !is_reparse; // !is_reparseは0バイトファイル対策

    if(is_open){

        int		mode = info.flags & USE_OSCACHE_READ ? 0 : F_NOCACHE;
        int	flg = 0;
        //O_NOATIMEはNFSで利用しようとするとのちのread()でEPERMになってしまうので、NFSの場合はNOATIMEつけない
        //SMB/CIFSでも同様みたいだし、致命的問題になりやすいから諦めるかあ
        //OSX へのSMB接続でもしれっとFSTYPE_NONEになっちゃったりするし、statfsあてにならなさすぎるわあ。。
        /*
        if(srcFsType == FSTYPE_NFS || srcFsType == FSTYPE_CIFS
            || srcFsType == FSTYPE_SMB || srcFsType == FSTYPE_FUSE
            || srcFsType == FSTYPE_NONE){
            flg |= O_RDONLY;
        }
        else{
            flg |= O_RDONLY;
            flg |= O_NOATIME;
        }
        */
        flg = O_RDONLY;

        if (is_reparse){
            //シンボリックリンクをリンクとしてコピーするのでリンク自身を開くフラグ付与
            flg |= O_NOFOLLOW;
            flg |= O_PATH;
        }
        if((stat->hFile = open((char*)src,flg,0777)) == SYSCALL_ERR){
            //errnoの値をgetlasterrorの代わりとしてセット。
            stat->lastError = errno;
            ret = FALSE;
        }
        else{

            if(mode == F_NOCACHE){
                //ファイルシステムキャッシュ無効要求しておく
                //どうせ一回しかアクセスされないのにキャッシュする理由ないでしょ
                posix_fadvise(stat->hFile,0,0,POSIX_FADV_DONTNEED);
            }
        }
    }

    if (ret && is_backup && is_open) {
        ret = OpenFileBackupProc(stat, dir_len + name_len);
    }
    return	ret;
}

//xattrおよびaclコピー処理関数入り口(file単位)
BOOL FastCopy::OpenFileBackupProc(FileStat *stat, int src_len)
{
    BOOL	ret = true;

    //xattrコピー有効？
    if(enableXattr){
        //winのように代替ストリームに細かい種類ないので、ここでまとめて処理しちゃう。
        ret = OpenFileBackupStreamLocal(stat,&fileStatBuf,NULL);
    }
    if(ret && enableAcl){
        //ret = aclもいい感じに
        ret = OpenFileBackupAclLocal(stat,&fileStatBuf,NULL);
    }

    if (ret){
        total.readAclStream++;
    }
    else{
        total.errAclStream++;
    }
    return	ret;
}

//機能:引数statが開いているファイルハンドルにaclがあればfilestatに登録する。
//返り値:true:一つ以上のaclをコピーした。またはaclが元々存在しなかった。
//		false:引数statが開いているファイルハンドルへのaclアクセスでなんらかのエラーが発生した。
//注意事項:in_lenの中身はfalseリターン時は保証しない。
BOOL FastCopy::OpenFileBackupAclLocal(FileStat *stat,VBuf *buf,int *in_len){

    BOOL ret = true;

    acl_t file_acl;								//当該ファイルのACL Contextへのポインタ

    //fileのACLポインタ取得
    file_acl = acl_get_fd(stat->hFile);

    //取得失敗？
    if(file_acl == NULL){
        //aclもってないファイル？
        if(errno == ENOENT){
            //aclがついてないdir(正常系だと思うけど)はENOENTリターンで判断するしかない。。
            goto end;
        }
        //error確定
        ret = false;
        if(info.flags & REPORT_ACL_ERROR){
            if(errno == EOPNOTSUPP || errno == ENOTSUP){
                ConfirmErr("OpenFileBackupAclLocal:acl_get_fd_np()",
                            MakeAddr(src, srcPrefixLen),
                            CEF_NORMAL,
                            (char*)GetLoadStrV(RAPIDCOPY_COUNTER_ACL_EOPNOTSUPP));
            }
            else{
                ConfirmErr("OpenFileBackupAclLocal:acl_get_fd_np()",
                            MakeAddr(src, srcPrefixLen),
                            CEF_NORMAL);
            }
        }
        goto end;
    }
    else{
        unsigned char *&data = stat->acl;
        data = buf->Buf() + buf->UsedSize();
        stat->aclSize += sizeof(file_acl);
        //バッファ空きあるのかね
        buf->AddUsedSize(stat->aclSize);
        if (buf->RemainSize() <= maxStatSize
            && !buf->Grow(ALIGN_SIZE(maxStatSize + sizeof(file_acl), MIN_ATTR_BUF))) {
            ConfirmErr("Can't alloc memory(OpenFileBackupAclLocal(acl))",MakeAddr(src, srcPrefixLen), CEF_STOP|CEF_NOAPI);
            ret = false;
            goto end;
        }
        //acl contextへのアドレスをコピー
        memcpy(data,&file_acl,sizeof(file_acl));
    }

end:
    return(ret);
}

//機能:引数statが開いているファイルハンドルにxattrがあればfilestatに登録する。
//返り値:true:一つ以上のxattrをコピーした。またはxattrが元々存在しなかった。
//		false:引数statが開いているファイルハンドルへのxattrアクセスでなんらかのエラーが発生した。
//注意事項:in_lenの中身はfalseリターン時は保証しない。
BOOL FastCopy::OpenFileBackupStreamLocal(FileStat *stat,VBuf *buf, int *in_len)
{
    BOOL ret = true;

    ssize_t xattr_namelen = 0;     // xattr namelen
    ssize_t xattr_nameoffset = 0;
    ssize_t xattr_datalen = 0;     // xattr datalen
    ssize_t xattr_datatotal = 0;   // xattr data total
    char *xattr_name = NULL;

    //xattrのname配列の合計長を事前取得
    //ex:"sawa.sawa.aaa\0sawa.sawa2.bbb\0"の場合は28。
    //最大で128*1000で約128KBだがstatbufは1GBデフォルトなのでたぶん大丈夫。
    xattr_namelen = flistxattr(stat->hFile,NULL,0);

    if(xattr_namelen == SYSCALL_ERR){
        if(info.flags & REPORT_STREAM_ERROR){
            ConfirmErr("OpenFileBackupStreamLocal:flistxattr(checklength)",
                        MakeAddr(src, srcPrefixLen),
                        CEF_NORMAL,
                        (char*)GetLoadStrV(RAPIDCOPY_COUNTER_XATTR_ENOTSUP));
            //以降のxattrでENOTSUP発生した時のエラー処理不要なのでサボり。
        }
        ret = false;
        goto end;
    }
    //そもそもxattrもってない？
    else if(xattr_namelen == 0){
        //xattrなしは成功扱い。けつにとばす
        ret = true;
        //データ無しで0設定しとく
        in_len = 0;
        goto end;
    }

    //xattrのname配列をbufに格納できるかチェック。入らないならVBufの仮想拡張する。
    xattr_name = (char*)buf->Buf() + buf->UsedSize();
    buf->AddUsedSize(xattr_namelen);
    if(buf->RemainSize() <= xattr_namelen
        && !buf->Grow(ALIGN_SIZE(xattr_namelen, MIN_ATTR_BUF))) {
            ConfirmErr("Can't alloc memory(OpenFileBackupStreamLocal:buf(1))",MakeAddr(src, srcPrefixLen),CEF_STOP|CEF_NOAPI);
        goto end;
    }

    //xattrのname配列をbuf内に取得。
    xattr_namelen = flistxattr(stat->hFile,xattr_name,xattr_namelen);

    if(xattr_namelen == SYSCALL_ERR){
        //xattrエラー報告オプション立ってる？
        if(info.flags & REPORT_STREAM_ERROR){
            ConfirmErr("OpenFileBackupStreamLocal:flistxattr()",
                        MakeAddr(src, srcPrefixLen),
                        CEF_NORMAL,
                        (char*)GetLoadStrV(RAPIDCOPY_COUNTER_XATTR_ENOTSUP));
        }
        ret = false;
        goto end;
    }

    //xattrデータ実体の数だけループ。
    while(xattr_nameoffset != xattr_namelen){

        //まずはxattrデータ実体の長さチェック
        xattr_datalen = fgetxattr(stat->hFile,xattr_name + xattr_nameoffset,NULL,0);
        //失敗？
        if(xattr_datalen == SYSCALL_ERR){
            //xattrエラー報告オプション立ってる？
            if(info.flags & REPORT_STREAM_ERROR){
                ConfirmErr("OpenFileBackupStreamLocal:fgetxattr()",
                            MakeAddr(src, srcPrefixLen),
                            CEF_NORMAL,
                            (char*)GetLoadStrV(RAPIDCOPY_COUNTER_XATTR_ENOTSUP_EA),
                            xattr_name + xattr_nameoffset);
            }
            ret = false;
            goto end;
        }
        FileStat *subStat = (FileStat *)(buf->Buf() + buf->UsedSize());

        openFiles[openFilesCnt++] = subStat;
        subStat->fileID = nextFileID++;
        //xattrはALTSTREAMと異なり独自のファイル名でopenする必要がない。
        //既にopen済みのファイル本体のfdを引き継がせておく
        subStat->hFile = stat->hFile;
        subStat->nFileSize = xattr_datalen;

        subStat->dwFileAttributes = 0;	// xattrだよ印
        subStat->renameCount = 0;
        subStat->lastError = 0;

        subStat->size = offsetof(FileStat, cFileName) +
                         strlen(xattr_name +
                         xattr_nameoffset) +
                         STR_NULL_GUARD;		// \0

        subStat->minSize = ALIGN_SIZE(subStat->size, 8);

        buf->AddUsedSize(subStat->minSize);
        if (buf->RemainSize() <= maxStatSize && !buf->Grow(MIN_ATTR_BUF)) {
            ConfirmErr("Can't alloc memory(OpenFileBackupStreamLocal:buf(2))", NULL, CEF_STOP|CEF_NOAPI);
            ret = false;
            goto end;
        }

        // xattr Stringをコピー
        memcpy(subStat->cFileName,
               xattr_name + xattr_nameoffset,
               strlen(xattr_name + xattr_nameoffset) + CHAR_LEN_V);  // \0も含めてコピー

        //in_len指定時にお返しするように合計値加算
        xattr_datatotal = xattr_datatotal + xattr_datalen;
        //xattr String配列のオフセット更新(\0の長さも含むよ)
        xattr_nameoffset = xattr_nameoffset
                            + strlen(xattr_name + xattr_nameoffset)
                            + CHAR_LEN_V;							// \0
    }
    //in_lenに指定あればxattrString全体長 + xattrdata合計長を格納
    if(in_len != NULL){
        *in_len = xattr_namelen + xattr_datatotal;
    }

end:

    return(ret);
}

BOOL FastCopy::ReadMultiFilesProc(int dir_len)
{
    for (int i=0; !isAbort && i < openFilesCnt; ) {
        ReadFileProc(i, &i, dir_len);
        CheckSuspend();
    }
    return	!isAbort;
}

BOOL FastCopy::CloseMultiFilesProc(int maxCnt)
{
    if (maxCnt == 0) {
        maxCnt = openFilesCnt;
        openFilesCnt = 0;
    }
    if (!isListingOnly) {
        for(int i=0; i < maxCnt; i++) {
            //open失敗またはxattr処理用に親statからコピったfdじゃなければcloseする
            //dwFileAttributesが0=subStatだよ
            if (openFiles[i]->hFile != SYSCALL_ERR && openFiles[i]->dwFileAttributes != 0) {
                //::CloseHandle(openFiles[i]->hFile);
                //qDebug() << "maxCnt" << maxCnt << "CloseMultiFilesProc:" << i << " fh=" << openFiles[i]->hFile;
                if(close(openFiles[i]->hFile) == SYSCALL_ERR){
                    ConfirmErr("CloseMultiFilesProc:close",(char*)openFiles[i]->cFileName,CEF_NORMAL);
                }
                //MOVEMODE時にReadFileProc延長で先にcloseされている。
                else{
                    openFiles[i]->hFile = SYSCALL_ERR;
                }
            }
        }
    }
    CheckSuspend();
    return	!isAbort;
}

void *FastCopy::RestoreOpenFilePath(void *path, int idx, int dir_len)
{
    FileStat	*stat = openFiles[idx];
    BOOL		is_stream = stat->dwFileAttributes ? FALSE : TRUE;

    if (is_stream) {
        int	i;
        for (i=idx-1; i >= 0; i--) {
            if (openFiles[i]->dwFileAttributes) {
                dir_len += sprintfV(MakeAddr(path, dir_len), FMT_STR_V, openFiles[i]->cFileName);
                break;
            }
        }
        if (i < 0) {
            ConfirmErr("RestoreOpenFilePath", MakeAddr(path, srcPrefixLen),
                       CEF_STOP|CEF_NOAPI|CEF_NORMAL);
        }
    }
    sprintfV(MakeAddr(path, dir_len), FMT_STR_V, stat->cFileName);
    return	path;
}

BOOL FastCopy::ReadFileWithReduce(int hFile, void *buf, DWORD size, DWORD *reads, void *overwrap)
{
    DWORD	trans = 0;
    DWORD	total = 0;
    DWORD	maxReadSizeSv = maxReadSize;

    struct	aiocb readlist[4];
    DWORD	aio_pnum = sizeof(readlist) / sizeof(struct aiocb);
    struct	aiocb *list[aio_pnum];		//実験用

    while ((trans = size - total) > 0) {
        DWORD	transed = 0;
        trans = min(trans, maxReadSize);
        //時間計測処理
        QElapsedTimer et;
        if(info.flags_second & STAT_MODE){
            et.start();
        }

        //aio有効かつ要求バイトが同時io実行数以上？こっちはあくまで実験用のなごりだよ
        /*
        if(info.flags_second & FastCopy::ASYNCIO_MODE && (trans % aio_pnum) == 0){
            memset(&readlist,0,(sizeof(struct aiocb) * aio_pnum));
            //fdのカレントオフセット取得
            off_t current = lseek(hFile,0,SEEK_CUR);
            //qDebug() << "offset=" << current;
            for(DWORD i=0; i < aio_pnum;i++){
                readlist[i].aio_fildes = hFile;
                readlist[i].aio_lio_opcode = LIO_READ;
                //ex:1MBで同時要求数4だったら0,256KB,512KB,768KBにオフセット設定ね
                readlist[i].aio_buf = (BYTE *)buf + ((trans / aio_pnum) * i);
                //ex:1MBで同時要求数4だったら256KBずつ
                readlist[i].aio_nbytes = trans / aio_pnum;
                //ex:1MBで同時要求数4だったらファイル読み込み開始位置はcurrentオフセットから0,256KB,512KB,768KBの位置
                readlist[i].aio_offset = current + ((trans / aio_pnum) * i);
                struct aiocb *ap = &readlist[i];
                list[i] = ap;
            }
            //まとめてread発行
            if(::lio_listio(LIO_WAIT,list,aio_pnum,NULL) == SYSCALL_ERR){
                ConfirmErr("ReadFileWithReduce:lio_listio()",NULL);
            }
            //結果をまとめて回収
            for(DWORD i=0; i < aio_pnum;i++){
                ssize_t wk_trans = ::aio_return(list[i]);
                //qDebug() << "wk_trans=" << wk_trans;
                if(wk_trans == SYSCALL_ERR){
                    char buf[512];
                    sprintf(buf,"%s %d %lu","ReadFileWithReduce:aio_return()",errno,i);
                    ConfirmErr(buf,NULL);
                    return FALSE;
                }
                transed = transed + wk_trans;
            }
            //fdオフセット進めて次の開始位置を設定
            lseek(hFile,transed,SEEK_CUR);
        }
        else{
        */
        transed = read(hFile,(BYTE *)buf + total,trans);
        //}

        if(info.flags_second & STAT_MODE){
            QTime time = QTime::currentTime();
            QString time_str = time.toString("hh:mm:ss.zzz");
            QString out(time_str + "re:" + QString::number(et.elapsed()) + "rq:" + QString::number(transed));
            PutStat(out.toLocal8Bit().data());
        }
        if(transed == SYSCALL_ERR){
            if(errno || min(size, maxReadSize) <= REDUCE_SIZE){
                return FALSE;
            }
            maxReadSize -= REDUCE_SIZE;
            maxReadSize = ALIGN_SIZE(maxReadSize,REDUCE_SIZE);
            continue;
        }
        total += transed;
        if (transed != trans) {
            break;
        }
    }
    *reads = total;

    if (maxReadSize != maxReadSizeSv) {
        WCHAR buf[128];
        sprintfV(buf, FMT_REDUCEMSG_V, 'R', maxReadSizeSv / 1024/1024, maxReadSize / 1024/1024);
        WriteErrLog(buf);
    }
    return	TRUE;
}

//xattr取得、バッファコピー関数(バッファサイズ未満のコピー用)
BOOL FastCopy::ReadFileXattr(int hFile,char *xattrname, void *buf, DWORD size, DWORD *reads){

    ssize_t xattr_datalen = 0;

    xattr_datalen = fgetxattr(hFile,
                        xattrname,
                        buf,
                        size);
    if(xattr_datalen == SYSCALL_ERR){
        if(info.flags & (REPORT_STREAM_ERROR)){
            ConfirmErr("ReadFileXattr:fgetxattr",
                        (char*)src,
                        CEF_NORMAL,
                        (char*)GetLoadStrV(RAPIDCOPY_COUNTER_XATTR_ENOTSUP_EA),
                        xattrname);
        }
        return false;
    }
    *reads = xattr_datalen;
    return true;
}

//xattr取得、バッファコピー関数(バッファサイズ以上の分割コピー用)
BOOL FastCopy::ReadFileXattrWithReduce(int hFile,char *xattrname, void *buf, DWORD size, DWORD *reads,off_t file_size,_int64 remain_size){

    //xattrは分割できないので、仕方なく毎回mallocしてデータ確保する。
    void*	wk_xattr;			//xattrワークデータ

    ssize_t xattr_datalen = 0;

    //xattr実データ分の領域を確保
    wk_xattr = malloc(file_size);
    //確保失敗？
    if(wk_xattr == NULL){
        //エラーリターン
        return false;
    }
    //一回mallocしたワーク領域に内容を全部コピー
    xattr_datalen = fgetxattr(hFile,
                        xattrname,
                        (char*)wk_xattr,
                        file_size);

    if(xattr_datalen == SYSCALL_ERR){
        if(info.flags & REPORT_STREAM_ERROR){
            ConfirmErr("GetDirExtData:fgetxattr()",
                        (char*)src,
                        CEF_NORMAL,
                        (char*)GetLoadStrV(RAPIDCOPY_COUNTER_XATTR_ENOTSUP_EA),
                        xattrname);
        }
        return false;
    }
    //バッファ余分に用意しすぎなら残りサイズぴったりに調整
    //けつ調整用
    if(size > remain_size){
        size = remain_size;
    }
    memcpy(buf,
           ((char*)wk_xattr) + (file_size - remain_size),
           size);

    *reads = size;
    free(wk_xattr);
    return true;
}

BOOL FastCopy::ReadFileProc(int start_idx, int *end_idx, int dir_len)
{
    BOOL		ret = TRUE, ext_ret = TRUE;
    FileStat	*stat = openFiles[start_idx];
    off_t		file_size = stat->FileSize();
    DWORD		trans_size;
    ReqBuf		req_buf;
                          //ACL有効またはEA有効じゃない=WRITE_FILE
                          //ACL有効またはEA有効かつdwFileAttributesに0(substatによるEA指示)=WRITE_BACKUP_ALTSTREAM
                          //ACL有効またはEA有効でdwFileAttributesに何か入ってたらWRITE_BACKUP_FILE
    Command		command = enableAcl || enableXattr ? stat->dwFileAttributes ?
                            WRITE_BACKUP_FILE : WRITE_BACKUP_ALTSTREAM : WRITE_FILE;
    BOOL		is_stream = command == WRITE_BACKUP_ALTSTREAM;
    BOOL		is_reparse = IsReparse(stat->dwFileAttributes) && (info.flags & FILE_REPARSE) == 0;
    BOOL		is_send_request = FALSE;
    int			&totalErrFiles = is_stream ? total.errAclStream   : total.errFiles;
    _int64		&totalErrTrans = is_stream ? total.errStreamTrans : total.errTrans;
    BOOL		is_digest = IsUsingDigestList() && !is_stream && !is_reparse;

    *end_idx = start_idx + 1;

//	ReadFile で対処
//	if (!is_reparse && IsReparse(stat->dwFileAttributes)) {		// reparse先をコピー
//		srcSectorSize = max(srcSectorSize, OPT_SECTOR_SIZE);
//		sectorSize = max(srcSectorSize, dstSectorSize);
//	}

    if ((info.flags & RESTORE_HARDLINK) && !is_stream) {	// include listing only
        if (CheckHardLink(stat, src,-1,stat->GetLinkData())
                == LINK_ALREADY) {
            stat->SetFileSize(0);
            ret = SendRequest(CREATE_HARDLINK, 0, stat);
            goto END;
        }

    }

    if ((file_size == 0 && !is_reparse) || isListingOnly) {
        ret = SendRequest(command, 0, stat);
        is_send_request = TRUE;
        if (command != WRITE_BACKUP_FILE || isListingOnly) {
            goto END;
        }
    }

    //そもそもopen失敗してないか、またはシンボリックリンク処理じゃないかちぇっく
    if(stat->hFile == SYSCALL_ERR && !is_reparse){
        if(!is_stream){
            errno = stat->lastError;
            totalErrFiles++;
            totalErrTrans += file_size;
            stat->SetFileSize(0);

            ConfirmErr("ReadFileProc:open()",(MakeAddr(RestoreOpenFilePath(src, start_idx, dir_len), srcPrefixLen)),CEF_NORMAL);
            if(is_send_request && command == WRITE_BACKUP_FILE){
                SendRequest(WRITE_BACKUP_END,0,0);
            }
            ret = false;
            goto END;
        }
    }

    //シンボリックリンク？
    if (is_reparse) {
        char	rd[MAX_PATH + STR_NULL_GUARD];
        int		readlink_bytes = SYSCALL_ERR;					//readlink返り値
        readlink_bytes = readlink((char*)src,rd,sizeof(rd));
        //readlink失敗？
        if(readlink_bytes == SYSCALL_ERR){
            ConfirmErr("ReadFileProc:readlink()",(char*)src,CEF_REPARASE);
            totalErrFiles++;
            totalErrTrans += file_size;
            ret = FALSE;
            goto END;
        }
        //readlink()はけつに\0入れてくれないので自分で入れる。
        rd[readlink_bytes] = '\0';
        readlink_bytes++;

        //シンボリックリンク先文字列サイズをstatにぶらさげ
        stat->repSize = readlink_bytes;

        //文字列バッファ確保
        if (PrepareReqBuf(offsetof(ReqHeader, stat) + stat->minSize, stat->repSize,
                stat->fileID, &req_buf) == FALSE) {
            ConfirmErr("ReadFileProc:PrepareReqBuf()",(char*)src,CEF_NOAPI|CEF_REPARASE);
            totalErrFiles++;
            totalErrTrans += file_size;
            ret = FALSE;
            goto END;
        }
        memcpy(req_buf.buf, rd, stat->repSize);
        SendRequest(command, &req_buf, stat);
        is_send_request = TRUE;
    }
    else {
        //::SetFilePointer(stat->hFile, 0, NULL, FILE_BEGIN);
        //とりあえず先頭にセット
        if(lseek(stat->hFile,0,SEEK_SET) == SYSCALL_ERR){
            //debug
            ERRNO_OUT(errno,"lseek");
        }
        BOOL	prepare_ret = TRUE;

        for (_int64 remain_size=file_size; remain_size > 0; remain_size -= trans_size) {
            trans_size = 0;

            while (1) {
                if ((ret = PrepareReqBuf(offsetof(ReqHeader, stat) +
                            (is_send_request && !is_digest ? 0 : stat->minSize), remain_size,
                            stat->fileID, &req_buf)) == FALSE) {
                    prepare_ret = FALSE;
                    break;
                }
                //xattrデータ読み込み要求じゃない？
                if(stat->dwFileAttributes != 0){
                    ret = ReadFileWithReduce(stat->hFile,
                                             req_buf.buf,
                                             req_buf.bufSize,
                                             &trans_size,
                                             NULL);
                }
                //xattrデータ読み込み要求
                else{
                    //対象xattrデータのサイズが1MB以下？(最小のメインバッファ単位以下？)
                    if(file_size < MIN_BUF){
                        //xattr分割送信不要なので、メインバッファに直接載せる
                        ret = ReadFileXattr(stat->hFile,
                                            (char*)stat->cFileName,
                                            req_buf.buf,
                                            file_size,
                                            &trans_size);
                    }
                    else{
                        //要xattr分割送信なので、メインバッファには分割して格納する
                        //今のところここに来るのはエイリアスのコピーの時のみ
                        ret = ReadFileXattrWithReduce(stat->hFile,
                                                      (char*)stat->cFileName,
                                                      req_buf.buf,
                                                      req_buf.bufSize,
                                                      &trans_size,
                                                      file_size,
                                                      remain_size);
                    }
                }
                break;
            }
            if (!ret || trans_size != (DWORD)req_buf.bufSize && trans_size < remain_size) {
                ret = FALSE;
                totalErrFiles++;
                totalErrTrans += file_size;
                if(is_stream && !(info.flags & REPORT_STREAM_ERROR)
                    || !prepare_ret /* dst でのエラー発生が原因の場合 */
                    || ConfirmErr(is_stream ? "ReadFile(xattr)" : ret && !trans_size ?
                        "ReadFile(truncate)" : "ReadFile",
                        MakeAddr(RestoreOpenFilePath(src, start_idx, dir_len), srcPrefixLen),
                        CEF_NORMAL) == Confirm::CONTINUE_RESULT){
                    stat->SetFileSize(0);
                    req_buf.bufSize = 0;
                    if((info.flags & CREATE_OPENERR_FILE) || is_send_request){
                        SendRequest(is_send_request ? WRITE_ABORT : command, &req_buf,
                                    is_send_request && !is_digest ? 0 : stat);
                        is_send_request = TRUE;
                    }
                }
                break;
            }

            // 逐次移動チェック（並列処理時のみ）
            if (!isSameDrv && info.mode == MOVE_MODE && remain_size > trans_size) {
                FlushMoveList(FALSE);
            }
            total.readTrans += trans_size;
            SendRequest(is_send_request ? WRITE_FILE_CONT : command, &req_buf,
                        is_send_request && !is_digest ? 0 : stat);
            is_send_request = TRUE;
            if (waitTick && remain_size > trans_size) Wait();
        }
    }

    if (command == WRITE_BACKUP_FILE) {
        CheckSuspend();
        while (ret && ext_ret && !isAbort && (*end_idx) < openFilesCnt
        && openFiles[(*end_idx)]->dwFileAttributes == 0) {
            ext_ret = ReadFileProc(*end_idx, end_idx, dir_len);
        }
        if (ret && ext_ret && stat->acl) {
            if ((ext_ret = PrepareReqBuf(offsetof(ReqHeader, stat) + stat->minSize,
                    stat->aclSize, stat->fileID, &req_buf))) {
                memcpy(req_buf.buf, stat->acl, stat->aclSize);
                ext_ret = SendRequest(WRITE_BACKUP_ACL, &req_buf, stat);
            }
        }
        if (ret && ext_ret && stat->ead) {
            if ((ext_ret = PrepareReqBuf(offsetof(ReqHeader, stat) + stat->minSize,
                    stat->eadSize, stat->fileID, &req_buf))) {
                memcpy(req_buf.buf, stat->ead, stat->eadSize);
                ext_ret = SendRequest(WRITE_BACKUP_EADATA, &req_buf, stat);
            }
        }
        CheckSuspend();
        if (!isAbort && is_send_request) {
            SendRequest(WRITE_BACKUP_END, 0, 0);
        }
    }

END:
    if(ret && info.mode == MOVE_MODE && !is_stream){
        CloseMultiFilesProc(*end_idx);
        int	path_len = dir_len + sprintfV(MakeAddr(src, dir_len), FMT_STR_V, stat->cFileName) + 1;
        PutMoveList(stat->fileID, src, path_len, file_size, stat->dwFileAttributes,
                    MoveObj::START);
    }
    CheckSuspend();
    return	ret && ext_ret && !isAbort;
}

void FastCopy::DstRequest(DstReqKind kind)
{
    cv.Lock();
    dstAsyncRequest = kind;
    cv.Notify();
    cv.UnLock();
}

BOOL FastCopy::WaitDstRequest(void)
{
    cv.Lock();
    CheckSuspend();
    while(dstAsyncRequest != DSTREQ_NONE && !isAbort){
        cv.Wait(CV_WAIT_TICK);
    }
    cv.UnLock();
    return	dstRequestResult;
}

BOOL FastCopy::CheckDstRequest(void)
{
    CheckSuspend();
    if(isAbort || dstAsyncRequest == DSTREQ_NONE){
        return FALSE;
    }

    if(!isSameDrv){
        cv.UnLock();
    }

    switch(dstAsyncRequest) {
    case DSTREQ_READSTAT:
        dstRequestResult = ReadDstStat();
        break;

    case DSTREQ_DIGEST:
        dstRequestResult = MakeDigest(confirmDst, &dstDigestBuf, &dstDigest, dstDigestVal);
        break;
    }

    if (!isSameDrv) {
        cv.Lock();
        cv.Notify();
    }
    dstAsyncRequest = DSTREQ_NONE;

    return	dstRequestResult;
}

BOOL FastCopy::ReadDstStat(void)
{
    int			num = 0, len;
    FileStat	*dstStat, **dstStatIdx;
    struct stat	fdat;
    BOOL		ret = TRUE;
    char	wk_fullpath[MAX_PATH + STR_NULL_GUARD];	//ファイルのパス合成用ワーク
    char	wk_pathname[MAX_PATH + STR_NULL_GUARD];	//パス名のワーク
    char	wk_filename[MAX_PATH + STR_NULL_GUARD];	//ファイル名のワーク
    int		wk_rtn = SYSCALL_ERR;

    struct dirent ** namelist = NULL;	// 当該ディレクトリのファイルエントリリスト
    int cdir_fnum = SYSCALL_ERR;		// 当該ディレクトリのファイルエントリ数

    dstStat = (FileStat *)dstStatBuf.Buf();
    dstStatIdx = (FileStat **)dstStatIdxBuf.Buf();
    dstStatBuf.SetUsedSize(0);
    dstStatIdxBuf.SetUsedSize(0);

    //errDirs加算のために一回statだすで
    if(stat((const char*)confirmDst,&fdat) == SYSCALL_ERR){

        // ファイル単体指定で、書き込み対象のディレクトリが存在しない以外のエラー？
        if(errno != ENOENT && !isExtendDir){
            ret = FALSE;
            total.errDirs++;
            ConfirmErr("ReadDstStat:stat()", MakeAddr(confirmDst, dstPrefixLen),CEF_DIRECTORY);
        }
        // ファイル名を指定してのコピーで、コピー先が見つからない場合は、
        // エントリなしで成功として処理。
        // 複数src指定またはdst末尾に/が指定されてる場合はディレクトリコピーするので
        // 成功として処理するよ
        goto END;
    }
    //stat成功
    else{
        //正規化済みのパスをパスとファイル名に分離
        apath_to_path_file_a((char*)confirmDst,&wk_pathname[0],&wk_filename[0]);

        /* directoryじゃない？ */
        if(!IsDir(fdat.st_mode)){

            //ファイル名が'.'または'..'？
            if (IsParentOrSelfDirs((void*)&wk_filename[0])){
                //"."と".."は除外
                goto END;
            }
            //".から始まるファイルをコピーしないオプションonかつ.から始まるファイル？ */
            else if(info.flags & SKIP_DOTSTART_FILE && IsDotStartFile((void*)&wk_filename[0])){
                //ユーザが意図的にチェックしたオプションなので、
                //各種skip数とかの統計には加算しないぞー。
                goto END;
            }
            dstStatIdx[num++] = dstStat;
            len = FdatToFileStat(&fdat, (FileStat *)dstStat,TRUE,wk_filename);
            dstStatBuf.AddUsedSize(len);
            dstStatIdxBuf.AddUsedSize(sizeof(FileStat *));
            if (dstStatBuf.RemainSize() <= maxStatSize && !dstStatBuf.Grow(MIN_ATTR_BUF)) {
                ConfirmErr("Can't alloc memory(dstStatBuf)", NULL, CEF_STOP);
            }
            if (dstStatIdxBuf.RemainSize() <= sizeof(FileStat *)
            && dstStatIdxBuf.Grow(MIN_ATTRIDX_BUF) == FALSE) {
                ConfirmErr("Can't alloc memory(dstStatIdxBuf)", NULL, CEF_STOP);
            }
            //ファイル一個だけなので、以降の処理を無視してケツに飛ばす。
            goto END;
        }
        /* directoryかつReadProc()延長で初めてのコールかつsrcのフォルダごとコピーする？ */
        else if((strlen((char*)confirmDst) - strlen(wk_filename)) == dstBaseLen && isExtendDir){

            /* FindFirstFileはdir単体を指定すると中のファイルエントリを調査せずにdirエントリだけ返す謎仕様 */
            /* FindFirstFileの挙動エミュレートのために、dir情報だけ登録してリターンしちゃうぞ */
            dstStatIdx[num++] = dstStat;
            len = FdatToFileStat(&fdat, (FileStat *)dstStat,TRUE,wk_filename);
            dstStatBuf.AddUsedSize(len);
            dstStatIdxBuf.AddUsedSize(sizeof(FileStat *));

            if (dstStatBuf.RemainSize() <= maxStatSize && !dstStatBuf.Grow(MIN_ATTR_BUF)) {
                ConfirmErr("Can't alloc memory(dirStatBuf)", NULL, CEF_STOP);
            }

            if (dstStatIdxBuf.RemainSize() <= sizeof(FileStat *)
            && dstStatIdxBuf.Grow(MIN_ATTRIDX_BUF) == FALSE) {
                ConfirmErr("Can't alloc memory(dstStatIdxBuf)", NULL, CEF_STOP);
            }
            /* けつに飛ばす */
            goto END;
        }
        //Dirなのでscandirにすすむよー
        //else{
        //}
    }

    //まずは指定ディレクトリ一覧とるで
    cdir_fnum = scandir((const char*)confirmDst,&namelist,NULL,alphasort);

    //そもそもscandir自体失敗してる？
    if(cdir_fnum == SYSCALL_ERR){

        ret = false;
        total.errDirs++;
        ConfirmErr("ReadDstStat(scandir)", MakeAddr(confirmDst, dstPrefixLen),CEF_DIRECTORY);
        goto END;
    }

    for(int i=0;i<cdir_fnum;i++){

        if (IsParentOrSelfDirs(&namelist[i]->d_name[0])){
            continue;
        }
        //".から始まるファイルをコピーしないオプションonかつ.から始まるファイル？ */
        else if(info.flags & SKIP_DOTSTART_FILE && IsDotStartFile(&namelist[i]->d_name[0])){
            //ユーザが意図的にチェックしたオプションなので、
            //各種skip数とかの統計には加算しないぞー。
            continue;
        }

        /* stat出すためにパス仮合成 */
        strcpy(wk_fullpath,(const char*)confirmDst);
        //合成後の結果がMAX_PATHを越える場合は強制打ち切りする。
        //MAX_PATH+1のパスを生成することでENAMETOOLONG発生を誘導
        strncat(wk_fullpath,(const char*)&namelist[i]->d_name[0],(MAX_PATH - strlen(wk_fullpath)));
        //qDebug() << "wk_fullpath=" << wk_fullpath;

        //リンクは実体としてコピー？
        if(info.flags & DIR_REPARSE || info.flags & FILE_REPARSE){
            wk_rtn = stat((const char*)wk_fullpath,&fdat);
        }
        //リンクとして調査
        else{
            wk_rtn = lstat((const char*)wk_fullpath,&fdat);
        }
        if(wk_rtn == SYSCALL_ERR){
            //失敗時のファイル加算は元のfastcopyはやってないというか、できないのでスルー。
            //ここで下手にカウントするとwinと仕様が違うっていう不良になるぞ！！
            if(IsDir3(namelist[i]->d_type)){
                total.errDirs++;
                ConfirmErr("ReadDstStat:(l)stat", wk_fullpath,CEF_DIRECTORY);
            }
            else{
                total.errFiles++;
                ConfirmErr("ReadDstStat:(l)stat", wk_fullpath,CEF_NORMAL);
            }
            //他のファイルは上手く行くかもしれんので続行
            continue;
        }
        //事前調査対象として適切なファイルかどうか判定。
        if(IsFile(fdat.st_mode) || IsDir(fdat.st_mode) || IsSlnk(fdat.st_mode)){
            dstStatIdx[num++] = dstStat;
            len = FdatToFileStat(&fdat, dstStat, TRUE,&namelist[i]->d_name[0]);
            dstStatBuf.AddUsedSize(len);

            // 次の stat 用 buffer のセット
            dstStat = (FileStat *)(dstStatBuf.Buf() + dstStatBuf.UsedSize());
            dstStatIdxBuf.AddUsedSize(sizeof(FileStat *));

            if (dstStatBuf.RemainSize() <= maxStatSize && dstStatBuf.Grow(MIN_ATTR_BUF) == FALSE) {
                ConfirmErr("Can't alloc memory(dstStatBuf)", NULL, CEF_STOP);
                break;
            }
            if (dstStatIdxBuf.RemainSize() <= sizeof(FileStat *)
                    && dstStatIdxBuf.Grow(MIN_ATTRIDX_BUF) == FALSE) {
                ConfirmErr("Can't alloc memory(dstStatIdxBuf)", NULL, CEF_STOP);
                break;
            }
        }
        else{
            //ブロックデバイス・スペシャルファイルとかその他もろもろのファイル種別。
            //qDebug("Other not supported. ReadDstStat path = %s",wk_fullpath);
            ConfirmErr("ReadDstStat:No Supported File Type.", wk_fullpath);
            continue;
        }
    }

END:
    //scandir成功してる分はfreeしとくべよ
    if(cdir_fnum != SYSCALL_ERR){
        while(cdir_fnum > 0){
            free(namelist[--cdir_fnum]);
            //cdir_fnum--;
        }
        free(namelist);
    }

    if (ret){
        ret = MakeHashTable();
    }

    return	ret;
}

BOOL FastCopy::MakeHashTable(void)
{
    int		num = dstStatIdxBuf.UsedSize() / sizeof(FileStat *);
    int		require_size = hash.RequireSize(num), grow_size;

    if((grow_size = require_size - dstStatIdxBuf.RemainSize()) > 0
        && dstStatIdxBuf.Grow(ALIGN_SIZE(grow_size, MIN_ATTRIDX_BUF)) == FALSE){
        ConfirmErr("Can't alloc memory(dstStatIdxBuf2)", NULL, CEF_STOP);
        return	FALSE;
    }

    return	hash.Init((FileStat **)dstStatIdxBuf.Buf(), num,
                dstStatIdxBuf.Buf() + dstStatIdxBuf.UsedSize());
}

int StatHash::HashNum(int data_num)
{
    return	data_num | 1;
}

BOOL StatHash::Init(FileStat **data, int data_num, void *tbl_buf)
{
    hashNum = HashNum(data_num);
    hashTbl = (FileStat **)tbl_buf;
    memset(hashTbl, 0, hashNum * sizeof(FileStat *));

    for (int i=0; i < data_num; i++) {
        FileStat **stat = hashTbl+(data[i]->hashVal % hashNum);
        while (*stat) {
            stat = &(*stat)->next;
        }
        *stat = data[i];
    }
    return	TRUE;
}

FileStat *StatHash::Search(void *upperName, DWORD hash_val)
{
    for (FileStat *target = hashTbl[hash_val % hashNum]; target; target = target->next) {
        if (target->hashVal == hash_val && strcmpV(target->upperName, upperName) == 0)
            return	target;
    }

    return	NULL;
}

/*=========================================================================
  関  数 ： DeleteThread
  概  要 ： DELETE_MODE 処理
  説  明 ：
  注  意 ：
=========================================================================*/
bool FastCopy::DeleteThread(void *fastCopyObj)
{
    return	((FastCopy *)fastCopyObj)->DeleteThreadCore();
}

BOOL FastCopy::DeleteThreadCore(void)
{
    if ((info.flags & PRE_SEARCH) && info.mode == DELETE_MODE){
        phase = PRESEARCHING;
        PreSearch();
    }
    phase = DELETING;

    for (int i=0; i < srcArray.Num() && !isAbort; i++) {
        if (InitDeletePath(i)){
            DeleteProc(dst, dstBaseLen,true);
        }
        CheckSuspend();
    }
    FinishNotify();
    return	TRUE;
}

//削除実処理
BOOL FastCopy::DeleteProc(void *path, int dir_len,bool isfirst)
{
    BOOL		ret = TRUE;
    int			rc  = SYSCALL_ERR;
    FileStat	stat;
    struct dirent ** namelist = NULL;	// 当該ディレクトリのファイルエントリリスト
    int cdir_fnum = 0;					// 当該ディレクトリのファイルエントリ数
    struct stat	fdat;

    char	wk_pathname[MAX_PATH + STR_NULL_GUARD];	// パス名のワーク
                                                    // 兼 DT_UNKNOWN時のパス合成ワーク
    char	wk_filename[MAX_PATH + STR_NULL_GUARD];	/* ファイル名のワーク		*/
    char	wk_root_delpath[MAX_PATH + STR_NULL_GUARD];	/* 大元src削除用退避ワーク	*/
    int wk_rtn = SYSCALL_ERR;

    wk_rtn = lstat((const char*)path,&fdat);

    if(wk_rtn == SYSCALL_ERR){

        //失敗なら加算してリターン
        //課題：これ、エラー時にファイルなんだかディレクトリなんだか判別つかないなあ。
        //とりあえずファイルってことにしとくか。unixではディレクトリもファイルって言えなくもないし！
        ret = false;
        total.errFiles++;
        //ERRNO_OUT(errno,"DeleteProc:lstat()");
        ConfirmErr("DeleteProc:(l)stat", MakeAddr(dst,dstPrefixLen),CEF_DIRECTORY);
        return ret;
    }
    if(IsFile(fdat.st_mode) || IsSlnk(fdat.st_mode)){
        //正規化済みのパスをパスとファイル名に分離
        apath_to_path_file_a((char*)src,&wk_pathname[0],&wk_filename[0]);

        stat.nFileSize = fdat.st_size;
        stat.dwFileAttributes = fdat.st_mode;
        DeleteFileProc(path,dir_len,wk_filename,&stat);
        //ファイル単体はささっと飛ばしちゃう
        goto end;
    }

    //初回コールの場合はワークにsrcの内容退避しとく
    if(isfirst){
        strcpy(wk_root_delpath,(char*)path);
        //けつの'/'を除去
        SetChar(wk_root_delpath,strlen(wk_root_delpath) - 1,0);
    }
    //まずは指定ディレクトリ一覧とるで
    cdir_fnum = scandir((char*)path,&namelist,NULL,alphasort);

    if(cdir_fnum == SYSCALL_ERR){
        //読み込み元ディレクトリが読めない場合は続行不能
        ret = false;
        ConfirmErr("DeleteProc:scandir()", MakeAddr(path, dir_len),CEF_DIRECTORY);
        goto end;
    }
    //取得できたファイル数でループ
    for(int i=0;i<cdir_fnum;i++){

        if (IsParentOrSelfDirs((void*)&namelist[i]->d_name[0])){
            //"."と".."は除外
            continue;
        }

        //のちのDelete*Proc関数でpath中身書き換えられちゃうので、カレントディレクトリ記憶し続けるために
        //書き換えられる前の位置まで戻す。
        SetChar(path, dir_len, 0);

        //lstat出してファイル固有の情報確認
        strcpy(wk_pathname,(char*)path);
        strcat(wk_pathname,(const char*)&namelist[i]->d_name[0]);

        rc = lstat(wk_pathname,&fdat);

        if(rc == SYSCALL_ERR){

            ConfirmErr("DeleteProc(lstat)", MakeAddr(path,dir_len));
            continue;
        }

        if ((dstBaseLen != dir_len || isMetaSrc
                || !IsDir(fdat.st_mode))
                        /* 課題:FilterCheckはダミーでコール */
                        && !FilterCheck(path,
                                        namelist[i]->d_name,
                                        fdat.st_mode,
                                        fdat.st_mtim.tv_sec,
                                        fdat.st_size,
                                        FASTCOPY_FILTER_FLAGS_STAT)){
            total.filterDelSkips++;
            //qDebug("delskipcnt = %d,skipfile = %s",total.filterDelSkips,namelist[i]->d_name);
            continue;
        }

        stat.nFileSize = fdat.st_size;
        stat.dwFileAttributes = fdat.st_mode;

        //削除対象がディレクトリ？
        if(IsDir(fdat.st_mode)){
            DeleteDirProc(path,dir_len,&namelist[i]->d_name[0],&stat);
        }
        //削除対象がファイルまたはシンボリックリンク？
        else if(IsFile(fdat.st_mode) || IsSlnk(fdat.st_mode)){
            DeleteFileProc(path,dir_len,&namelist[i]->d_name[0],&stat);
        }
        else {
            //元々コピー対象外のファイルなのにエラー扱いにするのもなんかなあ。。まあとりあえずは安全に倒しておくか。
            ConfirmErr("Deleteproc:lstat() not support file.)",wk_pathname,CEF_NORMAL);
        }
    }

    //Find***File APIでは元ディレクトリ自身も削除対象としてDeleteDirProc内で処理されるが、
    //scandir化によって大元ディレクトリ削除しなくなるため、ここで吸収。
    //大元ディレクトリを指定した最初のDeleteProcコール？
    if(isfirst){
        //大元ディレクトリの削除処理ここで頑張る
        //中身はDeleteDirProcのパチリよ
        if (!isListingOnly) {
            if(rmdir(wk_root_delpath) == SYSCALL_ERR){
                //filterモード有効な時は最後のディレクトリ削除成功しない場合がある。
                //この場合の自ディレクトリ削除できないエラーはエラー扱いにしない。(win版と挙動合わせ)
                if(filterMode && errno == ENOTEMPTY){
                    //フィルタによる削除の結果、まだ子ディレクトリにファイルが存在するのでエラーじゃない。
                }
                else{
                    total.errDirs++;
                    ConfirmErr("DeleteProc:rmdir()", MakeAddr(wk_root_delpath, dstPrefixLen),CEF_DIRECTORY);
                }
                goto end;
            }
        }
        if(isListing){
            PutList(MakeAddr(wk_root_delpath, dstPrefixLen), PL_DIRECTORY|PL_DELETE);
        }
        total.deleteDirs++;
    }

end:
    // scandir確保領域の解放
    for(int i=0;i < cdir_fnum;i++){
        free(namelist[i]);
    }
    free(namelist);

    CheckSuspend();
    return	ret && !isAbort;

}

BOOL FastCopy::DeleteDirProc(void *path, int dir_len, void *fname, FileStat *stat,BOOL isfirst)
{
    int		new_dir_len = dir_len + sprintfV(MakeAddr(path, dir_len), FMT_CAT_ASTER_V, fname); -1 ;
    BOOL	ret = TRUE;
    int		cur_skips = total.filterDelSkips;
    BOOL	is_reparse = IsReparse(stat->dwFileAttributes);

    if (info.mode == DELETE_MODE && (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))) {
        memcpy(MakeAddr(confirmDst, dir_len), MakeAddr(path, dir_len),
            //'¥*'の分の文字列をさっ引く(windowsでは)
            //(new_dir_len - dir_len + 2) * CHAR_LEN_V);
            //末尾の'/'文字を差っ引く
            (new_dir_len - dir_len + 1) * CHAR_LEN_V);
    }

    if (!is_reparse) {
        ret = DeleteProc(path, new_dir_len,isfirst);
        CheckSuspend();
        if (isAbort){
            return	ret;
        }
    }

    if (cur_skips != total.filterDelSkips
    || (filterMode && info.mode == DELETE_MODE && (info.flags & DELDIR_WITH_FILTER) == 0))
        return	ret;

    // けつの'/'をとる
    SetChar(path, new_dir_len - 1, 0);

    if (!isListingOnly) {
        void	*target = path;

        if (info.mode == DELETE_MODE && (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))) {
            if (RenameRandomFname(path, confirmDst, dir_len, new_dir_len-dir_len - 1)) {
                target = confirmDst;
            }
        }
        if(rmdir((const char*)target) == SYSCALL_ERR){
            //printf("error caused FILE=%s,LINE=%d,errno=%d",__FILE__,__LINE__,errno);
            total.errDirs++;
            return	ConfirmErr("DeleteDirProc:rmdir()", MakeAddr(target, dstPrefixLen)), FALSE;
        }
    }
    if (isListing) {
        PutList(MakeAddr(path, dstPrefixLen), PL_DIRECTORY|PL_DELETE|(is_reparse ? PL_REPARSE:0));
    }
    total.deleteDirs++;
    return	ret;
}

BOOL FastCopy::DeleteFileProc(void *path, int dir_len, void *fname, FileStat *stat)
{

    int		len = sprintfV(MakeAddr(path, dir_len), FMT_STR_V, fname);
    BOOL	is_reparse = IsReparse(stat->dwFileAttributes);

    if (!isListingOnly) {
        void	*target = path;

        if (info.mode == DELETE_MODE && (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))
        && !is_reparse) {
            if (RenameRandomFname(target, confirmDst, dir_len, len)) {
                target = confirmDst;
            }
            if (stat->FileSize()) {
                if (WriteRandomData(target, stat, TRUE) == FALSE) {
                    total.errFiles++;
                    return	ConfirmErr("OverWrite", MakeAddr(target, dstPrefixLen)), FALSE;
                }
                total.writeFiles++;
            }
        }
        if(unlink((const char*)target) == SYSCALL_ERR){
            total.errFiles++;
            return	ConfirmErr("DeleteFileProc:unlink()", MakeAddr(target, dstPrefixLen)), FALSE;
        }
    }
    if (isListing) PutList(MakeAddr(path, dstPrefixLen), PL_DELETE|(is_reparse ? PL_REPARSE : 0),0,NULL,stat->FileSize());

    total.deleteFiles++;
    total.deleteTrans += stat->FileSize();
    return	TRUE;
}


/*=========================================================================
  関  数 ： RDigestThread
  概  要 ： RDigestThread 処理
  説  明 ：
  注  意 ：
=========================================================================*/
bool FastCopy::RDigestThread(void *fastCopyObj)
{
    return	((FastCopy *)fastCopyObj)->RDigestThreadCore();
}


BOOL FastCopy::RDigestThreadCore(void)
{
    _int64	fileID = 0;
    _int64	remain_size;

    cv.Lock();

    while (!isAbort) {

        while (rDigestReqList.IsEmpty() && !isAbort) {
            cv.Wait(CV_WAIT_TICK);
            CheckSuspend();
        }
        ReqHeader *req = rDigestReqList.TopObj();
        if (!req || isAbort) {
            break;
        }
        Command cmd = req->command;

        FileStat	*stat = &req->stat;
        if ((cmd == WRITE_FILE || cmd == WRITE_BACKUP_FILE
                || (cmd == WRITE_FILE_CONT && fileID == stat->fileID))
            && stat->FileSize() > 0 && (!IsReparse(stat->dwFileAttributes)
            || (info.flags & FILE_REPARSE))) {
            cv.UnLock();
            if (fileID != stat->fileID) {
                fileID = stat->fileID;
                remain_size = stat->FileSize();
                srcDigest.Reset();
            }

            int		trans_size = (int)(req->bufSize < remain_size ? req->bufSize : remain_size);
            srcDigest.Update(req->buf, trans_size);

            if ((remain_size -= trans_size) <= 0) {
                srcDigest.GetVal(stat->digest);
                if (remain_size < 0) {
                    ConfirmErr("Internal Error(digest)", NULL, CEF_STOP|CEF_NOAPI|CEF_NORMAL);
                }
            }
            cv.Lock();
        }
        rDigestReqList.DelObj(req);

        if (isSameDrv) {
            readReqList.AddObj(req);
//			if (rDigestReqList.IsEmpty()) {
                cv.Notify();
//			}
        }
        else {
            writeReqList.AddObj(req);
            cv.Notify();
        }
        if (cmd == REQ_EOF) {
            break;
        }
        CheckSuspend();
    }

    cv.UnLock();

    return	isAbort;
}

/*=========================================================================
  概  要 ： 上書き削除用ルーチン
=========================================================================*/
void FastCopy::SetupRandomDataBuf(void)
{
    RandomDataBuf	*data = (RandomDataBuf *)mainBuf.Buf();

    data->is_nsa = (info.flags & OVERWRITE_DELETE_NSA) ? TRUE : FALSE;
    data->base_size = max(PAGE_SIZE, dstSectorSize);
    data->buf_size = mainBuf.Size() - data->base_size;
    data->buf[0] = mainBuf.Buf() + data->base_size;

    if (data->is_nsa) {
        data->buf_size /= 3;
        data->buf_size = (data->buf_size / data->base_size) * data->base_size;
        data->buf_size = min(info.maxTransSize, data->buf_size);

        data->buf[1] = data->buf[0] + data->buf_size;
        data->buf[2] = data->buf[1] + data->buf_size;
        if (info.flags & OVERWRITE_PARANOIA) {

            TGenRandom(data->buf[0], data->buf_size);
            TGenRandom(data->buf[1], data->buf_size);

        }
        else {
            for (int i=0, max=data->buf_size / sizeof(int) * 2; i < max; i++) {
                *((int *)data->buf[0] + i) = rand();
            }
        }
        memset(data->buf[2], 0, data->buf_size);
    }
    else {
        data->buf_size = min(info.maxTransSize, data->buf_size);
        if (info.flags & OVERWRITE_PARANOIA) {
            TGenRandom(data->buf[0], data->buf_size);
        }
        else {
            for (int i=0, max=data->buf_size / sizeof(int); i < max; i++) {
                *((int *)data->buf[0] + i) = rand();
            }
        }
    }
}

void FastCopy::GenRandomName(void *path, int fname_len, int ext_len)
{
    static char *char_dict = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    for (int i=0; i < fname_len; i++) {
        SetChar(path, i, char_dict[(rand() >> 4) % 62]);
    }
    if (ext_len) {
        SetChar(path, fname_len - ext_len, '.');
    }
    SetChar(path, fname_len, 0);
}

BOOL FastCopy::RenameRandomFname(void *org_path, void *rename_path, int dir_len, int fname_len)
{
    void	*fname = MakeAddr(org_path, dir_len);
    void	*rename_fname = MakeAddr(rename_path, dir_len);
    void	*dot = strrchrV(fname, '.');
    int		ext_len = dot ? fname_len - DiffLen(dot, fname) : 0;

    for (int i=fname_len; i <= MAX_FNAME_LEN; i++) {
        for (int j=0; j < 128; j++) {
            GenRandomName(rename_fname, i, ext_len);
            //適当なファイル名にリネーム
            if(QFile::rename((char*)org_path,(char*)rename_path)){
                return	TRUE;
            }
            //コピーしようとしたけど既にファイル存在してるばあいはエラーにしないって処理したいんだけど、。
            //QFile::renameのリターンは既に存在するなのか別のエラーなのか判断つかない。。
            //renameしたら被っちゃった場合でもfalseリターンとする。
            else{
                return	FALSE;
            }
        }
    }
    return	FALSE;
}

BOOL FastCopy::WriteRandomData(void *path, FileStat *stat, BOOL skip_hardlink)
{
    BOOL	is_nonbuf = (stat->FileSize() >= PAGE_SIZE
                         || (stat->FileSize() % dstSectorSize) == 0)
                            && (info.flags & USE_OSCACHE_WRITE) == 0 ? TRUE : FALSE;

    DWORD	flg = (is_nonbuf ? F_NOCACHE : 0);

    DWORD	trans_size;
    BOOL	ret = TRUE;
    int		hFile = open((char*)path,O_RDWR,0777);

    if (hFile == SYSCALL_ERR) {
        return	ConfirmErr("Write by Random Data(open)", MakeAddr(path, dstPrefixLen),CEF_NORMAL), FALSE;
    }

    if (waitTick) Wait((waitTick + 9) / 10);

    struct stat	bhi;

    if (!skip_hardlink || lstat((char*)path,&bhi) != SYSCALL_ERR || bhi.st_nlink <= 1) {
        RandomDataBuf	*data = (RandomDataBuf *)mainBuf.Buf();
        _int64			file_size = is_nonbuf ? ALIGN_SIZE(stat->FileSize(), dstSectorSize)
                                            :   stat->FileSize();

        for (int i=0, end=data->is_nsa ? 3 : 1; i < end && ret; i++) {

            lseek(hFile,0,SEEK_SET);

            for (_int64 remain_size=file_size; remain_size > 0; remain_size -= trans_size) {
                int	max_trans = waitTick && (info.flags & AUTOSLOW_IOLIMIT) ?
                                MIN_BUF : (int)maxWriteSize;
                max_trans = min(max_trans, data->buf_size);
                int	io_size = remain_size > max_trans ?
                                max_trans : (int)ALIGN_SIZE(remain_size, dstSectorSize);

                if (!(ret = WriteFileWithReduce(hFile, data->buf[i], io_size,
                        &trans_size, NULL))) {
                    break;
                }
                total.writeTrans += trans_size;
                if (waitTick) Wait();
            }
            total.writeTrans -= file_size - stat->FileSize();
            fsync(hFile);
        }
        lseek(hFile,0,SEEK_SET);
        ftruncate(hFile,file_size);
    }

    if(hFile != SYSCALL_ERR){
        if(close(hFile) == SYSCALL_ERR){
            ConfirmErr("WriteRandomData:close()",(char*)path,CEF_NORMAL);
        }
    }
    return	ret;
}


/*=========================================================================
  関  数 ： WriteThread
  概  要 ： Write 処理
  説  明 ：
  注  意 ：
=========================================================================*/
bool FastCopy::WriteThread(void *fastCopyObj)
{
    return	((FastCopy *)fastCopyObj)->WriteThreadCore();
}

BOOL FastCopy::WriteThreadCore(void)
{
    // トップレベルディレクトリが存在しない場合は作成
    CheckAndCreateDestDir(dstBaseLen);

    BOOL	ret = WriteProc(dstBaseLen);

    cv.Lock();
    writeReq = NULL;
    writeReqList.Init();
    cv.Notify();
    cv.UnLock();

    return	ret;
}

// dstに指定されたディレクトリまでのディレクトリが存在しない場合ディレクトリ作成する。
BOOL FastCopy::CheckAndCreateDestDir(int dst_len)
{
    BOOL	ret = TRUE;
    struct stat wk_stat;		//ワーク用のstat

    //directory作成用に末尾の'/'いったんとるのね
    SetChar(dst, dst_len - 1, 0);

    if(stat((const char *)dst,&wk_stat) == SYSCALL_ERR && errno == ENOENT){
        int	parent_dst_len = 0;

        const char *cur = (const char *)MakeAddr(dst, 0);;
        const char *end = (const char *)MakeAddr(dst, dst_len);

        while (cur < end) {
            if (lGetCharIncA(&cur) == '/') {
                parent_dst_len = (int)(cur - (const char *)dst);
            }
        }
        if (parent_dst_len < 0) parent_dst_len = 0;

        ret = parent_dst_len ? CheckAndCreateDestDir(parent_dst_len) : FALSE;

        if (isListingOnly || !mkdir((const char*)dst,0777) && isListing) {
            PutList(MakeAddr(dst, dstPrefixLen), PL_DIRECTORY);
            total.writeDirs++;
        }
        else ret = FALSE;
    }
    //末尾の'/'戻すのね
    SetChar(dst, dst_len - 1, '/');

    return	ret;
}

BOOL FastCopy::FinishNotify(void)
{
    //endTick = ::GetTickCount();
    endTick = elapse_timer->elapsed();
    //MainWindowへの終了制御要求をPOST
    QApplication::postEvent((QObject*)info.hNotifyWnd,
                    new FastCopyEvent(END_NOTIFY));
    return	true;
}

BOOL FastCopy::WriteProc(int dir_len)
{
    BOOL	ret = TRUE;
    struct stat dststat;
    int			rc = 0;

    while (!isAbort && (ret = RecvRequest())) {

        if (writeReq->command == REQ_EOF) {
            CheckSuspend();
            if (IsUsingDigestList() && !isAbort) {
                phase = VERIFYING;
                CheckDigests(CD_FINISH);
            }
            break;
        }
        if (writeReq->command == WRITE_FILE || writeReq->command == WRITE_BACKUP_FILE
        || writeReq->command == CREATE_HARDLINK) {
            int		new_dst_len;
            if (writeReq->stat.renameCount == 0){
                new_dst_len = dir_len + sprintfV(MakeAddr(dst, dir_len), FMT_STR_V,
                                        writeReq->stat.cFileName);
            }
            else{
                new_dst_len = dir_len + MakeRenameName(MakeAddr(dst, dir_len),
                                        writeReq->stat.renameCount, writeReq->stat.cFileName);
            }
            if (mkdirQueueBuf.UsedSize()) {
                ExecDirQueue();
            }
            if (isListingOnly) {
                if (writeReq->command == CREATE_HARDLINK) {
                    PutList(MakeAddr(dst, dstPrefixLen), PL_HARDLINK);
                    total.linkFiles++;
                }
                else {
                    //verifyモード時は、dstに存在しないファイルがあったらエラーとする
                    if(info.mode == VERIFY_MODE){
                        //エラー要因を確認
                        //対象がシンボリックリンクでも中身をコピー？
                        if(info.flags & DIR_REPARSE || info.flags & FILE_REPARSE){
                            rc = stat((char*)dst,&dststat);
                        }
                        else{
                            rc = lstat((char*)dst,&dststat);
                        }
                        //stat失敗？
                        if(rc == SYSCALL_ERR){
                            //dstの対象ファイルがそもそも存在しない
                            if(errno == ENOENT){
                                //独自エラーコードで上書き
                                rc = IDS_VERIFY_FILE_MISSING_ERR;
                            }
                            //存在しない以外のエラーはなにが違うか確認不能、とりあえずerrno出力しておくか。
                        }
                        else{
                            //ファイルサイズが違う？
                            if(writeReq->stat.FileSize() != dststat.st_size){
                                rc = IDS_VERIFY_FILE_SIZE_ERR;
                            }
                            //通算秒の値が異なる？または通算秒が同一でnanosec単位が異なる？
                            //srcとdstのファイルシステムが異ると、nanosecの精度違いで丸めがおきちゃうかも。
                            //安全のために秒までで判断するかあ。
                            /*
                            else if(writeReq->stat.ftLastWriteTime.tv_sec != dststat.st_mtim.tv_sec){
                                rc = IDS_VERIFY_FILE_DATE_ERR;
                            }
                            */
                            else{
                                //内部矛盾
                                ret = false;
                                ConfirmErr("Internal Error(VERIFY_FILE_ERROR)", dst, CEF_STOP|CEF_NOAPI|CEF_NORMAL);
                                break;
                            }
                        }
                        total.errFiles++;
                        ConfirmErr(rc != SYSCALL_ERR ? (char*)GetLoadStrV(rc) : "WriteProc:(l)stat",
                                   dst,
                                   rc != SYSCALL_ERR ? CEF_NOAPI|CEF_NORMAL : CEF_NORMAL);
                    }
                    //Listingモードなので+付与して出力
                    else{
                        //シンボリックリンクのlistingの場合はPL_REPARSE付与ね
                        PutList(MakeAddr(dst, dstPrefixLen),
                                IsReparse(writeReq->stat.dwFileAttributes) ? PL_NORMAL|PL_REPARSE:PL_NORMAL,
                                0,
                                NULL,
                                IsReparse(writeReq->stat.dwFileAttributes) ? SYSCALL_ERR : writeReq->stat.FileSize());
                    }
                    total.writeFiles++;
                    total.writeTrans += writeReq->stat.FileSize();
                }
                if (info.mode == MOVE_MODE) {
                    SetFinishFileID(writeReq->stat.fileID, MoveObj::DONE);
                }

            }
            else if ((ret = WriteFileProc(new_dst_len)), isAbort) {
                break;
            }
        }
        else if (writeReq->command == CASE_CHANGE) {
            ret = CaseAlignProc(dir_len);
        }
        else if (writeReq->command == MKDIR || writeReq->command == INTODIR) {
            ret = WriteDirProc(dir_len);
        }
        else if (writeReq->command == DELETE_FILES) {
            if (IsDir(writeReq->stat.dwFileAttributes)){
                ret = DeleteDirProc(dst, dir_len, writeReq->stat.cFileName, &writeReq->stat);
            }
            else{
                ret = DeleteFileProc(dst, dir_len, writeReq->stat.cFileName, &writeReq->stat);
            }
        }
        else if (writeReq->command == RETURN_PARENT) {
            break;
        }
        else {
            switch (writeReq->command) {
            case WRITE_ABORT: case WRITE_FILE_CONT:
            case WRITE_BACKUP_ACL: case WRITE_BACKUP_EADATA:
            case WRITE_BACKUP_ALTSTREAM: case WRITE_BACKUP_END:
                break;

            default:
                ret = FALSE;
                WCHAR cmd[2] = { writeReq->command + '0', 0 };
                ConfirmErr("Illegal Request (internal error)", cmd, CEF_STOP|CEF_NOAPI);
                break;
            }
        }
        CheckSuspend();
    }
    return	ret && !isAbort;
}

//Linux環境では未使用関数
BOOL FastCopy::CaseAlignProc(int dir_len)
{
    int ret;

    if (dir_len >= 0) {
        sprintfV(MakeAddr(dst, dir_len), FMT_STR_V, writeReq->stat.cFileName);
    }

//	PutList(MakeAddr(dst, dstPrefixLen), PL_CASECHANGE);

    if (isListingOnly) return TRUE;

    /* CaseAlignProc自体呼ばれないのでコメントアウトしちゃお
     * ret = copyfile((const char*)dst,(const char*)dst,NULL,
                   COPYFILE_ALL | COPYFILE_MOVE);
    */
    /*
    return	MoveFileV(dst, dst);
    */

    return((ret == 0)? true : false);
}

BOOL FastCopy::WriteDirProc(int dir_len)
{
    BOOL		ret = TRUE;
    BOOL		is_mkdir = writeReq->command == MKDIR;
    BOOL		is_reparse = IsReparse(writeReq->stat.dwFileAttributes)
                             && (info.flags & DIR_REPARSE) == 0;
    int			buf_size = writeReq->bufSize;
    int			new_dir_len;
    FileStat	sv_stat;
    struct stat dststat;
    int			rc = 0;

    memcpy(&sv_stat, &writeReq->stat, offsetof(FileStat, cFileName));

    if (writeReq->stat.renameCount == 0){
        new_dir_len = dir_len + sprintfV(MakeAddr(dst, dir_len), FMT_STR_V,
                                writeReq->stat.cFileName);
    }
    else{
        new_dir_len = dir_len + MakeRenameName(MakeAddr(dst, dir_len),
                                writeReq->stat.renameCount, writeReq->stat.cFileName);
    }
    if(buf_size){

        //課題:dirのxattr分割送信対応までとりあえず
        if(dstDirExtBuf.RemainSize() < MIN_DSTDIREXT_BUF
            && !dstDirExtBuf.Grow(ALIGN_SIZE(buf_size,MIN_DSTDIREXT_BUF))) {
            ConfirmErr("Can't alloc memory(dstDirExtBuf)", NULL, CEF_STOP);
            goto END;
        }
        memcpy(dstDirExtBuf.Buf() + dstDirExtBuf.UsedSize(), writeReq->buf, buf_size);
        sv_stat.acl = dstDirExtBuf.Buf() + dstDirExtBuf.UsedSize();
        sv_stat.ead = sv_stat.acl + sv_stat.aclSize;
        sv_stat.rep = sv_stat.ead + sv_stat.eadSize;
        dstDirExtBuf.AddUsedSize(buf_size);
    }

    if (is_mkdir) {
        if (is_reparse || (info.flags & SKIP_EMPTYDIR) == 0) {
            if(mkdirQueueBuf.UsedSize()){
                ExecDirQueue();
            }
            if(isListingOnly || !mkdir((const char*)dst, 0777)){
                if(isListing && !is_reparse){
                    if(info.mode == VERIFY_MODE){
                        total.errDirs++;
                        //mkdirするの確定 = dstdir存在せず
                        ConfirmErr((char*)GetLoadStrV(IDS_VERIFY_DIR_MISSING_ERR),
                                   dst,
                                   CEF_NOAPI | CEF_DIRECTORY);
                    }
                    else{
                        PutList(MakeAddr(dst, dstPrefixLen), PL_DIRECTORY);
                    }
                }
                total.writeDirs++;
            }
//			else total.skipDirs++;
        }
        else{
            if(mkdirQueueBuf.RemainSize() < sizeof(int)
                && mkdirQueueBuf.Grow(MIN_MKDIRQUEUE_BUF) == FALSE){
                ConfirmErr("Can't alloc memory(mkdirQueueBuf)", NULL, CEF_STOP);
                goto END;
            }
            *(int *)(mkdirQueueBuf.Buf() + mkdirQueueBuf.UsedSize()) = new_dir_len;
            mkdirQueueBuf.AddUsedSize(sizeof(int));
        }
    }
    strcpyV(MakeAddr(dst, new_dir_len), BACK_SLASH_V);

    if (!is_reparse) {
        CheckSuspend();
        if ((ret = WriteProc(new_dir_len + 1)), isAbort) {	// 再帰
            goto END;
        }
    }

    SetChar(dst, new_dir_len, 0);	// 末尾の '\\' を取る

    if (!is_mkdir || (info.flags & SKIP_EMPTYDIR) == 0 || mkdirQueueBuf.UsedSize() == 0
        || is_reparse) {
        // タイムスタンプ/ACL/属性/リパースポイントのセット
        if (isListingOnly || (ret = SetDirExtData(&sv_stat))) {
            if (isListing && is_reparse && is_mkdir) {
                PutList(MakeAddr(dst, dstPrefixLen), PL_DIRECTORY|PL_REPARSE);
            }
            //VERIFYONLYモードで既存ディレクトリが存在する?

            /* DIRのサイズ変更と変更日付をチェックしたいところだが、
               OSXではFinderで再作成しようとすると.DS_Storeを作ったりして親ディレクトリのサイズを変更してしまう。
               ので、DIRのチェックはさぼる。存在しない場合の検知だけで充分でしょ。
               Linuxの場合もとりあえずサボりとしておく。
            if(info.mode == VERIFY_MODE && !is_mkdir){
                DIRの更新日付チェックしてみる
                対象がシンボリックリンクでも中身をコピー？
                if(info.flags & DIR_REPARSE || info.flags & FILE_REPARSE){
                    rc = stat((char*)dst,&dststat);
                }
                else{
                    rc = lstat((char*)dst,&dststat);
                }
                ファイルサイズが違う？
                if(sv_stat.FileSize() != dststat.st_size){
                    rc = IDS_VERIFY_DIR_SIZE_ERR;
                }
                ファイルの最終更新時刻が異なる？
                else if(sv_stat.ftLastWriteTime.tv_sec != dststat.st_mtimespec.tv_sec
                        || sv_stat.ftLastWriteTime.tv_nsec != dststat.st_mtimespec.tv_nsec){
                    rc = IDS_VERIFY_DIR_DATE_ERR;
                }
                if(rc){
                    total.errDirs++;
                    ConfirmErr(rc != SYSCALL_ERR ? (char*)GetLoadStrV(rc) : "WriteDirProc:(l)stat",
                               dst,
                               rc != SYSCALL_ERR ? CEF_NOAPI:0);
                }
            }
            DIRチェックサボりここまで
            */
        }
    }

    if (buf_size) {
        dstDirExtBuf.AddUsedSize(-buf_size);
    }
    if (mkdirQueueBuf.UsedSize()) {
        mkdirQueueBuf.AddUsedSize(-(int)sizeof(int));
    }

END:
    CheckSuspend();
    return	ret && !isAbort;
}

BOOL FastCopy::ExecDirQueue(void)
{
    for (int offset=0; offset < mkdirQueueBuf.UsedSize(); offset += sizeof(int)) {
        int dir_len = *(int *)(mkdirQueueBuf.Buf() + offset);
        SetChar(dst, dir_len, 0);
        if(isListingOnly || !mkdir((const char*)dst, 0777)) {
            if (isListing){
                //verifyオンリーモード時は、dstに存在しないディレクトリがあったら強制的にエラー扱いとする
                if(info.mode == VERIFY_MODE){
                    total.errDirs++;
                    ConfirmErr((char*)GetLoadStrV(IDS_VERIFY_DIR_MISSING_ERR),dst,CEF_NOAPI|CEF_DIRECTORY);
                }
                else{
                    PutList(MakeAddr(dst, dstPrefixLen), PL_DIRECTORY);
                }
            }
            total.writeDirs++;
        }
//		else total.skipDirs++;
        //SetChar(dst, dir_len, '\\');
        SetChar(dst, dir_len, '/');
    }
    mkdirQueueBuf.SetUsedSize(0);
    return	TRUE;
}

BOOL FastCopy::SetDirExtData(FileStat *stat)
{

    BOOL	is_reparse = IsReparse(stat->dwFileAttributes) && (info.flags & DIR_REPARSE) == 0;
    BOOL	ret = TRUE;
    int rc	= 0;
    bool	sym_acl_flg = false;		//symlink作成時、symlink自身にaclつける？

    struct timeval timeval_wk[2];		//[0] = ファイルアクセス日更新用ワーク
                                        //[1] = ファイル変更日更新用ワーク
                                        //lutimes,utimesシステムコール用

    if(is_reparse && stat->rep) {
        char	rp[MAX_PATH + STR_NULL_GUARD];
        BOOL	is_set = TRUE;
        struct stat dst_stat;


        sym_acl_flg = true;

        if(lstat((char*)dst,&dst_stat) == SYSCALL_ERR){
            ret = FALSE;
            total.errDirs++;
            ConfirmErr("SetDirExtData(lstat)", (char*)dst,CEF_DIRECTORY);
        }
        else{
            if(IsSlnk(dst_stat.st_mode)){
                if(readlink((char*)dst,rp,sizeof(rp)) != SYSCALL_ERR){
                    //既存シンボリックリンクが同じところ指してる？
                    if(memcmp(rp, stat->rep,stat->repSize - STR_NULL_GUARD) == 0) {
                        //シンボリックリンク作り直しいらんよね
                        is_set = FALSE;
                        //作り直しいらなかったけど生成したことにしておくか
                        total.writeDirs++;
                    }
                }
            }
            //シンボリックリンク作成要求あり？
            if(is_set){
                //rmdirだとdstdirがdirectoryとして既に存在していてファイルがdstdir中に存在してる時に削除できない
                if(rmdir((char*)dst) != SYSCALL_ERR){;
                    if (symlink((char*)stat->rep,(char*)dst) == SYSCALL_ERR) {
                        ret = FALSE;
                        total.errDirs++;
                        ConfirmErr("SetDirExtData:symlink()", (char*)dst,CEF_DIRECTORY|CEF_REPARASE);
                    }
                    else{
                        total.writeDirs++;
                    }
                }
                else{
                    //強制削除失敗
                    ret = FALSE;
                    total.errDirs++;
                    ConfirmErr("SetDirExtData:rmdir()",(char*)dst,CEF_DIRECTORY|CEF_REPARASE);
                }
            }
        }
    }

    // xattr:
    //if(stat->ead){
    if(stat->eadSize){
        unsigned char* offset_pt = stat->ead;
        uint32_t xattrs_offset = 0;
        do{
            void* xattr_llpt;
            int*  xattr_datalen;
            //xattr dataのLLだけとりあえず取り出し。
            // xattrString(¥0保証不定長),xattrDatalen(int),xattrData(不定長)だぞ！
            // とりあえずll位置のポインタ取り出す
            xattr_llpt = offset_pt + strlen((const char*)offset_pt) + CHAR_LEN_V;
            // intポインタに入れとく
            xattr_datalen = (int*)xattr_llpt;
            //xattr設定
            rc = setxattr((char*)dst,
                           (const char*)offset_pt,
                           offset_pt + strlen((const char*)offset_pt) + CHAR_LEN_V + sizeof(int),
                           *xattr_datalen,
                           0);
            if(rc == SYSCALL_ERR){
                if(info.flags & REPORT_STREAM_ERROR){
                    ConfirmErr("SetDirExtData:setxattr()",
                                (char*)dst,
                                CEF_DIRECTORY,
                                (char*)GetLoadStrV(RAPIDCOPY_COUNTER_XATTR_ENOTSUP_EA),
                                (char*)offset_pt);
                }
                break;
            }
            else{
                total.writeTrans += *xattr_datalen;
            }

            unsigned char* prevoffset_pt = offset_pt;
            //offset_ptをネクストエントリに進める
            offset_pt = offset_pt +
                         strlen((const char*)offset_pt) + CHAR_LEN_V +						//xattrString
                         sizeof(int) +														//LL
                         *xattr_datalen;													//xattrData

            xattrs_offset += (uint32_t)(offset_pt - prevoffset_pt);

        }while(offset_pt != stat->ead + stat->eadSize);
    }

    //if (stat->acl){
    if (stat->aclSize){
        acl_t write_acl;
        //ACL contextのアドレスをワークに何となくコピー
        memcpy(&write_acl,stat->acl,stat->aclSize);

        if(!sym_acl_flg){
            rc = acl_set_file((const char *)dst,
                              ACL_TYPE_ACCESS,
                              write_acl);
        }
        //symlink
        else{
            /* LinuxではしんぼリックリンクにACL付与はできない
            rc = acl_set_link_np((const char *)dst,
                                 ACL_TYPE_ACCESS,
                                 write_acl);
            */
            rc = 0;
        }
        if(rc == SYSCALL_ERR){
            //ACLエラー報告有効？
            if(info.flags & REPORT_ACL_ERROR){
                if(errno == EOPNOTSUPP || errno == ENOTSUP){
                    ConfirmErr("SetDirExtData:acl_set_file(acl_set_link_np)",
                                MakeAddr(dst,dstPrefixLen),
                                CEF_DIRECTORY,
                                (char*)GetLoadStrV(RAPIDCOPY_COUNTER_ACL_EOPNOTSUPP));
                }
                else{
                    ConfirmErr("SetDirExtData:acl_set_file(acl_set_link_np)",MakeAddr(dst,dstPrefixLen));
                }
            }
        }

        //取得したacl contextを解放
        //課題:途中でエラーとか中断した時のwrite_aclの解放さぼってるので、途中キャンセルとかされると
        //		contextにゴミ残したままになっちゃう。キャンセルした瞬間に開いてるfdは2個しかないし、
        //		気にしないでいっかぁ。。
        rc = acl_free(write_acl);
        if(rc == SYSCALL_ERR){
            if(info.flags & REPORT_ACL_ERROR){
                ConfirmErr("SetDirExtData:acl_free",MakeAddr(dst,dstPrefixLen),CEF_DIRECTORY);
            }
        }
    }

    //utimes更新しないオプションの場合は変更日更新処理を行わない
    if(!(info.flags_second & FastCopy::DISABLE_UTIME)){
        // stat上はtimespecなんだけど、utimesがtimeval要求するので泣く泣くワーク変換
        //utimes発行してファイル日付を同期する
        TIMESPEC_TO_TIMEVAL(&timeval_wk[0],&stat->ftLastAccessTime);
        TIMESPEC_TO_TIMEVAL(&timeval_wk[1],&stat->ftLastWriteTime);
        //時刻更新
        if(is_reparse){
            if(::lutimes((char *) dst,&timeval_wk[0]) == SYSCALL_ERR){
                //FUSEを利用したユーザスペースファイルシステムではあっさりエラーになりえるので、
                //エラーチェックしないwindows版と同じ方針で。。
                //ERRNO_OUT(errno,"SetDirExtData:lutimes");
            }
        }
        else if(::utimes((char *)dst,&timeval_wk[0]) == SYSCALL_ERR){
            //FUSEを利用したユーザスペースファイルシステムではあっさりエラーになりえるので、
            //エラーチェックしないwindows版と同じ方針で。。
            //ERRNO_OUT(errno,"SetDirExtData:utimes");
        }
    }
    return	ret;
}

/*=========================================================================
  関  数 ： WDigestThread
  概  要 ： WDigestThread 処理
  説  明 ：
  注  意 ：
=========================================================================*/
bool FastCopy::WDigestThread(void *fastCopyObj)
{
    return	((FastCopy *)fastCopyObj)->WDigestThreadCore();
}

BOOL FastCopy::WDigestThreadCore(void)
{
    _int64			fileID = 0;
    BYTE			digest[SHA3_512SIZE];
    DigestCalc		*calc = NULL;
    DataList::Head	*head;

    wDigestList.Lock();

    while(1){
        while((!(head = wDigestList.Fetch())
        || (calc = (DigestCalc *)head->data)->status == DigestCalc::INIT) && !isAbort) {
            wDigestList.Wait(CV_WAIT_TICK);
        }
        CheckSuspend();
        if(isAbort)break;
        if(calc->status == DigestCalc::FIN){
            wDigestList.Get();
            wDigestList.Notify();
            break;
        }

        if(calc->status == DigestCalc::CONT || calc->status == DigestCalc::DONE){
            if(calc->dataSize){
                wDigestList.UnLock();
//				Sleep(0);
                if(fileID != calc->fileID){
                    dstDigest.Reset();
                }
                dstDigest.Update(calc->data, calc->dataSize);
                if(calc->status == DigestCalc::DONE){
                    dstDigest.GetVal(digest);
                }
//				total.verifyTrans += calc->dataSize;
                wDigestList.Lock();
            }
            fileID = calc->fileID;
        }

        if(calc->status == DigestCalc::DONE){
            if(calc->dataSize){
                if(memcmp(calc->digest, digest, dstDigest.GetDigestSize()) == 0){
                    // compare OK
                }
                else{
                    QString str;
                    QByteArray srcdigest((char*)calc->digest,dstDigest.GetDigestSize());
                    QByteArray dstdigest((char*)digest,dstDigest.GetDigestSize());
                    str.append((char*)GetLoadStrV(IDS_VERIFY_ERROR_WARN_1));
                    str.append(srcdigest.toHex());
                    str.append((char*)GetLoadStrV(IDS_VERIFY_ERROR_WARN_2));
                    str.append(dstdigest.toHex());
                    str.append((char*)GetLoadStrV(IDS_VERIFY_ERROR_WARN_3));
                    ConfirmErr(str.toLocal8Bit().data(), MakeAddr(calc->path, dstPrefixLen),CEF_NOAPI);
                    //debug このまま使うとバッファオーバフローになるぞ。buf[512]を増やして確認しろよ！
                    //strcat (buf," compare NG ");
                    //strcat (buf,(char*)calc->path);
                    //qDebug("%s",buf);
                    //debug
                    //ベリファイエラー検出した場合、dstのファイルを強制削除する。
                    //再度のコピーでスキップされないようにすることで事故防止。
                    switch(info.mode){
                        case DIFFCP_MODE:
                        case SYNCCP_MODE:
                        case MOVE_MODE:
                            unlink((char*)calc->path);		//エラーは無視
                            break;
                            //その他のモードの場合は削除しない。そもそもこないけど。。
                        case VERIFY_MODE:
                        case DELETE_MODE:
                        case MUTUAL_MODE:
                        default:
                            break;
                    }
                    calc->status = DigestCalc::ERR;
                }
            }
            else if (isListing){
                dstDigest.GetEmptyVal(calc->digest);
            }
        }

        if(calc->status == DigestCalc::DONE){
            if(isListing){
                PutList(MakeAddr(calc->path, dstPrefixLen),
                        PL_NORMAL,0,
                        calc->fileSize ? calc->digest : NULL,calc->fileSize);
            }
            if(info.mode == MOVE_MODE){
                SetFinishFileID(calc->fileID, MoveObj::DONE);
            }
            total.verifyFiles++;
        }
        else if(calc->status == DigestCalc::ERR){
            if(info.mode == MOVE_MODE){
                SetFinishFileID(calc->fileID, MoveObj::ERR);
            }
            total.errFiles++;
            if(isListing){
                PutList(MakeAddr(calc->path, dstPrefixLen), PL_NORMAL|PL_COMPARE, 0,calc->digest,calc->fileSize);
            }
        }
        wDigestList.Get();	// remove from wDigestList
        wDigestList.Notify();
        calc = NULL;
    }
    wDigestList.UnLock();

    CheckSuspend();
    return	isAbort;
}

FastCopy::DigestCalc *FastCopy::GetDigestCalc(DigestObj *obj, int io_size)
{
    DataList::Head	*head;
    DigestCalc		*calc = NULL;
    int				require_size = sizeof(DigestCalc) + (obj ? obj->pathLen : 0);

    CheckSuspend();
    if (wDigestList.Size() != wDigestList.MaxSize() && !isAbort) {
        BOOL	is_eof = FALSE;
        cv.Lock();
        if (writeReq && writeReq->command == REQ_EOF) {
            is_eof = TRUE;
        }
        cv.UnLock();

        if (is_eof) {
            mainBuf.FreeBuf();
        }

        if (!wDigestList.Grow(wDigestList.MaxSize() - wDigestList.Size())) {
            ConfirmErr("Can't alloc memory(digest)", NULL, CEF_STOP);
        }

        if (isAbort) return	NULL;
    }

    if (io_size) {
        require_size += io_size + dstSectorSize;
    }
    wDigestList.Lock();

    while (wDigestList.RemainSize() <= require_size && !isAbort) {
        wDigestList.Wait(CV_WAIT_TICK);
        CheckSuspend();
    }
    CheckSuspend();
    if (!isAbort && (head = wDigestList.Alloc(NULL, 0, require_size)) != NULL) {
        calc = (DigestCalc *)head->data;
        calc->status = DigestCalc::INIT;
        if (obj) {
            calc->fileID = obj->fileID;
            calc->fileSize = obj->fileSize;
            calc->data = (BYTE *)ALIGN_SIZE((DWORD)(calc->path + obj->pathLen), dstSectorSize);
            memcpy(calc->path, obj->path, obj->pathLen);
            if (obj->fileSize) {
                memcpy(calc->digest, obj->digest, dstDigest.GetDigestSize());
            }
        }
        else {
            calc->fileID = -1;
            calc->data = NULL;
        }
    }
    wDigestList.UnLock();

    return	calc;
}

BOOL FastCopy::PutDigestCalc(DigestCalc *obj, DigestCalc::Status status)
{
    wDigestList.Lock();
    obj->status = status;
    wDigestList.Notify();
    wDigestList.UnLock();

    return	TRUE;
}

BOOL FastCopy::MakeDigestAsync(DigestObj *obj)
{

    int			mode = ((info.flags & USE_OSCACHE_READVERIFY) ? 0 : F_NOCACHE);
    int			flg = O_RDWR;
    int			hFile = SYSCALL_ERR;

    _int64		remain_size = 0;
    BOOL		ret = TRUE;
    DigestCalc	*calc = NULL;

    //digestしているファイルもti->curPathに渡したいので
    //digest対象のファイルをdstに上書き。
    strcpy((char*)dst,(char*)obj->path);

    if (waitTick) Wait((waitTick + 9) / 10);

    //対象がシンボリックリンクの場合、"リンクをリンクとしてコピーする"の設定に関係なくリンク先ファイルをopenして
    //ファイル実体同士での比較をしていたが、SANDBOX時はリンク先ファイルopenがブロックされるので、open諦める
    if(obj->fileSize){
        struct stat	bhi;
        if((hFile = CreateFileWithRetry(obj->path, mode, 0, 0, 0, flg,
                        0, 5)) == SYSCALL_ERR){
            ConfirmErr("MakeDigestAsync:open", MakeAddr(obj->path, dstPrefixLen));
            ret = FALSE;
        }
        posix_fadvise(hFile,0,0,POSIX_FADV_DONTNEED);
        if(ret && (fstat(hFile,&bhi) == 0)){
            remain_size = bhi.st_size;
        }
        else goto END;
    }

    if(remain_size != obj->fileSize){
        ret = FALSE;
        ConfirmErr((char*)GetLoadStrV(IDS_SIZECHANGED_ERROR), MakeAddr(obj->path, dstPrefixLen), CEF_NOAPI);
        goto END;
    }
    if(obj->fileSize == 0){
        calc = GetDigestCalc(obj, 0);
        if (calc) {
            calc->dataSize = 0;
            PutDigestCalc(calc, DigestCalc::DONE);
        }
        goto END;
    }

    while(remain_size > 0 && !isAbort){
        int	max_trans = waitTick && (info.flags & AUTOSLOW_IOLIMIT) ? MIN_BUF : maxDigestReadSize;
        int	io_size = remain_size > max_trans ? max_trans : (int)remain_size;

        io_size = (int)ALIGN_SIZE(io_size, dstSectorSize);
        calc = GetDigestCalc(obj, io_size);

        if(calc){
            DWORD	trans_size = 0;
            if (isAbort || io_size && (!ReadFileWithReduce(hFile, calc->data, io_size,
                                        &trans_size, NULL) || trans_size <= 0)) {
                ConfirmErr("MakeDigestAsync:ReadFileWithReduce", MakeAddr(obj->path, dstPrefixLen));
                ret = FALSE;
                break;
            }
            calc->dataSize = trans_size;
            remain_size -= trans_size;
            total.verifyTrans += trans_size;
            PutDigestCalc(calc, remain_size > 0 ? DigestCalc::CONT : DigestCalc::DONE);
        }
        if (waitTick && remain_size > 0) Wait();
        CheckSuspend();
    }

END:
    if(hFile != SYSCALL_ERR){

        if(close(hFile) == SYSCALL_ERR){
            ConfirmErr("MakeDigestAsync:close()",(char*)obj->path);
        }
    }

    if(!ret){
        if(!calc)calc = GetDigestCalc(obj, 0);
        if(calc)PutDigestCalc(calc, DigestCalc::ERR);
    }

    return	ret;
}

BOOL FastCopy::CheckDigests(CheckDigestMode mode)
{
    DataList::Head *head;
    BOOL	ret = TRUE;

    if(mode == CD_NOWAIT && !digestList.Fetch())return ret;

    while(!isAbort && (head = digestList.Get())){
        if(!MakeDigestAsync((DigestObj *)head->data)){
            ret = FALSE;
        }
        CheckSuspend();
    }
    digestList.Clear();

    cv.Lock();
    runMode = RUN_NORMAL;
    cv.Notify();
    cv.UnLock();

    if(mode == CD_FINISH){
        DigestCalc	*calc = GetDigestCalc(NULL, 0);
        if (calc) PutDigestCalc(calc, DigestCalc::FIN);
    }

    if(mode == CD_WAIT || mode == CD_FINISH){
        wDigestList.Lock();
        while(wDigestList.Num() > 0 && !isAbort){
            wDigestList.Wait(CV_WAIT_TICK);
        }
        wDigestList.UnLock();
        CheckSuspend();
    }

    return	ret && !isAbort;
}

int FastCopy::CreateFileWithRetry(void *path, DWORD mode, DWORD share,
    int sa, DWORD cr_mode, DWORD flg, void* hTempl, int retry_max)
{

    int	fh = 0;


    fh = open((const char*)path,flg,0777);
    //エラー処理は上位任せ
    if(mode == F_NOCACHE){
        //読み込んだデータのキャッシュ破棄指示
        posix_fadvise(fh,0,0,POSIX_FADV_DONTNEED);
    }

    return	fh;
}

BOOL FastCopy::WriteFileWithReduce(int hFile, void *buf, DWORD size, DWORD *written,void* overwrap)
{
    DWORD	trans = 0;
    DWORD	total = 0;
    DWORD	maxWriteSizeSv = maxWriteSize;

    struct aiocb writelist[4];
    int			 aio_pnum = sizeof(writelist) / sizeof(struct aiocb);
    struct aiocb *list[aio_pnum];		//aio実験用

    while ((trans = size - total) > 0) {
        DWORD	transed = 0;
        trans = min(trans, maxWriteSize);

        QElapsedTimer et;
        if(info.flags_second & STAT_MODE){
            et.start();
        }
        //aio有効かつ要求バイトが同時io実行数以上？
        /*
        if(info.flags_second & FastCopy::ASYNCIO_MODE && (trans % aio_pnum) == 0){
            memset(&writelist,0,(sizeof(struct aiocb) * aio_pnum));
            //fdのカレントオフセット取得
            off_t current = lseek(hFile,0,SEEK_CUR);
            //qDebug() << "offset=" << current;
            for(int i=0; i < aio_pnum;i++){
                writelist[i].aio_fildes = hFile;
                writelist[i].aio_lio_opcode = LIO_WRITE;
                //ex:1MBで同時要求数4だったら0,256KB,512KB,768KBにオフセット設定ね
                writelist[i].aio_buf = (BYTE *)buf + ((trans / aio_pnum) * i);
                //ex:1MBで同時要求数4だったら256KBずつ
                writelist[i].aio_nbytes = trans / aio_pnum;
                //ex:1MBで同時要求数4だったらファイル読み込み開始位置はcurrentオフセットから0,256KB,512KB,768KBの位置
                writelist[i].aio_offset = current + ((trans / aio_pnum) * i);
                struct aiocb *ap = &writelist[i];
                list[i] = ap;
            }
            //まとめてread発行
            if(::lio_listio(LIO_WAIT,list,aio_pnum,NULL) == SYSCALL_ERR){
                ConfirmErr("WriteFileWithReduce:lio_listio()",NULL);
            }
            //結果をまとめて回収
            for(int i=0; i < aio_pnum;i++){
                ssize_t wk_trans = ::aio_return(list[i]);
                //qDebug() << "wk_trans=" << wk_trans;
                if(wk_trans == SYSCALL_ERR){
                    char buf[512];
                    sprintf(buf,"%s %d %d","WriteFileWithReduce:aio_return():",errno,i);
                    ConfirmErr(buf,NULL);
                    return FALSE;
                }
                transed = transed + wk_trans;
                //qDebug() << "transed=" << transed;
            }
            //fdオフセット進めて次の開始位置を設定
            lseek(hFile,transed,SEEK_CUR);
        }
        else{
        */
        transed = write(hFile,(BYTE *)buf + total,trans);
        //}
        if(info.flags_second & STAT_MODE){
            QTime time = QTime::currentTime();
            QString time_str = time.toString("hh:mm:ss.zzz");
            QString out(time_str + "we:" + QString::number(et.elapsed()) + "wq:" + QString::number(transed));
            PutStat(out.toLocal8Bit().data());
        }
        if(transed == SYSCALL_ERR){
            //Windowsの事情と異なるので、エラー時は即リターン
            //書き込み続行かどうかは上位で判断する
            return FALSE;
        }
        // writeシステムコールにかえたよんここまで
        total += transed;
    }
    *written = total;
    return	TRUE;
}

BOOL FastCopy::WriteFileXattr(int hFile,char* xattr_name, void*buf ,DWORD size, DWORD *written){

    ssize_t xattr_datalen = 0;
    xattr_datalen = fsetxattr(hFile,
                        xattr_name,
                        buf,
                        size,
                        0);
    if(xattr_datalen == SYSCALL_ERR){
        //エラーは上位で出力
        return false;
    }
    *written = size;
    return true;
}

BOOL FastCopy::WriteFileXattrWithReduce(int hFile,char* xattr_name, void*buf ,DWORD size, DWORD *written,void *wk_xattrbuf,_int64 file_size, _int64 remain_size){

    ssize_t xattr_datalen = 0;
    _int64 currentxattr_offset = file_size - remain_size;

    //ワークバッファに今回分に分割されたxattrをコピー
    memcpy(((char*)wk_xattrbuf) + currentxattr_offset,buf,size);
    //上位関数に返す書き込み済みサイズ加算
    *written = size;
    //今回のワークバッファへの組み立てでxattr完成する？
    if(((file_size - remain_size) + *written) == file_size){
        //完成したので、組み立て済みのワークバッファを使ってxattr付与
        xattr_datalen = fsetxattr(hFile,
                                  xattr_name,
                                  wk_xattrbuf,
                                  file_size,
                                  0);
        if(xattr_datalen == SYSCALL_ERR){
            //エラーは上位で出力
            return false;
        }
    }

    return true;
}

BOOL FastCopy::RestoreHardLinkInfo(DWORD *link_data, void *path, int base_len)
{
    LinkObj	*obj;
    DWORD	hash_id = hardLinkList.MakeHashId(link_data);

    if (!(obj = (LinkObj *)hardLinkList.Search(link_data, hash_id))) {
        ConfirmErr("HardLinkInfo is gone(internal error)", MakeAddr(path, base_len), CEF_NOAPI);
        return	FALSE;
    }

    strcpyV(MakeAddr(path, base_len), obj->path);

    if (--obj->nLinks <= 1) {
        hardLinkList.UnRegister(obj);
        delete obj;
    }

    return	TRUE;
}

BOOL FastCopy::WriteFileProc(int dst_len,int parent_fh)
{

    int	fh	= parent_fh;
    int rc_sym = SYSCALL_ERR;		//symbolic link
    int rc = SYSCALL_ERR;			//for hardlink
    BOOL	ret = TRUE;
    _int64	file_size = writeReq->stat.FileSize();
    _int64	remain = file_size;
    DWORD	trans_size;
    FileStat *stat = &writeReq->stat, sv_stat;
    Command	command = writeReq->command;

    struct timeval timeval_wk[2];		//[0] = ファイルアクセス日更新用ワーク
                                        //[1] = ファイル変更日更新用ワーク
                                        //futimesシステムコール用

    BOOL	is_reparse = IsReparse(stat->dwFileAttributes) && (info.flags & FILE_REPARSE) == 0;
    BOOL	is_hardlink = command == CREATE_HARDLINK;

    // is_nonbuf = true = fastcopyで確保した自バッファをコピーに使う
    // is_nonbuf = false = OSキャッシュを利用する
    BOOL	is_nonbuf = //dstFsType != FSTYPE_SMB &&
                        //指定ファイルサイズ未満の場合はOSキャッシュ利用して書き込みを廃止
                        //(file_size >= nbMinSize || (file_size % dstSectorSize) == 0)
                        ((file_size % dstSectorSize) == 0)
                        && (info.flags & USE_OSCACHE_WRITE) == 0 && !is_reparse ? TRUE : FALSE;
    //fh2によるreopen不要のためコメントアウト。
    //BOOL	is_reopen = is_nonbuf && (file_size % dstSectorSize) && !is_reparse ? TRUE : FALSE;
    BOOL	is_stream = command == WRITE_BACKUP_ALTSTREAM;
    int		&totalFiles = is_hardlink ? total.linkFiles : is_stream ?
                            total.writeAclStream : total.writeFiles;
    int		&totalErrFiles = is_stream ? total.errAclStream  : total.errFiles;
    _int64	&totalErrTrans = is_stream ? total.errStreamTrans : total.errTrans;
    BOOL	is_digest = IsUsingDigestList() && !is_stream && !is_reparse;

    BOOL	is_require_del = (info.flags & (DEL_BEFORE_CREATE|RESTORE_HARDLINK)) ? TRUE : FALSE;
    void	*wk_xattrbuf = NULL;				 //xattr分割送信時にワークとして利用するバッファポインタ(要解放)
    char	wk_xattrname[XATTR_NAME_MAX + STR_NULL_GUARD]; //xattr分割送信時のxattr名退避用バッファ

    //Macだとシンボリックリンク再現は上書きOKopenに頼れないので/recreateオプション強制有効にしちゃうぞ。
    //シンボリックリンクをリンクとしてコピーするときは上書き前に削除を強制有効とする
    if(!(info.flags & DIR_REPARSE || info.flags & FILE_REPARSE)){
        is_require_del = true;
    }
    // writeReq の stat を待避して、それを利用する
    if(command == WRITE_BACKUP_FILE || file_size > writeReq->bufSize){
        memcpy((stat = &sv_stat), &writeReq->stat, offsetof(FileStat, cFileName));
    }

    if (waitTick) Wait((waitTick + 9) / 10);

    //ファイル書き込み前に対象が存在してれば一旦削除有効？
    //WRITE_BACKUP_ALTSTREAM時は削除しちゃダメよ
    if(is_require_del && command != WRITE_BACKUP_ALTSTREAM){
        if(unlink((char*)dst) == SYSCALL_ERR){
            if(errno == ENOENT){
                //debug
                //ERRQT_OUT(errno,"DEL_BEFORE_CREATE enabled unlink() ENOENT its ok.",(char*)dst);
            }
            else{
                //debug
                //ERRQT_OUT(errno,"DEL_BEFORE_CREATE enabled unlink() ?????? its NG.",(char*)dst);
            }
        }
    }
    if(is_hardlink){
        if(!(ret = RestoreHardLinkInfo(writeReq->stat.GetLinkData(), hardLinkDst, dstBaseLen))){
            goto END2;
        }
        rc = link((char*)hardLinkDst,(char*)dst);
        qDebug() << "src=" << (char*)hardLinkDst << "dst=" << (char*)dst;
        if(rc == SYSCALL_ERR){
            ConfirmErr("link()", MakeAddr(dst, dstPrefixLen));
            ret = false;
        }
        else{
            ret = true;
        }
        goto END2;
    }

    //winのreparseはファイルハンドルに発行するけど、シンボリックリンクはfhに出せないので
    //open前に処理移動。
    if(is_reparse){

        // シンボリックリンク再現
        rc_sym = symlink((char*)writeReq->buf,(char*)dst);
        if(rc_sym == SYSCALL_ERR){
            totalErrFiles++;
            totalErrTrans += file_size;
            ConfirmErr("WriteFileProc:symlink(File)", MakeAddr(dst, dstPrefixLen));
            ret = false;
            goto END;
        }
        //symlinkには各種属性はつけないので、O_SYMLINKをくっつけたopenは行わない
    }
    else {
        //xattr延長の再帰でWriteFileProcが呼ばれてない？(最初のファイル実体writeか？)
        if(fh == SYSCALL_ERR){
            //書き込み対象のファイルハンドルをopen

            //LTFSへの書き込みで禁則文字の":"や"/"を発見した場合は
            //"_"に強制置換する。
            if(info.flags_second & FastCopy::LTFS_MODE){
                //本当はCでしれっとネイティブに処理したいけど、さぼってQtに任せちゃお。。
                QString dst_wk((char*)dst);
                bool convert_req = false;
                //禁則文字の":"入ってる？
                if(dst_wk.contains(LTFS_PROHIBIT_COLON)){
                    dst_wk.replace(LTFS_PROHIBIT_COLON,LTFS_CONVERTED_AFTER);
                    convert_req = true;
                }
                if(convert_req){
                    //禁則文字の強制変換したら既存ファイルとファイル名かぶった？
                    //なぜかこれがtrue?
                    if(QFile(dst_wk).exists()){
                        //かぶっちゃった、。諦めてエラーにする。
                        ConfirmErr((char*)GetLoadStrV(IDS_LTFS_REPLACE_ERROR),dst_wk.toLocal8Bit().data(),CEF_NOAPI);
                        total.errFiles++;
                    }
                    else{
                        //かぶらなかったので正常だけど、変更したよWarning出力
                        ConfirmErr((char*)GetLoadStrV(IDS_LTFS_REPLACE_WARN),dst_wk.toLocal8Bit().data(),CEF_NOAPI);
                    }
                    //dstを変換後名称で上書き
                    strcpy((char*)dst,dst_wk.toLocal8Bit().data());
                }
            }
            fh = open((const char*)dst,O_CREAT | O_WRONLY | O_TRUNC,0777);
            if (fh == SYSCALL_ERR) {
                SetErrFileID(stat->fileID);
                totalErrFiles++;
                totalErrTrans += file_size;
                //LTFSではwrite時にENOSPCやEDQUOTを返さないことがあり、結果としてだんまりになっちゃう事がある。
                //open時に先に返ってくる事もあるようなので、できるだけ継続不能エラーは先に検知、停止させる。
                //エラー内容が続行不能級エラーなら強制停止
                ConfirmErr("WriteFileProc:open()",(char*)dst,
                            (errno == EDQUOT || errno == ENOSPC) ? CEF_STOP : 0);
                ret = FALSE;
                goto END;
            }
            //open成功してたらpreallocateで断片化阻止
            if(info.flags_second & FastCopy::ENABLE_PREALLOCATE){
                //posix_fallocate(fh,0,file_size);    //エラー発生時の処理はのちのWriteFileWithReduceに任せる
                fallocate(fh,FALLOC_FL_ZERO_RANGE,0,file_size);
            }
        }
        while (remain > 0) {
            DWORD	write_size = (remain >= writeReq->bufSize) ? writeReq->bufSize :
                                //(DWORD)(is_nonbuf ? ALIGN_SIZE(remain, dstSectorSize) : remain);
                                (DWORD)((is_nonbuf && stat->dwFileAttributes) ? ALIGN_SIZE(remain, dstSectorSize) : remain);
                                //xattr付与の場合は書き込み長のアライン不要
            //xattrデータ書き込みじゃない？
            if(stat->dwFileAttributes != 0){
                //通常ファイル書き込み
                ret = WriteFileWithReduce(fh, writeReq->buf, write_size, &trans_size, NULL);
            }
            //xattrデータ書き込み要求
            else{
                //対象xattrデータのサイズが1MB以下？(最小のメインバッファ単位以下？)

                if(file_size <= MIN_BUF){
                    //xattr分割じゃないので、そのまxattr付与すればok
                    //後ろのエラー処理共通化のためにワークバッファにxattr名退避
                    strncpy(wk_xattrname,(char*)writeReq->stat.cFileName,sizeof(wk_xattrname));
                    ret = WriteFileXattr(fh,wk_xattrname,writeReq->buf,write_size,&trans_size);
                }
                else{
                    //xattrが分割で送信されてくるので、組み立てなおさないとダメ
                    //最初の受信？
                    if(file_size == remain){
                        //WRITE_FILE_CONT時はstat付いてこないので、初回のうちにxattr名退避しとく。
                        strncpy(wk_xattrname,(char*)writeReq->stat.cFileName,sizeof(wk_xattrname));
                        wk_xattrbuf = malloc(file_size);
                        if(wk_xattrbuf == NULL){
                            ret = false;
                        }
                        //else{ret = true;} 初回のret=true保証済み
                    }
                    if(ret){
                        ret = WriteFileXattrWithReduce(fh,
                                                       wk_xattrname,
                                                       writeReq->buf,
                                                       write_size,
                                                       &trans_size,
                                                       wk_xattrbuf,
                                                       file_size,
                                                       remain);
                    }
                }
            }
            if (!ret || write_size != trans_size){
                ret = FALSE;
                SetErrFileID(stat->fileID);

                //Xattr書き込みでENOSPC検知で強制停止もありえるよなあ
                if(errno == ENOSPC || errno == EDQUOT){
                    //書き込み対象に関わらず強制停止
                    ConfirmErr(stat->dwFileAttributes ? "WriteFileWithReduce" : "WriteFileXattr",
                               MakeAddr(dst, dstPrefixLen),CEF_STOP);
                }
                else{
                    //その他は強制停止じゃあないけどエラーだよ
                    //xattrじゃない通常ファイル書き込みでのエラー？
                    if(stat->dwFileAttributes){
                        ConfirmErr("WriteFileWithReduce",MakeAddr(dst, dstPrefixLen));
                    }
                    //xattrエラーなんだけど、オプションチェックしてないとエラー報告しないよん
                    else if(info.flags & REPORT_STREAM_ERROR){
                        ConfirmErr("WriteFileXattr",
                                    MakeAddr(dst, dstPrefixLen),
                                    0,
                                    (char*)GetLoadStrV(RAPIDCOPY_COUNTER_XATTR_ENOTSUP_EA),
                                    wk_xattrname);
                    }
                }
                break;
            }
            if ((remain -= trans_size) > 0) {	// 続きがある
                total.writeTrans += trans_size;
                if (RecvRequest() == FALSE || writeReq->command != WRITE_FILE_CONT) {
                    ret = FALSE;
                    remain = 0;
                    CheckSuspend();
                    if (!isAbort && writeReq->command != WRITE_ABORT) {
                        WCHAR cmd[2] = { writeReq->command + '0', 0 };
                        ConfirmErr("Illegal Request2 (internal error)", cmd, CEF_STOP|CEF_NOAPI);
                    }
                    break;
                }
                if (waitTick) Wait();
            }
            else {
                total.writeTrans += trans_size + remain;	// remain は 0 か負値
            }
        }

        if (ret && remain != 0) {
            //ファイルサイズが4kアラインで多めに書き込まれてる場合はftruncateで実際のファイルサイズに縮小。
            if(ftruncate(fh,stat->nFileSize) == SYSCALL_ERR){
                ret = FALSE;
                if (!is_stream || (info.flags & REPORT_STREAM_ERROR)){
                    ConfirmErr(is_stream ? "WriteFileProc:ftruncate2(stream)" : "WriteFileProc:ftruncate2",
                        MakeAddr(dst, dstPrefixLen));
                }
            }
            //ftruncateによりファイル日付等のメタデータ情報が非同期に更新される。
            //のちのfutimesまたはlutimesと日付更新操作が前後しないように、一度フラッシュする。
            if(fsync(fh) == SYSCALL_ERR){
                ERRNO_OUT(errno,"WriteFileProc:fsync");
            }
        }
    }

END2:

    CheckSuspend();
    if (ret && is_digest && !isAbort) {
        int path_size = (dst_len + 1) * CHAR_LEN_V;
        DataList::Head	*head = digestList.Alloc(NULL, 0, sizeof(DigestObj) + path_size);
        if (!head) {
            ConfirmErr("Can't alloc memory(digestList)", NULL, CEF_STOP);
        }
        DigestObj *obj = (DigestObj *)head->data;
        obj->fileID = stat->fileID;
        obj->fileSize = stat->FileSize();
        obj->dwFileAttributes = stat->dwFileAttributes;
        obj->pathLen = path_size;
        memcpy(obj->digest, writeReq->stat.digest, dstDigest.GetDigestSize());
        memcpy(obj->path, dst, path_size);

        BOOL is_empty_buf = digestList.RemainSize() <= digestList.MinMargin();
        if (is_empty_buf || (info.flags & SERIAL_VERIFY_MOVE))
            phase = VERIFYING;
            CheckDigests(is_empty_buf ? CD_WAIT : CD_NOWAIT); // empty なら wait
            phase = COPYING;
    }

    if (command == WRITE_BACKUP_FILE) {
        /* ret = */ WriteFileBackupProc(fh, dst_len);	// ACL/EADATA/STREAM エラーは無視
    }

    if (!is_hardlink) {
        //一個のfileへの処理がすべて終わるときだけfd閉じる
        if(parent_fh == SYSCALL_ERR){
            //シンボリックリンク作成時はfdオープンしてないのでclose自体発行不要
            if(!is_reparse){
                if(close(fh) == SYSCALL_ERR){
                    ConfirmErr("WriteFileProc:close()",(char*)dst);
                }
            }
            //close()とutimesをfdで処理しないのはメタ更新順番が怪しいかもと思っているからだが、
            //性能に問題あればfdへの処理に改めるかなあと思ってたけど、is_reparseとの整合性が面倒なので
            //fdへの発行はとりあえずいいや。。

            //utimes更新しないオプションの場合は変更日更新処理を行わない
            if(!(info.flags_second & FastCopy::DISABLE_UTIME)){
                //utimes発行してファイル日付を同期する
                // stat上はtimespecなんだけど、utimesがtimeval要求するので泣く泣くワーク変換
                TIMESPEC_TO_TIMEVAL(&timeval_wk[0],&stat->ftLastAccessTime);
                TIMESPEC_TO_TIMEVAL(&timeval_wk[1],&stat->ftLastWriteTime);

                int	retry_cnt = 0;
                while(retry_cnt < FASTCOPY_UTIMES_RETRYCOUNT){
                    //シンボリックリンク処理？
                    if(is_reparse && rc_sym != SYSCALL_ERR){
                        //日付情報更新
                        if(lutimes((char*)dst,timeval_wk) == SYSCALL_ERR){
                            //EAGAINならリトライ(古いMacで何故かこけるとかがあったので。。)
                            if(errno == EAGAIN){
                                retry_cnt++;
                                //0.5秒待つ
                                usleep(FASTCOPY_UTIMES_RETRYUTIMES);
                                continue;
                            }
                            else{
                                //EAGAIN以外のエラーはエラー出力して抜ける
                                total.errFiles++;
                                ConfirmErr("WriteFileProc:lutimes()",(char*)dst,CEF_REPARASE);
                                break;
                            }
                        }
                        else{
                            //成功なのでしれっと抜ける
                            break;
                        }
                    }
                    //ファイル処理
                    else{
                        //日付情報更新
                        if(utimes((char*)dst,timeval_wk) == SYSCALL_ERR){
                            //EAGAINリトライ
                            if(errno == EAGAIN){
                                retry_cnt++;
                                //0.5秒待つ
                                usleep(FASTCOPY_UTIMES_RETRYUTIMES);
                                continue;
                            }
                            else{
                                //EAGAIN以外のエラーはエラー出力して抜ける
                                total.errFiles++;
                                ConfirmErr("WriteFileProc:utimes()",(char*)dst,CEF_NORMAL);
                                break;
                            }
                        }
                        else{
                            //成功なのでしれっと抜ける
                            break;
                        }
                    }
                }
                //リトライしてもどうしても更新できなかった？
                if(retry_cnt == FASTCOPY_UTIMES_RETRYCOUNT){
                    //リトライしても補正できないので、差分コピーやり直しで上書きしてくれメッセージ出力
                    total.errFiles++;
                    ConfirmErr("WriteFileProc:(l)utimes()",
                               (char*)dst,
                               is_reparse ? CEF_REPARASE : CEF_NORMAL,
                               (char*)GetLoadStrV(RAPIDCOPY_COUNTER_UTIMES_EAGAIN));
                }
            }
        }
    }

    if (ret) {
        totalFiles++;
    }
    else {
        totalErrFiles++;
        totalErrTrans += file_size;
        SetErrFileID(stat->fileID);
        if (!is_stream) {
            unlink((const char*)dst);
        }
    }

END:
    CheckSuspend();

    if (!is_stream && info.mode == MOVE_MODE && (info.flags & VERIFY_FILE) == 0 && !isAbort) {
        SetFinishFileID(stat->fileID, ret ? MoveObj::DONE : MoveObj::ERR);
    }
    //moveモード時シンボリックリンクはverify不要
    else if (is_reparse && info.mode == MOVE_MODE && !isAbort) {
        SetFinishFileID(stat->fileID, ret ? MoveObj::DONE : MoveObj::ERR);
    }
    if ((isListingOnly || isListing && !is_digest) && !is_stream && ret) {
        DWORD flags = is_hardlink ? PL_HARDLINK : is_reparse ? PL_REPARSE : PL_NORMAL;
        PutList(MakeAddr(dst, dstPrefixLen),
                flags,
                0,
                NULL,
                is_reparse ? SYSCALL_ERR : stat->FileSize());
    }
    if(wk_xattrbuf != NULL){
        free(wk_xattrbuf);
    }
    return	ret && !isAbort;
}

BOOL FastCopy::WriteFileBackupProc(int fh, int dst_len)
{
    BOOL	ret = TRUE, is_continue = TRUE;

    while (!isAbort && is_continue) {
        if (RecvRequest() == FALSE) {
            ret = FALSE;
            if (!isAbort) {
                WCHAR cmd[2] = { writeReq->command + '0', 0 };
                ConfirmErr("Illegal Request3 (internal error)", cmd, CEF_STOP|CEF_NOAPI);
            }
            break;
        }
        switch (writeReq->command) {
        case WRITE_BACKUP_ACL: case WRITE_BACKUP_EADATA:

            ret = WriteFileBackupAclLocal(fh,writeReq);
            break;

        case WRITE_BACKUP_ALTSTREAM:
            //オリジナルの制御に頑張って載せる。
            //writeFileProcに既にopen済みのfd渡す
            ret = WriteFileProc(0,fh);
            break;

        case WRITE_BACKUP_END:
            is_continue = FALSE;
            break;

        case WRITE_FILE_CONT:	// エラー時のみ
            break;

        default:
            if (!isAbort) {
                WCHAR cmd[2] = { writeReq->command + '0', 0 };
                ConfirmErr("Illegal Request4 (internal error)", cmd, CEF_STOP|CEF_NOAPI);
            }
            ret = FALSE;
            break;
        }
        CheckSuspend();
    }
    return	ret && !isAbort;
}

//機能:引数statが開いているファイルハンドルにACLを追加する。
//返り値:true:ACLをコピーした。
//		false:なんらかのエラーが発生した。
BOOL FastCopy::WriteFileBackupAclLocal(int fh,ReqHeader *writereq)
{

    BOOL ret = true;
    int rc;
    acl_t write_acl;

    //なんとなくワークにコピー
    memcpy(&write_acl,writereq->buf,sizeof(write_acl));

    //acl設定
    rc = acl_set_fd(fh,write_acl);

    if(rc == SYSCALL_ERR){
        ret = false;
        //ACLエラー報告有効かつ対象ファイルシステムがACLサポート無し以外のエラー？
        if(info.flags & REPORT_ACL_ERROR){
            //manによるとEOPNOTSUPPが返ってくるはずなんだけど、ENOTSUP返ってくるんだよなあ。。
            if(errno == EOPNOTSUPP || errno == ENOTSUP){
                ConfirmErr("WriteFileBackupAclLocal:acl_set_fd_np()",
                            MakeAddr(dst,dstPrefixLen),
                            CEF_NORMAL,
                            (char*)GetLoadStrV(RAPIDCOPY_COUNTER_ACL_EOPNOTSUPP));
            }
            else{
                ConfirmErr("WriteFileBackupAclLocal:acl_set_fd_np()",MakeAddr(dst,dstPrefixLen),CEF_NORMAL);
            }
        }
    }

    //取得したacl contextを解放
    //課題:途中でエラーとか中断した時のwrite_aclの解放さぼってるので、途中キャンセルとかされると
    //     contextにゴミ残したままになっちゃうかも？その内解放する仕組み作らないとかなあ。。。
    rc = acl_free(write_acl);
    if(rc == SYSCALL_ERR){
        ret = false;
        if(info.flags & REPORT_ACL_ERROR){
            ConfirmErr("WriteFileBackupAclLocal:acl_free",MakeAddr(dst,dstPrefixLen),CEF_NORMAL);
        }
    }
    return(ret);
}

BOOL FastCopy::ChangeToWriteModeCore(BOOL is_finish)
{
    BOOL	isResetRunMode = runMode == RUN_DIGESTREQ;

    while ((!readReqList.IsEmpty() || !rDigestReqList.IsEmpty() || !writeReqList.IsEmpty()
    || writeReq || runMode == RUN_DIGESTREQ) && !isAbort) {
        if (!readReqList.IsEmpty()) {
            writeReqList.MoveList(&readReqList);
            cv.Notify();
            if (isResetRunMode) {
                runMode = RUN_DIGESTREQ;
            }
        }
        //このwaitが無限ループとな
        cv.Wait(CV_WAIT_TICK);
        if (isResetRunMode && readReqList.IsEmpty() && rDigestReqList.IsEmpty()
        && writeReqList.IsEmpty() && writeReq) {
            runMode = RUN_DIGESTREQ;
            isResetRunMode = FALSE;
            cv.Notify();
        }
        CheckSuspend();
    }

    return	!isAbort;
}

BOOL FastCopy::ChangeToWriteMode(BOOL is_finish)
{
    cv.Lock();
    BOOL	ret = ChangeToWriteModeCore(is_finish);
    cv.UnLock();
    return	ret;
}

BOOL FastCopy::AllocReqBuf(int req_size, _int64 _data_size, ReqBuf *buf)
{
    //メインバッファの終端アドレスから、当該アドレス以前が使用中を示すusedOffsetのアドレスを引いて求める。
    int		max_free = (int)(mainBuf.Buf() + mainBuf.Size() - usedOffset);
    //r/wが伴う場合の一回のデータ転送サイズの決定前処理。AUTOSLOW時は強制的にMIN_BUF(下限値)となる。
    int		max_trans = waitTick && (info.flags & AUTOSLOW_IOLIMIT) ? MIN_BUF : maxReadSize;
    //r/wが伴う場合の一回のデータ転送サイズの最終決定。要求データ長がmax I/Oサイズを超えてるとmax I/Oサイズに丸める。
    int		data_size = (_data_size > max_trans) ? max_trans : (int)_data_size;
    //r/wが伴う場合の一回のデータ転送サイズを8バイト単位にアライメントした値を記憶しとく。
    int		align_data_size = ALIGN_SIZE(data_size, 8);
    //8バイト単位にアライメントした値をセクタ境界にアライメントした値を記憶。windowsでosキャッシュに邪魔されないための下準備。
    int		sector_data_size = ALIGN_SIZE(data_size, sectorSize);
    //実際にメモリ確保するサイズは8バイト単位アライメントした値 + 制御用ヘッダ構造体(ReqHeader分)
    int		require_size = align_data_size + req_size;

    //セクタ境界アライメントをバッファ内で調整したいので、セクタアライメントされたバッファ位置を決定。
    BYTE	*align_offset = data_size ?
                            (BYTE *)ALIGN_SIZE((unsigned long)usedOffset, sectorSize) : usedOffset;
    //前行でセクタアライメントした場合は、空き容量をアライメントした分減らしておく。
    //もったいないけどこうしないとwindowsのosキャッシュ汚しちゃう=速度遅くなるでな。
    max_free -= (int)(align_offset - usedOffset);

    //ヘッダのみじゃない(r/w要求がある要求)かつ
    //r/wサイズの8バイト単位アライン済みサイズ + ヘッダ分が1セクタより小さい？
    if (data_size && align_data_size + req_size < sector_data_size){
        //んなちまちまやってるとセクタアラインがめんどくさいので強制的に1セクタ分使っちゃいなYO!
        require_size = sector_data_size;
    }

    //要求する実際の長さが空きの長さを上回ってる？
    if (require_size > max_free) {
        //空き領域のサイズが最小r/wサイズのMINBUFよりもさらに小さい=どうやっても入らない！
        if (max_free < MIN_BUF) {
            //セクタアライメントされたバッファ位置を先頭に戻しとく
            align_offset = mainBuf.Buf();
            //同一HDDモード？
            if (isSameDrv) {
                //同一HDDモードの場合は今あるぶんのバッファをはききっちゃうので切り替えをかける
                if (ChangeToWriteModeCore() == FALSE) {	// Read -> Write 切り替え
                    //失敗でreturnさせとく
                    return	FALSE;
                }
            }
        }
        //max_free領域内に収まるのが確定
        else {
            //data_sizeを残り空き容量からヘッダ分を除いた値にする。
            //後ろ領域を全部使っちゃうってことか！？
            data_size = max_free - req_size;
            //8バイトアライン単位だったアラインサイズとdata_sizeを32KB単位アラインでアラインする。
            align_data_size = data_size = (data_size / BIGTRANS_ALIGN) * BIGTRANS_ALIGN;
            //要求長は32KB単位アラインしたサイズにヘッダ分を加味した値とする
            require_size = data_size + req_size;
        }
    }
    //セクタ長にアライメントされたバッファ内位置をr/w開始位置として決定
    buf->buf = align_offset;
    //バッファ長はデータ長をセクタ境界にアラインした値
    //(内部データのケツを強制的に4KBアラインしないといけないので、中身無くても切り上げないと駄目)
    buf->bufSize = ALIGN_SIZE(data_size, sectorSize);
    //制御用ヘッダの場所を決定。アラインされたデータ部のケツに貼付けてるー！！
    buf->req = (ReqHeader *)(buf->buf + align_data_size);
    //制御用ヘッダのLLを設定。上位関数から渡されてきたサイズを信じる。
    buf->reqSize = req_size;
    // isSameDrv == TRUE の場合、必ず Empty
    //w要求用リストが空じゃないまたはRD要求用リストが空じゃない間、確保待ちループをまわす
    while ((!writeReqList.IsEmpty() || !rDigestReqList.IsEmpty()) && !isAbort) {
        //書き込み予定場所がmainBuf先頭か？(freeoffset領域に入れられなかったので先頭に入れるしかなかった？)
        if (buf->buf == mainBuf.Buf()) {

            //mainBuf先頭から使う場合(一周して先頭から使うイメージ)
            //writeReqDoneによるfreeoffset移動が完了、解放した結果次のエントリを入れる事が可能？
            if (freeOffset < usedOffset && (freeOffset - mainBuf.Buf()) >= require_size) {
                //確保待ちを抜ける
                break;
            }
        }
        //mainBuf途中から使う場合
        //writeReqDoneによるfreeoffset移動が完了して、解放した結果現在のバッファ位置からfreeoffsetまでの
        //サイズで次のエントリを入れる事が可能？
        else {
            if (freeOffset < usedOffset || buf->buf + require_size <= freeOffset) {
                break;
            }
        }
        //どっちも駄目ならwriteReqDoneによるfreeoffsetの移動まで待つしかないおね。
        cv.Wait(CV_WAIT_TICK);
        CheckSuspend();
    }
    //debug 開発黎明期用メインバッファオーバーフローチェック
    /*
    if(buf->buf + require_size > (mainBuf.Buf() + mainBuf.MaxSize())){
        char buf[1024];
        sprintf(buf,"mainBuf OVERFLOW detected!! reqsize=%d, require_size=%d, mainBuf.Buf() = %08x over addr = %08x\n",
                req_size,
                require_size,
                mainBuf.Buf() + mainBuf.MaxSize(),
                usedOffset);
        ConfirmErr(buf,NULL,CEF_STOP|CEF_NOAPI);
        qDebug("mainBuf OVERFLOW!!!");
        qDebug("reqsize=%d, require_size=%d, mainBuf.Buf() = %08x over addr = %08x\n",
                req_size,
                require_size,
                mainBuf.Buf() + mainBuf.MaxSize(),
                usedOffset);
        return false;
    }
    */
    //使用済みオフセットを加算
    usedOffset = buf->buf + require_size;
    return	!isAbort;
}

BOOL FastCopy::PrepareReqBuf(int req_size, _int64 data_size, _int64 file_id, ReqBuf *buf)
{
    BOOL ret = TRUE;

    cv.Lock();

    if (errFileID) {
        if (errFileID == file_id)
            ret = FALSE;
        errFileID = 0;
    }

    if (ret)
        ret = AllocReqBuf(req_size, data_size, buf);

    cv.UnLock();

    return	ret;
}


BOOL FastCopy::SendRequest(Command command, ReqBuf *buf, FileStat *stat)
{
    BOOL	ret = TRUE;
    ReqBuf	tmp_buf;

    cv.Lock();

    if (buf == NULL) {
        buf = &tmp_buf;
        ret = AllocReqBuf(offsetof(ReqHeader, stat) + (stat ? stat->minSize : 0), 0, buf);
    }

    CheckSuspend();
    if (ret && !isAbort) {
        ReqHeader	*readReq;
        readReq				= buf->req;
        readReq->reqSize	= buf->reqSize;
        readReq->command	= command;
        readReq->buf		= buf->buf;
        readReq->bufSize	= buf->bufSize;
        if (stat) {
            memcpy(&readReq->stat, stat, stat->minSize);
        }

        if (IsUsingDigestList()) {
            rDigestReqList.AddObj(readReq);
            cv.Notify();
        }
        else if (isSameDrv) {
            readReqList.AddObj(readReq);
        }
        else {
            writeReqList.AddObj(readReq);
            cv.Notify();
        }
    }

    cv.UnLock();

    CheckSuspend();
    return	ret && !isAbort;
}

BOOL FastCopy::RecvRequest(void)
{
    cv.Lock();

    if (writeReq) {
        WriteReqDone();
    }

    CheckDstRequest();

    CheckSuspend();
    while (writeReqList.IsEmpty() && !isAbort) {
        if (info.mode == MOVE_MODE && (info.flags & VERIFY_FILE)) {
            if (runMode == RUN_DIGESTREQ
            || (info.flags & SERIAL_VERIFY_MOVE) && digestList.Num() > 0) {
                cv.UnLock();
                CheckDigests(CD_WAIT);
                cv.Lock();
                continue;
            }
        }
        cv.Wait(CV_WAIT_TICK);
        CheckDstRequest();
    }
    writeReq = writeReqList.TopObj();

    cv.UnLock();

    CheckSuspend();
    return	writeReq && !isAbort;
}

void FastCopy::WriteReqDone(void)
{
    writeReqList.DelObj(writeReq);

    freeOffset = (BYTE *)writeReq + writeReq->reqSize;
    if (!isSameDrv || writeReqList.IsEmpty())
        cv.Notify();
    writeReq = NULL;
}

void FastCopy::SetErrFileID(_int64 file_id)
{
    cv.Lock();
    errFileID = file_id;
    cv.UnLock();
}

BOOL FastCopy::SetFinishFileID(_int64 _file_id, MoveObj::Status status)
{
    moveList.Lock();

    do {
        CheckSuspend();
        while (moveFinPtr = moveList.Fetch(moveFinPtr)) {
            MoveObj *data = (MoveObj *)moveFinPtr->data;
            if (data->fileID == _file_id) {
                data->status = status;
                break;
            }
        }
        if (moveFinPtr == NULL) {
            moveList.Wait(CV_WAIT_TICK);
        }
    } while (!isAbort && !moveFinPtr);

    if (moveList.IsWait()) {
        moveList.Notify();
    }
    moveList.UnLock();
    return	TRUE;
}


BOOL FastCopy::End(void)
{
    isAbort = TRUE;

    cv.Lock();
    cv.Notify();
    cv.UnLock();

    //無限待ち
    hReadThread_c->wait();

    delete hReadThread_c;
    // スレッドへのポインタはFastCopyが実行中かどうかの判定に使うのでNULL挿入
    hReadThread_c = NULL;

    // hReadThreadが生きている場合はhReadTreadが解放してくれてるはず
    // 両方チェックするのめんどいのでThread本体だけチェック
    // Thread本体は後解放してるので、こいつ生きてるならたぶんwatcherも生存してるでしょ
    if(IsUsingDigestList() && hRDigestThread_c != NULL){

        delete hWDigestThread_c;
        delete hRDigestThread_c;

        // null初期化
        hWDigestThread_c = NULL;
        hRDigestThread_c = NULL;
    }

    if(hWriteThread_c){
        hWriteThread_c->wait();
        delete hWriteThread_c;
        hWriteThread_c = NULL;
    }
    //posixセマフォを取得して動作している？
    if(isgetSemaphore){
        //自分が排他取ってたんだから返却処理する
        if(sem_post((sem_t*)rapidcopy_semaphore) == 0) {
            isgetSemaphore = false;
        }
        else{
            //万が一失敗した場合はsem_unlinkで強制削除する
            //(最悪の場合でもRapidCopy本体の再起動で排他もちっぱは解除されることを期待)
            sem_unlink(FASTCOPY_MUTEX);
            isgetSemaphore = false;
        }
    }
    else{
        //自分じゃないってことは自分は強制実行か「直ちに開始」でここに来たってことだね。
        //返却処理勝手にやっちゃダメなのでなにもしないよ
    }

    delete [] openFiles;
    openFiles = NULL;

    mainBuf.FreeBuf();
//	baseBuf.FreeBuf();
//	ntQueryBuf.FreeBuf();
    dstDirExtBuf.FreeBuf();
    mkdirQueueBuf.FreeBuf();
    dstStatIdxBuf.FreeBuf();
    dstStatBuf.FreeBuf();
    dirStatBuf.FreeBuf();
    fileStatBuf.FreeBuf();
    listBuf.FreeBuf();
    csvfileBuf.FreeBuf();
    errBuf.FreeBuf();
    statBuf.FreeBuf();
    srcDigestBuf.FreeBuf();
    dstDigestBuf.FreeBuf();
    digestList.UnInit();
    moveList.UnInit();
    hardLinkList.UnInit();
    wDigestList.UnInit();
    delete [] hardLinkDst;
    hardLinkDst = NULL;
    phase = NORUNNING;

    return	TRUE;
}

BOOL FastCopy::Suspend(void)
{
    bool req_suspend = false;

    if (!hReadThread_c && !hWriteThread_c || isSuspend){
        return	FALSE;
    }
    isSuspend = true;
    suspendTick = elapse_timer->elapsed();
    if(hReadThread_c != NULL){
        req_suspend = true;
    }
    if(hWriteThread_c != NULL){
        req_suspend = true;
    }
    if(IsUsingDigestList()){
        if(hRDigestThread_c != NULL){
            req_suspend = true;
        }
        if(hWDigestThread_c != NULL){
            req_suspend = true;
        }
    }
    if(req_suspend){
        //全スレッド一時停止(サスペンド)だよん
        suspend_Mutex.lock();
    }
    return	TRUE;
}

BOOL FastCopy::Resume(void)
{
    bool req_resume = false;

    if (!hReadThread_c && !hWriteThread_c || !isSuspend){
        return	FALSE;
    }

    isSuspend = FALSE;
    startTick += (elapse_timer->elapsed() - suspendTick);

    if(hReadThread_c != NULL){
        req_resume = true;
    }
    if(hWriteThread_c != NULL){
        req_resume = true;
    }

    if(IsUsingDigestList()){
        if(hRDigestThread_c != NULL){
            req_resume = true;
        }
        if(hWDigestThread_c != NULL){
            req_resume = true;
        }
    }
    if(req_resume){
        //全スレッド走行再開
        suspend_Mutex.unlock();
    }
    return	TRUE;
}

BOOL FastCopy::GetTransInfo(TransInfo *ti, BOOL fullInfo)
{
    ti->total = total;
    ti->listBuf = &listBuf;
    ti->csvfileBuf = &csvfileBuf;
    ti->listCs = &listCs;
    ti->errBuf = &errBuf;
    ti->errCs = &errCs;
    ti->statCs = &statCs;
    ti->statBuf = &statBuf;
    ti->isSameDrv = isSameDrv;
    ti->ignoreEvent = info.ignoreEvent;
    ti->waitTick = waitTick;
    ti->tickCount = (isSuspend ? suspendTick : endTick ? endTick : elapse_timer->elapsed()) - startTick;
    if (fullInfo) {
        ConvertExternalPath(MakeAddr(dst, dstPrefixLen), ti->curPath,
            sizeof(ti->curPath)/CHAR_LEN_V);
    }
    GetPhaseInfo(ti);
    return	TRUE;
}

BOOL FastCopy::GetPhaseInfo(TransInfo *ti)
{
    ti->phase = phase;
    return true;
}

//mainからコールバックされるシグナルハンドラ
//最小限の処理しかしてはいけない、再代入可能関数以外はコールしてはいけない
//printf()するとバグになるので注意
void FastCopy::signal_handler(int signum){
    //POSIXセマフォ解放が最優先
    //セマフォで排他抱えたままなのに落ちちゃうの？
    if(isgetSemaphore){
        //死ぬ前にせめてセマフォは解放していくぞ！
        sem_post((sem_t*)rapidcopy_semaphore);
        //どうせプロセスダウンなのでsem_close呼ばなくてもいいんだけど念のため
        sem_close((sem_t*)rapidcopy_semaphore);
        //テストでとりあえず呼んでるだけよ。本来はコールしちゃだめよ！
        //qDebug() << "signal on sem_post() called.";
    }
    abort();
}

int FastCopy::FdatToFileStat(struct stat *fdat, FileStat *stat, BOOL is_usehash,char* f_name)
{

    stat->fileID = 0;

    //差分コピーの時刻補正判定IsOverWriteFile()に影響するよ
    stat->ftCreationTime.tv_sec	= fdat->st_ctim.tv_sec;
    stat->ftCreationTime.tv_nsec = fdat->st_ctim.tv_nsec;

    stat->ftLastAccessTime.tv_sec	= fdat->st_atim.tv_sec;
    stat->ftLastAccessTime.tv_nsec	= fdat->st_atim.tv_nsec;

    stat->ftLastWriteTime.tv_sec	= fdat->st_mtim.tv_sec;
    stat->ftLastWriteTime.tv_nsec	= fdat->st_mtim.tv_nsec;

    stat->dwFileAttributes = fdat->st_mode;

    //対象がシンボリックリンクじゃない？またはリンクを実体としてコピー？
    if(!IsReparse(stat->dwFileAttributes)
                  || info.flags & FILE_REPARSE
                  || info.flags & DIR_REPARSE){
        //ファイルとしてコピーすればいいのでファイルサイズ格納
        stat->nFileSize = fdat->st_size;
    }
    else{
        //シンボリックリンクをリンクとしてコピーするので、とりあえずファイルサイズ0扱いとする
        stat->nFileSize = 0;
    }

    //fdはOpenの時に設定するのでここではとりあえず初期化だけ
    //その他はそれっぽく初期化
    stat->hFile				= SYSCALL_ERR;		//file descripter
    stat->lastError			= 0;
    stat->isExists			= FALSE;
    stat->isCaseChanged		= FALSE;
    stat->renameCount		= 0;
    stat->acl				= NULL;
    stat->aclSize			= 0;
    stat->ead				= NULL;
    stat->eadSize			= 0;
    stat->rep				= NULL;
    stat->repSize			= 0;
    stat->next				= NULL;
    memset(stat->digest, 0, SHA3_512SIZE);

    //hardlink再現用データ入れる
    if(info.flags & RESTORE_HARDLINK
        && fdat->st_nlink >= 2){
        stat->st_dev = fdat->st_dev;
        stat->st_ino = fdat->st_ino;
        stat->st_nlink = fdat->st_nlink;
    }
    else{
        //スピードアップのために初期化さぼっちゃお。
        //stat->st_dev = 0;
        //stat->st_ino = 0;
        //stat->st_nlink = 0;
    }

    //	int	len = (sprintfV(stat->cFileName, FMT_STR_V, fdat->cFileName) + 1) * CHAR_LEN_V;
    //stat->cFileName自体は4バイトしかないけど、後ろのエリアに無理矢理ぶっ込む構造だよ。
    int	len = (sprintfV(stat->cFileName, FMT_STR_V, f_name) + 1) * CHAR_LEN_V;

    //4バイトを超えてぶっこんだ分も込みでサイズ長を設定
    //FilestatのcFileNameまでのサイズ + CFileNameから始まる、ぶっこんだ分のサイズ
    //が入ってるよ。ちなみにLL含むサイズなので、頭からLL分足せばnextレコードになるよ。
    stat->size = len + offsetof(FileStat, cFileName);
    stat->minSize = ALIGN_SIZE(stat->size, 8);

    if (is_usehash || isNonCSCopy) {
        //hash
        stat->upperName = stat->cFileName + len;
        memcpy(stat->upperName, stat->cFileName, len);
        //Windowsの場合はファイル名の大文字と小文字は同値と見なすけど、
        //UNIXの場合は同値じゃないのでupperしなくていいはず。。
        //CharUpperV(stat->upperName);
        //とおもったけど、XFSみたいなcase-sensitiveな環境からFAT32とかにこぴっちゃうと問題になるよ。

        //caseSensitive->NoncaseSensitive?
        if(isNonCSCopy){
            //upperNameはupperしておく。
            unsigned char *p;
            for (p = stat->upperName; *p != '\0'; p++){
                *p = toupper(*p);
            }
        }
        stat->hashVal = MakeHash(stat->upperName, len);
        stat->size += len;
    }
    stat->size = ALIGN_SIZE(stat->size, 8);

    return	stat->size;
}

BOOL FastCopy::ConvertExternalPath(const void *path, void *buf, int max_buf)
{
    sprintfV(buf, "%.*s", max_buf, path);
    return	TRUE;
}

//機能:絶対パスをファイル名の文字列とパス名の文字列に分離する。
// *path 入力：文字列のアドレス。
// *dir	 出力：ファイル名を除いたパスを格納する領域。¥0を保証して返却する。
// *file 出力：ファイル名を格納する領域。¥0を保証して返却する。
// 返り値：なし
// 注意事項：引数*dirと引数*fileには*pathが示す文字列長+1と同じ領域を確保すること。
void FastCopy::apath_to_path_file_a(char *path, char *dir, char *file)
{
    char *p = strrchr(path, '/');

    if (p == NULL) {		/* '/' がない */
        *dir = '\0';
        strcpy(file, path);
    }
    else if (p == path) {   /* '/' が先頭だけ */
        strcpy(dir, (char*)"/");
        strcpy(file, p + 1);
    }
    else {				  /* '/' が見つかった */
        memcpy(dir, path, p - path); dir[p - path] = '\0';
        strcpy(file, p + 1);
    }
}

//機能:  FastCopyエンジン内でのエラー処理を行う。
//返り値:FastCopy::Confirm::Result参照
//特記事項:引数flagsに指定するフラグによって以下のように挙動が変わる
//			CEF_NOAPI:errno表示をおこなわない
//			CEF_STOP:コピー動作強制停止要求(各種設定にかかわらず、実行を強制停止する)
FastCopy::Confirm::Result FastCopy::ConfirmErr(const char *message, const void *path, DWORD flags,const char *count_message,char *count_message2)
{
    if (isAbort) return	Confirm::CANCEL_RESULT;

    BOOL	api_err = (flags & CEF_NOAPI) ? FALSE : TRUE;
    BOOL	allow_continue = (flags & CEF_STOP) ? FALSE : TRUE;

    char	msg_buf[MAX_PATH * 4];
    char	errno_strbuf[512];								// System Callエラー詳細格納用バッファ
    void*	errmessage_pt = NULL;							// 独自エラーメッセージある場合のポインタ
    DWORD	err_code = api_err ? errno : 0;
    int		len = 0;

    len = sprintfV(msg_buf,"%s",message);

    if (api_err && err_code) {
        //システムコールエラー番号からメッセージをロード
        errmessage_pt = GetLoadStrV(RAPIDCOPY_ERRNO_BASE + err_code);
        //当該errno用のエラーメッセージは想定してない？
        if(errmessage_pt == NULL){
            //しゃーないのでワークバッファにstrerror_rで適当な説明を取得
            strerror_r(err_code,errno_strbuf,sizeof(errno_strbuf));
            //出力メッセージはstrerror_rで取得したメッセージとする
            errmessage_pt = errno_strbuf;
        }
    }

    void *fmt_us_v = api_err ? (void *)"(errno:%u %s %s %s\n%s" : (void *)"%s\n%s";
    if(api_err){
        len += sprintfV(MakeAddr(msg_buf, len),
                    fmt_us_v,
                    err_code,
                    err_code ? errmessage_pt : "",
                    count_message != NULL ? count_message : "",
                    count_message2 != NULL ? count_message2 : "",
                    path ? (void *)path : (void *)"");
    }
    else{
        len += sprintfV(MakeAddr(msg_buf, len),
                    fmt_us_v,
                    count_message != NULL ? count_message : "",
                    path ? (void *)path : (void *)"");
    }

    WriteErrLog(msg_buf, len);

    if(listBuf.Buf() && isListing){
        //csv出力時はファイルパス投げるためにパスだけ別引数で投げる
        PutList(msg_buf,
                flags | PL_ERRMSG,		//Convert CEF_NORMAL<->CEF_REPARSE to PL_NORMAL <->PL_REPARSE
                0,
                NULL,
                SYSCALL_ERR,
                (void*)path);
    }

    if(allow_continue){
        //エラー発生時無視有効？
        if (info.ignoreEvent & FASTCOPY_ERROR_EVENT){
            return	Confirm::CONTINUE_RESULT;
        }
    }
    else{
        isAbort = TRUE;
    }

    Confirm	 confirm = { msg_buf, allow_continue, path, err_code, Confirm::CONTINUE_RESULT };

    //エラー発生時中断する？
    //qDebug() << info.ignoreEvent;
    if ((info.ignoreEvent & FASTCOPY_STOP_EVENT) == 0) {
        //エラー発生時中断有効なので、一回止めるのね。
        isSuspend = TRUE;
        //suspendTick = ::GetTickCount();
        suspendTick = elapse_timer->elapsed();

        //型登録
        qRegisterMetaType<FastCopy::Confirm>("FastCopy::Confirm");
        //他スレッドからGUI操作可能なメインスレッドのSLOTを直接出口コールする。
        //メインスレッド延長でのコールかどうかは
        //起動時に確認したmainThreadのThreadPointerアドレスと同一かどうかで判定。
        QMetaObject::invokeMethod((QObject *)info.hNotifyWnd,
                                  "ConfirmNotify",
                                  QThread::currentThreadId() != mainThread_pt ?
                                    Qt::BlockingQueuedConnection : Qt::DirectConnection,
                                  Q_RETURN_ARG(FastCopy::Confirm,confirm),
                                  Q_ARG(FastCopy::Confirm,confirm));

        isSuspend = FALSE;
        startTick += (elapse_timer->elapsed() - suspendTick);
    }
    else{
        confirm.result = Confirm::CANCEL_RESULT;
    }

    switch(confirm.result){
        case Confirm::IGNORE_RESULT:
            info.ignoreEvent |= FASTCOPY_ERROR_EVENT;
            confirm.result	= Confirm::CONTINUE_RESULT;
            break;

        case Confirm::CANCEL_RESULT:
            isAbort = TRUE;
            break;
    }
    return	confirm.result;
}

BOOL FastCopy::WriteErrLog(void *message, int len)
{
#define ERRMSG_SIZE		60

    errCs.lock();

    BOOL	ret = TRUE;
    void	*msg_buf = (char *)errBuf.Buf() + errBuf.UsedSize();

    if (len == -1)
        len = strlenV(message);
    int	need_size = ((len + 3) * CHAR_LEN_V) + ERRMSG_SIZE;

    if(errBuf.UsedSize() + need_size <= errBuf.MaxSize()){
        if(errBuf.RemainSize() > need_size || errBuf.Grow(ALIGN_SIZE(need_size, PAGE_SIZE))){
            memcpy(msg_buf, message, len * CHAR_LEN_V);
            SetChar(msg_buf, len++, '\n');
            errBuf.AddUsedSize(len * CHAR_LEN_V);
        }
    }
    else{
        if(errBuf.RemainSize() > ERRMSG_SIZE){
            void *err_msg = (void *) " Too Many Errors...\n";
            sprintfV(msg_buf, FMT_STR_V, err_msg);
            errBuf.SetUsedSize(errBuf.MaxSize());
        }
        ret = FALSE;
    }
    errCs.unlock();
    return	ret;
}

void FastCopy::Aborting(void)
{
    isAbort = TRUE;
    void *err_msg = (void *)" Aborted by User";
    WriteErrLog(err_msg);
    if (!isListingOnly && isListing) PutList(err_msg, PL_ERRMSG);
}


BOOL FastCopy::Wait(DWORD tick)
{
#define SLEEP_UNIT	200 * 100
    int	svWaitTick = (int)waitTick;
    int	remain = (int)(tick ? tick : waitTick);

    static int call_num;

    while(remain > 0 && !isAbort){
        int	unit = remain > SLEEP_UNIT ? SLEEP_UNIT : remain;
        call_num++;
        usleep(unit * 50);

        int	curWaitTick = waitTick;
        if(curWaitTick != SUSPEND_WAITTICK){
            remain -= unit + (svWaitTick - curWaitTick);
            svWaitTick = curWaitTick;
        }
        CheckSuspend();
    }
    return	!isAbort;
}

/*=========================================================================
  クラス : FastCopy_Thread
  概  要 : FastCopyの各スレッドのスレッドインスタンス
=========================================================================*/
FastCopy_Thread::FastCopy_Thread(FastCopy *_fastcopyPt,int _thread_type, int _stack_size)
{
    //FastCopyインスタンスへのポインタ受け取る
    fastcopy_pt = _fastcopyPt;
    my_type = _thread_type;
    my_stacksize = _stack_size;

    setStackSize(my_stacksize);
}

void FastCopy_Thread::run(){

    switch(my_type){

        case RTHREAD:
            fastcopy_pt->ReadThread(fastcopy_pt);
            break;

        case WTHREAD:
            fastcopy_pt->WriteThread(fastcopy_pt);
            break;

        case RDTHREAD:
            fastcopy_pt->RDigestThread(fastcopy_pt);
            break;

        case WDTHREAD:
            fastcopy_pt->WDigestThread(fastcopy_pt);
            break;

        case DTHREAD:
            fastcopy_pt->DeleteThread(fastcopy_pt);
            break;
    }
    return;
}

