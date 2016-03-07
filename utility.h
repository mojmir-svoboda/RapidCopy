/* static char *utility_id =
    "@(#)Copyright (C) 2004-2012 H.Shirouzu		utility.h	Ver2.10"; */
/* ========================================================================
    Project  Name			: Utility
    Create					: 2004-09-15(Wed)
    Update					: 2012-06-17(Sun)
    port update             : 2016-02-28
    Copyright				: H.Shirouzu, Kengo Sawatsu
    Reference				:
    ======================================================================== */

#ifndef UTILITY_H
#define UTILITY_H

#include "tlib.h"
#include "osl.h"

class PathArray : public THashTbl {
protected:
    struct PathObj : THashObj {
        void	*path;
        int		len;
        PathObj(const void *_path, int len=-1) { Set(_path, len); }
        ~PathObj() { if (path) free(path); }
        BOOL Set(const void *_path, int len=-1);
    };
    int		num;
    PathObj	**pathArray;
    DWORD	flags;
    BOOL	SetPath(int idx, const void *path, int len=-1);

    virtual BOOL	IsSameVal(THashObj *obj, const void *val) {
        return lstrcmpiV(((PathObj *)obj)->path, val) == 0;
    }

public:
    enum { ALLOW_SAME=1, NO_REMOVE_QUOTE=2 };
    PathArray(void);
    PathArray(const PathArray &);
    ~PathArray();
    void	Init(void);
    void	SetMode(DWORD _flags) { flags = _flags; }
    int		RegisterMultiPath(const void *_multi_path, void *separator=UNICODE_PARAGRAPH);
    int		GetMultiPath(void *multi_path, int max_len, const void *separator=UNICODE_PARAGRAPH,
                const void *escape_char=NULL);
    int		GetMultiPathLen(const void *separator=UNICODE_PARAGRAPH,
                const void *escape_char=NULL);

    PathArray& operator=(const PathArray& init);

    void	*Path(int idx) const { return idx < num ? pathArray[idx]->path : NULL; }
    int		Num(void) const { return	num; }
    BOOL	RegisterPath(const void *path);
    BOOL	ReplacePath(int idx, void *new_path);

    u_int	MakeHashId(const void *data, int len=-1) {
        return MakeHash(data, (len >= 0 ? len : strlenV(data)) * CHAR_LEN_V);
    }
    u_int	MakeHashId(const PathObj *obj) { return MakeHash(obj->path, obj->len * CHAR_LEN_V); }
};

class DataList {
public:
    struct Head {
        Head	*prior;
        Head	*next;
        int		alloc_size;
        int		data_size;
        BYTE	data[1];	// opaque
    };

protected:
    VBuf		buf;
    Head		*top;
    Head		*end;
    int			num;
    int			grow_size;
    int			min_margin;
    Condition	cv;

public:
    DataList(int size=0, int max_size=0, int _grow_size=0, VBuf *_borrowBuf=NULL, int _min_margin=65536);
    ~DataList();

    BOOL Init(int size, int max_size, int _grow_size, VBuf *_borrowBuf=NULL, int _min_margin=65536);
    void UnInit();

    void Lock() { cv.Lock(); }
    void UnLock() { cv.UnLock(); }
    BOOL Wait(DWORD timeout=-1) { return cv.Wait(timeout); }
    BOOL IsWait() { return cv.WaitThreads() ? TRUE : FALSE; }
    void Notify() { cv.Notify(); }

    Head *Alloc(void *data, int copy_size, int need_size);
    Head *Get();
    Head *Fetch(Head *prior=NULL);
    void Clear();
    int Num() { return num; }
    int RemainSize();
    int MaxSize() { return buf.MaxSize(); }
    int Size() { return buf.Size(); }
    int Grow(int grow_size) { return buf.Grow(grow_size); }
    int MinMargin() { return min_margin; }
};

#endif

