# -*- Python -*-
"""lit configuration for the MLIR Toy tutorial built out-of-tree.

The test inputs under test/Ch*/ are copied verbatim from
mlir/test/Examples/Toy/, so the RUN lines refer to `toyc-chN`, `FileCheck`,
`not` and `mlir-opt`. The first comes from this build, the rest are provided
by the host LLVM/MLIR build pointed at by `llvm_tools_dir`.
"""

import os
import subprocess

import lit.formats
import lit.util

from lit.llvm import llvm_config

config.name = "TOY"
config.test_format = lit.formats.ShTest()
config.suffixes = [".toy", ".mlir"]
config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = os.path.join(config.toy_obj_root, "test")

config.excludes = ["Inputs", "CMakeLists.txt", "README.txt", "LICENSE.txt"]

llvm_config.with_system_environment(["HOME", "INCLUDE", "LIB", "TMP", "TEMP"])
llvm_config.use_default_substitutions()

config.toy_tools_dir = os.path.join(config.toy_obj_root, "bin")
config.substitutions.append(("%PATH%", config.environment["PATH"]))

# Make FileCheck / not / mlir-opt reachable from the host build.
llvm_config.with_environment("PATH", config.llvm_tools_dir, append_path=True)

tool_dirs = [config.toy_tools_dir, config.llvm_tools_dir]
tools = [
    "FileCheck",
    "count",
    "not",
    "mlir-opt",
    "toyc-ch1",
    "toyc-ch2",
    "toyc-ch3",
    "toyc-ch4",
    "toyc-ch5",
    "toyc-ch6",
    "toyc-ch7",
]
llvm_config.add_tool_substitutions(tools, tool_dirs)


def _host_supports(feature):
    """Mirror of the check in mlir/test/lit.cfg.py, using the host mlir-runner."""
    mlir_runner = lit.util.which("mlir-runner", config.llvm_tools_dir)
    if not mlir_runner:
        return False
    try:
        proc = subprocess.Popen(
            [mlir_runner, "--host-supports-" + feature], stdout=subprocess.PIPE
        )
    except OSError:
        return False
    out = proc.stdout.read().decode("ascii")
    proc.wait()
    return "true" in out


# Ch6/Ch7 tests need it (see test/Ch{6,7}/lit.local.cfg).
if _host_supports("jit"):
    config.available_features.add("host-supports-jit")
