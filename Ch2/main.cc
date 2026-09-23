#include "ast.hh"
#include "dialect.hh"
#include "lexer.hh"
#include "mlir_gen.hh"
#include "parser.hh"

#include "mlir/IR/AsmState.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Parser/Parser.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <string>
#include <system_error>

namespace cl = llvm::cl;

static cl::opt<std::string> input_filename(cl::Positional, cl::desc("<input toy file>"),
                                          cl::init("-"), cl::value_desc("filename"));

namespace {

enum InputType { Toy, Mlir };

enum Action { None, DumpAst, DumpMlir };

}

static cl::opt<enum InputType> input_type(
    "x", cl::init(Toy), cl::desc("Decided the kind of output desired"),
    cl::values(clEnumValN(Toy, "toy", "load the input file as a Toy source.")),
    cl::values(clEnumValN(Mlir, "mlir", "load the input file as an MLIR file")));

static cl::opt<enum Action> emit_action(
    "emit", cl::desc("Select the kind of output desired"),
    cl::values(clEnumValN(DumpAst, "ast", "output the AST dump")),
    cl::values(clEnumValN(DumpMlir, "mlir", "output the MLIR dump")));

static std::unique_ptr<toy::Module> parse_input_file(llvm::StringRef filename) {
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> file_or_err =
        llvm::MemoryBuffer::getFileOrSTDIN(filename);
    if (std::error_code ec = file_or_err.getError()) {
        llvm::errs() << "Could not open input file: " << ec.message() << "\n";
        return nullptr;
    }

    auto buffer = file_or_err.get()->getBuffer();
    toy::LexerBuffer lexer(buffer.begin(), buffer.end(), std::string(filename));
    toy::Parser parser(lexer);
    return parser.parse_module();
}

static int dump_mlir() {
    mlir::MLIRContext context;
    context.getOrLoadDialect<mlir::toy::ToyDialect>();

    if (input_type != InputType::Mlir &&
        !llvm::StringRef(input_filename).ends_with(".mlir")) {
        auto module = parse_input_file(input_filename);
        if (!module)
            return 6;

        mlir::OwningOpRef<mlir::ModuleOp> mlir_module = toy::mlir_gen(context, *module);
        if (!mlir_module)
            return 1;

        mlir_module->dump();
        return 0;
    }

    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> file_or_err =
        llvm::MemoryBuffer::getFileOrSTDIN(input_filename);
    if (std::error_code ec = file_or_err.getError()) {
        llvm::errs() << "Could not open input file: " << ec.message() << "\n";
        return -1;
    }

    llvm::SourceMgr source_mgr;
    source_mgr.AddNewSourceBuffer(std::move(*file_or_err), llvm::SMLoc());
    mlir::OwningOpRef<mlir::ModuleOp> module =
        mlir::parseSourceFile<mlir::ModuleOp>(source_mgr, &context);
    if (!module) {
        llvm::errs() << "Error can't load file " << input_filename << "\n";
        return 3;
    }

    module->dump();
    return 0;
}

static int dump_ast() {
    if (input_type == InputType::Mlir) {
        llvm::errs() << "Can't dump a Toy AST when the input is MLIR\n";
        return 5;
    }

    auto module = parse_input_file(input_filename);
    if (!module)
        return 1;

    toy::dump(*module);
    return 0;
}

int main(int argc, char **argv) {
    mlir::registerAsmPrinterCLOptions();
    mlir::registerMLIRContextCLOptions();
    cl::ParseCommandLineOptions(argc, argv, "toy compiler\n");

    switch (emit_action) {
    case Action::DumpAst:
        return dump_ast();
    case Action::DumpMlir:
        return dump_mlir();
    default:
        llvm::errs() << "No action specified (parsing only?), use -emit=<action>\n";
    }

    return 0;
}
