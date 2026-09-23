#include "ast.hh"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

namespace toy {

namespace {

struct Indent {
    Indent(int &level) : level_(level) { ++level_; }

    ~Indent() { --level_; }

    int &level_;
};

class AstDumper {
public:
    void dump(Module *node);

private:
    void dump(const VarType &type);
    void dump(VarDeclExpr *node);
    void dump(Expr *node);
    void dump(ExprList *node);
    void dump(NumberExpr *node);
    void dump(LiteralExpr *node);
    void dump(VariableExpr *node);
    void dump(ReturnExpr *node);
    void dump(BinaryExpr *node);
    void dump(CallExpr *node);
    void dump(PrintExpr *node);
    void dump(Prototype *node);
    void dump(Function *node);

    void indent() {
        for (int i = 0; i < indent_; ++i)
            llvm::errs() << "  ";
    }

    int indent_ = 0;
};

template <typename T>
static std::string loc(T *node) {
    const auto &location = node->loc();
    return (llvm::Twine("@") + *location.file + ":" + llvm::Twine(location.line) + ":" +
            llvm::Twine(location.col))
        .str();
}

static void print_literal(Expr *expr) {
    if (auto *number = llvm::dyn_cast<NumberExpr>(expr)) {
        llvm::errs() << number->get_value();
        return;
    }

    auto *literal = llvm::cast<LiteralExpr>(expr);

    llvm::errs() << "<";
    llvm::interleaveComma(literal->get_dims(), llvm::errs());
    llvm::errs() << ">";

    llvm::errs() << "[ ";
    llvm::interleaveComma(literal->get_values(), llvm::errs(),
                          [&](auto &elt) { print_literal(elt.get()); });
    llvm::errs() << "]";
}

void AstDumper::dump(Expr *node) {
    llvm::TypeSwitch<Expr *>(node)
        .Case<BinaryExpr, CallExpr, LiteralExpr, NumberExpr, PrintExpr, ReturnExpr, VarDeclExpr,
              VariableExpr>([&](auto *expr) { dump(expr); })
        .Default([&](Expr *expr) {
            Indent level(indent_);
            indent();
            llvm::errs() << "<unknown Expr, kind " << static_cast<int>(expr->get_kind()) << ">\n";
        });
}

void AstDumper::dump(Module *node) {
    Indent level(indent_);
    indent();
    llvm::errs() << "Module:\n";
    for (auto &function : *node)
        dump(&function);
}

void AstDumper::dump(Function *node) {
    Indent level(indent_);
    indent();
    llvm::errs() << "Function \n";
    dump(node->get_proto());
    dump(node->get_body());
}

void AstDumper::dump(Prototype *node) {
    Indent level(indent_);
    indent();
    llvm::errs() << "Proto '" << node->get_name() << "' " << loc(node) << "\n";
    indent();
    llvm::errs() << "Params: [";
    llvm::interleaveComma(node->get_args(), llvm::errs(),
                          [](auto &arg) { llvm::errs() << arg->get_name(); });
    llvm::errs() << "]\n";
}

void AstDumper::dump(ExprList *node) {
    Indent level(indent_);
    indent();
    llvm::errs() << "Block {\n";
    for (auto &expr : *node)
        dump(expr.get());
    indent();
    llvm::errs() << "} // Block\n";
}

void AstDumper::dump(VarDeclExpr *node) {
    Indent level(indent_);
    indent();
    llvm::errs() << "VarDecl " << node->get_name();
    dump(node->get_type());
    llvm::errs() << " " << loc(node) << "\n";
    dump(node->get_init_val());
}

void AstDumper::dump(const VarType &type) {
    llvm::errs() << "<";
    llvm::interleaveComma(type.shape, llvm::errs());
    llvm::errs() << ">";
}

void AstDumper::dump(NumberExpr *node) {
    Indent level(indent_);
    indent();
    llvm::errs() << node->get_value() << " " << loc(node) << "\n";
}

void AstDumper::dump(LiteralExpr *node) {
    Indent level(indent_);
    indent();
    llvm::errs() << "Literal: ";
    print_literal(node);
    llvm::errs() << " " << loc(node) << "\n";
}

void AstDumper::dump(VariableExpr *node) {
    Indent level(indent_);
    indent();
    llvm::errs() << "var: " << node->get_name() << " " << loc(node) << "\n";
}

void AstDumper::dump(ReturnExpr *node) {
    Indent level(indent_);
    indent();
    llvm::errs() << "Return\n";
    if (node->get_expr().has_value())
        return dump(*node->get_expr());
    {
        Indent void_level(indent_);
        indent();
        llvm::errs() << "(void)\n";
    }
}

void AstDumper::dump(BinaryExpr *node) {
    Indent level(indent_);
    indent();
    llvm::errs() << "BinOp: " << node->get_op() << " " << loc(node) << "\n";
    dump(node->get_lhs());
    dump(node->get_rhs());
}

void AstDumper::dump(CallExpr *node) {
    Indent level(indent_);
    indent();
    llvm::errs() << "Call '" << node->get_callee() << "' [ " << loc(node) << "\n";
    for (auto &arg : node->get_args())
        dump(arg.get());
    indent();
    llvm::errs() << "]\n";
}

void AstDumper::dump(PrintExpr *node) {
    Indent level(indent_);
    indent();
    llvm::errs() << "Print [ " << loc(node) << "\n";
    dump(node->get_arg());
    indent();
    llvm::errs() << "]\n";
}

}

void dump(Module &module) { AstDumper().dump(&module); }

}
