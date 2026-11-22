#ifndef BRIDGE_H
#define BRIDGE_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        const char *barcode_type;
        const char *barcode_value;
        int x1;
        int y1;
        int x2;
        int y2;
        int x3;
        int y3;
        int x4;
        int y4;
    } Barcode;

    typedef struct
    {
        Barcode *barcodes;
        int count;
    } BarcodeResults;

    typedef struct
    {
        int width;
        int height;
        unsigned char *rgb; // width * height * 3 bytes, RGB888
        int rgb_length;
        Barcode *barcodes; // barcodes on this page; NULL when count is 0
        int count;
    } PageResult;

    typedef struct
    {
        PageResult *pages;
        int count;
    } PageResults;

    Barcode *create_barcode(const char *type, const char *value, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);
    BarcodeResults *decode_barcode_file(void *instance, const char *filename);
    PageResults *decode_barcode_file_pages(void *instance, const char *filename);
    BarcodeResults *decode_barcode_buffer(void *instance, const unsigned char *bytes, int width, int height, int stride);
    void free_barcode(BarcodeResults *results);
    void free_page_results(PageResults *results);
    int init_license(const char *license);
    void *CVR_CreateInstance();
    void CVR_DestroyInstance(void *instance);

#ifdef __cplusplus
}
#endif

#endif // BRIDGE_H
