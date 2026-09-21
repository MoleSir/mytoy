#pragma once

#include "llvm/ADT/StringRef.h"
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <string>

namespace toy {

struct Location {
    std::shared_ptr<std::string> file;
    int line;
    int col;
};

enum Token : int {
    Semicolon = ';',
    ParentheseOpen = '(',
    ParentheseClose = ')',
    BracketOpen = '{',
    BracketClose = '}',
    SbracketOpen = '[',
    SbracketClose = ']',

    Eof = -1,

    Return = -2,
    Var = -3,
    Def = -4,

    Identifier = -5,
    Number = -6,
};

class Lexer {
public:
    Lexer(std::string filename) :
        last_location_{std::make_shared<std::string>(std::move(filename)), 0, 0}
    {
    }

    virtual ~Lexer() = default;

    Token get_cur_token() { return cur_token_; }

    Token get_next_token() { return cur_token_ = get_token(); }

    void consume(Token token) {
        assert(token == cur_token_);
        get_next_token();
    }

    llvm::StringRef get_id() {
        assert(cur_token_ == Identifier);
        return identifier_str_;
    }

    double get_value() {
        assert(cur_token_ == Number);
        return num_val_;
    }

    Location get_last_location() { return last_location_; }

    int get_line() { return cur_line_; }

    int get_col() { return cur_col_; }

private:
    virtual llvm::StringRef read_next_line() = 0;

    int get_next_char() {
        if (cur_line_buffer_.empty())
            return EOF;

        ++cur_col_;
        auto next_char = cur_line_buffer_.front();
        cur_line_buffer_ = cur_line_buffer_.drop_front();
        if (cur_line_buffer_.empty())
            cur_line_buffer_ = read_next_line();
        if (next_char == '\n') {
            ++cur_line_;
            cur_col_ = 0;
        }
        return next_char;
    }

    Token get_token() {
        while (isspace(last_char_))
            last_char_ = Token(get_next_char());

        last_location_.line = cur_line_;
        last_location_.col = cur_col_;

        if (isalpha(last_char_)) {
            identifier_str_ = static_cast<char>(last_char_);
            while (isalnum((last_char_ = Token(get_next_char()))) || last_char_ == '_')
                identifier_str_ += static_cast<char>(last_char_);

            if (identifier_str_ == "return")
                return Return;
            if (identifier_str_ == "def")
                return Def;
            if (identifier_str_ == "var")
                return Var;
            return Identifier;
        }

        if (isdigit(last_char_) || last_char_ == '.') {
            std::string num_str;
            do {
                num_str += static_cast<char>(last_char_);
                last_char_ = Token(get_next_char());
            } while (isdigit(last_char_) || last_char_ == '.');

            num_val_ = strtod(num_str.c_str(), nullptr);
            return Number;
        }

        if (last_char_ == '#') {
            do {
                last_char_ = Token(get_next_char());
            } while (last_char_ != EOF && last_char_ != '\n' && last_char_ != '\r');

            if (last_char_ != EOF)
                return get_token();
        }

        if (last_char_ == EOF)
            return Eof;

        auto this_char = Token(last_char_);
        last_char_ = Token(get_next_char());
        return this_char;
    }

    Token cur_token_ = Eof;
    Location last_location_;
    std::string identifier_str_;
    double num_val_ = 0;
    Token last_char_ = Token(' ');
    int cur_line_ = 0;
    int cur_col_ = 0;
    llvm::StringRef cur_line_buffer_ = "\n";
};

class LexerBuffer final : public Lexer {
public:
    LexerBuffer(const char *begin, const char *end, std::string filename) :
        Lexer(std::move(filename)), current_(begin), end_(end)
    {
    }

private:
    llvm::StringRef read_next_line() override {
        auto *begin = current_;
        while (current_ <= end_ && *current_ && *current_ != '\n')
            ++current_;

        if (current_ <= end_ && *current_)
            ++current_;

        return llvm::StringRef{begin, static_cast<size_t>(current_ - begin)};
    }

    const char *current_;
    const char *end_;
};

}
