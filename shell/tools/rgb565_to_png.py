import array
import sys

from PIL import Image

WIDTH, HEIGHT = 1280, 720


def expand(pixel):
    red = (pixel >> 11) & 0x1F
    green = (pixel >> 5) & 0x3F
    blue = pixel & 0x1F
    return (red * 255 // 31, green * 255 // 63, blue * 255 // 31)


def main(src, dst):
    pixels = array.array("H")
    with open(src, "rb") as file:
        pixels.frombytes(file.read())
    rgb = bytearray()
    for pixel in pixels:
        rgb.extend(expand(pixel))
    Image.frombytes("RGB", (WIDTH, HEIGHT), bytes(rgb)).save(dst)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
