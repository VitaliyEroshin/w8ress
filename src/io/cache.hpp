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
    struct File {
        std::string filename;
        size_t length;
        char* content;
        bool uncached;
    };
public:
    Cache(size_t cache_size) : cache_size(cache_size) {}

    File GetFileContent(std::string file) {
        // std::cout << "Asked cache about " << file << std::endl;
        auto it = cached.find(file);
        if (it == cached.end()) {
            // std::cout << "Have not found in the cache, adding" << std::endl;
            return AddToCache(std::move(file));
        } else {
            // std::cout << "Found in the cache, touching and returning" << std::endl;
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

    File AddToCache(std::string file) {
        int desc = open(file.c_str(), O_RDONLY);
        struct stat file_stat;

        fstat(desc, &file_stat);
        size_t length = file_stat.st_size;

        File f;
        f.filename = file;
        f.length = length;
        f.content = reinterpret_cast<char*>(mmap(NULL, length, PROT_READ, MAP_PRIVATE, desc, 0));
        f.uncached = length > cache_size;

        if (length > cache_size) {
            return f;
        }

        while (total_size + length > cache_size) {
            Remove();
        }

        list.push_back(f);
        auto it = --list.end();
        cached.insert({file, it});
        total_size += length;
        return f;
    }

    File Touch(decltype(cached)::iterator cached_it, std::list<File>::iterator it) {
        File f = *it;
        list.erase(it);
        list.push_back(f);
        cached_it->second = --list.end();
        return f;
    }

    void Remove() {
        auto f = std::move(list.front());
        list.pop_front();

        auto it = cached.find(std::move(f.filename));
        cached.erase(it);

        total_size -= f.length;
        Unmap(std::move(f));
    }
};