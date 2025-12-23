#ifndef C_BRIDGING_H
#define C_BRIDGING_H

#include <iostream>
#include <string>

#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftUtility.h"

using namespace std;
using namespace dynamsoft::license;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::utility;
using namespace dynamsoft::basic_structures;

#define C_API __declspec(dllexport)
typedef struct
{
    CCaptureVisionRouter *cvr;
    CCapturedResult *result;
} BarcodeReader;

typedef struct
{

    /**Barcode type in BarcodeFormat group 1 as string */
    char *barcodeFormatString;

    /**The barcode text, ends by '\0' */
    char *barcodeText;

    /**Reserved memory for the struct. The length of this array indicates the size of the memory reserved for this struct. */
    char reserved[44];
} TextResult;

typedef struct
{
    /**The total count of text result */
    int resultsCount;

    /**The text result array */
    TextResult *results;
} TextResultArray;

#ifdef __cplusplus
extern "C"
{
#endif

    C_API int DBR_InitLicense(const char *pLicense, char errorMsgBuffer[], const int errorMsgBufferLen);
    C_API int DBR_DecodeFile(void *barcodeReader, const char *pFileName, const char *pTemplateName);
    C_API void *DBR_CreateInstance();
    C_API void DBR_DestroyInstance(void *barcodeReader);
    C_API const char *DBR_GetVersion();
    C_API int DBR_GetAllTextResults(void *barcodeReader, TextResultArray **pResults);
    C_API void DBR_FreeTextResults(TextResultArray **pResults);
#ifdef __cplusplus
}
#endif

#endif