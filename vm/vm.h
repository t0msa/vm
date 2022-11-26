#pragma once

#include <stdbool.h>

typedef char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;

#define PLACEHOLDER 0xCC

const char* vm_statuses[] = { "VM_RUNNING", "VM_SUCCESS", "VM_FAILURE", "VM_INVALID_INSTRUCTION" };

enum e_registers {
	V0,
	V1
};

typedef enum {
	PUSH = 0x10,
	PUSH_REG = 0x11,
	POP = 0x20,
	CMP = 0x30,
	JMP = 0x40,
	JE = 0x41,
	JNE = 0x42,
	JL = 0x43,
	JG = 0x44,
	MOV = 0x50,
	MOV_ADDR_IMM = 0x51,
	MOV_REG_ADDR = 0x52,
	PRINT = 0x60,
	LEA = 0x70,
	HALT = 0xFF
}e_opcode;

typedef enum {
	REG_IMM,
	ADDR_IMM,
	REG_ADDR
}e_mov_mode;

typedef enum {
	VM_RUNNING,
	VM_SUCCESS,
	VM_FAILURE,
	VM_INVALID_INSTRUCTION
}e_vm_status;

typedef struct {
	bool m_zero_flag;
	bool m_carry_flag;
	u8 m_vsp;
	u8 m_vip;

	union {
		u8 m_gprs[2];
		struct {
			u8 m_v0;
			u8 m_v1;
		};
	};

	u8 m_stack[0x100];
	u8 m_ram[0x100];
	e_vm_status m_status;
}vm_t;
