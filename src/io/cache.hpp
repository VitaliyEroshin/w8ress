#pragma once

#include <fcntl.h>
#include <iostream>
#include <list>
#include <sys/mman.h>
#include <sys/stat.h>
#include <vector>
#include <string>
#include <unordered_map>

class Cache {
public:
    struct File {
        std::string filename;
        size_t length;
        char* content;
        bool uncached;

        ~File() {
            if (!filename.empty() && uncached) {
                Unmap(std::move(*this));
            }
        }

        File(std::string filename, size_t length, char *content)
            : filename(std::move(filename)),
              length(length),
              content(content),
              uncached(false) {}
        
        void MakeUncached() {
            uncached = true;
        }

        File Clone() {
            return {filename, length, content};
        }

        File(File&&) noexcept = default;
        File& operator=(File&&) noexcept = default;

        File(const File& other) = delete;
        File& operator=(const File& other) = delete;
    };

    Cache(size_t cache_size) : cache_size(cache_size) {}

    File GetFileContent(std::string file) {
        auto it = cached.find(file);
        if (it == cached.end()) {
            return AddToCache(std::move(file));
        } else {
            return Touch(it, it->second);
        }
    }

    static void Unmap(File f) {
        munmap(f.content, f.length);
    }

private:
    size_t cache_size = 0;
    size_t total_size = 0;
    std::unordered_map<std::string, std::list<File>::iterator> cached;
    std::list<File> list;

    File AddToCache(std::string filename) {
        int desc = open(filename.c_str(), O_RDONLY);
        struct stat file_stat;

        fstat(desc, &file_stat);
        size_t length = file_stat.st_size;

        char* content = reinterpret_cast<char*>(mmap(NULL, length, PROT_READ, MAP_PRIVATE, desc, 0));
        File f(filename, length, content);

        if (length > cache_size) {
            f.MakeUncached();
            return f;
        }

        while (total_size + length > cache_size) {
            Remove();
        }

        list.push_back(f.Clone());
        auto it = --list.end();
        cached.insert({filename, it});
        total_size += length;
        return f;
    }

    File Touch(decltype(cached)::iterator cached_it, std::list<File>::iterator it) {
        File f = std::move(*it);
        list.erase(it);
        list.push_back(f.Clone());
        cached_it->second = --list.end();
        return f;
    }

    void Remove() {
        auto f = std::move(list.front());
        list.pop_front();

        auto it = cached.find(f.filename);
        cached.erase(it);

        total_size -= f.length;
        f.MakeUncached();
    }
};