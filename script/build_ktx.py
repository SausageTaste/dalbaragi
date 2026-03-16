import argparse
import os

from PIL import Image, UnidentifiedImageError


def __gen_cmd_str(ch_count: int, srgb: bool, src_path: str):
    commands = [
        "ktx",
        "create",
        #"--encode uastc",
        "--generate-mipmap",
    ]

    if ch_count == 4:
        format_prefix = "R8G8B8A8"
    elif ch_count == 3:
        format_prefix = "R8G8B8"
    elif ch_count == 2:
        format_prefix = "R8G8"
    elif ch_count == 1:
        format_prefix = "R8"
    else:
        raise ValueError(f"Invalid channel count: {commands}")

    if srgb:
        commands.append(f"--format {format_prefix}_SRGB")
        commands.append("--assign-tf srgb")
        commands.append("--convert-tf srgb")
    else:
        commands.append(f"--format {format_prefix}_UNORM")
        commands.append("--assign-tf linear")
        commands.append("--convert-tf linear")

    commands.append(f'"{src_path}"')
    dst_path = os.path.splitext(src_path)[0] + ".ktx"
    commands.append(f'"{dst_path}"')

    return " ".join(commands)


def __do_file(path: str):
    try:
        img = Image.open(path)
    except UnidentifiedImageError as e:
        print(f"[ERR] Not an image: '{path}'")
        return

    if img.mode == "RGBA":
        ch_count = 4
    elif img.mode == "RGB":
        ch_count = 3
    else:
        print(f"[ERR] Unsupported mode ({img.mode}):{path}")
        return

    cmd = __gen_cmd_str(ch_count, True, path)
    res = os.system(cmd)
    if 0 != res:
        print(f"[ERR] Failed to convert ({res}): '{path}'")
    else:
        print(f"[OK] Converted: '{path}'")


def __iter_file_paths(fol_path: str, recursive: bool):
    if recursive:
        for loc, folders, files in os.walk(fol_path):
            for file_name_ext in files:
                yield os.path.join(loc, file_name_ext)
    else:
        for item in os.listdir(fol_path):
            item_path = os.path.join(fol_path, item)
            if os.path.isfile(item_path):
                yield item_path


def __do_dir(path: str, recursive: bool):
    if not os.path.isdir(path):
        raise NotADirectoryError(path)

    for x in __iter_file_paths(path, recursive):
        __do_file(x)


def main():
    parser = argparse.ArgumentParser(
        prog="build_ktx",
        description="Convert images to KTX (Khronos Texture) files",
        epilog='Text at the bottom of help')
    parser.add_argument("paths", metavar="PATHS", type=str,
                        nargs="+", help="Paths for files or folders")
    parser.add_argument("-d", "--directory", action="store_true")
    parser.add_argument("-r", "--recursive", action="store_true")
    args = parser.parse_args()

    for p in args.paths:
        if args.directory:
            __do_dir(p, args.recursive)
        else:
            if not os.path.isfile(p):
                raise FileNotFoundError(p)


if __name__ == '__main__':
    main()
