#include "decoder_adapter.h"
#include "normalization.h"

#include <nlohmann/json.hpp>

#include <Scandit/ScBarcode.h>
#include <Scandit/ScBarcodeArray.h>
#include <Scandit/ScBarcodeScanner.h>
#include <Scandit/ScBarcodeScannerSession.h>
#include <Scandit/ScBarcodeScannerSettings.h>
#include <Scandit/ScByteArray.h>
#include <Scandit/ScCommon.h>
#include <Scandit/ScConfig.h>
#include <Scandit/ScImageDescription.h>
#include <Scandit/ScRecognitionContext.h>
#include <Scandit/ScSymbologySettings.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace bench {
namespace {

struct ScanditConfig {
    bool multi_scan = true;
    bool color_inverted = true;
    std::uint32_t max_locations_cap = 64;
    std::vector<std::string> symbologies;
};

ScanditConfig readConfig(const std::string& path)
{
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read Scandit settings: " + path);
    const auto value = nlohmann::json::parse(input);
    ScanditConfig config;
    config.multi_scan = value.value("multi_scan", true);
    config.color_inverted = value.value("color_inverted", true);
    config.max_locations_cap = value.value("max_locations_per_frame_cap", 64u);
    for (const auto& entry : value.at("symbologies"))
        config.symbologies.push_back(entry.get<std::string>());
    if (config.symbologies.empty())
        throw std::runtime_error("Scandit settings contain no symbologies: " + path);
    return config;
}

// The message is an ScByteArray owned by the status; copy it before freeing.
std::string statusMessage(ScContextStatus status)
{
    if (!status.message.str) return {};
    return std::string(status.message.str, sc_byte_array_get_size(status.message));
}

// Scandit returns the raw barcode bytes. Some codes carry binary or Latin-1
// payloads that are not valid UTF-8, while the benchmark's matching and JSONL
// writer both expect UTF-8 text. Keep valid UTF-8 sequences intact and encode
// every stray byte as its Latin-1 code point, so no information is dropped and
// the resulting string is always serializable.
std::string toValidUtf8(std::string_view input)
{
    std::string output;
    output.reserve(input.size());
    for (std::size_t i = 0; i < input.size();) {
        const auto c = static_cast<unsigned char>(input[i]);
        if (c < 0x80) { output.push_back(static_cast<char>(c)); ++i; continue; }
        std::size_t length = 0;
        if ((c & 0xE0) == 0xC0) length = 2;
        else if ((c & 0xF0) == 0xE0) length = 3;
        else if ((c & 0xF8) == 0xF0) length = 4;
        bool valid = length >= 2 && length <= 4 && i + length <= input.size()
                     && !(length == 2 && c < 0xC2);
        if (valid) {
            for (std::size_t k = 1; k < length; ++k)
                if ((static_cast<unsigned char>(input[i + k]) & 0xC0) != 0x80) { valid = false; break; }
        }
        if (valid) {
            output.append(input.substr(i, length));
            i += length;
        } else {
            output.push_back(static_cast<char>(0xC0 | (c >> 6)));
            output.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            ++i;
        }
    }
    return output;
}

struct ApplyResult {
    bool ok = false;
    int flag = 0;
    std::string message;
};

ApplyResult applySettings(ScBarcodeScanner* scanner, ScBarcodeScannerSettings* settings)
{
    const ScContextStatus status = sc_barcode_scanner_apply_settings(scanner, settings);
    ApplyResult result{status.flag == SC_RECOGNITION_CONTEXT_STATUS_SUCCESS,
                       static_cast<int>(status.flag), statusMessage(status)};
    sc_context_status_free(status);
    return result;
}

ScBarcodeScannerSettings* makeSettings(const ScanditConfig& config, int max_symbols, bool multi_scan)
{
    auto* settings = sc_barcode_scanner_settings_new_with_preset(SC_PRESET_HIGH_EFFORT);
    if (!settings) throw std::runtime_error("Scandit settings allocation failed");

    for (const auto& name : config.symbologies) {
        const ScSymbology symbology = sc_symbology_from_string(name.c_str());
        if (symbology == SC_SYMBOLOGY_UNKNOWN)
            throw std::runtime_error("Scandit does not know symbology: " + name);
        sc_barcode_scanner_settings_set_symbology_enabled(settings, symbology, SC_TRUE);
        if (config.color_inverted) {
            if (auto* symbology_settings =
                    sc_barcode_scanner_settings_get_symbology_settings(settings, symbology))
                sc_symbology_settings_set_color_inverted_enabled(symbology_settings, SC_TRUE);
        }
    }

    // Still images: codes may be anywhere, in any orientation, and repeats are not filtered.
    sc_barcode_scanner_settings_set_code_duplicate_filter(settings, SC_DUPLICATE_FILTER_OFF);
    sc_barcode_scanner_settings_set_code_location_constraint_1d(settings, SC_CODE_LOCATION_IGNORE);
    sc_barcode_scanner_settings_set_code_location_constraint_2d(settings, SC_CODE_LOCATION_IGNORE);
    sc_barcode_scanner_settings_set_code_direction_hint(settings, SC_CODE_DIRECTION_NONE);

    if (multi_scan) {
        const std::uint32_t wanted = static_cast<std::uint32_t>(
            (std::max)(8, (std::min)(static_cast<int>(config.max_locations_cap), max_symbols * 2 + 8)));
        sc_barcode_scanner_settings_set_multi_scan_enabled(settings, SC_TRUE);
        sc_barcode_scanner_settings_set_max_num_locations_to_process_per_frame(
            settings, (std::max)(1u, wanted));
    }
    return settings;
}

class ScanditDecoder final : public IDecoderAdapter {
public:
    ScanditDecoder(const std::string& settings_path,
                   const std::string& license_key,
                   const std::string& writable_path,
                   int max_symbols)
    {
        const auto config = readConfig(settings_path);

        // One context and one scanner per process. A bad key does not fail here;
        // it is reported by apply_settings / process_frame with a status code.
        context_ = sc_recognition_context_new(license_key.c_str(), writable_path.c_str(), nullptr);
        if (!context_) throw std::runtime_error("Scandit recognition context creation failed");
        scanner_ = sc_barcode_scanner_new(context_);
        if (!scanner_) throw std::runtime_error("Scandit barcode scanner creation failed");

        settings_ = makeSettings(config, max_symbols, config.multi_scan);
        ApplyResult applied = applySettings(scanner_, settings_);
        if (!applied.ok && config.multi_scan) {
            // Multi-scan needs the "tracking" license feature. Trial keys do not
            // always include it, so fall back to single-code scanning and disclose it.
            std::cerr << "Scandit multi-scan unavailable (status " << applied.flag << ": "
                      << applied.message << "); falling back to single-code scanning\n";
            sc_barcode_scanner_settings_release(settings_);
            settings_ = makeSettings(config, max_symbols, false);
            applied = applySettings(scanner_, settings_);
            multi_scan_ = false;
        }
        if (!applied.ok)
            throw std::runtime_error("Scandit apply_settings failed: status " +
                                     std::to_string(applied.flag) + ": " + applied.message);
        if (!sc_barcode_scanner_wait_for_setup_completed(scanner_))
            throw std::runtime_error("Scandit scanner setup did not complete (check the Linux license key)");

        description_ = sc_image_description_new();
        if (!description_) throw std::runtime_error("Scandit image description creation failed");
    }

    ~ScanditDecoder() override
    {
        sc_image_description_release(description_);
        sc_barcode_scanner_release(scanner_);
        sc_barcode_scanner_settings_release(settings_);
        sc_recognition_context_release(context_);
    }

    std::string name() const override { return "scandit-linux"; }

    std::string version() const override
    {
        const char* value = sc_get_information_string(SC_INFORMATION_KEY_SDK_VERSION);
        return value ? value : SC_VERSION_STRING;
    }

    std::string configuration() const override
    {
        return multi_scan_ ? "multi-scan" : "single-code (tracking feature unavailable)";
    }

    DecodeRun decode(const ImageBuffer& image) override
    {
        DecodeRun run;
        sc_image_description_set_width(description_, static_cast<std::uint32_t>(image.width));
        sc_image_description_set_height(description_, static_cast<std::uint32_t>(image.height));
        sc_image_description_set_layout(description_, SC_IMAGE_LAYOUT_RGB_8U);
        sc_image_description_set_plane_row_bytes(description_, 0u,
                                                 static_cast<std::uint32_t>(image.stride));

        // A new frame sequence per independent still image. Only process_frame is
        // timed, matching the DBR and ZXing adapters, which time their SDK call only.
        sc_recognition_context_start_new_frame_sequence(context_);
        const auto begin = std::chrono::steady_clock::now();
        const ScProcessFrameResult result =
            sc_recognition_context_process_frame(context_, description_, image.rgb.data());
        run.decode_time = std::chrono::steady_clock::now() - begin;

        const bool ok = result.status.flag == SC_RECOGNITION_CONTEXT_STATUS_SUCCESS;
        std::string error;
        if (!ok)
            error = "Scandit process_frame failed: status " +
                    std::to_string(static_cast<int>(result.status.flag)) + ": " +
                    statusMessage(result.status);
        sc_process_frame_result_free(result);

        if (ok) {
            ScBarcodeScannerSession* session = sc_barcode_scanner_get_session(scanner_);
            ScBarcodeArray* codes = sc_barcode_scanner_session_get_newly_recognized_codes(session);
            const std::uint32_t count = codes ? sc_barcode_array_get_size(codes) : 0u;
            for (std::uint32_t i = 0; i < count; ++i) {
                const ScBarcode* code = sc_barcode_array_get_item_at(codes, i);
                if (!code) continue;
                const ScByteArray data = sc_barcode_get_data(code);
                const ScSymbology symbology = sc_barcode_get_symbology(code);
                const char* symbology_name = sc_symbology_to_string(symbology);

                DecodedBarcode decoded;
                std::string format_name = symbology_name ? symbology_name : "";
                // Scandit reports GS1-128 through the CODE128 symbology plus a
                // GS1 carrier flag. Expose it the same way DBR does.
                if (symbology == SC_SYMBOLOGY_CODE128 && sc_barcode_is_gs1_data_carrier(code))
                    format_name = "gs1-128";
                decoded.canonical_format = canonicalFormat(format_name);
                if (data.bytes && data.size > 0) {
                    decoded.text = toValidUtf8(
                        std::string_view(reinterpret_cast<const char*>(data.bytes), data.size));
                    decoded.raw_bytes.assign(data.bytes, data.bytes + data.size);
                }
                run.results.push_back(std::move(decoded));
            }
            if (codes) sc_barcode_array_release(codes);
            sc_barcode_scanner_session_clear(session);
        } else {
            run.error = std::move(error);
        }

        sc_recognition_context_end_frame_sequence(context_);
        return run;
    }

private:
    ScRecognitionContext* context_ = nullptr;
    ScBarcodeScanner* scanner_ = nullptr;
    ScBarcodeScannerSettings* settings_ = nullptr;
    ScImageDescription* description_ = nullptr;
    bool multi_scan_ = true;
};

} // namespace

std::unique_ptr<IDecoderAdapter> createScanditDecoder(const std::string& settings_path,
                                                      const std::string& license_key,
                                                      const std::string& writable_path,
                                                      int max_symbols)
{
    return std::make_unique<ScanditDecoder>(settings_path, license_key, writable_path, max_symbols);
}

} // namespace bench
