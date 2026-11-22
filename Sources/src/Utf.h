// 문자 인코딩 변환 도우미 (UTF-16LE, UTF-8, UTF-32, 경로)
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace hanseek {

// 코드포인트 하나를 UTF-8로 붙인다.
void appendUtf8(std::string& out, char32_t cp);

// UTF-8 문자열을 코드포인트 배열로 바꾼다. 잘못된 바이트는 U+FFFD로 바꾼다.
std::u32string utf8ToU32(const std::string& s);

// 코드포인트 배열을 UTF-8로 바꾼다.
std::string u32ToUtf8(const std::u32string& s);

// UTF-16LE 바이트 배열을 UTF-8로 바꾼다. (HWP 파일 안의 글자가 이 형식이다)
std::string utf16leToUtf8(const std::uint8_t* data, std::size_t byteLength);

// ASCII 영문자만 소문자로 바꾼다. 한글 바이트는 그대로 두므로 바이트 위치가 바뀌지 않는다.
std::string toLowerAscii(std::string s);

// 경로와 UTF-8 문자열 사이 변환 (Windows에서도 한글 경로가 깨지지 않게 한다)
std::string pathToUtf8(const std::filesystem::path& p);
std::filesystem::path utf8ToPath(const std::string& s);

}  // namespace hanseek
