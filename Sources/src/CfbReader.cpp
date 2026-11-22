#include "CfbReader.h"

#include <cstring>

#include "Document.h"
#include "Utf.h"

namespace hanseek {

namespace {
const unsigned char kSignature[8] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
const std::uint32_t kEndOfChain = 0xFFFFFFFE;
const std::uint32_t kNoStream = 0xFFFFFFFF;
const std::uint32_t kMaxRegularSector = 0xFFFFFFFA;
}  // namespace

std::uint32_t CfbReader::u32(std::size_t offset) const {
    if (offset + 4 > data_.size()) throw ReadError("CFB: 파일이 잘렸습니다");
    return data_[offset] | (data_[offset + 1] << 8) | (data_[offset + 2] << 16) |
           (static_cast<std::uint32_t>(data_[offset + 3]) << 24);
}

const unsigned char* CfbReader::sector(std::uint32_t id) const {
    std::uint64_t offset = (static_cast<std::uint64_t>(id) + 1) * sectorSize_;
    if (id > kMaxRegularSector || offset + sectorSize_ > data_.size()) {
        throw ReadError("CFB: 섹터 번호가 파일 범위를 벗어났습니다");
    }
    return data_.data() + offset;
}

CfbReader::CfbReader(std::vector<unsigned char> data) : data_(std::move(data)) {
    if (data_.size() < 512 || std::memcmp(data_.data(), kSignature, 8) != 0) {
        throw ReadError("OLE 복합 파일이 아닙니다");
    }
    std::uint16_t sectorShift = data_[0x1E] | (data_[0x1F] << 8);
    std::uint16_t miniShift = data_[0x20] | (data_[0x21] << 8);
    if (sectorShift != 9 && sectorShift != 12) throw ReadError("CFB: 지원하지 않는 섹터 크기");
    sectorSize_ = 1u << sectorShift;
    miniSectorSize_ = 1u << miniShift;
    miniCutoff_ = u32(0x38);
    std::uint32_t numFatSectors = u32(0x2C);
    std::uint32_t firstDirSector = u32(0x30);
    std::uint32_t firstMiniFatSector = u32(0x3C);
    std::uint32_t numMiniFatSectors = u32(0x40);
    std::uint32_t firstDifatSector = u32(0x44);
    std::uint32_t numDifatSectors = u32(0x48);

    // 1) DIFAT: FAT 섹터들의 위치 목록. 헤더에 109개, 넘치면 별도 섹터 체인에 이어진다.
    std::vector<std::uint32_t> fatSectorIds;
    for (int i = 0; i < 109 && fatSectorIds.size() < numFatSectors; ++i) {
        fatSectorIds.push_back(u32(0x4C + 4 * i));
    }
    std::uint32_t difat = firstDifatSector;
    std::uint32_t perSector = sectorSize_ / 4;
    for (std::uint32_t n = 0; n < numDifatSectors && difat <= kMaxRegularSector; ++n) {
        const unsigned char* s = sector(difat);
        for (std::uint32_t i = 0; i + 1 < perSector && fatSectorIds.size() < numFatSectors; ++i) {
            std::uint32_t v;
            std::memcpy(&v, s + 4 * i, 4);
            fatSectorIds.push_back(v);
        }
        std::memcpy(&difat, s + 4 * (perSector - 1), 4);
    }

    // 2) FAT: 섹터마다 "다음 섹터 번호"를 적은 표
    for (std::uint32_t id : fatSectorIds) {
        const unsigned char* s = sector(id);
        for (std::uint32_t i = 0; i < perSector; ++i) {
            std::uint32_t v;
            std::memcpy(&v, s + 4 * i, 4);
            fat_.push_back(v);
        }
    }

    // 3) 디렉터리: 128바이트 항목들의 배열
    std::vector<unsigned char> dir = readChain(firstDirSector, 0);
    for (std::size_t off = 0; off + 128 <= dir.size(); off += 128) {
        DirEntry e;
        std::uint16_t nameLen = dir[off + 0x40] | (dir[off + 0x41] << 8);
        if (nameLen >= 2 && nameLen <= 64) e.name = utf16leToUtf8(dir.data() + off, nameLen - 2);
        e.type = dir[off + 0x42];
        std::memcpy(&e.left, dir.data() + off + 0x44, 4);
        std::memcpy(&e.right, dir.data() + off + 0x48, 4);
        std::memcpy(&e.child, dir.data() + off + 0x4C, 4);
        std::memcpy(&e.startSector, dir.data() + off + 0x74, 4);
        std::uint32_t sizeLow;
        std::memcpy(&sizeLow, dir.data() + off + 0x78, 4);
        e.size = sizeLow;  // 512바이트 섹터 버전은 하위 4바이트만 쓴다
        entries_.push_back(e);
    }
    if (entries_.empty() || entries_[0].type != 5) throw ReadError("CFB: 루트 항목이 없습니다");

    // 4) 미니 스트림: 작은 스트림(4096바이트 미만)은 루트의 미니 스트림 안에 64바이트 단위로 들어 있다.
    if (numMiniFatSectors > 0 && firstMiniFatSector <= kMaxRegularSector) {
        std::vector<unsigned char> mf = readChain(firstMiniFatSector, 0);
        for (std::size_t i = 0; i + 4 <= mf.size(); i += 4) {
            std::uint32_t v;
            std::memcpy(&v, mf.data() + i, 4);
            miniFat_.push_back(v);
        }
    }
    if (entries_[0].startSector <= kMaxRegularSector) {
        miniStream_ = readChain(entries_[0].startSector, entries_[0].size);
    }

    // 5) 이진 트리로 된 디렉터리를 따라가며 "폴더/이름" 경로를 붙인다.
    buildPaths(entries_[0].child, "", 0);
}

std::vector<unsigned char> CfbReader::readChain(std::uint32_t start, std::uint64_t size) const {
    std::vector<unsigned char> out;
    std::uint32_t id = start;
    std::size_t guard = 0;
    while (id != kEndOfChain && id <= kMaxRegularSector) {
        if (++guard > fat_.size() + 1) throw ReadError("CFB: 섹터 체인이 순환합니다");
        const unsigned char* s = sector(id);
        out.insert(out.end(), s, s + sectorSize_);
        if (size > 0 && out.size() >= size) break;
        if (id >= fat_.size()) throw ReadError("CFB: FAT 범위를 벗어났습니다");
        id = fat_[id];
    }
    if (size > 0 && out.size() > size) out.resize(static_cast<std::size_t>(size));
    return out;
}

std::vector<unsigned char> CfbReader::readMiniChain(std::uint32_t start, std::uint64_t size) const {
    std::vector<unsigned char> out;
    std::uint32_t id = start;
    std::size_t guard = 0;
    while (id != kEndOfChain && id <= kMaxRegularSector && out.size() < size) {
        if (++guard > miniFat_.size() + 1) throw ReadError("CFB: 미니 섹터 체인이 순환합니다");
        std::size_t off = static_cast<std::size_t>(id) * miniSectorSize_;
        if (off + miniSectorSize_ > miniStream_.size()) throw ReadError("CFB: 미니 섹터 범위 오류");
        out.insert(out.end(), miniStream_.begin() + off, miniStream_.begin() + off + miniSectorSize_);
        if (id >= miniFat_.size()) break;
        id = miniFat_[id];
    }
    if (out.size() > size) out.resize(static_cast<std::size_t>(size));
    return out;
}

void CfbReader::buildPaths(std::uint32_t id, const std::string& parent, int depth) {
    // 형제는 left/right, 하위 항목은 child로 이어진다. 깨진 파일의 무한 재귀를 막는다.
    if (id == kNoStream || id >= entries_.size() || depth > 64) return;
    DirEntry& e = entries_[id];
    if (!e.fullPath.empty()) return;  // 이미 방문함
    e.fullPath = parent.empty() ? e.name : parent + "/" + e.name;
    buildPaths(e.left, parent, depth + 1);
    buildPaths(e.right, parent, depth + 1);
    if (e.type == 1) buildPaths(e.child, e.fullPath, depth + 1);
}

const CfbReader::DirEntry* CfbReader::findEntry(const std::string& path) const {
    for (const DirEntry& e : entries_) {
        if (e.type == 2 && e.fullPath == path) return &e;
    }
    return nullptr;
}

bool CfbReader::hasStream(const std::string& path) const {
    return findEntry(path) != nullptr;
}

std::vector<unsigned char> CfbReader::readStream(const std::string& path) const {
    const DirEntry* e = findEntry(path);
    if (e == nullptr) throw ReadError("CFB: 스트림이 없습니다: " + path);
    if (e->size == 0) return {};
    if (e->size < miniCutoff_) return readMiniChain(e->startSector, e->size);
    return readChain(e->startSector, e->size);
}

std::vector<std::string> CfbReader::streamPaths() const {
    std::vector<std::string> out;
    for (const DirEntry& e : entries_) {
        if (e.type == 2 && !e.fullPath.empty()) out.push_back(e.fullPath);
    }
    return out;
}

}  // namespace hanseek
