#!/usr/bin/python3

import os
import board
import hid

print("BLINKA_MCP2221:",os.environ["BLINKA_MCP2221"])

print("~" * 80)

# import hid
print("hid.enumerate() --but only the MCP2221 bits...")

for h in hid.enumerate():
  if "MCP2" in h["product_string"] or h["vendor_id"]==1240:
    for e in h.items():
      print(e)

print("~" * 80)
print("hid.device().open(0x04D8, 0x00DD)")

device = hid.device()
device.open(0x04D8, 0x00DD)
print(dir(device))

print("~" * 80)
print("import board")

# import board

print("dir(board)")
# print(dir(board))

