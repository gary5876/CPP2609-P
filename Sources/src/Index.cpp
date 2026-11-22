#include "Index.h"

#include <chrono>
#include <cstring>
#include <fstream>

#include "Tokenizer.h"
#include "Utf.h"

namespace hanseek {

namespace {

const char kMagic[4] = {'H', 'S', 'I', 'X'};
const std::uint32_t kVersion = 1;

// 정수와 문자열을 리틀 엔디언 바이트로 쓰고 읽는 도우미
template <typename T>
void writeNum(std::ofstream& out, T v) {
    unsigned char b[sizeof(T)];
    for (std::size_t i = 0; i < sizeof(T); ++i) b[i] = static_cast<unsigned char>((static_cast<std::uint64_t>(v) >> (8 * i)) & 0xFF);
    out.write(reinterpret_cast<const char*>(b), sizeof(T));
}

void writeStr(std::ofstream& out, const std::string& s) {
    writeNum<std::uint32_t>(out, static_cast<std::uint32_t>(s.size()));
    out.write(s.data(), static_cast<std::streamsize>(s.size()));
}

template <typename T>
T readNum(std::ifstream& in) {
    unsigned char b[sizeof(T)];
    if (!in.read(reinterpret_cast<char*>(b), sizeof(T))) throw ReadError("색인 파일이 잘렸습니다");
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) v |= static_cast<std::uint64_t>(b[i]) << (8 * i);
    return static_cast<T>(v);
}

std::string readStr(std::ifstream& in) {
    std::uint32_t n = readNum<std::uint32_t>(in);
    if (n > (1u << 30)) throw ReadError("색인 파일이 깨졌습니다");
    std::string s(n, '\0');
    if (n > 0 && !in.read(&s[0], n)) throw ReadError("색인 파일이 잘렸습니다");
    return s;
}

}  // namespace

void Index::addDocument(const Document& doc, std::int64_t mtime, std::uint64_t size) {
    DocEntry e;
    e.path = doc.path;
    e.format = doc.format;
    e.mtime = mtime;
    e.size = size;
    e.firstPara = static_cast<std::uint32_t>(paras_.size());
    e.paraCount = static_cast<std::uint32_t>(doc.paragraphs.size());
    std::uint32_t docId = static_cast<std::uint32_t>(docs_.size());
    docs_.push_back(e);
    for (const std::string& p : doc.paragraphs) {
        std::uint32_t paraId = static_cast<std::uint32_t>(paras_.size());
        paras_.push_back(p);
        paraDoc_.push_back(docId);
        for (const std::string& t : uniqueTokens(p)) postings_[t].push_back(paraId);  // 번호가 커지는 순서로 쌓인다
    }
}

const std::vector<std::uint32_t>* Index::postings(const std::string& token) const {
    auto it = postings_.find(token);
    return it == postings_.end() ? nullptr : &it->second;
}

void Index::save(const std::filesystem::path& file) const {
    std::ofstream out(file, std::ios::binary);
    if (!out) throw ReadError("색인 파일을 쓸 수 없습니다: " + pathToUtf8(file));
    out.write(kMagic, 4);
    writeNum<std::uint32_t>(out, kVersion);
    writeNum<std::uint32_t>(out, static_cast<std::uint32_t>(docs_.size()));
    for (const DocEntry& e : docs_) {
        writeStr(out, e.path);
        writeStr(out, e.format);
        writeNum<std::int64_t>(out, e.mtime);
        writeNum<std::uint64_t>(out, e.size);
        writeNum<std::uint32_t>(out, e.firstPara);
        writeNum<std::uint32_t>(out, e.paraCount);
    }
    writeNum<std::uint32_t>(out, static_cast<std::uint32_t>(paras_.size()));
    for (std::size_t i = 0; i < paras_.size(); ++i) {
        writeStr(out, paras_[i]);
        writeNum<std::uint32_t>(out, paraDoc_[i]);
    }
    writeNum<std::uint32_t>(out, static_cast<std::uint32_t>(postings_.size()));
    for (const auto& kv : postings_) {
        writeStr(out, kv.first);
        writeNum<std::uint32_t>(out, static_cast<std::uint32_t>(kv.second.size()));
        for (std::uint32_t id : kv.second) writeNum<std::uint32_t>(out, id);
    }
    if (!out) throw ReadError("색인 파일 저장에 실패했습니다");
}

Index Index::load(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw ReadError("색인 파일이 없습니다. 먼저 index 명령으로 색인을 만드세요: " + pathToUtf8(file));
    char magic[4];
    if (!in.read(magic, 4) || std::memcmp(magic, kMagic, 4) != 0) throw ReadError("HanSeek 색인 파일이 아닙니다");
    if (readNum<std::uint32_t>(in) != kVersion) throw ReadError("색인 파일 버전이 다릅니다. 다시 색인하세요");
    Index idx;
    std::uint32_t nDocs = readNum<std::uint32_t>(in);
    for (std::uint32_t i = 0; i < nDocs; ++i) {
        DocEntry e;
        e.path = readStr(in);
        e.format = readStr(in);
        e.mtime = readNum<std::int64_t>(in);
        e.size = readNum<std::uint64_t>(in);
        e.firstPara = readNum<std::uint32_t>(in);
        e.paraCount = readNum<std::uint32_t>(in);
        idx.docs_.push_back(e);
    }
    std::uint32_t nParas = readNum<std::uint32_t>(in);
    idx.paras_.reserve(nParas);
    idx.paraDoc_.reserve(nParas);
    for (std::uint32_t i = 0; i < nParas; ++i) {
        idx.paras_.push_back(readStr(in));
        idx.paraDoc_.push_back(readNum<std::uint32_t>(in));
    }
    std::uint32_t nTokens = readNum<std::uint32_t>(in);
    idx.postings_.reserve(nTokens);
    for (std::uint32_t i = 0; i < nTokens; ++i) {
        std::string t = readStr(in);
        std::uint32_t n = readNum<std::uint32_t>(in);
        std::vector<std::uint32_t> list(n);
        for (std::uint32_t k = 0; k < n; ++k) list[k] = readNum<std::uint32_t>(in);
        idx.postings_.emplace(std::move(t), std::move(list));
    }
    return idx;
}

Index buildIndex(const std::filesystem::path& root, IndexReport& report, const ProgressFn& progress) {
    namespace fs = std::filesystem;
    auto start = std::chrono::steady_clock::now();
    ReaderRegistry registry;
    Index index;
    std::error_code ec;
    if (!fs::is_directory(root, ec)) throw ReadError("폴더가 아닙니다: " + pathToUtf8(root));

    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        const fs::path& p = it->path();
        std::string name = pathToUtf8(p.filename());
        if (!name.empty() && name[0] == '.') {  // 숨김 파일과 폴더는 건너뛴다
            if (it->is_directory(ec)) it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(ec) || name.rfind("~$", 0) == 0) continue;  // 임시 파일 제외
        if (registry.find(p) == nullptr) continue;
        ++report.scanned;
        if (progress) progress(report.scanned, pathToUtf8(p));
        try {
            Document doc = registry.read(p);
            auto mtime = fs::last_write_time(p, ec).time_since_epoch().count();
            auto size = fs::file_size(p, ec);
            index.addDocument(doc, static_cast<std::int64_t>(mtime), static_cast<std::uint64_t>(size));
            ++report.indexed;
        } catch (const std::exception& e) {
            report.failures.emplace_back(pathToUtf8(p), e.what());
        }
    }
    report.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return index;
}

}  // namespace hanseek
