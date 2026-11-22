// 검색어 해석과 검색
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Index.h"

namespace hanseek {

// 검색어 문법
//   예산 집행        두 단어가 모두 들어 있는 문서 (AND)
//   "예산 집행"      따옴표 안의 구문이 그대로 들어 있는 문서
//   예산 -추경       "추경"이 들어 있는 문서는 뺀다
struct Query {
    std::vector<std::string> terms;     // 단어와 구문
    std::vector<std::string> excludes;  // 제외어
};

Query parseQuery(const std::string& text);

struct SearchHit {
    std::uint32_t doc = 0;
    double score = 0;
    std::vector<std::uint32_t> paragraphs;  // 검색어가 나온 문단 번호 (문맥 표시용)
};

struct SearchResult {
    std::vector<SearchHit> hits;  // 점수 높은 순
    std::size_t totalDocs = 0;    // 잘리기 전 전체 결과 수
    double milliseconds = 0;
};

SearchResult search(const Index& index, const Query& query, std::size_t limit = 20);

// 문단에서 검색어 주변만 잘라 보여 준다. 검색어 앞뒤에 mark 문자열을 붙인다.
std::string makeSnippet(const std::string& paragraph, const std::vector<std::string>& terms,
                        const std::string& markOpen, const std::string& markClose, std::size_t radius = 30);

}  // namespace hanseek
