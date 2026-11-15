// SPDX-License-Identifier: AGPL-3.0-or-later
// Minimal JSON reader for render cases (objects, arrays, numbers, strings, bools, null).
#pragma once

#include <cctype>
#include <cstdlib>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace glint::json {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::vector<Value> arr;
    std::vector<std::pair<std::string, Value>> obj; // keeps order

    bool isObject() const { return type == Type::Object; }
    bool isArray() const { return type == Type::Array; }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isBool() const { return type == Type::Bool; }

    const Value* find(const std::string& key) const
    {
        for (const auto& [k, v] : obj)
            if (k == key)
                return &v;
        return nullptr;
    }
    double number(const std::string& key, double def) const
    {
        const Value* v = find(key);
        if (!v) return def;
        if (v->isNumber()) return v->num;
        if (v->isBool()) return v->b ? 1.0 : 0.0;
        return def;
    }
    std::string string(const std::string& key, const std::string& def) const
    {
        const Value* v = find(key);
        return v && v->isString() ? v->str : def;
    }
};

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text) {}
    Value parse()
    {
        Value v = value();
        ws();
        if (i_ != s_.size())
            fail("trailing characters");
        return v;
    }

private:
    [[noreturn]] void fail(const char* what) { throw std::runtime_error(std::string("JSON: ") + what + " at " + std::to_string(i_)); }
    void ws()
    {
        while (i_ < s_.size() && (std::isspace(static_cast<unsigned char>(s_[i_])) || s_[i_] == '/')) {
            if (s_[i_] == '/' && i_ + 1 < s_.size() && s_[i_ + 1] == '/') {
                while (i_ < s_.size() && s_[i_] != '\n') ++i_;
            } else if (s_[i_] == '/') {
                fail("unexpected /");
            } else {
                ++i_;
            }
        }
    }
    Value value()
    {
        ws();
        if (i_ >= s_.size()) fail("unexpected end");
        const char c = s_[i_];
        Value v;
        if (c == '{') {
            v.type = Value::Type::Object;
            ++i_;
            ws();
            if (s_[i_] == '}') { ++i_; return v; }
            for (;;) {
                ws();
                std::string k = string();
                ws();
                if (s_[i_++] != ':') fail("expected :");
                v.obj.emplace_back(std::move(k), value());
                ws();
                if (s_[i_] == ',') { ++i_; continue; }
                if (s_[i_] == '}') { ++i_; break; }
                fail("expected , or }");
            }
        } else if (c == '[') {
            v.type = Value::Type::Array;
            ++i_;
            ws();
            if (s_[i_] == ']') { ++i_; return v; }
            for (;;) {
                v.arr.push_back(value());
                ws();
                if (s_[i_] == ',') { ++i_; continue; }
                if (s_[i_] == ']') { ++i_; break; }
                fail("expected , or ]");
            }
        } else if (c == '"') {
            v.type = Value::Type::String;
            v.str = string();
        } else if (s_.compare(i_, 4, "true") == 0) {
            v.type = Value::Type::Bool; v.b = true; i_ += 4;
        } else if (s_.compare(i_, 5, "false") == 0) {
            v.type = Value::Type::Bool; v.b = false; i_ += 5;
        } else if (s_.compare(i_, 4, "null") == 0) {
            i_ += 4;
        } else {
            char* end = nullptr;
            v.type = Value::Type::Number;
            v.num = std::strtod(s_.c_str() + i_, &end);
            if (end == s_.c_str() + i_) fail("bad value");
            i_ = static_cast<size_t>(end - s_.c_str());
        }
        return v;
    }
    std::string string()
    {
        if (s_[i_] != '"') fail("expected string");
        ++i_;
        std::string out;
        while (i_ < s_.size() && s_[i_] != '"') {
            if (s_[i_] == '\\' && i_ + 1 < s_.size()) {
                ++i_;
                const char e = s_[i_];
                out += e == 'n' ? '\n' : e == 't' ? '\t' : e;
            } else {
                out += s_[i_];
            }
            ++i_;
        }
        ++i_;
        return out;
    }

    const std::string& s_;
    size_t i_ = 0;
};

inline Value parse(const std::string& text) { return Parser(text).parse(); }

} // namespace glint::json
