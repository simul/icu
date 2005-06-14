/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 2003-2005, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/*
* File hpmufn.c
*
*/

#include "unicode/utypes.h"
#include "unicode/putil.h"
#include "unicode/uclean.h"
#include "unicode/uchar.h"
#include "unicode/ures.h"
#include "cintltst.h"
#include "umutex.h"
#include "unicode/utrace.h"
#include <stdlib.h>
#include <string.h>

/**
 * This should align the memory properly on any machine.
 */
typedef union {
    long    t1;
    double  t2;
    void   *t3;
} ctest_AlignedMemory;

static void TestHeapFunctions(void);
static void TestMutexFunctions(void);
static void TestIncDecFunctions(void);

void addHeapMutexTest(TestNode **root);


void
addHeapMutexTest(TestNode** root)
{
    addTest(root, &TestHeapFunctions,       "tsutil/hpmufn/TestHeapFunctions"  );
    addTest(root, &TestMutexFunctions,      "tsutil/hpmufn/TestMutexFunctions" );
    addTest(root, &TestIncDecFunctions,     "tsutil/hpmufn/TestIncDecFunctions");
}

static int32_t gMutexFailures = 0;

#define TEST_STATUS(status, expected) \
if (status != expected) { \
log_err("FAIL at  %s:%d. Actual status = \"%s\";  Expected status = \"%s\"\n", \
  __FILE__, __LINE__, u_errorName(status), u_errorName(expected)); gMutexFailures++; }


#define TEST_ASSERT(expr) \
if (!(expr)) { \
    log_err("FAILED Assertion \"" #expr "\" at  %s:%d.\n", __FILE__, __LINE__); \
    gMutexFailures++; \
}


/*  These tests do cleanup and reinitialize ICU in the course of their operation.
 *    The ICU data directory must be preserved across these operations.
 *    Here is a helper function to assist with that.
 */
static char *safeGetICUDataDirectory() {
    const char *dataDir = u_getDataDirectory();  /* Returned string vanashes with u_cleanup */
    char *retStr = NULL;
    if (dataDir != NULL) {
        retStr = (char *)malloc(strlen(dataDir)+1);
        strcpy(retStr, dataDir);
    }
    return retStr;
}


    
/*
 *  Test Heap Functions.
 *    Implemented on top of the standard malloc heap.
 *    All blocks increased in size by 8 to 16 bytes, and the poiner returned to ICU is
 *       offset up by 8 to 16, which should cause a good heap corruption if one of our "blocks"
 *       ends up being freed directly, without coming through us.
 *    Allocations are counted, to check that ICU actually does call back to us.
 */
int    gBlockCount = 0;
const void  *gContext;

static void * U_CALLCONV myMemAlloc(const void *context, size_t size) {
    char *retPtr = (char *)malloc(size+sizeof(ctest_AlignedMemory));
    if (retPtr != NULL) {
        retPtr += sizeof(ctest_AlignedMemory);
    }
    gBlockCount ++;
    return retPtr;
}

static void U_CALLCONV myMemFree(const void *context, void *mem) {
    char *freePtr = (char *)mem;
    if (freePtr != NULL) {
        freePtr -= sizeof(ctest_AlignedMemory);
    }
    free(freePtr);
}



static void * U_CALLCONV myMemRealloc(const void *context, void *mem, size_t size) {
    char *p = (char *)mem;
    char *retPtr;

    if (p!=NULL) {
        p -= sizeof(ctest_AlignedMemory);
    }
    retPtr = realloc(p, size+sizeof(ctest_AlignedMemory));
    if (retPtr != NULL) {
        p += sizeof(ctest_AlignedMemory);
    }
    return retPtr;
}


static void TestHeapFunctions() {
    UErrorCode       status = U_ZERO_ERROR;
    UResourceBundle *rb     = NULL;
    char            *icuDataDir;
    UVersionInfo unicodeVersion = {0,0,0,0};

    UTraceEntry   *traceEntryFunc;           /* Tracing function ptrs.  We need to save    */
    UTraceExit    *traceExitFunc;            /*  and restore them across calls to          */
    UTraceData    *traceDataFunc;            /*  u_cleanup() that we make in this test.    */
    const void    *traceContext;
    int32_t        traceLevel;

    icuDataDir = safeGetICUDataDirectory();   /* save icu data dir, so we can put it back
                                               *  after doing u_cleanup().                */

    utrace_getFunctions(&traceContext, &traceEntryFunc, &traceExitFunc, &traceDataFunc);
    traceLevel = utrace_getLevel();

    /* Can not set memory functions if ICU is already initialized */
    u_setMemoryFunctions(&gContext, myMemAlloc, myMemRealloc, myMemFree, &status);
    TEST_STATUS(status, U_INVALID_STATE_ERROR);

    /* Un-initialize ICU */
    u_cleanup();
    utrace_setFunctions(traceContext, traceEntryFunc, traceExitFunc, traceDataFunc);
    utrace_setLevel(traceLevel);

    /* Can not set memory functions with NULL values */
    status = U_ZERO_ERROR;
    u_setMemoryFunctions(&gContext, NULL, myMemRealloc, myMemFree, &status);
    TEST_STATUS(status, U_ILLEGAL_ARGUMENT_ERROR);
    status = U_ZERO_ERROR;
    u_setMemoryFunctions(&gContext, myMemAlloc, NULL, myMemFree, &status);
    TEST_STATUS(status, U_ILLEGAL_ARGUMENT_ERROR);
    status = U_ZERO_ERROR;
    u_setMemoryFunctions(&gContext, myMemAlloc, myMemRealloc, NULL, &status);
    TEST_STATUS(status, U_ILLEGAL_ARGUMENT_ERROR);

    /* u_setMemoryFunctions() should work with null or non-null context pointer */
    status = U_ZERO_ERROR;
    u_setMemoryFunctions(NULL, myMemAlloc, myMemRealloc, myMemFree, &status);
    TEST_STATUS(status, U_ZERO_ERROR);
    u_setMemoryFunctions(&gContext, myMemAlloc, myMemRealloc, myMemFree, &status);
    TEST_STATUS(status, U_ZERO_ERROR);


    /* After reinitializing ICU, we should not be able to set the memory funcs again. */
    status = U_ZERO_ERROR;
    u_setDataDirectory(icuDataDir);
    u_init(&status);
    TEST_STATUS(status, U_ZERO_ERROR);
    u_setMemoryFunctions(NULL, myMemAlloc, myMemRealloc, myMemFree, &status);
    TEST_STATUS(status, U_INVALID_STATE_ERROR);

    /* Doing ICU operations should cause allocations to come through our test heap */
    gBlockCount = 0;
    status = U_ZERO_ERROR;
    rb = ures_open(NULL, "es", &status);
    TEST_STATUS(status, U_ZERO_ERROR);
    if (gBlockCount == 0) {
        log_err("Heap functions are not being called from ICU.\n");
    }
    ures_close(rb);

    /* Cleanup should put the heap back to its default implementation. */
    u_cleanup();
    utrace_setFunctions(traceContext, traceEntryFunc, traceExitFunc, traceDataFunc);
    utrace_setLevel(traceLevel);
    u_setDataDirectory(icuDataDir);
    u_getUnicodeVersion(unicodeVersion);
    if (unicodeVersion[0] <= 0) {
        log_err("Properties doesn't reinitialize without u_init.\n");
    }
    status = U_ZERO_ERROR;
    u_init(&status);
    TEST_STATUS(status, U_ZERO_ERROR);

    /* ICU operations should no longer cause allocations to come through our test heap */
    gBlockCount = 0;
    status = U_ZERO_ERROR;
    rb = ures_open(NULL, "fr", &status);
    TEST_STATUS(status, U_ZERO_ERROR);
    if (gBlockCount != 0) {
        log_err("Heap functions did not reset after u_cleanup.\n");
    }
    ures_close(rb);
    free(icuDataDir);
}


/*
 *  Test u_setMutexFunctions()
 */

int         gTotalMutexesInitialized = 0;         /* Total number of mutexes created */
int         gTotalMutexesActive      = 0;         /* Total mutexes created, but not destroyed  */
int         gAccumulatedLocks        = 0;
const void *gMutexContext;

typedef struct DummyMutex {
    int  fLockCount;
    int  fMagic;
} DummyMutex;


static void U_CALLCONV myMutexInit(const void *context, UMTX *mutex, UErrorCode *status) {
    DummyMutex *theMutex;

    TEST_STATUS(*status, U_ZERO_ERROR);
    theMutex = (DummyMutex *)malloc(sizeof(DummyMutex));
    theMutex->fLockCount = 0;
    theMutex->fMagic     = 123456;
    gTotalMutexesInitialized++;
    gTotalMutexesActive++;
    gMutexContext = context;
    *mutex = theMutex;
}


static void U_CALLCONV myMutexDestroy(const void *context, UMTX  *mutex) {
    DummyMutex *This = *(DummyMutex **)mutex;

    gTotalMutexesActive--;
    TEST_ASSERT(This->fLockCount == 0);
    TEST_ASSERT(This->fMagic == 123456);
    This->fMagic = 0;
    This->fLockCount = 0;
    free(This);
}

static void U_CALLCONV myMutexLock(const void *context, UMTX *mutex) {
    DummyMutex *This = *(DummyMutex **)mutex;

    TEST_ASSERT(This->fMagic == 123456);
    This->fLockCount++;
    gAccumulatedLocks++;
}

static void U_CALLCONV myMutexUnlock(const void *context, UMTX *mutex) {
    DummyMutex *This = *(DummyMutex **)mutex;

    TEST_ASSERT(This->fMagic == 123456);
    This->fLockCount--;
    TEST_ASSERT(This->fLockCount >= 0);
}



static void TestMutexFunctions() {
    UErrorCode       status = U_ZERO_ERROR;
    UResourceBundle *rb     = NULL;
    char            *icuDataDir;

    UTraceEntry     *traceEntryFunc;           /* Tracing function ptrs.  We need to save    */
    UTraceExit      *traceExitFunc;            /*  and restore them across calls to          */
    UTraceData      *traceDataFunc;            /*  u_cleanup() that we make in this test.    */
    const void      *traceContext;
    int32_t          traceLevel;

    gMutexFailures = 0;

    /*  Save initial ICU state so that it can be restored later.
     *  u_cleanup(), which is called in this test, resets ICU's state.
     */
    icuDataDir = safeGetICUDataDirectory();
    utrace_getFunctions(&traceContext, &traceEntryFunc, &traceExitFunc, &traceDataFunc);
    traceLevel = utrace_getLevel();

    /* Can not set mutex functions if ICU is already initialized */
    u_setMutexFunctions(&gContext, myMutexInit, myMutexDestroy, myMutexLock, myMutexUnlock, &status);
    TEST_STATUS(status, U_INVALID_STATE_ERROR);

    /* Un-initialize ICU */
    u_cleanup();
    utrace_setFunctions(traceContext, traceEntryFunc, traceExitFunc, traceDataFunc);
    utrace_setLevel(traceLevel);

    /* Can not set Mutex functions with NULL values */
    status = U_ZERO_ERROR;
    u_setMutexFunctions(&gContext, NULL, myMutexDestroy, myMutexLock, myMutexUnlock, &status);
    TEST_STATUS(status, U_ILLEGAL_ARGUMENT_ERROR);
    status = U_ZERO_ERROR;
    u_setMutexFunctions(&gContext, myMutexInit, NULL, myMutexLock, myMutexUnlock, &status);
    TEST_STATUS(status, U_ILLEGAL_ARGUMENT_ERROR);
    status = U_ZERO_ERROR;
    u_setMutexFunctions(&gContext, myMutexInit, myMutexDestroy, NULL, myMutexUnlock, &status);
    TEST_STATUS(status, U_ILLEGAL_ARGUMENT_ERROR);
    status = U_ZERO_ERROR;
    u_setMutexFunctions(&gContext, myMutexInit, myMutexDestroy, myMutexLock, NULL, &status);
    TEST_STATUS(status, U_ILLEGAL_ARGUMENT_ERROR);

    /* u_setMutexFunctions() should work with null or non-null context pointer */
    status = U_ZERO_ERROR;
    u_setMutexFunctions(&gContext, myMutexInit, myMutexDestroy, myMutexLock, myMutexUnlock, &status);
    TEST_STATUS(status, U_ZERO_ERROR);
    u_setMutexFunctions(&gContext, myMutexInit, myMutexDestroy, myMutexLock, myMutexUnlock, &status);
    TEST_STATUS(status, U_ZERO_ERROR);


    /* After reinitializing ICU, we should not be able to set the mutex funcs again. */
    status = U_ZERO_ERROR;
    u_setDataDirectory(icuDataDir);
    u_init(&status);
    TEST_STATUS(status, U_ZERO_ERROR);
    u_setMutexFunctions(&gContext, myMutexInit, myMutexDestroy, myMutexLock, myMutexUnlock, &status);
    TEST_STATUS(status, U_INVALID_STATE_ERROR);

    /* Doing ICU operations should cause allocations to come through our test mutexes */
    gBlockCount = 0;
    status = U_ZERO_ERROR;
    rb = ures_open(NULL, "es", &status);
    TEST_STATUS(status, U_ZERO_ERROR);
    TEST_ASSERT(gTotalMutexesInitialized > 0);
    TEST_ASSERT(gTotalMutexesActive > 0);

    ures_close(rb);

    /* Cleanup should destroy all of the mutexes. */
    u_cleanup();
    u_setDataDirectory(icuDataDir);
    utrace_setFunctions(traceContext, traceEntryFunc, traceExitFunc, traceDataFunc);
    utrace_setLevel(traceLevel);
    status = U_ZERO_ERROR;
    TEST_ASSERT(gTotalMutexesInitialized > 0);
    TEST_ASSERT(gTotalMutexesActive == 0);


    /* Additional ICU operations should no longer use our dummy test mutexes */
    gTotalMutexesInitialized = 0;
    gTotalMutexesActive      = 0;
    u_init(&status);
    TEST_STATUS(status, U_ZERO_ERROR);

    status = U_ZERO_ERROR;
    rb = ures_open(NULL, "fr", &status);
    TEST_STATUS(status, U_ZERO_ERROR);
    TEST_ASSERT(gTotalMutexesInitialized == 0);
    TEST_ASSERT(gTotalMutexesActive == 0);

    ures_close(rb);
    free(icuDataDir);

    if(gMutexFailures) {
      log_info("Note: these failures may be caused by ICU failing to initialize/uninitialize properly.\n");
      log_verbose("Check for prior tests which may not have closed all open resources. See the internal function ures_flushCache()\n");
    }
}




/*
 *  Test Atomic Increment & Decrement Functions
 */

int         gIncCount             = 0;
int         gDecCount             = 0;
const void *gIncDecContext;


static int32_t U_CALLCONV myIncFunc(const void *context, int32_t *p) {
    int32_t  retVal;
    TEST_ASSERT(context == &gIncDecContext);
    gIncCount++;
    retVal = ++(*p);
    return retVal;
}

static int32_t U_CALLCONV myDecFunc(const void *context, int32_t *p) {
    int32_t  retVal;
    TEST_ASSERT(context == &gIncDecContext);
    gDecCount++;
    retVal = --(*p);
    return retVal;
}




static void TestIncDecFunctions() {
    UErrorCode   status = U_ZERO_ERROR;
    int32_t      t = 1; /* random value to make sure that Inc/dec works */
    char         *dataDir;

    UTraceEntry     *traceEntryFunc;           /* Tracing function ptrs.  We need to save    */
    UTraceExit      *traceExitFunc;            /*  and restore them across calls to          */
    UTraceData      *traceDataFunc;            /*  u_cleanup() that we make in this test.    */
    const void      *traceContext;
    int32_t          traceLevel;

    /* Can not set mutex functions if ICU is already initialized */
    u_setAtomicIncDecFunctions(&gIncDecContext, myIncFunc, myDecFunc,  &status);
    TEST_STATUS(status, U_INVALID_STATE_ERROR);

    /* Un-initialize ICU */
    dataDir = safeGetICUDataDirectory();
    utrace_getFunctions(&traceContext, &traceEntryFunc, &traceExitFunc, &traceDataFunc);
    traceLevel = utrace_getLevel();
    u_cleanup();
    utrace_setFunctions(traceContext, traceEntryFunc, traceExitFunc, traceDataFunc);
    utrace_setLevel(traceLevel);

    /* Can not set functions with NULL values */
    status = U_ZERO_ERROR;
    u_setAtomicIncDecFunctions(&gIncDecContext, NULL, myDecFunc,  &status);
    TEST_STATUS(status, U_ILLEGAL_ARGUMENT_ERROR);
    status = U_ZERO_ERROR;
    u_setAtomicIncDecFunctions(&gIncDecContext, myIncFunc, NULL,  &status);
    TEST_STATUS(status, U_ILLEGAL_ARGUMENT_ERROR);

    /* u_setIncDecFunctions() should work with null or non-null context pointer */
    status = U_ZERO_ERROR;
    u_setAtomicIncDecFunctions(NULL, myIncFunc, myDecFunc,  &status);
    TEST_STATUS(status, U_ZERO_ERROR);
    u_setAtomicIncDecFunctions(&gIncDecContext, myIncFunc, myDecFunc,  &status);
    TEST_STATUS(status, U_ZERO_ERROR);


    /* After reinitializing ICU, we should not be able to set the inc/dec funcs again. */
    status = U_ZERO_ERROR;
    u_setDataDirectory(dataDir);
    u_init(&status);
    TEST_STATUS(status, U_ZERO_ERROR);
    u_setAtomicIncDecFunctions(&gIncDecContext, myIncFunc, myDecFunc,  &status);
    TEST_STATUS(status, U_INVALID_STATE_ERROR);

    /* Doing ICU operations should cause our functions to be called */
    gIncCount = 0;
    gDecCount = 0;
    umtx_atomic_inc(&t);
    TEST_ASSERT(t == 2);
    umtx_atomic_dec(&t);
    TEST_ASSERT(t == 1);
    TEST_ASSERT(gIncCount > 0);
    TEST_ASSERT(gDecCount > 0);


    /* Cleanup should cancel use of our inc/dec functions. */
    /* Additional ICU operations should not use them */
    u_cleanup();
    utrace_setFunctions(traceContext, traceEntryFunc, traceExitFunc, traceDataFunc);
    utrace_setLevel(traceLevel);
    gIncCount = 0;
    gDecCount = 0;
    status = U_ZERO_ERROR;
    u_setDataDirectory(dataDir);
    u_init(&status);
    TEST_ASSERT(gIncCount == 0);
    TEST_ASSERT(gDecCount == 0);

    status = U_ZERO_ERROR;
    umtx_atomic_inc(&t);
    umtx_atomic_dec(&t);
    TEST_STATUS(status, U_ZERO_ERROR);
    TEST_ASSERT(gIncCount == 0);
    TEST_ASSERT(gDecCount == 0);

    free(dataDir);
}

