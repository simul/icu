/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 2002, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

/* Created by weiv 05/09/2002 */

/* Base class for data driven tests */

#ifndef INTLTST_TESTMODULE
#define INTLTST_TESTMODULE

#include "unicode/utypes.h"
#include "unicode/unistr.h"
#include "intltest.h"
#include "unicode/ures.h"
#include "testdata.h"
#include "datamap.h"

/* This class abstracts the actual organization of the  
 * data for data driven tests                           
 */


class DataMap;
class TestData;

/** Facilitates internal logging of data driven test service 
 *  It would be interesting to develop this into a full      
 *  fledged control system as in Java.                       
 */
class TestLog : public IntlTest{
};


/** Main data driven test class. Corresponds to one named data 
 *  unit (such as a resource bundle. It is instantiated using  
 *  a factory method getTestDataModule 
 */
class TestDataModule {
  const char* name;

protected:
  DataMap *fInfo;
  TestLog& log;

public:
  /** Factory method. 
   *  @param name name of the test module. Usually name of a resource bundle or a XML file 
   *  @param log a logging class, used for internal error reporting.                       
   *  @param status if something goes wrong, status will be set                            
   *  @return a TestDataModule object. Use it to get test data from it                     
   */
  static TestDataModule *getTestDataModule(const char* name, TestLog& log, UErrorCode &status);
  virtual ~TestDataModule();

protected:
  TestDataModule(const char* name, TestLog& log, UErrorCode& status);

public:
  /** Name of this TestData module. 
   *  @return a name 
   */
  const char * getName() const;

  /** Get a pointer to an object owned DataMap that contains more information on this module 
   *  Usual fields are "Description", "LongDescription", "Settings". Also, if containing a   
   *  field "Headers" these will be used as the default headers, so that you don't have to   
   *  to specify per test headers.                                                           
   *  @param info pass in a const DataMap pointer. If no info, it will be set to NULL
   */
  virtual UBool getInfo(const DataMap *& info, UErrorCode &status) const = 0;

  /** Create a test data object from an index. Helpful for integrating tests with current 
   *  intltest framework which addresses the tests by index.                              
   *  @param index index of the test to be instantiated                                   
   *  @return an instantiated TestData object, ready to provide settings and cases for    
   *          the tests.                                                                  
   */
  virtual TestData* createTestData(int32_t index, UErrorCode &status) const = 0;

  /** Create a test data object from a name.                              
   *  @param name name of the test to be instantiated                                     
   *  @return an instantiated TestData object, ready to provide settings and cases for    
   *          the tests.                                                                  
   */
  virtual TestData* createTestData(const char* name, UErrorCode &status) const = 0;
};

class RBTestDataModule : public TestDataModule {
public:
  virtual ~RBTestDataModule();

public:
  RBTestDataModule(const char* name, TestLog& log, UErrorCode& status);

public:
  virtual UBool getInfo(const DataMap *& info, UErrorCode &status) const;

  virtual TestData* createTestData(int32_t index, UErrorCode &status) const;
  virtual TestData* createTestData(const char* name, UErrorCode &status) const;

private:
  UResourceBundle *getTestBundle(const char* bundleName, UErrorCode &status);
  const char* loadTestData(UErrorCode& err);

private:
  UResourceBundle *fModuleBundle;
  UResourceBundle *fTestData;
  UResourceBundle *info;
  UBool fDataTestValid;
  char *tdpath;

  const char* fTestName;
  int32_t fNumberOfTests;

};


#endif
