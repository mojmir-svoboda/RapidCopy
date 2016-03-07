/* ========================================================================
    Project  Name			: Fast/Force copy file and directory
    Create					: 2016-02-25
    Update					: 2016-02-25
    Copyright				: Kengo Sawatsu
    Reference				:
    Summary 				: VC->C++型変換及びOS依存定数ヘッダ
    ======================================================================== */

#ifndef OSL_H
#define OSL_H

#include <limits.h>
#include <QtGlobal>
#include <sys/stat.h>
//system static convert

#define MAX_PATH PATH_MAX

//VC++ -> ANSI C99 Type Convert
#define BOOL bool
#define UINT unsigned int
#define WCHAR char
#define DWORD unsigned long
#define WORD long
#define TRUE true
#define FALSE false
#define BYTE unsigned char

#define _int64 qint64
#define BLANKARG (void*)&""

#define INVALID_HANDLE_VALUE (NULL)

#define STAT (struct stat_t)

#define SYSCALL_ERR (-1)
#define FCNTL_ON 1

int max(int a,int b);
int min(int a,int b);
bool IsDir2(int d);

// errnoと発生ファイル名、発生システムコール名を表示するマクロ
// 使用する時はerrno.hをincludeしてね
#define ERRNO_OUT(_err_no,_name) { \
    printf("error caused CALL= %s FILE=%s,LINE=%d,errno=%d\n",_name,__FILE__,__LINE__,_err_no); \
    fflush(stdout);				\
    }

#define ERRQT_OUT(_no,_name,_string){ \
        printf("error caused CALL= %s FILE=%s,LINE=%d,no=%d,string=%s\n",_name,__FILE__,__LINE__,_no,_string); \
        fflush(stdout);				\
    }

#define STR_NULL_GUARD 1				//MAX_PATHなど、あてにならない文字列長定数の\0保証

//struct tm補正用定数
#define TM_YEAR_OFFSET			1900
#define TM_MONTH_OFFSET			1

//nanosleep用定数
#define NANO_SECOND_MULTIPLIER  1000000  // 1 millisecond = 1,000,000 Nanoseconds
const long INTERVAL_MS = 250 * NANO_SECOND_MULTIPLIER;

//statfsのf_typeで取得できるファイルシステムタイプdefine
//usr/include/linux/magic.h以外で認識したいファイルシステムをdefineする。
//本来はkernelに直接書かれているものらしい。。(manによると)
#define CIFS_MAGIC_NUMBER 0xFF534D42
#define FUSE_SUPER_MAGIC  0x65735546
#define HFS_SUPER_MAGIC   0x4244
#define JFS_SUPER_MAGIC   0x3153464a
#define NTFS_SB_MAGIC     0x5346544e
#define UDF_SUPER_MAGIC   0x15013346
#define XFS_SUPER_MAGIC   0x58465342

//Log出力時の文字列
//RapidCopyが独自に決めてるだけだよ
#define FSLOGNAME_UNKNOWN	"UNKNOWN"
#define FSLOGNAME_ADFS		"ADFS"
#define FSLOGNAME_AFS		"AFS"
#define FSLOGNAME_CODA      "CODA"
#define FSLOGNAME_EXT       "EXT"
#define FSLOGNAME_EXT2      FSLOGNAME_EXT
#define FSLOGNAME_EXT3      FSLOGNAME_EXT
#define FSLOGNAME_EXT4      FSLOGNAME_EXT
#define FSLOGNAME_BTRFS     "BTRFS"
#define FSLOGNAME_F2FS      "F2FS"
#define FSLOGNAME_ISO       "ISO"
#define FSLOGNAME_FAT		"FAT"
#define FSLOGNAME_NFS		"NFS"
#define FSLOGNAME_REISERFS	"REISERFS"
#define FSLOGNAME_SMBFS     "SMB"
#define FSLOGNAME_CIFS      "CIFS"
#define FSLOGNAME_FUSE      "FUSE"
#define FSLOGNAME_HFS       "HFS"
#define FSLOGNAME_JFS       "JFS"
#define FSLOGNAME_NTFS		"NTFS"
#define FSLOGNAME_UDF       "UDF"
#define FSLOGNAME_XFS       "XFS"

#define SW_NORMAL 10
#define SW_MINIMIZE 11
#define SW_HIDE 12
#define SW_SHOW 13

#endif // OSL_H
