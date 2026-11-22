#include "Tokenizer.h"

#include <algorithm>

#include "Utf.h"

namespace hanseek {

bool isCjk(char32_t cp) {
    return (cp >= 0xAC00 && cp <= 0xD7A3) ||  // 한글 음절
           (cp >= 0x1100 && cp <= 0x11FF) ||  // 한글 자모
           (cp >= 0x3130 && cp <= 0x318F) ||  // 호환 자모
           (cp >= 0x4E00 && cp <= 0x9FFF) ||  // 한자
           (cp >= 0x3040 && cp <= 0x30FF);    // 가나
}

namespace {

bool isWordChar(char32_t cp) {
    return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') ||
           (cp >= 0xC0 && cp <= 0x24F && cp != 0xD7 && cp != 0xF7);  // 라틴 확장 문자
}

char32_t lower(char32_t cp) {
    return (cp >= 'A' && cp <= 'Z') ? cp - 'A' + 'a' : cp;
}

}  // namespace

std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::u32string s = utf8ToU32(text);
    std::size_t i = 0;
    while (i < s.size()) {
        if (isCjk(s[i])) {
            std::size_t j = i;
            while (j < s.size() && isCjk(s[j])) ++j;
            if (j - i == 1) {
                tokens.push_back(u32ToUtf8(s.substr(i, 1)));
            } else {
                for (std::size_t k = i; k + 1 < j; ++k) tokens.push_back(u32ToUtf8(s.substr(k, 2)));
            }
            i = j;
        } else if (isWordChar(s[i])) {
            std::u32string word;
            while (i < s.size() && isWordChar(s[i])) word += lower(s[i++]);
            tokens.push_back(u32ToUtf8(word));
        } else {
            ++i;  // 공백, 문장 부호 등은 구분자
        }
    }
    return tokens;
}

std::vector<std::string> uniqueTokens(const std::string& text) {
    std::vector<std::string> t = tokenize(text);
    std::sort(t.begin(), t.end());
    t.erase(std::unique(t.begin(), t.end()), t.end());
    return t;
}

}  // namespace hanseek
