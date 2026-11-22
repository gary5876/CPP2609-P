#include "Utf.h"

namespace hanseek {

void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x110000) {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        appendUtf8(out, 0xFFFD);
    }
}

std::u32string utf8ToU32(const std::string& s) {
    std::u32string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp = 0;
        int extra = 0;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            extra = 2;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            extra = 3;
        } else {
            out += 0xFFFD;
            ++i;
            continue;
        }
        if (i + extra >= s.size()) {  // 문자열 끝에서 잘린 글자
            out += 0xFFFD;
            break;
        }
        bool ok = true;
        for (int k = 1; k <= extra; ++k) {
            unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) {
            out += 0xFFFD;
            ++i;
            continue;
        }
        out += cp;
        i += extra + 1;
    }
    return out;
}

std::string u32ToUtf8(const std::u32string& s) {
    std::string out;
    out.reserve(s.size() * 3);
    for (char32_t cp : s) appendUtf8(out, cp);
    return out;
}

std::string utf16leToUtf8(const std::uint8_t* data, std::size_t byteLength) {
    std::string out;
    std::size_t n = byteLength / 2;
    for (std::size_t i = 0; i < n; ++i) {
        char32_t u = data[2 * i] | (data[2 * i + 1] << 8);
        if (u >= 0xD800 && u <= 0xDBFF && i + 1 < n) {
            char32_t lo = data[2 * (i + 1)] | (data[2 * (i + 1) + 1] << 8);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                ++i;
            }
        }
        appendUtf8(out, u);
    }
    return out;
}

std::string toLowerAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

std::string pathToUtf8(const std::filesystem::path& p) {
    return p.u8string();
}

std::filesystem::path utf8ToPath(const std::string& s) {
    return std::filesystem::u8path(s);
}

}  // namespace hanseek
