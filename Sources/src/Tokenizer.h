// 한국어와 영어 본문을 색인용 토큰으로 나눈다.
#pragma once

#include <string>
#include <vector>

namespace hanseek {

// 한글(과 한자, 가나)이 이어진 구간은 두 글자씩 겹쳐 자른다(bigram).
//   "예산을" -> "예산", "산을"   (조사가 붙어도 "예산"으로 찾을 수 있다)
//   한 글자뿐인 구간은 그 글자 하나를 토큰으로 쓴다.
// 영문과 숫자는 단어 단위로 자르고 소문자로 바꾼다.
//   "HWP 5.0" -> "hwp", "5", "0"
std::vector<std::string> tokenize(const std::string& text);

// 한 문단 안에서 중복을 없앤 토큰 목록 (색인에 넣을 때 쓴다)
std::vector<std::string> uniqueTokens(const std::string& text);

// 한글 구간 문자인지 (한글 음절, 자모, 한자, 가나)
bool isCjk(char32_t cp);

}  // namespace hanseek
