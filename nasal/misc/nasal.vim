" Vim syntax file
" Language:	Nasal (FlightGear)
" Maintainer:	Melchior FRANZ <mfranz@aon.at>
" URL:		http://members.aon.at/mfranz/nasal.vim
" Last Change:	2004 Feb 16
" $Id: nasal.vim,v 1.3 2004-12-07 00:32:37 andy Exp $
"
" type ":help new-filetype" in vim for installation instructions


" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if !exists("main_syntax")
  if version < 600
    syntax clear
"  elseif exists("b:current_syntax")
"    finish
  endif
  let main_syntax = 'nasal'
endif

setlocal iskeyword=46,95,97-122

syn region  nasalFunction         start="\h[[:alnum:]_.]*\s*=\s*func" end="{"he=e-1 contains=nasalStatementFunc,nasalComment,nasalEqual
syn keyword nasalStatementFunc    contained func
syn match   nasalEqual            display contained "="
syn match   nasalFunction         display "\h[[:alnum:]_.]\+(\@="
syn match   nasalComment          display "#.*$"
syn region  nasalStringS          start=+'+  skip=+\\'+  end=+'+  contains=nasalSpecialS
syn region  nasalStringD          start=+"+  skip=+\\"+  end=+"+  contains=nasalSpecialD,nasalSpecial
syn match   nasalSpecialS         contained "\\'"
syn match   nasalSpecialD         contained "\\[\\rnt\"]"
syn match   nasalSpecial          contained "\\x[[:xdigit:]][[:xdigit:]]"

syn match   nasalNumber           display "[a-zA-Z_]\@<![-+]\=\d[[:digit:]_]*[a-zA-Z_]\@!"
syn match   nasalFloat            display "[a-zA-Z_]\@<![-+]\=\d[[:digit:]_]*[eE][\-+]\=\d\+[a-zA-Z_]\@!"
syn match   nasalFloat            display "[a-zA-Z_]\@<![-+]\=\d[[:digit:]_]*\.[[:digit:]_]*\([eE][\-+]\=\d\+\)\=[a-zA-Z_]\@!"
syn match   nasalFloat            display "[a-zA-Z_]\@<![-+]\=\.[[:digit:]_]\+\([eE][\-+]\=\d\+\)\=[a-zA-Z_]\@!"
syn match   nasalStatement        display "\<contains\>"
"globals?
syn keyword nasalIdentifier       me arg parents _cmdarg _globals
syn keyword nasalOperator         or and
syn keyword nasalConditional      if elsif else
syn keyword nasalRepeat           while for foreach
syn keyword nasalBranch           break continue switch case default
syn keyword nasalStatement        return with append call eval
syn keyword nasalStatement        delete int keys num pop size streq substr typeof
syn keyword nasalMathStatement    math.sin math.cos math.exp math.ln math.sqrt math.atan
syn keyword nasalMathStatement    math.e math.pi
syn keyword nasalFGFSStatement    getprop setprop print rand settimer
syn keyword nasalFGFSStatement    _fgcommand _interpolate
syn keyword nasalFGFSStatement    _fgcommand _interpolate _new _getNode _removeChild _getChildren _getChild
syn keyword nasalFGFSStatement    _getParent _setDoubleValue _setBoolValue _setIntValue _setValue _getValue
syn keyword nasalFGFSStatement    _getType _getName _getIndex
syn keyword nasalType             nil
syn keyword nasalBoolean          true false

syn cluster nasalParenGroup       contains=nasalParenError,nasalSpecial
syn region  nasalParen            transparent start='(' end=')' contains=ALLBUT,@nasalParenGroup,nasalErrInBracket
syn match   nasalParenError       display "[\])]"
syn match   nasalErrInParen       display contained "[\]{}]"
syn region  nasalBracket          transparent start='\[' end=']' contains=ALLBUT,@nasalParenGroup,nasalErrInParen
syn match   nasalErrInBracket     display contained "[);{}]"


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
  HiLink nasalStatementFunc     Statement
  HiLink nasalFunctionName      Function
  HiLink nasalComment           Comment
  HiLink nasalSpecial           Special
  HiLink nasalSpecialS          Special
  HiLink nasalSpecialD          Special
  HiLink nasalStringS           String
  HiLink nasalStringD           String
  HiLink nasalCharacter         Character
  HiLink nasalSpecialCharacter  nasalSpecial
  HiLink nasalNumber            Number
  HiLink nasalFloat             Float
  HiLink nasalIdentifier        Identifier
  HiLink nasalConditional       Conditional
  HiLink nasalRepeat            Repeat
  HiLink nasalBranch            Conditional
  HiLink nasalOperator          Operator
  HiLink nasalType              Type
  HiLink nasalStatement         Statement
  HiLink nasalFGFSStatement     Statement
  HiLink nasalMathStatement     Statement
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
