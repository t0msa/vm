#include "vm.h"

#include <Windows.h>

#include <stdio.h>
#include <stdlib.h>

#pragma warning( disable : 6385 5105 )

#pragma region inst_impl

void vm_push(vm_t* vm) {
	vm->m_stack[vm->m_vsp++] = vm->m_ram[vm->m_vip + 1];
	vm->m_vip += 2;
}

void vm_pop(vm_t* vm) {
	const u8 dst = (vm->m_ram[vm->m_vip++] & 0xF) % 2;
	vm->m_gprs[dst] = vm->m_stack[--vm->m_vip];
}

void vm_cmp(vm_t* vm) {
	const u8 src_reg = vm->m_ram[vm->m_vip + 1] & 0xF;

	vm->m_zero_flag = vm->m_gprs[src_reg] == vm->m_ram[vm->m_vip + 2] ? true : false;
	vm->m_carry_flag = vm->m_gprs[src_reg] < vm->m_ram[vm->m_vip + 2] ? true : false;

	vm->m_vip += 3;
}

void vm_jmp(vm_t* vm) {
	vm->m_vip = vm->m_ram[vm->m_vip + 1];
}

void vm_je(vm_t* vm) {
	if (!vm->m_zero_flag) {
		vm->m_vip += 2;
		return;
	}

	vm_jmp(vm);
}

void vm_jne(vm_t* vm) {
	if (vm->m_zero_flag) {
		vm->m_vip += 2;
		return;
	}

	vm_jmp(vm);
}

void vm_jl(vm_t* vm) {
	if (!vm->m_carry_flag) {
		vm->m_vip += 2;
		return;
	}

	vm_jmp(vm);
}

void vm_jg(vm_t* vm) {
	if (vm->m_carry_flag) {
		vm->m_vip += 2;
		return;
	}

	vm_jmp(vm);
}

void vm_mov(vm_t* vm) {
	const u8 dst_reg = vm->m_ram[vm->m_vip + 1 & 0xF] % 2;
	vm->m_gprs[dst_reg] = vm->m_ram[vm->m_vip + 2];
	vm->m_vip += 3;
}

void vm_mov_reg_addr(vm_t* vm) {
	const u8 dst_reg = vm->m_ram[vm->m_vip + 1 & 0xF] % 2;
	vm->m_gprs[dst_reg] = *(u8*)(vm->m_ram + vm->m_ram[vm->m_vip + 2]);
	vm->m_vip += 3;
}

void vm_mov_addr_imm(vm_t* vm) {
	*(u8*)(vm->m_ram + vm->m_ram[vm->m_vip + 1]) = vm->m_ram[vm->m_vip + 2];
	vm->m_vip += 3;
}

void vm_print(vm_t* vm) {
	const char* str = (const char*)(vm->m_ram + vm->m_ram[vm->m_vip + 1]);
	puts(str);
	vm->m_vip += 2;
}

void vm_halt(vm_t* vm) {
	vm->m_status = vm->m_stack[vm->m_vsp - 1];
}

void vm_invalid_instruction(vm_t* vm) {
	vm->m_status = VM_INVALID_INSTRUCTION;
	printf_s("Invalid instruction met at 0x%02X\n", vm->m_vip);
}

#pragma endregion

void(*instructions[0x100])(vm_t*);

void vm_initialize_instructions() {
	for (int i = 0; i < sizeof instructions / sizeof * instructions; ++i)
		instructions[i] = vm_invalid_instruction;

	instructions[PUSH] = vm_push;
	instructions[CMP] = vm_cmp;
	instructions[JMP] = vm_jmp;
	instructions[JE] = vm_je;
	instructions[JNE] = vm_jne;
	instructions[JL] = vm_jl;
	instructions[JG] = vm_jg;
	instructions[MOV] = vm_mov;
	instructions[MOV_ADDR_IMM] = vm_mov_addr_imm;
	instructions[MOV_REG_ADDR] = vm_mov_reg_addr;
	instructions[PRINT] = vm_print;
	instructions[HALT] = vm_halt;
}

const char* reg_tbl[] = { "V0", "V1" };
const char* jmp_tbl[] = { "JMP", "JE", "JNE", "JL", "JG" };

void disasm_all(u8* source, const size_t size) {
	int i = 0;
	while (i != size) {
		if (source[i] == PLACEHOLDER)
			break;

		printf_s("0x%02X  ", i);
		switch (source[i]) {
		case PUSH:  printf_s("PUSH 0x%02X\n", source[i + 1]); i += 2; break;
		case CMP:	printf_s("CMP %s, 0x%02X\n", reg_tbl[source[i + 1] & 0xF], source[i + 2]); i += 3; break;
		case JMP:
		case JE:
		case JNE:
		case JL:
		case JG:	printf_s("%s 0x%02X\n", jmp_tbl[source[i] & 0xF], source[i + 1]); i += 2; break;
		case MOV:
		case MOV_ADDR_IMM:
		case MOV_REG_ADDR:
		{
			switch (source[i] & 0xF) {
			case REG_IMM: printf_s("MOV %s, 0x%02X\n", reg_tbl[source[i + 1]], source[i + 2]); break;
			case ADDR_IMM: printf_s("MOV [0x%02X], 0x%02X\n", source[i + 1], source[i + 2]); break;
			case REG_ADDR:	printf_s("MOV %s, [0x%02X]\n", reg_tbl[source[i + 1]], source[i + 2]); break;
			}

			i += 3;
			break;
		}
		case PRINT:	printf_s("PRINT 0x%02X\n", source[i + 1]); i += 2; break;
		case HALT:	printf_s("HALT\n"); i++; break;
		default: printf_s("Invalid instruction \"0x%X\"\n", source[i]); return;
		}
	}
}

u8* read_file(const char* file, size_t* size) {
	FILE* file_in = fopen(file, "rb");

	if (!file_in)
		return NULL;

	fseek(file_in, 0, SEEK_END);

	*size = ftell(file_in);

	if (*size == (size_t)(-1)) {
		fclose(file_in);
		return NULL;
	}

	fseek(file_in, 0, SEEK_SET);

	u8* contents = malloc(*size);

	if (!contents) {
		fclose(file_in);
		return NULL;
	}

	fread(contents, 1, *size, file_in);
	fclose(file_in);
	return contents;
}

int main(int argc, char** argv) {

	if (argc < 2) {
		printf_s("Usage: <file in>\n");
		return 0;
	}

	vm_initialize_instructions();

	size_t prog_size;

	const char* vm_prog = argv[1];

	u8* contents = read_file(vm_prog, &prog_size);

	if (!contents) {
		printf_s("VM Binary could not be read, make sure you didn't move it from the exe's directory.\n");
		free(contents);
		Sleep(1500);
		return 0;
	}
	if (prog_size > 256) {
		printf_s("Input program is too big (max size is 256 bytes.\n");
		return 0;
	}

	vm_t vm;

	SecureZeroMemory(&vm, sizeof(vm_t));

	printf_s("Booting VM with %s...\n", vm_prog);

	vm.m_status = VM_RUNNING;

	memcpy(vm.m_ram, contents, prog_size);

	disasm_all(vm.m_ram, prog_size);

	do {
		instructions[vm.m_ram[vm.m_vip]](&vm);
	} while (vm.m_status == VM_RUNNING);

	printf_s("\nVM Returned with \"%s\"\n", vm_statuses[vm.m_status]);

	free(contents);

	system("pause");
}
