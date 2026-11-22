#include <algorithm>
#include <cstring>

#include "CfbReader.h"
#include "Readers.h"
#include "Utf.h"
#include "miniz.h"

namespace hanseek {

namespace {

const int kTagParaText = 67;  // HWPTAG_BEGIN(16) + 51

// deflate로 저장된 섹션을 푼다. HWP는 zlib 헤더가 없는 raw deflate를 쓴다.
std::vector<unsigned char> inflateRaw(const std::vector<unsigned char>& in) {
    std::size_t outLen = 0;
    void* p = tinfl_decompress_mem_to_heap(in.data(), in.size(), &outLen, 0);
    if (p == nullptr) throw ReadError("HWP: 본문 스트림의 deflate를 풀지 못했습니다");
    std::vector<unsigned char> out(static_cast<unsigned char*>(p), static_cast<unsigned char*>(p) + outLen);
    mz_free(p);
    return out;
}

int sectionNumber(const std::string& path) {
    // "BodyText/Section12" -> 12
    std::size_t pos = path.find("Section");
    if (pos == std::string::npos) return -1;
    try {
        return std::stoi(path.substr(pos + 7));
    } catch (...) {
        return -1;
    }
}

}  // namespace

std::string HwpReader::decodeParaText(const unsigned char* data, std::size_t length) {
    std::string out;
    std::size_t n = length / 2;
    std::size_t i = 0;
    while (i < n) {
        char32_t ch = data[2 * i] | (data[2 * i + 1] << 8);
        if (ch < 32) {
            // 제어 문자: 한 글자짜리(char)와 8글자 묶음(inline, extended)이 있다.
            bool single = (ch == 0 || ch == 10 || ch == 13 || (ch >= 24 && ch <= 31));
            if (ch == 10 || ch == 30 || ch == 31) out += ' ';  // 줄바꿈, 묶음 빈칸, 고정폭 빈칸
            if (ch == 9) out += ' ';                           // 탭
            i += single ? 1 : 8;
            continue;
        }
        if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < n) {
            char32_t lo = data[2 * (i + 1)] | (data[2 * (i + 1) + 1] << 8);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                appendUtf8(out, 0x10000 + ((ch - 0xD800) << 10) + (lo - 0xDC00));
                i += 2;
                continue;
            }
        }
        appendUtf8(out, ch);
        ++i;
    }
    return out;
}

void HwpReader::parseSection(const std::vector<unsigned char>& records, Document& doc) {
    // 레코드 헤더 4바이트: 태그 10비트, 레벨 10비트, 크기 12비트 (0xFFF면 뒤 4바이트가 크기)
    std::size_t pos = 0;
    while (pos + 4 <= records.size()) {
        std::uint32_t header;
        std::memcpy(&header, records.data() + pos, 4);
        pos += 4;
        int tag = header & 0x3FF;
        std::uint32_t size = (header >> 20) & 0xFFF;
        if (size == 0xFFF) {
            if (pos + 4 > records.size()) break;
            std::memcpy(&size, records.data() + pos, 4);
            pos += 4;
        }
        if (pos + size > records.size()) break;  // 잘린 레코드
        if (tag == kTagParaText) {
            addParagraph(doc, decodeParaText(records.data() + pos, size));
        }
        pos += size;
    }
}

Document HwpReader::read(const std::filesystem::path& file) const {
    CfbReader cfb(readAllBytes(file));
    if (!cfb.hasStream("FileHeader")) throw ReadError("HWP: FileHeader가 없습니다");
    std::vector<unsigned char> header = cfb.readStream("FileHeader");
    if (header.size() < 40 || std::memcmp(header.data(), "HWP Document File", 17) != 0) {
        throw ReadError("HWP 5.x 문서가 아닙니다");
    }
    std::uint32_t flags;
    std::memcpy(&flags, header.data() + 36, 4);
    bool compressed = flags & 0x1;
    if (flags & 0x2) throw ReadError("암호가 걸린 문서입니다");
    if (flags & 0x4) throw ReadError("배포용 문서는 아직 지원하지 않습니다");

    std::vector<std::string> sections;
    for (const std::string& p : cfb.streamPaths()) {
        if (p.rfind("BodyText/Section", 0) == 0) sections.push_back(p);
    }
    std::sort(sections.begin(), sections.end(), [](const std::string& a, const std::string& b) {
        return sectionNumber(a) < sectionNumber(b);
    });
    if (sections.empty()) throw ReadError("HWP: 본문 섹션이 없습니다");

    Document doc;
    doc.path = pathToUtf8(file);
    doc.format = format();
    for (const std::string& s : sections) {
        std::vector<unsigned char> raw = cfb.readStream(s);
        parseSection(compressed ? inflateRaw(raw) : raw, doc);
    }
    return doc;
}

}  // namespace hanseek
