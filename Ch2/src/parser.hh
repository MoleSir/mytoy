#pragma once

#include "ast.hh"
#include "lexer.hh"

#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <cctype>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace toy {

class Parser {
public:
    Parser(Lexer &lexer) : lexer_(lexer) {}

    std::unique_ptr<Module> parse_module() {
        lexer_.get_next_token();

        std::vector<Function> functions;
        while (auto function = parse_definition()) {
            functions.push_back(std::move(*function));
            if (lexer_.get_cur_token() == Eof)
                break;
        }

        if (lexer_.get_cur_token() != Eof)
            return parse_error<Module>("nothing", "at end of module");

        return std::make_unique<Module>(std::move(functions));
    }

private:
    std::unique_ptr<ReturnExpr> parse_return() {
        auto loc = lexer_.get_last_location();
        lexer_.consume(Return);

        std::optional<std::unique_ptr<Expr>> expr;
        if (lexer_.get_cur_token() != ';') {
            expr = parse_expression();
            if (!expr)
                return nullptr;
        }
        return std::make_unique<ReturnExpr>(std::move(loc), std::move(expr));
    }

    std::unique_ptr<Expr> parse_number_expr() {
        auto loc = lexer_.get_last_location();
        auto result = std::make_unique<NumberExpr>(std::move(loc), lexer_.get_value());
        lexer_.consume(Number);
        return result;
    }

    std::unique_ptr<Expr> parse_tensor_literal_expr() {
        auto loc = lexer_.get_last_location();
        lexer_.consume(SbracketOpen);

        std::vector<std::unique_ptr<Expr>> values;
        std::vector<int64_t> dims;
        do {
            if (lexer_.get_cur_token() == SbracketOpen) {
                values.push_back(parse_tensor_literal_expr());
                if (!values.back())
                    return nullptr;
            } else {
                if (lexer_.get_cur_token() != Number)
                    return parse_error<Expr>("<num> or [", "in literal expression");
                values.push_back(parse_number_expr());
            }

            if (lexer_.get_cur_token() == SbracketClose)
                break;

            if (lexer_.get_cur_token() != ',')
                return parse_error<Expr>("] or ,", "in literal expression");

            lexer_.get_next_token();
        } while (true);

        if (values.empty())
            return parse_error<Expr>("<something>", "to fill literal expression");
        lexer_.get_next_token();

        dims.push_back(values.size());

        if (llvm::any_of(values, [](std::unique_ptr<Expr> &expr) {
                return llvm::isa<LiteralExpr>(expr.get());
            })) {
            auto *first_literal = llvm::dyn_cast<LiteralExpr>(values.front().get());
            if (!first_literal)
                return parse_error<Expr>("uniform well-nested dimensions",
                                         "inside literal expression");

            auto first_dims = first_literal->get_dims();
            dims.insert(dims.end(), first_dims.begin(), first_dims.end());

            for (auto &expr : values) {
                auto *expr_literal = llvm::cast<LiteralExpr>(expr.get());
                if (!expr_literal)
                    return parse_error<Expr>("uniform well-nested dimensions",
                                             "inside literal expression");
                if (expr_literal->get_dims() != first_dims)
                    return parse_error<Expr>("uniform well-nested dimensions",
                                             "inside literal expression");
            }
        }

        return std::make_unique<LiteralExpr>(std::move(loc), std::move(values), std::move(dims));
    }

    std::unique_ptr<Expr> parse_paren_expr() {
        lexer_.get_next_token();
        auto expr = parse_expression();
        if (!expr)
            return nullptr;

        if (lexer_.get_cur_token() != ParentheseClose)
            return parse_error<Expr>(")", "to close expression with parentheses");

        lexer_.consume(ParentheseClose);
        return expr;
    }

    std::unique_ptr<Expr> parse_identifier_expr() {
        std::string name(lexer_.get_id());

        auto loc = lexer_.get_last_location();
        lexer_.get_next_token();

        if (lexer_.get_cur_token() != ParentheseOpen)
            return std::make_unique<VariableExpr>(std::move(loc), name);

        lexer_.consume(ParentheseOpen);
        std::vector<std::unique_ptr<Expr>> args;
        if (lexer_.get_cur_token() != ParentheseClose) {
            while (true) {
                if (auto arg = parse_expression())
                    args.push_back(std::move(arg));
                else
                    return nullptr;

                if (lexer_.get_cur_token() == ParentheseClose)
                    break;

                if (lexer_.get_cur_token() != ',')
                    return parse_error<Expr>(", or )", "in argument list");
                lexer_.get_next_token();
            }
        }
        lexer_.consume(ParentheseClose);

        if (name == "print") {
            if (args.size() != 1)
                return parse_error<Expr>("<single arg>", "as argument to print()");

            return std::make_unique<PrintExpr>(std::move(loc), std::move(args[0]));
        }

        return std::make_unique<CallExpr>(std::move(loc), name, std::move(args));
    }

    std::unique_ptr<Expr> parse_primary() {
        switch (lexer_.get_cur_token()) {
        default:
            llvm::errs() << "unknown token '" << lexer_.get_cur_token()
                         << "' when expecting an expression\n";
            return nullptr;
        case Identifier:
            return parse_identifier_expr();
        case Number:
            return parse_number_expr();
        case ParentheseOpen:
            return parse_paren_expr();
        case SbracketOpen:
            return parse_tensor_literal_expr();
        case Semicolon:
            return nullptr;
        case BracketClose:
            return nullptr;
        }
    }

    std::unique_ptr<Expr> parse_bin_op_rhs(int expr_prec, std::unique_ptr<Expr> lhs) {
        while (true) {
            int token_prec = get_token_precedence();

            if (token_prec < expr_prec)
                return lhs;

            int bin_op = lexer_.get_cur_token();
            lexer_.consume(Token(bin_op));
            auto loc = lexer_.get_last_location();

            auto rhs = parse_primary();
            if (!rhs)
                return parse_error<Expr>("expression", "to complete binary operator");

            int next_prec = get_token_precedence();
            if (token_prec < next_prec) {
                rhs = parse_bin_op_rhs(token_prec + 1, std::move(rhs));
                if (!rhs)
                    return nullptr;
            }

            lhs = std::make_unique<BinaryExpr>(std::move(loc), bin_op, std::move(lhs),
                                               std::move(rhs));
        }
    }

    std::unique_ptr<Expr> parse_expression() {
        auto lhs = parse_primary();
        if (!lhs)
            return nullptr;

        return parse_bin_op_rhs(0, std::move(lhs));
    }

    std::unique_ptr<VarType> parse_type() {
        if (lexer_.get_cur_token() != '<')
            return parse_error<VarType>("<", "to begin type");
        lexer_.get_next_token();

        auto type = std::make_unique<VarType>();

        while (lexer_.get_cur_token() == Number) {
            type->shape.push_back(lexer_.get_value());
            lexer_.get_next_token();
            if (lexer_.get_cur_token() == ',')
                lexer_.get_next_token();
        }

        if (lexer_.get_cur_token() != '>')
            return parse_error<VarType>(">", "to end type");
        lexer_.get_next_token();
        return type;
    }

    std::unique_ptr<VarDeclExpr> parse_declaration() {
        if (lexer_.get_cur_token() != Var)
            return parse_error<VarDeclExpr>("var", "to begin declaration");
        auto loc = lexer_.get_last_location();
        lexer_.get_next_token();

        if (lexer_.get_cur_token() != Identifier)
            return parse_error<VarDeclExpr>("identified", "after 'var' declaration");
        std::string id(lexer_.get_id());
        lexer_.get_next_token();

        std::unique_ptr<VarType> type;
        if (lexer_.get_cur_token() == '<') {
            type = parse_type();
            if (!type)
                return nullptr;
        }

        if (!type)
            type = std::make_unique<VarType>();
        lexer_.consume(Token('='));
        auto expr = parse_expression();
        return std::make_unique<VarDeclExpr>(std::move(loc), std::move(id), std::move(*type),
                                             std::move(expr));
    }

    std::unique_ptr<ExprList> parse_block() {
        if (lexer_.get_cur_token() != BracketOpen)
            return parse_error<ExprList>("{", "to begin block");
        lexer_.consume(BracketOpen);

        auto expr_list = std::make_unique<ExprList>();

        while (lexer_.get_cur_token() == Semicolon)
            lexer_.consume(Semicolon);

        while (lexer_.get_cur_token() != BracketClose && lexer_.get_cur_token() != Eof) {
            if (lexer_.get_cur_token() == Var) {
                auto var_decl = parse_declaration();
                if (!var_decl)
                    return nullptr;
                expr_list->push_back(std::move(var_decl));
            } else if (lexer_.get_cur_token() == Return) {
                auto ret = parse_return();
                if (!ret)
                    return nullptr;
                expr_list->push_back(std::move(ret));
            } else {
                auto expr = parse_expression();
                if (!expr)
                    return nullptr;
                expr_list->push_back(std::move(expr));
            }

            if (lexer_.get_cur_token() != Semicolon)
                return parse_error<ExprList>(";", "after expression");

            while (lexer_.get_cur_token() == Semicolon)
                lexer_.consume(Semicolon);
        }

        if (lexer_.get_cur_token() != BracketClose)
            return parse_error<ExprList>("}", "to close block");

        lexer_.consume(BracketClose);
        return expr_list;
    }

    std::unique_ptr<Prototype> parse_prototype() {
        auto loc = lexer_.get_last_location();

        if (lexer_.get_cur_token() != Def)
            return parse_error<Prototype>("def", "in prototype");
        lexer_.consume(Def);

        if (lexer_.get_cur_token() != Identifier)
            return parse_error<Prototype>("function name", "in prototype");

        std::string fn_name(lexer_.get_id());
        lexer_.consume(Identifier);

        if (lexer_.get_cur_token() != ParentheseOpen)
            return parse_error<Prototype>("(", "in prototype");
        lexer_.consume(ParentheseOpen);

        std::vector<std::unique_ptr<VariableExpr>> args;
        if (lexer_.get_cur_token() != ParentheseClose) {
            do {
                std::string name(lexer_.get_id());
                auto arg_loc = lexer_.get_last_location();
                lexer_.consume(Identifier);
                auto decl = std::make_unique<VariableExpr>(std::move(arg_loc), name);
                args.push_back(std::move(decl));
                if (lexer_.get_cur_token() != ',')
                    break;
                lexer_.consume(Token(','));
                if (lexer_.get_cur_token() != Identifier)
                    return parse_error<Prototype>("identifier",
                                                  "after ',' in function parameter list");
            } while (true);
        }

        if (lexer_.get_cur_token() != ParentheseClose)
            return parse_error<Prototype>(")", "to end function prototype");

        lexer_.consume(ParentheseClose);
        return std::make_unique<Prototype>(std::move(loc), fn_name, std::move(args));
    }

    std::unique_ptr<Function> parse_definition() {
        auto proto = parse_prototype();
        if (!proto)
            return nullptr;

        if (auto block = parse_block())
            return std::make_unique<Function>(std::move(proto), std::move(block));
        return nullptr;
    }

    int get_token_precedence() {
        if (!isascii(lexer_.get_cur_token()))
            return -1;

        switch (static_cast<char>(lexer_.get_cur_token())) {
        case '-':
            return 20;
        case '+':
            return 20;
        case '*':
            return 40;
        default:
            return -1;
        }
    }

    template <typename R, typename T, typename U = const char *>
    std::unique_ptr<R> parse_error(T &&expected, U &&context = "") {
        auto cur_token = lexer_.get_cur_token();
        llvm::errs() << "Parse error (" << lexer_.get_last_location().line << ", "
                     << lexer_.get_last_location().col << "): expected '" << expected << "' "
                     << context << " but has Token " << cur_token;
        if (isprint(cur_token))
            llvm::errs() << " '" << static_cast<char>(cur_token) << "'";
        llvm::errs() << "\n";
        return nullptr;
    }

    Lexer &lexer_;
};

}
