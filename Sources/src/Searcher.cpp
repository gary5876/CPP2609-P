#include "Searcher.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <set>

#include "Tokenizer.h"
#include "Utf.h"

namespace hanseek {

namespace {

// 정렬된 두 목록의 교집합
std::vector<std::uint32_t> intersect(const std::vector<std::uint32_t>& a, const std::vector<std::uint32_t>& b) {
    std::vector<std::uint32_t> out;
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out));
    return out;
}

// 검색어 하나(단어 또는 구문)가 실제로 들어 있는 문단 번호 목록
std::vector<std::uint32_t> matchTerm(const Index& index, const std::string& term) {
    std::string needle = toLowerAscii(term);
    std::vector<std::string> tokens = uniqueTokens(term);

    // 1) 색인으로 후보 문단을 좁힌다. 토큰 목록의 교집합.
    std::vector<std::uint32_t> candidates;
    bool restricted = false;
    for (const std::string& t : tokens) {
        const std::vector<std::uint32_t>* list = index.postings(t);
        if (list == nullptr) {
            // 한 글자 한글은 긴 구간 안에서는 bigram으로만 색인되어 있어 목록이 없을 수 있다.
            std::u32string u = utf8ToU32(t);
            if (u.size() == 1 && isCjk(u[0])) continue;
            return {};  // 색인에 없는 토큰이면 결과가 없다
        }
        candidates = restricted ? intersect(candidates, *list) : *list;
        restricted = true;
        if (candidates.empty()) return {};
    }
    if (!restricted) {  // 한 글자 검색처럼 색인으로 좁힐 수 없으면 전체 문단을 본다
        candidates.resize(index.paragraphCount());
        for (std::uint32_t i = 0; i < candidates.size(); ++i) candidates[i] = i;
    }

    // 2) bigram은 순서를 보장하지 않으므로 실제 문자열이 들어 있는지 확인한다.
    std::vector<std::uint32_t> out;
    for (std::uint32_t id : candidates) {
        if (toLowerAscii(index.paragraph(id)).find(needle) != std::string::npos) out.push_back(id);
    }
    return out;
}

}  // namespace

Query parseQuery(const std::string& text) {
    Query q;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
        if (i >= text.size()) break;
        bool exclude = false;
        if (text[i] == '-' && i + 1 < text.size() && text[i + 1] != ' ') {
            exclude = true;
            ++i;
        }
        std::string term;
        if (text[i] == '"') {
            std::size_t close = text.find('"', i + 1);
            if (close == std::string::npos) close = text.size();
            term = text.substr(i + 1, close - i - 1);
            i = close + 1;
        } else {
            std::size_t j = i;
            while (j < text.size() && text[j] != ' ' && text[j] != '\t') ++j;
            term = text.substr(i, j - i);
            i = j;
        }
        if (term.empty()) continue;
        (exclude ? q.excludes : q.terms).push_back(term);
    }
    return q;
}

SearchResult search(const Index& index, const Query& query, std::size_t limit) {
    auto start = std::chrono::steady_clock::now();
    SearchResult result;
    if (query.terms.empty()) return result;

    // 문서별로 검색어마다 몇 문단에 나왔는지 센다.
    std::map<std::uint32_t, std::vector<std::size_t>> perDoc;  // 문서 -> 검색어별 문단 수
    std::map<std::uint32_t, std::set<std::uint32_t>> docParas;  // 문서 -> 일치 문단
    for (std::size_t t = 0; t < query.terms.size(); ++t) {
        for (std::uint32_t para : matchTerm(index, query.terms[t])) {
            std::uint32_t doc = index.paragraphDoc(para);
            auto& counts = perDoc[doc];
            if (counts.empty()) counts.assign(query.terms.size(), 0);
            ++counts[t];
            docParas[doc].insert(para);
        }
    }

    std::set<std::uint32_t> excluded;
    for (const std::string& ex : query.excludes) {
        for (std::uint32_t para : matchTerm(index, ex)) excluded.insert(index.paragraphDoc(para));
    }

    for (const auto& kv : perDoc) {
        const std::vector<std::size_t>& counts = kv.second;
        bool all = std::all_of(counts.begin(), counts.end(), [](std::size_t c) { return c > 0; });
        if (!all || excluded.count(kv.first)) continue;
        SearchHit hit;
        hit.doc = kv.first;
        // 1차 점수: 검색어가 나온 문단 수 (2차 중간에서 BM25로 바꿀 예정)
        for (std::size_t c : counts) hit.score += std::log(1.0 + static_cast<double>(c));
        const auto& paras = docParas[kv.first];
        hit.paragraphs.assign(paras.begin(), paras.end());
        result.hits.push_back(std::move(hit));
    }
    std::sort(result.hits.begin(), result.hits.end(), [&](const SearchHit& a, const SearchHit& b) {
        if (a.score != b.score) return a.score > b.score;
        return index.documents()[a.doc].path < index.documents()[b.doc].path;
    });
    result.totalDocs = result.hits.size();
    if (result.hits.size() > limit) result.hits.resize(limit);
    result.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result;
}

std::string makeSnippet(const std::string& paragraph, const std::vector<std::string>& terms,
                        const std::string& markOpen, const std::string& markClose, std::size_t radius) {
    std::u32string text = utf8ToU32(paragraph);
    std::u32string lowered = text;
    for (char32_t& c : lowered) {
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    }
    // 검색어가 나오는 구간 [시작, 끝) 목록 (글자 단위)
    std::vector<std::pair<std::size_t, std::size_t>> spans;
    for (const std::string& term : terms) {
        std::u32string needle = utf8ToU32(toLowerAscii(term));
        if (needle.empty()) continue;
        for (std::size_t pos = lowered.find(needle); pos != std::u32string::npos; pos = lowered.find(needle, pos + 1)) {
            spans.emplace_back(pos, pos + needle.size());
        }
    }
    std::sort(spans.begin(), spans.end());
    std::size_t center = spans.empty() ? 0 : spans.front().first;
    std::size_t from = center > radius ? center - radius : 0;
    std::size_t to = std::min(text.size(), center + radius * 2);

    std::string out = from > 0 ? "..." : "";
    std::size_t i = from;
    for (const auto& s : spans) {
        if (s.first < i || s.first >= to) continue;
        out += u32ToUtf8(text.substr(i, s.first - i));
        std::size_t e = std::min(s.second, to);
        out += markOpen + u32ToUtf8(text.substr(s.first, e - s.first)) + markClose;
        i = e;
    }
    if (i < to) out += u32ToUtf8(text.substr(i, to - i));
    if (to < text.size()) out += "...";
    return out;
}

}  // namespace hanseek
