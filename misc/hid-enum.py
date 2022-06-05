#!/usr/bin/python3

import json
import os
import hid  # USB

# import hid
print("hid.enumerate()")

for h in hid.enumerate():
  if h["vendor_id"] not in [4176,8738,9610,0]:  # blocklist keyboard,mouse
    h["path"] = h["path"].decode()
    print(json.dumps(h,indent=2))

# print("hid.device().open(0x04D8, 0x00DD)")
device = hid.device()
# device.open(0x04D8, 0x00DD)
print(dir(device))


