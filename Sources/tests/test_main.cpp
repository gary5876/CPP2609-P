// HanSeek 단위 테스트
// 사용법: hanseek_tests <fixtures 폴더>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "Document.h"
#include "Index.h"
#include "Readers.h"
#include "Searcher.h"
#include "Tokenizer.h"
#include "Utf.h"

using namespace hanseek;

namespace {

int g_failed = 0;
int g_checks = 0;
std::string g_fixtures = "tests/fixtures";

#define CHECK(cond)                                                                   \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(cond)) {                                                                \
            ++g_failed;                                                               \
            std::cout << "    실패: " << #cond << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        }                                                                             \
    } while (0)

bool contains(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& x : v) {
        if (x == s) return true;
    }
    return false;
}

bool anyParagraphHas(const Document& d, const std::string& s) {
    for (const auto& p : d.paragraphs) {
        if (p.find(s) != std::string::npos) return true;
    }
    return false;
}

std::filesystem::path fixture(const std::string& name) {
    return utf8ToPath(g_fixtures) / utf8ToPath(name);
}

// ---------------- 인코딩 ----------------
void testUtf() {
    // "가A" = UTF-16LE 00 AC 41 00
    const unsigned char u16[] = {0x00, 0xAC, 0x41, 0x00};
    CHECK(utf16leToUtf8(u16, 4) == "가A");
    // 서로게이트 쌍: U+1F600
    const unsigned char emoji[] = {0x3D, 0xD8, 0x00, 0xDE};
    CHECK(utf16leToUtf8(emoji, 4) == "\xF0\x9F\x98\x80");
    CHECK(u32ToUtf8(utf8ToU32("한글 HWP")) == "한글 HWP");
    CHECK(utf8ToU32("한글").size() == 2);
    CHECK(toLowerAscii("HWP 한글") == "hwp 한글");
}

// ---------------- 토큰화 ----------------
void testTokenizer() {
    std::vector<std::string> t = tokenize("예산을 집행");
    CHECK(contains(t, "예산"));
    CHECK(contains(t, "산을"));
    CHECK(contains(t, "집행"));
    CHECK(t.size() == 3);
    CHECK(tokenize("법").size() == 1);  // 한 글자 구간은 그대로
    std::vector<std::string> e = tokenize("HWP 5.0 Document");
    CHECK(contains(e, "hwp"));
    CHECK(contains(e, "5"));
    CHECK(contains(e, "document"));
    CHECK(uniqueTokens("예산 예산 예산").size() == 1);
    CHECK(tokenize("  ,.!  ").empty());
}

// ---------------- HWP 레코드 해석 ----------------
void testParaText() {
    // "가" + 탭(8글자 묶음 제어 문자) + "나" + 문단 끝(13)
    std::vector<unsigned char> b = {0x00, 0xAC};
    b.push_back(9);
    b.push_back(0);
    for (int i = 0; i < 7; ++i) {  // 제어 문자 뒤 7글자(14바이트)는 건너뛰어야 한다
        b.push_back(0x41);
        b.push_back(0);
    }
    b.push_back(0x98);
    b.push_back(0xB0);
    b.push_back(13);
    b.push_back(0);
    CHECK(HwpReader::decodeParaText(b.data(), b.size()) == "가 나");

    // 레코드 묶음: PARA_TEXT(67) 레코드 하나
    std::vector<unsigned char> rec;
    std::uint32_t header = 67 | (0u << 10) | (4u << 20);  // 크기 4바이트
    for (int i = 0; i < 4; ++i) rec.push_back(static_cast<unsigned char>(header >> (8 * i)));
    rec.insert(rec.end(), {0x00, 0xAC, 0x41, 0x00});
    Document d;
    HwpReader::parseSection(rec, d);
    CHECK(d.paragraphs.size() == 1);
    CHECK(!d.paragraphs.empty() && d.paragraphs[0] == "가A");
}

// ---------------- HWPX XML 해석 ----------------
void testHwpxXml() {
    std::string xml =
        "<hs:sec xmlns:hp='x'><hp:p><hp:run><hp:t>첫 문단 &amp; 기호</hp:t></hp:run></hp:p>"
        "<hp:p><hp:run><hp:t>둘째<hp:tab/>문단</hp:t></hp:run></hp:p>"
        "<hp:p><hp:run><hp:t/></hp:run></hp:p></hs:sec>";
    Document d;
    HwpxReader::parseSectionXml(xml, d);
    CHECK(d.paragraphs.size() == 2);  // 빈 문단은 빠진다
    CHECK(d.paragraphs.size() == 2 && d.paragraphs[0] == "첫 문단 & 기호");
    CHECK(d.paragraphs.size() == 2 && d.paragraphs[1] == "둘째 문단");
    CHECK(decodeXmlEntities("&lt;&#44032;&#xAC01;&gt;") == "<가각>");
}

// ---------------- 실제 파일 읽기 ----------------
void testHwpFile() {
    ReaderRegistry reg;
    Document d = reg.read(fixture("form.hwp"));
    CHECK(d.format == "hwp");
    CHECK(d.paragraphs.size() == 37);
    CHECK(!d.paragraphs.empty() && d.paragraphs[0] == "교과구분 정정 신청서");
    CHECK(anyParagraphHas(d, "2. 교과구분 변경 신청 내역"));
    CHECK(anyParagraphHas(d, "교과목코드"));  // 표 안의 글자
}

void testHwpxFile() {
    ReaderRegistry reg;
    Document d = reg.read(fixture("form.hwpx"));
    CHECK(d.format == "hwpx");
    CHECK(!d.paragraphs.empty() && d.paragraphs[0] == "교과구분 정정 신청서");
    CHECK(anyParagraphHas(d, "교과목코드"));
    // 같은 문서를 HWP와 HWPX로 읽은 결과가 같아야 한다.
    Document h = reg.read(fixture("form.hwp"));
    CHECK(d.paragraphs == h.paragraphs);
}

void testUnsupported() {
    ReaderRegistry reg;
    CHECK(reg.find(utf8ToPath("a.pdf")) == nullptr);
    CHECK(reg.find(utf8ToPath("A.HWP")) != nullptr);  // 대문자 확장자
    bool threw = false;
    try {
        reg.read(fixture("sample.txt").replace_extension(".hwp"));  // 없는 파일
    } catch (const ReadError&) {
        threw = true;
    }
    CHECK(threw);
    threw = false;
    try {
        HwpReader().read(fixture("sample.txt"));  // HWP가 아닌 파일
    } catch (const ReadError&) {
        threw = true;
    }
    CHECK(threw);
}

// ---------------- 색인과 검색 ----------------
Index makeIndex() {
    Index idx;
    Document a{"a.hwp", "hwp", {"2026년 예산을 집행한 결과", "회의록 작성"}};
    Document b{"b.hwp", "hwp", {"예산 계획", "추경 예산 편성"}};
    Document c{"c.hwpx", "hwpx", {"집행 내역", "HWP Viewer 설치"}};
    idx.addDocument(a, 1, 10);
    idx.addDocument(b, 2, 20);
    idx.addDocument(c, 3, 30);
    return idx;
}

std::vector<std::string> paths(const Index& idx, const SearchResult& r) {
    std::vector<std::string> out;
    for (const auto& h : r.hits) out.push_back(idx.documents()[h.doc].path);
    return out;
}

void testSearch() {
    Index idx = makeIndex();
    // 조사가 붙은 "예산을"도 "예산"으로 찾는다.
    std::vector<std::string> r = paths(idx, search(idx, parseQuery("예산")));
    CHECK(r.size() == 2 && contains(r, "a.hwp") && contains(r, "b.hwp"));
    // AND: 두 단어가 모두 들어 있는 문서만
    r = paths(idx, search(idx, parseQuery("예산 집행")));
    CHECK(r.size() == 1 && r[0] == "a.hwp");
    // 제외어
    r = paths(idx, search(idx, parseQuery("예산 -추경")));
    CHECK(r.size() == 1 && r[0] == "a.hwp");
    // 구문: "예산 계획"은 b에만 있다
    r = paths(idx, search(idx, parseQuery("\"예산 계획\"")));
    CHECK(r.size() == 1 && r[0] == "b.hwp");
    // 영문은 대소문자를 가리지 않는다.
    r = paths(idx, search(idx, parseQuery("viewer")));
    CHECK(r.size() == 1 && r[0] == "c.hwpx");
    // bigram 순서가 달라도 실제 문자열이 없으면 결과에서 뺀다: "산예"
    CHECK(search(idx, parseQuery("산예")).hits.empty());
    // 한 글자 검색
    CHECK(search(idx, parseQuery("록")).hits.size() == 1);
    // 없는 단어
    CHECK(search(idx, parseQuery("없는단어")).hits.empty());
    // limit
    SearchResult lim = search(idx, parseQuery("예산"), 1);
    CHECK(lim.hits.size() == 1 && lim.totalDocs == 2);
}

void testQueryParse() {
    Query q = parseQuery("예산  \"집행 결과\" -추경 HWP");
    CHECK(q.terms.size() == 3);
    CHECK(q.terms.size() == 3 && q.terms[1] == "집행 결과");
    CHECK(q.excludes.size() == 1 && q.excludes[0] == "추경");
    CHECK(parseQuery("   ").terms.empty());
}

void testSnippet() {
    std::string s = makeSnippet("2026년 예산을 집행한 결과", {"예산"}, "[", "]");
    CHECK(s == "2026년 [예산]을 집행한 결과");
    std::string longText(200, 'x');
    std::string s2 = makeSnippet(longText + "예산" + longText, {"예산"}, "[", "]", 5);
    CHECK(s2 == "...xxxxx[예산]xxxxxxxx...");
}

void testSaveLoad() {
    Index idx = makeIndex();
    std::filesystem::path tmp = std::filesystem::temp_directory_path() / "hanseek_test.idx";
    idx.save(tmp);
    Index loaded = Index::load(tmp);
    std::filesystem::remove(tmp);
    CHECK(loaded.documents().size() == 3);
    CHECK(loaded.paragraphCount() == idx.paragraphCount());
    CHECK(loaded.tokenCount() == idx.tokenCount());
    CHECK(loaded.documents()[2].path == "c.hwpx" && loaded.documents()[2].mtime == 3);
    std::vector<std::string> r = paths(loaded, search(loaded, parseQuery("예산 집행")));
    CHECK(r.size() == 1 && r[0] == "a.hwp");
}

void testBuildIndex() {
    IndexReport report;
    Index idx = buildIndex(utf8ToPath(g_fixtures), report);
    CHECK(report.scanned == 3);  // form.hwp, form.hwpx, sample.txt
    CHECK(report.indexed == 3);
    CHECK(report.failures.empty());
    CHECK(search(idx, parseQuery("교과목코드")).totalDocs == 2);
    CHECK(search(idx, parseQuery("HanSeek")).totalDocs == 1);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) g_fixtures = argv[1];
    std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"인코딩 변환", testUtf},
        {"토큰화", testTokenizer},
        {"HWP PARA_TEXT 해석", testParaText},
        {"HWPX XML 해석", testHwpxXml},
        {"HWP 파일 읽기", testHwpFile},
        {"HWPX 파일 읽기", testHwpxFile},
        {"지원하지 않는 파일", testUnsupported},
        {"검색어 해석", testQueryParse},
        {"검색", testSearch},
        {"문맥 미리보기", testSnippet},
        {"색인 저장과 불러오기", testSaveLoad},
        {"폴더 색인", testBuildIndex},
    };
    int passedTests = 0;
    for (auto& t : tests) {
        int before = g_failed;
        try {
            t.second();
        } catch (const std::exception& e) {
            ++g_failed;
            std::cout << "    예외: " << e.what() << "\n";
        }
        bool ok = g_failed == before;
        if (ok) ++passedTests;
        std::cout << (ok ? "[통과] " : "[실패] ") << t.first << "\n";
    }
    std::cout << "\n테스트 " << tests.size() << "개 중 " << passedTests << "개 통과, 검사 " << g_checks << "개 중 "
              << g_checks - g_failed << "개 통과\n";
    return g_failed == 0 ? 0 : 1;
}
