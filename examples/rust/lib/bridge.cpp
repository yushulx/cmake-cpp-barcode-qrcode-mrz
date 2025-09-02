#include "bridge.h"
#include <cstring>
#include <cstdlib>
#include <stdio.h>

#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftUtility.h"

using namespace std;
using namespace dynamsoft::license;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::utility;
using namespace dynamsoft::basic_structures;

Barcode *create_barcode(const char *type, const char *value, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4)
{
    Barcode *barcode = (Barcode *)std::malloc(sizeof(Barcode));
    barcode->barcode_type = _strdup(type);
    barcode->barcode_value = _strdup(value);
    barcode->x1 = x1;
    barcode->y1 = y1;
    barcode->x2 = x2;
    barcode->y2 = y2;
    barcode->x3 = x3;
    barcode->y3 = y3;
    barcode->x4 = x4;
    barcode->y4 = y4;
    return barcode;
}

void free_barcode(BarcodeResults *results)
{
    for (int i = 0; i < results->count; i++)
    {
        std::free((void *)results->barcodes[i].barcode_type);
        std::free((void *)results->barcodes[i].barcode_value);
    }
    std::free(results->barcodes);
    std::free(results);
}

int init_license(const char *license)
{
    int iRet = -1;
    char szErrorMsg[256];
    // Initialize license.
    // Request a trial from https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform
    iRet = CLicenseManager::InitLicense(license, szErrorMsg, 256);
    if (iRet != EC_OK)
    {
        printf("%s\n", szErrorMsg);
    }

    return iRet;
}

BarcodeResults *decode_barcode_file(void *instance, const char *filename)
{
    CCaptureVisionRouter *cvr = (CCaptureVisionRouter *)instance;
    BarcodeResults *all_barcodes = NULL;

    CCapturedResult *result = cvr->Capture(filename, CPresetTemplate::PT_READ_BARCODES);
    if (result && result->GetErrorCode() == 0)
    {
        CDecodedBarcodesResult *barcodeResult = result->GetDecodedBarcodesResult();
        if (barcodeResult)
        {
            int count = barcodeResult->GetItemsCount();
            if (count > 0)
            {
                all_barcodes = (BarcodeResults *)std::malloc(sizeof(BarcodeResults));
                all_barcodes->count = count;
                all_barcodes->barcodes = (Barcode *)std::malloc(sizeof(Barcode) * count);

                for (int i = 0; i < count; i++)
                {
                    const CBarcodeResultItem *barcodeItem = barcodeResult->GetItem(i);
                    if (barcodeItem)
                    {
                        CQuadrilateral location = barcodeItem->GetLocation();
                        CPoint *points = location.points;

                        const char *text = barcodeItem->GetText();
                        const char *format = barcodeItem->GetFormatString();

                        Barcode *barcode = create_barcode(
                            format ? format : "Unknown",
                            text ? text : "",
                            (int)points[0][0], (int)points[0][1],
                            (int)points[1][0], (int)points[1][1],
                            (int)points[2][0], (int)points[2][1],
                            (int)points[3][0], (int)points[3][1]);
                        all_barcodes->barcodes[i] = *barcode;
                        std::free(barcode);
                    }
                }
            }
        }
    }

    if (result)
    {
        result->Release();
    }

    return all_barcodes;
}

void *CVR_CreateInstance()
{
    return new CCaptureVisionRouter();
}

void CVR_DestroyInstance(void *instance)
{
    if (instance)
    {
        delete (CCaptureVisionRouter *)instance;
    }
}