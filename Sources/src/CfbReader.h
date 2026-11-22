// OLE 복합 파일(Compound File Binary) 읽기
// HWP 5.x 파일은 이 구조 안에 FileHeader, BodyText/Section0 같은 스트림을 담고 있다.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hanseek {

class CfbReader {
public:
    // 파일 내용 전체를 받아 헤더, FAT, 디렉터리를 해석한다. 형식이 틀리면 ReadError.
    explicit CfbReader(std::vector<unsigned char> data);

    // "BodyText/Section0"처럼 '/'로 구분한 경로의 스트림이 있는지
    bool hasStream(const std::string& path) const;
    // 스트림 내용을 읽는다. 없으면 ReadError.
    std::vector<unsigned char> readStream(const std::string& path) const;
    // 모든 스트림 경로 목록
    std::vector<std::string> streamPaths() const;

private:
    struct DirEntry {
        std::string name;
        int type = 0;  // 1: 저장소(폴더), 2: 스트림, 5: 루트
        std::uint32_t left = 0, right = 0, child = 0;
        std::uint32_t startSector = 0;
        std::uint64_t size = 0;
        std::string fullPath;
    };

    std::vector<unsigned char> data_;
    std::uint32_t sectorSize_ = 512;
    std::uint32_t miniSectorSize_ = 64;
    std::uint32_t miniCutoff_ = 4096;
    std::vector<std::uint32_t> fat_;
    std::vector<std::uint32_t> miniFat_;
    std::vector<DirEntry> entries_;
    std::vector<unsigned char> miniStream_;

    std::uint32_t u32(std::size_t offset) const;
    const unsigned char* sector(std::uint32_t id) const;
    std::vector<unsigned char> readChain(std::uint32_t start, std::uint64_t size) const;
    std::vector<unsigned char> readMiniChain(std::uint32_t start, std::uint64_t size) const;
    void buildPaths(std::uint32_t id, const std::string& parent, int depth);
    const DirEntry* findEntry(const std::string& path) const;
};

}  // namespace hanseek
