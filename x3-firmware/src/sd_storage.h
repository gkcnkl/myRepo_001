#pragma once
// FreeInkBook storage adapters over the SD card (SdFat via SDCardManager).

#include <BookStorage.h>
#include <SDCardManager.h>
#include <stdio.h>

// A book file opened for random reads.
class SdBookSource : public freeink::book::BookSource {
 public:
  bool open(const char* path) {
    close();
    file_ = SdMan.open(path, O_RDONLY);
    if (!file_) return false;
    size_ = file_.fileSize();
    return true;
  }
  void close() {
    if (file_) file_.close();
  }
  int32_t readAt(uint64_t offset, void* dst, uint32_t len) override {
    if (!file_ || !file_.seekSet(offset)) return -1;
    return file_.read(dst, len);
  }
  uint64_t size() const override { return size_; }

 private:
  FsFile file_;
  uint64_t size_ = 0;
};

// Page cache files live in one directory per book (/kitaplar/.x3/<id>/).
class SdCacheStorage : public freeink::book::CacheStorage {
 public:
  void setDir(const char* dir) {
    snprintf(dir_, sizeof dir_, "%s", dir);
    SdMan.ensureDirectoryExists(dir_);
  }
  bool exists(const char* name) override { return SdMan.exists(path(name)); }
  bool remove(const char* name) override { return SdMan.remove(path(name)); }
  int64_t fileSize(const char* name) override {
    FsFile f = SdMan.open(path(name), O_RDONLY);
    if (!f) return -1;
    const int64_t s = f.fileSize();
    f.close();
    return s;
  }
  int32_t readAt(const char* name, uint32_t offset, void* dst, uint32_t len) override {
    // Keep the last read file open: page turns read the same cache file.
    if (!readFile_ || strcmp(readName_, name) != 0) {
      if (readFile_) readFile_.close();
      readFile_ = SdMan.open(path(name), O_RDONLY);
      snprintf(readName_, sizeof readName_, "%s", name);
      if (!readFile_) return -1;
    }
    if (!readFile_.seekSet(offset)) return -1;
    return readFile_.read(dst, len);
  }
  bool beginWrite(const char* name) override {
    if (readFile_) readFile_.close();  // the file may be the one being rewritten
    writeFile_ = SdMan.open(path(name), O_RDWR | O_CREAT | O_TRUNC);
    return static_cast<bool>(writeFile_);
  }
  bool write(const void* data, uint32_t len) override {
    return writeFile_ && writeFile_.write(static_cast<const uint8_t*>(data), len) == len;
  }
  bool endWrite() override {
    if (!writeFile_) return false;
    const bool ok = writeFile_.sync();
    writeFile_.close();
    return ok;
  }
  int32_t readBackAt(uint32_t offset, void* dst, uint32_t len) override {
    if (!writeFile_) return -1;
    const uint64_t cur = writeFile_.curPosition();
    if (!writeFile_.seekSet(offset)) return -1;
    const int32_t r = writeFile_.read(dst, len);
    writeFile_.seekSet(cur);
    return r;
  }
  void closeAll() {
    if (readFile_) readFile_.close();
    if (writeFile_) writeFile_.close();
  }

 private:
  const char* path(const char* name) {
    snprintf(path_, sizeof path_, "%s/%s", dir_, name);
    return path_;
  }
  char dir_[64] = "";
  char path_[128];
  char readName_[64] = "";
  FsFile readFile_;
  FsFile writeFile_;
};
