; do *NOT* use register4 to register7 during time slicing
inpLutAddr          EQU     register0
inpKeyBak           EQU     register0
inpVarsAddr         EQU     register1
inpStrsAddr         EQU     register2
inpTypesAddr        EQU     register4
inpTextAddr         EQU     register5
inpTextOfs          EQU     register6
inpTypeData         EQU     register7
inpTextEnd          EQU     register8
printXYBak          EQU     register11
cursXYBak           EQU     register12
cursFlash           EQU     register13
cursorChr           EQU     register14
cursXYOfs           EQU     register15

cursorDelay         EQU     30

    
%SUB                input
                    ; inputs numerics and text into vars
input               PUSH
                    STW     inpLutAddr
                    DEEK
                    STW     inpVarsAddr         ; vars LUT address
                    LDW     inpLutAddr
                    ADDI    2
                    DEEK
                    STW     inpStrsAddr         ; strings LUT address
                    LDW     inpLutAddr
                    ADDI    4
                    DEEK
                    STW     inpTypesAddr        ; types LUT address
                    
                    LD      giga_serialRaw
                    ST      serialRawPrev       ; initialise previous keystroke
                    LD      giga_frameCount
                    ADDI    cursorDelay
                    STW     cursFlash           ; delay for cursor flash
                    LDI     127
                    STW     cursorChr           ; cursor char
                    CALLI   inputExt1           ; doesn't return to here
%ENDS

%SUB                inputExt1
                    ; input extended 1
inputExt1           LDW     inpTypesAddr
                    DEEK
                    BEQ     inputE1_exit        ; exit on LUT delimiter
                    STW     inpTypeData         ; high byte is max string length, 8th and 7th bits of low byte are newlines, last 6 bits of low byte is type
                    ANDI    0x40                
                    BEQ     inputE1_print       ; check for prefix newline
                    CALLI   inputNewline
                    
inputE1_print       LDW     inpStrsAddr
                    DEEK
                    CALLI   printText           ; print strings LUT
                    LD      inpTypeData
                    ANDI    0x80
                    BEQ     inputE1_skip        ; check for suffix newline
                    CALLI   inputNewline
                    
inputE1_skip        LDWI    textWorkArea + 1
                    STW     inpTextAddr         ; text work area, treated as a string so skip length
                    LDI     0
                    STW     inpTextOfs          ; print text offset
                    LDWI    textWorkArea
                    STW     inpTextEnd          ; print text end
                    LD      inpTypeData + 1
                    ADDW    inpTextEnd
                    STW     inpTextEnd          ; text max = textWorkArea + (highByte(inpTypeData) >> 8)
                    
                    LDW     cursorXY
                    STW     cursXYBak
                    STW     printXYBak
                    CALLI   inputExt2           ; doesn't return to here
                    
inputE1_exit        LDI     ENABLE_SCROLL_BIT
                    ORW     miscFlags
                    STW     miscFlags           ; enable text scrolling
                    POP
                    RET
%ENDS

%SUB                inputExt2
                    ; input extended 2
inputExt2           CALLI   inputCursor
                    CALLI   inputKeys
                    BEQ     inputExt2           ; loop until return key pressed

                    INC     inpVarsAddr
                    INC     inpVarsAddr
                    INC     inpStrsAddr
                    INC     inpStrsAddr
                    INC     inpTypesAddr
                    INC     inpTypesAddr
                    CALLI   inputExt1           ; doesn't return to here
%ENDS

%SUB                inputCursor
                    ; flashes cursor
inputCursor         LD      giga_frameCount
                    SUBW    cursFlash
                    BEQ     inputC_toggle
                    RET
                    
inputC_toggle       PUSH
                    LDW     cursXYBak
                    STW     cursorXY            ; restore cursor position before the printChr
                    LD      giga_frameCount
                    ADDI    cursorDelay
                    ST      cursFlash           ; delay for cursor flash
                    LD      cursorChr
                    CALLI   printChr            ; display cursor
                    LD      cursorChr
                    XORI    0xDF
                    ST      cursorChr           ; toggle between 127 and 32 for cursor char
                    POP
                    RET
%ENDS

%SUB                inputKeys
                    ; saves key press into string work area buffer
inputKeys           PUSH
                    LD      giga_serialRaw
                    STW     inpKeyBak           ; save keystroke
                    LD      serialRawPrev
                    SUBW    inpKeyBak
                    BEQ     inputK_exit         ; if keystroke hasn't changed exit
                    LD      inpKeyBak
                    ST      serialRawPrev       ; save as previous keystroke
                    SUBI    127
                    BGT     inputK_exit
                    BNE     inputK_ret
                    CALLI   inputDelete         ; delete key, doesn't return to here
                    
inputK_ret          LD      inpKeyBak
                    SUBI    10
                    BNE     inputK_char
                    CALLI   inputReturn         ; return key, doesn't return to here
                    
inputK_char         LDW     inpTextEnd
                    SUBW    inpTextAddr
                    BEQ     inputK_exit         ; text string bounds, (check after delete and return keys)
                    LD      inpKeyBak
                    SUBI    32
                    BLT     inputK_exit
                    LD      inpKeyBak
                    POKE    inpTextAddr         ; set char
                    INC     inpTextAddr
                    LDI     0
                    POKE    inpTextAddr         ; set new end of text string
                    LD      cursXYBak
                    SUBI    giga_xres - 11
                    BLT     inputK_advance      ; cursor max bounds
                    INC     inpTextOfs
                    LDI     0
                    BRA     inputK_print
                    
inputK_advance      LDI     6
                    
inputK_print        STW     cursXYOfs           ; advance cursor
                    CALLI   inputPrint          ; doesn't return to here
                    
inputK_exit         LDI     0                   ; keep looping on current var
                    POP
                    RET                    
%ENDS

%SUB                inputIntVar
inputIntVar         PUSH
                    LDWI    textWorkArea + 1
                    STW     intSrcAddr          ; src str address, (skip length)
                    LDW     inpVarsAddr
                    DEEK
                    STW     register12          ; dst int address
                    CALLI   integerStr
                    DOKE    register12          ; convert string to integer
                    POP
                    RET
%ENDS

%SUB                inputStrVar
inputStrVar         LDWI    textWorkArea
                    STW     register11          ; src str address
                    LDW     inpVarsAddr
                    DEEK
                    STW     register12          ; dst var address

inputS_copy         LDW     register11
                    PEEK
                    POKE    register12
                    INC     register11
                    INC     register12
                    BNE     inputS_copy         ; copy char until terminating char
                    RET
%ENDS                    

%SUB                inputReturn
inputReturn         LDI     32
                    STW     cursorChr
                    LDWI    inputC_toggle
                    CALL    giga_vAC            ; erase cursor
                    
                    LDWI    textWorkArea
                    STW     register0
                    LDW     inpTextAddr
                    SUBW    register0
                    SUBI    1
                    POKE    register0           ; text length
                    ADDW    register0
                    ADDI    1
                    STW     register0
                    LDI     0
                    POKE    register0           ; text delimiter
                    
                    LD      inpTypeData         ; check var tye
                    ANDI    0x3F                ; var type is bottom 6 bits
                    SUBI    3                   ; var is string or integer?
                    BNE     inputR_int
                    CALLI   inputStrVar         ; copy string
                    BRA     inputR_exit
                    
inputR_int          LDWI    inputIntVar
                    CALL    giga_vAC            ; convert numeric

inputR_exit         LDI     1                   ; return key pressed, next var
                    POP
                    RET
%ENDS

%SUB                inputDelete
inputDelete         LD      inpTextOfs
                    BEQ     inputD_bounds
                    SUBI    1
                    STW     inpTextOfs          ; decrement print text offset
                    LDI     0
                    STW     cursXYOfs           ; stationary cursor
                    LDI     0
                    POKE    inpTextAddr         ; delimiter
                    LDW     inpTextAddr
                    SUBI    1
                    STW     inpTextAddr         ; decrement text pointer
                    LDI     32                  
                    POKE    inpTextAddr         ; delete char
                    BRA     inputD_print

inputD_bounds       LDW     printXYBak
                    SUBW    cursXYBak
                    BGE     inputD_exit         ; cursor min bounds
                    LDWI    -6
                    STW     cursXYOfs           ; retreat cursor
                    LDI     32                  
                    POKE    inpTextAddr         ; delete cursor
                    INC     inpTextAddr
                    LDI     0
                    POKE    inpTextAddr         ; delimiter
                    LDW     inpTextAddr
                    SUBI    2
                    STW     inpTextAddr         ; decrement text pointer
                    LDI     32                  
                    POKE    inpTextAddr         ; delete char
                    
inputD_print        CALLI   inputPrint          ; doesn't return to here
                    
inputD_exit         LDI     0                   ; keep looping on current var
                    POP
                    RET
%ENDS

%SUB                inputPrint
inputPrint          CALLI   inputCursor         ; call cursor flash frequently
                    LDW     printXYBak
                    STW     cursorXY            ; restore cursor position before the printText
                    LDWI    textWorkArea
                    ADDW    inpTextOfs
                    CALLI   printText
                    LDW     cursXYBak           ; new cursor position
                    ADDW    cursXYOfs
                    STW     cursXYBak
                    LDI     0                   ; keep looping on current var
                    POP
                    RET
%ENDS                    

%SUB                inputNewline
inputNewline        PUSH
                    LDI     ENABLE_SCROLL_BIT
                    ORW     miscFlags
                    STW     miscFlags           ; enable text scrolling
                    CALLI   newLineScroll       ; new line
                    LDWI    ENABLE_SCROLL_MSK
                    ANDW    miscFlags
                    STW     miscFlags           ; disable text scrolling
                    POP
                    RET
%ENDS

