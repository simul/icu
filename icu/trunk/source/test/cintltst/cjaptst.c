/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-1999, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/********************************************************************************
*
* File CJAPTST.C
*
* Modification History:
*        Name                     Description            
*     Madhu Katragadda            Ported for C API
* synwee                          Added TestBase, TestPlainDakutenHandakuten,
*                                 TestSmallLarge, TestKatakanaHiragana,
*                                 TestChooonKigoo
*********************************************************************************/
/**
 * CollationKannaTest is a third level test class.  This tests the locale
 * specific primary, secondary and tertiary rules.  For example, the ignorable
 * character '-' in string "black-bird".  The en_US locale uses the default
 * collation rules as its sorting sequence.
 */

#include <stdlib.h>
#include "unicode/utypes.h"
#include "unicode/ucol.h"
#include "unicode/uloc.h"
#include "cintltst.h"
#include "ccolltst.h"
#include "callcoll.h"
#include "cjaptst.h"
#include "unicode/ustring.h"
#include "string.h"

static UCollator *myCollation;
const static UChar testSourceCases[][MAX_TOKEN_LEN] = {
    {0x0041/*'A'*/, 0x0300, 0x0301, 0x0000},
    {0x0041/*'A'*/, 0x0300, 0x0316, 0x0000},
    {0x0041/*'A'*/, 0x0300, 0x0000},
    {0x00C0, 0x0301, 0x0000},
    {0x00C0, 0x0316, 0x0000},
    {0xff9E, 0x0000},
    {0x3042, 0x0000},
    {0x30A2, 0x0000},
    {0x3042, 0x3042, 0x0000},
    {0x30A2, 0x30FC, 0x0000},
    {0x30A2, 0x30FC, 0x30C8, 0x0000}                               /*  11 */
};

const static UChar testTargetCases[][MAX_TOKEN_LEN] = {
    {0x0041/*'A'*/, 0x0301, 0x0300, 0x0000},
    {0x0041/*'A'*/, 0x0316, 0x0300, 0x0000},
    {0x00C0, 0},
    {0x0041/*'A'*/, 0x0301, 0x0300, 0x0000},
    {0x0041/*'A'*/, 0x0316, 0x0300, 0x0000},
    {0xFF9F, 0x0000},
    {0x30A2, 0x0000},
    {0x3042, 0x3042, 0x0000},
    {0x30A2, 0x30FC, 0x0000},
    {0x30A2, 0x30FC, 0x30C8, 0x0000},
    {0x3042, 0x3042, 0x3068, 0x0000}                              /*  11 */
};

const static UCollationResult results[] = {
    UCOL_GREATER,
    UCOL_EQUAL,
    UCOL_EQUAL,
    UCOL_GREATER,
    UCOL_EQUAL,
    UCOL_LESS,
    UCOL_LESS,
    UCOL_LESS,
    UCOL_GREATER,
    UCOL_LESS,
    UCOL_LESS                                          /*  11 */
};

const static UChar testBaseCases[][MAX_TOKEN_LEN] = {
  {0x30AB, 0x0000},
  {0x30AB, 0x30AD, 0x0000},
  {0x30AD, 0x0000},
  {0x30AD, 0x30AD, 0x0000}
};

const static UChar testPlainDakutenHandakutenCases[][MAX_TOKEN_LEN] = {
  {0x30CF, 0x30AB, 0x0000},
  {0x30D0, 0x30AB, 0x0000},
  {0x30CF, 0x30AD, 0x0000},
  {0x30D0, 0x30AD, 0x0000}
};

const static UChar testSmallLargeCases[][MAX_TOKEN_LEN] = {
  {0x30C3, 0x30CF, 0x0000},
  {0x30C4, 0x30CF, 0x0000},
  {0x30C3, 0x30D0, 0x0000},
  {0x30C4, 0x30D0, 0x0000}
};

const static UChar testKatakanaHiraganaCases[][MAX_TOKEN_LEN] = {
  {0x3042, 0x30C3, 0x0000},
  {0x30A2, 0x30C3, 0x0000},
  {0x3042, 0x30C4, 0x0000},
  {0x30A2, 0x30C4, 0x0000}
};

const static UChar testChooonKigooCases[][MAX_TOKEN_LEN] = {
  /*0*/ {0x30AB, 0x30FC, 0x3042, 0x0000},
  /*1*/ {0x30AB, 0x30FC, 0x30A2, 0x0000},
  /*2*/ {0x30AB, 0x30A4, 0x3042, 0x0000},
  /*3*/ {0x30AB, 0x30A4, 0x30A2, 0x0000},
  /*4*/ {0x30AD, 0x30A4, 0x3042, 0x0000},
  /*5*/ {0x30AD, 0x30A4, 0x30A2, 0x0000},
  /*6*/ {0x30AD, 0x30FC, 0x3042, 0x0000},
  /*7*/ {0x30AD, 0x30FC, 0x30A2, 0x0000}
};

void addKannaCollTest(TestNode** root)
{
  addTest(root, &TestTertiary, "tscoll/cjacoll/TestTertiary");
  addTest(root, &TestBase, "tscoll/cjacoll/TestBase");
  addTest(root, &TestPlainDakutenHandakuten, 
                                 "tscoll/cjacoll/TestPlainDakutenHandakuten");
  addTest(root, &TestSmallLarge, "tscoll/cjacoll/TestSmallLarge"); 
  addTest(root, &TestKatakanaHiragana, "tscoll/cjacoll/TestKatakanaHiragana");
  addTest(root, &TestChooonKigoo, "tscoll/cjacoll/TestChooonKigoo");
}

static void TestTertiary( )
{
    
    int32_t i;
    UErrorCode status = U_ZERO_ERROR;
    myCollation = ucol_open("ja_JP", &status);
    if(U_FAILURE(status)){
        log_err("ERROR: in creation of rule based collator: %s\n", myErrorName(status));
	return;
    }
    log_verbose("Testing Kanna(Japan) Collation with Tertiary strength\n");
    ucol_setNormalization(myCollation, UCOL_DECOMP_COMPAT);
    ucol_setStrength(myCollation, UCOL_TERTIARY);
    ucol_setAttribute(myCollation, UCOL_CASE_LEVEL, UCOL_ON, &status);
    for (i = 0; i < 11 ; i++)
    {
        doTest(myCollation, testSourceCases[i], testTargetCases[i], results[i]);
    }
    ucol_close(myCollation);
}

/* Testing base letters */
static void TestBase()
{
  int32_t i;
  UErrorCode status = U_ZERO_ERROR;
  myCollation = ucol_open("ja_JP", &status);
  if (U_FAILURE(status))
  {
    log_err("ERROR: in creation of rule based collator: %s\n", 
            myErrorName(status));
	  return;
  }
  
  log_verbose("Testing Japanese Base Characters Collation\n");
  ucol_setNormalization(myCollation, UCOL_DECOMP_COMPAT);
  ucol_setStrength(myCollation, UCOL_PRIMARY);
  for (i = 0; i < 3 ; i++)
    doTest(myCollation, testBaseCases[i], testBaseCases[i + 1], UCOL_LESS);

  ucol_close(myCollation);
}

/* Testing plain, Daku-ten, Handaku-ten letters */
static void TestPlainDakutenHandakuten(void)
{
  int32_t i;
  UErrorCode status = U_ZERO_ERROR;
  myCollation = ucol_open("ja_JP", &status);
  if (U_FAILURE(status))
  {
    log_err("ERROR: in creation of rule based collator: %s\n", 
            myErrorName(status));
	   return;
  }
  
  log_verbose("Testing plain, Daku-ten, Handaku-ten letters Japanese Characters Collation\n");
  ucol_setNormalization(myCollation, UCOL_DECOMP_COMPAT);
  ucol_setStrength(myCollation, UCOL_SECONDARY);
  for (i = 0; i < 3 ; i++)
    doTest(myCollation, testPlainDakutenHandakutenCases[i], 
           testPlainDakutenHandakutenCases[i + 1], UCOL_LESS);

  ucol_close(myCollation);
}

/* 
* Test Small, Large letters
*/
static void TestSmallLarge(void)
{
  int32_t i;
  UErrorCode status = U_ZERO_ERROR;
  myCollation = ucol_open("ja_JP", &status);
  if (U_FAILURE(status))
  {
    log_err("ERROR: in creation of rule based collator: %s\n", 
            myErrorName(status));
	   return;
  }
  
  log_verbose("Testing Japanese Small and Large Characters Collation\n");
  ucol_setNormalization(myCollation, UCOL_DECOMP_COMPAT);
  ucol_setStrength(myCollation, UCOL_TERTIARY);
  ucol_setAttribute(myCollation, UCOL_CASE_LEVEL, UCOL_ON, &status);
  for (i = 0; i < 3 ; i++)
    doTest(myCollation, testSmallLargeCases[i], testSmallLargeCases[i + 1], 
           UCOL_LESS);

  ucol_close(myCollation); 
}

/*
* Test Katakana, Hiragana letters
*/
static void TestKatakanaHiragana(void)
{
  int32_t i;
  UErrorCode status = U_ZERO_ERROR;
  myCollation = ucol_open("ja_JP", &status);
  if (U_FAILURE(status))
  {
    log_err("ERROR: in creation of rule based collator: %s\n", 
            myErrorName(status));
	   return;
  }
  
  log_verbose("Testing Japanese Katakana, Hiragana Characters Collation\n");
  ucol_setNormalization(myCollation, UCOL_DECOMP_COMPAT);
  ucol_setStrength(myCollation, UCOL_QUATERNARY);
  ucol_setAttribute(myCollation, UCOL_CASE_LEVEL, UCOL_ON, &status);
  for (i = 0; i < 3 ; i++) {
    doTest(myCollation, testKatakanaHiraganaCases[i], 
           testKatakanaHiraganaCases[i + 1], UCOL_LESS);
  }

  ucol_close(myCollation);
}

/*
* Test Choo-on kigoo
*/
static void TestChooonKigoo(void)
{
  int32_t i;
  UErrorCode status = U_ZERO_ERROR;
  myCollation = ucol_open("ja_JP", &status);
  if (U_FAILURE(status))
  {
    log_err("ERROR: in creation of rule based collator: %s\n", 
            myErrorName(status));
	   return;
  }
  
  log_verbose("Testing Japanese Choo-on Kigoo Characters Collation\n");
  ucol_setNormalization(myCollation, UCOL_DECOMP_COMPAT);
  ucol_setStrength(myCollation, UCOL_QUATERNARY);
  ucol_setAttribute(myCollation, UCOL_CASE_LEVEL, UCOL_ON, &status);
  for (i = 0; i < 7 ; i++) {
    doTest(myCollation, testChooonKigooCases[i], testChooonKigooCases[i + 1], 
           UCOL_LESS);
  }

  ucol_close(myCollation);
}
