ORG 32768

START:
        DI              ; Disable system interrupts
        LD HL, $0001    ; H will store our Shift Register (Turing Machine) state

DSP_LOOP:
        ; ---------------------------------------------------------
        ; 1. HARD SYNC INPUTS (Pulse 1 & 2)
        ; ---------------------------------------------------------
        IN A, ($23)     ; Read Pulse 1
        OR A
        JR Z, NO_SYNC1
        LD D, 0         ; Pulse 1 HIGH = Reset Phase Accumulator A
NO_SYNC1:

        IN A, ($27)     ; Read Pulse 2
        OR A
        JR Z, NO_SYNC2
        LD E, 0         ; Pulse 2 HIGH = Reset Phase Accumulator B
NO_SYNC2:

        ; ---------------------------------------------------------
        ; 2. ADVANCE PHASE A & CLOCK THE SHIFT REGISTER
        ; ---------------------------------------------------------
        IN A, ($2B)     ; CV In 1 (Frequency of Phase A)
        ADD A, D
        LD D, A         ; D = Phase A
        
        ; If Phase A didn't overflow (wrap past 255), we skip clocking the shift register.
        ; This turns Phase A into a variable clock divider!
        JR NC, ADVANCE_B 

        ; --- TURING MACHINE CLOCK TICK ---
        ; We compare Audio 1 and Audio 2 to generate a 1-bit boolean state.
        ; This bit gets fed into the shift register.
        IN A, ($33)     ; Audio In 1
        LD B, A
        IN A, ($63)     ; Audio In 2
        CP B            
        ; If Audio 2 > Audio 1, the Carry Flag is set.
        
        LD A, H         ; Load current shift register state
        RRA             ; Rotate Right. The Carry Flag (our boolean state) is pushed into bit 7!
        LD H, A         ; Save the new state

ADVANCE_B:
        ; ---------------------------------------------------------
        ; 3. ADVANCE PHASE B
        ; ---------------------------------------------------------
        IN A, ($2F)     ; CV In 2 (Frequency of Phase B)
        ADD A, E
        LD E, A         ; E = Phase B

        ; ---------------------------------------------------------
        ; 4. ALGORITHM SELECT (Switch)
        ; ---------------------------------------------------------
        IN A, ($67)     ; Switch
        OR A
        JR NZ, ALGO_STEPPED

ALGO_CONTINUOUS:
        ; --- MODE 0: Audio-Rate Complex Morphing ---
        IN A, ($33)     ; Audio In 1
        XOR D           ; Ring-modulate Audio 1 with Phase A
        ADD A, E        ; Add Phase B (creates a folded sawtooth shape)
        JR FINAL_MIX

ALGO_STEPPED:
        ; --- MODE 1: Stepped Shift Register CV ---
        LD A, H         ; Get the current Turing Machine state
        XOR E           ; XOR with Phase B to add high-frequency micro-textures 
                        ; (Turn CV 2 to zero if you want pure stepped CV!)

FINAL_MIX:
        ; ---------------------------------------------------------
        ; 5. MASTER OFFSET & MEMORY MAPPED CV OUT
        ; ---------------------------------------------------------
        LD B, A         ; Store generated core value
        IN A, ($5F)     ; Read Knob Y
        ADD A, B        ; Add Knob Y as a DC Offset / Master Base Voltage
        LD C, A         ; C NOW HOLDS OUR FINAL 8-BIT OUTPUT

        LD (16384), A   ; Write directly to memory mapped CV out! ($4000)
                        ; (Watch the top left pixels of the screen glitch out)

        ; ---------------------------------------------------------
        ; 6. BORDER & AUDIO HARDWARE OUT ($FE)
        ; ---------------------------------------------------------
        ; The border uses the lowest 3 bits. This turns the border 
        ; into a 3-bit stepped DAC (0 to 7) based on our main CV out.
        LD A, C
        AND $07         ; Mask out bits 0,1,2 (Border color)
        LD B, A         ; Store border CV safely

        ; Audio Output: We'll just map the Most Significant Bit (bit 7)
        ; of our CV signal to generate a square wave for the audio jack.
        BIT 7, C
        JR Z, NO_AUDIO
        LD A, B
        OR 16           ; Add Bit 4 (Speaker HIGH)
        JR FIRE_ULA

NO_AUDIO:
        LD A, B         ; Audio is LOW, just keep the border CV

FIRE_ULA:
        OUT (254), A    ; Send Border CV and Audio out to the world!

        JR DSP_LOOP     ; Run forever

        end 32768