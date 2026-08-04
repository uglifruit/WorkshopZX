ORG 32768

START:
        DI              ; Disable interrupts

MAIN_LOOP:
        ; --- KEYBOARD SCANNER ---
        LD BC, $FBFE
        IN A, (C)
        BIT 0, A
        JR Z, PLAY_Q

        LD BC, $FDFE
        IN A, (C)
        BIT 0, A
        JR Z, PLAY_A

        LD BC, $DFFE
        IN A, (C)
        BIT 0, A
        JR Z, PLAY_P
        BIT 1, A
        JR Z, PLAY_O

        LD BC, $7FFE
        IN A, (C)
        BIT 0, A
        JP Z, PLAY_SPACE

        LD BC, $BFFE
        IN A, (C)
        BIT 0, A
        JP Z, PLAY_ENTER

        JR MAIN_LOOP

; ---------------------------------------------------------
; [Q] VCA DECAY PING
; Port 95 is patched to Envelope Length.
; Pitch falls slightly, but Port 95 defines if it's a
; micro-click (0) or a long, ringing tom/ping (255).
; ---------------------------------------------------------
PLAY_Q:
        IN A, (95)
        LD D, A         ; Port 95 directly sets the High Byte of duration!
        LD E, 0
        LD L, 1         ; Initial pitch (delay counter)
Q_LOOP:
        LD A, 16
        OUT (254), A
        LD B, L
Q_D1:   DJNZ Q_D1       ; High phase

        XOR A
        OUT (254), A
        LD B, L
Q_D2:   DJNZ Q_D2       ; Low phase

        INC L           ; Drop pitch slightly every cycle

        DEC DE          ; Decrease overall VCA envelope
        LD A, D
        OR E
        JR NZ, Q_LOOP
        JR MAIN_LOOP

; ---------------------------------------------------------
; [A] PULSE WIDTH MODULATION (PWM)
; Port 95 is patched to Duty Cycle.
; A fixed pitch where CV sweeps the square wave from a
; thin, nasal 1% pulse to a full, hollow 99% square.
; ---------------------------------------------------------
PLAY_A:
        LD DE, $0800    ; Fixed duration
A_LOOP:
        IN A, (95)
        LD B, A         ; B = HIGH time
        CPL             ; Invert bits (255 - A)
        LD C, A         ; C = LOW time

        LD A, 16
        OUT (254), A
A_D1:   DJNZ A_D1       ; Delay for HIGH time

        XOR A
        OUT (254), A
A_D2:   DEC C           ; DJNZ uses B, so we use DEC C here
        JR NZ, A_D2     ; Delay for LOW time

        DEC DE
        LD A, D
        OR E
        JR NZ, A_LOOP
        JR MAIN_LOOP

; ---------------------------------------------------------
; [O] BINARY RHYTHMIC GATING (AM)
; Port 95 acts as a bitmask patched to the VCA.
; It bitwise-ANDs against a rising counter to gate white
; noise. Sweeping Port 95 creates shifting, evolving
; stutter rhythms and glitching percussion.
; ---------------------------------------------------------
PLAY_O:
        LD HL, 0        ; H will be our slow rhythm counter
        LD DE, $1500    ; Duration
O_LOOP:
        INC HL          ; Advance rhythm time
        IN A, (95)
        AND H           ; Mask the counter with the CV input!
        JR Z, O_SILENT  ; If the result is 0, gate is CLOSED

        LD A, R         ; Gate is OPEN: generate noise
        AND 16
        OUT (254), A
        JR O_NEXT

O_SILENT:
        XOR A
        OUT (254), A

O_NEXT:
        LD B, 15        ; Base noise pitch delay
O_DEL:  DJNZ O_DEL

        DEC DE
        LD A, D
        OR E
        JR NZ, O_LOOP
        JP MAIN_LOOP

; ---------------------------------------------------------
; [P] HARD SYNC VOCAL FORMANTS
; A dual-oscillator engine. The fundamental pitch is locked,
; but Port 95 is patched to the pitch of a Master oscillator
; that gets hard-reset by the Sync oscillator.
; Sweeping 95 creates "talking" vowel sounds.
; ---------------------------------------------------------
PLAY_P:
        LD H, 0         ; Sync Oscillator (Fundamental)
        LD L, 0         ; Master Oscillator (Formant)
        LD DE, $0800
P_LOOP:
        INC H
        INC L

        LD A, H
        CP 80           ; Fixed fundamental pitch (reset point)
        JR C, P_CHK_MST
        LD H, 0         ; Reset Sync
        LD L, 0         ; HARD SYNC reset the Master!

P_CHK_MST:
        IN A, (95)
        CP L            ; Does Master reach the CV threshold?
        JR NC, P_OUT
        LD L, 0         ; Reset Master

P_OUT:
        ; Output a 50% duty cycle based on the Master oscillator
        SRL A           ; Half of Port 95
        CP L
        JR C, P_HIGH
        XOR A
        JR P_DO_OUT
P_HIGH: LD A, 16
P_DO_OUT:
        OUT (254), A

        LD B, 8         ; Global pitch scalar
P_DEL:  DJNZ P_DEL

        DEC DE
        LD A, D
        OR E
        JR NZ, P_LOOP
        JP MAIN_LOOP

; ---------------------------------------------------------
; [SPACE] LFSR NOISE CLOCK DIVIDER
; Generates true pseudorandom noise using a shift register.
; Port 95 is patched to the Sample Rate (Clock Divider),
; sweeping from bright hiss (0) to crushed, granular crackle (255).
; ---------------------------------------------------------
PLAY_SPACE:
        LD HL, 1        ; LFSR seed (must not be 0)
        LD DE, $1800    ; Duration
SPC_LOOP:
        ; 16-bit Galois LFSR tap shift
        SRL H
        RR L
        JR NC, SPC_OUT
        LD A, H
        XOR $B4         ; Magic feedback polynomial
        LD H, A
SPC_OUT:
        LD A, L
        AND 16          ; Output bit 4
        OUT (254), A

        IN A, (95)      ; Read CV for clock division
        INC A
        LD B, A
SPC_DEL:
        DJNZ SPC_DEL    ; Delay loop extends the current bit's duration

        DEC DE
        LD A, D
        OR E
        JR NZ, SPC_LOOP
        JP MAIN_LOOP

; ---------------------------------------------------------
; [ENTER] ARPEGGIATOR INTERVAL
; Port 95 is patched to the pitch jump amount.
; Every 256 cycles, the base pitch is incremented by
; the value of Port 95, creating rapid arpeggios, chiptune
; chords, or slow plunging sweeps depending on the CV value.
; ---------------------------------------------------------
PLAY_ENTER:
        LD C, 0         ; Active pitch tracker
        LD DE, $0A00    ; Duration
ENT_LOOP:
        IN A, (95)
        LD B, A         ; B = Arp step size

        LD A, D
        AND $07         ; Check lower bits of duration counter
        JR NZ, ENT_PLAY ; Only step the sequence occasionally

        LD A, C
        ADD A, B        ; Add interval to pitch
        LD C, A

ENT_PLAY:
        LD A, 16
        OUT (254), A
        LD B, C
ENT_D1: DJNZ ENT_D1

        XOR A
        OUT (254), A
        LD B, C
ENT_D2: DJNZ ENT_D2

        DEC DE
        LD A, D
        OR E
        JR NZ, ENT_LOOP
        JP MAIN_LOOP

        end 32768
