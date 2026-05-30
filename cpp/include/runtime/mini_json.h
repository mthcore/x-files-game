// SPDX-License-Identifier: MIT
//
// A tiny header-only JSON reader tailored to the shape of the artifacts this
// project emits (game_definition.json, scene_asset_map.json, hotspots_*.json,
// playthrough_trace.json, ...). Accepts: null, bool, integer / floating
// numbers, double-quoted strings (standard escapes including \\u for BMP),
// arrays, objects. No exceptions: a parse error leaves the parser in a
// failed state with a one-line diagnostic; the DOM is the variant `Value`.
//
// Both the reference engine and the SDL2 playable shell include this header
// so they read the same bytes the same way.
#pragma once

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace xfiles::engine::json {

struct Value;
struct Array  { std::vector<Value> v; };
struct Object {
    // ordered insertion for stable iteration order
    std::vector<std::string> keys;
    std::map<std::string, Value> m;
};

struct Value {
    enum Kind { kNull, kBool, kInt, kReal, kStr, kArr, kObj };
    Kind        kind = kNull;
    bool        b   = false;
    long long   i   = 0;
    double      r   = 0.0;
    std::string s;
    std::shared_ptr<Array>  a;
    std::shared_ptr<Object> o;

    bool is_null() const { return kind == kNull; }
    bool is_obj()  const { return kind == kObj && o; }
    bool is_arr()  const { return kind == kArr && a; }
    bool is_str()  const { return kind == kStr; }
    bool is_num()  const { return kind == kInt || kind == kReal; }

    long long as_int() const { return kind == kReal ? (long long)r : i; }
    const std::string& as_str() const { return s; }
    const std::vector<Value>& as_arr() const {
        static std::vector<Value> e; return a ? a->v : e;
    }
    const std::map<std::string, Value>& as_obj() const {
        static std::map<std::string, Value> e; return o ? o->m : e;
    }

    const Value& at(const std::string& k) const {
        static Value null;
        if (!is_obj()) return null;
        auto it = o->m.find(k);
        return it == o->m.end() ? null : it->second;
    }
    const Value& at(std::size_t idx) const {
        static Value null;
        if (!is_arr() || idx >= a->v.size()) return null;
        return a->v[idx];
    }
    bool has(const std::string& k) const {
        return is_obj() && o->m.find(k) != o->m.end();
    }
};

class Parser {
public:
    Parser(const char* data, std::size_t n) : p_(data), end_(data + n) {}
    bool parse(Value& out) {
        skip_ws();
        if (!parse_value(out)) return false;
        skip_ws();
        if (p_ != end_) return fail("trailing data");
        return true;
    }
    const std::string& error() const { return error_; }

private:
    const char* p_;
    const char* end_;
    std::string error_;

    bool fail(const char* msg) {
        if (error_.empty()) {
            std::ostringstream ss;
            ss << "json parse error: " << msg;
            error_ = ss.str();
        }
        return false;
    }
    void skip_ws() {
        while (p_ < end_) {
            char c = *p_;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++p_;
            else break;
        }
    }
    bool parse_value(Value& v) {
        skip_ws();
        if (p_ >= end_) return fail("eof in value");
        char c = *p_;
        if (c == '{') return parse_object(v);
        if (c == '[') return parse_array(v);
        if (c == '"') return parse_string_val(v);
        if (c == 't' || c == 'f') return parse_bool(v);
        if (c == 'n') return parse_null(v);
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number(v);
        return fail("unexpected character");
    }
    bool parse_null(Value& v) {
        if (end_ - p_ < 4 || std::memcmp(p_, "null", 4) != 0) return fail("expected null");
        p_ += 4; v.kind = Value::kNull; return true;
    }
    bool parse_bool(Value& v) {
        if (end_ - p_ >= 4 && std::memcmp(p_, "true", 4) == 0) {
            p_ += 4; v.kind = Value::kBool; v.b = true; return true;
        }
        if (end_ - p_ >= 5 && std::memcmp(p_, "false", 5) == 0) {
            p_ += 5; v.kind = Value::kBool; v.b = false; return true;
        }
        return fail("expected bool");
    }
    bool parse_number(Value& v) {
        const char* start = p_;
        if (*p_ == '-') ++p_;
        while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        bool is_real = false;
        if (p_ < end_ && *p_ == '.') {
            is_real = true; ++p_;
            while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        }
        if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
            is_real = true; ++p_;
            if (p_ < end_ && (*p_ == '+' || *p_ == '-')) ++p_;
            while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        }
        std::string tok(start, p_ - start);
        if (tok.empty() || tok == "-") return fail("malformed number");
        if (is_real) { v.kind = Value::kReal; v.r = std::strtod(tok.c_str(), nullptr); }
        else         { v.kind = Value::kInt;  v.i = std::strtoll(tok.c_str(), nullptr, 10); }
        return true;
    }
    bool parse_string_raw(std::string& out) {
        if (p_ >= end_ || *p_ != '"') return fail("expected '\"'");
        ++p_;
        while (p_ < end_) {
            char c = *p_++;
            if (c == '"') return true;
            if (c == '\\') {
                if (p_ >= end_) return fail("eof in escape");
                char e = *p_++;
                switch (e) {
                    case '"':  out.push_back('"');  break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u': {
                        if (end_ - p_ < 4) return fail("eof in \\u escape");
                        unsigned code = 0;
                        for (int k = 0; k < 4; ++k) {
                            char h = *p_++;
                            unsigned digit;
                            if      (h >= '0' && h <= '9') digit = h - '0';
                            else if (h >= 'a' && h <= 'f') digit = 10 + (h - 'a');
                            else if (h >= 'A' && h <= 'F') digit = 10 + (h - 'A');
                            else return fail("bad \\u digit");
                            code = (code << 4) | digit;
                        }
                        if (code < 0x80) {
                            out.push_back((char)code);
                        } else if (code < 0x800) {
                            out.push_back((char)(0xC0 | (code >> 6)));
                            out.push_back((char)(0x80 | (code & 0x3F)));
                        } else {
                            out.push_back((char)(0xE0 | (code >> 12)));
                            out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
                            out.push_back((char)(0x80 | (code & 0x3F)));
                        }
                        break;
                    }
                    default: return fail("bad escape");
                }
            } else {
                out.push_back(c);
            }
        }
        return fail("eof in string");
    }
    bool parse_string_val(Value& v) {
        v.kind = Value::kStr;
        return parse_string_raw(v.s);
    }
    bool parse_array(Value& v) {
        if (*p_ != '[') return fail("expected '['");
        ++p_;
        v.kind = Value::kArr;
        v.a = std::make_shared<Array>();
        skip_ws();
        if (p_ < end_ && *p_ == ']') { ++p_; return true; }
        while (true) {
            Value e;
            if (!parse_value(e)) return false;
            v.a->v.push_back(std::move(e));
            skip_ws();
            if (p_ >= end_) return fail("eof in array");
            if (*p_ == ',') { ++p_; skip_ws(); continue; }
            if (*p_ == ']') { ++p_; return true; }
            return fail("expected ',' or ']'");
        }
    }
    bool parse_object(Value& v) {
        if (*p_ != '{') return fail("expected '{'");
        ++p_;
        v.kind = Value::kObj;
        v.o = std::make_shared<Object>();
        skip_ws();
        if (p_ < end_ && *p_ == '}') { ++p_; return true; }
        while (true) {
            skip_ws();
            std::string key;
            if (!parse_string_raw(key)) return false;
            skip_ws();
            if (p_ >= end_ || *p_ != ':') return fail("expected ':'");
            ++p_;
            Value child;
            if (!parse_value(child)) return false;
            if (v.o->m.find(key) == v.o->m.end()) v.o->keys.push_back(key);
            v.o->m[key] = std::move(child);
            skip_ws();
            if (p_ >= end_) return fail("eof in object");
            if (*p_ == ',') { ++p_; continue; }
            if (*p_ == '}') { ++p_; return true; }
            return fail("expected ',' or '}'");
        }
    }
};

inline bool load_file(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
}

}  // namespace xfiles::engine::json
