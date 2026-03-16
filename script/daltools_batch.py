import os
import json
import shutil
import sys
from typing import Iterable, Optional

import yaml
from PIL import Image


def mkdir_tree(path: str):
    os.makedirs(path, exist_ok=True)


def make_neighbour_path(host_file_path: str, neighbour_file_path: str):
    host_dir = os.path.split(host_file_path)[0]
    return os.path.relpath(os.path.join(host_dir, neighbour_file_path))


def find_texture_file(texture_path_suffix: str, folders: Iterable[str]):
    for folder in folders:
        tex_path = os.path.join(folder, texture_path_suffix)
        if os.path.exists(tex_path):
            return os.path.relpath(tex_path)

    raise FileNotFoundError(f"Texture file not found: {texture_path_suffix}")


class YamlLoader:
    def __init__(self, yaml_path: str):
        with open(yaml_path, 'r', encoding="utf8") as f:
            self.__yml_content = yaml.safe_load(f)
        print(json.dumps(self.__yml_content, indent=4))

        self.__yaml_path = os.path.relpath(yaml_path)

        self.__tex_lookup_paths = [
            self.loc,
        ]

        try:
            for x in self.__yml_content["texture_lookup_paths"]:
                self.__tex_lookup_paths.append(os.path.join(self.loc, x))
        except KeyError:
            pass

    @property
    def loc(self):
        return os.path.split(self.__yaml_path)[0]

    @property
    def src_path(self):
        return os.path.join(self.loc, self.__yml_content["dmd"]["src"])

    @property
    def src_loc(self):
        return os.path.split(self.src_path)[0]

    @property
    def dmd_compression(self):
        try:
            return str(self.__yml_content["dmd"]["compression_method"])
        except KeyError:
            return "none"

    @property
    def bundle_enabled(self):
        try:
            return bool(self.__yml_content["bundle"]["enable"])
        except KeyError:
            return False

    @property
    def bundle_name(self):
        try:
            return self.__yml_content["bundle"]["name"]
        except KeyError:
            return ""

    @property
    def bundle_comp_level(self):
        try:
            return int(self.__yml_content["bundle"]["compression_level"])
        except KeyError:
            return 6

    @property
    def ktx_zstd_level(self):
        try:
            return int(self.__yml_content["ktx_config"]["zstd"])
        except KeyError:
            return 0

    @property
    def tex_lookup_paths(self):
        for x in self.__tex_lookup_paths:
            yield x

    def gen_ktx_conversions(self):
        for x in self.__yml_content["ktx_conversion"]:
            if "sources" not in x.keys():
                continue
            if x["sources"] is None:
                continue

            for y in x["sources"]:
                ktx_params = KtxParameters()
                ktx_params.src_path = find_texture_file(y, self.tex_lookup_paths)
                ktx_params.channels = x["channels"]
                ktx_params.srgb = x["srgb"]
                yield ktx_params


class KtxParameters:
    def __init__(self):
        self.__src_path: str = ""
        self.__dst_path: Optional[str] = None
        self.__channels: Optional[str] = None
        self.__srgb: Optional[bool] = None

    @property
    def src_path(self):
        return self.__src_path

    @src_path.setter
    def src_path(self, value: str):
        self.__src_path = str(value)

    @property
    def dst_path(self):
        if self.__dst_path is None:
            raise ValueError("Destination path not set")
        return self.__dst_path

    @property
    def channels(self):
        return self.__channels

    @channels.setter
    def channels(self, value):
        value = str(value).lower()
        if value not in ("r", "rg", "rgb", "rgba"):
            raise ValueError(f"Invalid channels: {value})")
        self.__channels = value

    @property
    def srgb(self):
        return self.__srgb

    @srgb.setter
    def srgb(self, value):
        self.__srgb = bool(value)

    def finalize(self, ktx_dir: str, yaml_loc: str):
        if self.__channels is None:
            raise ValueError("Channel value not set")
        if self.__srgb is None:
            raise ValueError("sRGB value not set")

        if not os.path.isfile(self.__src_path):
            raise FileNotFoundError(f"Source file not found: {self.__src_path}")

        src_rel_path = os.path.relpath(self.src_path, yaml_loc)
        self.__dst_path = os.path.join(ktx_dir, os.path.splitext(src_rel_path)[0] + ".ktx")
        return

    def make_ktx_cmd(self, zstd_level: int):
        commands = [
            "ktx",
            "create",
            "--encode uastc",
            "--generate-mipmap",
        ]

        if zstd_level > 0:
            commands.append(f"--zstd {zstd_level}")

        if self.channels == "rgba":
            format_prefix = "R8G8B8A8"
        elif self.channels == "rgb":
            format_prefix = "R8G8B8"
        elif self.channels == "rg":
            format_prefix = "R8G8"
        elif self.channels == "r":
            format_prefix = "R8"
        else:
            raise ValueError(f"Invalid channel count: {commands}")

        if self.srgb:
            commands.append(f"--format {format_prefix}_SRGB")
            commands.append("--assign-tf srgb")
            commands.append("--convert-tf srgb")
        else:
            commands.append(f"--format {format_prefix}_UNORM")
            commands.append("--assign-tf linear")
            commands.append("--convert-tf linear")

        commands.append(f'"{self.src_path}"')
        commands.append(f'"{self.dst_path}"')

        return " ".join(commands)


def __do_ktx_conversion(ktx_params: KtxParameters, ktx_dir: str, yaml_data: YamlLoader):
    if os.path.isfile(ktx_params.dst_path):
        print(f"KTX file already exists: {ktx_params.dst_path}")
        return

    if not ktx_params.src_path.endswith(".png"):
        img = Image.open(ktx_params.src_path)
        png_dir = os.path.relpath(os.path.split(ktx_dir)[0] + "/png")
        os.makedirs(png_dir, exist_ok=True)
        png_file_path = os.path.join(png_dir, os.path.relpath(ktx_params.src_path, yaml_data.loc)) + ".png"
        os.makedirs(os.path.split(png_file_path)[0], exist_ok=True)
        img.save(png_file_path, "PNG")
        ktx_params.src_path = png_file_path

    mkdir_tree(ktx_dir)
    cmd = ktx_params.make_ktx_cmd(yaml_data.ktx_zstd_level)
    print("Executing:", cmd)
    if 0 != os.system(cmd):
        raise RuntimeError("Failed to convert to KTX")
    else:
        print("Success")


def do_once(yaml_path: str):
    yaml_data = YamlLoader(yaml_path)
    out_dir = os.path.join(yaml_data.loc, "out")

    ktx_map = {}
    ktx_path = os.path.join(out_dir, "ktx")
    for ktx_params in yaml_data.gen_ktx_conversions():
        ktx_params.finalize(ktx_path, yaml_data.loc)

        if ktx_params.src_path in ktx_map.keys():
            raise ValueError(f"Duplicate source path: {ktx_params.src_path}")
        ktx_map[ktx_params.src_path] = ktx_params.dst_path
        __do_ktx_conversion(ktx_params, ktx_path, yaml_data)

    with open(yaml_data.src_path, 'r') as f:
        json_data = json.load(f)

    textures = set()
    for s in json_data["scenes"]:
        for m in s["materials"]:
            for k, v in m.items():
                if k.endswith("map") and v:
                    textures.add(v)

    intermediate_dir = os.path.join(out_dir, "intermediate")
    shutil.rmtree(intermediate_dir, ignore_errors=True)
    mkdir_tree(intermediate_dir)

    dal_tex_map = {}
    for t in textures:
        tex_path = find_texture_file(t, yaml_data.tex_lookup_paths)
        if tex_path in ktx_map.keys():
            tex_path = ktx_map[tex_path]
        tex_path_in_dal = os.path.split(tex_path)[1]

        if t != tex_path_in_dal:
            dal_tex_map[t] = tex_path_in_dal
        else:
            dal_tex_map[t] = t

        tex_path_in_intermediate = os.path.join(intermediate_dir, tex_path_in_dal)
        os.makedirs(os.path.split(tex_path_in_intermediate)[0], exist_ok=True)
        shutil.copy(tex_path, tex_path_in_intermediate)
        print(f"Copy {os.path.abspath(tex_path)} to {os.path.abspath(tex_path_in_intermediate)}")

    for s in json_data["scenes"]:
        for m in s["materials"]:
            for key in m.keys():
                value = m[key]
                if value in dal_tex_map.keys():
                    m[key] = dal_tex_map[value]

    new_json_path = os.path.join(intermediate_dir, os.path.split(yaml_data.src_path)[1])
    with open(new_json_path, 'w') as f:
        json.dump(json_data, f, indent=4)

    bin_path = os.path.splitext(yaml_data.src_path)[0] + ".bin"
    if os.path.exists(bin_path):
        shutil.copy(bin_path, os.path.join(intermediate_dir, os.path.split(bin_path)[1]))

    daltools_compile_commands = [
        "daltools",
        "compile",
        "-c",
        f"{yaml_data.dmd_compression}",
        f'"{new_json_path}"',
    ]
    cmd = " ".join(daltools_compile_commands)
    if 0 != os.system(cmd):
        raise RuntimeError("Failed to compile")
    print("Success:", cmd)

    final_dir = os.path.join(out_dir, "final")
    shutil.rmtree(final_dir, ignore_errors=True)
    os.makedirs(final_dir, exist_ok=True)

    if yaml_data.bundle_enabled:
        bundle_name = yaml_data.bundle_name
        bundle_out_path = os.path.abspath(os.path.join(intermediate_dir, bundle_name))

        daltools_bundle_commands = [
            "daltools",
            "bundle",
            "-q",
            f"{yaml_data.bundle_comp_level}",
            "-o",
            f'"{bundle_out_path}"',
            f'"{os.path.abspath(os.path.join(intermediate_dir, "*.dmd"))}"',
            f'"{os.path.abspath(os.path.join(intermediate_dir, "*.png"))}"',
            f'"{os.path.abspath(os.path.join(intermediate_dir, "*.tga"))}"',
            f'"{os.path.abspath(os.path.join(intermediate_dir, "*.jpg"))}"',
            f'"{os.path.abspath(os.path.join(intermediate_dir, "*.ktx"))}"',
        ]
        cmd = " ".join(daltools_bundle_commands)
        print("Executing", cmd)
        if 0 != os.system(cmd):
            raise RuntimeError("Failed to bundle")
        print("Success")

        shutil.move(bundle_out_path, os.path.join(final_dir, bundle_name))
    else:
        dmd_path = os.path.splitext(new_json_path)[0] + ".dmd"
        shutil.move(dmd_path, os.path.join(final_dir, os.path.split(dmd_path)[1]))

        for x in os.listdir(intermediate_dir):
            if x in dal_tex_map.values():
                shutil.move(os.path.join(intermediate_dir, x), os.path.join(final_dir, x))


def main():
    for x in sys.argv[1:]:
        do_once(x)


if __name__ == '__main__':
    main()
