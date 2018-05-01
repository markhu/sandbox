
" A 'minimal' ~/.vimrc file

set bg=dark
set expandtab
set hlsearch
set incsearch
set ignorecase
set laststatus=2
set shiftwidth=4
set softtabstop=4

" set number!  " display line numbers
set ruler    " easy display current line/col vs. specifying a complex statusline
set t_ti= t_te=  " norestorescreen (don't clear)

syntax on  " colorize hilighting

" restore last position when reopening a file --requires `mkdir -p ~/.vim/`
au BufWinLeave * mkview
au BufWinEnter * silent loadview

