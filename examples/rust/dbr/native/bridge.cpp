#include "bridge.h"

#include <cstdlib>
#include <cstring>

#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftUtility.h"

using namespace dynamsoft::license;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::utility;
using namespace dynamsoft::basic_structures;

static char *dup_string(const char *s)
{
    if (!s)
        s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)std::malloc(n);
    if (p)
        std::memcpy(p, s, n);
    return p;
}

static void set_error(DbrResults *results, int code, const char *message)
{
    results->error_code = code;
    if (message)
    {
        std::strncpy(results->error_message, message, sizeof(results->error_message) - 1);
        results->error_message[sizeof(results->error_message) - 1] = '\0';
    }
    else
    {
        results->error_message[0] = '\0';
    }
}

int dbr_init_license(const char *license, char *error_message, int error_message_capacity)
{
    char buffer[256] = {0};
    int ret = CLicenseManager::InitLicense(license, buffer, (int)sizeof(buffer));
    if (ret != EC_OK && error_message && error_message_capacity > 0)
    {
        std::strncpy(error_message, buffer, error_message_capacity - 1);
        error_message[error_message_capacity - 1] = '\0';
    }
    return ret;
}

void *dbr_create_instance(void)
{
    return new CCaptureVisionRouter();
}

void dbr_destroy_instance(void *instance)
{
    delete (CCaptureVisionRouter *)instance;
}

static DbrResults *extract_results(CCapturedResult *result)
{
    DbrResults *out = (DbrResults *)std::calloc(1, sizeof(DbrResults));
    if (!out)
    {
        if (result)
            result->Release();
        return NULL;
    }

    if (!result)
    {
        set_error(out, -1, "Capture returned no result");
        return out;
    }

    int code = result->GetErrorCode();
    if (code != 0)
    {
        set_error(out, code, result->GetErrorString());
        result->Release();
        return out;
    }

    CDecodedBarcodesResult *decoded = result->GetDecodedBarcodesResult();
    if (decoded)
    {
        int count = decoded->GetItemsCount();
        if (count > 0)
        {
            out->barcodes = (DbrBarcode *)std::calloc(count, sizeof(DbrBarcode));
            if (!out->barcodes)
            {
                set_error(out, -2, "Out of memory");
                result->Release();
                return out;
            }
            out->count = count;

            for (int i = 0; i < count; i++)
            {
                DbrBarcode *barcode = &out->barcodes[i];
                const CBarcodeResultItem *item = decoded->GetItem(i);
                if (!item)
                {
                    barcode->format = dup_string("Unknown");
                    barcode->text = dup_string("");
                    continue;
                }

                CQuadrilateral location = item->GetLocation();
                barcode->format = dup_string(item->GetFormatString());
                barcode->text = dup_string(item->GetText());
                barcode->x1 = (int)location.points[0][0];
                barcode->y1 = (int)location.points[0][1];
                barcode->x2 = (int)location.points[1][0];
                barcode->y2 = (int)location.points[1][1];
                barcode->x3 = (int)location.points[2][0];
                barcode->y3 = (int)location.points[2][1];
                barcode->x4 = (int)location.points[3][0];
                barcode->y4 = (int)location.points[3][1];
            }
        }
    }

    result->Release();
    return out;
}

DbrResults *dbr_decode_file(void *instance, const char *path)
{
    CCaptureVisionRouter *cvr = (CCaptureVisionRouter *)instance;
    CCapturedResult *result = cvr->Capture(path, CPresetTemplate::PT_READ_BARCODES);
    return extract_results(result);
}

DbrResults *dbr_decode_buffer(void *instance, const unsigned char *bytes, int width, int height, int stride)
{
    CCaptureVisionRouter *cvr = (CCaptureVisionRouter *)instance;
    CImageData image((unsigned long long)stride * height, bytes, width, height, stride, IPF_RGB_888);
    CCapturedResult *result = cvr->Capture(&image, CPresetTemplate::PT_READ_BARCODES);
    return extract_results(result);
}

void dbr_free_results(DbrResults *results)
{
    if (!results)
        return;
    for (int i = 0; i < results->count; i++)
    {
        std::free((void *)results->barcodes[i].format);
        std::free((void *)results->barcodes[i].text);
    }
    std::free(results->barcodes);
    std::free(results);
}