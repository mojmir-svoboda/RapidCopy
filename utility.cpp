static char *utility_id =
    "@(#)Copyright (C) 2004-2012 H.Shirouzu		utility.cpp	ver2.10";
/* ========================================================================
    Project  Name			: general routine
    Create					: 2004-09-15(Wed)
    Update					: 2012-06-17(Sun)
    ported update           : 2016-02-28
    Copyright				: H.Shirouzu,Kengo Sawatsu
    Reference				:
    ======================================================================== */

#include "utility.h"
#include <QDebug>

/*=========================================================================
    PathArray
=========================================================================*/
PathArray::PathArray(void) : THashTbl(1000)
{
    num = 0;
    pathArray = NULL;
    flags = 0;
}

PathArray::PathArray(const PathArray &src) : THashTbl(1000)
{
    num = 0;
    pathArray = NULL;
    *this = src;
    flags = 0;
}

PathArray::~PathArray()
{
    Init();
}

void PathArray::Init(void)
{
    while(--num >= 0){
        delete pathArray[num];
    }
    free(pathArray);
    num = 0;
    pathArray = NULL;
}

BOOL PathArray::PathObj::Set(const void *_path, int _len)
{
    path = NULL;
    len = 0;

    if(_path){
        if (_len < 0) _len = strlenV(_path);
        len = _len;
        int	alloc_len = len + 1;
        path = malloc(alloc_len * CHAR_LEN_V);
        memcpy(path, _path, alloc_len * CHAR_LEN_V);
    }
    return	TRUE;
}

int PathArray::RegisterMultiPath(const void *_multi_path,void *separator)
{
    //Windowsでは";"や\"自体をファイル名に設定できないので、strtok_pathVのような特殊関数が必要だったけど
    //HFSPlusでは";"や\"," "は普通にファイル名やフォルダ名として使用可能なので、こりゃもうだめぽ。
    //思い切ってQt使う仕様に変える
    int		cnt = 0;
    QString multi_path =(char*)_multi_path;
    QString splitstr = (char*)separator;
    //1バイト文字、マルチバイト文字等の区切り文字に関係なくパス区切りする
    QStringList pathlist = multi_path.split(splitstr);
    for(int i=0; i<pathlist.count();i++){
        //区切ったパスを一つ登録
        if(RegisterPath(pathlist.at(i).toLocal8Bit().data())){
            cnt++;
        }
    }
    return	cnt;
}

int PathArray::GetMultiPath(void *multi_path, int max_len,
    const void *separator, const void *escape_char)
{
    void	*buf = multi_path;

    int		sep_len = strlenV(separator);
    int		total_len = 0;
    int		escape_val = escape_char ? GetChar(escape_char, 0) : 0;

    for(int i=0; i < num; i++){
        BOOL	is_escape = escape_val && strchrV(pathArray[i]->path, escape_val);
        int		need_len = pathArray[i]->len + 1 + (is_escape ? 2 : 0) + (i ? sep_len : 0);

        if(max_len - total_len < need_len){
            SetChar(multi_path, total_len, 0);
            return -1;
        }
        if(i){
            memcpy(MakeAddr(buf, total_len), separator, sep_len * CHAR_LEN_V);
            total_len += sep_len;
        }
        if(is_escape){
            SetChar(buf, total_len, '"');
            total_len++;
        }
        memcpy(MakeAddr(buf, total_len), pathArray[i]->path, pathArray[i]->len * CHAR_LEN_V);
        total_len += pathArray[i]->len;
        if(is_escape){
            SetChar(buf, total_len, '"');
            total_len++;
        }
    }
    SetChar(multi_path, total_len, 0);
    return	total_len;
}

int PathArray::GetMultiPathLen(const void *separator, const void *escape_char)
{
    int		total_len = 0;
    int		sep_len = strlenV(separator);
    int		escape_val = escape_char ? GetChar(escape_char, 0) : 0;

    for (int i=0; i < num; i++) {
        BOOL	is_escape = escape_val && strchrV(pathArray[i]->path, escape_val);
        total_len += pathArray[i]->len + (is_escape ? 2 : 0) + (i ? sep_len : 0);
    }

    return	total_len + 1;
}

BOOL PathArray::SetPath(int idx, const void *path, int len)
{
    if(len < 0) len = strlenV(path);
    pathArray[idx] = new PathObj(path, len);
    Register(pathArray[idx], MakeHashId(pathArray[idx]));
    return	TRUE;
}

BOOL PathArray::RegisterPath(const void *path)
{
    if(!path || !GetChar(path, 0))return	FALSE;

    int len = strlenV(path);

    if((flags & ALLOW_SAME) == 0 && Search(path, MakeHashId(path, len)))return FALSE;

#define MAX_ALLOC	100
    if((num % MAX_ALLOC) == 0){
        pathArray = (PathObj **)realloc(pathArray, (num + MAX_ALLOC) * sizeof(void *));
    }
    SetPath(num++,path,len);
    return	TRUE;
}

BOOL PathArray::ReplacePath(int idx, void *new_path)
{
    if(idx >= num)
        return	FALSE;

    if(pathArray[idx]){
        free(pathArray[idx]);
    }
    SetPath(idx, new_path);
    return	TRUE;
}

PathArray& PathArray::operator=(const PathArray& init)
{
    Init();

    pathArray = (PathObj **)malloc(((((num = init.num) / MAX_ALLOC) + 1) * MAX_ALLOC)
                * sizeof(void *));

    for(int i=0; i < init.num; i++){
        SetPath(i, init.pathArray[i]->path, init.pathArray[i]->len);
    }

    return	*this;
}

DataList::DataList(int size, int max_size, int _grow_size, VBuf *_borrowBuf, int _min_margin)
{
    num = 0;
    top = end = NULL;

    if (size) Init(size, max_size, _grow_size, _borrowBuf, _min_margin);
}

DataList::~DataList()
{
    UnInit();
}

BOOL DataList::Init(int size, int max_size, int _grow_size, VBuf *_borrowBuf, int _min_margin)
{
    grow_size = _grow_size;
    min_margin = _min_margin;
    cv.Initialize(2);

    BOOL ret = buf.AllocBuf(size, max_size, _borrowBuf);
    Clear();
    return	ret;
}

void DataList::UnInit()
{
    top = end = NULL;
    buf.FreeBuf();
    cv.UnInitialize();
}

void DataList::Clear()
{
    top = end = NULL;
    buf.SetUsedSize(0);
    num = 0;
}

DataList::Head *DataList::Alloc(void *data, int data_size, int need_size)
{
    Head	*cur = NULL;
    int		alloc_size = need_size + sizeof(Head);

    alloc_size = ALIGN_SIZE(alloc_size, 8);

    if (!top) {
        cur = top = end = (Head *)buf.Buf();
        cur->next = cur->prior = NULL;
    }
    else {
        if (top >= end) {
            cur = (Head *)((BYTE *)top + top->alloc_size);
            if ((BYTE *)cur + alloc_size < buf.Buf() + buf.MaxSize()) {
                int need_grow = (int)(((BYTE *)cur + alloc_size) - (buf.Buf() + buf.Size()));
                if (need_grow > 0) {
                    if (!buf.Grow(ALIGN_SIZE(need_grow, PAGE_SIZE))) {
                        //MessageBox(0, "can't alloc mem", "", MB_OK);
                        goto END;
                    }
                }
            }
            else {
                if((BYTE *)end < buf.Buf() + alloc_size){
                    //MessageBox(0, "buf is too small", "", MB_OK);
                    goto END;
                }
                cur = (Head *)buf.Buf();
            }
        }
        else{
            if((BYTE *)end < (BYTE *)top + top->alloc_size + alloc_size){	// for debug
                //MessageBox(0, "buf is too small2", "", MB_OK);
                goto END;
            }
            cur = (Head *)((BYTE *)top + top->alloc_size);
        }
        top->next = cur;
        cur->prior = top;
        cur->next = NULL;
        top = cur;
    }
    cur->alloc_size = alloc_size;
    cur->data_size = data_size;
    if(data){
        memcpy(cur->data, data, data_size);
    }
    buf.AddUsedSize(alloc_size);
    num++;

END:
    return	cur;
}

DataList::Head *DataList::Get()
{
    Head	*cur = end;

    if (!cur) goto END;

    if (cur->next) {
        cur->next->prior = cur->prior;
    }
    else {
        top = cur->prior;
    }
    end = cur->next;

    num--;

END:
    return	cur;
}

DataList::Head *DataList::Fetch(Head *prior)
{
    Head	*cur = prior ? prior->next : end;

    return	cur;
}

int DataList::RemainSize()
{
    int ret = 0;

    if(top){
        BYTE *top_end = (BYTE *)top + top->alloc_size;

        if(top >= end){
            int size1 = (int)(buf.MaxSize() - (top_end - buf.Buf()));
            int size2 = (int)((BYTE *)end - buf.Buf());
            ret = std::max(size1, size2);
        }
        else{
            ret = (int)((BYTE *)end - top_end);
        }
    }
    else{
        ret = buf.MaxSize();
    }

    if(ret > 0)ret -= sizeof(Head);

    return	ret;
}


