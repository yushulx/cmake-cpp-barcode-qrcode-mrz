#include "matcher.h"
#include "normalization.h"

#include <unordered_map>

namespace bench {

namespace {
bool equivalentFormatAndPayload(const GroundTruth& gt, const DecodedBarcode& prediction)
{
    const auto gf = canonicalFormat(gt.format);
    const auto pf = canonicalFormat(prediction.canonical_format);
    if (gf == pf) return normalizedPayload(gf, gt.text) == normalizedPayload(gf, prediction.text);
    if ((gf == "UPC_A" && pf == "EAN_13") || (gf == "EAN_13" && pf == "UPC_A"))
        return normalizedPayload("UPC_A", gt.text) == normalizedPayload("UPC_A", prediction.text);
    return false;
}
}

std::string toString(Outcome value)
{
    switch (value) {
    case Outcome::Correct: return "correct";
    case Outcome::NotFound: return "not_found";
    case Outcome::WrongText: return "wrong_text";
    case Outcome::WrongFormat: return "wrong_format";
    case Outcome::ExtraResult: return "extra_result";
    case Outcome::UnsupportedFormat: return "unsupported_format";
    case Outcome::AmbiguousGroundTruth: return "ambiguous_ground_truth";
    case Outcome::DecoderError: return "decoder_error";
    case Outcome::LicenseOrInitializationError: return "license_or_initialization_error";
    case Outcome::InputPipelineError: return "input_pipeline_error";
    }
    return "unknown";
}

std::vector<MatchItem> matchResults(const std::vector<GroundTruth>& truth,
                                    const std::vector<DecodedBarcode>& predictions,
                                    std::string_view decoder)
{
    std::vector<MatchItem> output;
    std::vector<bool> used(predictions.size(), false);

    for (std::size_t ti = 0; ti < truth.size(); ++ti) {
        const auto& gt = truth[ti];
        if (!gt.decode_eligible) continue;
        if (!isFormatSupported(decoder, gt.format)) {
            output.push_back({ti, std::nullopt, Outcome::UnsupportedFormat});
            continue;
        }
        const auto gt_format = canonicalFormat(gt.format);
        const auto gt_text = normalizedPayload(gt_format, gt.text);
        std::optional<std::size_t> exact;
        for (std::size_t pi = 0; pi < predictions.size(); ++pi) {
            if (used[pi]) continue;
            if (equivalentFormatAndPayload(gt, predictions[pi])) {
                exact = pi;
                break;
            }
        }
        if (exact) {
            used[*exact] = true;
            output.push_back({ti, exact, Outcome::Correct});
            continue;
        }
        std::optional<std::size_t> same_text;
        std::optional<std::size_t> same_format;
        for (std::size_t pi = 0; pi < predictions.size(); ++pi) {
            if (used[pi]) continue;
            const auto pf = canonicalFormat(predictions[pi].canonical_format);
            if (!same_text && normalizedPayload(pf, predictions[pi].text) == gt_text) same_text = pi;
            if (!same_format && pf == gt_format) same_format = pi;
        }
        if (same_text) {
            used[*same_text] = true;
            output.push_back({ti, same_text, Outcome::WrongFormat});
        } else if (same_format) {
            used[*same_format] = true;
            output.push_back({ti, same_format, Outcome::WrongText});
        } else {
            output.push_back({ti, std::nullopt, Outcome::NotFound});
        }
    }
    for (std::size_t pi = 0; pi < predictions.size(); ++pi)
        if (!used[pi]) output.push_back({std::nullopt, pi, Outcome::ExtraResult});
    return output;
}

} // namespace bench
