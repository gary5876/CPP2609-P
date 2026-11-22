// 형식별 읽기 클래스: HWP 5.x, HWPX, 텍스트
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Document.h"

namespace hanseek {

// HWP 5.x 바이너리 문서
class HwpReader : public DocumentReader {
public:
    std::string format() const override { return "hwp"; }
    bool supports(const std::string& extension) const override { return extension == "hwp"; }
    Document read(const std::filesystem::path& file) const override;

    // 섹션 스트림(압축을 푼 레코드 묶음)에서 문단 글자를 꺼낸다. 테스트에서도 쓴다.
    static void parseSection(const std::vector<unsigned char>& records, Document& doc);
    // PARA_TEXT 레코드 하나(UTF-16LE + 제어 문자)를 글자로 바꾼다.
    static std::string decodeParaText(const unsigned char* data, std::size_t length);
};

// HWPX (zip 안의 XML) 문서
class HwpxReader : public DocumentReader {
public:
    std::string format() const override { return "hwpx"; }
    bool supports(const std::string& extension) const override { return extension == "hwpx"; }
    Document read(const std::filesystem::path& file) const override;

    // 섹션 XML 하나에서 문단 글자를 꺼낸다. 테스트에서도 쓴다.
    static void parseSectionXml(const std::string& xml, Document& doc);
};

// 일반 텍스트(.txt, .md) 문서: 줄 하나를 문단 하나로 본다.
class TextReader : public DocumentReader {
public:
    std::string format() const override { return "txt"; }
    bool supports(const std::string& extension) const override {
        return extension == "txt" || extension == "md";
    }
    Document read(const std::filesystem::path& file) const override;
};

// XML 문자 참조(&lt; &#44032; 등)를 글자로 바꾼다.
std::string decodeXmlEntities(const std::string& s);

}  // namespace hanseek
