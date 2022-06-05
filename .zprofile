
alias ll='ls -la --color'
export PATH=${PATH}:~/Library/Python/3.8/bin
export JAVA_HOME=$(/usr/libexec/java_home -v11)

eval "$(/opt/homebrew/bin/brew shellenv)"  # M1: exports several environment variables

# https://learn.adafruit.com/circuitpython-libraries-on-any-computer-with-mcp2221/mac-osx
export BLINKA_MCP2221="1"

