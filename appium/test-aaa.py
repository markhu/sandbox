#!/usr/bin/env python

# appium test demo --Mark Hudson, 2019 May 08
# prep step(s) for Android:
#   $ emulator -avd "Nexus_5X_API_27"

import unittest
from appium import webdriver
from appium.webdriver.common.touch_action import TouchAction

testApp = "com.ace.shell.production"

class app:
  def launch_zip(self,myZip,textID):
    desired_caps = { "platformName": "Android", "platformVersion": 8.1,
      "deviceName": "Android Emulator",
      "appActivity": "com.ace.shell.launcherActivity.LauncherActivity",
      "appPackage": testApp,
      "app": "/tmp/" + testApp + "_1009.apk"
      }

    self.driver = webdriver.Remote('http://localhost:4723/wd/hub', desired_caps)

    inst = self.driver.is_app_installed(testApp)

    # send keys to enter ZIP code
    el = self.driver.find_element_by_id(testApp + ":id/activity_zipcode")
    el.send_keys(myZip)

    # tap "NEXT"
    el = self.driver.find_element_by_id(testApp + ":id/activity_zipcode_next")
    TouchAction(self.driver).tap(el).perform()

    # wait for ZIPcode processing...
    self.driver.implicitly_wait(3)

    # assert text field visible
    r = self.driver.find_elements_by_id(testApp + textID)
    el = self.driver.find_element_by_id(testApp + textID)
    el.send_keys("markhu")

    # self.driver.quit()
    return(r)


if __name__ == "__main__":
    print("launching " + testApp + " and sending ZIP 90006")
    textID = ":id/activity_login_username_editText"
    r = app().launch_zip(90006,textID)
    print("finding field" + textID)
    print("Result: {}".format(True if r else False))

