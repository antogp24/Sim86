lea ax, [bx]
lea ax, [si+5]
lea bx, [bp+di+10]
lea dx, [1234]

mul al
mul byte [bx]
mul byte [bp+si+6]
mul ah

mul ax
mul word [si]
mul word [bp+7]
mul dx

imul al
imul byte [bx]
imul byte [bp+di+4]
imul ah

imul ax
imul word [si]
imul word [bp+5]
imul dx

div al
div byte [bx]
div byte [bp+di+2]
div ah

div ax
div word [si]
div word [bp+7]
div dx

idiv al
idiv byte [bx]
idiv byte [bp+si+8]
idiv ah

idiv ax
idiv word [di]
idiv word [bp+6]
idiv dx

; shift by 1
shl al, 1
sal byte [bx], 1
shl ah, 1
sal word [bp+si], 1
shl ax, 1
sal word [si], 1

; shift by cl
shl al, cl
sal byte [bx], cl
shl ah, cl
sal word [bp+si], cl
shl ax, cl
sal word [si], cl

; shift by 1
sar al, 1
sar byte [bx], 1
sar ah, 1
sar word [bp+di], 1
sar ax, 1
sar word [di], 1

; shift by cl
sar al, cl
sar byte [bx], cl
sar ah, cl
sar word [bp+di], cl
sar ax, cl
sar word [di], cl

not al
not byte [bx]
not ah
not byte [bp+si]

not ax
not word [di]
not dx
not word [bp+di]
