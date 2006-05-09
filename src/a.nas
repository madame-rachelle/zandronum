; "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
; Ken Silverman's official web site: "http://www.advsys.net/ken"
; See the included license file "BUILDLIC.TXT" for license info.
; This file has been modified from Ken Silverman's original release

	SECTION .data

%ifndef M_TARGET_LINUX
%define ylookup			_ylookup
%define vplce			_vplce
%define vince			_vince
%define palookupoffse	_palookupoffse
%define bufplce			_bufplce
%define dc_iscale		_dc_iscale
%define dc_colormap		_dc_colormap
%define dc_count		_dc_count
%define dc_dest			_dc_dest
%define dc_source		_dc_source
%define dc_texturefrac	_dc_texturefrac

%define setupvlineasm	_setupvlineasm
%define prevlineasm1	_prevlineasm1
%define vlineasm1		_vlineasm1
%define vlineasm4		_vlineasm4

%define setupmvlineasm		_setupmvlineasm
%define mvlineasm1		_mvlineasm1
%define mvlineasm4		_mvlineasm4
%endif

EXTERN ylookup ; near

EXTERN vplce ; near
EXTERN vince ; near
EXTERN palookupoffse ; near
EXTERN bufplce ; near

EXTERN dc_iscale
EXTERN dc_colormap
EXTERN dc_count
EXTERN dc_dest
EXTERN dc_source
EXTERN dc_texturefrac

mvlineasm4_counter:
	dd 0

	SECTION .text

ALIGN 16
GLOBAL setvlinebpl_
setvlinebpl_:
	mov [fixchain1a+2], eax
	mov [fixchain1b+2], eax
	mov [fixchain2a+2], eax
	mov [fixchain1m+2], eax
	mov [fixchain2ma+2], eax
	mov [fixchain2mb+2], eax
	ret

; pass it log2(texheight)

ALIGN 16
GLOBAL setupvlineasm
setupvlineasm:
	mov ecx, [esp+4]

		;First 2 lines for VLINEASM1, rest for VLINEASM4
	mov byte [premach3a+2], cl
	mov byte [mach3a+2], cl

	mov byte [machvsh1+2], cl      ;32-shy
	mov byte [machvsh3+2], cl      ;32-shy
	mov byte [machvsh5+2], cl      ;32-shy
	mov byte [machvsh6+2], cl      ;32-shy
	mov ch, cl
	sub ch, 16
	mov byte [machvsh8+2], ch      ;16-shy
	neg cl
	mov byte [machvsh7+2], cl      ;shy
	mov byte [machvsh9+2], cl      ;shy
	mov byte [machvsh10+2], cl     ;shy
	mov byte [machvsh11+2], cl     ;shy
	mov byte [machvsh12+2], cl     ;shy
	mov eax, 1
	shl eax, cl
	dec eax
	mov dword [machvsh2+2], eax    ;(1<<shy)-1
	mov dword [machvsh4+2], eax    ;(1<<shy)-1
	ret

	SECTION .rtext	progbits alloc exec write align=64

;eax = xscale
;ebx = palookupoffse
;ecx = # pixels to draw-1
;edx = texturefrac
;esi = texturecolumn
;edi = buffer pointer

ALIGN 16
GLOBAL prevlineasm1
prevlineasm1:
		mov ecx, [dc_count]
		cmp ecx, 1
		ja vlineasm1

		mov eax, [dc_iscale]
		mov edx, [dc_texturefrac]
		add eax, edx
		mov ecx, [dc_source]
premach3a: 	shr edx, 32
		push ebx
		push edi
		mov edi, [dc_colormap]
		xor ebx, ebx
		mov bl, byte [ecx+edx]
		mov ecx, [dc_dest]
		mov bl, byte [edi+ebx]
		pop edi
		mov byte [ecx], bl
		pop ebx
		ret

GLOBAL vlineasm1
ALIGN 16
vlineasm1:
		push ebx
		push edi
		push esi
		push ebp
		mov ecx, [dc_count]
		mov ebp, [dc_colormap]
		mov edi, [dc_dest]
		mov eax, [dc_iscale]
		mov edx, [dc_texturefrac]
		mov esi, [dc_source]
fixchain1a:	sub edi, 320
		nop
		nop
		nop
beginvline:
		mov ebx, edx
mach3a:		shr ebx, 32
fixchain1b:	add edi, 320
		mov bl, byte [esi+ebx]
		add edx, eax
		dec ecx
		mov bl, byte [ebp+ebx]
		mov byte [edi], bl
		jnz short beginvline
		pop ebp
		pop esi
		pop edi
		pop ebx
		mov eax, edx
		ret

	;eax: -------temp1-------
	;ebx: -------temp2-------
	;ecx:  dat  dat  dat  dat
	;edx: ylo2           ylo4
	;esi: yhi1           yhi2
	;edi: ---videoplc/cnt----
	;ebp: yhi3           yhi4
	;esp:
ALIGN 16
GLOBAL vlineasm4
vlineasm4:
		mov ecx, [dc_count]
		push ebp
		push ebx
		push esi
		push edi
		mov edi, [dc_dest]

		mov eax, dword [ylookup+ecx*4-4]
		add eax, edi
		mov dword [machvline4end+2], eax
		sub edi, eax

		mov eax, dword [bufplce+0]
		mov ebx, dword [bufplce+4]
		mov ecx, dword [bufplce+8]
		mov edx, dword [bufplce+12]
		mov dword [machvbuf1+2], ecx
		mov dword [machvbuf2+2], edx
		mov dword [machvbuf3+2], eax
		mov dword [machvbuf4+2], ebx

		mov eax, dword [palookupoffse+0]
		mov ebx, dword [palookupoffse+4]
		mov ecx, dword [palookupoffse+8]
		mov edx, dword [palookupoffse+12]
		mov dword [machvpal1+2], ecx
		mov dword [machvpal2+2], edx
		mov dword [machvpal3+2], eax
		mov dword [machvpal4+2], ebx

;     旼컴컴컴컴컴컴컴쩡컴컴컴컴컴컴컴�
;edx: 퀆3lo           퀆1lo           �
;     쳐컴컴컴컴컴컴컴좔컴컴컴쩡컴컴컴�
;esi: 퀆2hi  v2lo             �   v3hi�
;     쳐컴컴컴컴컴컴컴컴컴컴컴탠컴컴컴�
;ebp: 퀆0hi  v0lo             �   v1hi�
;     읕컴컴컴컴컴컴컴컴컴컴컴좔컴컴컴�

		mov ebp, dword [vince+0]
		mov ebx, dword [vince+4]
		mov esi, dword [vince+8]
		mov eax, dword [vince+12]
		and esi, 0fffffe00h
		and ebp, 0fffffe00h
machvsh9:	rol eax, 88h                ;sh
machvsh10:	rol ebx, 88h                ;sh
		mov edx, eax
		mov ecx, ebx
		shr ecx, 16
		and edx, 0ffff0000h
		add edx, ecx
		and eax, 000001ffh
		and ebx, 000001ffh
		add esi, eax
		add ebp, ebx
		;
		mov eax, edx
		and eax, 0ffff0000h
		mov dword [machvinc1+2], eax
		mov dword [machvinc2+2], esi
		mov byte [machvinc3+2], dl
		mov byte [machvinc4+2], dh
		mov dword [machvinc5+2], ebp

		mov ebp, dword [vplce+0]
		mov ebx, dword [vplce+4]
		mov esi, dword [vplce+8]
		mov eax, dword [vplce+12]
		and esi, 0fffffe00h
		and ebp, 0fffffe00h
machvsh11:	rol eax, 88h                ;sh
machvsh12:	rol ebx, 88h                ;sh
		mov edx, eax
		mov ecx, ebx
		shr ecx, 16
		and edx, 0ffff0000h
		add edx, ecx
		and eax, 000001ffh
		and ebx, 000001ffh
		add esi, eax
		add ebp, ebx

		mov ecx, esi
		jmp short beginvlineasm4
ALIGN 16
beginvlineasm4:
machvsh1:	shr ecx, 88h          ;32-sh
		mov ebx, esi
machvsh2:	and ebx, 00000088h    ;(1<<sh)-1
machvinc1:	add edx, 88880000h
machvinc2:	adc esi, 88888088h
machvbuf1:	mov cl, byte [ecx+88888888h]
machvbuf2:	mov bl, byte [ebx+88888888h]
		mov eax, ebp
machvsh3:	shr eax, 88h          ;32-sh
machvpal1:	mov cl, byte [ecx+88888888h]
machvpal2:	mov ch, byte [ebx+88888888h]
		mov ebx, ebp
		shl ecx, 16
machvsh4:	and ebx, 00000088h    ;(1<<sh)-1
machvinc3:	add dl, 88h
machvbuf3:	mov al, byte [eax+88888888h]
machvinc4:	adc dh, 88h
machvbuf4:	mov bl, byte [ebx+88888888h]
machvinc5:	adc ebp, 88888088h
machvpal3:	mov cl, byte [eax+88888888h]
machvpal4:	mov ch, byte [ebx+88888888h]
machvline4end:	mov dword [edi+88888888h], ecx
fixchain2a:	add edi, 88888888h
		mov ecx, esi
		jle short beginvlineasm4

;     旼컴컴컴컴컴컴컴쩡컴컴컴컴컴컴컴�
;edx: 퀆3lo           퀆1lo           �
;     쳐컴컴컴컴컴컴컴좔컴컴컴쩡컴컴컴�
;esi: 퀆2hi  v2lo             �   v3hi�
;     쳐컴컴컴컴컴컴컴컴컴컴컴탠컴컴컴�
;ebp: 퀆0hi  v0lo             �   v1hi�
;     읕컴컴컴컴컴컴컴컴컴컴컴좔컴컴컴�

		mov dword [vplce+8], esi
		mov dword [vplce+0], ebp
;vplc2 = (esi<<(32-sh))+(edx>>sh)
;vplc3 = (ebp<<(32-sh))+((edx&65535)<<(16-sh))
machvsh5:	shl esi, 88h     ;32-sh
		mov eax, edx
machvsh6:	shl ebp, 88h     ;32-sh
		and edx, 0000ffffh
machvsh7:	shr eax, 88h     ;sh
		add esi, eax
machvsh8:	shl edx, 88h     ;16-sh
		add ebp, edx
		mov dword [vplce+12], esi
		mov dword [vplce+4], ebp

		pop edi
		pop esi
		pop ebx
		pop ebp
		ret

;*************************************************************************
;************************* Masked Vertical Lines *************************
;*************************************************************************

; pass it log2(texheight)

ALIGN 16
GLOBAL setupmvlineasm
setupmvlineasm:
		mov ecx, dword [esp+4]
		mov byte [maskmach3a+2], cl
		mov byte [machmv13+2], cl
		mov byte [machmv14+2], cl
		mov byte [machmv15+2], cl
		mov byte [machmv16+2], cl
		ret

ALIGN 16
GLOBAL mvlineasm1	;Masked vline
mvlineasm1:
		push ebx
		push edi
		push esi
		push ebp
		mov ecx, [dc_count]
		mov ebp, [dc_colormap]
		mov edi, [dc_dest]
		mov eax, [dc_iscale]
		mov edx, [dc_texturefrac]
		mov esi, [dc_source]
beginmvline:
		mov ebx, edx
maskmach3a:	shr ebx, 32
		mov bl, byte [esi+ebx]
		cmp bl, 0
		je short skipmask1
maskmach3c:	mov bl, byte [ebp+ebx]
		mov [edi], bl
skipmask1:	add edx, eax
fixchain1m:	add edi, 320
		dec ecx
		jnz short beginmvline

		pop ebp
		pop esi
		pop edi
		pop ebx
		mov eax, edx
		ret

ALIGN 16
GLOBAL mvlineasm4
mvlineasm4:
		push ebx
		push esi
		push edi
		push ebp

		mov ecx,[dc_count]
		mov edi,[dc_dest]

		mov eax, [bufplce+0]
		mov ebx, [bufplce+4]
		mov [machmv1+2], eax
		mov [machmv4+2], ebx
		mov eax, [bufplce+8]
		mov ebx, [bufplce+12]
		mov [machmv7+2], eax
		mov [machmv10+2], ebx

		mov eax, [palookupoffse]
		mov ebx, [palookupoffse+4]
		mov [machmv2+2], eax
		mov [machmv5+2], ebx
		mov eax, [palookupoffse+8]
		mov ebx, [palookupoffse+12]
		mov [machmv8+2], eax
		mov [machmv11+2], ebx

		mov eax, [vince]	;vince
		mov ebx, [vince+4]
		xor al, al
		xor bl, bl
		mov [machmv3+2], eax
		mov [machmv6+2], ebx
		mov eax, [vince+8]
		mov ebx, [vince+12]
		mov [machmv9+2], eax
		mov [machmv12+2], ebx

		inc ecx
		push ecx
		mov ecx, [vplce+0]
		mov edx, [vplce+4]
		mov esi, [vplce+8]
		mov ebp, [vplce+12]
fixchain2ma:	sub edi, 320

		jmp short beginmvlineasm4
ALIGN 16
beginmvlineasm4:
		dec dword [esp]
		jz near endmvlineasm4

		mov eax, ebp
		mov ebx, esi
machmv16:	shr eax, 32
machmv15:	shr ebx, 32
machmv12:	add ebp, 0x88888888		;vince[3]
machmv9:	add esi, 0x88888888		;vince[2]
machmv10:	mov al, [eax+0x88888888]	;bufplce[3]
machmv7:	mov bl, [ebx+0x88888888]	;bufplce[2]
		cmp al, 1
		adc dl, dl
		cmp bl, 1
		adc dl, dl
machmv8:	mov bl, [ebx+0x88888888]	;palookupoffs[2]
machmv11:	mov bh, [eax+0x88888888]	;palookupoffs[3]

		mov eax, edx
machmv14:	shr eax, 32
		shl ebx, 16
machmv4:	mov al, [eax+0x88888888]	;bufplce[1]
		cmp al, 1
		adc dl, dl
machmv6:	add edx, 0x88888888		;vince[1]
machmv5:	mov bh, [eax+0x88888888]	;palookupoffs[1]

		mov eax, ecx
machmv13:	shr eax, 32
machmv3:	add ecx, 0x88888888		;vince[0]
machmv1:	mov al, [eax+0x88888888]	;bufplce[0]
		cmp al, 1
		adc dl, dl
machmv2:	mov bl, [eax+0x88888888]	;palookupoffs[0]

		shl dl, 4
		xor eax, eax
fixchain2mb:	add edi, 320
		mov al, dl
		add eax, mvcase15
		jmp eax		;16 byte cases

ALIGN 16
endmvlineasm4:
		mov [_vplce], ecx
		mov [_vplce+4], edx
		mov [_vplce+8], esi
		mov [_vplce+12], ebp
		pop ecx
		pop ebp
		pop edi
		pop esi
		pop ebx
		ret

	;5,7,8,8,11,13,12,14,11,13,14,14,12,14,15,7
ALIGN 16
mvcase15:	mov [edi], ebx
		jmp beginmvlineasm4
ALIGN 16
mvcase14:	mov [edi+1], bh
		shr ebx, 16
		mov [edi+2], bx
		jmp beginmvlineasm4
ALIGN 16
mvcase13:	mov [edi], bl
		shr ebx, 16
		mov [edi+2], bx
		jmp beginmvlineasm4
ALIGN 16
mvcase12:	shr ebx, 16
		mov [edi+2], bx
		jmp beginmvlineasm4
ALIGN 16
mvcase11:	mov [edi], bx
		shr ebx, 16
		mov [edi+3], bh
		jmp beginmvlineasm4
ALIGN 16
mvcase10:	mov [edi+1], bh
		shr ebx, 16
		mov [edi+3], bh
		jmp beginmvlineasm4
ALIGN 16
mvcase9:	mov [edi], bl
		shr ebx, 16
		mov [edi+3], bh
		jmp beginmvlineasm4
ALIGN 16
mvcase8:	shr ebx, 16
		mov [edi+3], bh
		jmp beginmvlineasm4
ALIGN 16
mvcase7:	mov [edi], bx
		shr ebx, 16
		mov [edi+2], bl
		jmp beginmvlineasm4
ALIGN 16
mvcase6:	shr ebx, 8
		mov [edi+1], bx
		jmp beginmvlineasm4
ALIGN 16
mvcase5:	mov [edi], bl
		shr ebx, 16
		mov [edi+2], bl
		jmp beginmvlineasm4
ALIGN 16
mvcase4:	shr ebx, 16
		mov [edi+2], bl
		jmp beginmvlineasm4
ALIGN 16
mvcase3:	mov [edi], bx
		jmp beginmvlineasm4
ALIGN 16
mvcase2:	mov [edi+1], bh
		jmp beginmvlineasm4
ALIGN 16
mvcase1:	mov [edi], bl
		jmp beginmvlineasm4
ALIGN 16
mvcase0:	jmp beginmvlineasm4
