/*
 *******************************************************************************
 * Copyright (C) 1996-2000, International Business Machines Corporation and    *
 * others. All Rights Reserved.                                                *
 *******************************************************************************
 */

#ifndef ITRBNF_H
#define ITRBNF_H

#include "intltest.h"

#include "unicode/utypes.h"
#include "unicode/rbnf.h"


class IntlTestRBNF : public IntlTest {
 public:

  // IntlTest override
  virtual void runIndexedTest(int32_t index, UBool exec, const char* &name, char* par);

#if U_HAVE_RBNF
  /** 
   * Perform an API test
   */
  virtual void TestAPI();

  /**
   * Perform a simple spot check on the FractionalRuleSet logic
   */
  virtual void TestFractionalRuleSet();

#if 0
  /**
   * Perform API tests on llong
   */
  virtual void TestLLong();
  virtual void TestLLongConstructors();
  virtual void TestLLongSimpleOperators();
#endif

  /**
   * Perform a simple spot check on the English spellout rules
   */
  virtual void TestEnglishSpellout();

  /**
   * Perform a simple spot check on the English ordinal-abbreviation rules
   */
  virtual void TestOrdinalAbbreviations();

  /**
   * Perform a simple spot check on the duration-formatting rules
   */
  virtual void TestDurations();

  /**
   * Perform a simple spot check on the Spanish spellout rules
   */
  virtual void TestSpanishSpellout();

  /**
   * Perform a simple spot check on the French spellout rules
   */
  virtual void TestFrenchSpellout();

  /**
   * Perform a simple spot check on the Swiss French spellout rules
   */
  virtual void TestSwissFrenchSpellout();

  /**
   * Perform a simple spot check on the Italian spellout rules
   */
  virtual void TestItalianSpellout();

  /**
   * Perform a simple spot check on the German spellout rules
   */
  virtual void TestGermanSpellout();

  /**
   * Perform a simple spot check on the Thai spellout rules
   */
  virtual void TestThaiSpellout();

  /**
   * Perform a simple spot check on the Swedish spellout rules
   */
  virtual void TestSwedishSpellout();

 protected:
  virtual void doTest(RuleBasedNumberFormat* formatter, const char* testData[][2], UBool testParsing);
  virtual void doLenientParseTest(RuleBasedNumberFormat* formatter, const char* testData[][2]);

/* U_HAVE_RBNF */
#else

  virtual void TestRBNFDisabled();

/* U_HAVE_RBNF */
#endif
};

// endif ITRBNF_H
#endif
