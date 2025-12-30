def _cuda_repository_impl(repository_ctx):
    # Detect OS
    os_name = repository_ctx.os.name.lower()
    is_windows = os_name.startswith("windows")
    
    # Determine the preferred path from attributes based on OS
    path_from_attr = ""
    if is_windows:
        path_from_attr = repository_ctx.attr.win_path
    else:
        path_from_attr = repository_ctx.attr.linux_path

    # Determine default path
    if is_windows:
        default_path = "C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v13.1"
    else:
        default_path = "/usr/local/cuda"

    # Priority: 
    # 1. Explicit path attribute (win_path or linux_path)
    # 2. CUDA_PATH environment variable
    # 3. Default system path
    
    if path_from_attr:
        cuda_path = path_from_attr
    elif "CUDA_PATH" in repository_ctx.os.environ:
        cuda_path = repository_ctx.os.environ["CUDA_PATH"]
    else:
        cuda_path = default_path

    repository_ctx.report_progress("Configuring CUDA at " + cuda_path)

    # Symlink the required subdirectories
    for d in ["include", "lib", "lib64", "bin", "nvvm", "extras"]:
        target_dir = repository_ctx.path(cuda_path + "/" + d)
        if target_dir.exists:
            repository_ctx.symlink(target_dir, d)
    
    # Symlink the provided BUILD file
    repository_ctx.symlink(repository_ctx.attr.build_file, "BUILD.bazel")

cuda_repository = repository_rule(
    implementation = _cuda_repository_impl,
    attrs = {
        "build_file": attr.label(allow_single_file = True),
        "win_path": attr.string(doc = "Explicit path to CUDA toolkit on Windows"),
        "linux_path": attr.string(doc = "Explicit path to CUDA toolkit on Linux"),
    },
    local = True,
    environ = ["CUDA_PATH"],
)

def _cuda_configure_impl(module_ctx):
    win_path = ""
    linux_path = ""
    
    # Check for configuration tags
    for mod in module_ctx.modules:
        for config in mod.tags.config:
            if config.win_path:
                win_path = config.win_path
            if config.linux_path:
                linux_path = config.linux_path

    cuda_repository(
        name = "cuda",
        build_file = "//thirdparty:cuda/BUILD.oss",
        win_path = win_path,
        linux_path = linux_path,
    )

cuda_configure = module_extension(
    implementation = _cuda_configure_impl,
    tag_classes = {
        "config": tag_class(attrs = {
            "win_path": attr.string(),
            "linux_path": attr.string(),
        }),
    },
)