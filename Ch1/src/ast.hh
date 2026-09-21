#pragma once

#include "lexer.hh"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace toy {

struct VarType {
    std::vector<int64_t> shape;
};

enum class ExprKind {
    VarDecl,
    Return,
    Number,
    Literal,
    Variable,
    Binary,
    Call,
    Print,
};

class Expr {
public:
    Expr(ExprKind kind, Location location) :
        kind_(kind), location_(std::move(location))
    {
    }

    virtual ~Expr() = default;

    ExprKind get_kind() const { return kind_; }

    const Location &loc() { return location_; }

private:
    const ExprKind kind_;
    Location location_;
};

using ExprList = std::vector<std::unique_ptr<Expr>>;

class NumberExpr : public Expr {
public:
    NumberExpr(Location loc, double val) :
        Expr(ExprKind::Number, std::move(loc)), val_(val)
    {
    }

    double get_value() { return val_; }

    static bool classof(const Expr *expr) { return expr->get_kind() == ExprKind::Number; }

private:
    double val_;
};

class LiteralExpr : public Expr {
public:
    LiteralExpr(Location loc, std::vector<std::unique_ptr<Expr>> values, std::vector<int64_t> dims) :
        Expr(ExprKind::Literal, std::move(loc)),
        values_(std::move(values)),
        dims_(std::move(dims))
    {
    }

    llvm::ArrayRef<std::unique_ptr<Expr>> get_values() { return values_; }

    llvm::ArrayRef<int64_t> get_dims() { return dims_; }

    static bool classof(const Expr *expr) { return expr->get_kind() == ExprKind::Literal; }

private:
    std::vector<std::unique_ptr<Expr>> values_;
    std::vector<int64_t> dims_;
};

class VariableExpr : public Expr {
public:
    VariableExpr(Location loc, llvm::StringRef name) :
        Expr(ExprKind::Variable, std::move(loc)), name_(name)
    {
    }

    llvm::StringRef get_name() { return name_; }

    static bool classof(const Expr *expr) { return expr->get_kind() == ExprKind::Variable; }

private:
    std::string name_;
};

class VarDeclExpr : public Expr {
public:
    VarDeclExpr(Location loc, llvm::StringRef name, VarType type, std::unique_ptr<Expr> init_val) :
        Expr(ExprKind::VarDecl, std::move(loc)),
        name_(name),
        type_(std::move(type)),
        init_val_(std::move(init_val))
    {
    }

    llvm::StringRef get_name() { return name_; }

    const VarType &get_type() { return type_; }

    Expr *get_init_val() { return init_val_.get(); }

    static bool classof(const Expr *expr) { return expr->get_kind() == ExprKind::VarDecl; }

private:
    std::string name_;
    VarType type_;
    std::unique_ptr<Expr> init_val_;
};

class ReturnExpr : public Expr {
public:
    ReturnExpr(Location loc, std::optional<std::unique_ptr<Expr>> expr) :
        Expr(ExprKind::Return, std::move(loc)), expr_(std::move(expr))
    {
    }

    std::optional<Expr *> get_expr() {
        if (expr_.has_value())
            return expr_->get();
        return std::nullopt;
    }

    static bool classof(const Expr *expr) { return expr->get_kind() == ExprKind::Return; }

private:
    std::optional<std::unique_ptr<Expr>> expr_;
};

class BinaryExpr : public Expr {
public:
    BinaryExpr(Location loc, char op, std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) :
        Expr(ExprKind::Binary, std::move(loc)),
        op_(op),
        lhs_(std::move(lhs)),
        rhs_(std::move(rhs))
    {
    }

    char get_op() { return op_; }

    Expr *get_lhs() { return lhs_.get(); }

    Expr *get_rhs() { return rhs_.get(); }

    static bool classof(const Expr *expr) { return expr->get_kind() == ExprKind::Binary; }

private:
    char op_;
    std::unique_ptr<Expr> lhs_;
    std::unique_ptr<Expr> rhs_;
};

class CallExpr : public Expr {
public:
    CallExpr(Location loc, const std::string &callee, std::vector<std::unique_ptr<Expr>> args) :
        Expr(ExprKind::Call, std::move(loc)), callee_(callee), args_(std::move(args))
    {
    }

    llvm::StringRef get_callee() { return callee_; }

    llvm::ArrayRef<std::unique_ptr<Expr>> get_args() { return args_; }

    static bool classof(const Expr *expr) { return expr->get_kind() == ExprKind::Call; }

private:
    std::string callee_;
    std::vector<std::unique_ptr<Expr>> args_;
};

class PrintExpr : public Expr {
public:
    PrintExpr(Location loc, std::unique_ptr<Expr> arg) :
        Expr(ExprKind::Print, std::move(loc)), arg_(std::move(arg))
    {
    }

    Expr *get_arg() { return arg_.get(); }

    static bool classof(const Expr *expr) { return expr->get_kind() == ExprKind::Print; }

private:
    std::unique_ptr<Expr> arg_;
};

class Prototype {
public:
    Prototype(Location location, const std::string &name,
              std::vector<std::unique_ptr<VariableExpr>> args) :
        location_(std::move(location)),
        name_(name),
        args_(std::move(args))
    {
    }

    const Location &loc() { return location_; }

    llvm::StringRef get_name() const { return name_; }

    llvm::ArrayRef<std::unique_ptr<VariableExpr>> get_args() { return args_; }

private:
    Location location_;
    std::string name_;
    std::vector<std::unique_ptr<VariableExpr>> args_;
};

class Function {
public:
    Function(std::unique_ptr<Prototype> proto, std::unique_ptr<ExprList> body) :
        proto_(std::move(proto)), body_(std::move(body))
    {
    }

    Prototype *get_proto() { return proto_.get(); }

    ExprList *get_body() { return body_.get(); }

private:
    std::unique_ptr<Prototype> proto_;
    std::unique_ptr<ExprList> body_;
};

class Module {
public:
    Module(std::vector<Function> functions) : functions_(std::move(functions)) {}

    auto begin() { return functions_.begin(); }

    auto end() { return functions_.end(); }

private:
    std::vector<Function> functions_;
};

void dump(Module &module);

}
