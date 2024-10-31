
#ifndef TEST_H_
#define TEST_H_

#include <stdbool.h>
#define MAX_TOKENS 16

typedef struct Scanner Scanner;
typedef struct Token Token;
typedef enum TokenType TokenType;

struct Scanner {
	char* s;
	int start;
	int cur;
};

enum TokenType {
	TOKEN_EQUAL,
	TOKEN_R_BRACKET,
	TOKEN_L_BRACKET,
	TOKEN_AT,
	TOKEN_DOLLAR,
	TOKEN_COMMA,

	TOKEN_LOAD,
	TOKEN_STORE,
	TOKEN_READ,
	TOKEN_WRITE,

	TOKEN_ADD,
	TOKEN_SUB,
	TOKEN_MUL,
	TOKEN_DIV,
	TOKEN_INC,

	TOKEN_BR,
	TOKEN_BLT,
	TOKEN_BGT,
	TOKEN_BLEQ,
	TOKEN_BGEQ,
	TOKEN_BEQ,

	TOKEN_LABEL_REF,
	TOKEN_REGISTER,
	TOKEN_NUMBER,

	TOKEN_END
};

struct Token {
	TokenType type;
	char* literal; // Pointer to original text - Aka not malloced
	int length; // Length of actual string
};


char ScannerAdvance();
void ScannerAddToken(TokenType token_type);
void ScannerInit(char* s);
void ScannerFree();
bool ScannerAtEnd();

void TokenDbg(Token* token);
void TokensPrint();
void Tokenize();

#endif // TEST_H_