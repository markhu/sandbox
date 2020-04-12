#!/usr/bin/env python3

# invoke with robot --listener API-Libs/RF_Listener.py

# http://robotframework.org/robotframework/latest/RobotFrameworkUserGuide.html#listeners-logging
# https://stackoverflow.com/questions/54152481/parse-robot-frameworks-output-xml

import os
import sys
import xml.etree.ElementTree as xmlElementTree

ROBOT_LISTENER_API_VERSION = 3


class RF_Listener:  # class should be same as filename

    def __init__(self, max_seconds=10):
        self.ROBOT_LISTENER_API_VERSION = 3
        self.max_milliseconds = float(max_seconds) * 1000
        self.qaCount = { "FAIL":0, "OTHER":0, "WARN":0 }

    def output_file(self, path):  # Listener that parses the output XML when it is ready
      root = xmlElementTree.parse(path).getroot()
      for type_tag in root.findall('./statistics/total/stat'):
      # <stat pass="1" fail="2">Critical Tests</stat>
      # <stat pass="3" fail="4">All Tests</stat>
        cntPassed = int(type_tag.attrib.get("pass"))  # attrib is dict-like (except for 'text')
        cntFailed = int(type_tag.attrib.get("fail"))
        cntTests = cntPassed + cntFailed
        pct_pass = cntPassed / (cntTests + 0.01) * 100  # defend divide-by-zero
        fmt_str = "{}: {} tests, {} passed, {} failed, {:.3g}% pass rate (--listener summary)"
        print(fmt_str.format(type_tag.text,cntTests, cntPassed, cntFailed,pct_pass))
      # optionally write grand total results summary to a file


if __name__ == "__main__":
  print("Invoked from __main__ for testing")
  try:
    filename = sys.argv[1]
  except:
    filename = "Results/output.xml"

  print("DEBUG: test data filename: " + filename)
  RF_Listener.output_file(self=None,path=filename)

