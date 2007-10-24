/*
******************************************************************************
*
*   Copyright (C) 2000-2007, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  ucnvmbcs.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2000jul07
*   created by: Markus W. Scherer
*/

#ifndef __UCNVMBCS_H__
#define __UCNVMBCS_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "unicode/ucnv.h"
#include "ucnv_cnv.h"

/**
 * ICU conversion (.cnv) data file structure, following the usual UDataInfo
 * header.
 *
 * Format version: 6.2
 *
 * struct UConverterStaticData -- struct containing the converter name, IBM CCSID,
 *                                min/max bytes per character, etc.
 *                                see ucnv_bld.h
 *
 * --------------------
 *
 * The static data is followed by conversionType-specific data structures.
 * At the moment, there are only variations of MBCS converters. They all have
 * the same toUnicode structures, while the fromUnicode structures for SBCS
 * differ from those for other MBCS-style converters.
 *
 * _MBCSHeader.version 4.3 optionally modifies the fromUnicode data structures
 * slightly and optionally adds a table for conversion to MBCS (non-SBCS)
 * charsets.
 *
 * The modifications are to make the data utf8Friendly. Not every 4.3 file
 * file contains utf8Friendly data.
 * It is utf8Friendly if _MBCSHeader.version[2]!=0.
 * In this case, the data structures are utf8Friendly up to the code point
 *   maxFastUChar=((_MBCSHeader.version[2]<<8)|0xff)
 *
 * A utf8Friendly file has fromUnicode stage 3 entries for code points up to
 * maxFastUChar allocated in blocks of 64 for indexing with the 6 bits from
 * a UTF-8 trail byte. ASCII is allocated linearly with 128 contiguous entries.
 *
 * In addition, a utf8Friendly MBCS file contains an additional
 *   uint16_t mbcsIndex[(maxFastUChar+1)>>6];
 * which replaces the stage 1 and 2 tables for indexing with bits from the
 * UTF-8 lead byte and middle trail byte. Unlike the older MBCS stage 2 table,
 * the mbcsIndex does not contain roundtrip flags. Therefore, all fallbacks
 * from code points up to maxFastUChar (and roundtrips to 0x00) are moved to
 * the extension data structure. This also allows for faster roundtrip
 * conversion from UTF-16.
 *
 * SBCS files do not contain an additional sbcsIndex[] array because the
 * proportional size increase would be noticeable, but the runtime
 * code builds one for the code point range for which the runtime conversion
 * code is optimized.
 *
 * For SBCS, maxFastUChar should be at least U+0FFF. The initial makeconv
 * implementation sets it to U+1FFF. Because the sbcsIndex is not stored in
 * the file, a larger maxFastUChar only affects stage 3 block allocation size
 * and is free in empty blocks. (Larger blocks with sparse contents cause larger
 * files.) U+1FFF includes almost all of the small scripts.
 * U+0FFF covers UTF-8 two-byte sequences and three-byte sequences starting with
 * 0xe0. This includes most scripts with legacy SBCS charsets.
 * The initial runtime implementation using 4.3 files only builds an sbcsIndex
 * for code points up to U+0FFF.
 *
 * For MBCS, maxFastUChar should be at least U+D7FF (=initial value).
 * This boundary is convenient because practically all of the commonly used
 * characters are below it, and because it is the boundary to surrogate
 * code points, above which special handling is necessary anyway.
 * (Surrogate pair assembly for UTF-16, validity checking for UTF-8.)
 *
 * maxFastUChar could be up to U+FFFF to cover the whole BMP, which could be
 * useful especially for conversion from UTF-8 when the input can be assumed
 * to be valid, because the surrogate range would then not have to be
 * checked.
 * (With maxFastUChar=0xffff, makeconv would have to check for mbcsIndex value
 * overflow because with the all-unassigned block 0 and nearly full mappings
 * from the BMP it is theoretically possible that an index into stage 3
 * exceeds 16 bits.)
 *
 * _MBCSHeader.version 4.2 adds an optional conversion extension data structure.
 * If it is present, then an ICU version reading header versions 4.0 or 4.1
 * will be able to use the base table and ignore the extension.
 *
 * The unicodeMask in the static data is part of the base table data structure.
 * Especially, the UCNV_HAS_SUPPLEMENTARY flag determines the length of the
 * fromUnicode stage 1 array.
 * The static data unicodeMask refers only to the base table's properties if
 * a base table is included.
 * In an extension-only file, the static data unicodeMask is 0.
 * The extension data indexes have a separate field with the unicodeMask flags.
 *
 * MBCS-style data structure following the static data.
 * Offsets are counted in bytes from the beginning of the MBCS header structure.
 * Details about usage in comments in ucnvmbcs.c.
 *
 * struct _MBCSHeader (see the definition in this header file below)
 * contains 32-bit fields as follows:
 * 8 values:
 *  0   uint8_t[4]  MBCS version in UVersionInfo format (currently 4.3.x.0)
 *  1   uint32_t    countStates
 *  2   uint32_t    countToUFallbacks
 *  3   uint32_t    offsetToUCodeUnits
 *  4   uint32_t    offsetFromUTable
 *  5   uint32_t    offsetFromUBytes
 *  6   uint32_t    flags, bits:
 *                      31.. 8 offsetExtension -- _MBCSHeader.version 4.2 (ICU 2.8) and higher
 *                                                0 for older versions and if
 *                                                there is not extension structure
 *                       7.. 0 outputType
 *  7   uint32_t    fromUBytesLength -- _MBCSHeader.version 4.1 (ICU 2.4) and higher
 *                  counts bytes in fromUBytes[]
 *
 * if(outputType==MBCS_OUTPUT_EXT_ONLY) {
 *     -- base table name for extension-only table
 *     char baseTableName[variable]; -- with NUL plus padding for 4-alignment
 *
 *     -- all _MBCSHeader fields except for version and flags are 0
 * } else {
 *     -- normal base table with optional extension
 *
 *     int32_t stateTable[countStates][256];
 *    
 *     struct _MBCSToUFallback { (fallbacks are sorted by offset)
 *         uint32_t offset;
 *         UChar32 codePoint;
 *     } toUFallbacks[countToUFallbacks];
 *    
 *     uint16_t unicodeCodeUnits[(offsetFromUTable-offsetToUCodeUnits)/2];
 *                  (padded to an even number of units)
 *    
 *     -- stage 1 tables
 *     if(staticData.unicodeMask&UCNV_HAS_SUPPLEMENTARY) {
 *         -- stage 1 table for all of Unicode
 *         uint16_t fromUTable[0x440]; (32-bit-aligned)
 *     } else {
 *         -- BMP-only tables have a smaller stage 1 table
 *         uint16_t fromUTable[0x40]; (32-bit-aligned)
 *     }
 *    
 *     -- stage 2 tables
 *        length determined by top of stage 1 and bottom of stage 3 tables
 *     if(outputType==MBCS_OUTPUT_1) {
 *         -- SBCS: pure indexes
 *         uint16_t stage 2 indexes[?];
 *     } else {
 *         -- DBCS, MBCS, EBCDIC_STATEFUL, ...: roundtrip flags and indexes
 *         uint32_t stage 2 flags and indexes[?];
 *     }
 *    
 *     -- stage 3 tables with byte results
 *     if(outputType==MBCS_OUTPUT_1) {
 *         -- SBCS: each 16-bit result contains flags and the result byte, see ucnvmbcs.c
 *         uint16_t fromUBytes[fromUBytesLength/2];
 *     } else {
 *         -- DBCS, MBCS, EBCDIC_STATEFUL, ... 2/3/4 bytes result, see ucnvmbcs.c
 *         uint8_t fromUBytes[fromUBytesLength]; or
 *         uint16_t fromUBytes[fromUBytesLength/2]; or
 *         uint32_t fromUBytes[fromUBytesLength/4];
 *     }
 *
 *     -- optional utf8Friendly mbcsIndex -- _MBCSHeader.version 4.3 (ICU 3.8) and higher
 *     if(outputType!=MBCS_OUTPUT_1 &&
 *        _MBCSHeader.version[1]>=3 &&
 *        (maxFastUChar=_MBCSHeader.version[2])!=0
 *     ) {
 *         maxFastUChar=(maxFastUChar<<8)|0xff;
 *         uint16_t mbcsIndex[(maxFastUChar+1)>>6];
 *     }
 * }
 *
 * -- extension table, details see ucnv_ext.h
 * int32_t indexes[>=32]; ...
 */

/* MBCS converter data and state -------------------------------------------- */

enum {
    MBCS_MAX_STATE_COUNT=128
};

/**
 * MBCS action codes for conversions to Unicode.
 * These values are in bits 23..20 of the state table entries.
 */
enum {
    MBCS_STATE_VALID_DIRECT_16,
    MBCS_STATE_VALID_DIRECT_20,

    MBCS_STATE_FALLBACK_DIRECT_16,
    MBCS_STATE_FALLBACK_DIRECT_20,

    MBCS_STATE_VALID_16,
    MBCS_STATE_VALID_16_PAIR,

    MBCS_STATE_UNASSIGNED,
    MBCS_STATE_ILLEGAL,

    MBCS_STATE_CHANGE_ONLY
};

/* Macros for state table entries */
#define MBCS_ENTRY_TRANSITION(state, offset) (int32_t)(((int32_t)(state)<<24L)|(offset))
#define MBCS_ENTRY_TRANSITION_SET_OFFSET(entry, offset) (int32_t)(((entry)&0xff000000)|(offset))
#define MBCS_ENTRY_TRANSITION_ADD_OFFSET(entry, offset) (int32_t)((entry)+(offset))

#define MBCS_ENTRY_FINAL(state, action, value) (int32_t)(0x80000000|((int32_t)(state)<<24L)|((action)<<20L)|(value))
#define MBCS_ENTRY_SET_FINAL(entry) (int32_t)((entry)|0x80000000)
#define MBCS_ENTRY_FINAL_SET_ACTION(entry, action) (int32_t)(((entry)&0xff0fffff)|((int32_t)(action)<<20L))
#define MBCS_ENTRY_FINAL_SET_VALUE(entry, value) (int32_t)(((entry)&0xfff00000)|(value))
#define MBCS_ENTRY_FINAL_SET_ACTION_VALUE(entry, action, value) (int32_t)(((entry)&0xff000000)|((int32_t)(action)<<20L)|(value))

#define MBCS_ENTRY_SET_STATE(entry, state) (int32_t)(((entry)&0x80ffffff)|((int32_t)(state)<<24L))

#define MBCS_ENTRY_STATE(entry) (((entry)>>24)&0x7f)

#define MBCS_ENTRY_IS_TRANSITION(entry) ((entry)>=0)
#define MBCS_ENTRY_IS_FINAL(entry) ((entry)<0)

#define MBCS_ENTRY_TRANSITION_STATE(entry) ((entry)>>24)
#define MBCS_ENTRY_TRANSITION_OFFSET(entry) ((entry)&0xffffff)

#define MBCS_ENTRY_FINAL_STATE(entry) (((entry)>>24)&0x7f)
#define MBCS_ENTRY_FINAL_IS_VALID_DIRECT_16(entry) ((entry)<(int32_t)0x80100000)
#define MBCS_ENTRY_FINAL_ACTION(entry) (((entry)>>20)&0xf)
#define MBCS_ENTRY_FINAL_VALUE(entry) ((entry)&0xfffff)
#define MBCS_ENTRY_FINAL_VALUE_16(entry) (uint16_t)(entry)

#define IS_ASCII_ROUNDTRIP(b, asciiRoundtrips) (((asciiRoundtrips) & (1<<((b)>>2)))!=0)

/* single-byte fromUnicode: get the 16-bit result word */
#define MBCS_SINGLE_RESULT_FROM_U(table, results, c) (results)[ (table)[ (table)[(c)>>10] +(((c)>>4)&0x3f) ] +((c)&0xf) ]

/* single-byte fromUnicode using the sbcsIndex */
#define SBCS_RESULT_FROM_LOW_BMP(table, results, c) (results)[ (table)[(c)>>6] +((c)&0x3f) ]

/* single-byte fromUTF8 using the sbcsIndex; l and t must be masked externally; can be l=0 and t<=0x7f */
#define SBCS_RESULT_FROM_UTF8(table, results, l, t) (results)[ (table)[l] +(t) ]

/* multi-byte fromUnicode: get the 32-bit stage 2 entry */
#define MBCS_STAGE_2_FROM_U(table, c) ((const uint32_t *)(table))[ (table)[(c)>>10] +(((c)>>4)&0x3f) ]
#define MBCS_FROM_U_IS_ROUNDTRIP(stage2Entry, c) ( ((stage2Entry) & ((uint32_t)1<< (16+((c)&0xf)) )) !=0)

#define MBCS_VALUE_2_FROM_STAGE_2(bytes, stage2Entry, c) ((uint16_t *)(bytes))[16*(uint32_t)(uint16_t)(stage2Entry)+((c)&0xf)]
#define MBCS_VALUE_4_FROM_STAGE_2(bytes, stage2Entry, c) ((uint32_t *)(bytes))[16*(uint32_t)(uint16_t)(stage2Entry)+((c)&0xf)]

#define MBCS_POINTER_3_FROM_STAGE_2(bytes, stage2Entry, c) ((bytes)+(16*(uint32_t)(uint16_t)(stage2Entry)+((c)&0xf))*3)

/* double-byte fromUnicode using the mbcsIndex */
#define DBCS_RESULT_FROM_MOST_BMP(table, results, c) (results)[ (table)[(c)>>6] +((c)&0x3f) ]

/* double-byte fromUTF8 using the mbcsIndex; l and t1 combined into lt1; lt1 and t2 must be masked externally */
#define DBCS_RESULT_FROM_UTF8(table, results, lt1, t2) (results)[ (table)[lt1] +(t2) ]


/**
 * MBCS output types for conversions from Unicode.
 * These per-converter types determine the storage method in stage 3 of the lookup table,
 * mostly how many bytes are stored per entry.
 */
enum {
    MBCS_OUTPUT_1,          /* 0 */
    MBCS_OUTPUT_2,          /* 1 */
    MBCS_OUTPUT_3,          /* 2 */
    MBCS_OUTPUT_4,          /* 3 */

    MBCS_OUTPUT_3_EUC=8,    /* 8 */
    MBCS_OUTPUT_4_EUC,      /* 9 */

    MBCS_OUTPUT_2_SISO=12,  /* c */
    MBCS_OUTPUT_2_HZ,       /* d */

    MBCS_OUTPUT_EXT_ONLY,   /* e */

    MBCS_OUTPUT_COUNT,

    MBCS_OUTPUT_DBCS_ONLY=0xdb  /* runtime-only type for DBCS-only handling of SISO tables */
};

/**
 * Fallbacks to Unicode are stored outside the normal state table and code point structures
 * in a vector of items of this type. They are sorted by offset.
 */
typedef struct {
    uint32_t offset;
    UChar32 codePoint;
} _MBCSToUFallback;

/** Constants for fast and UTF-8-friendly conversion. */
enum {
    SBCS_FAST_MAX=0x0fff,               /* maximum code point with UTF-8-friendly SBCS runtime code, see makeconv SBCS_UTF8_MAX */
    SBCS_FAST_LIMIT=SBCS_FAST_MAX+1,    /* =0x1000 */
    MBCS_FAST_MAX=0xd7ff,               /* maximum code point with UTF-8-friendly MBCS runtime code, see makeconv MBCS_UTF8_MAX */
    MBCS_FAST_LIMIT=MBCS_FAST_MAX+1     /* =0xd800 */
};

/**
 * This is the MBCS part of the UConverterTable union (a runtime data structure).
 * It keeps all the per-converter data and points into the loaded mapping tables.
 *
 * utf8Friendly data structures added with _MBCSHeader.version 4.3
 */
typedef struct UConverterMBCSTable {
    /* toUnicode */
    uint8_t countStates, dbcsOnlyState, stateTableOwned;
    uint32_t countToUFallbacks;

    const int32_t (*stateTable)/*[countStates]*/[256];
    int32_t (*swapLFNLStateTable)/*[countStates]*/[256]; /* for swaplfnl */
    const uint16_t *unicodeCodeUnits/*[countUnicodeResults]*/;
    const _MBCSToUFallback *toUFallbacks;

    /* fromUnicode */
    const uint16_t *fromUnicodeTable;
    const uint16_t *mbcsIndex;              /* for fast conversion from most of BMP to MBCS (utf8Friendly data) */
    uint16_t sbcsIndex[SBCS_FAST_LIMIT>>6]; /* for fast conversion from low BMP to SBCS (utf8Friendly data) */
    const uint8_t *fromUnicodeBytes;
    uint8_t *swapLFNLFromUnicodeBytes;      /* for swaplfnl */
    uint32_t fromUBytesLength;
    uint8_t outputType, unicodeMask;
    UBool utf8Friendly;                     /* for utf8Friendly data */
    UChar maxFastUChar;                     /* for utf8Friendly data */

    /* roundtrips */
    uint32_t asciiRoundtrips;

    /* converter name for swaplfnl */
    char *swapLFNLName;

    /* extension data */
    struct UConverterSharedData *baseSharedData;
    const int32_t *extIndexes;
} UConverterMBCSTable;

/**
 * MBCS data header. See data format description above.
 */
typedef struct {
    UVersionInfo version;
    uint32_t countStates,
             countToUFallbacks,
             offsetToUCodeUnits,
             offsetFromUTable,
             offsetFromUBytes,
             flags,
             fromUBytesLength;
} _MBCSHeader;

/*
 * This is a simple version of _MBCSGetNextUChar() that is used
 * by other converter implementations.
 * It only returns an "assigned" result if it consumes the entire input.
 * It does not use state from the converter, nor error codes.
 * It does not handle the EBCDIC swaplfnl option (set in UConverter).
 * It handles conversion extensions but not GB 18030.
 *
 * Return value:
 * U+fffe   unassigned
 * U+ffff   illegal
 * otherwise the Unicode code point
 */
U_CFUNC UChar32
ucnv_MBCSSimpleGetNextUChar(UConverterSharedData *sharedData,
                        const char *source, int32_t length,
                        UBool useFallback);

/**
 * This version of _MBCSSimpleGetNextUChar() is optimized for single-byte, single-state codepages.
 * It does not handle the EBCDIC swaplfnl option (set in UConverter).
 * It does not handle conversion extensions (_extToU()).
 */
U_CFUNC UChar32
ucnv_MBCSSingleSimpleGetNextUChar(UConverterSharedData *sharedData,
                              uint8_t b, UBool useFallback);

/**
 * This macro version of _MBCSSingleSimpleGetNextUChar() gets a code point from a byte.
 * It works for single-byte, single-state codepages that only map
 * to and from BMP code points, and it always
 * returns fallback values.
 */
#define _MBCS_SINGLE_SIMPLE_GET_NEXT_BMP(sharedData, b) \
    (UChar)MBCS_ENTRY_FINAL_VALUE_16((sharedData)->mbcs.stateTable[0][(uint8_t)(b)])

/**
 * This is an internal function that allows other converter implementations
 * to check whether a byte is a lead byte.
 */
U_CFUNC UBool
ucnv_MBCSIsLeadByte(UConverterSharedData *sharedData, char byte);

/** This is a macro version of _MBCSIsLeadByte(). */
#define _MBCS_IS_LEAD_BYTE(sharedData, byte) \
    (UBool)MBCS_ENTRY_IS_TRANSITION((sharedData)->mbcs.stateTable[0][(uint8_t)(byte)])

/*
 * This is another simple conversion function for internal use by other
 * conversion implementations.
 * It does not use the converter state nor call callbacks.
 * It does not handle the EBCDIC swaplfnl option (set in UConverter).
 * It handles conversion extensions but not GB 18030.
 *
 * It converts one single Unicode code point into codepage bytes, encoded
 * as one 32-bit value. The function returns the number of bytes in *pValue:
 * 1..4 the number of bytes in *pValue
 * 0    unassigned (*pValue undefined)
 * -1   illegal (currently not used, *pValue undefined)
 *
 * *pValue will contain the resulting bytes with the last byte in bits 7..0,
 * the second to last byte in bits 15..8, etc.
 * Currently, the function assumes but does not check that 0<=c<=0x10ffff.
 */
U_CFUNC int32_t
ucnv_MBCSFromUChar32(UConverterSharedData *sharedData,
                 UChar32 c, uint32_t *pValue,
                 UBool useFallback);

/**
 * This version of _MBCSFromUChar32() is optimized for single-byte codepages.
 * It does not handle the EBCDIC swaplfnl option (set in UConverter).
 *
 * It returns the codepage byte for the code point, or -1 if it is unassigned.
 */
U_CFUNC int32_t
ucnv_MBCSSingleFromUChar32(UConverterSharedData *sharedData,
                       UChar32 c,
                       UBool useFallback);

/**
 * SBCS, DBCS, and EBCDIC_STATEFUL are replaced by MBCS, but
 * we cheat a little about the type, returning the old types if appropriate.
 */
U_CFUNC UConverterType
ucnv_MBCSGetType(const UConverter* converter);

U_CFUNC void 
ucnv_MBCSFromUnicodeWithOffsets(UConverterFromUnicodeArgs *pArgs,
                            UErrorCode *pErrorCode);
U_CFUNC void 
ucnv_MBCSToUnicodeWithOffsets(UConverterToUnicodeArgs *pArgs,
                          UErrorCode *pErrorCode);

#if 0  /* Replaced by ucnv_MBCSGetFilteredUnicodeSetForUnicode() until we implement ucnv_getUnicodeSet() with reverse fallbacks. */
/*
 * Internal function returning a UnicodeSet for toUnicode() conversion.
 * Currently only used for ISO-2022-CN, and only handles roundtrip mappings.
 * In the future, if we add support for reverse-fallback sets, this function
 * needs to be updated, and called for each initial state.
 * Does not currently handle extensions.
 * Does not empty the set first.
 */
U_CFUNC void
ucnv_MBCSGetUnicodeSetForBytes(const UConverterSharedData *sharedData,
                           const USetAdder *sa,
                           UConverterUnicodeSet which,
                           uint8_t state, int32_t lowByte, int32_t highByte,
                           UErrorCode *pErrorCode);
#endif

/*
 * Internal function returning a UnicodeSet for toUnicode() conversion.
 * Currently only used for ISO-2022-CN, and only handles roundtrip mappings.
 * In the future, if we add support for fallback sets, this function
 * needs to be updated.
 * Handles extensions.
 * Does not empty the set first.
 */
U_CFUNC void
ucnv_MBCSGetUnicodeSetForUnicode(const UConverterSharedData *sharedData,
                                 const USetAdder *sa,
                                 UConverterUnicodeSet which,
                                 UErrorCode *pErrorCode);

typedef enum UConverterSetFilter {
    UCNV_SET_FILTER_NONE,
    UCNV_SET_FILTER_DBCS_ONLY,
    UCNV_SET_FILTER_2022_CN,
    UCNV_SET_FILTER_SJIS,
    UCNV_SET_FILTER_GR94DBCS,
    UCNV_SET_FILTER_COUNT
} UConverterSetFilter;

/*
 * Same as ucnv_MBCSGetUnicodeSetForUnicode() but
 * the set can be filtered by encoding scheme.
 * Used by stateful converters which share regular conversion tables
 * but only use a subset of their mappings.
 */
U_CFUNC void
ucnv_MBCSGetFilteredUnicodeSetForUnicode(const UConverterSharedData *sharedData,
                                         const USetAdder *sa,
                                         UConverterUnicodeSet which,
                                         UConverterSetFilter filter,
                                         UErrorCode *pErrorCode);

#endif

#endif
