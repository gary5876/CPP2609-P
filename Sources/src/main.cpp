// HanSeek CLI
//   hanseek index <폴더> [--db 파일]
//   hanseek search <검색어> [--db 파일] [--limit N]
//   hanseek extract <파일>
//   hanseek stats [--db 파일]
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "Document.h"
#include "Index.h"
#include "Searcher.h"
#include "Utf.h"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

using namespace hanseek;

const char* kDefaultDb = "hanseek.idx";

struct Options {
    std::string command;
    std::vector<std::string> args;
    std::string db = kDefaultDb;
    std::size_t limit = 10;
};

void printUsage() {
    std::cout << "HanSeek: 한컴오피스 없이 HWP, HWPX 본문을 찾는 로컬 검색 엔진\n\n"
              << "사용법:\n"
              << "  hanseek index <폴더> [--db 파일]       폴더 안의 HWP, HWPX, TXT 문서를 색인\n"
              << "  hanseek search <검색어> [--db 파일] [--limit N]\n"
              << "                                         색인에서 검색 (기본 10개)\n"
              << "  hanseek extract <파일>                 문서 한 개의 본문을 출력\n"
              << "  hanseek stats [--db 파일]              색인 정보 출력\n\n"
              << "검색어 예:\n"
              << "  hanseek search 예산 집행               두 단어가 모두 들어 있는 문서\n"
              << "  hanseek search \"\\\"예산 집행\\\"\"           구문 그대로 들어 있는 문서\n"
              << "  hanseek search 예산 -추경              \"추경\"이 들어 있는 문서는 제외\n\n"
              << "색인 파일 기본값: " << kDefaultDb << " (현재 폴더)\n";
}

bool parseOptions(const std::vector<std::string>& argv, Options& opt) {
    if (argv.empty()) return false;
    opt.command = argv[0];
    for (std::size_t i = 1; i < argv.size(); ++i) {
        if (argv[i] == "--db" && i + 1 < argv.size()) {
            opt.db = argv[++i];
        } else if (argv[i] == "--limit" && i + 1 < argv.size()) {
            try {
                opt.limit = static_cast<std::size_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "--limit 뒤에는 숫자를 적으세요\n";
                return false;
            }
        } else {
            opt.args.push_back(argv[i]);
        }
    }
    return true;
}

// 터미널이면 색으로 강조하고, 파일로 보낼 때는 [ ]로 강조한다.
bool useColor() {
#ifdef _WIN32
    if (!_isatty(_fileno(stdout))) return false;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) return false;
    return SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

int cmdIndex(const Options& opt) {
    if (opt.args.empty()) {
        std::cerr << "색인할 폴더를 적으세요. 예: hanseek index ~/Documents\n";
        return 2;
    }
    IndexReport report;
    Index index = buildIndex(utf8ToPath(opt.args[0]), report, [](std::size_t done, const std::string&) {
        if (done % 50 == 0) std::cerr << "\r읽은 문서: " << done << std::flush;
    });
    std::cerr << "\r";
    index.save(utf8ToPath(opt.db));
    std::cout << "색인 완료: 문서 " << report.indexed << "개 / 대상 " << report.scanned << "개, 문단 "
              << index.paragraphCount() << "개, 토큰 " << index.tokenCount() << "종류, " << std::fixed
              << std::setprecision(2) << report.seconds << "초\n";
    std::cout << "색인 파일: " << opt.db << "\n";
    if (!report.failures.empty()) {
        std::cout << "읽지 못한 문서 " << report.failures.size() << "개:\n";
        for (const auto& f : report.failures) std::cout << "  " << f.first << " (" << f.second << ")\n";
    }
    return 0;
}

int cmdSearch(const Options& opt) {
    if (opt.args.empty()) {
        std::cerr << "검색어를 적으세요. 예: hanseek search 예산 집행\n";
        return 2;
    }
    std::string text;
    for (const std::string& a : opt.args) text += (text.empty() ? "" : " ") + a;
    Query query = parseQuery(text);
    Index index = Index::load(utf8ToPath(opt.db));
    SearchResult result = search(index, query, opt.limit);

    bool color = useColor();
    std::string open = color ? "\033[1;33m" : "[", close = color ? "\033[0m" : "]";
    std::cout << "검색어: " << text << "\n결과: 문서 " << result.totalDocs << "개 (" << std::fixed
              << std::setprecision(2) << result.milliseconds << " ms)\n";
    int rank = 0;
    for (const SearchHit& hit : result.hits) {
        const DocEntry& d = index.documents()[hit.doc];
        std::cout << "\n" << ++rank << ". " << d.path << "  (일치 문단 " << hit.paragraphs.size() << "개)\n";
        std::size_t shown = 0;
        for (std::uint32_t p : hit.paragraphs) {
            std::cout << "   - " << makeSnippet(index.paragraph(p), query.terms, open, close) << "\n";
            if (++shown == 2) break;
        }
    }
    if (result.totalDocs > result.hits.size()) {
        std::cout << "\n... 외 " << result.totalDocs - result.hits.size() << "개 (--limit로 더 보기)\n";
    }
    return 0;
}

int cmdExtract(const Options& opt) {
    if (opt.args.empty()) {
        std::cerr << "본문을 볼 파일을 적으세요\n";
        return 2;
    }
    ReaderRegistry registry;
    Document doc = registry.read(utf8ToPath(opt.args[0]));
    std::cout << "# " << doc.path << " (" << doc.format << ", 문단 " << doc.paragraphs.size() << "개)\n";
    for (const std::string& p : doc.paragraphs) std::cout << p << "\n";
    return 0;
}

int cmdStats(const Options& opt) {
    Index index = Index::load(utf8ToPath(opt.db));
    std::size_t hwp = 0, hwpx = 0, txt = 0;
    for (const DocEntry& d : index.documents()) {
        if (d.format == "hwp") ++hwp;
        else if (d.format == "hwpx") ++hwpx;
        else ++txt;
    }
    std::cout << "색인 파일: " << opt.db << "\n문서: " << index.documents().size() << "개 (HWP " << hwp
              << ", HWPX " << hwpx << ", 텍스트 " << txt << ")\n문단: " << index.paragraphCount()
              << "개\n토큰 종류: " << index.tokenCount() << "개\n";
    return 0;
}

int run(const std::vector<std::string>& argv) {
    Options opt;
    if (!parseOptions(argv, opt) || opt.command == "help" || opt.command == "--help" || opt.command == "-h") {
        printUsage();
        return argv.empty() ? 2 : 0;
    }
    try {
        if (opt.command == "index") return cmdIndex(opt);
        if (opt.command == "search") return cmdSearch(opt);
        if (opt.command == "extract") return cmdExtract(opt);
        if (opt.command == "stats") return cmdStats(opt);
    } catch (const std::exception& e) {
        std::cerr << "오류: " << e.what() << "\n";
        return 1;
    }
    std::cerr << "알 수 없는 명령입니다: " << opt.command << "\n\n";
    printUsage();
    return 2;
}

}  // namespace

#ifdef _WIN32
// Windows는 명령줄 인자를 UTF-16으로 받아 UTF-8로 바꾸고, 콘솔 출력도 UTF-8로 맞춘다.
int wmain(int argc, wchar_t** wargv) {
    SetConsoleOutputCP(CP_UTF8);
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        int n = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(n > 0 ? n - 1 : 0, '\0');
        if (n > 1) WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, &s[0], n, nullptr, nullptr);
        args.push_back(s);
    }
    return run(args);
}
#else
int main(int argc, char** argv) {
    return run(std::vector<std::string>(argv + 1, argv + argc));
}
#endif
