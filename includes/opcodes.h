#define LUI 0x37

#define JAL 0x6f
#define JALR 0x67

#define B_TYPE 0x63
    #define BEQ 0x0

#define LOAD 0x03
    #define LD 0x3

#define S_TYPE 0x23
    #define SD 0x3

#define I_TYPE 0x13
    #define ADDI 0x0

#define R_TYPE 0x33
    #define ADDSUB 0x0
        #define ADD 0x0
        #define SUB 0x20
        #define MUL 0x1
    #define SLTU 0x3

#define R_TYPE_64 0x3b
    #define SRW 0x5
        #define DIVU 0x1
    #define REMU 0x7

#define CSR 0x73
    #define ECALLBREAK 0x00