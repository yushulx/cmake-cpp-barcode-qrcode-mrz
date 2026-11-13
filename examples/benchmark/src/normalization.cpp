#include "normalization.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace bench {
namespace {

std::string key(std::string_view input)
{
    std::string out;
    for (unsigned char c : input) {
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::toupper(c)));
    }
    return out;
}

bool digits(std::string_view s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

bool validGtin(std::string_view value)
{
    if(!digits(value)||value.size()<2)return false;
    int sum=0; bool weight_three=true;
    for(std::size_t i=value.size()-1;i-- > 0;){
        sum+=(value[i]-'0')*(weight_three?3:1);
        weight_three=!weight_three;
    }
    return (10-(sum%10))%10==value.back()-'0';
}

} // namespace

std::string canonicalFormat(std::string_view value)
{
    static const std::unordered_map<std::string, std::string> map = {
        {"AZTEC", "AZTEC"}, {"C128", "CODE_128"}, {"CODE128", "CODE_128"},
        {"UCC128", "GS1_128"}, {"GS1128", "GS1_128"},
        {"C39", "CODE_39"}, {"CODE39", "CODE_39"}, {"CODE39EXTENDED", "CODE_39"},
        {"DATAMATRIX", "DATA_MATRIX"}, {"EAN13", "EAN_13"}, {"EAN8", "EAN_8"},
        {"2DIGIT", "EAN_2"}, {"EAN2", "EAN_2"},
        // Scandit Linux C API symbology names (lower-case, hyphenated).
        {"EAN13UPCA", "EAN_13"}, {"TWODIGITADDON", "EAN_2"}, {"FIVEDIGITADDON", "EAN_5"},
        {"IATA2OF5", "IATA_2_OF_5"}, {"USPSINTELLIGENTMAIL", "USPS_INTELLIGENT_MAIL"},
        {"ROYALMAIL4STATE", "ROYAL_MAIL"}, {"RM4SCC", "ROYAL_MAIL"},
        {"CODE93", "CODE_93"}, {"CODABAR", "CODABAR"}, {"CODE11", "CODE_11"},
        {"CODE25", "CODE_25"}, {"MSIPLESSEY", "MSI_PLESSEY"},
        {"MICROPDF417", "MICRO_PDF_417"}, {"MICROQR", "MICRO_QR"},
        {"DATABAR", "GS1_DATABAR"}, {"DATABAREXPANDED", "GS1_DATABAR_EXPANDED"},
        {"DATABARLIMITED", "GS1_DATABAR_LIMITED"},
        {"I2O5", "ITF"}, {"ITF", "ITF"}, {"INTERLEAVED2OF5", "ITF"},
        {"IATA25", "IATA_2_OF_5"},
        {"INTELLIGENTMAIL", "USPS_INTELLIGENT_MAIL"},
        {"JAPANPOST", "JAPAN_POST"}, {"KIX", "KIX"}, {"PDF417", "PDF_417"},
        {"POSTNET", "POSTNET"}, {"QR", "QR_CODE"}, {"QRCODE", "QR_CODE"},
        // BarBeR spells this one with an underscore. key() strips punctuation, so
        // without this entry the ground-truth name reduces to ROYALMAIL and matches
        // no decoder's supported set (protocol v1 scored all 192 annotations as
        // unsupported).
        {"ROYALMAIL", "ROYAL_MAIL"},
        {"ROYALMAILCODE", "ROYAL_MAIL"}, {"UPCA", "UPC_A"},
        {"UPCS", "UPC_A"}, {"UPCE", "UPC_E"}, {"1D", "GENERIC_1D"},
        {"GENERIC1D", "GENERIC_1D"}, {"UNKNOWN", "UNKNOWN"}, {"1", "UNKNOWN"}
    };
    auto normalized = key(value);
    auto it = map.find(normalized);
    return it == map.end() ? normalized : it->second;
}

std::string formatFamily(std::string_view value)
{
    // GS1-128 and Code 128 are the same symbology; see normalization.h. The
    // readers report the GS1-ness through three different channels ({GS} prefix,
    // AIM identifier, carrier flag), so matching must not depend on the name one
    // of them happens to use.
    const auto canonical = canonicalFormat(value);
    return canonical == "GS1_128" ? "CODE_128" : canonical;
}

void replaceAll(std::string& value, std::string_view from, std::string_view to)
{
    if (from.empty()) return;
    std::size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::string unescapeHtmlEntities(std::string value)
{
    replaceAll(value, "&amp;", "&");
    replaceAll(value, "&lt;", "<");
    replaceAll(value, "&gt;", ">");
    replaceAll(value, "&quot;", "\"");
    replaceAll(value, "&#39;", "'");
    replaceAll(value, "&apos;", "'");
    return value;
}

std::string rstripNewlines(std::string value)
{
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
        value.pop_back();
    return value;
}

void stripLeadingGs1Marker(std::string& value)
{
    for (;;) {
        if (value.rfind("{GS}", 0) == 0) {
            value.erase(0, 4);
            continue;
        }
        if (value.rfind("{FNC1}", 0) == 0) {
            value.erase(0, 6);
            continue;
        }
        if (!value.empty() && static_cast<unsigned char>(value.front()) == 0x1d) {
            value.erase(value.begin());
            continue;
        }
        break;
    }
}

bool isUnreliablePlaceholder(std::string_view payload)
{
    return payload == "^";
}

// GS1 readers that print the human-readable form wrap every application
// identifier in parentheses: "(90)85320". BarBeR stores the raw element string
// "9085320". Dropping only the parentheses keeps the AI digits, so the two
// forms become comparable. Applied to the GS1-capable linear symbologies only.
void unbracketGs1Ais(std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size();) {
        if (value[i] == '(') {
            std::size_t j = i + 1;
            while (j < value.size() && std::isdigit(static_cast<unsigned char>(value[j]))) ++j;
            const auto count = j - (i + 1);
            if (count >= 2 && count <= 4 && j < value.size() && value[j] == ')') {
                out.append(value, i + 1, count);
                i = j + 1;
                continue;
            }
        }
        out.push_back(value[i]);
        ++i;
    }
    value.swap(out);
}

std::string normalizedPayload(std::string_view format, std::string_view payload)
{
    std::string result = rstripNewlines(unescapeHtmlEntities(std::string(payload)));
    if (result.rfind("\\000001", 0) == 0)
        result.erase(0, 7);
    const auto canonical = canonicalFormat(format);
    if (canonical == "UPC_A" && result.size() == 13 && result.front() == '0')
        result.erase(result.begin());
    if (canonical == "CODE_39" && result.size() >= 2 && result.front() == '*' && result.back() == '*')
        result = result.substr(1, result.size() - 2);
    if (canonical == "CODE_128" || canonical == "GS1_128") {
        stripLeadingGs1Marker(result);
        unbracketGs1Ais(result);
    }
    return result;
}

bool isSpecificBarberFormat(std::string_view value)
{
    const auto f = canonicalFormat(value);
    return !f.empty() && f != "GENERIC_1D" && f != "UNKNOWN";
}

bool isPayloadStructurallyValid(std::string_view format, std::string_view payload)
{
    if (payload.empty()) return false;
    const auto f = canonicalFormat(format);
    if (f == "EAN_13") return payload.size() == 13 && validGtin(payload);
    if (f == "EAN_8") return payload.size() == 8 && validGtin(payload);
    if (f == "EAN_2") return payload.size() == 2 && digits(payload);
    if (f == "UPC_A") {
        const auto normalized=normalizedPayload(f,payload);
        return normalized.size()==12&&validGtin(normalized);
    }
    return true;
}

const std::unordered_set<std::string>& zxingSupportedFormats()
{
    static const std::unordered_set<std::string> value = {
        "AZTEC", "CODE_128", "GS1_128", "CODE_39", "DATA_MATRIX", "EAN_13",
        "EAN_8", "EAN_2", "ITF", "PDF_417", "QR_CODE", "UPC_A", "UPC_E"
    };
    return value;
}

const std::unordered_set<std::string>& dbrSupportedFormats()
{
    static const std::unordered_set<std::string> value = {
        "AZTEC", "CODE_128", "GS1_128", "CODE_39", "DATA_MATRIX", "EAN_13",
        "EAN_8", "EAN_2", "ITF", "IATA_2_OF_5", "USPS_INTELLIGENT_MAIL",
        "JAPAN_POST", "KIX", "PDF_417", "POSTNET", "QR_CODE", "ROYAL_MAIL",
        "UPC_A", "UPC_E"
    };
    return value;
}

const std::unordered_set<std::string>& scanditSupportedFormats()
{
    // Scandit 8.7.1 exposes no JAPAN_POST or POSTNET symbology. Every other
    // BarBeR format that DBR supports is enabled in configs/scandit_all_supported.json.
    static const std::unordered_set<std::string> value = {
        "AZTEC", "CODE_128", "GS1_128", "CODE_39", "DATA_MATRIX", "EAN_13",
        "EAN_8", "EAN_2", "ITF", "IATA_2_OF_5", "USPS_INTELLIGENT_MAIL",
        "KIX", "PDF_417", "QR_CODE", "ROYAL_MAIL", "UPC_A", "UPC_E"
    };
    return value;
}

bool isFormatSupported(std::string_view decoder, std::string_view format)
{
    const auto f = canonicalFormat(format);
    if (decoder == "zxing-cpp") return zxingSupportedFormats().count(f) != 0;
    if (decoder == "scandit-linux") return scanditSupportedFormats().count(f) != 0;
    return dbrSupportedFormats().count(f) != 0;
}

} // namespace bench
