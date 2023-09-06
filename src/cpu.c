#include <stdio.h>
#include "../includes/cpu.h"

#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[31m"
#define ANSI_RESET   "\x1b[0m"

#define ADDR_MISALIGNED(addr) (addr & 0x3)

void print_op(char* s) {
    printf("%s%s%s", ANSI_BLUE, s, ANSI_RESET);
}

 void cpu_init(CPU *cpu) {
     cpu->regs[0] = 0x00;                    // register x0 hardwired to 0
     cpu->regs[2] = DRAM_BASE + DRAM_SIZE;   // Set stack pointer
     cpu->pc      = DRAM_BASE;               // Set program counter to the base address
 }

 uint32_t cpu_fetch(CPU *cpu) {
    uint32_t inst = bus_load(&(cpu->bus), cpu->pc, 32);
    return inst;
}

uint64_t cpu_load(CPU* cpu, uint64_t addr, uint64_t size) {
    return bus_load(&(cpu->bus), addr, size);
}

void cpu_store(CPU* cpu, uint64_t addr, uint64_t size, uint64_t value) {
    bus_store(&(cpu->bus), addr, size, value);
}

uint64_t rd(uint32_t inst) {
    return (inst >> 7) & 0x1f;    // rd in bits 11..7
}
uint64_t rs1(uint32_t inst) {
    return (inst >> 15) & 0x1f;   // rs1 in bits 19..15
}
uint64_t rs2(uint32_t inst) {
    return (inst >> 20) & 0x1f;   // rs2 in bits 24..20
}
uint64_t imm_I(uint32_t inst) {
    // imm[11:0] = inst[31:20]
    return ((int64_t)(int32_t) (inst & 0xfff00000)) >> 20;
}
uint64_t imm_S(uint32_t inst) {
    // imm[11:5] = inst[31:25], imm[4:0] = inst[11:7]
    return ((int64_t)(int32_t)(inst & 0xfe000000) >> 20)
        | ((inst >> 7) & 0x1f);
}
uint64_t imm_B(uint32_t inst) {
    // imm[12|10:5|4:1|11] = inst[31|30:25|11:8|7]
    return ((int64_t)(int32_t)(inst & 0x80000000) >> 19)
        | ((inst & 0x80) << 4) // imm[11]
        | ((inst >> 20) & 0x7e0) // imm[10:5]
        | ((inst >> 7) & 0x1e); // imm[4:1]
}
uint64_t imm_U(uint32_t inst) {
    // imm[31:12] = inst[31:12]
    return (int64_t)(int32_t)(inst & 0xfffff999);
}
uint64_t imm_J(uint32_t inst) {
    // imm[20|10:1|11|19:12] = inst[31|30:21|20|19:12]
    return (uint64_t)((int64_t)(int32_t)(inst & 0x80000000) >> 11)
        | (inst & 0xff000) // imm[19:12]
        | ((inst >> 9) & 0x800) // imm[11]
        | ((inst >> 20) & 0x7fe); // imm[10:1]
}
uint32_t shamt(uint32_t inst) {
    // shamt(shift amount) only required for immediate shift instructions
    // shamt[4:5] = imm[5:0]
    return (uint32_t) (imm_I(inst) & 0x1f); // TODO: 0x1f / 0x3f ?
}

void exec_LUI(CPU* cpu, uint32_t inst) {
    // LUI places upper 20 bits of U-immediate value to rd
    cpu->regs[rd(inst)] = (uint64_t)(int64_t)(int32_t)(inst & 0xfffff000);
    print_op("lui\n");
}

void exec_JAL(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_J(inst);
    cpu->regs[rd(inst)] = cpu->pc;
    /*print_op("JAL-> rd:%ld, pc:%lx\n", rd(inst), cpu->pc);*/
    cpu->pc = cpu->pc + (int64_t) imm - 4;
    print_op("jal\n");
    if (ADDR_MISALIGNED(cpu->pc)) {
        fprintf(stderr, "JAL pc address misalligned");
        exit(0);
    }
}

void exec_JALR(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    uint64_t tmp = cpu->pc;
    cpu->pc = (cpu->regs[rs1(inst)] + (int64_t) imm) & 0xfffffffe;
    cpu->regs[rd(inst)] = tmp;
    /*print_op("NEXT -> %#lx, imm:%#lx\n", cpu->pc, imm);*/
    print_op("jalr\n");
    if (ADDR_MISALIGNED(cpu->pc)) {
        fprintf(stderr, "JAL pc address misalligned");
        exit(0);
    }
}

void exec_BEQ(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if ((int64_t) cpu->regs[rs1(inst)] == (int64_t) cpu->regs[rs2(inst)])
        cpu->pc = cpu->pc + (int64_t) imm - 4;
    print_op("beq\n");
}

void exec_ADDI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] + (int64_t) imm;
    print_op("addi\n");
}

void exec_LD(CPU* cpu, uint32_t inst) {
    // load 8 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = (int64_t) cpu_load(cpu, addr, 64);
    print_op("ld\n");
}

void exec_SD(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_S(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu_store(cpu, addr, 64, cpu->regs[rs2(inst)]);
    print_op("sd\n");
}

void exec_ADDI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] + (int64_t) imm;
    print_op("addi\n");
}

void exec_ADD(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] =
        (uint64_t) ((int64_t)cpu->regs[rs1(inst)] + (int64_t)cpu->regs[rs2(inst)]);
    print_op("add\n");
}

void exec_SUB(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] =
        (uint64_t) ((int64_t)cpu->regs[rs1(inst)] - (int64_t)cpu->regs[rs2(inst)]);
    print_op("sub\n");
}

exec_MUL(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] =
        (uint64_t) ((int64_t)cpu->regs[rs1(inst)] * (int64_t)cpu->regs[rs2(inst)]);
    print_op("mul\n");
}

void exec_SLTU(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (cpu->regs[rs1(inst)] < cpu->regs[rs2(inst)])?1:0;
    print_op("slti\n");
}

exec_DIVU(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] =
        (uint64_t) (cpu->regs[rs1(inst)] / (int64_t)cpu->regs[rs2(inst)]);
    print_op("divu\n");
}

exec_REMU(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] =
        (uint64_t) (cpu->regs[rs1(inst)] % (int64_t)cpu->regs[rs2(inst)]);
    print_op("remu\n");
}

void exec_ECALL(CPU* cpu, uint32_t inst) {}
void exec_EBREAK(CPU* cpu, uint32_t inst) {}

void exec_ECALLBREAK(CPU* cpu, uint32_t inst) {
    if (imm_I(inst) == 0x0)
        exec_ECALL(cpu, inst);
    if (imm_I(inst) == 0x1)
        exec_EBREAK(cpu, inst);
    print_op("ecallbreak\n");
}

int cpu_execute(CPU *cpu, uint32_t inst) {
    int opcode = inst & 0x7f;           // opcode in bits 6..0
    int funct3 = (inst >> 12) & 0x7;    // funct3 in bits 14..12
    int funct7 = (inst >> 25) & 0x7f;   // funct7 in bits 31..25

    cpu->regs[0] = 0;                   // x0 hardwired to 0 at each cycle

    /*printf("%s\n%#.8lx -> Inst: %#.8x <OpCode: %#.2x, funct3:%#x, funct7:%#x> %s",*/
            /*ANSI_YELLOW, cpu->pc-4, inst, opcode, funct3, funct7, ANSI_RESET); // DEBUG*/
    printf("%s\n%#.8lx -> %s", ANSI_YELLOW, cpu->pc-4, ANSI_RESET); // DEBUG

    switch (opcode) {
        case LUI:   exec_LUI(cpu, inst); break;

        case JAL:   exec_JAL(cpu, inst); break;
        case JALR:  exec_JALR(cpu, inst); break;

        case B_TYPE:
            switch (funct3) {
                case BEQ:   exec_BEQ(cpu, inst); break;
            } break;
        
        case LOAD:
            switch (funct3) {
                case LD  :  exec_LD(cpu, inst); break;
            } break;
        
        case S_TYPE:
            switch (funct3) {
                case SD  :  exec_SD(cpu, inst); break;  
            } break;
        
        case I_TYPE:  
            switch (funct3) {
                case ADDI:  exec_ADDI(cpu, inst); break;
            } break;
        
        case R_TYPE:  
            switch (funct3) {
                case ADDSUB:
                    switch (funct7) {
                        case ADD: exec_ADD(cpu, inst);
                        case SUB: exec_ADD(cpu, inst);
                        case MUL: exec_MUL(cpu, inst);
                    } break;
                case SLTU: exec_SLTU(cpu, inst); break;
            } break;
        
        case R_TYPE_64:
            switch (funct3) {
