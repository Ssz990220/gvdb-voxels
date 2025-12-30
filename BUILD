load("@rules_uv//uv:pip.bzl", "pip_compile")
load("@rules_uv//uv:venv.bzl", "create_venv", "sync_venv")

exports_files([
    "pyproject.toml",
    "uv.lock",
])

pip_compile(
    name = "requirements",
    args = [
        "--extra",
        "plot",
    ],
    requirements_in = "pyproject.toml",
    requirements_txt = "requirements_lock.txt",
)

create_venv(
    name = "venv",
    destination_folder = ".venv",
    requirements_txt = ":requirements_lock.txt",
)

sync_venv(
    name = "sync_venv",
    destination_folder = ".venv",
    requirements_txt = ":requirements_lock.txt",
)
