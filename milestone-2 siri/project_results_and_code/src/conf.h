// conf.h - tiny helpers for key=value config files and "--key value" command lines.
#pragma once
#include <sys/stat.h>

#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

struct Args {
    std::map<std::string, std::string> kv;
    // Config files first (--loads / --calib name files), then the command line wins.
    Args(int argc, char** argv, std::initializer_list<const char*> conf_keys = {}) {
        std::map<std::string, std::string> cl;
        for (int i = 1; i < argc; i += 2) {
            std::string k = argv[i];
            if (k.rfind("--", 0) != 0 || i + 1 >= argc) throw std::runtime_error("bad argument: " + k);
            cl[k.substr(2)] = argv[i + 1];
        }
        for (const char* ck : conf_keys)
            if (cl.count(ck)) load(cl[ck]);
        for (auto& p : cl) kv[p.first] = p.second;
    }
    void load(const std::string& path) {
        std::ifstream f(path);
        if (!f) throw std::runtime_error("cannot open " + path);
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            auto eq = line.find('=');
            if (eq != std::string::npos) kv[line.substr(0, eq)] = line.substr(eq + 1);
        }
    }
    bool has(const std::string& k) const { return kv.count(k) > 0; }
    std::string str(const std::string& k) const {
        auto it = kv.find(k);
        if (it == kv.end()) throw std::runtime_error("missing --" + k);
        return it->second;
    }
    std::string str(const std::string& k, const std::string& d) const { return has(k) ? kv.at(k) : d; }
    double num(const std::string& k) const { return std::atof(str(k).c_str()); }
    double num(const std::string& k, double d) const { return has(k) ? std::atof(kv.at(k).c_str()) : d; }
};

inline uint64_t file_size_of(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) throw std::runtime_error("cannot stat " + path);
    return static_cast<uint64_t>(st.st_size);
}
