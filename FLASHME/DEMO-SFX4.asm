ORG 32768

START:
        DI              ; Disable system interrupts for clean DSP timing
        LD HL, 0        ; Initialize our internal phase accumulator (Time)

DSP_LOOP:
        ; ---------------------------------------------------------
        ; 1. HARD SYNC (Pulse In 1: 0x23)
        ; ---------------------------------------------------------
        IN A, ($23)     ; Read Pulse 1 (0 or 255)
        OR A            ; Check if zero
        JR Z, NO_SYNC
        LD HL, 0        ; If 255, Hard Sync! Reset oscillator phase.
NO_SYNC:

        ; ---------------------------------------------------------
        ; 2. ADVANCE OSCILLATOR PHASE (CV In 1: 0x2B)
        ; ---------------------------------------------------------
        IN A, ($2B)     ; Read Pitch CV (0-255)
        ADD A, L        ; Add to low byte of phase
        LD L, A
        JR NC, NO_INC_H
        INC H           ; Carry over to high byte (H is our base wave)
NO_INC_H:

        ; ---------------------------------------------------------
        ; 3. ROUTING / ENGINE SELECT (Switch: 0x67)
        ; ---------------------------------------------------------
        IN A, ($67)     ; Read Switch (0 or 255)
        OR A
        JR NZ, AUDIO_MODE ; If 255, jump to external audio mangler

        ; ====== SYNTH MODE (Internal Oscillator) ======
SYNTH_MODE:
        LD C, H         ; Start with our internal saw wave (H)
        
        IN A, ($5F)     ; Read Knob Y (0-255) for FM Phase Modulation
        ADD A, C        ; Wrap the phase based on knob position
        LD C, A         ; C now holds the FM-modulated wave
        
        JR MIX_BUS      ; Jump to output stage

        ; ====== AUDIO THRU MODE (External Processing) ======
AUDIO_MODE:
        IN A, ($33)     ; Read raw Audio In 1
        LD C, A         ; Store in C
        
        IN A, ($2F)     ; Read CV In 2 (0-255)
        AND C           ; Bitwise AND the CV against the Audio! 
                        ; (Creates brutal gating and bitcrushing)
        LD C, A         ; C now holds the crushed audio

        ; ---------------------------------------------------------
        ; 4. WAVESHAPING / PWM (Audio In 2: 0x63)
        ; ---------------------------------------------------------
MIX_BUS:
        ; C currently holds our 8-bit signal (either internal or external)
        ; We must convert this 8-bit signal into a 1-bit output for the Spectrum.
        
        IN A, ($63)     ; Read Audio In 2 / CV (0-255)
        CP C            ; Compare our 8-bit wave (C) against the CV threshold (A)
        
        ; If Wave < Threshold, Carry Flag is set.
        JR C, OUT_HIGH
        
OUT_LOW:
        LD D, 0         ; Output state = LOW
        JR FINAL_OUT
        
OUT_HIGH:
        LD D, 16        ; Output state = HIGH (Spectrum Speaker is bit 4)

        ; ---------------------------------------------------------
        ; 5. RING MODULATION & OUTPUT (Pulse In 2: 0x27)
        ; ---------------------------------------------------------
FINAL_OUT:
        IN A, ($27)     ; Read Pulse 2 (0 or 255)
        AND 16          ; Mask bit 4 (will be 16 if Pulse 2 is HIGH, 0 if LOW)
        XOR D           ; XOR against our current output state (D)
                        ; If Pulse 2 is high, the 1-bit wave is inverted!
                        
        OUT (254), A    ; Send final 1-bit state to ULA ($FE)

        ; ---------------------------------------------------------
        ; 6. LOOP
        ; ---------------------------------------------------------
        ; No artificial delay is used. The time it takes the Z80 to
        ; execute this loop natively defines the sample rate.
        JR DSP_LOOP

        end 32768