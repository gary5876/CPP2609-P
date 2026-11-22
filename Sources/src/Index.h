// 역색인: 토큰 -> 그 토큰이 들어 있는 문단 번호 목록
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Document.h"

namespace hanseek {

struct DocEntry {
    std::string path;
    std::string format;
    std::int64_t mtime = 0;      // 마지막 수정 시각 (다음 단계의 증분 색인에 쓴다)
    std::uint64_t size = 0;
    std::uint32_t firstPara = 0;  // 이 문서의 첫 문단 번호
    std::uint32_t paraCount = 0;
};

class Index {
public:
    // 문서 하나를 색인에 넣는다.
    void addDocument(const Document& doc, std::int64_t mtime, std::uint64_t size);

    // 토큰이 들어 있는 문단 번호 목록 (오름차순). 없으면 nullptr.
    const std::vector<std::uint32_t>* postings(const std::string& token) const;

    const std::vector<DocEntry>& documents() const { return docs_; }
    const std::string& paragraph(std::uint32_t id) const { return paras_[id]; }
    std::uint32_t paragraphDoc(std::uint32_t id) const { return paraDoc_[id]; }
    std::size_t paragraphCount() const { return paras_.size(); }
    std::size_t tokenCount() const { return postings_.size(); }

    // 바이너리 파일로 저장하고 불러온다.
    void save(const std::filesystem::path& file) const;
    static Index load(const std::filesystem::path& file);

private:
    std::vector<DocEntry> docs_;
    std::vector<std::string> paras_;        // 문단 본문 (검색 결과 문맥을 보여 줄 때 쓴다)
    std::vector<std::uint32_t> paraDoc_;    // 문단 번호 -> 문서 번호
    std::unordered_map<std::string, std::vector<std::uint32_t>> postings_;
};

// 폴더를 훑어 지원하는 문서를 모두 색인한다.
struct IndexReport {
    std::size_t scanned = 0;   // 지원 형식 파일 수
    std::size_t indexed = 0;   // 색인 성공
    std::vector<std::pair<std::string, std::string>> failures;  // (경로, 이유)
    double seconds = 0;
};

using ProgressFn = std::function<void(std::size_t done, const std::string& path)>;

Index buildIndex(const std::filesystem::path& root, IndexReport& report, const ProgressFn& progress = nullptr);

}  // namespace hanseek
