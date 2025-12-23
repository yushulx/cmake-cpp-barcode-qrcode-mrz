#include <iostream>
#include "bridge.h"

C_API int DBR_InitLicense(const char *pLicense, char errorMsgBuffer[], const int errorMsgBufferLen)
{
    return CLicenseManager::InitLicense(pLicense, errorMsgBuffer, 512);
}

C_API void *DBR_CreateInstance()
{
    BarcodeReader *barcodeReader = new BarcodeReader;
    CCaptureVisionRouter *cvr = new CCaptureVisionRouter;
    barcodeReader->cvr = cvr;
    barcodeReader->result = NULL;
    return (void *)barcodeReader;
}

C_API void DBR_DestroyInstance(void *barcodeReader)
{
    if (barcodeReader != NULL)
    {
        BarcodeReader *reader = (BarcodeReader *)barcodeReader;
        if (reader->cvr != NULL)
        {
            delete reader->cvr;
            reader->cvr = NULL;
        }

        if (reader->result != NULL)
        {
            reader->result->Release();
            reader->result = NULL;
        }

        delete barcodeReader;
        barcodeReader = NULL;
    }
}

C_API int DBR_DecodeFile(void *barcodeReader, const char *pFileName, const char *pTemplateName)
{
    BarcodeReader *reader = (BarcodeReader *)barcodeReader;
    if (!reader || !reader->cvr)
        return -1;

    CCapturedResult *result = reader->cvr->Capture(pFileName, CPresetTemplate::PT_READ_BARCODES);
    if (result == NULL)
        return -1;

    int errorCode = result->GetErrorCode();
    if (errorCode != 0)
    {
        cout << "Error: " << errorCode << "," << result->GetErrorString() << endl;
    }

    if (reader->result != NULL)
    {
        reader->result->Release();
    }
    reader->result = result;

    return errorCode;
}

C_API const char *DBR_GetVersion()
{
    return CBarcodeReaderModule::GetVersion();
}

C_API int DBR_GetAllTextResults(void *barcodeReader, TextResultArray **pResults)
{
    BarcodeReader *reader = (BarcodeReader *)barcodeReader;
    if (!reader || !reader->cvr || !reader->result)
        return -1;

    CCapturedResult *result = reader->result;

    int capturedResultItemCount = result->GetItemsCount();
    if (capturedResultItemCount == 0)
        return -1;

    TextResultArray *textResults = (TextResultArray *)calloc(1, sizeof(TextResultArray));
    textResults->resultsCount = capturedResultItemCount;
    textResults->results = (TextResult *)calloc(capturedResultItemCount, sizeof(TextResult));
    *pResults = textResults;

    for (int j = 0; j < capturedResultItemCount; j++)
    {
        const CCapturedResultItem *capturedResultItem = result->GetItem(j);
        CapturedResultItemType type = capturedResultItem->GetType();
        if (type == CapturedResultItemType::CRIT_BARCODE)
        {
            const CBarcodeResultItem *barcodeResultItem = dynamic_cast<const CBarcodeResultItem *>(capturedResultItem);
            // cout << "Result " << j + 1 << endl;
            // cout << "Barcode Format: " << barcodeResultItem->GetFormatString() << endl;
            // cout << "Barcode Text: " << barcodeResultItem->GetText() << endl;
            char *barcodeFormatString = (char *)barcodeResultItem->GetFormatString();
            char *barcodeText = (char *)barcodeResultItem->GetText();
            textResults->results[j].barcodeFormatString = (char *)malloc(strlen(barcodeFormatString) + 1);
            strcpy(textResults->results[j].barcodeFormatString, barcodeFormatString);
            textResults->results[j].barcodeText = (char *)malloc(strlen(barcodeText) + 1);
            strcpy(textResults->results[j].barcodeText, barcodeText);
        }
    }

    result->Release();
    reader->result = NULL;

    return 0;
}

C_API void DBR_FreeTextResults(TextResultArray **pResults)
{
    if (pResults)
    {
        if (*pResults)
        {
            if ((*pResults)->results)
            {
                for (int i = 0; i < (*pResults)->resultsCount; i++)
                {
                    if ((*pResults)->results[i].barcodeFormatString)
                    {
                        free((*pResults)->results[i].barcodeFormatString);
                    }
                    if ((*pResults)->results[i].barcodeText)
                    {
                        free((*pResults)->results[i].barcodeText);
                    }
                }
                free((*pResults)->results);
            }
        }
    }
}