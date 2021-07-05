# Converts the output of the raytracing program to a PNG image

import png
import numpy as np
from scipy.misc import imsave
from sys import argv

with open(argv[1], 'r') as inp:
  width, height = map(int, inp.readline().split())

  img = np.ndarray(shape=(height,width,3), dtype=np.uint16)
  for y in range(height):
    row = list(map(float, inp.readline().split()))
    row = [int(65535 * x) for x in row]
    for x in range(width):
      img[y][x] = np.array(row[3*x: 3*x+3])

with open(argv[2], 'wb') as f:
  writer = png.Writer(width=img.shape[1], height=img.shape[0], bitdepth=16, greyscale=False)
  z = img.reshape(-1, img.shape[1]*img.shape[2]).tolist()
  writer.write(f, z);

