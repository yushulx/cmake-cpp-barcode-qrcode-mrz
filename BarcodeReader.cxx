#include "bridge.h"

#include <iostream>
#include <vector>
#include <string>
#include <chrono>

void decodeImage(void *barcodeReader, const std::string &imagePath)
{
    std::cout << "----------------------------------------------------------" << std::endl;
    std::cout << "Decoding: " << imagePath << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    int errorCode = DBR_DecodeFile(barcodeReader, imagePath.c_str(), "");
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    if (errorCode != 0)
    {
        std::cout << "Failed to read barcode: " << errorCode << std::endl;
        return;
    }

    TextResultArray *paryResult = NULL;
    DBR_GetAllTextResults(barcodeReader, &paryResult);

    if (paryResult)
    {
        std::cout << "Barcode count: " << paryResult->resultsCount << " (Time: " << elapsed.count() << "s)" << std::endl;

        for (int index = 0; index < paryResult->resultsCount; index++)
        {
            printf("Barcode %d:\n", index + 1);
            printf("    Type: %s\n", paryResult->results[index].barcodeFormatString);
            printf("    Text: %s\n", paryResult->results[index].barcodeText);
        }

        DBR_FreeTextResults(&paryResult);
    }
    else
    {
        std::cout << "No barcodes found." << std::endl;
    }
}

int main(int argc, char *argv[])
{
    std::cout << "Version: " << DBR_GetVersion() << std::endl;
    char errorMsgBuffer[512];
    int errorCode = DBR_InitLicense("DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ==", errorMsgBuffer, 512);
    if (errorCode != 0)
    {
        std::cout << "InitLicense error: " << errorMsgBuffer << std::endl;
    }

    void *barcodeReader = DBR_CreateInstance();

    std::vector<std::string> images;
    if (argc > 1)
    {
        for (int i = 1; i < argc; i++)
        {
            images.push_back(argv[i]);
        }
    }
    else
    {
        // Default images if no arguments provided
        images = {
            "../images/UPC-E.jpg",
            "../images/QR-Blurred.jpg",
            "../images/AllSupportedBarcodeTypes.png",
            "../images/multi-barcode.jpg"};
    }

    for (const auto &imagePath : images)
    {
        decodeImage(barcodeReader, imagePath);
    }

    DBR_DestroyInstance(barcodeReader);
    return 0;
}