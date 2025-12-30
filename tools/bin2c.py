import sys
import os


def main():
    if len(sys.argv) < 3:
        print("Usage: bin2c.py <input> <output> [var_name]")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    # Derive variable name from output filename (e.g., "optix_kernels_blob.h" -> "optix_kernels_ir")
    # or use explicit var_name if provided
    if len(sys.argv) >= 4:
        var_name = sys.argv[3]
    else:
        basename = os.path.basename(output_path)
        # Remove "_blob.h" suffix and add "_ir"
        if basename.endswith("_blob.h"):
            var_name = basename[:-7] + "_ir"
        else:
            var_name = "optix_kernels_ir"

    with open(input_path, "rb") as f:
        data = f.read()

    with open(output_path, "w") as f:
        f.write("#pragma once\n")
        f.write("#include <cstddef>\n")
        f.write(f"extern const unsigned char {var_name}[];\n")
        f.write(f"extern const size_t {var_name}_len;\n")
        f.write(f"const unsigned char {var_name}[] = {{ ")
        f.write(",".join(str(b) for b in data))
        f.write(" };\n")
        f.write(f"const size_t {var_name}_len = {len(data)};\n")


if __name__ == "__main__":
    main()
