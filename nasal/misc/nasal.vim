" Vim syntax file
" Language:	Nasal (FlightGear)
" Maintainer:	Melchior FRANZ <mfranz@aon.at>
" URL:		http://members.aon.at/mfranz/nasal.vim
" Last Change:	2003 Nov 26

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
"if !exists("main_syntax")
"  if version < 600
    syntax clear
"  elseif exists("b:current_syntax")
"  finish
"endif
  let main_syntax = 'nasal'
"endif

syn case ignore
setlocal iskeyword=45,46,64-90,97-122,192-255


syn match   nasalLineComment      "#.*$"
syn match   nasalSpecial          "\\\d\d\d\|\\."
syn region  nasalStringD          start=+"+  skip=+\\\\\|\\"+  end=+"+  contains=nasalSpecial,@htmlPreproc
syn region  nasalStringS          start=+'+  skip=+\\\\\|\\'+  end=+'+  contains=nasalSpecial,@htmlPreproc
"syn match   nasalSpecialCharacter "\\."
syn match   nasalNumber           "[+-]\=\(\d\+\|\d*\.\d\+\)"
syn keyword nasalConditional      if elsif else
syn keyword nasalRepeat           while for foreach
syn keyword nasalBranch           break continue switch case default
syn keyword nasalStatement        return with
syn keyword nasalStatement        func
syn keyword nasalFunction         append contains
syn keyword nasalFunction         delete int keys num pop size streq substr typeof
syn keyword nasalFunction         math.sin math.cos math.exp math.ln math.sqrt math.atan
syn keyword nasalFunction         getprop setprop print fgcommand settimer
syn keyword nasalBoolean          true false
syn match   nasalBraces           "[{}]"

" catch errors caused by wrong parenthesis
"syn match   nasalInParen     contained "[{}]"
"syn region  nasalParen       transparent start="(" end=")" contains=nasalParen,nasal.*
"syn match   nasalParenError  ")"

if main_syntax == "nasal"
  syn sync ccomment nasalComment
endif

" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_nasal_syn_inits")
  if version < 508
    let did_nasal_syn_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif
  HiLink nasalComment           Comment
  HiLink nasalLineComment       Comment
  HiLink nasalSpecial           Special
  HiLink nasalStringS           String
  HiLink nasalStringD           String
  HiLink nasalCharacter         Character
  HiLink nasalSpecialCharacter  nasalSpecial
  HiLink nasalNumber            Number
  HiLink nasalConditional       Conditional
  HiLink nasalRepeat            Repeat
  HiLink nasalBranch            Conditional
  HiLink nasalOperator          Operator
  HiLink nasalType              Type
  HiLink nasalStatement         Statement
  HiLink nasalFunction          Function
  HiLink nasalBraces            Function
  HiLink nasalError             Error
  HiLink nasalParenError        nasalError
  HiLink nasalInParen           nasalError
  HiLink nasalBoolean           Boolean
  delcommand HiLink
endif

let b:current_syntax = "nasal"
if main_syntax == 'nasal'
  unlet main_syntax
endif

" vim: ts=8
