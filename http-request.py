#!/usr/bin/env python

import json
import requests  # HTTP Requests lib

r = requests.get("http://example.com/")

print("GET " + r.url + " returned Content-Length: {}".format(r.headers["content-length"]))
print("...and all the rest of the headers:")
print(json.dumps(dict(r.headers),sort_keys=True,indent=2))
