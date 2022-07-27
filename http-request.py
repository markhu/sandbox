#!/usr/bin/env python3

import json
import requests  # HTTP Requests lib
import sys

url = sys.argv[1] if len(sys.argv)>1 else "http://example.com/"
url = f"http://{url}" if "http" not in url else url
r = requests.get(url)

print("GET " + r.url + " returned Content-Length: {}".format(r.headers.get("Content-Length")))
print("...and these headers:")
print(json.dumps(dict(r.headers),sort_keys=True,indent=2))
