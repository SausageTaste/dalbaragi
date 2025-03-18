import os
import multiprocessing.pool as mpp

from PIL import Image, ImageFile
import numpy as np


ImageFile.LOAD_TRUNCATED_IMAGES = False
INPUT_DIR = r"D:\CG Assets\Bistro\textures - Copy"


def __gen_input_files():
    for loc, folders, files in os.walk(INPUT_DIR):
        for file in files:
            if not file.endswith("_Normal.png"):
                continue

            yield os.path.join(loc, file)


def __do_once(src_path: str):
    parent_dir = os.path.dirname(src_path)
    out_dir = os.path.join(parent_dir, "normal_maps")
    out_path = os.path.join(out_dir, os.path.basename(src_path))

    img = Image.open(src_path).convert("RGB")
    image_np = np.array(img).astype(np.float32) / 255.0
    r = image_np[:, :, 0] * 2.0 - 1.0  # X component
    g = image_np[:, :, 1] * 2.0 - 1.0  # Y component
    b = np.sqrt(np.maximum(0.0, 1.0 - r**2 - g**2))  # Z component

    # Convert back to [0,1] range
    r = (r + 1.0) / 2.0
    g = (g + 1.0) / 2.0
    b = (b + 1.0) / 2.0

    # Invert G channel
    g = 1.0 - g

    # Merge RGB channels
    final_image = (np.stack([r, g, b], axis=-1) * 255).astype(np.uint8)

    # Save the corrected normal map
    img = Image.fromarray(final_image)
    os.makedirs(out_dir, exist_ok=True)
    img.save(out_path)

    return "Success: " + out_path


def main():
    items = set(__gen_input_files())

    with mpp.Pool() as pool:
        for x in pool.imap_unordered(__do_once, items):
            print(x)


if __name__ == "__main__":
    main()
