/*
**********************************************************************
* Copyright (C) 2000, International Business Machines Corporation 
* and others.  All Rights Reserved.
**********************************************************************

Get a message out of the default resource bundle, messageformat it,
and print it to stderr
*/

#ifndef _UWMSG
#define _UWMSG
#include "unicode/utypes.h"

/* Set the path to wmsg's bundle.
   Caller owns storage.
*/
U_CAPI void u_wmsg_setPath(const char *path, UErrorCode *err);

/* Format a message and print it's output to stderr */
U_CAPI void u_wmsg(const char *tag, ... );

/* format an error message */
U_CAPI const UChar* u_wmsg_errorName(UErrorCode err);

#endif
