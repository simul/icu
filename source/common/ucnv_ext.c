/*
******************************************************************************
*
*   Copyright (C) 2003, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  ucnv_ext.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2003jun13
*   created by: Markus W. Scherer
*
*   Conversion extensions
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_LEGACY_CONVERSION

#include "ucnv_bld.h"
#include "ucnv_cnv.h"
#include "ucnv_ext.h"
#include "cmemory.h"

/*
 * ### TODO
 *
 * implement getUnicodeSet for the extension table
 * implement data swapping for it
 */

/*
 * ### TODO: probably need pointer to baseTableSharedData
 * and also copy the base table's pointers for the base table arrays etc.
 * into this sharedData
 */

/* to Unicode --------------------------------------------------------------- */

/*
 * @return lookup value for the byte, if found; else 0
 */
static U_INLINE uint32_t
ucnv_extFindToU(const uint32_t *toUSection, int32_t length, uint8_t byte) {
    uint32_t word;
    int32_t i, start, limit;

    /* check the input byte against the lowest and highest section bytes */
    start=(int32_t)UCNV_EXT_TO_U_GET_BYTE(toUSection[0]);
    limit=(int32_t)UCNV_EXT_TO_U_GET_BYTE(toUSection[length-1]);
    if(byte<start || limit<byte) {
        return 0; /* the byte is out of range */
    }

    if(length==((limit-start)+1)) {
        /* direct access on a linear array */
        return UCNV_EXT_TO_U_GET_VALUE(toUSection[byte-start]); /* could be 0 */
    }

    /*
     * Shift byte once instead of each section word and add 0xffffff.
     * We will compare the shifted/added byte (bbffffff) against
     * section words which have byte values in the same bit position.
     * If and only if byte bb < section byte ss then bbffffff<ssvvvvvv
     * for all v=0..f
     * so we need not mask off the lower 24 bits of each section word.
     */
    word=UCNV_EXT_TO_U_MAKE_WORD(byte, UCNV_EXT_TO_U_VALUE_MASK);

    /* binary search */
    start=0;
    limit=length;
    for(;;) {
        i=limit-start;
        if(i<=1) {
            break; /* done */
        }
        /* start<limit-1 */

        if(i<=4) {
            /* linear search for the last part */
            if(word>=toUSection[start]) {
                break;
            }
            if(++start<limit && word>=toUSection[start]) {
                break;
            }
            if(++start<limit && word>=toUSection[start]) {
                break;
            }
            /* always break at start==limit-1 */
            ++start;
            break;
        }

        i=(start+limit)/2;
        if(word<toUSection[i]) {
            limit=i;
        } else {
            start=i;
        }
    }

    /* did we really find it? */
    if(start<limit && byte==UCNV_EXT_TO_U_GET_BYTE(word=toUSection[start])) {
        return UCNV_EXT_TO_U_GET_VALUE(word); /* never 0 */
    } else {
        return 0; /* not found */
    }
}

/*
 * this works like ucnv_extMatchFromU() except
 * - the first character is in pre
 * - no trie is used
 * - the returned matchLength is not offset by 2
 */
static int32_t
ucnv_extMatchToU(const int32_t *cx,
                 const char *pre, int32_t preLength,
                 const char *src, int32_t srcLength,
                 const UChar **pResult, int32_t *pResultLength,
                 UBool useFallback, UBool flush) {
    const uint32_t *toUTable, *toUSection;

    uint32_t value, matchValue;
    int32_t i, j, index, length, matchLength;
    uint8_t b;

    if(cx==NULL) {
        return 0; /* no extension data, no match */
    }

    /* initialize */
    toUTable=UCNV_EXT_ARRAY(cx, UCNV_EXT_TO_U_INDEX, uint32_t);
    index=0;

    matchValue=0;
    i=j=matchLength=0;

    /* we must not remember fallback matches when not using fallbacks */

    /* match input units until there is a full match or the input is consumed */
    for(;;) {
        /* go to the next section */
        toUSection=toUTable+index;

        /* read first pair of the section */
        value=*toUSection++;
        length=UCNV_EXT_TO_U_GET_BYTE(value);
        value=UCNV_EXT_TO_U_GET_VALUE(value);
        if( value!=0 &&
            (UCNV_EXT_TO_U_IS_ROUNDTRIP(value) ||
             TO_U_USE_FALLBACK(useFallback))
        ) {
            /* remember longest match so far */
            matchValue=value;
            matchLength=i+j;
        }

        /* match pre[] then src[] */
        if(i<preLength) {
            b=(uint8_t)pre[i++];
        } else if(j<srcLength) {
            b=(uint8_t)src[j++];
        } else {
            /* all input consumed, partial match */
            if(flush || (length=(i+j))>UCNV_EXT_MAX_BYTES) {
                /*
                 * end of the entire input stream, stop with the longest match so far
                 * or: partial match must not be longer than UCNV_EXT_MAX_BYTES
                 * because it must fit into state buffers
                 */
                break;
            } else {
                /* continue with more input next time */
                return -length;
            }
        }

        /* search for the current UChar */
        value=ucnv_extFindToU(toUSection, length, b);
        if(value==0) {
            /* no match here, stop with the longest match so far */
            break;
        } else {
            if(UCNV_EXT_TO_U_IS_PARTIAL(value)) {
                /* partial match, continue */
                index=(int32_t)UCNV_EXT_TO_U_GET_PARTIAL_INDEX(value);
            } else {
                if( UCNV_EXT_TO_U_IS_ROUNDTRIP(value) ||
                     TO_U_USE_FALLBACK(useFallback)
                ) {
                    /* full match, stop with result */
                    matchValue=value;
                    matchLength=i+j;
                } else {
                    /* full match on fallback not taken, stop with the longest match so far */
                }
                break;
            }
        }
    }

    if(matchLength==0) {
        /* no match at all */
        return 0;
    }

    /* return result */
    matchValue=UCNV_EXT_TO_U_MASK_ROUNDTRIP(matchValue);
    if(UCNV_EXT_TO_U_IS_CODE_POINT(matchValue)) {
        *pResultLength=-(int32_t)matchValue;
    } else {
        *pResultLength=UCNV_EXT_TO_U_GET_LENGTH(matchValue);
        *pResult=UCNV_EXT_ARRAY(cx, UCNV_EXT_TO_U_UCHARS_INDEX, UChar)+UCNV_EXT_TO_U_GET_INDEX(matchValue);
    }

    return matchLength;
}

static U_INLINE void
ucnv_extWriteToU(UConverter *cnv,
                 const UChar *result, int32_t resultLength,
                 UChar **target, const UChar *targetLimit,
                 int32_t **offsets, int32_t srcIndex,
                 UErrorCode *pErrorCode) {
    /* output the result */
    if(resultLength<0) {
        /* output a single code point */
        ucnv_toUWriteCodePoint(
            cnv, UCNV_EXT_TO_U_GET_CODE_POINT(-resultLength),
            target, targetLimit,
            offsets, srcIndex,
            pErrorCode);
    } else {
        /* output a string - with correct data we have resultLength>0 */
        ucnv_toUWriteUChars(
            cnv,
            result, resultLength,
            target, targetLimit,
            offsets, srcIndex,
            pErrorCode);
    }
}

/*
 * target<targetLimit; set error code for overflow
 */
U_CFUNC UBool
ucnv_extInitialMatchToU(UConverter *cnv, const int32_t *cx,
                        int32_t firstLength,
                        const char **src, const char *srcLimit,
                        UChar **target, const UChar *targetLimit,
                        int32_t **offsets, int32_t srcIndex,
                        UBool flush,
                        UErrorCode *pErrorCode) {
    const UChar *result;
    int32_t resultLength, match;

    /* try to match */
    match=ucnv_extMatchToU(cx,
                           (const char *)cnv->toUBytes, firstLength,
                           *src, (int32_t)(srcLimit-*src),
                           &result, &resultLength,
                           cnv->useFallback, flush);
    if(match>0) {
        /* advance src pointer for the consumed input */
        *src+=match-firstLength;

        /* write result to target */
        ucnv_extWriteToU(cnv,
                         result, resultLength,
                         target, targetLimit,
                         offsets, srcIndex,
                         pErrorCode);
        return TRUE;
    } else if(match<0) {
        /* save state for partial match */
        const char *s;
        int32_t j;

        /* copy the first code point */
        s=(const char *)cnv->toUBytes;
        cnv->preToUFirstLength=(int8_t)firstLength;
        for(j=0; j<firstLength; ++j) {
            cnv->preToU[j]=*s++;
        }

        /* now copy the newly consumed input */
        s=*src;
        match=-match;
        for(; j<match; ++j) {
            cnv->preToU[j]=*s++;
        }
        *src=s; /* same as *src=srcLimit; because we reached the end of input */
        cnv->preToULength=(int8_t)match;
        return TRUE;
    } else /* match==0 no match */ {
        return FALSE;
    }
}

#if 0
/* ### TODO */

U_CFUNC int32_t
ucnv_extSimpleMatchToU(const int32_t *cx,
                       UChar32 cp, uint32_t *pValue,
                       UBool useFallback,
                       UErrorCode *pErrorCode) {
    const uint8_t *result;
    int32_t resultLength, match;

    /* try to match */
    match=ucnv_extMatchToU(cx,
                             cp,
                             NULL, 0,
                             NULL, 0,
                             &result, &resultLength,
                             useFallback, TRUE);
    if(match>=2) {
        /* write result for simple, single-character conversion */
        if(resultLength<0) {
            resultLength=-resultLength;
            *pValue=(uint32_t)UCNV_EXT_TO_U_GET_DATA(resultLength);
            return UCNV_EXT_TO_U_GET_LENGTH(resultLength);
        } else if(resultLength==4) {
            /* de-serialize a 4-byte result */
            *pValue=
                ((uint32_t)result[0]<<24)|
                ((uint32_t)result[1]<<16)|
                ((uint32_t)result[2]<<8)|
                result[3];
            return 4;
        }
    }

    /*
     * return no match because
     * - match>1 && resultLength>4: result too long for simple conversion
     * - match==1: no match found, <subchar1> preferred
     * - match==0: no match found in the first place
     * - match<0: partial match, not supported for simple conversion (and flush==TRUE)
     */
    return 0;
}

#endif

/*
 * continue partial match with new input
 * never called for simple, single-character conversion
 */
U_CFUNC void
ucnv_extContinueMatchToU(UConverter *cnv,
                         UConverterToUnicodeArgs *pArgs, int32_t srcIndex,
                         UErrorCode *pErrorCode) {
    const UChar *result;
    int32_t resultLength, match, length;

    match=ucnv_extMatchToU(cnv->sharedData->table->mbcs.extIndexes,
                           cnv->preToU, cnv->preToULength,
                           pArgs->source, (int32_t)(pArgs->sourceLimit-pArgs->source),
                           &result, &resultLength,
                           cnv->useFallback, pArgs->flush);
    if(match>0) {
        if(match>=cnv->preToULength) {
            /* advance src pointer for the consumed input */
            pArgs->source+=match-cnv->preToULength;
            cnv->preToULength=0;
        } else {
            /* the match did not use all of preToU[] - keep the rest for replay */
            int32_t length=cnv->preToULength-match;
            uprv_memmove(cnv->preToU, cnv->preToU+match, length);
            cnv->preToULength=(int8_t)-length;
        }

        /* write result */
        ucnv_extWriteToU(cnv,
                         result, resultLength,
                         &pArgs->target, pArgs->targetLimit,
                         &pArgs->offsets, srcIndex,
                         pErrorCode);
    } else if(match<0) {
        /* save state for partial match */
        const char *s;
        int32_t j;

        /* just _append_ the newly consumed input to preToU[] */
        s=pArgs->source;
        match=-match;
        for(j=cnv->preToULength; j<match; ++j) {
            cnv->preToU[j]=*s++;
        }
        pArgs->source=s; /* same as *src=srcLimit; because we reached the end of input */
        cnv->preToULength=(int8_t)match;
    } else /* match==0 */ {
        /*
         * no match
         *
         * We need to split the previous input into two parts:
         *
         * 1. The first codepage character is unmappable - that's how we got into
         *    trying the extension data in the first place.
         *    We need to move it from the preToU buffer
         *    to the error buffer, set an error code,
         *    and prepare the rest of the previous input for 2.
         *
         * 2. The rest of the previous input must be converted once we
         *    come back from the callback for the first character.
         *    At that time, we have to try again from scratch to convert
         *    these input characters.
         *    The replay will be handled by the ucnv.c conversion code.
         */

        /* move the first codepage character to the error field */
        uprv_memcpy(cnv->toUBytes, cnv->preToU, cnv->preToUFirstLength);
        cnv->toULength=cnv->preToUFirstLength;

        /* move the rest up inside the buffer */
        length=cnv->preToULength-cnv->preToUFirstLength;
        if(length>0) {
            uprv_memmove(cnv->preToU, cnv->preToU+cnv->preToUFirstLength, length);
        }

        /* mark preToU for replay */
        cnv->preToULength=(int8_t)-length;

        /* set the error code for unassigned */
        *pErrorCode=U_INVALID_CHAR_FOUND;
    }
}

/* from Unicode ------------------------------------------------------------- */

/*
 * @return index of the UChar, if found; else <0
 */
static U_INLINE int32_t
ucnv_extFindFromU(const UChar *fromUSection, int32_t length, UChar u) {
    int32_t i, start, limit;

    /* binary search */
    start=0;
    limit=length;
    for(;;) {
        i=limit-start;
        if(i<=1) {
            break; /* done */
        }
        /* start<limit-1 */

        if(i<=4) {
            /* linear search for the last part */
            if(u>=fromUSection[start]) {
                break;
            }
            if(++start<limit && u>=fromUSection[start]) {
                break;
            }
            if(++start<limit && u>=fromUSection[start]) {
                break;
            }
            /* always break at start==limit-1 */
            ++start;
            break;
        }

        i=(start+limit)/2;
        if(u<fromUSection[i]) {
            limit=i;
        } else {
            start=i;
        }
    }

    /* did we really find it? */
    if(start<limit && u==fromUSection[start]) {
        return start;
    } else {
        return -1; /* not found */
    }
}

/*
 * @param cx pointer to extension data; if NULL, returns 0
 * @param firstCP the first code point before all the other UChars
 * @param pre UChars that must match; !initialMatch: partial match with them
 * @param preLength length of pre, >=0
 * @param src UChars that can be used to complete a match
 * @param srcLength length of src, >=0
 * @param pResult [out] address of pointer to result bytes
 *                      set only in case of a match
 * @param pResultLength [out] address of result length variable;
 *                            gets a negative value if the length variable
 *                            itself contains the length and bytes, encoded in
 *                            the format of fromUTableValues[] and then inverted
 * @param useFallback "use fallback" flag, usually from cnv->useFallback
 * @param flush TRUE if the end of the input stream is reached
 * @return >1: matched, return value=total match length (number of input units matched)
 *          1: matched, no mapping but request for <subchar1>
 *             (only for the first code point)
 *          0: no match
 *         <0: partial match, return value=negative total match length
 *             (partial matches are never returned for flush==TRUE)
 *             (partial matches are never returned as being longer than UCNV_EXT_MAX_UCHARS)
 *         the matchLength is 2 if only firstCP matched, and >2 if firstCP and
 *         further code units matched
 */
static int32_t
ucnv_extMatchFromU(const int32_t *cx,
                   UChar32 firstCP,
                   const UChar *pre, int32_t preLength,
                   const UChar *src, int32_t srcLength,
                   const uint8_t **pResult, int32_t *pResultLength,
                   UBool useFallback, UBool flush) {
    const uint16_t *stage12, *stage3;
    const uint32_t *stage3b;

    const UChar *fromUTableUChars, *fromUSectionUChars;
    const uint32_t *fromUTableValues, *fromUSectionValues;

    uint32_t value, matchValue;
    int32_t i, j, index, length, matchLength;
    UChar c;

    if(cx==NULL) {
        return 0; /* no extension data, no match */
    }

    /* trie lookup of firstCP */
    index=firstCP>>10; /* stage 1 index */
    if(index>=cx[UCNV_EXT_FROM_U_STAGE_1_LENGTH]) {
        return 0; /* the first code point is outside the trie */
    }

    stage12=UCNV_EXT_ARRAY(cx, UCNV_EXT_FROM_U_STAGE_12_INDEX, uint16_t);
    stage3=UCNV_EXT_ARRAY(cx, UCNV_EXT_FROM_U_STAGE_3_INDEX, uint16_t);
    index=UCNV_EXT_FROM_U(stage12, stage3, index, firstCP);

    stage3b=UCNV_EXT_ARRAY(cx, UCNV_EXT_FROM_U_STAGE_3B_INDEX, uint32_t);
    value=stage3b[index];
    if(value==0) {
        return 0;
    }

    if(UCNV_EXT_TO_U_IS_PARTIAL(value)) {
        /* partial match, enter the loop below */
        index=(int32_t)UCNV_EXT_FROM_U_GET_PARTIAL_INDEX(value);

        /* initialize */
        fromUTableUChars=UCNV_EXT_ARRAY(cx, UCNV_EXT_FROM_U_UCHARS_INDEX, UChar);
        fromUTableValues=UCNV_EXT_ARRAY(cx, UCNV_EXT_FROM_U_VALUES_INDEX, uint32_t);

        matchValue=0;
        i=j=matchLength=0;

        /* we must not remember fallback matches when not using fallbacks */

        /* match input units until there is a full match or the input is consumed */
        for(;;) {
            /* go to the next section */
            fromUSectionUChars=fromUTableUChars+index;
            fromUSectionValues=fromUTableValues+index;

            /* read first pair of the section */
            length=*fromUSectionUChars++;
            value=*fromUSectionValues++;
            if( value!=0 &&
                (UCNV_EXT_FROM_U_IS_ROUNDTRIP(value) ||
                 FROM_U_USE_FALLBACK(useFallback, firstCP))
            ) {
                /* remember longest match so far */
                matchValue=value;
                matchLength=2+i+j;
            }

            /* match pre[] then src[] */
            if(i<preLength) {
                c=pre[i++];
            } else if(j<srcLength) {
                c=src[j++];
            } else {
                /* all input consumed, partial match */
                if(flush || (length=(i+j))>UCNV_EXT_MAX_UCHARS) {
                    /*
                     * end of the entire input stream, stop with the longest match so far
                     * or: partial match must not be longer than UCNV_EXT_MAX_UCHARS
                     * because it must fit into state buffers
                     */
                    break;
                } else {
                    /* continue with more input next time */
                    return -(2+length);
                }
            }

            /* search for the current UChar */
            index=ucnv_extFindFromU(fromUSectionUChars, length, c);
            if(index<0) {
                /* no match here, stop with the longest match so far */
                break;
            } else {
                value=fromUSectionValues[index];
                if(UCNV_EXT_FROM_U_IS_PARTIAL(value)) {
                    /* partial match, continue */
                    index=(int32_t)UCNV_EXT_FROM_U_GET_PARTIAL_INDEX(value);
                } else {
                    if( UCNV_EXT_FROM_U_IS_ROUNDTRIP(value) ||
                         FROM_U_USE_FALLBACK(useFallback, firstCP)
                    ) {
                        /* full match, stop with result */
                        matchValue=value;
                        matchLength=2+i+j;
                    } else {
                        /* full match on fallback not taken, stop with the longest match so far */
                    }
                    break;
                }
            }
        }

        if(matchLength==0) {
            /* no match at all */
            return 0;
        }
    } else /* result from firstCP trie lookup */ {
        if( UCNV_EXT_FROM_U_IS_ROUNDTRIP(value) ||
             FROM_U_USE_FALLBACK(useFallback, firstCP)
        ) {
            /* full match, stop with result */
            matchValue=value;
            matchLength=2;
        } else {
            /* fallback not taken */
            return 0;
        }
    }

    if(matchValue&UCNV_EXT_FROM_U_RESERVED_MASK) {
        /* do not interpret values with reserved bits used, for forward compatibility */
        return 0;
    }

    /* return result */
    if(matchValue==UCNV_EXT_FROM_U_SUBCHAR1) {
        return 1;
    }

    matchValue=UCNV_EXT_FROM_U_MASK_ROUNDTRIP(matchValue);
    length=(int32_t)UCNV_EXT_FROM_U_GET_LENGTH(matchValue);
    if(length<=UCNV_EXT_FROM_U_MAX_DIRECT_LENGTH) {
        *pResultLength=-(int32_t)matchValue;
    } else {
        *pResultLength=length;
        *pResult=UCNV_EXT_ARRAY(cx, UCNV_EXT_FROM_U_BYTES_INDEX, uint8_t)+UCNV_EXT_FROM_U_GET_DATA(matchValue);
    }

    return matchLength;
}

static U_INLINE void
ucnv_extWriteFromU(UConverter *cnv,
                   const uint8_t *result, int32_t resultLength,
                   char **target, const char *targetLimit,
                   int32_t **offsets, int32_t srcIndex,
                   UErrorCode *pErrorCode) {
    uint8_t buffer[4];

    /* output the result */
    if(resultLength<0) {
        /*
         * Generate a byte array and then write it below.
         * This is not the fastest possible way, but it should be ok for
         * extension mappings, and it is much simpler.
         * Offset and overflow handling are only done once this way.
         */
        uint8_t *p;
        uint32_t value;

        resultLength=-resultLength;
        value=(uint32_t)UCNV_EXT_FROM_U_GET_DATA(resultLength);
        resultLength=UCNV_EXT_FROM_U_GET_LENGTH(resultLength);
        /* resultLength<=UCNV_EXT_FROM_U_MAX_DIRECT_LENGTH==3 */

        p=buffer;
        switch(resultLength) {
        case 3:
            *p++=(uint8_t)(value>>16);
        case 2:
            *p++=(uint8_t)(value>>8);
        case 1:
            *p++=(uint8_t)value;
        default:
            break; /* will never occur */
        }
        result=buffer;
    }

    /* with correct data we have resultLength>0 */
    ucnv_fromUWriteBytes(cnv, (const char *)result, resultLength,
                         target, targetLimit,
                         offsets, srcIndex,
                         pErrorCode);
}

/*
 * target<targetLimit; set error code for overflow
 */
U_CFUNC UBool
ucnv_extInitialMatchFromU(UConverter *cnv, const int32_t *cx,
                          UChar32 cp,
                          const UChar **src, const UChar *srcLimit,
                          char **target, const char *targetLimit,
                          int32_t **offsets, int32_t srcIndex,
                          UBool flush,
                          UErrorCode *pErrorCode) {
    const uint8_t *result;
    int32_t resultLength, match;

    /* try to match */
    match=ucnv_extMatchFromU(cx, cp,
                             NULL, 0,
                             *src, (int32_t)(srcLimit-*src),
                             &result, &resultLength,
                             cnv->useFallback, flush);
    if(match>=2) {
        /* advance src pointer for the consumed input */
        *src+=match-2; /* remove 2 for the initial code point */

        /* write result to target */
        ucnv_extWriteFromU(cnv,
                           result, resultLength,
                           target, targetLimit,
                           offsets, srcIndex,
                           pErrorCode);
        return TRUE;
    } else if(match<0) {
        /* save state for partial match */
        const UChar *s;
        int32_t j;

        /* copy the first code point */
        cnv->preFromUFirstCP=cp;

        /* now copy the newly consumed input */
        s=*src;
        match=-match-2; /* remove 2 for the initial code point */
        for(j=0; j<match; ++j) {
            cnv->preFromU[j]=*s++;
        }
        *src=s; /* same as *src=srcLimit; because we reached the end of input */
        cnv->preFromULength=(int8_t)match;
        return TRUE;
    } else if(match==1) {
        /* matched, no mapping but request for <subchar1> */
        cnv->useSubChar1=TRUE;
        return FALSE;
    } else /* match==0 no match */ {
        return FALSE;
    }
}

U_CFUNC int32_t
ucnv_extSimpleMatchFromU(const int32_t *cx,
                         UChar32 cp, uint32_t *pValue,
                         UBool useFallback,
                         UErrorCode *pErrorCode) {
    const uint8_t *result;
    int32_t resultLength, match;

    /* try to match */
    match=ucnv_extMatchFromU(cx,
                             cp,
                             NULL, 0,
                             NULL, 0,
                             &result, &resultLength,
                             useFallback, TRUE);
    if(match>=2) {
        /* write result for simple, single-character conversion */
        if(resultLength<0) {
            resultLength=-resultLength;
            *pValue=(uint32_t)UCNV_EXT_FROM_U_GET_DATA(resultLength);
            return UCNV_EXT_FROM_U_GET_LENGTH(resultLength);
        } else if(resultLength==4) {
            /* de-serialize a 4-byte result */
            *pValue=
                ((uint32_t)result[0]<<24)|
                ((uint32_t)result[1]<<16)|
                ((uint32_t)result[2]<<8)|
                result[3];
            return 4;
        }
    }

    /*
     * return no match because
     * - match>1 && resultLength>4: result too long for simple conversion
     * - match==1: no match found, <subchar1> preferred
     * - match==0: no match found in the first place
     * - match<0: partial match, not supported for simple conversion (and flush==TRUE)
     */
    return 0;
}

/*
 * continue partial match with new input, requires cnv->preFromUFirstCP>=0
 * never called for simple, single-character conversion
 */
U_CFUNC void
ucnv_extContinueMatchFromU(UConverter *cnv,
                           UConverterFromUnicodeArgs *pArgs, int32_t srcIndex,
                           UErrorCode *pErrorCode) {
    const uint8_t *result;
    int32_t resultLength, match;

    match=ucnv_extMatchFromU(cnv->sharedData->table->mbcs.extIndexes,
                             cnv->preFromUFirstCP,
                             cnv->preFromU, cnv->preFromULength,
                             pArgs->source, (int32_t)(pArgs->sourceLimit-pArgs->source),
                             &result, &resultLength,
                             cnv->useFallback, pArgs->flush);
    if(match>=2) {
        match-=2; /* remove 2 for the initial code point */

        if(match>=cnv->preFromULength) {
            /* advance src pointer for the consumed input */
            pArgs->source+=match-cnv->preFromULength;
            cnv->preFromULength=0;
        } else {
            /* the match did not use all of preFromU[] - keep the rest for replay */
            int32_t length=cnv->preFromULength-match;
            uprv_memmove(cnv->preFromU, cnv->preFromU+match, length*U_SIZEOF_UCHAR);
            cnv->preFromULength=(int8_t)-length;
        }

        /* finish the partial match */
        cnv->preFromUFirstCP=U_SENTINEL;

        /* write result */
        ucnv_extWriteFromU(cnv,
                           result, resultLength,
                           &pArgs->target, pArgs->targetLimit,
                           &pArgs->offsets, srcIndex,
                           pErrorCode);
    } else if(match<0) {
        /* save state for partial match */
        const UChar *s;
        int32_t j;

        /* just _append_ the newly consumed input to preFromU[] */
        s=pArgs->source;
        match=-match-2; /* remove 2 for the initial code point */
        for(j=cnv->preFromULength; j<match; ++j) {
            cnv->preFromU[j]=*s++;
        }
        pArgs->source=s; /* same as *src=srcLimit; because we reached the end of input */
        cnv->preFromULength=(int8_t)match;
    } else /* match==0 or 1 */ {
        /*
         * no match
         *
         * We need to split the previous input into two parts:
         *
         * 1. The first code point is unmappable - that's how we got into
         *    trying the extension data in the first place.
         *    We need to move it from the preFromU buffer
         *    to the error buffer, set an error code,
         *    and prepare the rest of the previous input for 2.
         *
         * 2. The rest of the previous input must be converted once we
         *    come back from the callback for the first code point.
         *    At that time, we have to try again from scratch to convert
         *    these input characters.
         *    The replay will be handled by the ucnv.c conversion code.
         */

        if(match==1) {
            /* matched, no mapping but request for <subchar1> */
            cnv->useSubChar1=TRUE;
        }

        /* move the first code point to the error field */
        cnv->fromUChar32=cnv->preFromUFirstCP;
        cnv->preFromUFirstCP=U_SENTINEL;

        /* mark preFromU for replay */
        cnv->preFromULength=-cnv->preFromULength;

        /* set the error code for unassigned */
        *pErrorCode=U_INVALID_CHAR_FOUND;
    }
}

/*
 * ### TODO
 *
 * - test toU() functions
 *
 * - EBCDIC_STATEFUL: support extensions, but the charset string must be
 *   either one single-byte character or a sequence of double-byte ones,
 *   to avoid state transitions inside the mapping and to avoid having to
 *   store character boundaries.
 *   The extension functions will need an additional EBCDIC state in/out
 *   parameter and will have to be able to insert an SI or SO before writing
 *   the mapping result.
 * - EBCDIC_STATEFUL: toU() may need to check if in DB mode, do nothing if in SB
 * - EBCDIC_STATEFUL: fix prefix checking to keep SBCS & DBCS separate
 * - make dbcsonly work with extensions
 *
 * - test |2 to <subchar1> for regular code point, prefix code point,
 *   multiple code points
 * - test fallback from non-zero to 00
 * - try a smaller U_CNV_SAFECLONE_BUFFERSIZE and try ccapitst/TestConvertSafeClone()
 */

#endif /* #if !UCONFIG_NO_LEGACY_CONVERSION */
