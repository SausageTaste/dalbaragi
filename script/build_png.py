import os
import multiprocessing.pool as mpp

from PIL import Image, ImageFile


ImageFile.LOAD_TRUNCATED_IMAGES = False
INPUT_DIR = r"D:\CG Assets\Bistro\Bistro_v5_2\Textures"


def __gen_input_files():
    for loc, folders, files in os.walk(INPUT_DIR):
        for file in files:
            if file.endswith(".png"):
                continue

            yield os.path.join(loc, file)


def __do_once(src_path: str):
    parent_dir, file_name_ext = os.path.split(src_path)
    output_dir = parent_dir
    output_path = os.path.join(output_dir, os.path.splitext(file_name_ext)[0] + ".png")

    #if os.path.isfile(output_path):
    #    return "Already exists: " + repr(output_path)

    img = Image.open(src_path)
    os.makedirs(output_dir, exist_ok=True)
    img.save(output_path, "PNG")

    return "Success: " + repr(output_path)


def main():
    items = set(__gen_input_files())

    with mpp.Pool() as pool:
        for x in pool.imap_unordered(__do_once, items):
            print(x)


if __name__ == "__main__":
    main()
