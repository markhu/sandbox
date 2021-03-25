

# git log1  # requires alias ~/.gitconfig
#   l0g1 = log -n11 --oneline
#   log1 = log -n11 --pretty=format:'%C(auto,yellow)%h %C(auto,blue)%>(18,trunc)%ad %C(auto,green)%<(12,trunc)%aN %C(auto,reset)%s%C(auto,red)% gD% D'

git config --get remote.origin.url
git pull ; git log1 $* ; git status

