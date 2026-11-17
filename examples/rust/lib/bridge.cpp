#include "bridge.h"
#include <cstring>
#include <cstdlib>
#include <stdio.h>
#include <vector>

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

static BarcodeResults *extract_barcodes(CCapturedResult *result)
{
    BarcodeResults *all_barcodes = NULL;
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

static void append_barcode_items(CDecodedBarcodesResult *barcodeResult, std::vector<Barcode> &items)
{
    if (!barcodeResult || barcodeResult->GetErrorCode() != 0)
    {
        return;
    }
    int count = barcodeResult->GetItemsCount();
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
            items.push_back(*barcode);
            std::free(barcode);
        }
    }
}

BarcodeResults *decode_barcode_file(void *instance, const char *filename)
{
    CCaptureVisionRouter *cvr = (CCaptureVisionRouter *)instance;
    // CaptureMultiPages rasterizes PDFs and multi-page TIFFs into one result per
    // page; the single-page Capture() does not, so PDFs would return no barcodes.
    CCapturedResultArray *array = cvr->CaptureMultiPages(filename, CPresetTemplate::PT_READ_BARCODES);

    std::vector<Barcode> items;
    if (array)
    {
        int count = array->GetResultsCount();
        for (int i = 0; i < count; i++)
        {
            const CCapturedResult *result = array->GetResult(i);
            if (!result || result->GetErrorCode() != 0)
            {
                continue;
            }
            CDecodedBarcodesResult *barcodeResult = result->GetDecodedBarcodesResult();
            append_barcode_items(barcodeResult, items);
            if (barcodeResult)
            {
                barcodeResult->Release();
            }
        }
        array->Release();
    }

    if (items.empty())
    {
        return NULL;
    }

    BarcodeResults *all_barcodes = (BarcodeResults *)std::malloc(sizeof(BarcodeResults));
    all_barcodes->count = (int)items.size();
    all_barcodes->barcodes = (Barcode *)std::malloc(sizeof(Barcode) * items.size());
    for (size_t i = 0; i < items.size(); i++)
    {
        all_barcodes->barcodes[i] = items[i];
    }
    return all_barcodes;
}

// Copies a page image into a freshly allocated RGB888 buffer. The SDK keeps the
// original page image it rasterized (PDF pages included); GetOriginalImage()
// hands it back so the GUI can show exactly what was decoded.
static unsigned char *image_to_rgb(const CImageData *image, int *out_length)
{
    if (!image)
    {
        return NULL;
    }
    int width = image->GetWidth();
    int height = image->GetHeight();
    int stride = image->GetStride();
    const unsigned char *src = image->GetBytes();
    if (width <= 0 || height <= 0 || !src)
    {
        return NULL;
    }

    ImagePixelFormat format = image->GetImagePixelFormat();
    bool supported = format == IPF_RGB_888 || format == IPF_BGR_888 ||
                     format == IPF_GRAYSCALED || format == IPF_BINARY_8 ||
                     format == IPF_BINARY_8_INVERTED || format == IPF_ARGB_8888 ||
                     format == IPF_ABGR_8888 || format == IPF_BINARY ||
                     format == IPF_BINARYINVERTED;
    if (!supported)
    {
        return NULL;
    }

    int length = width * height * 3;
    unsigned char *rgb = (unsigned char *)std::malloc(length);
    if (!rgb)
    {
        return NULL;
    }

    for (int y = 0; y < height; y++)
    {
        const unsigned char *row = src + (size_t)y * stride;
        unsigned char *dst = rgb + (size_t)y * width * 3;
        for (int x = 0; x < width; x++)
        {
            unsigned char r = 0, g = 0, b = 0;
            switch (format)
            {
            case IPF_RGB_888:
            case IPF_BGR_888:
            {
                // CFileFetcher returns BGR bytes for both labels on this SDK build
                // (verified with a red/green/blue test image), so always swap.
                const unsigned char *p = row + x * 3;
                r = p[2];
                g = p[1];
                b = p[0];
                break;
            }
            case IPF_GRAYSCALED:
            case IPF_BINARY_8:
                r = g = b = row[x];
                break;
            case IPF_BINARY_8_INVERTED:
                r = g = b = (unsigned char)(255 - row[x]);
                break;
            case IPF_ARGB_8888:
            {
                const unsigned char *p = row + x * 4;
                r = p[1];
                g = p[2];
                b = p[3];
                break;
            }
            case IPF_ABGR_8888:
            {
                const unsigned char *p = row + x * 4;
                r = p[3];
                g = p[2];
                b = p[1];
                break;
            }
            case IPF_BINARY:
            case IPF_BINARYINVERTED:
            {
                int bit = (row[x >> 3] >> (7 - (x & 7))) & 1;
                bool white = format == IPF_BINARY ? bit == 1 : bit == 0;
                r = g = b = white ? 255 : 0;
                break;
            }
            default:
                break;
            }
            dst[x * 3] = r;
            dst[x * 3 + 1] = g;
            dst[x * 3 + 2] = b;
        }
    }

    *out_length = length;
    return rgb;
}

PageResults *decode_barcode_file_pages(void *instance, const char *filename)
{
    CCaptureVisionRouter *cvr = (CCaptureVisionRouter *)instance;
    // CFileFetcher splits a multi-page PDF or TIFF into one CImageData per page.
    // Rasterizing through it lets the GUI preview the very bitmap that gets decoded.
    CFileFetcher fetcher;
    if (fetcher.SetFile(filename) != EC_OK)
    {
        return NULL;
    }

    std::vector<PageResult> pages;
    while (fetcher.HasNextImageToFetch())
    {
        CImageData *image = fetcher.GetImage();
        if (!image)
        {
            break;
        }

        PageResult page;
        page.width = 0;
        page.height = 0;
        page.rgb = image_to_rgb(image, &page.rgb_length);
        if (page.rgb)
        {
            page.width = image->GetWidth();
            page.height = image->GetHeight();
        }
        page.barcodes = NULL;
        page.count = 0;

        CCapturedResult *result = cvr->Capture(image, CPresetTemplate::PT_READ_BARCODES);
        if (result)
        {
            if (result->GetErrorCode() == 0)
            {
                CDecodedBarcodesResult *barcodeResult = result->GetDecodedBarcodesResult();
                std::vector<Barcode> items;
                append_barcode_items(barcodeResult, items);
                if (barcodeResult)
                {
                    barcodeResult->Release();
                }
                if (!items.empty())
                {
                    page.count = (int)items.size();
                    page.barcodes = (Barcode *)std::malloc(sizeof(Barcode) * items.size());
                    for (size_t k = 0; k < items.size(); k++)
                    {
                        page.barcodes[k] = items[k];
                    }
                }
            }
            result->Release();
        }

        if (page.rgb || page.count > 0)
        {
            pages.push_back(page);
        }
        else if (page.barcodes)
        {
            std::free(page.barcodes);
        }
    }

    if (pages.empty())
    {
        return NULL;
    }

    PageResults *results = (PageResults *)std::malloc(sizeof(PageResults));
    results->count = (int)pages.size();
    results->pages = (PageResult *)std::malloc(sizeof(PageResult) * pages.size());
    for (size_t i = 0; i < pages.size(); i++)
    {
        results->pages[i] = pages[i];
    }
    return results;
}

void free_page_results(PageResults *results)
{
    if (!results)
    {
        return;
    }
    for (int i = 0; i < results->count; i++)
    {
        PageResult *page = &results->pages[i];
        for (int j = 0; j < page->count; j++)
        {
            std::free((void *)page->barcodes[j].barcode_type);
            std::free((void *)page->barcodes[j].barcode_value);
        }
        if (page->barcodes)
        {
            std::free(page->barcodes);
        }
        if (page->rgb)
        {
            std::free(page->rgb);
        }
    }
    std::free(results->pages);
    std::free(results);
}

BarcodeResults *decode_barcode_buffer(void *instance, const unsigned char *bytes, int width, int height, int stride)
{
    CCaptureVisionRouter *cvr = (CCaptureVisionRouter *)instance;
    CImageData image((unsigned long long)stride * height, bytes, width, height, stride, IPF_RGB_888);
    CCapturedResult *result = cvr->Capture(&image, CPresetTemplate::PT_READ_BARCODES);
    return extract_barcodes(result);
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