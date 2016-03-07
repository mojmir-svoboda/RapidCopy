/* ========================================================================
    Project  Name			: Fast/Force copy file and directory
    Create					: 2016-02-29
    Copyright				: Kengo Sawatsu
    Summary 				: FastCopyの互換関数群
    Reference				:
    ======================================================================== */
#include "osl.h"
#include "string.h"

//max関数ないので自分で定義
int max(int a,int b){
    return a>b?a:b;
}

int min(int a,int b){
    return a<b?a:b;
}

//	パス合成の関数、めんどくさいので自作して浮かせちゃうかな
//	引数fname,extはnull省略可
//	その他引数は省略不可、絶対指定して！
//	引数driveはstrcpy,後続引数はstrcatでつなぐ
//	引数pathに結果を格納するけど、格納領域は呼び元で用意してちょ
//	引数チェックはしない、バッファあふれなんて考えもしないぞ
void MakePathV(char *path,char *drive,char *dir,char *fname,char *ext){
    strcpy(path,drive);
    strcat(path,dir);
    if(fname != NULL){
        strcat(path,fname);
        if(ext != NULL){
            strcat(path,ext);
        }
    }
}



