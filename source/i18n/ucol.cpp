/*
*******************************************************************************
*   Copyright (C) 1996-1999, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*/

#include "unicode/ucol.h"

#include "unicode/uloc.h"
#include "unicode/coll.h"
#include "unicode/tblcoll.h"
#include "unicode/coleitr.h"
#include "unicode/ustring.h"
#include "unicode/normlzr.h"
#include "cpputils.h"

#define UCOL_LEVELTERMINATOR 0
#define UCOL_IGNORABLE 0x0000
#define UCOL_CHARINDEX 0x70000000             // need look up in .commit()
#define UCOL_EXPANDCHARINDEX 0x7E000000       // Expand index follows
#define UCOL_CONTRACTCHARINDEX 0x7F000000     // contract indexes follows
#define UCOL_UNMAPPED 0xFFFFFFFF              // unmapped character values
#define UCOL_PRIMARYORDERINCREMENT 0x00010000 // primary strength increment
#define UCOL_SECONDARYORDERINCREMENT 0x00000100 // secondary strength increment
#define UCOL_TERTIARYORDERINCREMENT 0x00000001 // tertiary strength increment
#define UCOL_MAXIGNORABLE 0x00010000          // maximum ignorable char order value
#define UCOL_PRIMARYORDERMASK 0xffff0000      // mask off anything but primary order
#define UCOL_SECONDARYORDERMASK 0x0000ff00    // mask off anything but secondary order
#define UCOL_TERTIARYORDERMASK 0x000000ff     // mask off anything but tertiary order
#define UCOL_SECONDARYRESETMASK 0x0000ffff    // mask off secondary and tertiary order
#define UCOL_IGNORABLEMASK 0x0000ffff         // mask off ignorable char order
#define UCOL_PRIMARYDIFFERENCEONLY 0xffff0000 // use only the primary difference
#define UCOL_SECONDARYDIFFERENCEONLY 0xffffff00  // use only the primary and secondary difference
#define UCOL_PRIMARYORDERSHIFT 16             // primary order shift
#define UCOL_SECONDARYORDERSHIFT 8            // secondary order shift
#define UCOL_SORTKEYOFFSET 1                  // minimum sort key offset
#define UCOL_CONTRACTCHAROVERFLOW 0x7FFFFFFF  // Indicates the char is a contract char

U_CAPI int32_t
u_normalize(const UChar*            source,
        int32_t                 sourceLength, 
        UNormalizationMode      mode, 
        int32_t                 option,
        UChar*                  result,
        int32_t                 resultLength,
        UErrorCode*             status)
{
  if(U_FAILURE(*status)) return -1;

  Normalizer::EMode normMode;
  switch(mode) {
  case UCOL_NO_NORMALIZATION:
    normMode = Normalizer::NO_OP;
    break;
  case UCOL_DECOMP_CAN:
    normMode = Normalizer::DECOMP;
    break;
  case UCOL_DECOMP_COMPAT:
    normMode = Normalizer::DECOMP_COMPAT;
    break;
  case UCOL_DECOMP_CAN_COMP_COMPAT:
    normMode = Normalizer::COMPOSE;
    break;
  case UCOL_DECOMP_COMPAT_COMP_CAN:
    normMode = Normalizer::COMPOSE_COMPAT;
    break;
  default:
    *status = U_ILLEGAL_ARGUMENT_ERROR;
    return -1;
  }

  int32_t len = (sourceLength == -1 ? u_strlen(source) : sourceLength);
  const UnicodeString src((UChar*)source, len, len);
  UnicodeString dst(result, 0, resultLength);
  Normalizer::normalize(src, normMode, option, dst, *status);
  int32_t actualLen;
  T_fillOutputParams(&dst, result, resultLength, &actualLen, status);
  return actualLen;
}

U_CAPI UCollator*
ucol_open(    const    char         *loc,
        UErrorCode      *status)
{
  if(U_FAILURE(*status)) return 0;

  Collator *col = 0;

  if(loc == 0) 
    col = Collator::createInstance(*status);
  else
    col = Collator::createInstance(Locale(loc), *status);

  if(col == 0) {
    *status = U_MEMORY_ALLOCATION_ERROR;
    return 0;
  }

  return (UCollator*)col;
}

U_CAPI UCollator*
ucol_openRules(    const    UChar                  *rules,
        int32_t                 rulesLength,
        UNormalizationMode      mode,
        UCollationStrength      strength,
        UErrorCode              *status)
{
  if(U_FAILURE(*status)) return 0;

  int32_t len = (rulesLength == -1 ? u_strlen(rules) : rulesLength);
  const UnicodeString ruleString((UChar*)rules, len, len);

  Normalizer::EMode normMode;
  switch(mode) {
  case UCOL_NO_NORMALIZATION:
    normMode = Normalizer::NO_OP;
    break;
  case UCOL_DECOMP_CAN:
    normMode = Normalizer::DECOMP;
    break;
  case UCOL_DECOMP_COMPAT:
    normMode = Normalizer::DECOMP_COMPAT;
    break;
  case UCOL_DECOMP_CAN_COMP_COMPAT:
    normMode = Normalizer::COMPOSE;
    break;
  case UCOL_DECOMP_COMPAT_COMP_CAN:
    normMode = Normalizer::COMPOSE_COMPAT;
    break;
  default:
    *status = U_ILLEGAL_ARGUMENT_ERROR;
    return 0;
  }

  RuleBasedCollator *col = 0;
  col = new RuleBasedCollator(ruleString, 
                  (Collator::ECollationStrength) strength, 
                  normMode, 
                  *status);

  if(col == 0) {
    *status = U_MEMORY_ALLOCATION_ERROR;
    return 0;
  }

  return (UCollator*) col;
}

U_CAPI void
ucol_close(UCollator *coll)
{
  delete (Collator*)coll;
}

U_CAPI UBool
ucol_greater(    const    UCollator        *coll,
        const    UChar            *source,
        int32_t            sourceLength,
        const    UChar            *target,
        int32_t            targetLength)
{
  return (ucol_strcoll(coll, source, sourceLength, target, targetLength) 
      == UCOL_GREATER);
}

U_CAPI UBool
ucol_greaterOrEqual(    const    UCollator    *coll,
            const    UChar        *source,
            int32_t        sourceLength,
            const    UChar        *target,
            int32_t        targetLength)
{
  return (ucol_strcoll(coll, source, sourceLength, target, targetLength) 
      != UCOL_LESS);
}

U_CAPI UBool
ucol_equal(        const    UCollator        *coll,
            const    UChar            *source,
            int32_t            sourceLength,
            const    UChar            *target,
            int32_t            targetLength)
{
  return (ucol_strcoll(coll, source, sourceLength, target, targetLength) 
      == UCOL_EQUAL);
}

U_CAPI UCollationStrength
ucol_getStrength(const UCollator *coll)
{
  return (UCollationStrength) ((Collator*)coll)->getStrength();
}


U_CAPI void
ucol_setStrength(    UCollator                *coll,
            UCollationStrength        strength)
{
  ((Collator*)coll)->setStrength((Collator::ECollationStrength)strength);
}

U_CAPI UNormalizationMode
ucol_getNormalization(const UCollator* coll)
{
  switch(((Collator*)coll)->getDecomposition()) {
  case Normalizer::NO_OP:
    return UCOL_NO_NORMALIZATION;

  case Normalizer::COMPOSE:
    return UCOL_DECOMP_COMPAT_COMP_CAN;

  case Normalizer::COMPOSE_COMPAT:
    return UCOL_DECOMP_CAN_COMP_COMPAT;

  case Normalizer::DECOMP:
    return UCOL_DECOMP_CAN;

  case Normalizer::DECOMP_COMPAT:
    return UCOL_DECOMP_COMPAT;

  }
  return UCOL_NO_NORMALIZATION;
}

U_CAPI void
ucol_setNormalization(  UCollator            *coll,
            UNormalizationMode    mode)
{
  Normalizer::EMode normMode;
  switch(mode) {
  case UCOL_NO_NORMALIZATION:
    normMode = Normalizer::NO_OP;
    break;
  case UCOL_DECOMP_CAN:
    normMode = Normalizer::DECOMP;
    break;
  case UCOL_DECOMP_COMPAT:
    normMode = Normalizer::DECOMP_COMPAT;
    break;
  case UCOL_DECOMP_COMPAT_COMP_CAN:
    normMode = Normalizer::COMPOSE;
    break;
  case UCOL_DECOMP_CAN_COMP_COMPAT:
    normMode = Normalizer::COMPOSE_COMPAT;
    break;
  default:
    /* Shouldn't get here. */
    /* *status = U_ILLEGAL_ARGUMENT_ERROR; */
    return;
  }

  ((Collator*)coll)->setDecomposition(normMode);
}

U_CAPI int32_t
ucol_getDisplayName(    const    char        *objLoc,
            const    char        *dispLoc,
            UChar             *result,
            int32_t         resultLength,
            UErrorCode        *status)
{
  if(U_FAILURE(*status)) return -1;

  UnicodeString dst(result, resultLength, resultLength);
  Collator::getDisplayName(Locale(objLoc), Locale(dispLoc), dst);
  int32_t actLen;
  T_fillOutputParams(&dst, result, resultLength, &actLen, status);
  return actLen;
}

U_CAPI const char*
ucol_getAvailable(int32_t index)
{
  return uloc_getAvailable(index);
}

U_CAPI int32_t
ucol_countAvailable()
{
  return uloc_countAvailable();
}

U_CAPI const UChar*
ucol_getRules(    const    UCollator        *coll, 
        int32_t            *length)
{
  const UnicodeString& rules = ((RuleBasedCollator*)coll)->getRules();
  *length = rules.length();
  return rules.getUChars();
}

static uint8_t utf16fixup[32] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0x20, 0xf8, 0xf8, 0xf8, 0xf8
};

// This will get the next CE(s)?
// Should be part macro, part function
#include <stdio.h>

#include "unicode/normlzr.h"
#include "ucmp32.h"
#include "tcoldata.h"
#include "tables.h"
#define UCOL_MAX_BUFFER 1000

struct collIterate {
  UChar *string; // Original string
  UChar *len;   // Original string length
  UChar *pos; // This is position in the string
  uint32_t *toReturn; // This is the CE from CEs buffer that should be returned
  uint32_t *CEpos; // This is the position to which we have stored processed CEs
  uint32_t CEs[1024]; // This is where we store CEs
};

void init_collIterate(const UChar *string, int32_t len, collIterate *s) {
    s->string = s->pos = (UChar *)string;
    s->len = (UChar *)string+len;
    s->CEpos = s->toReturn = s->CEs;
}

int32_t ucol_getNextCE(const UCollator *coll, collIterate *source, UErrorCode *status) {

  if (U_FAILURE(*status) || (source->pos>=source->len && source->CEpos <= source->toReturn)) {
    return CollationElementIterator::NULLORDER;
  }
  
  if (source->CEpos > source->toReturn) {
      return(*(source->toReturn++));
  }
 
  source->CEpos = source->toReturn = source->CEs;
 
  *(source->CEpos)  = ucmp32_get(((RuleBasedCollator *)coll)->data->mapping, *(source->pos));

  // this should benefit from reordering of the clauses, so that the cleanest case is returned the first.

  if(*(source->CEpos) < UCOL_EXPANDCHARINDEX) {
      
    source->pos++;
    return (*(source->CEpos));
  }

  if (*(source->CEpos) == UCOL_UNMAPPED) {
      // Returned an "unmapped" flag and save the character so it can be 
        // returned next time this method is called.
        if (*(source->pos) == 0x0000) return *(source->pos++); // \u0000 is not valid in C++'s UnicodeString
    	*(source->CEpos++) = CollationElementIterator::UNMAPPEDCHARVALUE;
	    *(source->CEpos) = *(source->pos)<<16;
    } else {
        // Contraction sequence start...
        if (*(source->CEpos) >= UCOL_CONTRACTCHARINDEX) {
            VectorOfPToContractElement* list = ((RuleBasedCollator *)coll)->data->contractTable->at(*(source->CEpos)-UCOL_CONTRACTCHARINDEX);
            // The upper line obtained a list of contracting sequences.
            if (list != NULL) {
				EntryPair *pair = (EntryPair *)list->at(0); // Taking out the first one.
				int32_t order = pair->value; // This got us mapping for just the first element - the one that signalled a contraction.

				UChar key[1024];
				uint32_t posKey = 0;

				key[posKey++] = *(source->pos);
				// This tries to find the longes common match for the data in contraction table...
				// and needs to be rewritten, especially the test down there!
				int32_t i;
				UBool foundSmaller = TRUE;

				while(source->pos<source->len && foundSmaller) {

					key[posKey++] = *(++source->pos);

					foundSmaller = FALSE;
					i = 0;
					while(i<list->size() && !foundSmaller) {
						pair = list->at(i);
						if ((pair != NULL) && (pair->fwd == TRUE /*fwd*/) && (pair->entryName == UnicodeString(key, posKey))) {
							order = pair->value;
							foundSmaller = TRUE;
						}
						i++;
					}
				}
				source->pos--; /* spit back the last char - it wasn't part of the sequence */
				*(source->CEpos) = order;
			}
    }
	// Expansion sequence start...
        if (*(source->CEpos) >= UCOL_EXPANDCHARINDEX) {
            VectorOfInt *v = ((RuleBasedCollator *)coll)->data->expandTable->at(*(source->CEpos)-UCOL_EXPANDCHARINDEX);
            if(v != NULL) {
                int32_t expandindex=0;
                while(expandindex < v->size()) {
                    *(source->CEpos++) = v->at(expandindex++);
                }
            }
        }

     // Thai/Lao reordering
        if (CollationElementIterator::isThaiPreVowel(*(source->pos))) {
            UChar consonant = *(source->pos+1);
            if (CollationElementIterator::isThaiBaseConsonant(consonant)) {
	      // find the element for consonant
	      // and reorder them
            }
        }
    }
    source->pos++;
    return (*(source->toReturn++));
}

U_CAPI UCollationResult
ucol_strcollEx(    const    UCollator    *coll,
        const    UChar        *source,
        int32_t            sourceLength,
        const    UChar        *target,
        int32_t            targetLength)
{
    if (coll == NULL) return UCOL_EQUAL;
    if (sourceLength == -1) sourceLength = u_strlen(source);
    if (targetLength == -1) targetLength = u_strlen(target);
        return (UCollationResult) ((RuleBasedCollator*)coll)->compareEx(source,sourceLength,target,targetLength);
}

U_CAPI UCollationResult
ucol_strcoll(    const    UCollator    *coll,
        const    UChar        *source,
        int32_t            sourceLength,
        const    UChar        *target,
        int32_t            targetLength)
{

    // check if source and target are valid strings
    if (((source == 0) && (target == 0)) ||
        ((sourceLength == 0) && (targetLength == 0)))
    {
        return UCOL_EQUAL;
    }

    UCollationResult result = UCOL_EQUAL;
    UErrorCode status = U_ZERO_ERROR;

    UChar normSource[UCOL_MAX_BUFFER], normTarget[UCOL_MAX_BUFFER];
    uint32_t normSourceLength = UCOL_MAX_BUFFER, normTargetLength = UCOL_MAX_BUFFER;

#if 0
    if (cursor1 == NULL)
    {
        ((RuleBasedCollator *)this)->cursor1 = new NormalizerIterator(source, sourceLength, getDecomposition());
    }
    else
    {
        cursor1->setModeAndText(getDecomposition(), source, sourceLength, status);
    }

    if ( /*cursor1->cursor == NULL ||*/ U_FAILURE(status))
    {
        return Collator::EQUAL;
    }

    if (cursor2 == NULL)
    {
        ((RuleBasedCollator *)this)->cursor2 = new NormalizerIterator(target, targetLength, getDecomposition());
    }
    else
    {
        cursor2->setModeAndText(getDecomposition(), target, targetLength, status);
    }
#endif

    collIterate sColl, tColl;


    UNormalizationMode normMode = ucol_getNormalization(coll);
    if(normMode == UCOL_NO_NORMALIZATION) {
        init_collIterate(source, sourceLength, &sColl);
        init_collIterate(target, targetLength, &tColl);
    } else {
        normSourceLength = u_normalize(source, sourceLength, normMode, 0, normSource, normSourceLength, &status);
        normTargetLength = u_normalize(target, targetLength, normMode, 0, normTarget, normTargetLength, &status);
        init_collIterate(normSource, normSourceLength, &sColl);
        init_collIterate(normTarget, normTargetLength, &tColl);
	}

    if (/*cursor2 == NULL ||*/ U_FAILURE(status))
    {
        return UCOL_EQUAL;
    }

    int32_t sOrder, tOrder;
    //    int32_t sOrder = CollationElementIterator::NULLORDER, tOrder = CollationElementIterator::NULLORDER;
    UBool gets = TRUE, gett = TRUE;
    UBool initialCheckSecTer = ucol_getStrength(coll) >= UCOL_SECONDARY;
    UBool checkSecTer = initialCheckSecTer;
    UBool checkTertiary = ucol_getStrength(coll) >= UCOL_TERTIARY;
    UBool isFrenchSec = (ucol_getAttribute(coll, UCOL_FRENCH_COLLATION, &status) == UCOL_ATTR_ON);
    uint32_t pSOrder, pTOrder;

    for(;;)
    {
        // Get the next collation element in each of the strings, unless
        // we've been requested to skip it.
        if (gets)
        {
            //sOrder = getStrengthOrder((NormalizerIterator*)cursor1, status);
            //printf("/Ex 1 Go/ %x, %x, %x, %x, %x, %x\n", sColl.string, sColl.len, sColl.pos, sColl.CEs, sColl.CEpos, sColl.toReturn);
            sOrder = ucol_getNextCE(coll, &sColl, &status);
            //printf("/Ex 1 End/ %x, %x, %x, %x, %x, %x\n", sColl.string, sColl.len, sColl.pos, sColl.CEs, sColl.CEpos, sColl.toReturn);

            if (U_FAILURE(status))
            {
                return UCOL_EQUAL;
            }
        }

        gets = TRUE;

        if (gett)
        {
            //tOrder = getStrengthOrder((NormalizerIterator*)cursor2, status);
            //printf("/Ex 2 Go/ %x, %x, %x, %x, %x, %x\n", tColl.string, tColl.len, tColl.pos, tColl.CEs, tColl.CEpos, tColl.toReturn);
            tOrder = ucol_getNextCE(coll, &tColl, &status);
            //printf("/Ex 2 End/ %x, %x, %x, %x, %x, %x\n", tColl.string, tColl.len, tColl.pos, tColl.CEs, tColl.CEpos, tColl.toReturn);

            if (U_FAILURE(status))
            {
                return UCOL_EQUAL;
            }
        }
        
        gett = TRUE;

        // If we've hit the end of one of the strings, jump out of the loop
        if ((sOrder == CollationElementIterator::NULLORDER)||
            (tOrder == CollationElementIterator::NULLORDER))
        {
            break;
        }

        // If there's no difference at this position, we can skip to the
        // next one.
        pSOrder = CollationElementIterator::primaryOrder(sOrder);
        pTOrder = CollationElementIterator::primaryOrder(tOrder);
        if (sOrder == tOrder)
        {
            if (isFrenchSec && pSOrder != 0)
            {
                if (!checkSecTer)
                {
                    // in french, a secondary difference more to the right is stronger,
                    // so accents have to be checked with each base element
                    checkSecTer = initialCheckSecTer;

                    // but tertiary differences are less important than the first 
                    // secondary difference, so checking tertiary remains disabled
                    checkTertiary = FALSE;
                }
            }

            continue;
        }

        // Compare primary differences first.
        if (pSOrder != pTOrder)
        {
            if (sOrder == 0)
            {
                // The entire source element is ignorable.
                // Skip to the next source element, but don't fetch another target element.
                gett = FALSE;
                continue;
            }

            if (tOrder == 0)
            {
                gets = FALSE;
                continue;
            }

            // The source and target elements aren't ignorable, but it's still possible
            // for the primary component of one of the elements to be ignorable....
            if (pSOrder == 0)  // primary order in source is ignorable
            {
                // The source's primary is ignorable, but the target's isn't.  We treat ignorables
                // as a secondary difference, so remember that we found one.
                if (checkSecTer)
                {
                    result = UCOL_GREATER;  // (strength is SECONDARY)
                    checkSecTer = FALSE;
                }

                // Skip to the next source element, but don't fetch another target element.
                gett = FALSE;
            }
            else if (pTOrder == 0)
            {
                // record differences - see the comment above.
                if (checkSecTer)
                {
                    result = UCOL_LESS;  // (strength is SECONDARY)
                    checkSecTer = FALSE;
                }

                // Skip to the next target element, but don't fetch another source element.
                gets = FALSE;
            }
            else
            {
                // Neither of the orders is ignorable, and we already know that the primary
                // orders are different because of the (pSOrder != pTOrder) test above.
                // Record the difference and stop the comparison.
                if (pSOrder < pTOrder)
                {
                    return UCOL_LESS;  // (strength is PRIMARY)
                }

                return UCOL_GREATER;  // (strength is PRIMARY)
            }
        }
        else
        { // else of if ( pSOrder != pTOrder )
            // primary order is the same, but complete order is different. So there
            // are no base elements at this point, only ignorables (Since the strings are
            // normalized)

            if (checkSecTer)
            {
                // a secondary or tertiary difference may still matter
                uint32_t secSOrder = CollationElementIterator::secondaryOrder(sOrder);
                uint32_t secTOrder = CollationElementIterator::secondaryOrder(tOrder);

                if (secSOrder != secTOrder)
                {
                    // there is a secondary difference
                    result = (secSOrder < secTOrder) ? UCOL_LESS : UCOL_GREATER;
                                            // (strength is SECONDARY)
                    checkSecTer = FALSE; 
                    // (even in french, only the first secondary difference within
                    //  a base character matters)
                }
                else
                {
                    if (checkTertiary)
                    {
                        // a tertiary difference may still matter
                        uint32_t terSOrder = CollationElementIterator::tertiaryOrder(sOrder);
                        uint32_t terTOrder = CollationElementIterator::tertiaryOrder(tOrder);

                        if (terSOrder != terTOrder)
                        {
                            // there is a tertiary difference
                            result = (terSOrder < terTOrder) ? UCOL_LESS : UCOL_GREATER;
                                            // (strength is TERTIARY)
                            checkTertiary = FALSE;
                        }
                    }
                }
            } // if (checkSecTer)

        }  // if ( pSOrder != pTOrder )
    } // while()

    if (sOrder != CollationElementIterator::NULLORDER)
    {
        // (tOrder must be CollationElementIterator::NULLORDER,
        //  since this point is only reached when sOrder or tOrder is NULLORDER.)
        // The source string has more elements, but the target string hasn't.
        do
        {
            if (CollationElementIterator::primaryOrder(sOrder) != 0)
            {
                // We found an additional non-ignorable base character in the source string.
                // This is a primary difference, so the source is greater
                return UCOL_GREATER; // (strength is PRIMARY)
            }

            if (CollationElementIterator::secondaryOrder(sOrder) != 0)
            {
                // Additional secondary elements mean the source string is greater
                if (checkSecTer)
                {
                    result = UCOL_GREATER;  // (strength is SECONDARY)
                    checkSecTer = FALSE;
                }
            } 
        }
        while ((sOrder = ucol_getNextCE(coll, &sColl, &status)) != CollationElementIterator::NULLORDER);
        //while ((sOrder = getStrengthOrder(cursor1, status)) != CollationElementIterator::NULLORDER);
    }
    else if (tOrder != CollationElementIterator::NULLORDER)
    {
        // The target string has more elements, but the source string hasn't.
        do
        {
            if (CollationElementIterator::primaryOrder(tOrder) != 0)
            {
                // We found an additional non-ignorable base character in the target string.
                // This is a primary difference, so the source is less
                return UCOL_LESS; // (strength is PRIMARY)
            }

            if (CollationElementIterator::secondaryOrder(tOrder) != 0)
            {
                // Additional secondary elements in the target mean the source string is less
                if (checkSecTer)
                {
                    result = UCOL_LESS;  // (strength is SECONDARY)
                    checkSecTer = FALSE;
                }
            } 
        }
        while ((tOrder = ucol_getNextCE(coll, &tColl, &status)) != CollationElementIterator::NULLORDER);
        //while ((tOrder = getStrengthOrder(cursor2, status)) != CollationElementIterator::NULLORDER);
    }


    // For IDENTICAL comparisons, we use a bitwise character comparison
    // as a tiebreaker if all else is equal
    // NOTE: The java code compares result with 0, and 
    // puts the result of the string comparison directly into result
    if (result == UCOL_EQUAL && ucol_getStrength(coll) == UCOL_IDENTICAL)
    {
#if 0
      // ******** for the  UChar normalization interface.
      // It doesn't work much faster, and the code was broken
      // so it's commented out. --srl
//          UChar sourceDecomp[1024], targetDecomp[1024];
//          int32_t sourceDecompLength = 1024;
//          int32_t targetDecompLength = 1024;

//          int8_t comparison;
//          Normalizer::EMode decompMode = getDecomposition();

//          if (decompMode != Normalizer::NO_OP)
//            {
//              Normalizer::normalize(source, sourceLength, decompMode,
//                        0, sourceDecomp, sourceDecompLength, status);

//              Normalizer::normalize(target, targetLength, decompMode,
//                        0, targetDecomp, targetDecompLength, status);

//              comparison = u_strcmp(sourceDecomp,targetDecomp);
//            }
//          else
//            {
//              comparison = u_strcmp(source, target); /* ! */
//            }

#else

        UnicodeString sourceDecomp, targetDecomp;

        int8_t comparison;
        
        Normalizer::normalize(source, ((RuleBasedCollator *)coll)->getDecomposition(), 
                      0, sourceDecomp,  status);

        Normalizer::normalize(target, ((RuleBasedCollator *)coll)->getDecomposition(), 
                      0, targetDecomp,  status);
        
        comparison = sourceDecomp.compare(targetDecomp);
#endif

        if (comparison < 0)
        {
            result = UCOL_LESS;
        }
        else if (comparison == 0)
        {
            result = UCOL_EQUAL;
        }
        else
        {
            result = UCOL_GREATER;
        }
    }

    return result;
}

U_CAPI int32_t
ucol_getSortKeyEx(const    UCollator    *coll,
        const    UChar        *source,
        int32_t        sourceLength,
        uint8_t        *result,
        int32_t        resultLength)
{
  int32_t         count;
  const uint8_t*     bytes = NULL;
  CollationKey         key;
  int32_t         copyLen;
    int32_t         len = (sourceLength == -1 ? u_strlen(source) 
                   : sourceLength);
  //  UnicodeString     string((UChar*)source, len, len);
  UErrorCode         status = U_ZERO_ERROR;

  ((RuleBasedCollator*)coll)->getCollationKeyEx(source, len, key, status);
  if(U_FAILURE(status)) 
    return 0;

  bytes = key.getByteArray(count);
  
  copyLen = uprv_min(count, resultLength);
  uprv_arrayCopy((const int8_t*)bytes, (int8_t*)result, copyLen);

  //  if(count > resultLength) {
  //    *status = U_BUFFER_OVERFLOW_ERROR;
  //  }

  return count;
}

U_CAPI int32_t
ucol_getSortKey(const    UCollator    *coll,
        const    UChar        *source,
        int32_t        sourceLength,
        uint8_t        *result,
        int32_t        resultLength)
{

    //uprv_memset(result, 0xAA, resultLength); // for debug purposes


    /* 
    Still problems in:
    SUMMARY:
        ******* [Total error count:     213]
         Errors in
           [tscoll/capitst/TestSortKey]  // this is normal, since we are changing binary keys
           [tscoll/cfrtst/TestSecondary] // this is also OK, ICU original implementation was messed up
           [tscoll/cfrtst/TestTertiary]  // probably the same as above
           [tscoll/cjacoll/TestTertiary] // most probably due to normalization...
           [tscoll/cg7coll/TestDemo4]    // need to check
         Total errors: 213
    */

    uint32_t i = 0; // general purpose counter

	UErrorCode status = U_ZERO_ERROR;

    uint8_t prim[2*UCOL_MAX_BUFFER], second[UCOL_MAX_BUFFER], tert[UCOL_MAX_BUFFER];

    uint8_t *primaries = prim, *secondaries = second, *tertiaries = tert;

    UChar normBuffer[2*UCOL_MAX_BUFFER];
    UChar *normSource = normBuffer;
    int32_t normSourceLen = 2048;

	int32_t len = (sourceLength == -1 ? u_strlen(source) : sourceLength);

    UBool  compareSec   = (((RuleBasedCollator *)coll)->getStrength() >= Collator::SECONDARY);
    UBool  compareTer   = (((RuleBasedCollator *)coll)->getStrength() >= Collator::TERTIARY);
    UBool  compareIdent = (((RuleBasedCollator *)coll)->getStrength() == Collator::IDENTICAL);

    if(len > UCOL_MAX_BUFFER) {
        primaries = (uint8_t *)uprv_malloc(6*len*sizeof(uint8_t));
        if(compareSec) {
            secondaries = (uint8_t *)uprv_malloc(2*len*sizeof(uint8_t));
        }
        if(compareTer) {
            tertiaries = (uint8_t *)uprv_malloc(2*len*sizeof(uint8_t));
        }
    }

    uint8_t *primstart = primaries;
    uint8_t *secstart = secondaries;
    uint8_t *terstart = tertiaries;

	collIterate s;
    init_collIterate((UChar *)source, len, &s);

    // If we need to normalize, we'll do it all at once at the beggining!
    if(((RuleBasedCollator *)coll)->getDecomposition() != Normalizer::NO_OP) {
		UnicodeString normalized;
		Normalizer::normalize(UnicodeString(source, sourceLength), ((RuleBasedCollator *)coll)->getDecomposition(),
			0, normalized, status);
		normSourceLen = normalized.length();

        if(normSourceLen > UCOL_MAX_BUFFER) {
            normSource = (UChar *) uprv_malloc(normSourceLen*sizeof(UChar));
        }

		normalized.extract(0, normSourceLen, normSource);
		s.string = normSource;
        s.pos = normSource;
		s.len = normSource+normSourceLen;
	}

    int32_t order = 0;

    uint16_t primary = 0;
    uint8_t secondary = 0;
    uint8_t tertiary = 0;

    while((order = ucol_getNextCE(coll, &s, &status)) !=
    CollationElementIterator::NULLORDER) {
        primary = ((order & UCOL_PRIMARYORDERMASK)>> UCOL_PRIMARYORDERSHIFT);
        secondary = ((order & UCOL_SECONDARYORDERMASK)>> UCOL_SECONDARYORDERSHIFT);
        tertiary = (order & UCOL_TERTIARYORDERMASK);

        if(primary != UCOL_IGNORABLE) {
            *(primaries++) = (primary+UCOL_SORTKEYOFFSET)>>8;
            *(primaries++) = (primary+UCOL_SORTKEYOFFSET)&0xFF;
            if(compareSec) {
                *(secondaries++) = secondary+UCOL_SORTKEYOFFSET;
            }
            if(compareTer) {
                *(tertiaries++) = tertiary+UCOL_SORTKEYOFFSET;
            }
        } else {
            if(compareSec && secondary != 0) {
                *(secondaries++) = secondary+UCOL_SORTKEYOFFSET;
            }
            if(compareTer && tertiary != 0) {
                *(tertiaries++) = tertiary+UCOL_SORTKEYOFFSET;
            }
        }
    }

    *(primaries++) = UCOL_LEVELTERMINATOR;
    *(primaries++) = UCOL_LEVELTERMINATOR;


    if(compareSec) {
      uint32_t secsize = secondaries-secstart;
      if(ucol_getAttribute(coll, UCOL_FRENCH_COLLATION, &status) == UCOL_ATTR_ON) { // do the reverse copy
          for(i = 0; i<secsize; i++) {
              *(primaries++) = *(secondaries-i-1);
          }
        } else { 
            uprv_memcpy(primaries, secstart, secsize); 
            primaries += secsize;
        }

        *(primaries++) = UCOL_LEVELTERMINATOR;
    }

    if(compareTer) {
      uint32_t tersize = tertiaries - terstart;
      uprv_memcpy(primaries, terstart, tersize);
      primaries += tersize;
      *(primaries++) = UCOL_LEVELTERMINATOR;
    }


    if(compareIdent) {
      for(i = 0; i<len; i++) {
          *(primaries++) = (s.string[i] >> 8) + utf16fixup[s.string[i] >> 11];
          *(primaries++) = (s.string[i] & 0xFF);
      }
      *(primaries++) = UCOL_LEVELTERMINATOR;
    }

    uprv_memcpy(result, primstart, uprv_min(resultLength, (primaries-primstart)));

    if(terstart != tert) {
        uprv_free(terstart);
    }
    if(secstart != second) {
        uprv_free(secstart);
    }
    if(primstart != prim) {
        uprv_free(primstart);
    }
    if(normSource != normBuffer) {
        uprv_free(normSource);
    }

    return primaries-primstart;
}

U_CAPI int32_t
ucol_keyHashCode(    const    uint8_t*    key, 
            int32_t        length)
{
  CollationKey newKey(key, length);
  return newKey.hashCode();
}

UCollationElements*
ucol_openElements(    const    UCollator            *coll,
            const    UChar                *text,
            int32_t              textLength,
            UErrorCode *status)
{
  int32_t len = (textLength == -1 ? u_strlen(text) : textLength);
  const UnicodeString src((UChar*)text, len, len);

  CollationElementIterator *iter = 0;
  iter = ((RuleBasedCollator*)coll)->createCollationElementIterator(src);
  if(iter == 0) {
    *status = U_MEMORY_ALLOCATION_ERROR;
    return 0;
  }

  return (UCollationElements*) iter;
}

U_CAPI void
ucol_closeElements(UCollationElements *elems)
{
  delete (CollationElementIterator*)elems;
}

U_CAPI void
ucol_reset(UCollationElements *elems)
{
  ((CollationElementIterator*)elems)->reset();
}

U_CAPI int32_t
ucol_next(    UCollationElements    *elems,
        UErrorCode            *status)
{
  if(U_FAILURE(*status)) return UCOL_NULLORDER;

  return ((CollationElementIterator*)elems)->next(*status);
}

U_CAPI int32_t
ucol_previous(    UCollationElements    *elems,
        UErrorCode            *status)
{
  if(U_FAILURE(*status)) return UCOL_NULLORDER;

  return ((CollationElementIterator*)elems)->previous(*status);
}

U_CAPI int32_t
ucol_getMaxExpansion(    const    UCollationElements    *elems,
            int32_t                order)
{
  return ((CollationElementIterator*)elems)->getMaxExpansion(order);
}

U_CAPI void
ucol_setText(UCollationElements        *elems,
         const    UChar                    *text,
         int32_t                    textLength,
         UErrorCode                *status)
{
  if(U_FAILURE(*status)) return;

  int32_t len = (textLength == -1 ? u_strlen(text) : textLength);
  const UnicodeString src((UChar*)text, len, len);

  ((CollationElementIterator*)elems)->setText(src, *status);
}

U_CAPI UTextOffset
ucol_getOffset(const UCollationElements *elems)
{
  return ((CollationElementIterator*)elems)->getOffset();
}

U_CAPI void
ucol_setOffset(    UCollationElements    *elems,
        UTextOffset            offset,
        UErrorCode            *status)
{
  if(U_FAILURE(*status)) return;
  
  ((CollationElementIterator*)elems)->setOffset(offset, *status);
}

U_CAPI void 
ucol_getVersion(const UCollator* coll, 
                UVersionInfo versionInfo) 
{
    ((Collator*)coll)->getVersion(versionInfo);
}

U_CAPI uint8_t *
ucol_cloneRuleData(UCollator *coll, int32_t *length, UErrorCode *status)
{
  return ((RuleBasedCollator*)coll)->cloneRuleData(*length,*status);
}

U_CAPI void ucol_setAttribute(UCollator *coll, UColAttribute attr, UColAttributeValue value, UErrorCode *status) {
	((RuleBasedCollator *)coll)->setAttribute(attr, value, *status);
}

U_CAPI UColAttributeValue ucol_getAttribute(const UCollator *coll, UColAttribute attr, UErrorCode *status) {
	return (((RuleBasedCollator *)coll)->getAttribute(attr, *status));
}

U_CAPI UCollator *ucol_safeClone(const UCollator *coll, void *stackBuffer, uint32_t bufferSize, UErrorCode *status) {
	*status = U_UNSUPPORTED_ERROR;
	return NULL;
}

U_CAPI UCollationResult ucol_strcollinc(const UCollator *coll, 
								 UCharForwardIterator *source, void *sourceContext,
								 UCharForwardIterator *target, void *targetContext) {
	return UCOL_EQUAL;
}

U_CAPI int32_t ucol_getRulesEx(const UCollator *coll, UColRuleOption delta, UChar *buffer, int32_t bufferLen) {
	return 0;
}
