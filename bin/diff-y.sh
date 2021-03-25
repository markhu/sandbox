
COLUMNS=$(tput cols)  # terminal window width
set -x
diff -y -W ${COLUMNS} $*

