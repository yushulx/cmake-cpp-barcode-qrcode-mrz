#include "decoder_adapter.h"
#include "normalization.h"

#include <DynamsoftCaptureVisionRouter.h>
#include <DynamsoftUtility.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace dynamsoft::basic_structures;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::license;

namespace bench {
namespace {

class DynamsoftDecoder final : public IDecoderAdapter {
public:
    DynamsoftDecoder(const std::string& template_path,
                     std::string template_name,
                     const std::string& license_key,
                     int expected_barcodes_count)
        : template_name_(std::move(template_name)),
          expected_barcodes_count_(expected_barcodes_count)
    {
        char error[1024] = {};
        const int license_result = CLicenseManager::InitLicense(license_key.c_str(), error, sizeof(error));
        if (license_result != EC_OK) throw std::runtime_error(std::string("Dynamsoft license initialization failed: ") + error);
        router_ = std::make_unique<CCaptureVisionRouter>();

        if (!template_path.empty()) {
            int result = router_->InitSettingsFromFile(template_path.c_str(), error, sizeof(error));
            if (result != EC_OK) throw std::runtime_error(std::string("Dynamsoft template initialization failed: ") + error);

            // The Linux build of DCV exposes the official preset templates but
            // does not always support the simplified-settings API on them. In
            // that case run the preset template unchanged instead of aborting.
            SimplifiedCaptureVisionSettings settings{};
            result = router_->GetSimplifiedSettings(template_name_.c_str(), &settings);
            if (result != EC_OK) {
                std::cerr << "Dynamsoft GetSimplifiedSettings unavailable (" << result
                          << "); using the preset template unchanged\n";
            } else {
                settings.barcodeSettings.barcodeFormatIds = BF_ALL;
                settings.barcodeSettings.maxThreadsInOneTask = 1;
                // Do not invent a barcode-count hint. A dataset-level maximum
                // (the manifest holds images with 14 barcodes) makes the decoder
                // search every 1-barcode image as if 14 were present: measured on
                // one image, expected=14 costs 11.7x the time of expected=1. The
                // Web harness never sets it either, so leaving the template's own
                // value is both faster and the only apples-to-apples setting.
                if (expected_barcodes_count_ >= 0)
                    settings.barcodeSettings.expectedBarcodesCount = expected_barcodes_count_;
                result = router_->UpdateSettings(template_name_.c_str(), &settings, error, sizeof(error));
                if (result != EC_OK)
                    std::cerr << "Dynamsoft UpdateSettings failed: " << error
                              << "; using the preset template unchanged\n";
            }
        }
    }

    ~DynamsoftDecoder() override
    {
        // Capture() is synchronous, but the SDK can retain worker state after a
        // call. Stop it explicitly before destroying the router so repeated
        // benchmark/build cycles do not leave the runtime DLLs locked.
        if (router_) router_->StopCapturing(false, true);
    }

    std::string name() const override { return "dynamsoft-dbr"; }
    std::string version() const override { return CBarcodeReaderModule::GetVersion(); }

    DecodeRun decode(const ImageBuffer& image) override
    {
        DecodeRun run;
        CImageData input(image.rgb.size(), image.rgb.data(), image.width, image.height,
                         image.stride, IPF_RGB_888);
        const auto begin = std::chrono::steady_clock::now();
        CCapturedResult* captured = router_->Capture(&input, template_name_.c_str());
        run.decode_time = std::chrono::steady_clock::now() - begin;
        if (!captured) {
            run.error = "Dynamsoft returned a null captured result";
            return run;
        }
        if (captured->GetErrorCode() != EC_OK) {
            run.error = captured->GetErrorString() ? captured->GetErrorString() : "Dynamsoft decode error";
            captured->Release();
            return run;
        }
        CDecodedBarcodesResult* decoded = captured->GetDecodedBarcodesResult();
        if (decoded) {
            if (decoded->GetErrorCode() != EC_OK)
                run.error = decoded->GetErrorString() ? decoded->GetErrorString() : "Dynamsoft barcode result error";
            for (int i = 0; i < decoded->GetItemsCount(); ++i) {
                const auto* item = decoded->GetItem(i);
                DecodedBarcode result;
                result.canonical_format = canonicalFormat(item->GetFormatString() ? item->GetFormatString() : "");
                result.text = item->GetText() ? item->GetText() : "";
                const auto* bytes = item->GetBytes();
                if (bytes && item->GetBytesLength() > 0)
                    result.raw_bytes.assign(bytes, bytes + item->GetBytesLength());
                result.confidence = item->GetConfidence();
                run.results.push_back(std::move(result));
            }
            decoded->Release();
        }
        captured->Release();
        return run;
    }

private:
    std::string template_name_;
    int expected_barcodes_count_;
    std::unique_ptr<CCaptureVisionRouter> router_;
};

} // namespace

std::unique_ptr<IDecoderAdapter> createDynamsoftDecoder(const std::string& template_path,
                                                        const std::string& template_name,
                                                        const std::string& license_key,
                                                        int expected_barcodes_count)
{
    return std::make_unique<DynamsoftDecoder>(template_path, template_name, license_key,
                                              expected_barcodes_count);
}

} // namespace bench
