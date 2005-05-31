/*
*******************************************************************************
*
*   Copyright (C) 2004-2005, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  utext.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2004oct06
*   created by: Markus W. Scherer
*/

#ifndef __UTEXT_H__
#define __UTEXT_H__

/**
 * \file
 * \brief C API: Abstract Unicode Text API
 *
 * TBD
 *
 * Text chunks must begin and end on Unicode code point boundaries.
 * That is, a chunk boundary must not fall between a leading and a trailing
 * surrogate.
 *
 * If an input index points not at a code point boundary, then the API behaves as if
 * the index is first adjusted to the immediately preceding code point boundary.
 *
 * Valid indexes into the text must begin at 0 (start of the text) and
 * must strictly increase going forward through the text.
 * (No reordering, and valid indexes must be non-negative.)
 *
 * Issues:
 * - Code point boundary adjustment if index points not already to a boundary
 *   + Currently, see above: always adjusts to the immediately preceding code point boundary.
 *     (For example, using U8_SET_CP_START().)
 *   + Alternatively: Could adjust to preceding boundary when going forward (U8_SET_CP_START())
 *     and to the following boundary when going backward (U8_SET_CP_LIMIT()).
 *     Result: next32From(index) and previous32From(index) would return the same
 *     character.
 * - Error handling - add UErrorCode parameters? Add UBool return values to void functions?
 *   + Add UErrorCode to extract()?
 * - This version does not expose NUL-termination to the caller.
 * - This version assumes option 2 (index mapping done by provider functions).
 * - This version uses one API for read-only as well as read-write access,
 *   with a way to find out whether the text object is writable or not.
 * - This version does not support absolute UTF-16 indexes when native indexes are used.
 * - Should the copy() function have a UBool for whether to copy or move the text?
 * - replace() needs a way to indicate that the current chunk
 *   (which would need to be passed in) became invalid during the operation.
 *   Same for copy().
 *
 * - Single text iterator only.  Because UText owns the buffer for non-UTF-16 sources, there
 *   can only be a single UTextIterator on a UText.  A second iterator could cause buffer
 *   contents to be moved, while info about what's in the buffer in the first iterator would
 *   not be updated.  
 *
 *     - Add some kind of failure status to construction of UTextIterator, to prevent
 *       two from existing?  Seems hostile to developers.
 *     - Make UText.clone() be shallow.  Don't clone the text, do clone the buffer so that
 *       a second UTextIterator can be instantiated.   Developer hostile, again.
 *     - Move the buffer from the UText to the UTextIterator.
 *         - developer-friendly.
 *         - NOP (good performance) on utf-16  strings.  (all can share a buffer)
 *         - Extra allocation for buffer on utf-8, codepage data, etc.
 *     - Use buffer in UText if available, otherwise allocate another
 *         Complicated implementation.
 *         Threading model?
 *       
 *
 * @see UText
 */

#include "unicode/utypes.h"
#include "unicode/rep.h"
#include "unicode/unistr.h"

#ifndef U_HIDE_DRAFT_API

U_CDECL_BEGIN

struct UText;
typedef struct UText UText; /**< C typedef for struct UText. @draft ICU 3.4 */

struct UTextChunk;
typedef struct UTextChunk UTextChunk; /**< C typedef for struct UTextChunk. @draft ICU 3.4 */



/***************************************************************************************
 *
 *   C Functions for creating UText wrappers around various kinds of text strings.
 *
 ****************************************************************************************/


/**
  * utext_close    Close function for UText instances.
  *                Cleans up, releases any resources being held by an
  *                open UText instance.
  * <p/>
  *     If the UText was originally allocated by one of the utext_open functions,
  *     the storage associated with the utext will also be deleted.
  *     If the UText storage originated with the application, as it would with
  *     a local or static instance, the storage will not be deleted.
  * 
  * @param ut  The UText instance object to be closed.
  */
U_DRAFT void U_EXPORT2
utext_close(UText *ut);

/**
 * Open a read-only UText implementation for UTF-8 strings.
 * 
 * @param t      Pointer to a UText struct.  If NULL, a new UText will be created.
 *               If non-NULL, must refer to an already existing UText, which will then
 *               be reset to reference the specified UTF-8 string.
 * @param s      A utf-8 string
 *               TODO:  does this want to be (uint8_t *) or (char *)?
 * @param length The length of the utf-8 string in bytes, or -1 if the string is
 *               zero terminated.
 * @param pErrorCode Errors are returned here.
 */
U_DRAFT UText * U_EXPORT2
utext_openUTF8(UText *t, const uint8_t *s, int32_t length, UErrorCode *pErrorCode);

/**
 * Open a read-only UText implementation for SBCS strings.
 * The implementation converts 1:1 according to the provided mapping table.
 * Supplementary code points are not supported.
 *
 * @param t      Pointer to a UText struct.  If NULL, a new UText will be created.
 *               If non-NULL, must refer to an already existing UText, which will then
 *               be reset to reference the specified input string.
 * @param toU    Mapping table for conversion from SBCS to Unicode (BMP only).
 *               The mapping table must be available during the lifetime of the
 *               UText object.
 * @param s      A byte text string
 * @param length The length of the input string in bytes, or -1 if the string is
 *               zero terminated.
 * @param pErrorCode Errors are returned here.
 */
U_DRAFT UText * U_EXPORT2
utext_openSBCS(UText *t,
               const UChar toU[256],
               const char *s, int32_t length,
               UErrorCode *pErrorCode);



/**
 * Open a  UText object for a UnicodeString.
 * 
 * @param t      Pointer to a UText struct.  If NULL, a new UText will be created.
 *               If non-NULL, must refer to an initialized UText, which will then
 *               be reset to reference the specified UTF-8 string.
 * @param s      A UnicodeString
 * @param pErrorCode Errors are returned here.
 * @return       Pointer to the UText.  If a UText was supplied as input, this
 *               will always be returned.
 */
U_DRAFT UText * U_EXPORT2
utext_openUnicodeString(UText *t, UnicodeString *s, UErrorCode *pErrorCode);



/**
 * Open a writable UText implementation for Replaceable objects.
 * @param t      Pointer to a UText struct.  If NULL, a new UText will be created.
 *               If non-NULL, must refer to an already existing UText, which will then
 *               be reset to reference the specified UTF-8 string.
 * @param rep    A Replaceable text object.
 * @param pErrorCode Errors are returned here.
 */
U_DRAFT UText * U_EXPORT2
utext_openReplaceable(UText *t, Replaceable *rep, UErrorCode *pErrorCode);


struct UTextChunk {
    /** Pointer to contents of text chunk. */
    const UChar *contents;
    /** Number of UChars in the chunk. */
    int32_t length;
    /** (Native) text index corresponding to the start of the chunk. */
    int32_t start;
    /** (Native) text index corresponding to the end of the chunk (contents+length). */
    int32_t limit;
    /** If TRUE, then non-UTF-16 indexes are used in this chunk. */
    UBool nonUTF16Indexes;
    /** Unused. */
    UBool padding;
    /** Contains sizeof(UTextChunk) and allows the future addition of fields. */
    uint16_t sizeOfStruct;
};


/**
 * UText provider properties (bit field indexes).
 *
 * @see UText
 * @draft ICU 3.4
 */
enum {
    /**
     * The provider works with non-UTF-16 ("native") text indexes.
     * For example, byte indexes into UTF-8 text or UTF-32 indexes into UTF-32 text.
     * @draft ICU 3.4
     */
    UTEXT_PROVIDER_NON_UTF16_INDEXES,
    /**
     * The provider can return the text length inexpensively.
     * @draft ICU 3.4
     */
    UTEXT_PROVIDER_LENGTH_IS_INEXPENSIVE,
    /**
     * Text chunks remain valid and usable until the text object is modified or
     * deleted, not just until the next time the access() function is called
     * (which is the default).
     * @draft ICU 3.4
     */
    UTEXT_PROVIDER_STABLE_CHUNKS,
    /**
     * The provider supports modifying the text via the replace() and copy()
     * functions.
     * @see Replaceable
     * @draft ICU 3.4
     */
    UTEXT_PROVIDER_WRITABLE,
    /**
     * There is meta data associated with the text.
     * @see Replaceable::hasMetaData()
     * @draft ICU 3.4
     */
    UTEXT_PROVIDER_HAS_META_DATA
};

/**
 * Function type declaration for UText.clone().
 *
 * clone this UText.  
 * Text providers are not required to support clone.
 * Applications must be prepared for the possibility that clone is not supported.
 *
 * This is a shallow clone.  The underlying text is not copied, only the
 * UText wrapper to the text is cloned.  A cloned UText allows having multiple
 * UTextIteartors active over the same underlying text.  (Any single instance
 * of UText can only have one iterator active.)
 *
 * TODO:  should we just drop clone altogether?
 *
 * @return a pointer to the newly created copy of the UTex object.
 *         May return NULL if the object cannot be cloned.
 *
 * @see UText
 * @draft ICU 3.4
 */
typedef UText * U_CALLCONV
UTextClone(const UText *t);

/**
 * Function type declaration for UText.GetProperties().
 *
 * Gets the provider properties for this UText.
 *
 * @return Provider properties bit field.
 *
 * @see UText
 * @draft ICU 3.4
 */
typedef int32_t U_CALLCONV
UTextGetProperties(UText *t);

/**
 * Function type declaration for UText.length().
 *
 * TBD
 *
 * @see UText
 * @draft ICU 3.4
 */
typedef int32_t U_CALLCONV
UTextLength(UText *t);

/**
 * Function type declaration for UText.access().
 *
 * @param index Requested (native) index.
 * @param forward If TRUE, then the returned chunk must contain text
 *        starting from the index, so that start<=index<limit.
 *        If FALSE, then the returned chunk must contain text
 *        before the index, so that start<index<=limit.
 * @return Chunk-relative UTF-16 offset corresponding to the requested index.
 *         Negative value if a chunk cannot be accessed
 *         (the requested index is out of bounds).
 *
 * @see UText
 * @draft ICU 3.4
 */
typedef int32_t U_CALLCONV
UTextAccess(UText *t, int32_t index, UBool forward, UTextChunk *chunk);

/**
 * Function type declaration for UText.extract().
 *
 * Extract text from a UText into a UChar buffer.  The range of text to be extracted
 * is specified in the native indices of the UText provider.  These may not necessarily
 * be utf-16 indices.
 * <p/>
 * The size (number of 16 bit UChars) in the data to be extracted is returned.  The
 * full amount is returned, even when the specified buffer size is smaller.
 *
 * The extracted string will (if you are a user) / must (if you are a text provider)
 * be NUL-terminated if there is sufficient space in the destination buffer.
 *
 * @param  ut    the UText from which to extract data.
 * @param  start the native index of the first characer to extract.
 * @param  limit the native string index of the position following the last
 *               character to extract.
 * @param  dest  the UChar (utf-16) buffer into which the extracted text is placed
 * @param  destCapacity  The size, in UChars, of the destination buffer.  May be zero
 *               for precomputing the required size.
 * @param  status receives any error status.
 *         If U_BUFFER_OVERFLOW_ERROR: Returns number of UChars for
 *         preflighting.
 * @return Number of UChars in the data.  Does not include a trailing NUL.
 *
 *  TODO: how should invalid source data be handled?  Corrupt utf-8, for example.
 *
 * @see UText
 * @draft ICU 3.4
 */
typedef int32_t U_CALLCONV
UTextExtract(UText *ut,
             int32_t start, int32_t limit,
             UChar *dest, int32_t destCapacity,
             UErrorCode *status);

/**
 * Function type declaration for UText.replace().
 *
 * TBD
 *
 * If chunk is not NULL and the chunk contents outside of start..limit is
 * modified, other than moving text after limit,
 * then the chunk->contents pointer is set to NULL.
 *
 * @return Delta between the limit of the replacement text and the limit argument,
 *         that is, the signed number of (native) storage units by which
 *         the old and the new pieces of text differ.
 *
 * @see UText
 * @draft ICU 3.4
 */
typedef int32_t U_CALLCONV
UTextReplace(UText *t,
             int32_t start, int32_t limit,
             const UChar *src, int32_t length,
             UTextChunk *chunk,
             UErrorCode *pErrorCode);

/**
 * Function type declaration for UText.copy().
 *
 * Copies a substring of this object, retaining metadata.
 * This method is used to duplicate or reorder substrings.
 * The destination index must not overlap the source range.
 *
 * TBD
 *
 * If chunk is not NULL and the chunk contents outside of start..limit is
 * modified, other than moving text after limit,
 * then the chunk->contents pointer is set to NULL.
 *
 * @param move If TRUE, then the substring is moved, not copied/duplicated.
 *
 * @see UText
 * @draft ICU 3.4
 */
typedef void U_CALLCONV
UTextCopy(UText *t,
          int32_t start, int32_t limit,
          int32_t destIndex,
          UBool move,
          UTextChunk *chunk,
          UErrorCode *pErrorCode);

/**
 * Function type declaration for UText.mapOffsetToNative().
 *
 * TBD
 *
 * @param offset UTF-16 offset relative to the current text chunk,
 *               0<=offset<=chunk->length.
 * @return Absolute (native) index corresponding to the UTF-16 offset
 *         relative to the current text chunk.
 *
 * @see UText
 * @draft ICU 3.4
 */
typedef int32_t U_CALLCONV
UTextMapOffsetToNative(UText *t, UTextChunk *chunk, int32_t offset);

/**
 * Function type declaration for UText.mapIndexToUTF16().
 *
 * TBD
 *
 * @param index Absolute (native) text index, chunk->start<=index<=chunk->limit.
 * @return Chunk-relative UTF-16 offset corresponding to the absolute (native)
 *         index.
 *
 * @see UText
 * @draft ICU 3.4
 */
typedef int32_t U_CALLCONV
UTextMapIndexToUTF16(UText *t, UTextChunk *chunk, int32_t index);


/**
 * Function type declaration for UText.utextClose().
 *
 * TBD
 *
 * @param t A UText object to be closed.
 *
 * @see UText
 * @draft ICU 3.4
 */
typedef void U_CALLCONV
UTextClose(UText *t);


/**
  *   UText struct.  Provides the interface between the generic UText access code
  *                  and the UText provider code that works on specific kinds of
  *                  text  (utf-8, noncontiugous utf-16, whatever.)
  *
  *                  This needs to be plain C, not C++, for reasons of release-to-
  *                  release binary compatibility.  An application may create a
  *                  provider for it's own unique text format, and that application 
  *                  binary must continue to work with future versions of ICU.
  *
  *                  Applications that are using predefined types of text providers
  *                  to pass text data to ICU services will have no need to view the
  *                  internals of the UText structs that they open.
  */
struct UText {
    /**
     * (protected) Pointer to string or wrapped object or similar.
     * Not used by caller.
     * @draft ICU 3.4
     */
    const void *context;

    /**
     * (protected) Pointer fields for use by text provider.
     * Not used by caller.
     * @draft ICU 3.4
     */
    const void *p, *q, *r;

    /**
     *  (protected)  Pointer to additional space requested by the
     *               provider during the utext_open operation.
     */
    void     *pExtra;

    /**
     *   (protected)  Size in bytes of the extra space (pExtra).
     */
    int32_t   extraSize;

    /**
     *     (private)  Flags for managing the allocation and freeing of
     *                memory associated with this UText.
     */
    int32_t   flags;

    /**
     *     (private)  Magic.  Try to detect when we are handed junk.
     */
    int32_t   magic;


    /**
     * (public) sizeOfStruct=sizeof(UText)
     * Allows possible backward compatible extension.
     *
     * @draft ICU 3.4
     */
    int32_t sizeOfStruct;

    /**
     * (protected) Integer fields for use by text provider.
     * Not used by caller.
     * @draft ICU 3.4
     */
    int32_t a, b, c;

    /**
     * (public) TBD
     *
     * @see UTextClone
     * @draft ICU 3.4
     */
    UTextClone *clone;

    /**
     * (public) TBD
     *
     * @see UTextGetProperties
     * @draft ICU 3.4
     */
    UTextGetProperties *properties;

    /**
     * (public) Returns the length of the text.
     * May be expensive to compute!
     *
     * @see UTextLength
     * @draft ICU 3.4
     */
    UTextLength *length;

    /**
     * (public) Access to a chunk of text.
     * Does not copy text but instead gives access to a portion of it.
     *
     * The intention is that for discontiguous storage the chunk would be an actual
     * storage block used for storing the text.
     * For contiguously stored text with known length, the whole text would be returned.
     * For NUL-terminated text, the implementation may scan forward in exponentially
     * larger chunks instead of finding the NUL right away.
     *
     * In: Text index; the returned chunk of text must contain the index.
     * Out:
     * - Pointer to chunk start
     * - Start and limit indexes corresponding to the chunk;
     *   it must be start<=input index<limit
     * - Indication of success: If the input index is negative or >=length then
     *   failure needs to be indicated, probably by returning a NULL pointer
     *
     * @see UTextAccess
     * @draft ICU 3.4
     */
    UTextAccess *access;

    /**
     * (public) Copy a chunk of text into a buffer.
     * Does it need a return value indicating success/failure?
     * The signature shown here is the same as in UReplaceable.
     * Not strictly minimally necessary; Replaceable has it.
     *
     * @see UTextExtract
     * @draft ICU 3.4
     */
    UTextExtract *extract;

    /**
     * (public) TBD
     *
     * @see UTextReplace
     * @draft ICU 3.4
     */
    UTextReplace *replace;

    /**
     * (public) TBD
     *
     * @see UTextCopy
     * @draft ICU 3.4
     */
    UTextCopy *copy;

    /**
     * (public) TBD
     *
     * @see UTextMapOffsetToNative
     * @draft ICU 3.4
     */
    UTextMapOffsetToNative *mapOffsetToNative;

    /**
     * (public) TBD
     *
     * @see UTextMapIndexToUTF16
     * @draft ICU 3.4
     */
    UTextMapIndexToUTF16 *mapIndexToUTF16;

    /**
      * (public)
      *
      * @see UTextClose
      * @draft ICU 3.4
      */
    UTextClose  *close;
};


/**
  *  Function for use by Text Provider implementations to allocate and/or initialize
  *  a new UText struct.  To be called in the implementation of utext_open() functions.
  *  If the suppliec utxt parameter is null, a new UText struct will be allocated on the heap.
  *  If the supplied UText is already open, the provider's clsoe function will be called
  *  so that the struct can be reused by the open that is in progress.
  *
  * @param utxt pointer to a UText struct to be re-used, or null if a new UText
  *             should be allocated.
  * @param extraSpace The amount of additional space to be allocated as part
  *             of this UText, for use by types of providers that require
  *             additional storage.
  */
U_DRAFT UText * U_EXPORT2
UTextSetup(UText *utxt, int32_t extraSpace, UErrorCode *status);

/**
  * @internal
  */
enum {
    UTEXT_MAGIC = 0xe45ad82c
};


/**
 * @internal
 */
#define UTEXT_INITIALZIER_HEAD  \
                  NULL,                 /* context      */ \
                  NULL, NULL, NULL,     /* p, q, r      */ \
                  NULL,                 /* pExtra       */ \
                  0,                    /* extraSize    */ \
                  0,                    /* flags        */ \
                  UTEXT_MAGIC,          /* magic        */ \
                  sizeof(UText),        /* sizeOfStruct */ \
                  0, 0, 0        


/**
 * initializer to be used with local (stack) instances of a UText
 *  struct.  UText structs must be initialized before passing
 *  them to one of the utext_open functions.
 *
 * @draft ICU 3.4
 */
#define UTEXT_INITIALIZER {                                \
                  UTEXT_INITIALZIER_HEAD,                  \
                  NULL,                 /* clone ()     */ \
                  NULL,                 /* properties ()*/ \
                  NULL,                 /* length ()    */ \
                  NULL,                 /* access ()    */ \
                  NULL,                 /* extract ()   */ \
                  NULL,                 /* replace ()   */ \
                  NULL,                 /* copy ()      */ \
                  NULL, NULL,           /* map * 2 ()   */ \
                  NULL                  /* close ()     */ \
};


U_CDECL_END




#ifdef XP_CPLUSPLUS
U_NAMESPACE_BEGIN


/**
 *
 *  UTextIterator    is the class used to access the text data that is
 *                   behind a UText object.  Services that receive text in the
 *                   form of a UText will instanitate a UTextIterator
 *                   for this purpose.
 *
 *                   @draft ICU 3.4
 */
class U_COMMON_API UTextIterator : public UMemory {
public:
    // all-inline, and stack-allocatable
    // constructors, get/set UText, etc.
    // needs to have state besides the current chunk: at least the current index
    // for performance, may use a current-position pointer and chunk start/limit
    // pointers and translate back into indexes only when necessary

    UTextIterator(UText *t);

    /**
     * Returns the code point at the requested index,
     * or U_SENTINEL (-1) if it is out of bounds.
     */
    // see next32From() -- inline UChar32 char32At(int32_t index);

    // U_SENTINEL (-1) if out of bounds
    inline UChar32 next32();

    /**
     *  Move the iterator position to the character (code point) whose
     *  index precedes the current position, and return that character.
     *  This is a pre-decrement operation.
     *  If the initial iterator position is in the interior of a multi-unit
     *  character (a utf-8 sequence or a utf-16 surrogate pair, for example),
     *  the position will be backed up to the beginning of that character,
     *  and that character will be returned.
     *
     *  @return the previous UChar32 code point, or U_SENTINEL (-1) 
     *          if the index is out of bounds
     *  @draft ICU 3.4
     */
    inline UChar32 previous32();

    /**
     * Set the iteration index for a following next32() or previous32().
     * Does not immediately access text from the provider.
     * If the specified index is less than zero or greater than the
     * length of the text, the position will be set to zero or the length.
     * next32From(index) is more efficient than setIndex()+next32().
     * previous32From(index) is more efficient than setIndex()+previous32().
     *
     * <p>
     * TODO:  what should happen if the new index is not on a code point boundary?
     * 
     * @param index the new index position to set the iterator to.
     *
     * @draft ICU 3.4

     */
    inline void setIndex(int32_t index);

    /**
     * Set the iteration index, access the text for forward iteration,
     * and return the code point starting at or before that index.
     *
     * @param index Iteration index.
     * @return Code point which starts at or before index,
     *         or U_SENTINEL (-1) if it is out of bounds.
     * @draft ICU 3.4
     */
    inline UChar32 next32From(int32_t index);
    /**
     * Set the iteration index, access the text for backward iteration,
     * and return the code point ending at or before that index.
     *
     * @param index Iteration index.
     * @return Code point which ends at or before index,
     *         or U_SENTINEL (-1) if it is out of bounds.
     *
     * @draft ICU 3.4
     */
    inline UChar32 previous32From(int32_t index);

    /**
     * Get the current iterator position, which can range from 0 to length.
     * The position is an index into the input text, in whatever format it
     * may have, and may not always correspond to a UChar (UTF-16) index
     * into the text.
     *
     * @return the current index position.
     * @draft ICU 3.4
     */
    inline int32_t getIndex();

    /**
     * Move the iterator postion by delta code points.  The amount to move
     * is a signed number; a negative delta will move the iterator backwards,
     * towards the start of the text.
     * <p/>
     * Behavior for out-of-bounds indexes:
     * <code>moveIndex</code> pins the input index to 0..length(), i.e.,
     * if the input index<0 then it is pinned to 0;
     * if it is index>length() then it is pinned to length().
     * Afterwards, the index is moved by <code>delta</code> code points
     * forward or backward,
     * but no further backward than to 0 and no further forward than to length().
     * The resulting index value will be in between 0 and length(), inclusive.
     *
     * @return TRUE if the position could be moved the requested number of positions without
     *              running of the start or end of the text.
     * @draft ICU 3.4
     */
    UBool moveIndex(int32_t delta);  

    /**
     * Compare the text starting from the current index with the string
     * argument. The index is modified. In case of a match (zero result),
     * the index is left exactly after the matching segment.
     * Otherwise, the index position is undefined.
     *
     * Negative/positive results mean that the text segment compares
     * lower/higher than the string. A zero result means that the text
     * segment compares equal, even if there is following text after the
     * matching segment.
     * Test for the end of the text using next32()>=0 if necessary.
     *
     * @param codePointOrder Choose between code unit order (FALSE)
     *                       and code point order (TRUE).
     *
     * @return negative/0/positive as comparison result.
     *
     *  TODO:  this function seems a little out of place in this class.
     *         Probably should be removed to some collection of TextIterator based 
     *         string utiltity functions.
     * @internal
     */
    UBool compare(const UChar *s, int32_t length, UBool codePointOrder);

    /**
     * Get the total length of the text, expressed in the units of the
     *   underlying text storage, which is not necessarily utf-16 code units.
     *   The length of a utf-8 string, for example, would be returned in bytes.
     * May be expensive to compute.
     *
     * @return the lenght of the text.
     *
     * @draft ICU 3.4
     */
    int32_t  length();

    // convenience wrappers for access(), extract()?
    // needed at least for extract()/copy() for chunk invalidation
    // getChunkStart(), getChunkLimit() for the current chunk?
    // const UTextChunk *getChunk()?

private:
    UText *t;
    UTextChunk chunk;   
    int32_t chunkOffset;                 // Current index within this chunk.
    int32_t providerProperties;          // -1 if not known yet

    void setChunkInvalid(int32_t index);

    /** Call chunkOffset=t->access() and return TRUE if a chunk is returned. */
    UBool access(int32_t index, UBool forward);

    UChar32  getSupplementary();         // Get a supplementary char at the current position.
                                         //  position could be on either lead or trail.
                                         //  Position will be adjusted to be on lead.
                                         //  Out-of-line, to reduce amount of inline code.
};


//--------------------------------------------------------------------------------
//
// UTextIterator inline implementations 
//
//--------------------------------------------------------------------------------


UChar32
UTextIterator::next32() {
    if(chunkOffset>=chunk.length && !access(chunk.limit, TRUE)) {
        // no chunk available here
        return U_SENTINEL;
    }

    UChar32 c;
    U16_NEXT(chunk.contents, chunkOffset, chunk.length, c);
    return c;
}

UChar32
UTextIterator::previous32() {
    if(chunkOffset<=0 && !access(chunk.start, FALSE)) {
        // no chunk available here
        return U_SENTINEL;
    }

    UChar32 c;
    U16_PREV(chunk.contents, 0, chunkOffset, c);
    return c;
}

void
UTextIterator::setIndex(int32_t index) {
    if(index<chunk.start || chunk.limit<index) {
        // The desired position is outside of the current chunk.  Invalidate it and
        // leave it to next32() or previous32() to access the text
        // in the desired direction.
        setChunkInvalid(index);
    } else if(chunk.nonUTF16Indexes) {
        chunkOffset=t->mapIndexToUTF16(t, &chunk, index);
    } else {
        chunkOffset=index-chunk.start;
        // Our convention is that the index must always be on a code point boundary.
        //  If we are somewhere in the middle of a utf-16 buffer, check that new index
        //  is not in the middle of a surrogate pair.
        if (index>chunk.start && index < chunk.limit) {   // TODO:  clean up end-of-chunk / end of input handling.  Everywhere.
            UChar c = chunk.contents[chunkOffset];
            if (U16_TRAIL(c)) {
                this->getSupplementary();  // force index onto a code point boundary.
            }
        }
    }
}

UChar32
UTextIterator::next32From(int32_t index) {
    if(index<chunk.start || chunk.limit<=index) {
        if(!access(index, TRUE)) {
            // no chunk available here
            return U_SENTINEL;
        }
    } else if(chunk.nonUTF16Indexes) {
        chunkOffset=t->mapIndexToUTF16(t, &chunk, index);
    } else {
        chunkOffset=index-chunk.start;
    }

    UChar32 c;
    U16_NEXT(chunk.contents, chunkOffset, chunk.length, c);
    return c;
}

UChar32 UTextIterator::previous32From(int32_t index) {
    if(index<=chunk.start || chunk.limit<index) {
        if(!access(index, FALSE)) {
            // no chunk available here
            return U_SENTINEL;
        }
    } else if(chunk.nonUTF16Indexes) {
        chunkOffset=t->mapIndexToUTF16(t, &chunk, index);
    } else {
        chunkOffset=index-chunk.start;
    }

    UChar32 c;
    chunkOffset--;
    c = chunk.contents[chunkOffset];
    if (U16_IS_SURROGATE(c)) {
        // take supplementary support out-of-line
        c = this->getSupplementary();
    }
    return c;
}

int32_t UTextIterator::getIndex() {
    if(!chunk.nonUTF16Indexes || chunkOffset==0) {
        return chunk.start+chunkOffset;
    } else {
        return t->mapOffsetToNative(t, &chunk, chunkOffset);
    }
}

U_NAMESPACE_END

#endif /* C++ */

#endif /* U_HIDE_DRAFT_API */

#endif
