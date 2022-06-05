#!/usr/bin/python3

import os
import hid

print("BLINKA_MCP2221:",os.environ["BLINKA_MCP2221"])

print("~" * 80)

# import hid
print("hid.enumerate()")

for h in hid.enumerate():
  if h["vendor_id"] not in [4176,8738,9610,0]:
    print("~" * 80)
    for e in h.items():
      print(e)

print("~" * 80)
print("hid.device().open(0x04D8, 0x00DD)")

device = hid.device()
device.open(0x04D8, 0x00DD)
print(dir(device))


