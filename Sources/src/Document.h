// 문서 한 개의 본문과, 형식별 읽기 클래스가 따르는 공통 인터페이스
#pragma once

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace hanseek {

// 형식과 상관없이 추출 결과는 이 구조로 통일한다.
struct Document {
    std::string path;                     // 파일 경로 (UTF-8)
    std::string format;                   // "hwp", "hwpx", "txt"
    std::vector<std::string> paragraphs;  // 문단별 본문 (UTF-8, 빈 문단 제외)
};

// 문서를 읽지 못했을 때 (암호 문서, 깨진 파일 등) 던지는 예외
class ReadError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// 형식별 읽기 클래스의 공통 인터페이스 (순수 가상 함수)
class DocumentReader {
public:
    virtual ~DocumentReader() = default;
    virtual std::string format() const = 0;
    // 확장자(소문자, 점 제외)를 보고 읽을 수 있는지 답한다.
    virtual bool supports(const std::string& extension) const = 0;
    virtual Document read(const std::filesystem::path& file) const = 0;
};

// 등록된 읽기 클래스 중 확장자에 맞는 것을 골라 준다.
class ReaderRegistry {
public:
    ReaderRegistry();  // 기본 읽기 클래스(HWP, HWPX, TXT)를 등록한다.
    const DocumentReader* find(const std::filesystem::path& file) const;
    Document read(const std::filesystem::path& file) const;

private:
    std::vector<std::unique_ptr<DocumentReader>> readers_;
};

// 파일 전체를 바이트 배열로 읽는다.
std::vector<unsigned char> readAllBytes(const std::filesystem::path& file);

// 문단 끝 공백을 지우고, 내용이 있으면 문서에 넣는다.
void addParagraph(Document& doc, std::string text);

}  // namespace hanseek
