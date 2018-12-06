// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*****************************************************************************************
* Copyright (C) 2010-2011, International Business Machines
* Corporation and others. All Rights Reserved.
*****************************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/udateintervalformat.h"
#include "unicode/dtitvfmt.h"
#include "unicode/dtintrv.h"
#include "unicode/localpointer.h"
#include "unicode/timezone.h"
#include "unicode/locid.h"
#include "unicode/unistr.h"

U_NAMESPACE_USE


U_CAPI UDateIntervalFormat* U_EXPORT2
udtitvfmt_open(const char*  locale,
               const UChar* skeleton,
               int32_t      skeletonLength,
               const UChar* tzID,
               int32_t      tzIDLength,
               UErrorCode*  status)
{
    if (U_FAILURE(*status)) {
        return NULL;
    }
    if ((skeleton == NULL ? skeletonLength != 0 : skeletonLength < -1) ||
        (tzID == NULL ? tzIDLength != 0 : tzIDLength < -1)
    ) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    UnicodeString skel((UBool)(skeletonLength == -1), skeleton, skeletonLength);
    LocalPointer<DateIntervalFormat> formatter(
            DateIntervalFormat::createInstance(skel, Locale(locale), *status));
    if (U_FAILURE(*status)) {
        return NULL;
    }
    if(tzID != 0) {
        TimeZone *zone = TimeZone::createTimeZone(UnicodeString((UBool)(tzIDLength == -1), tzID, tzIDLength));
        if(zone == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        formatter->adoptTimeZone(zone);
    }
    return (UDateIntervalFormat*)formatter.orphan();
}


U_CAPI void U_EXPORT2
udtitvfmt_close(UDateIntervalFormat *formatter)
{
    delete (DateIntervalFormat*)formatter;
}


U_CAPI int32_t U_EXPORT2
udtitvfmt_format(const UDateIntervalFormat* formatter,
                 UDate           fromDate,
                 UDate           toDate,
                 UChar*          result,
                 int32_t         resultCapacity,
                 UFieldPosition* position,
                 UErrorCode*     status)
{
    if (U_FAILURE(*status)) {
        return -1;
    }
    if (result == NULL ? resultCapacity != 0 : resultCapacity < 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString res;
    if (result != NULL) {
        // NULL destination for pure preflighting: empty dummy string
        // otherwise, alias the destination buffer (copied from udat_format)
        res.setTo(result, 0, resultCapacity);
    }
    FieldPosition fp;
    if (position != 0) {
        fp.setField(position->field);
    }

    DateInterval interval = DateInterval(fromDate,toDate);
    ((const DateIntervalFormat*)formatter)->format( &interval, res, fp, *status );
    if (U_FAILURE(*status)) {
        return -1;
    }
    if (position != 0) {
        position->beginIndex = fp.getBeginIndex();
        position->endIndex = fp.getEndIndex();
    }

    return res.extract(result, resultCapacity, *status);
}

U_CAPI void U_EXPORT2
udtitvfmt_setAttribute(UDateIntervalFormat*             formatter,
                      UDateIntervalFormatAttribute      attr,
                      UDateIntervalFormatAttributeValue value,
                      UErrorCode*                       status)
{
    if (U_FAILURE(*status)) {
        return;
    }
    ((DateIntervalFormat*)formatter)->setAttribute( attr, value, *status );
}

#endif /* #if !UCONFIG_NO_FORMATTING */