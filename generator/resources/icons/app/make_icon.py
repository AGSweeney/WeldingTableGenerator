# Copyright (c) 2026 Adam G. Sweeney <AGSweeney@gmail.com>
# SPDX-License-Identifier: MIT
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""Rasterize the welding-table mark into a multi-size Windows icon."""

from PIL import Image, ImageDraw


def draw(size: int) -> Image.Image:
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    s = size / 64.0

    def xy(*pts):
        return [(p[0] * s, p[1] * s) for p in pts]

    radius = max(2, int(round(size * 0.18)))
    d.rounded_rectangle((0, 0, size - 1, size - 1), radius=radius, fill=(0x2B, 0x2B, 0x2B, 255))

    green = (0x3D, 0x6B, 0x3D, 255)
    green_d = (0x2A, 0x4E, 0x2A, 255)

    def post(x0, y0, x1, y1):
        d.polygon(xy((x0, y0), (x1, y0), (x1, y1), (x0, y1)), fill=green)
        d.polygon(xy((x1, y0), (x1 + 1.6, y0 - 1.1), (x1 + 1.6, y1 - 1.1), (x1, y1)), fill=green_d)

    post(13, 36, 17.2, 54)
    post(46, 36, 50.2, 54)
    d.polygon(xy((15, 49), (32, 56), (32, 58.4), (15, 51.4)), fill=(0x5C, 0x6E, 0x7A, 255))
    d.polygon(xy((32, 56), (49, 49), (49, 51.4), (32, 58.4)), fill=(0x4E, 0x60, 0x6C, 255))
    post(30, 42, 34.4, 58)
    d.polygon(xy((12, 26), (32, 36), (32, 45), (12, 35)), fill=(0xB7, 0xC4, 0xCE, 255))
    d.polygon(xy((32, 36), (52, 26), (52, 35), (32, 45)), fill=(0x8F, 0xA0, 0xAB, 255))
    d.polygon(xy((12, 22), (32, 12), (52, 22), (32, 32)), fill=(0xF7, 0xF8, 0xF8, 255))
    d.polygon(xy((12, 22), (32, 32), (32, 36), (12, 26)), fill=(0xD5, 0xDB, 0xE0, 255))
    d.polygon(xy((32, 32), (52, 22), (52, 26), (32, 36)), fill=(0xC5, 0xCE, 0xD6, 255))
    if size >= 32:
        for cx, cy in ((24, 20), (32, 16.5), (40, 20), (27, 24), (37, 24), (32, 22)):
            r = max(1.0, 1.15 * s)
            d.ellipse((cx * s - r, cy * s - r, cx * s + r, cy * s + r), fill=(0x2A, 0x5A, 0x8A, 255))
    return im


def main() -> None:
    sizes = (16, 24, 32, 48, 64, 256)
    images = [draw(n) for n in sizes]
    images[-1].save(
        "welding-table.ico",
        format="ICO",
        append_images=images[:-1],
        sizes=[(n, n) for n in sizes],
    )
    images[-1].save("welding-table-256.png")


if __name__ == "__main__":
    main()
