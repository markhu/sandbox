sandbox
=======

sandbox is a test

## basic Git steps:

1. git-clone
2. edit files
2. git add  # (optional)
3. git-commit
4. git-push


### basic docker steps:

1. `docker build -t` repo/name .
2. `docker run` repo/name <optional_command(s)>

### basic Spock steps:

Note that running the Groovy/Spock scripts standalone requires _un-commenting_ the Grapes/Grab lines.
And conversely, running them under Maven requires _commenting out_ the Grapes/Grab lines.

1. `mvn clean test` ;  # runs the tests under src/ via Maven
2. `./src/test/groovy/spock/test-spock-Spec.groovy` ;  # standalone script

---

see also [..](..)
