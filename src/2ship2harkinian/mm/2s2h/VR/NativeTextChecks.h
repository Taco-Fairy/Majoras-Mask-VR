#pragma once
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Exercise the functions exported by the paired game, not a duplicate decoder.
std::wstring StringToU16(const std::string&);
extern "C" void OTRGfxPrint(const char*, void*, void (*)(void*, char));
namespace mmvrtest {
inline void VerifyNativeText() {
    auto require = [](bool ok) { if (!ok) throw std::runtime_error("Native UTF-8/glyph regression"); };
    unsigned scalars = 0;
    for (uint32_t cp = 0; cp <= 0x10FFFF; ++cp) {
        if (cp >= 0xD800 && cp <= 0xDFFF) continue;
        std::string encoded;
        if (cp < 0x80) encoded.push_back(char(cp));
        else if (cp < 0x800) { encoded.push_back(char(0xC0 | (cp >> 6))); encoded.push_back(char(0x80 | (cp & 63))); }
        else if (cp < 0x10000) {
            encoded.push_back(char(0xE0 | (cp >> 12))); encoded.push_back(char(0x80 | ((cp >> 6) & 63))); encoded.push_back(char(0x80 | (cp & 63)));
        } else {
            encoded.push_back(char(0xF0 | (cp >> 18))); encoded.push_back(char(0x80 | ((cp >> 12) & 63)));
            encoded.push_back(char(0x80 | ((cp >> 6) & 63))); encoded.push_back(char(0x80 | (cp & 63)));
        }
        std::wstring expected;
        if (cp != 1) {
            if (cp < 0x10000) expected.push_back(wchar_t(cp));
            else { expected.push_back(wchar_t(0xD800 + ((cp - 0x10000) >> 10))); expected.push_back(wchar_t(0xDC00 + ((cp - 0x10000) & 1023))); }
        }
        require(StringToU16(encoded) == expected); ++scalars;
    }
    const std::vector<std::pair<std::string,std::wstring>> malformed = {
        {"\xF8", L"\uFFFD"}, {"\xFF", L"\uFFFD"}, {"\xC0\xAF", L"\uFFFD"},
        {"\xE0\x80\xAF", L"\uFFFD"}, {"\xED\xA0\x80", L"\uFFFD"},
        {"\xF4\x90\x80\x80", L"\uFFFD"}, {"\xF0\x80\x80\xAF", L"\uFFFD"},
        {"\xC2", L"\uFFFD"}, {"\xE2\x82", L"\uFFFD"}, {"\xF0\x9F\x98", L"\uFFFD"},
        {"\xE2" "A", L"\uFFFDA"}, {"\xE2\x82" "AB", L"\uFFFDAB"},
        {"\x80\xBF" "A", L"A"}, {std::string("\1A",2),L"A"}, {"",L""}
    };
    for (const auto& [input, expected] : malformed) require(StringToU16(input) == expected);
    std::string printed;
    auto sink = [](void* out, char c) { static_cast<std::string*>(out)->push_back(c); };
    OTRGfxPrint("Arena 123 \xE3\x82\x92\xE3\x81\x82\xE3\x81\x9F\xE3\x82\x9C\xEF\xBD\xA1\xEF\xBE\x9F", &printed, sink);
    std::string expected = "Arena 123 ";
    for (unsigned char byte : {0x88,0x93,0xE0,0xFF,0xA1,0xDF}) expected.push_back(char(byte));
    require(printed == expected);
    printed.clear(); OTRGfxPrint("before\xF8" "after", &printed, sink); require(printed == "beforeafter");
    std::ofstream("native-text-validation.json") << "{\"passed\":true,\"validScalars\":" << scalars
        << ",\"malformedCases\":" << malformed.size() << ",\"glyphCases\":2}";
}
}
