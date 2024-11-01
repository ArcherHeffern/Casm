#include <stdlib.h>
#include <stdio.h>
#include "preprocess.h"
#include "lexer.h"

#define MAX_LABELS 16

int registers[10] = { 0 };
char* memory[64] = { NULL };
char* storage[64] = { NULL };
bool haltflag = false;

char* casm_error = NULL;

typedef struct Scanner {
	TokenList* token_list;
	int cur;
} Scanner;

typedef struct Register {
	int index;
	int value;
} Register;

// Entry Points
bool StepProgram();
void ExecuteInstruction(Scanner* scanner);
// Scanner
Token* Advance(Scanner* scanner);
Token* Check(Scanner* scanner, TokenType token_type);
Token* Consume(Scanner* scanner, TokenType token_type);
Token* Peek(Scanner* scanner);
Token* Prev(Scanner* scanner);
bool IsAtEnd(Scanner* scanner);
// Executors
void ExecuteLoad(Scanner* scanner);
void ExecuteMath(TokenType instruction, Scanner* scanner);
int ResolveDirectAddress(Scanner* scanner);
int ResolveImmediateAddress(Scanner* scanner);
int ResolveIndexAddress(Scanner* scanner);
int ResolveIndirectAddress(Scanner* scanner);
int ResolveLoadValue(Scanner* scanner);
int ResolveRelativeAddress(Scanner* scanner);
// Helpers
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
// Main
int main();


// ============
// Entry Points
// ============
bool StepProgram() {
	char* line = memory[registers[0]++];
	if (!line) {
		char* error_msg = malloc(64);
		asprintf(&error_msg, "Expected instruction but found garbage");
		SetErrorMsg(error_msg);
		return false;
	}
	TokenList* token_list = TokenizeLine(line);
	TokenListPrint(token_list);
	printf("\n");
	Scanner scanner = { token_list, 0 };

	ExecuteInstruction(&scanner);

	TokenListFree(token_list);

	bool can_continue = !casm_error && !haltflag;
	return can_continue;
}

void ExecuteInstruction(Scanner* scanner) {
	TokenType instruction = Advance(scanner)->type;
	switch (instruction) {
		case TOKEN_LOAD:
			ExecuteLoad(scanner);
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
			char* error_msg = malloc(64);
			asprintf(&error_msg, "Unexpected token: %s", TokenTypeToString[instruction]);
			SetErrorMsg(error_msg);
		}
	}
	if (!IsAtEnd(scanner)) {
		char* error_msg = malloc(64);
		asprintf(&error_msg, "Too many tokens on this line");
		SetErrorMsg(error_msg);
	}
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
		char* error_msg = malloc(64);
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
		char* error_msg = malloc(64);
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


void ExecuteLoad(Scanner* scanner) {
	Register r1 = GetRegister(scanner);
	Consume(scanner, TOKEN_COMMA);
	int value = ResolveLoadValue(scanner);
	if (!casm_error) {
		SetRegister(r1.index, value);
	}
}


int ResolveLoadValue(Scanner* scanner) {
	TokenType token_type = Peek(scanner)->type;
	switch (token_type) {
		case TOKEN_REGISTER:
			return ResolveDirectAddress(scanner);
		case TOKEN_EQUAL:
			return ResolveImmediateAddress(scanner);
		case TOKEN_L_BRACKET:
			return ResolveIndexAddress(scanner);
		case TOKEN_AT:
			return ResolveIndirectAddress(scanner);
		case TOKEN_DOLLAR:
			return ResolveRelativeAddress(scanner);
	}
	char* error_msg = malloc(64);
	asprintf(&error_msg, "Unexpected token %s", TokenTypeToString[token_type]);
	SetErrorMsg(error_msg);
	return 0;
}


int ResolveDirectAddress(Scanner* scanner) {
	return GetRegister(scanner).value;
}


int ResolveImmediateAddress(Scanner* scanner) {
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

	return GetMemory(addr+r.value);
}


int ResolveIndirectAddress(Scanner* scanner) {
	Advance(scanner);
	int address = GetRegister(scanner).value;

	if (casm_error) {
		return 0;
	}

	return GetMemory(GetMemory(address));
}


int ResolveRelativeAddress(Scanner* scanner) {
	Advance(scanner);
	int offset = GetRegister(scanner).value;
	int pc = 4*(registers[0]-1);
	return GetMemory(offset+pc);
}


// ============
// Helpers
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
	if (address % 4 != 0) {
		char* error_msg = malloc(64);
		asprintf(&error_msg, "Expected address to be multiple of 4: 0x%d", address);
		SetErrorMsg(error_msg);
		return 0;
	}
	char* line = memory[address/4];
	if (line == NULL) {
		char* error_msg = malloc(64);
		asprintf(&error_msg, "Garbage contained at memory address: 0x%d", address);
		SetErrorMsg(error_msg);
		return 0;
	}
	return atoi(line);
}


// ============
// Setters
// ============
// TODO: Add animations
void SetRegister(int num, int value) {
	registers[num] = value;
}


void SetMemory(int num, char* value, bool set_focused) {
	memory[num] = value;
}


void SetStorage(int num, char* value){
	memory[num] = value;
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


// ============
// Main
// ============
int main() {
	/*
	char* lines[] = {
		"			LOAD R1 =0",
		"			LOAD R2, =10",
		"Label: 	BGEQ R1, R2, Label2", 
		"			ADD R2, R1",
		"			INC R1 ",
		"Label2:	BR Label",
		"HALT"
	};
	*/
	memory[20] = "17";
	char* lines[] = {
		"LOAD R1, =12",
		"LOAD R2, R1",
		"LOAD R3, [68, R1]",
		"ADD R1, R2",
		"HALT"
	};
	int num_lines = sizeof(lines)/sizeof(char*);
	char* label_names[MAX_LABELS];
	int label_locations[MAX_LABELS];
	int num_labels = Preprocess(lines, num_lines, label_names, label_locations);
	if (num_labels < 0) {
		fprintf(stderr, "ERROR: %s\n", preprocess_error_msg);
		return 0;
	}
	// Load memory
	for (int i = 0; i < num_lines; i++) {
		memory[i] = lines[i];
	}

	while (StepProgram()) {}
	if (casm_error) {
		int pc = registers[0]-1;
		printf("Error on line 0x%X: %s\n", pc*4, casm_error);
		printf("%s\n\n", lines[pc]);
	}
	PrintRegisters();
	return 0;
}
