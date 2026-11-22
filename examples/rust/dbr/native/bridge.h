#ifndef DBR_BRIDGE_H
#define DBR_BRIDGE_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        const char *format;
        const char *text;
        int x1;
        int y1;
        int x2;
        int y2;
        int x3;
        int y3;
        int x4;
        int y4;
    } DbrBarcode;

    typedef struct
    {
        DbrBarcode *barcodes;
        int count;
        int error_code;
        char error_message[256];
    } DbrResults;

    int dbr_init_license(const char *license, char *error_message, int error_message_capacity);
    void *dbr_create_instance(void);
    void dbr_destroy_instance(void *instance);
    DbrResults *dbr_decode_file(void *instance, const char *path);
    DbrResults *dbr_decode_buffer(void *instance, const unsigned char *bytes, int width, int height, int stride);
    void dbr_free_results(DbrResults *results);

#ifdef __cplusplus
}
#endif

#endif