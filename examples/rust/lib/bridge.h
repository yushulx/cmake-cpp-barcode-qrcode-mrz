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

    Barcode *create_barcode(const char *type, const char *value, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);
    BarcodeResults *decode_barcode_file(void *instance, const char *filename);
    void free_barcode(BarcodeResults *results);
    int init_license(const char *license);
    void *CVR_CreateInstance();
    void CVR_DestroyInstance(void *instance);

#ifdef __cplusplus
}
#endif

#endif // BRIDGE_H
