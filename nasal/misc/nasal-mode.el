;; nasl-mode.el
;;
;; A major mode for writing Nasl code.
;; Copyright (C) 2003 Andrew Ross
;;
;; Based very closely on awk-mode.el as shipped with GNU Emacs 21.2
;; Copyright (C) 1988,94,96,2000  Free Software Foundation, Inc.

(defvar nasl-mode-syntax-table nil "Syntax table in use in Nasl-mode buffers.")
(if nasl-mode-syntax-table ()
  (setq nasl-mode-syntax-table (make-syntax-table))
  ; Operator characters are "punctuation"
  (modify-syntax-entry ?!  "."  nasl-mode-syntax-table)
  (modify-syntax-entry ?*  "."  nasl-mode-syntax-table)
  (modify-syntax-entry ?+  "."  nasl-mode-syntax-table)
  (modify-syntax-entry ?-  "."  nasl-mode-syntax-table)
  (modify-syntax-entry ?/  "."  nasl-mode-syntax-table)
  (modify-syntax-entry ?~  "."  nasl-mode-syntax-table)
  (modify-syntax-entry ?:  "."  nasl-mode-syntax-table)
  (modify-syntax-entry ?.  "."  nasl-mode-syntax-table)
  (modify-syntax-entry ?,  "."  nasl-mode-syntax-table)
  (modify-syntax-entry ?\; "."  nasl-mode-syntax-table)
  (modify-syntax-entry ?=  "."  nasl-mode-syntax-table)
  (modify-syntax-entry ?<  "."  nasl-mode-syntax-table)
  (modify-syntax-entry ?>  "."  nasl-mode-syntax-table)
  ; Underscores are allowed as "symbol constituent"
  (modify-syntax-entry ?_  "_"  nasl-mode-syntax-table)
  ; Backslash escapes; pound sign starts comments that newlines end.
  (modify-syntax-entry ?\\ "\\" nasl-mode-syntax-table)
  (modify-syntax-entry ?\# "<"  nasl-mode-syntax-table)
  (modify-syntax-entry ?\n ">"  nasl-mode-syntax-table)
  ; Square brackets act as parenthesis
  (modify-syntax-entry ?\[ "(]"  nasl-mode-syntax-table)
  (modify-syntax-entry ?]  ")"  nasl-mode-syntax-table))

(defconst nasl-font-lock-keywords
  (eval-when-compile
    (list
     (cons (regexp-opt '("parents" "me" "arg") 'words)
	   'font-lock-variable-name-face)

     (regexp-opt '("and" "or" "nil" "if" "elsif" "else" "for" "foreach"
		   "while" "return" "break" "continue" "func") 'words)

     (list (regexp-opt '("size" "keys" "append" "pop" "int" "streq" "substr"
			 "contains" "typeof") 'words)
	   1 'font-lock-builtin-face)
     ))
  "Nasl-specific syntax to hilight.")

(define-derived-mode nasl-mode c-mode "Nasl"
  "Major mode for editing Nasl code.
This is a C mode variant customized for Nasl's syntax.  It shares most of
C mode's features.  Turning on Nasl mode runs `nasl-mode-hook'."
  (set (make-local-variable 'paragraph-start) (concat "$\\|" page-delimiter))
  (set (make-local-variable 'paragraph-separate) paragraph-start)
  (set (make-local-variable 'comment-start) "# ")
  (set (make-local-variable 'comment-end) "")
  (set (make-local-variable 'comment-start-skip) "#+ *")
  (setq font-lock-defaults '(nasl-font-lock-keywords nil nil ((?_ . "w")))))

(provide 'nasl-mode)
