#include <algorithm>
#include <cstring>

#include "Readers.h"
#include "Utf.h"
#include "miniz.h"

namespace hanseek {

namespace {

// 태그 이름에서 네임스페이스 접두어를 뗀다. "hp:t" -> "t"
std::string localName(const std::string& tag) {
    std::size_t colon = tag.find(':');
    return colon == std::string::npos ? tag : tag.substr(colon + 1);
}

int sectionNumber(const std::string& name) {
    // "Contents/section3.xml" -> 3
    std::size_t pos = name.rfind("section");
    if (pos == std::string::npos) return -1;
    try {
        return std::stoi(name.substr(pos + 7));
    } catch (...) {
        return -1;
    }
}

}  // namespace

std::string decodeXmlEntities(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') {
            out += s[i];
            continue;
        }
        std::size_t semi = s.find(';', i);
        if (semi == std::string::npos || semi - i > 10) {
            out += s[i];
            continue;
        }
        std::string name = s.substr(i + 1, semi - i - 1);
        if (name == "lt") out += '<';
        else if (name == "gt") out += '>';
        else if (name == "amp") out += '&';
        else if (name == "quot") out += '"';
        else if (name == "apos") out += '\'';
        else if (!name.empty() && name[0] == '#') {
            try {
                char32_t cp = (name.size() > 1 && (name[1] == 'x' || name[1] == 'X'))
                                  ? static_cast<char32_t>(std::stoul(name.substr(2), nullptr, 16))
                                  : static_cast<char32_t>(std::stoul(name.substr(1)));
                appendUtf8(out, cp);
            } catch (...) {
                out += s.substr(i, semi - i + 1);
            }
        } else {
            out += s.substr(i, semi - i + 1);
        }
        i = semi;
    }
    return out;
}

void HwpxReader::parseSectionXml(const std::string& xml, Document& doc) {
    // 문단은 <hp:p>, 글자는 <hp:t> 안에 있다. 표 안의 문단도 <hp:p>로 중첩된다.
    std::string current;
    bool inText = false;
    std::size_t i = 0;
    while (i < xml.size()) {
        if (xml[i] != '<') {
            std::size_t next = xml.find('<', i);
            if (next == std::string::npos) next = xml.size();
            if (inText) current += decodeXmlEntities(xml.substr(i, next - i));
            i = next;
            continue;
        }
        if (xml.compare(i, 4, "<!--") == 0) {  // 주석
            std::size_t end = xml.find("-->", i);
            i = (end == std::string::npos) ? xml.size() : end + 3;
            continue;
        }
        if (xml.compare(i, 9, "<![CDATA[") == 0) {
            std::size_t end = xml.find("]]>", i);
            if (end == std::string::npos) end = xml.size();
            if (inText) current += xml.substr(i + 9, end - i - 9);
            i = (end == xml.size()) ? end : end + 3;
            continue;
        }
        std::size_t end = xml.find('>', i);
        if (end == std::string::npos) break;
        bool closing = xml[i + 1] == '/';
        bool selfClosing = xml[end - 1] == '/';
        std::size_t nameStart = i + (closing ? 2 : 1);
        std::size_t nameEnd = nameStart;
        while (nameEnd < end && xml[nameEnd] != ' ' && xml[nameEnd] != '/' && xml[nameEnd] != '>' &&
               xml[nameEnd] != '\t' && xml[nameEnd] != '\n' && xml[nameEnd] != '\r') {
            ++nameEnd;
        }
        std::string name = localName(xml.substr(nameStart, nameEnd - nameStart));
        if (name == "p") {
            addParagraph(doc, current);  // 문단 경계: 지금까지 모은 글자를 문단으로
            current.clear();
        } else if (name == "t") {
            inText = !closing && !selfClosing;
        } else if (inText && (name == "tab" || name == "lineBreak")) {
            current += ' ';
        }
        i = end + 1;
    }
    addParagraph(doc, current);
}

Document HwpxReader::read(const std::filesystem::path& file) const {
    std::vector<unsigned char> bytes = readAllBytes(file);
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
        throw ReadError("HWPX: zip 파일을 열지 못했습니다");
    }
    std::vector<std::pair<int, mz_uint>> sections;
    mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; ++i) {
        char name[512];
        mz_zip_reader_get_filename(&zip, i, name, sizeof(name));
        std::string n(name);
        if (n.rfind("Contents/section", 0) == 0 && n.size() > 4 && n.compare(n.size() - 4, 4, ".xml") == 0) {
            sections.emplace_back(sectionNumber(n), i);
        }
    }
    std::sort(sections.begin(), sections.end());

    Document doc;
    doc.path = pathToUtf8(file);
    doc.format = format();
    for (const auto& s : sections) {
        std::size_t size = 0;
        void* p = mz_zip_reader_extract_to_heap(&zip, s.second, &size, 0);
        if (p == nullptr) {
            mz_zip_reader_end(&zip);
            throw ReadError("HWPX: 섹션 XML을 풀지 못했습니다");
        }
        std::string xml(static_cast<char*>(p), size);
        mz_free(p);
        parseSectionXml(xml, doc);
    }
    mz_zip_reader_end(&zip);
    if (sections.empty()) throw ReadError("HWPX: 본문 섹션이 없습니다");
    return doc;
}

}  // namespace hanseek
