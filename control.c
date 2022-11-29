#include <stdio.h>
#include <stdlib.h>
#include "bits.h"
#include "control.h"
#include "instruction.h"
#include "x16.h"
#include "trap.h"

// Update condition code based on result
void update_cond(x16_t* machine, reg_t reg) {
    uint16_t result = x16_reg(machine, reg);
    if (result == 0) {
        x16_set(machine, R_COND, FL_ZRO);
    } else if (is_negative(result)) {
        x16_set(machine, R_COND, FL_NEG);
    } else {
        x16_set(machine, R_COND, FL_POS);
    }
}

// Execute a single instruction in the given X16 machine. Update
// memory and registers as required. PC is advanced as appropriate.
// Return 0 on success, or -1 if an error or HALT is encountered.
int execute_instruction(x16_t* machine) {
    // Fetch the instruction and advance the program counter
    uint16_t pc = x16_pc(machine);
    uint16_t instruction = x16_memread(machine, pc);
    x16_set(machine, R_PC, pc + 1);

    // Variables
    uint16_t immediate = getimmediate(instruction);
    uint16_t DR = getbits(instruction, 9, 3);
    uint16_t SR1 = getbits(instruction, 6, 3);
    uint16_t SR1_value = x16_reg(machine, SR1);
    uint16_t SR2 = getbits(instruction, 0, 3);
    uint16_t SR2_value = x16_reg(machine, SR2);
    uint16_t imm5 = getbits(instruction, 0, 5);
    uint16_t nzp = getbits(instruction, 9, 3);
    uint16_t PCoffset9 = getbits(instruction, 0, 9);
    uint16_t BaseR = getbits(instruction, 6, 3);
    uint16_t PCoffset11 = getbits(instruction, 0, 11);
    uint16_t offset6 = getbits(instruction, 0, 6);
    uint16_t SR = getbits(instruction, 9, 3);

    // Decode the instruction and execute it
    uint16_t opcode = getopcode(instruction);
    switch (opcode) {
        case OP_ADD:
            if (immediate == 0) {
                // DR = SR1 + SR2
                x16_set(machine, DR, SR1_value + SR2_value);
            } else {
                // DR = SR1 + SEXT(imm5)
                x16_set(machine, DR, SR1_value + sign_extend(imm5, 5));
            }
            update_cond(machine, DR);
            break;

        case OP_AND:
            if (immediate == 0) {
                // DR = SR1 + SR2
                x16_set(machine, DR, SR1_value & SR2_value);
            } else {
                // DR = SR1 + SEXT(imm5)
                x16_set(machine, DR, SR1_value & sign_extend(imm5, 5));
            }
            update_cond(machine, DR);
            break;

        case OP_NOT:
            x16_set(machine, DR, ~SR1_value);
            update_cond(machine, DR);
            break;

        case OP_BR:
            // if ((n AND N) OR (z AND Z) OR (p AND P)) {
            //     PC = PC++ + SEXT(PCoffset9);
            // }
            if (nzp & x16_reg(machine, R_COND)) {
                x16_set(machine, R_PC, (pc + 1 + sign_extend(PCoffset9, 9)));
            }
            break;

        case OP_JMP:
            x16_set(machine, R_PC, x16_reg(machine, BaseR));
            break;

        case OP_JSR:
            x16_set(machine, R_R7, pc + 1);

            if (getbit(instruction, 11) == 0) {
                x16_set(machine, R_PC, x16_reg(machine, BaseR));
            } else {
                x16_set(machine, R_PC, (pc + 1 + sign_extend(PCoffset11, 11)));
            }
            break;

        case OP_LD:
            x16_set(machine, DR, x16_memread(machine,\
             (pc + 1 + sign_extend(PCoffset9, 9))));
            update_cond(machine, DR);
            break;

        case OP_LDI:
            x16_set(machine, DR, x16_memread(machine,\
             x16_memread(machine, (pc + 1 + sign_extend(PCoffset9, 9)))));
            update_cond(machine, DR);
            break;

        case OP_LDR:
            x16_set(machine, DR, x16_memread(machine,\
             (x16_reg(machine, BaseR) + sign_extend(offset6, 6))));
            update_cond(machine, DR);
            break;

        case OP_LEA:
            x16_set(machine, DR, (pc + 1 + sign_extend(PCoffset9, 9)));
            update_cond(machine, DR);
            break;

        case OP_ST:
            x16_memwrite(machine,\
             (pc + 1 + sign_extend(PCoffset9, 9)), x16_reg(machine, SR));
            break;

        case OP_STI:
            x16_memwrite(machine, x16_memread(machine,\
             (pc + 1 + sign_extend(PCoffset9, 9))), x16_reg(machine, SR));
            break;

        case OP_STR:
            x16_memwrite(machine, (x16_reg(machine, BaseR) + sign_extend(offset6, 6)), x16_reg(machine, SR));
            break;

        case OP_TRAP:
            // Execute the trap
            return trap(machine, instruction);

        case OP_RES:
        case OP_RTI:
        default:
            // Bad codes, never used
            abort();
    }

    return 0;
}
