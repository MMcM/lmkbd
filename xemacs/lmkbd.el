;;; -*- Mode: Emacs-Lisp; coding: utf-8 -*-

;; Some of these Unicode characters do not correspond to anything in a
;; character set that un-define knows about.  They get lost when this
;; file is loaded.
(dolist (key '((alpha . #x03B1)         ;α
               (approximate . #x2248)   ;≈
               (atsign . #x0040)        ;@
               (beta . #x03B2)          ;β
               (braceleft . #x007B)     ;{
               (braceright . #x007D)    ;}
               (bracketleft . #x005B)   ;[
               (bracketright . #x005D)  ;]
               ;; These are only half of the legend.  Is there really
               ;; a graphic char on this keyboard with no
               ;; corresponding Unicode glyph?
               (broketleft . #x231C)    ;⌜
               (broketright . #x231E)   ;⌞
               (caret . #x005E)         ;^
               (ceiling . #x2308)       ;⌈
               (cent . #x00A2)          ;¢
               (chi . #x03C7)           ;χ
               (circle . #x25CB)        ;○
               (circleminus . #x2296)   ;⊖
               (circleplus . #x2295)    ;⊕
               (circleslash . #x2298)   ;⊘
               (circletimes . #x2297)   ;⊗
               (colon . #x003A)         ;:
               (contained . #x2283)     ;⊃
               (dagger . #x2020)        ;†
               (degree . #x00B0)        ;°
               (del . #x2207)           ;∇
               (delta . #x2206)         ;∆
               (division . #x00F7)      ;÷
               (doubbaselinedot . #x00A8) ;¨
               (doublearrow . #x2194)   ;↔
               (doublebracketleft . #x27E6) ;⟦
               (doublebracketright . #x27E7) ;⟧
               (doubledagger . #x2021)  ;‡
               (doublevertbar . #x2016) ;‖
               (downarrow . #x2193)     ;↓
               (downtack . #x22A4)      ;⊤
               (epsilon . #x03B5)       ;ε
               (eta . #x03B7)           ;η
               (exists . #x2203)        ;∃
               (floor . #x230A)         ;⌊
               (forall . #x2200)        ;∀
               (gamma . #x03B3)         ;γ
               (greaterthanequal . #x2265) ;≥
               (guillemotleft . #x00AB) ;«
               (guillemotright . #x00BB) ;»
               (horizbar . #x2015)      ;―
               (identical . #x2261)     ;≡
               (includes . #x2282)      ;⊂
               (infinity . #x221E)      ;∞
               (integral . #x222B)      ;∫
               (intersection . #x2229)  ;∩
               (iota . #x03B9)          ;ι
               (kappa . #x03BA)         ;κ
               (lambda . #x03BB)        ;λ
               (leftanglebracket . #x2039) ;‹
               (leftarrow . #x2190)     ;←
               (lefttack . #x22A3)      ;⊣
               (lessthanequal . #x2264) ;≤
               (logicaland . #x2227)    ;∧
               (logicalor . #x2228)     ;∨
               (mu . #x03BC)            ;μ
               (notequal . #x2260)      ;≠
               (notsign . #x2310)       ;⌐
               (nu . #x03BD)            ;ν
               (omega . #x03C9)         ;ω
               (omicron . #x03BF)       ;ο
               (paragraph . #x00B6)     ;¶
               (parenleft . #x0028)     ;(
               (parenright . #x0029)    ;)
               (partialderivative . #x2202) ;∂
               (periodcentered . #x00B7) ;·
               (phi . #x03C6)           ;φ
               (pi . #x03C0)            ;π
               (plusminus . #x00B1)     ;±
               (psi . #x03C8)           ;ψ
               (quad . #x2395)          ;⎕
               (rho . #x03C1)           ;ρ
               (rightanglebracket . #x203A) ;›
               (rightarrow . #x2192)    ;→
               (righttack . #x22A2)     ;⊢
               (section . #x00A7)       ;§
               (sigma . #x03C3)         ;σ
               (similarequal . #x2243)  ;≃
               (tau . #x03C4)           ;τ
               (theta . #x03B8)         ;θ
               (times . #x00D7)         ;×
               (union . #x222A)         ;∪
               (uparrow . #x2191)       ;↑
               (upsilon . #x03C5)       ;υ
               (uptack . #x22A5)        ;⊥
               (varsigma . #x03C2)      ;ς
               (vartheta . #x03D1)      ;ϑ
               (xi . #x03BE)            ;ξ
               (zeta . #x03B6)          ;ζ
               ))
  (if (null (get (car key) character-set-property))
      (let ((char (ucs-to-char (cdr key))))
        (if (null char)
            (display-warning 'lmkbd 
              (format "Unicode \\u%04x (%s) not mapped." (cdr key) (car key)))
          (put (car key) character-set-property char)
          (global-set-key (car key) 'self-insert-command)))))

(dolist (key '((help . [(control h)])
               (line . [(control j)])
               (clearscreen . [(control l)])
               (scroll . [(control v)])
               ))
  (define-key global-map (car key) (cdr key)))
