/*
*******************************************************************************
*
*   Copyright (C) 1999-2008, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  gentest.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2000mar03
*   created by: Madhu Katragadda
*
*   This program writes a little data file for testing the udata API.
*/

#include <stdio.h>
#include <stdlib.h>
#include "unicode/utypes.h"
#include "unicode/putil.h"
#include "unicode/uclean.h"
#include "unicode/udata.h"
#include "unicode/udbgutil.h"
#include "unicode/ucal.h"
#include "unewdata.h"
#include "cmemory.h"
#include "cstring.h"
#include "uoptions.h"
#include "gentest.h"

#define DATA_NAME "test"
#define DATA_TYPE "icu"

/* UDataInfo cf. udata.h */
static const UDataInfo dataInfo={
    sizeof(UDataInfo),
    0,

    U_IS_BIG_ENDIAN,
    U_CHARSET_FAMILY,
    sizeof(UChar),
    0,

    {0x54, 0x65, 0x73, 0x74},     /* dataFormat="Test" */
    {1, 0, 0, 0},                 /* formatVersion */
    {1, 0, 0, 0}                  /* dataVersion */
};

static void createData(const char*, UErrorCode *);

static int outputJavaStuff(const char * progname, const char *outputDir);

static UOption options[]={
  /*0*/ UOPTION_HELP_H,
  /*1*/ UOPTION_HELP_QUESTION_MARK,
  /*2*/ UOPTION_DESTDIR,
  /*3*/ UOPTION_DEF("genres", 'r', UOPT_NO_ARG),
  /*4*/ UOPTION_DEF("javastuff", 'j', UOPT_NO_ARG),  
};

extern int
main(int argc, char* argv[]) {
    UErrorCode errorCode = U_ZERO_ERROR;

    /* preset then read command line options */
    options[2].value=u_getDataDirectory();
    argc=u_parseArgs(argc, argv, sizeof(options)/sizeof(options[0]), options);

    /* error handling, printing usage message */
    if(argc<0) {
        fprintf(stderr,
            "error in command line argument \"%s\"\n",
            argv[-argc]);
    }
    if(argc<0 || options[0].doesOccur || options[1].doesOccur) {
        fprintf(stderr,
            "usage: %s [-options]\n"
            "\tcreate the test file " DATA_NAME "." DATA_TYPE " unless the -r option is given.\n"
            "\toptions:\n"
            "\t\t-h or -? or --help  this usage text\n"
            "\t\t-d or --destdir     destination directory, followed by the path\n"
            "\t\t-r or --genres      generate resource file testtable32.txt instead of UData test \n"
            "\t\t-j or --javastuff   generate Java source for DebugUtilities. \n",
            argv[0]);
        return argc<0 ? U_ILLEGAL_ARGUMENT_ERROR : U_ZERO_ERROR;
    }

    if( options[4].doesOccur ) {
    	return outputJavaStuff( argv[0], options[2].value );
    } else if ( options[3].doesOccur ) {
        return genres32( argv[0], options[2].value );
    } else { 
        /* printf("Generating the test memory mapped file\n"); */
        createData(options[2].value, &errorCode);
    }
    return U_FAILURE(errorCode);
}

/* Create data file ----------------------------------------------------- */
static void
createData(const char* outputDirectory, UErrorCode *errorCode) {
    UNewDataMemory *pData;
    char stringValue[]={'Y', 'E', 'A', 'R', '\0'};
    uint16_t intValue=2000;

    long dataLength;
    uint32_t size;

    pData=udata_create(outputDirectory, DATA_TYPE, DATA_NAME, &dataInfo,
                       U_COPYRIGHT_STRING, errorCode);
    if(U_FAILURE(*errorCode)) {
        fprintf(stderr, "gentest: unable to create data memory, error %d\n", *errorCode);
        exit(*errorCode);
    }

    /* write the data to the file */
    /* a 16 bit value  and a String*/
    udata_write16(pData, intValue);
    udata_writeString(pData, stringValue, sizeof(stringValue));

    /* finish up */
    dataLength=udata_finish(pData, errorCode);
    if(U_FAILURE(*errorCode)) {
        fprintf(stderr, "gentest: error %d writing the output file\n", *errorCode);
        exit(*errorCode);
    }
    size=sizeof(stringValue) + sizeof(intValue);


    if(dataLength!=(long)size) {
        fprintf(stderr, "gentest: data length %ld != calculated size %lu\n",
            dataLength, (unsigned long)size);
        exit(U_INTERNAL_PROGRAM_ERROR);
    }
}

/* Create Java file ----------------------------------------------------- */

int getCurrentYear() {
    UErrorCode status=U_ZERO_ERROR;
    int year = 0;
    UCalendar *cal =  ucal_open(NULL, -1, NULL, UCAL_TRADITIONAL, &status);
    ucal_setMillis(cal, ucal_getNow(), &status);
    year = ucal_get(cal, UCAL_YEAR, &status);
    ucal_close(cal);
    return year;
}

static int
outputJavaStuff(const char* progname, const char *outputDir) {
    int32_t i,t,count;
    char file[512];
    FILE *out;
    int year = getCurrentYear();

    uprv_strcpy(file,outputDir);
    if(*outputDir &&  /* don't put a trailing slash if outputDir is empty */ 
        file[strlen(file)-1]!=U_FILE_SEP_CHAR) {
            uprv_strcat(file,U_FILE_SEP_STRING);
    }
    uprv_strcat(file,"DebugUtilitiesData.java");
    out = fopen(file, "w");
    /*puts(file);*/
    printf("%s: Generating %s\n", progname, file);
    if(out == NULL) {
        fprintf(stderr, "%s: Couldn't create resource test file %s\n",
            progname, file);
        return 1;
    }

    fprintf(out, "/** Copyright (C) %d, International Business Machines Corporation and Others. All Rights Reserved. **/\n\n", year);
    fprintf(out, "/* NOTE: this file is AUTOMATICALLY GENERATED by gentest. */\n\n");
    fprintf(out, "package com.ibm.icu.dev.test.util;\n\n");
    fprintf(out, "public class DebugUtilitiesData extends Object {\n");
    fprintf(out, "    public static final String ICU4C_VERSION=\"%s\";\n", U_ICU_VERSION);
    for(t=0;t<UDBG_ENUM_COUNT;t++) {
        fprintf(out, "    public static final int %s = %d;\n", udbg_enumName(UDBG_UDebugEnumType,t), t);
    }
    fprintf(out, "    public static final String [] TYPES = { \n");
    for(t=0;t<UDBG_ENUM_COUNT;t++) {
        fprintf(out, "        \"%s\", /* %d */\n", udbg_enumName(UDBG_UDebugEnumType,t), t);
    }
    fprintf(out, "    };\n\n");

    fprintf(out, "    public static final String [][] NAMES = { \n");
    for(t=0;t<UDBG_ENUM_COUNT;t++) {
        count = udbg_enumCount((UDebugEnumType)t);
        fprintf(out, "        /* %s, %d */\n", udbg_enumName(UDBG_UDebugEnumType,t), t);
        fprintf(out, "        { \n");
        for(i=0;i<count;i++) {
            fprintf(out, 
                "           \"%s\", /* %d */ \n", udbg_enumName((UDebugEnumType)t,i), i);
        }
        fprintf(out, "        },\n");
    }
    fprintf(out, "    };\n\n");

    fprintf(out, "    public static final int [][] VALUES = { \n");
    for(t=0;t<UDBG_ENUM_COUNT;t++) {
        count = udbg_enumCount((UDebugEnumType)t);
        fprintf(out, "        /* %s, %d */\n", udbg_enumName(UDBG_UDebugEnumType,t), t);
        fprintf(out, "        { \n");
        for(i=0;i<count;i++) {
            fprintf(out, 
                "           ");
            switch(t) {
            case UDBG_UCalendarDateFields:
            case UDBG_UCalendarMonths:
                /* Temporary workaround for IS_LEAP_MOTH #6051 */
                if (t == UDBG_UCalendarDateFields && i == 22) {
                    fprintf(out, "com.ibm.icu.util.ChineseCalendar.%s, /* %d */", udbg_enumName((UDebugEnumType)t,i), i);
                } else
                fprintf(out, "com.ibm.icu.util.Calendar.%s, /* %d */", udbg_enumName((UDebugEnumType)t,i), i);
                break;
            case UDBG_UDebugEnumType:
            default:
                fprintf(out,"%d, /* %s */", i, udbg_enumName((UDebugEnumType)t,i));
            }
            fprintf(out,"\n");
        }
        fprintf(out, "        },\n");
    }
    fprintf(out, "    };\n\n");
    fprintf(out, "}\n");

    fclose(out);

    return 0;

#if 0
    int32_t count = udbg_enumCount((UDebugEnumType)t);
    if(count == -1) {
        fprintf(stderr,"%s: enumCount(%d) returned -1\n", progname, count);
        return 1;
    }
    for(t=0;t<=UDBG_ENUM_COUNT;t++) {
        int32_t count = udbg_enumCount((UDebugEnumType)t);
        if(count == -1) {
            fprintf(stderr,"%s: enumCount(%d) returned -1\n", progname, count);
            return 1;
        }
        for(i=0;i<=count;i++) {
            if(i<count) {
                if( i!=udbg_enumArrayValue((UDebugEnumType)t, i)) {
                    fprintf(stderr, "%s: FAIL: udbg_enumArrayValue(%d,%d) returned %d, expected %d\n", progname, t, i, udbg_enumArrayValue((UDebugEnumType)t,i), i);
                    return 1;
                }
            }
            fprintf(stderr, "%s: udbg_enumArrayValue(%d,%d) = %s, returned %d\n", progname, t, i, 
                udbg_enumName((UDebugEnumType)t,i), udbg_enumArrayValue((UDebugEnumType)t,i));
        }
        if(udbg_enumExpectedCount((UDebugEnumType)t) != count) {
            fprintf(stderr, "%s: FAIL: udbg_enumExpectedCount(%d): %d, != UCAL_FIELD_COUNT=%d \n", progname, t, udbg_enumExpectedCount((UDebugEnumType)t), count);
            return 1;
        } else {
            fprintf(stderr, "%s: udbg_ucal_fieldCount: %d, UCAL_FIELD_COUNT=udbg_enumCount %d ", progname, udbg_enumExpectedCount((UDebugEnumType)t), count);
        }
    }
#endif
}
