#include "Document.h"

#include <fstream>
#include <iterator>
#include <sstream>

#include "Readers.h"
#include "Utf.h"

namespace hanseek {

std::vector<unsigned char> readAllBytes(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw ReadError("파일을 열 수 없습니다: " + pathToUtf8(file));
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void addParagraph(Document& doc, std::string text) {
    while (!text.empty() && (text.back() == ' ' || text.back() == '\n' || text.back() == '\r' ||
                             text.back() == '\t')) {
        text.pop_back();
    }
    std::size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return;  // 빈 문단은 넣지 않는다
    doc.paragraphs.push_back(text.substr(start));
}

Document TextReader::read(const std::filesystem::path& file) const {
    std::vector<unsigned char> bytes = readAllBytes(file);
    std::string text(bytes.begin(), bytes.end());
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);  // UTF-8 BOM
    Document doc;
    doc.path = pathToUtf8(file);
    doc.format = format();
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) addParagraph(doc, line);
    return doc;
}

ReaderRegistry::ReaderRegistry() {
    readers_.push_back(std::make_unique<HwpReader>());
    readers_.push_back(std::make_unique<HwpxReader>());
    readers_.push_back(std::make_unique<TextReader>());
}

const DocumentReader* ReaderRegistry::find(const std::filesystem::path& file) const {
    std::string ext = toLowerAscii(pathToUtf8(file.extension()));
    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
    for (const auto& r : readers_) {
        if (r->supports(ext)) return r.get();
    }
    return nullptr;
}

Document ReaderRegistry::read(const std::filesystem::path& file) const {
    const DocumentReader* r = find(file);
    if (r == nullptr) throw ReadError("지원하지 않는 형식입니다: " + pathToUtf8(file));
    return r->read(file);
}

}  // namespace hanseek
