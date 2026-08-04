ORG 32768

START:
        DI              ; Disable interrupts

MAIN_LOOP:
        ; --- CONTINUOUS KEYBOARD SCANNING ---
        LD BC, $FBFE
        IN A, (C)
        BIT 0, A
        JR Z, INIT_Q

        LD BC, $FDFE
        IN A, (C)
        BIT 0, A
        JR Z, INIT_A

        LD BC, $DFFE
        IN A, (C)
        BIT 0, A
        JP Z, INIT_P
        BIT 1, A
        JP Z, INIT_O

        LD BC, $7FFE
        IN A, (C)
        BIT 0, A
        JP Z, INIT_SPACE

        LD BC, $BFFE
        IN A, (C)
        BIT 0, A
        JP Z, INIT_ENTER

        JP MAIN_LOOP

; =========================================================
; [Q] THE RISER / SIREN
; Starts at a low sub-bass pitch and slowly rises in pitch 
; the longer you hold the key. 
; PORT 95 MODULATION: Acts as an FM offset, allowing you 
; to bend or warble the pitch as it rises.
; =========================================================
INIT_Q:
        LD HL, $FF00    ; H = Delay (Pitch). Starts at FF (very low)
                        ; L = Sub-counter for timing the rise
GATE_Q:
        LD A, 16
        OUT (254), A
        IN A, (95)      ; Read external CV
        ADD A, H        ; Add it to our current pitch state
        LD B, A
Q_D1:   DJNZ Q_D1

        XOR A
        OUT (254), A
        IN A, (95)
        ADD A, H
        LD B, A
Q_D2:   DJNZ Q_D2

        INC L           ; Advance sub-counter
        JR NZ, Q_CHK
        DEC H           ; Every 256 cycles, decrease delay (pitch goes UP)
        JR NZ, Q_CHK    
        INC H           ; Keep at maximum pitch (1), don't wrap to 0!
Q_CHK:
        LD BC, $FBFE    ; Is Q still pressed?
        IN A, (C)
        BIT 0, A
        JR Z, GATE_Q    ; If yes, keep evolving!
        JP MAIN_LOOP    ; If released, instantly stop.

; =========================================================
; [A] PORTAMENTO GLIDE (SLEW LIMITER)
; The sound attempts to match the pitch defined by Port 95.
; Holding the key causes the pitch to slowly "glide" to 
; match the knob. Wiggling 95 causes the pitch to sluggishly
; chase your movements like a heavy tape motor.
; =========================================================
INIT_A:
        LD H, 128       ; H = Current internal pitch
        LD L, 0         ; Slew rate counter
GATE_A:
        LD A, 16
        OUT (254), A
        LD B, H
A_D1:   DJNZ A_D1

        XOR A
        OUT (254), A
        LD B, H
A_D2:   DJNZ A_D2

        INC L
        JR NZ, A_CHK
        ; Time to slew! Compare our pitch (H) with Port 95
        IN A, (95)
        CP H
        JR Z, A_CHK     ; We are at the target pitch!
        JR C, A_DOWN
        INC H           ; Target is higher, slew up
        JR A_CHK
A_DOWN: DEC H           ; Target is lower, slew down
A_CHK:
        LD BC, $FDFE    ; Is A still pressed?
        IN A, (C)
        BIT 0, A
        JR Z, GATE_A
        JP MAIN_LOOP

; =========================================================
; [O] CHARGING CAPACITOR (DENSITY BUILDER)
; Starts as pure silence. The longer you hold it, the more
; clicks appear, eventually saturating into pure white noise.
; PORT 95 MODULATION: Controls the sample rate / harshness
; of the resulting noise.
; =========================================================
INIT_O:
        LD HL, 0        ; H = Density Threshold (0 = silent, 255 = wall of noise)
GATE_O:
        LD A, R         ; Get pseudo-random byte
        CP H            ; Is random value below our threshold?
        JR C, O_NOISE
        XOR A           ; Below threshold = Silence
        JR O_OUT
O_NOISE:
        LD A, 16        ; Above threshold = Click
O_OUT:  OUT (254), A

        IN A, (95)      ; Port 95 sets grain width / pitch
        SRL A           ; Halve it so it doesn't get too slow
        INC A
        LD B, A
O_DEL:  DJNZ O_DEL

        INC L
        JR NZ, O_CHK
        INC H           ; Increase density threshold
        JR NZ, O_CHK
        DEC H           ; Saturate at max density, don't wrap to silence
O_CHK:
        LD BC, $DFFE    ; Is O still pressed?
        IN A, (C)
        BIT 1, A
        JR Z, GATE_O
        JP MAIN_LOOP

; =========================================================
; [P] LFO PULSE WIDTH SWEEP
; Generates a square wave where the width of the pulse 
; shrinks and expands back and forth over time.
; PORT 95 MODULATION: Sets the base pitch/frequency of the 
; oscillator while the PWM sweeps automatically.
; =========================================================
INIT_P:
        LD H, 1         ; H = PWM width 
        LD D, 1         ; D = Direction (1 = growing, -1 = shrinking)
        LD L, 0
GATE_P:
        LD A, 16
        OUT (254), A
        LD B, H         ; High phase duration = H
P_D1:   DJNZ P_D1
        
        XOR A
        OUT (254), A
        LD A, 255
        SUB H           ; Low phase duration = 255 - H
        LD B, A
P_D2:   DJNZ P_D2

        IN A, (95)      ; Port 95 adds a global pitch delay
        LD B, A
P_D3:   DJNZ P_D3

        INC L
        JR NZ, P_CHK
        LD A, H
        ADD A, D        ; Apply direction to width
        LD H, A
        CP 254
        JR Z, P_FLIP
        CP 1
        JR NZ, P_CHK
P_FLIP: LD A, D
        NEG             ; Z80 trick: Negate D (1 becomes -1, -1 becomes 1)
        LD D, A
P_CHK:
        LD BC, $DFFE    ; Is P still pressed?
        IN A, (C)
        BIT 0, A
        JR Z, GATE_P
        JP MAIN_LOOP

; =========================================================
; [SPACE] SEQUENCER PLAY BUTTON
; Holding space steps through the ZX Spectrum's ROM data 
; interpreting it as melodic pitches (an arpeggiator).
; PORT 95 MODULATION: Sets the sequence playback speed!
; =========================================================
INIT_SPACE:
        LD HL, $0000    ; Start at ROM 0
        LD D, 0         ; Sequence timer
GATE_SPACE:
        LD A, (HL)      ; Read byte from ROM
        AND $3F         ; Restrict to a musical pitch range
        ADD A, 10
        LD B, A
        
        LD A, 16
        OUT (254), A
S_D1:   DJNZ S_D1
        
        LD A, (HL)
        AND $3F
        ADD A, 10
        LD B, A
        
        XOR A
        OUT (254), A
S_D2:   DJNZ S_D2

        IN A, (95)      ; Port 95 sets speed
        ADD A, 5        ; Minimum speed limit
        ADD A, D        ; Add to our internal timer
        LD D, A
        JR NC, S_CHK    ; If timer didn't overflow, keep playing same note
        INC HL          ; Timer overflowed! Advance to next note in ROM
S_CHK:
        LD BC, $7FFE    ; Is SPACE still pressed?
        IN A, (C)
        BIT 0, A
        JR Z, GATE_SPACE
        JP MAIN_LOOP

; =========================================================
; [ENTER] BITCRUSH TEXTURE MORPH
; A fixed pitch tone where Port 95 modulates the XOR 
; texture. The longer you hold it, the more "crushed" and 
; chaotic the bitwise math becomes.
; =========================================================
INIT_ENTER:
        LD HL, 0        ; H = Bitcrush accumulator
GATE_ENTER:
        LD A, 16
        OUT (254), A
        
        IN A, (95)
        XOR H           ; XOR CV against our time accumulator
        LD B, A
E_D1:   DJNZ E_D1

        XOR A
        OUT (254), A
        
        IN A, (95)
        XOR H
        LD B, A
E_D2:   DJNZ E_D2
        
        INC L           
        JR NZ, E_CHK
        INC H           ; Evolve the bitcrush math slowly
E_CHK:
        LD BC, $BFFE    ; Is ENTER still pressed?
        IN A, (C)
        BIT 0, A
        JR Z, GATE_ENTER
        JP MAIN_LOOP
        
        end 32768