#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PROGRAM_LENGTH 65536
#define PC 63
// #define dprintf(...) 
#define dprintf printf

int _argc;
const char** _argv;

uint8_t flag;
uint8_t program[PROGRAM_LENGTH];
uint32_t registers[64];

void debug(const char* msg) {
	printf("DEBUG %s\n", msg);
}

void invalid() {
	printf("Invalid opcode\n");
	exit(1);
}

void write32(void* location, uint32_t value) {
	uint8_t* bytes = (uint8_t*)location;
	bytes[0] = value & 0xff;
	bytes[1] = (value >> 8) & 0xff;
	bytes[2] = (value >> 16) & 0xff;
	bytes[3] = (value >> 24) & 0xff;
}

uint32_t read32(void* location) {
	uint8_t* bytes = (uint8_t*)location;
	return bytes[0] | bytes[1] << 8 | bytes[2] << 16 | bytes[3] << 24;
}

uint32_t readpc8() {
	uint8_t value = program[registers[PC]];
	registers[PC]++;
	return value;
}

uint32_t readpc32() {
	uint32_t value = read32(&program[registers[PC]]);
	registers[PC] += 4;
	return value;
}

int sc(uint32_t syscall,
	uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5) {
	if (syscall == 0) {
		debug("open");
		dprintf("%s %08x\n", (const char*)&program[arg1], arg2);
		int r = open((const char*)&program[arg1], arg2, 0777);
		if (r < 0) {
			perror("open");
		}
		return r;
	} else if (syscall == 1) {
		debug("read");
		return read(arg1, &program[arg2], arg3);
	} else if (syscall == 2) {
		debug("write");
		return write(arg1, &program[arg2], arg3);
	} else if (syscall == 3) {
		debug("lseek");
		int whence = 0;
		if (arg3 == 0) {
			whence = SEEK_SET;
		}
		return (uint32_t)lseek(arg1, (off_t)arg2, whence);
	} else if (syscall == 4) {
		debug("close");
		return close(arg1);
	} else if (syscall == 5) {
		debug("getargv");
		uint32_t needed = _argc * 4; // includes trailing zero
		for (int i = 1; i < _argc; i++) {
			needed += strlen(_argv[i]) + 1;
		}
		printf("%d %08x\n", _argc, needed);
		uint8_t* address_write = &program[arg1];
		uint32_t string_write = arg1 + _argc * 4;
		if (needed <= arg2) {
			for (int i = 1; i < _argc; i++) {
				write32(address_write, string_write);
				strcpy((char*)&program[string_write], _argv[i]);
				string_write += strlen(_argv[i]) + 1;
				address_write += 4;
			}
			write32(address_write, 0);
		} else {
			dprintf("Buffer not large enough\n");
		}
		return needed;
	} else if (syscall == 7) {
		debug("exit");
		exit(arg1);
		return 0;
	} else {
		printf("%x\n", syscall);
		invalid();
	}
	return 0;
}

int char_to_register(uint8_t reg) {
	if (reg >= '0' && reg <= '9') {
		return reg - '0';
	}
	if (reg >= 'A' && reg <= 'Z') {
		return reg - 'A' + 10;
	}
	if (reg >= 'a' && reg <= 'z') {
		return reg - 'a' + 36;
	}
	return 0;
}

uint8_t hexchar(const char hex) {
	if (hex >= '0' && hex <= '9') {
		return hex - '0';
	}
	if (hex >= 'a' && hex <= 'f') {
		return hex - 'a' + 10;
	}
	invalid();
	return 0;
}

uint16_t readpchex() {
	return hexchar(readpc8()) << 12 | hexchar(readpc8()) << 8 | hexchar(readpc8()) << 4 | hexchar(readpc8()) << 0;
}

int main(int argc, const char** argv) {
	int fd;

	if (argc == 1) {
		printf("USAGE: vm program [arguments...]\n");
		exit(1);
	}

	_argc = argc;
	_argv = argv;
	flag = 0;
	memset(program, 0, PROGRAM_LENGTH);
	memset(registers, 0, sizeof(registers));

	fd = open(argv[1], O_RDONLY);
	read(fd, program, PROGRAM_LENGTH);
	close(fd);

	while (1) {
		dprintf("PC = %08x\n", registers[PC]);
		for (int i = 0; i < sizeof(registers) / sizeof(registers[0]); i += 8) {
			dprintf("%08x %08x %08x %08x %08x %08x %08x %08x\n", registers[i], registers[i+1], registers[i+2], registers[i+3],
				registers[i+4], registers[i+5], registers[i+6], registers[i+7]);
		}

		int pc = registers[PC];
		uint8_t op1 = program[pc+0];
		uint8_t op2 = program[pc+1];
		uint8_t op3 = program[pc+2];
		uint8_t op4 = program[pc+3];
		registers[PC] += 4;

		dprintf("%c%c%c%c\n", op1, op2, op3, op4);

		if (op2 == '?') {
			if (flag) {
				op2 = ' ';
			} else {
				// Skip
				continue;
			}
		}

		if (op1 == '=') {
			if (op2 == '#') {
				// 16-bit hex literal
				debug("hex literal");
				registers[char_to_register(op3)] = readpchex();
			} else if (op2 == ' ') {
				// Register to register
				debug("register-to-register");
				registers[char_to_register(op3)] = registers[char_to_register(op4)];
			} else if (op2 == '$') {
				// 32-bit binary literal
				debug("binary literal");
				registers[char_to_register(op3)] = readpc32();
			} else if (op2 == '[') {
				// 8-bit indirect load
				debug("8-bit indirect load");
				registers[char_to_register(op3)] = program[registers[char_to_register(op4)]];
			} else if (op2 == '{') {
				// 16-bit indirect load
				debug("16-bit indirect load");
			} else if (op2 == '(') {
				// 32-bit indirect load
				debug("32-bit indirect load");
				registers[char_to_register(op3)] = read32(&program[registers[char_to_register(op4)]]);
			} else {
				debug("invalid load");
				invalid();
			}
		} else if (op1 == '[' && op2 == '=') {
			// 8-bit indirect store
			debug("8-bit indirect store");
			program[registers[char_to_register(op3)]] = registers[char_to_register(op4)];
		} else if (op1 == '{' && op2 == '=') {
			// TODO
			invalid();
		} else if (op1 == '(' && op2 == '=') {
			write32(&program[registers[char_to_register(op3)]], registers[char_to_register(op4)]);
		} else if (op1 == '+' && op2 == ' ') {
			registers[char_to_register(op3)] += registers[char_to_register(op4)];
		} else if (op1 == '-' && op2 == ' ') {
			registers[char_to_register(op3)] -= registers[char_to_register(op4)];
		} else if (op1 == '*' && op2 == ' ') {
			registers[char_to_register(op3)] *= registers[char_to_register(op4)];
		} else if (op1 == '/' && op2 == ' ') {
			registers[char_to_register(op3)] /= registers[char_to_register(op4)];
		} else if (op1 == '&' && op2 == ' ') {
			registers[char_to_register(op3)] &= registers[char_to_register(op4)];
		} else if (op1 == '|' && op2 == ' ') {
			registers[char_to_register(op3)] |= registers[char_to_register(op4)];
		} else if (op1 == '^' && op2 == ' ') {
			registers[char_to_register(op3)] ^= registers[char_to_register(op4)];
		} else if (op1 == '>' && op2 == ' ') {
			registers[char_to_register(op3)] >>= registers[char_to_register(op4)];
		} else if (op1 == '<' && op2 == ' ') {
			registers[char_to_register(op3)] <<= registers[char_to_register(op4)];
		} else if (op1 == '?') {
			if (op2 == '=') {
				debug("equal?");
				flag = registers[char_to_register(op3)] == registers[char_to_register(op4)];
			} else if (op2 == '>') {
				debug("gt?");
				flag = registers[char_to_register(op3)] >registers[char_to_register(op4)];
			} else if (op2 == '<') {
				debug("lt?");
				flag = registers[char_to_register(op3)] < registers[char_to_register(op4)];
			} else if (op2 == '!') {
				debug("ne?");
				flag = registers[char_to_register(op3)] != registers[char_to_register(op4)];
			} else {
				invalid();
			}
		} else if (op1 == 'S') {
			// Syscall
			debug("syscall");
			if (op2 == ' ') {
				registers[char_to_register(op3)] = sc(registers[char_to_register(op3)], registers[char_to_register(op4)], 0, 0, 0, 0);
			} else if (op2 == '+') {
				int a = readpc8();
				int b = readpc8();
				int c = readpc8();
				int d = readpc8();
				registers[char_to_register(op3)] = sc(registers[char_to_register(op3)], registers[char_to_register(op4)],
					registers[char_to_register(a)], registers[char_to_register(b)],
					registers[char_to_register(c)], registers[char_to_register(d)]);
			} else {
				invalid();
			}
		} else if (op1 == 'J') {
			debug("jump");
			if (op2 == ' ') {
				registers[PC] = registers[char_to_register(op3)];
			} else {
				invalid();
			}
		} else {
			invalid();
		}
	}
	return 0;
}
