"""
Bazel rules for compiling CUDA kernels to PTX and OptiX IR.
"""

load("@rules_cc//cc:defs.bzl", "CcInfo")
load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain")

def _get_copts(ctx):
    """Returns compilation flags based on the build mode (dbg/opt/fastbuild)."""
    mode = ctx.var.get("COMPILATION_MODE", "fastbuild")
    flags = []

    # Common flags

    if mode == "dbg":
        # Debug symbols and device debug info
        # -G overrides -lineinfo, so we don't need both
        flags.extend(["-g", "-G", "-O0"])
    elif mode == "opt":
        # Optimization flags matching the project defaults
        flags.extend(["-O3", "--use_fast_math", "-lineinfo"])
    else:
        # Fastbuild - balanced
        flags.extend(["-O1", "-lineinfo"])

    return flags

def _cuda_compile_impl(ctx, extension, flag):
    nvcc = ctx.executable._nvcc

    # Get C++ toolchain to find the host compiler (cl.exe on Windows, gcc/clang on Linux)
    cc_toolchain = find_cpp_toolchain(ctx)
    cpp_compiler = cc_toolchain.compiler_executable
    
    # Extract directory of the compiler
    # cpp_compiler usually has forward slashes from Bazel
    compiler_dir = cpp_compiler[:cpp_compiler.rfind("/")]

    src = ctx.file.src

    # Determine output filename
    if ctx.attr.out:
        out_name = ctx.attr.out
    else:
        # src.basename is e.g. "kernel.cu"
        if src.basename.endswith(".cu"):
            out_name = src.basename[:-3] + extension
        else:
            out_name = src.basename + extension

    out = ctx.actions.declare_file(out_name)

    args = ctx.actions.args()
    # args.add("-ccbin", cpp_compiler) # Removed to avoid path issues
    args.add(flag)  # --ptx or --optix-ir
    args.add("-o", out.path)
    args.add(src.path)

    # Add user provided copts
    args.add_all(ctx.attr.copts)

    # Add mode-specific flags
    args.add_all(_get_copts(ctx))

    # Add current package directory to includes (often needed for relative includes)
    # src.dirname gives the path relative to execroot.
    args.add("-I" + src.dirname)

    # Include paths from deps
    transitive_headers = []

    for dep in ctx.attr.deps:
        if CcInfo in dep:
            cc_info = dep[CcInfo]

            # Collect headers to ensure they are available in the sandbox
            transitive_headers.append(cc_info.compilation_context.headers)

            # Add includes
            for inc in cc_info.compilation_context.includes.to_list():
                args.add("-I" + inc)
            for inc in cc_info.compilation_context.quote_includes.to_list():
                args.add("-I" + inc)
            for inc in cc_info.compilation_context.system_includes.to_list():
                args.add("-isystem", inc)

    # Add explicit includes if any
    for inc in ctx.attr.includes:
        args.add("-I" + inc)

    # Add built-in include directories from toolchain (critical for MSVC)
    for inc in cc_toolchain.built_in_include_directories:
        args.add("-I" + inc)

    # Flatten headers
    all_headers = depset(transitive = transitive_headers)

    # Update PATH to include compiler directory
    env = dict(ctx.configuration.default_shell_env)
    
    # Set NVCC_CCBIN to avoid command line parsing issues with spaces
    env["NVCC_CCBIN"] = cpp_compiler

    # Add tools to inputs
    tools_files = ctx.files._nvcc_tools
    
    # Add CUDA bin paths to PATH
    nvcc_file = ctx.executable._nvcc
    nvcc_dir = nvcc_file.dirname
    # nvcc_dir ends with /bin. We need root to find nvvm/bin
    # But since we are in sandbox, structure is preserved.
    # nvcc_dir is like external/repo/bin
    cuda_root = nvcc_dir.rpartition("/")[0]
    nvvm_bin = cuda_root + "/nvvm/bin"
    
    current_path = env.get("PATH", "")
    
    # Platform-specific PATH setup
    separator = ctx.configuration.host_path_separator
    
    paths_to_add = [nvcc_dir, nvvm_bin]
    
    # On Windows, we need to explicitly add the compiler directory to PATH
    # so that nvcc can find cl.exe
    if separator == ";":
        paths_to_add.insert(0, compiler_dir)
        
    env["PATH"] = separator.join(paths_to_add + [current_path])

    ctx.actions.run(
        outputs = [out],
        inputs = depset([src] + tools_files, transitive = [all_headers]),
        executable = nvcc,
        arguments = [args],
        mnemonic = "Nvcc" + extension.replace(".", "").upper(),
        progress_message = "Compiling {} to {}: {}".format(src.short_path, extension, out.short_path),
        use_default_shell_env = True, # Still needed for other system env vars
        env = env,
    )

    return [DefaultInfo(files = depset([out]))]

def _cuda_to_ptx_impl(ctx):
    return _cuda_compile_impl(ctx, ".ptx", "--ptx")

def _cuda_to_optixir_impl(ctx):
    return _cuda_compile_impl(ctx, ".optixir", "--optix-ir")

# Attributes common to both rules
_common_attrs = {
    "src": attr.label(allow_single_file = [".cu"], mandatory = True),
    "deps": attr.label_list(providers = [CcInfo]),
    "includes": attr.string_list(doc = "List of include paths"),
    "copts": attr.string_list(doc = "Additional compiler options"),
    "out": attr.string(doc = "Output filename. If not specified, derived from src name."),
    "_nvcc": attr.label(
        default = "@cuda//:nvcc",
        executable = True,
        cfg = "exec",
    ),
    "_nvcc_tools": attr.label(
        default = "@cuda//:nvcc_tools",
        cfg = "exec",
    ),
    "_cc_toolchain": attr.label(
        default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
    ),
}

cuda_to_ptx = rule(
    implementation = _cuda_to_ptx_impl,
    attrs = _common_attrs,
    fragments = ["cpp"],
    toolchains = ["@bazel_tools//tools/cpp:toolchain_type"],
)

cuda_to_optixir = rule(
    implementation = _cuda_to_optixir_impl,
    attrs = _common_attrs,
    fragments = ["cpp"],
    toolchains = ["@bazel_tools//tools/cpp:toolchain_type"],
)
