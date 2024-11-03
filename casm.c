#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "preprocess.h"
#include "lexer.h"
#include "util.h"

#define MAX_LABELS 16
#define MAX_REGISTERS 9
#define MEMORY_SIZE 64
#define STORAGE_SIZE 64
#define MAX_LABEL_JUMPS 1000

int registers[MAX_REGISTERS+1] = { 0 };
char* memory[MEMORY_SIZE] = { NULL };
char* storage[STORAGE_SIZE] = { NULL };
bool haltflag = false;

int num_labels = 0;
int num_label_jumps = 0; // Checks for possible infinite loops
char* label_names[MAX_LABELS] = { NULL };
int label_locations[MAX_LABELS] = { 0 };
int label_jump_counts[MAX_LABELS] = { 0 };

char* casm_error = NULL;

typedef struct Scanner {
	TokenList* token_list;
	int cur;
} Scanner;

typedef struct Register {
	int index;
	int value;
} Register;

// Entry Points - If any return false, check casm_error
bool LoadProgram(char** program, int num_lines); // Automatically resets state from previous execution
bool RunProgram(); // True if program was successful
bool StepProgram();
void PrintErrorMsg();
// Scanner
Token* Advance(Scanner* scanner);
Token* Check(Scanner* scanner, TokenType token_type);
Token* Consume(Scanner* scanner, TokenType token_type);
Token* Peek(Scanner* scanner);
Token* Prev(Scanner* scanner);
bool IsAtEnd(Scanner* scanner);
// Executors
void ExecuteInstruction(Scanner* scanner);
void ExecuteLoad(Scanner* scanner);
void ExecuteStore(Scanner* scanner);
void ExecuteMath(TokenType instruction, Scanner* scanner);
int ResolveDirectAddressValue(Scanner* scanner);
int ResolveLoadValue(Scanner* scanner);
int ResolveStoreAddress(Scanner* scanner);
// Addressing
int ResolveImmediateAddressValue(Scanner* scanner);
int ResolveIndexAddress(Scanner* scanner);
int ResolveIndexAddressValue(Scanner* scanner);
int ResolveIndirectAddress(Scanner* scanner);
int ResolveIndirectAddressValue(Scanner* scanner);
int ResolveRelativeAddress(Scanner* scanner);
int ResolveRelativeAddressValue(Scanner* scanner);
// Getters
Register GetRegister(Scanner* scanner);
int GetNumberNumber(Scanner* scanner);
int GetMemory(int address);
// Setters
void SetRegister(int num, int value);
void SetMemory(int num, char* value, bool set_focused);
void SetStorage(int num, char* value);
void SetErrorMsg(char* msg);
// Debug Info
void PrintRegisters();
void PrintMemory();
void PrintMemoryRange(int lower, int upper); // Inclusive on both bounds
char* PrintJumpLabelBreakdown();
// Main
int main();
// Tests
void LoadTest();
void StoreTest();
void LoopTest();


// ============
// Entry Points
// ============
bool LoadProgram(char** program, int num_lines) {
	for (int i = 0; i < 10; i++) {
		SetRegister(i, 0);
	}
	for (int i = 0; i < MEMORY_SIZE; i++) {
		SetMemory(i, NULL, true);
	}
	for (int i = 0; i < STORAGE_SIZE; i++) {
		SetStorage(i, NULL);
	}
	for (int i = 0; i < num_labels; i++) {
		free(label_names[i]);
		label_names[i] = NULL;
		label_locations[i] = 0; 
		label_jump_counts[i] = 0;
	}
	num_labels = 0;
	num_label_jumps = 0;
	haltflag = false;
	if (casm_error) {
		free(casm_error);
		casm_error = NULL;
	}

	num_labels = Preprocess(program, num_lines, label_names, label_locations);
	if (num_labels < 0) {
		char* error_msg;
		asprintf(&error_msg, "Preprocess error: %s", preprocess_error_msg);
		SetErrorMsg(error_msg);
		return false;
	}
	// Load memory
	for (int i = 0; i < num_lines; i++) {
		memory[i] = program[i];
	}
	return true;
}

bool RunProgram() {
	while (StepProgram()) {
		if (num_label_jumps >= MAX_LABEL_JUMPS) {
			char* error_msg;
			asprintf(&error_msg, "%d jumps performed - Possible infinite loop\n\n%s", MAX_LABEL_JUMPS, PrintJumpLabelBreakdown());
			SetErrorMsg(error_msg);
			break;
		}
	}
	return casm_error == NULL;
}

void PrintErrorMsg() {
	if (!casm_error) {
		printf("Attempted to print error msg when there was no error\n");
		return;
	}
	int pc = registers[0]-1;
	printf("Error at address %d executing '%s'\n", pc*4, memory[pc]);
	printf("%s\n", casm_error);
}

bool StepProgram() {
	char* line = memory[registers[0]++];
	if (!line) {
		char* error_msg;
		asprintf(&error_msg, "Expected instruction but found garbage");
		SetErrorMsg(error_msg);
		return false;
	}
	TokenList* token_list = TokenizeLine(line);
	if (token_list == NULL) {
		char* error_msg;
		asprintf(&error_msg, "Lexer Error: %s", lexer_error);
		SetErrorMsg(error_msg);
		return false;
	}
	Scanner scanner = { token_list, 0 };

	ExecuteInstruction(&scanner);

	TokenListFree(token_list);

	bool can_continue = !casm_error && !haltflag;
	return can_continue;
}

// ============
// Scanner
// ============
Token* Peek(Scanner* scanner) {
	if (IsAtEnd(scanner)) {
		return NULL;
	}
	return scanner->token_list->tokens[scanner->cur];
}


Token* Advance(Scanner* scanner) {
	Token* token = Peek(scanner);
	if (token == NULL) {
		return NULL;
	}
	scanner->cur++;
	return token;
}


Token* Check(Scanner* scanner, TokenType token_type) {
	if (casm_error) {
		return NULL;
	}
	Token* token = Peek(scanner);
	if (!token || token->type != token_type) {
		char* error_msg;
		asprintf(&error_msg, "Expected %s but found %s", TokenTypeToString[token_type], TokenTypeToString[token||TOKEN_NONE]);
		SetErrorMsg(error_msg);
		return NULL;
	}
	return token;
}


Token* Consume(Scanner* scanner, TokenType token_type) {
	if (casm_error) {
		return NULL;
	}
	Token* token = Advance(scanner);
	if (!token || token->type != token_type) {
		char* error_msg;
		asprintf(&error_msg, "Expected %s but found %s", 
			TokenTypeToString[token_type], 
			TokenTypeToString[token->type?token->type: TOKEN_NONE]
		);
		SetErrorMsg(error_msg);
		
		return NULL;
	}
	return token;
}


Token* Prev(Scanner* scanner) {
	return scanner->token_list->tokens[scanner->cur-1];
}


bool IsAtEnd(Scanner* scanner) {
	return scanner->cur == scanner->token_list->size;
}


// ============
// Executors
// ============
void ExecuteInstruction(Scanner* scanner) {
	TokenType instruction = Advance(scanner)->type;
	switch (instruction) {
		case TOKEN_LOAD:
			ExecuteLoad(scanner);
			break;
		case TOKEN_STORE:
			ExecuteStore(scanner);
			break;
		case TOKEN_HALT:
			haltflag = true;
			break;
		case TOKEN_ADD:
		case TOKEN_SUB:
		case TOKEN_MUL:
		case TOKEN_DIV: 
			ExecuteMath(instruction, scanner);
			break;
		default: {
			char* error_msg;
			asprintf(&error_msg, "Unexpected token while resolving instruction: %s", TokenTypeToString[instruction]);
			SetErrorMsg(error_msg);
		}
	}
	if (!IsAtEnd(scanner)) {
		char* error_msg;
		asprintf(&error_msg, "Too many tokens on this line");
		SetErrorMsg(error_msg);
	}
}

void ExecuteMath(TokenType instruction, Scanner* scanner) {
	Register r1 = GetRegister(scanner);
	Consume(scanner, TOKEN_COMMA);
	Register r2 = GetRegister(scanner);
	if (casm_error) {
		return;
	}
	int op1 = registers[r1.index];
	int op2 = registers[r2.index];
	int result;
	if (instruction == TOKEN_ADD) {
		result = op1 + op2;
	} else if (instruction == TOKEN_SUB) {
		result = op1 - op2;
	} else if (instruction == TOKEN_MUL) {
		result = op1 * op2;
	} else if (instruction == TOKEN_DIV) {
		result = op1 / op2;
		SetRegister(r1.index, op1%op2);
	}
	SetRegister(r1.index, result);
}

int ResolveLoadValue(Scanner* scanner) {
	Token* token = Peek(scanner);
	if (token == NULL) goto err;
	switch (token->type) {
		case TOKEN_REGISTER:
			return ResolveDirectAddressValue(scanner);
		case TOKEN_EQUAL:
			return ResolveImmediateAddressValue(scanner);
		case TOKEN_L_BRACKET:
			return ResolveIndexAddressValue(scanner);
		case TOKEN_AT:
			return ResolveIndirectAddressValue(scanner);
		case TOKEN_DOLLAR:
			return ResolveRelativeAddressValue(scanner);
	}
	err: {
		char* error_msg;
		asprintf(&error_msg, "Unexpected token %s while resolving load value", 
			TokenTypeToString[token?token->type: TOKEN_NONE]
		);
		SetErrorMsg(error_msg);
		return 0;
	}
}

int ResolveStoreAddress(Scanner* scanner) {
	Token* token = Peek(scanner);
	if (token == NULL) goto err;
	switch (token->type) {
		case TOKEN_REGISTER:
			return ResolveDirectAddressValue(scanner);
		case TOKEN_L_BRACKET:
			return ResolveIndexAddress(scanner);
		case TOKEN_DOLLAR:
			return ResolveRelativeAddress(scanner);
	}
	err: {
		char* error_msg;
		asprintf(&error_msg, "Unexpected token %s while resolving store value", 
			TokenTypeToString[token?token->type: TOKEN_NONE]
		);
		SetErrorMsg(error_msg);
		return 0;
	}
	
}


void ExecuteLoad(Scanner* scanner) {
	Register r1 = GetRegister(scanner);
	Consume(scanner, TOKEN_COMMA);
	int value = ResolveLoadValue(scanner);
	if (!casm_error) {
		SetRegister(r1.index, value);
	}
}

void ExecuteStore(Scanner* scanner) {
	Register r1 = GetRegister(scanner);
	Consume(scanner, TOKEN_COMMA);
	int address = ResolveStoreAddress(scanner); // Needs to get the addresses instead of the values
	if (!casm_error) {
		char* str_value = IntToString(r1.value);
		SetMemory(address, str_value, false);
	}
}

// ============
// Addressing
// ============
int ResolveDirectAddressValue(Scanner* scanner) {
	return GetRegister(scanner).value;
}


int ResolveImmediateAddressValue(Scanner* scanner) {
	Advance(scanner);
	return GetNumberNumber(scanner);
}


int ResolveIndexAddress(Scanner* scanner) {
	Advance(scanner);
	int addr = GetNumberNumber(scanner);
	Consume(scanner, TOKEN_COMMA);
	Register r = GetRegister(scanner);
	Consume(scanner, TOKEN_R_BRACKET);
	
	if (casm_error) {
		return 0;
	}

	return addr+r.value;
}

int ResolveIndexAddressValue(Scanner* scanner) {
	return GetMemory(ResolveIndexAddress(scanner));
}


int ResolveIndirectAddress(Scanner* scanner) {
	Advance(scanner);
	int address = GetRegister(scanner).value;

	if (casm_error) {
		return 0;
	}

	return GetMemory(address);
}

int ResolveIndirectAddressValue(Scanner* scanner) {
	return GetMemory(ResolveIndirectAddress(scanner));
}


int ResolveRelativeAddress(Scanner* scanner) {
	Advance(scanner);
	int offset = GetRegister(scanner).value;
	int pc = 4*(registers[0]-1);
	return offset+pc;
}

int ResolveRelativeAddressValue(Scanner* scanner) {
	return GetMemory(ResolveRelativeAddress(scanner));
}


// ============
// Getters
// ============
Register GetRegister(Scanner* scanner) {
	// From R1->5 returns { .index=1, .value=.5 }
	Token* register_token = Consume(scanner, TOKEN_REGISTER);
	Register r;
	if (!register_token) {
		return r;
	}
	r.index = register_token->literal[1] - '0';
	r.value = registers[r.index];
	return r;
}

int GetNumberNumber(Scanner* scanner) {
	// From Token {8} -> 8
	Token* number_token = Consume(scanner, TOKEN_NUMBER);
	if (casm_error) {
		return 0;
	}
	return atoi(number_token->literal);
}

int GetMemory(int address) {
	// M:[0, 1, 2, 3, 4]
	// GetMemory(4) -> 1
	if (address % 4 >= MEMORY_SIZE) {
		char* error_msg;
		asprintf(&error_msg, "Memory address '%d' greater than max memory size '%d'", address, MEMORY_SIZE);
		SetErrorMsg(error_msg);
		return 0;
	}
	if (address % 4 != 0) {
		char* error_msg;
		asprintf(&error_msg, "Expected address to be multiple of 4: 0x%d", address);
		SetErrorMsg(error_msg);
		return 0;
	}
	char* line = memory[address/4];
	int contents = 0;
	if (line == NULL || !ToInteger(line, &contents)) {
		char* error_msg;
		asprintf(&error_msg, "Cannot read memory address %d since it contains garbage or a non positive integer: '%s'\nWhile this is *Technically* valid, since every memory address is actually just numbers being interpreted as instructions and whatnot, I'm assuming this is not what you were intending.", address, line);
		SetErrorMsg(error_msg);
		return 0;
	}
	return contents;
}


// ============
// Setters
// ============
// TODO: Add animations and enforce validations
void SetRegister(int reg_num, int value) {
	if (reg_num > MAX_REGISTERS || reg_num < 1) {
		char* error_msg;
		asprintf(&error_msg, "General purpose registers range from 1-%d. Used nonexistant register %d",
			MAX_REGISTERS,
			reg_num);
		return;
	}
	registers[reg_num] = value;
}


void SetMemory(int address, char* value, bool set_focused) {
	if (address % 4 >= MEMORY_SIZE) {
		char* error_msg;
		asprintf(&error_msg, "Memory address '%d' greater than max memory size '%d'", address, MEMORY_SIZE);
		SetErrorMsg(error_msg);
		return;
	}
	if (address % 4 != 0) {
		char* error_msg;
		asprintf(&error_msg, "Expected address to be multiple of 4: 0x%d", address);
		SetErrorMsg(error_msg);
		return;
	}
	memory[address/4] = value;
}


void SetStorage(int address, char* value){
	if (address % 4 >= STORAGE_SIZE) {
		char* error_msg;
		asprintf(&error_msg, "Storage address '%d' greater than max storage size '%d'", address, MEMORY_SIZE);
		SetErrorMsg(error_msg);
		return;
	}
	if (address % 4 != 0) {
		char* error_msg;
		asprintf(&error_msg, "Expected address to be multiple of 4: 0x%d", address);
		SetErrorMsg(error_msg);
		return;
	}
	storage[address/4] = value;
} 

void SetErrorMsg(char* msg) {
	if (casm_error) {
		free(msg);
		return;
	}
	casm_error = msg;
}

// ============
// Debug Info
// ============
void PrintRegisters() {
	printf("PC: %d\n", registers[0]);
	for (int i = 1; i < 10; i++) {
		printf("R%d: %d\n", i, registers[i]);
	}
}

void PrintMemory() {
	PrintMemoryRange(0, MEMORY_SIZE-1);
}

void PrintMemoryRange(int lower, int upper) {
	for (int i = lower/4; i < upper/4+1; i++) {
		printf("%d: %s\n", i*4, memory[i]);
	}
}

char* PrintJumpLabelBreakdown() {
	char* result = NULL;
    char *temp = NULL;    
	asprintf(&result, "Jumps per label breakdown:");

    for (int i = 0; i < num_labels; i++) {
        asprintf(&temp, "\n%s: %d", label_names[i], label_jump_counts[i]);

		char *new_result;
		asprintf(&new_result, "%s%s", result, temp);
		free(result);  
		result = new_result;
		free(temp);    
    }
	return result;
}


// ============
// Main
// ============
int main() {
	// LoadTest();
	StoreTest();
	// LoopTest();
}

void LoadTest() {
	char* lines[] = {
		"LOAD R1, =8",
		"LOAD R2, R1",
		"LOAD R3, [72, R1]", // Expect 28
		"LOAD R4, =80",
		"LOAD R5, @R4", // Expect 21
		"LOAD R6, $R1", // Expect 21
		"HALT",
		"21"
	};
	LoadProgram(lines, sizeof(lines)/sizeof(char*));
	memory[20] = "28"; // Override for index and indirect addressing
	if (!RunProgram()) {
		PrintErrorMsg();
	}
	assert(registers[0] == 7);
	assert(registers[1] == 8);
	assert(registers[2] == 8);
	assert(registers[3] == 28);
	assert(registers[4] == 80);
	assert(registers[5] == 21);
	assert(registers[6] == 21);
}

void StoreTest() {
	char* lines[] = {
		"LOAD R1, =100",
		"LOAD R2, =48",
		"LOAD R3, =4",
		"LOAD R4, =8",
		"STORE R1, R2", // M48:100
		"ADD R1, R3",
		"STORE R1, [4, R2]", // M52:104
		"ADD R1, R3",
		"STORE R1, $R4", // After haltflag: 108
		"HALT"
	};
	int num_lines = sizeof(lines)/sizeof(char*);
	if (!LoadProgram(lines, num_lines)) {
		PrintErrorMsg();
		return;
	}
	if (!RunProgram()) {
		PrintErrorMsg();
		return;
	}
	assert(memory[48/4] != NULL && "Memory at address 48 is not null");
	assert(GetMemory(48) == 100 && "Memory at address 48 is 100");
	assert(memory[52/4] != NULL && "Memory at address 52 is not null");
	assert(GetMemory(52) == 104 && "Memory at address 52 is 104");
	assert(memory[num_lines] != NULL && "Memory at address after haltflag is not null");
	assert(GetMemory(num_lines*4) == 108 && "Memory at after haltflag is 108");
}

void LoopTest() {
	char* lines[] = {
		"			LOAD R1, =0",
		"			LOAD R2, =10",
		"Label: 	BGEQ R1, R2, Label2", 
		"			ADD R2, R1",
		"			INC R1 ",
		"Label2:	BR Label",
		"HALT"
	};
	LoadProgram(lines, sizeof(lines)/sizeof(char*));
	printf("%s\n", PrintJumpLabelBreakdown());
	
	if (!RunProgram()) {
		PrintErrorMsg();
	}
}
