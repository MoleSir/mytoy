#include "ast.hh"
#include "lexer.hh"
#include "parser.hh"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <string>
#include <system_error>

namespace cl = llvm::cl;

static cl::opt<std::string> input_filename(cl::Positional, cl::desc("<input toy file>"),
                                          cl::init("-"), cl::value_desc("filename"));

namespace {

enum Action { None, DumpAst };

}

static cl::opt<enum Action> emit_action("emit", cl::desc("Select the kind of output desired"),
                                        cl::values(clEnumValN(DumpAst, "ast",
                                                              "output the AST dump")));

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

int main(int argc, char **argv) {
    cl::ParseCommandLineOptions(argc, argv, "toy compiler\n");

    auto module = parse_input_file(input_filename);
    if (!module)
        return 1;

    switch (emit_action) {
    case Action::DumpAst:
        toy::dump(*module);
        return 0;
    default:
        llvm::errs() << "No action specified (parsing only?), use -emit=<action>\n";
    }

    return 0;
}
