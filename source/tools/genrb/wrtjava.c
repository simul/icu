/*
*******************************************************************************
*
*   Copyright (C) 2000, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File wrtjava.c
*
* Modification History:
*
*   Date        Name        Description
*   01/11/02    Ram        Creation.
*******************************************************************************
*/

#include <assert.h>
#include "reslist.h"
#include "unewdata.h"
#include "unicode/ures.h"
#include "error.h"
#include "filestrm.h"
#include "cstring.h"
#include "unicode/ucnv.h"
#include "genrb.h"
#include "rle.h"
#include "ucol_tok.h"
#include "uhash.h"

void res_write_java(struct SResource *res,UErrorCode *status);


static const char* copyRight =   "/* \n"
                                 " *******************************************************************************\n"
                                 " *\n"
                                 " *   Copyright (C) International Business Machines\n"
                                 " *   Corporation and others.  All Rights Reserved.\n"
                                 " *\n"
                                 " *******************************************************************************\n"
                                 " * $Source: /xsrl/Nsvn/icu/icu/source/tools/genrb/wrtjava.c,v $ \n"
                                 " * $Date: 2002/03/20 23:56:22 $ \n"
                                 " * $Revision: 1.4 $ \n"
                                 " *******************************************************************************\n"
                                 " */\n\n"
                                 "/*******************************************************************************\n"
                                 " ###############################################################################\n"
                                 "                                                                              \n"
                                 "    WARNING: This file is generated by genrb Version " GENRB_VERSION ".       \n"
                                 "             If you edit this file, please make sure that, the source         \n"
                                 "             of this file (XXXX.txt in LocaleElements_XXXX.java)              \n"
                                 "             is also edited                                                   \n"
                                 " ###############################################################################\n"
                                 " *******************************************************************************\n"
                                 " */\n\n";
static const char* openBrace="{\n";
static const char* closeBrace="}\n";
static const char* closeClass="    };\n"
                              "}\n";

static const char* javaClass =  "package com.ibm.icu.impl.data;\n\n"
                                "import java.util.ListResourceBundle;\n"
                                "import com.ibm.icu.impl.ICUListResourceBundle;\n\n"
                                "public class ";
 
static const char* javaClass1=  " extends ICUListResourceBundle {\n\n"
                                "    /**\n"
                                "     * Overrides ListResourceBundle \n"
                                "     */\n"
                                "    public final Object[][] getContents() { \n"
                                "          return  contents;\n"
                                "    }\n"
                                "    private static Object[][] contents = {\n";
static const char* javaClassICU= " extends ICUListResourceBundle {\n\n"
                                 "    public %s  () {\n"
                                 "          contents = new Object[][] { \n";
static int tabCount = 3;

static FileStream* out=NULL;
static struct SRBRoot* srBundle ;
static const char* outDir = NULL;

static void write_tabs(FileStream* out){
    int i=0;
    for(;i<=tabCount;i++){
        T_FileStream_write(out,"    ",4);
    }
}
static const char* enc ="";
static UConverter* conv = NULL;

#define MAX_DIGITS 10
static int32_t 
itostr(char * buffer, int32_t i, uint32_t radix, int32_t pad)
{
    int32_t length = 0;
    int32_t num = 0;
    int32_t save = i;
    int digit;
    int32_t j;
    char temp;
    
    /* if i is negative make it positive */
    if(i<0){
        i=-i;
    }
    
    do{
        digit = (int)(i % radix);
        buffer[length++]=(digit<=9?(0x0030+digit):(0x0030+digit+7));
        i=i/radix;
    } while(i);

    while (length < pad){
        buffer[length++] = 0x0030;/*zero padding */
    }
    
    /* if i is negative add the negative sign */
    if(save < 0){
        buffer[length++]='-';
    }

    /* null terminate the buffer */
    if(length<MAX_DIGITS){
        buffer[length] =  0x0000;
    }

    num= (pad>=length) ? pad :length;
 

    /* Reverses the string */
    for (j = 0; j < (num / 2); j++){
        temp = buffer[(length-1) - j];
        buffer[(length-1) - j] = buffer[j];
        buffer[j] = temp;
    }
    return length;
}



static int32_t 
uCharsToChars( char* target,int32_t targetLen, UChar* source, int32_t sourceLen,UErrorCode* status){
    int i=0, j=0;
    char str[30]={'\0'};
    while(i<sourceLen){
        if(source[i]==0x0A){
            if(j+2<targetLen){
                uprv_strcat(target,"\\r");
            }
            j+=2;
        }else if(source[i]==0x0D){
            if(j+2<targetLen){
                uprv_strcat(target,"\\f");
            }
            j+=2;
        }else if(source[i] == '"'){
            if(source[i-1]=='\''){
                if(j+2<targetLen){
                    uprv_strcat(target,"\\");
                    target[j+1]= (char)source[i];
                }
                j+=2;
            }else if(source[i-1]!='\\'){
                if(j+2<targetLen){
                    uprv_strcat(target,"\\");
                    target[j+1]= (char)source[i];
                }
                j+=2;
            }
        }else if(source[i]=='\\'){
            if(i+1<sourceLen){
                switch(source[i+1]){
                case ',':
                case '!':
                case '?':
                case '#':
                case '.':
                case '%':
                case '&':
                case ':':
                case ';':
                    if(j+2<targetLen){
                       uprv_strcat(target,"\\\\");
                    }
                    j+=2;
                    break;
                case '"':
                case '\'':
                    if(j+3<targetLen){
                       uprv_strcat(target,"\\\\\\");
                    }
                    j+=3;
                    break;
                default :
                    if(j<targetLen){
                        target[j]=(char)source[i];
                    }
                    j++;
                    break;
                }
            }else{
                if(j<targetLen){
                    uprv_strcat(target,"\\\\");
                }
                j+=2;
            }
        }else if(source[i]>=0x20 && source[i]<0x7F/*ASCII*/){
            if(j<targetLen){
                target[j] = (char) source[i];
            }
            j++;            
        }else{
            if(enc =="" || source[i]==0x0000){
                uprv_strcpy(str,"\\u");
                itostr(str+2,source[i],16,4);
                if(j+6<targetLen){
                    uprv_strcat(target,str);
                }
                j+=6;
            }else{
                char dest[30] = {0};
                int retVal=ucnv_fromUChars(conv,dest,30,source+i,1,status);
                if(U_FAILURE(*status)){
                    return 0;
                }
                if(j+retVal<targetLen){
                    uprv_strcat(target,dest);
                }
                j+=retVal;
            }
        }
        i++;
    }
    return j;
}


static uint32_t 
strrch(const char* source,uint32_t sourceLen,char find){
    const char* tSource = source  ;
    const char* tSourceEnd =source + (sourceLen-1);
    while(tSourceEnd>= source){
        if(*tSourceEnd==find){
            return (tSourceEnd-source);
        }
        tSourceEnd--;
    }
    return (tSourceEnd-source);
}

static void
str_write_java( uint16_t* src, int32_t srcLen, UErrorCode *status){

    uint32_t length = srcLen*8;
    uint32_t retVal = 0;
    char* buf = (char*) malloc(sizeof(char)*length);
    memset(buf,0,length);
  
    retVal = uCharsToChars(buf,length,src,srcLen,status);
    
    if(U_FAILURE(*status)){
        return;
    }
    
    if(retVal > 80 ){
        uint32_t len = 0;
        char* current = buf;
        char* str =NULL;
        uint32_t add;
        uint32_t index;
        while(len < retVal){
            index =0;
            add = 80;
            current = buf +len;
            index = strrch(current,add,'\\');
            if(current[index+1]=='u'){
                if((add-index)<6){
                    add+= (6-(add-index));
                }
            }else{
                if(current[index-1]!='\\'){
                    if((add-index)<2){
                        add+= (2-(add-index));
                    }
                }
            }
            T_FileStream_write(out,"\"",1);
            if(len+add<retVal){
                T_FileStream_write(out,current,add);
                T_FileStream_write(out,"\"+\n",3);
                write_tabs(out);
            }else{
                T_FileStream_write(out,current,retVal-len);
            }
            len+=add;
        }
    }else{
        T_FileStream_write(out,"\"",1);
        T_FileStream_write(out, buf,retVal);
    }
    T_FileStream_write(out,"\",\n",3);

}

/* Writing Functions */
static void 
string_write_java(struct SResource *res,UErrorCode *status) {       
    str_write_java(res->u.fString.fChars,res->u.fString.fLength,status);
     if(uprv_strcmp(srBundle->fKeys+res->fKey,"Rule")==0){
        UChar* buf = (UChar*) uprv_malloc(sizeof(UChar)*res->u.fString.fLength);
        int32_t bufLen =0;
        uprv_memcpy(buf,res->u.fString.fChars,res->u.fString.fLength);      
        uprv_free(buf);
     }
}

static void 
array_write_java( struct SResource *res, UErrorCode *status) {

    uint32_t  i         = 0;
    char* arr ="new String[] { \n";
    struct SResource *current = NULL;
    struct SResource *first =NULL;
    UBool decrementTabs = FALSE;
    UBool allStrings    = TRUE;
    UBool isTable       = TRUE;
    if (U_FAILURE(*status)) {
        return;
    }

    if (res->u.fArray.fCount > 0) {

        current = res->u.fArray.fFirst;
        i = 0;
        while(current != NULL){
            if(current->fType!=RES_STRING){
                allStrings = FALSE;
                break;
            }
            current= current->fNext;
        }

        current = res->u.fArray.fFirst;
         if(allStrings==FALSE){
            char* object = "new Object[]{\n";
            write_tabs(out);
            T_FileStream_write(out, object,uprv_strlen(object));
            tabCount++;
            decrementTabs = TRUE;
        }else{
            write_tabs(out);
            T_FileStream_write(out,arr,uprv_strlen(arr));
            tabCount++;
        }
        first=current;
        while (current != NULL) {
            if(current->fType==RES_STRING){
                write_tabs(out);
            }
            res_write_java(current, status);
            if(U_FAILURE(*status)){
                return;
            }
            i++;
            current = current->fNext;
        }
        T_FileStream_write(out,"\n",1);

        tabCount--;
        write_tabs(out);
        T_FileStream_write(out,"},\n",3);

    } else {
        /* array is empty */

    }
}

static void 
intvector_write_java( struct SResource *res, UErrorCode *status) {
    uint32_t i = 0;
    char* intArr = "new Integer[] {\n";
    char* intC   = "new Integer(";
    char* stringArr = "new String[]{\n"; 
    char buf[100];
    int len =0;
    buf[0]=0;
    write_tabs(out);

    if(uprv_strcmp(srBundle->fKeys+res->fKey,"DateTimeElements")==0){
        T_FileStream_write(out,stringArr,uprv_strlen(stringArr));
        tabCount++;
        for(i = 0; i<res->u.fIntVector.fCount; i++) {
            write_tabs(out);
            len=itostr(buf,res->u.fIntVector.fArray[i],10,0);
            T_FileStream_write(out,"\"",1);
            T_FileStream_write(out,buf,len);
            T_FileStream_write(out,"\",",2);
            T_FileStream_write(out,"\n",1);
        }
    }else{
        T_FileStream_write(out,intArr,uprv_strlen(intArr));
        tabCount++;
        for(i = 0; i<res->u.fIntVector.fCount; i++) {
            write_tabs(out);
            T_FileStream_write(out,intC,uprv_strlen(intC));
            len=itostr(buf,res->u.fIntVector.fArray[i],10,0);
            T_FileStream_write(out,buf,len);
            T_FileStream_write(out,"),",2);
            T_FileStream_write(out,"\n",1);
        }
    }
    tabCount--;
    write_tabs(out);
    T_FileStream_write(out,"},\n",3);
}

static void 
int_write_java(struct SResource *res,UErrorCode *status) {
    uint32_t i = 0;
    char* intC   =  "new Integer(";
    char buf[100];
    int len =0;
    buf[0]=0;

    /* write the binary data */
    write_tabs(out);
    T_FileStream_write(out,intC,uprv_strlen(intC));
    len=itostr(buf,res->u.fIntValue.fValue,10,0);
    T_FileStream_write(out,buf,len);
    T_FileStream_write(out,"),\n",3 );

}

static void 
bin_write_java( struct SResource *res, UErrorCode *status) {
    char* arr ="new Object[][]{\n";
    char* type = "COMPRESSED_BINARY, ";
    uint32_t i =0;
    int len =0,count=1;
    char* ext;
    int32_t srcLen=res->u.fBinaryValue.fLength;
    /*char buffer[16];*/
  
    if(srcLen>0 ){
        uint16_t* target=NULL;
        uint16_t* saveTarget = NULL;
        int32_t tgtLen = 0;
        if(uprv_strcmp(srBundle->fKeys+res->fKey,"%%CollationBin")==0 || uprv_strcmp(srBundle->fKeys+res->fKey,"BreakDictionaryData")==0){
            char fileName[1024] ={0};
            char fn[1024] =  {0};
            FileStream* datFile = NULL;
            if(uprv_strcmp(srBundle->fKeys+res->fKey,"BreakDictionaryData")==0){
                uprv_strcat(fileName,"BreakDictionaryData");
                ext = ".ucs";
            }else{
                uprv_strcat(fileName,"CollationElements");
                ext=".res";
            }
            if(uprv_strcmp(srBundle->fLocale,"root")!=0){
                uprv_strcat(fileName,"_");
                uprv_strcat(fileName,srBundle->fLocale);
            }
            
            uprv_strcat(fileName,ext);

            uprv_strcat(fn,outDir);
            if(outDir[uprv_strlen(outDir)-1]!=U_FILE_SEP_CHAR){
                uprv_strcat(fn,U_FILE_SEP_STRING);
            }
            uprv_strcat(fn,fileName);
            type = "RESOURCE_BINARY,";
            write_tabs(out);
            T_FileStream_write(out,arr,uprv_strlen(arr));
            tabCount++;
            write_tabs(out);
            T_FileStream_write(out,openBrace,uprv_strlen(openBrace));
            tabCount++;
            write_tabs(out);
            T_FileStream_write(out,type,uprv_strlen(type));
            T_FileStream_write(out,"\"",1);
            T_FileStream_write(out,fileName,uprv_strlen(fileName));
            T_FileStream_write(out,"\"\n",2);
            tabCount--;
            write_tabs(out);
            T_FileStream_write(out,"},\n",3);
            tabCount--;
            write_tabs(out);
            T_FileStream_write(out,"},\n",3);
            datFile=T_FileStream_open(fn,"w");
            T_FileStream_write(datFile,res->u.fBinaryValue.fData,res->u.fBinaryValue.fLength);
            T_FileStream_close(datFile);

        }else{
            if(uprv_strcmp(srBundle->fKeys+res->fKey,"BreakDictionaryData")==0){
                srcLen = res->u.fBinaryValue.fLength/2;
                target = (uint16_t*)malloc(sizeof(uint16_t) * srcLen);
                if(target){
                    saveTarget  = target;
                    type = "COMPRESSED_STRING, ";
                    tgtLen = usArrayToRLEString((uint16_t*)res->u.fBinaryValue.fData,
                                                 srcLen,target, srcLen,status);
                    if(U_FAILURE(*status)){
                         printf("Could not encode got error : %s \n", u_errorName(*status));
                         return;
                    }
#if DEBUG
                    {
                        /***************** Test Roundtripping *********************/
                        int32_t myTargetLen = rleStringToUCharArray(target,tgtLen,NULL,0,status);
                        uint16_t* myTarget = (uint16_t*) malloc(sizeof(uint16_t) * myTargetLen);
                        int i=0;
                        int32_t retVal=0;
                        uint16_t* saveSrc = (uint16_t*)res->u.fBinaryValue.fData;
                        *status = U_ZERO_ERROR;
                        retVal=rleStringToUCharArray(target,tgtLen,myTarget,myTargetLen,status);
                        if(U_SUCCESS(*status)){

                            for(i=0; i< srcLen;i++){
                                if(saveSrc[i]!= myTarget[i]){
                                    printf("the encoded string cannot be decoded Expected : 0x%04X Got : %: 0x%04X at %i\n",res->u.fBinaryValue.fData[i],myTarget[i], i);
                                }
                            }
                        }else{
                            printf("Could not decode got error : %s \n", u_errorName(*status));
                        }
                        free(myTarget);
                     }
#endif

                }else{
                    *status= U_MEMORY_ALLOCATION_ERROR;
                    return;
                }
            }else{
                srcLen = res->u.fBinaryValue.fLength;
                target = (uint16_t*)malloc(sizeof(uint16_t) * srcLen);
                saveTarget  = target;
                if(target){
                    tgtLen = byteArrayToRLEString(res->u.fBinaryValue.fData,
                                                  srcLen,target, srcLen,status);
                    if(U_FAILURE(*status)){
                         printf("Could not encode got error : %s \n", u_errorName(*status));
                         return;
                    }
#if DEBUG
                    /***************** Test Roundtripping *********************/
                    {
                        int32_t myTargetLen = rleStringToByteArray(target,tgtLen,NULL,0,status);
                        uint8_t* myTarget = (uint8_t*) malloc(sizeof(uint8_t) * myTargetLen);
                        int i=0;
                        int32_t retVal=0;

                        *status = U_ZERO_ERROR;
                        retVal=rleStringToByteArray(target,tgtLen,myTarget,myTargetLen,status);
                        if(U_SUCCESS(*status)){

                            for(i=0; i< srcLen;i++){
                                if(res->u.fBinaryValue.fData[i]!= myTarget[i]){
                                    printf("the encoded string cannot be decoded Expected : 0x%02X Got : %: 0x%02X at %i\n",res->u.fBinaryValue.fData[i],myTarget[i], i);
                                }
                            }
                        }else{
                            printf("Could not decode got error : %s \n", u_errorName(*status));
                        }
                        free(myTarget);

                    }
#endif

                }else{
                    *status = U_MEMORY_ALLOCATION_ERROR;
                    return;
                }

            }
        
        

            write_tabs(out);
            T_FileStream_write(out,arr,uprv_strlen(arr));
            tabCount++;
            write_tabs(out);
            T_FileStream_write(out,openBrace,uprv_strlen(openBrace));
            tabCount++;
            write_tabs(out);
            T_FileStream_write(out,type,uprv_strlen(type));
            str_write_java(target,tgtLen,status);
            tabCount--;
            write_tabs(out);
            T_FileStream_write(out,"},\n",3);
            tabCount--;
            write_tabs(out);
            T_FileStream_write(out,"},\n",3);


            free(target);
        }
   }
   /* uprv_strcpy(buffer,"(byte) 0x");

    if(res->u.fBinaryValue.fLength>0 ){
        write_tabs(out);
        T_FileStream_write(out,byte,uprv_strlen(byte));
        tabCount++;
        write_tabs(out);
        while(i<res->u.fBinaryValue.fLength){
            uint8_t data= res->u.fBinaryValue.fData[i];
            buffer[9] =0;
            len=itostr(&buffer[9],data,16,2);
            T_FileStream_write(out,buffer,len+9);
            T_FileStream_write(out,", ",2);
            if(count==10){
                count=0;
                T_FileStream_write(out,"\n",1);
                write_tabs(out);
            }
            count++;
            i++;
        }
        T_FileStream_write(out,"\n",1);
        tabCount--;
        write_tabs(out);
        T_FileStream_write(out,"},\n",3);
    }*/
}


static UBool start = TRUE;

static void 
table_write_java(struct SResource *res, UErrorCode *status) {
    uint32_t  i         = 0;
    char* key =NULL;
    UBool allStrings =TRUE;
    struct SResource *current = NULL;
    struct SResource *save = NULL;
    char* obj = "new Object[][]{\n";
    UBool embeddedTables = FALSE;

    if (U_FAILURE(*status)) {
        return ;
    }
    
    if (res->u.fTable.fCount > 0) {
        if(start==FALSE){
            write_tabs(out);
            T_FileStream_write(out,obj,uprv_strlen(obj));
            tabCount++;
        }
        start = FALSE;
        save = current = res->u.fTable.fFirst;
        i       = 0;


        while (current != NULL) {
            assert(i < res->u.fTable.fCount);
            write_tabs(out);
            
            T_FileStream_write(out,"{\n",2);


            tabCount++;
            allStrings=FALSE;

            write_tabs(out);

            T_FileStream_write(out,"\"",1);
            T_FileStream_write(out,srBundle->fKeys+current->fKey,uprv_strlen(srBundle->fKeys+current->fKey));
            T_FileStream_write(out,"\",",2);

            if(current->fType!=RES_STRING){
                T_FileStream_write(out,"\n",1);
            }

            res_write_java(current, status);
            if(U_FAILURE(*status)){
                return;
            }
            i++;
            current = current->fNext;
            tabCount--;
            write_tabs(out);
            T_FileStream_write(out,"},\n",3);
        }
        if(tabCount>4){
            tabCount--;
            write_tabs(out);
            T_FileStream_write(out,"},\n",3);
        }

    } else {
        /* table is empty */

    }

}

void 
res_write_java(struct SResource *res,UErrorCode *status) {
    
    if (U_FAILURE(*status)) {
        return ;
    }

    if (res != NULL) {
        switch (res->fType) {
        case RES_STRING:
             string_write_java    (res, status);
             return;
        case RES_INT_VECTOR:
             intvector_write_java (res, status);
             return;
        case RES_BINARY:
             bin_write_java       (res, status);
             return;
        case RES_INT:
             int_write_java       (res, status);
             return;
        case RES_ARRAY:
             array_write_java     (res, status);
             return;
        case RES_TABLE:
             table_write_java     (res, status);
             return;

        default:
            break;
        }
    }

    *status = U_INTERNAL_PROGRAM_ERROR;
}

void 
bundle_write_java(struct SRBRoot *bundle, const char *outputDir,const char* outputEnc, char *writtenFilename, int writtenFilenameLen, UErrorCode *status) {

    UNewDataMemory *mem        = NULL;
    uint32_t        usedOffset = 0;
    char fileName[256] = {'\0'};
    char className[256]={'\0'};
    char constructor[1000] = { 0 };    
    UBool j1 =FALSE;
    outDir = outputDir;
    uprv_strcpy(className, "LocaleElements");
    srBundle = bundle;
    if(uprv_strcmp(srBundle->fLocale,"root")!=0){
        uprv_strcat(className,"_");
        uprv_strcat(className,srBundle->fLocale);
    }
    if(outputDir){
        uprv_strcpy(fileName, outputDir);
        if(outputDir[uprv_strlen(outputDir)-1] !=U_FILE_SEP_CHAR){
            uprv_strcat(fileName,U_FILE_SEP_STRING);
        }
        uprv_strcat(fileName,className);
        uprv_strcat(fileName,".java");
    }else{
        uprv_strcat(fileName,className);
        uprv_strcat(fileName,".java");
    }

    if (writtenFilename) {
        uprv_strncpy(writtenFilename, fileName, writtenFilenameLen);
    }

    if (U_FAILURE(*status)) {
        return;
    }
    
    out= T_FileStream_open(fileName,"w");

    if(out==NULL){
        *status = U_FILE_ACCESS_ERROR;
        return;
    }

    T_FileStream_write(out,copyRight,uprv_strlen(copyRight));
    T_FileStream_write(out,javaClass,uprv_strlen(javaClass));
    T_FileStream_write(out,className,uprv_strlen(className));
    if(j1){
        T_FileStream_write(out,javaClass1,uprv_strlen(javaClass1));
    }else{
        sprintf(constructor,javaClassICU,className);
        T_FileStream_write(out,constructor,uprv_strlen(constructor));
    }

    if(outputEnc && outputEnc!=""){
        /* store the output encoding */
        enc = outputEnc;
        conv=ucnv_open(enc,status);
        if(U_FAILURE(*status)){
            return;
        }
    }
    res_write_java(bundle->fRoot, status);
    if(j1){
        T_FileStream_write(out,closeClass, uprv_strlen(closeClass));
    }else{
        tabCount--;
        write_tabs(out);
        T_FileStream_write(out,"};\n",3);
        tabCount--;
        write_tabs(out);
        T_FileStream_write(out,closeBrace,uprv_strlen(closeBrace));
        T_FileStream_write(out,closeBrace,uprv_strlen(closeBrace));
    }



    T_FileStream_close(out);
    
    ucnv_close(conv);
}
