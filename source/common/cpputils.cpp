#define  EXTENDED_FUNCTIONALITY
#include "cpputils.h"
#include "cstring.h"
#include "ustring.h"



/******************************************************
 * Simple utility to set output buffer parameters
 ******************************************************/
void T_fillOutputParams(const UnicodeString* temp,
            UChar* result, 
            const int32_t resultLength,
            int32_t* resultLengthOut, 
            UErrorCode* status) 
{
  
  const int32_t actual = temp->size();
  const bool_t overflowed = actual >= resultLength;
  const int32_t returnedSize = icu_min(actual, resultLength-1);
  if ((temp->size() < resultLength) && (result != temp->getUChars()) && (returnedSize > 0)) {
    u_strcpy(result, temp->getUChars());
  }
  
  if (resultLength > 0) {
    result[returnedSize] = 0;
  }
  if (resultLengthOut) {
    *resultLengthOut = actual;
    if (SUCCESS(*status) && overflowed) {
      *status = BUFFER_OVERFLOW_ERROR;
    }
  }
}




