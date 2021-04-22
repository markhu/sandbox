#!/usr/bin/bash

# brew cask reinstall chromedriver
set -x
brew cask upgrade chromedriver
chromedriver --version

