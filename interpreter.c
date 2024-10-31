#include <math.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

#include "interpreter.h"

Token tokens[MAX_TOKENS];
size_t tokens_added;
Scanner* scanner = NULL;


char ScannerAdvance() {
	scanner->cur++;
	return scanner->s[scanner->cur-1];
}

char ScannerPeek() {
	return scanner->s[scanner->cur];
}

void ScannerDbg() {
	printf("{ .start=%d, .cur=%d, %.*s }\n", 
		scanner->start,
		scanner->cur,
		scanner->cur-scanner->start,
		&scanner->s[scanner->start]
	);
}

void ScannerAddToken(TokenType token_type) {
	ScannerDbg();
	Token* token = &tokens[tokens_added];
	token->type = token_type;
	token->literal = &scanner->s[scanner->start];
	token->length = scanner->cur-scanner->start;
	scanner->start = scanner->cur;
	scanner->cur = scanner->start;
	tokens_added++;
}

void ScannerInit(char* s) {
	scanner = (Scanner*)malloc(sizeof(Scanner));
	memset(scanner, 0, sizeof(Scanner));
	scanner->s = s;
}

int ScannerTokenLength() {
	return scanner->cur - scanner->start;
}

char ScannerGetTokenCharAt(int i) {
	return scanner->s[scanner->start+i];
}

void ScannerFree() {
	free(scanner);
};

bool ScannerAtEnd() {
	char cur_char = scanner->s[scanner->cur];
	return cur_char == '\0' || cur_char == '\n' || cur_char == ';'; // ; is comment
}

void TokenDbg(Token* token) {
	printf("{ .type=%d, .literal=%.*s }\n", 
		token->type,
		token->length, 
		token->literal
	);
}

void TokensPrint() {
	for (int i = 0; i < tokens_added; i++) {
		TokenDbg(&tokens[i]);
	}
}

bool IsDigit(char c) {
	return c <='9' && c >= '0';
}

bool IsAlpha(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

void ScannerScanNumber() {
	while (!ScannerAtEnd() && IsDigit(ScannerPeek())) {
		ScannerAdvance();
	};
	ScannerAddToken(TOKEN_NUMBER);
}

bool ScannerContainsRegister() {
	return scanner->cur - scanner->start == 2 
		&& scanner->s[scanner->start] == 'R'
		&& IsDigit(scanner->s[scanner->start+1]);
}

TokenType ScannerCheckRest(int pos, char* s, int len, TokenType token) {
	if (ScannerTokenLength() - pos != len) {
		return TOKEN_LABEL_REF;
	}
	if (strncasecmp(&scanner->s[scanner->start+pos], s, len) == 0) {
		return token;
	}
	return TOKEN_LABEL_REF;
}

TokenType ScannerParseIdentifier() {
	// Very fast! Conceptually a hard coded trie
	switch (ScannerGetTokenCharAt(0)) {
		case 'A':
			return ScannerCheckRest(1, "DD", 2, TOKEN_ADD);
		case 'B':
			if (ScannerTokenLength() < 2) {
				return TOKEN_LABEL_REF;
			}
			switch (ScannerGetTokenCharAt(1)) {
				case 'E':
					return ScannerCheckRest(2, "Q", 1, TOKEN_BEQ);
				case 'G':
					if (ScannerTokenLength() < 3) {
						return TOKEN_LABEL_REF;
					}
					switch (ScannerGetTokenCharAt(2)) {
						case 'T':
							return ScannerCheckRest(3, "", 0, TOKEN_BGT);
						case 'E':
							return ScannerCheckRest(3, "Q", 1, TOKEN_BGEQ);
					}
				case 'L':
					if (ScannerTokenLength() < 3) {
						return TOKEN_LABEL_REF;
					}
					switch (ScannerGetTokenCharAt(2)) {
						case 'E':
							return ScannerCheckRest(3, "Q", 1, TOKEN_BLEQ);
						case 'T':
							return ScannerCheckRest(3, "", 0, TOKEN_BLT);
					}
				case 'R':
					return ScannerCheckRest(2, "", 0, TOKEN_BR);
			}
		case 'D':
			return ScannerCheckRest(1, "IV", 2, TOKEN_DIV);
		case 'I':
			return ScannerCheckRest(1, "NC", 2, TOKEN_INC);
		case 'L':
			return ScannerCheckRest(1, "OAD", 3, TOKEN_LOAD);
		case 'M':
			return ScannerCheckRest(1, "UL", 2, TOKEN_MUL);
		case 'R':
			return ScannerCheckRest(1, "EAD", 3, TOKEN_READ);
		case 'S':
			if (ScannerTokenLength() < 2) {
				return TOKEN_LABEL_REF;
			}
			switch (ScannerGetTokenCharAt(1)) {
				case 'T':
					return ScannerCheckRest(2, "ORE", 3, TOKEN_STORE);
				case 'U':
					return ScannerCheckRest(2, "B", 1, TOKEN_SUB);
			}
		case 'W':
			return ScannerCheckRest(1, "RITE", 4, TOKEN_WRITE);
	}
	return TOKEN_LABEL_REF;
}

void ScannerScanIdentifier() {
	while (!ScannerAtEnd()) {
		char c = ScannerPeek();
		if (!IsDigit(c) && !IsAlpha(c) && c != '_') {
			break;
		}
		ScannerAdvance();
	}
	if (ScannerContainsRegister()) {
		ScannerAddToken(TOKEN_REGISTER);
	} else {
		ScannerAddToken(ScannerParseIdentifier());
	}
}

void ScannerSkipWhitespace() {
	while (!ScannerAtEnd()) {
		char c = ScannerPeek();
		switch (c) {
			case ' ':
			case '\n':
			case '\r':
			case '\t':
				scanner->start++;
				scanner->cur++;
			default:
				return;
		}
	}
}

void Tokenize() {
	while (!ScannerAtEnd()) {
		ScannerSkipWhitespace();
		char c = ScannerAdvance();
		switch (c) {
			case '=':
				ScannerAddToken(TOKEN_EQUAL);
				break;
			case ']':
				ScannerAddToken(TOKEN_R_BRACKET);
				break;
			case '[':
					ScannerAddToken(TOKEN_L_BRACKET);
					break;
			case '@':
					ScannerAddToken(TOKEN_AT);
					break;
			case '$':
					ScannerAddToken(TOKEN_DOLLAR);
					break;
			case ',':
					ScannerAddToken(TOKEN_COMMA);
					break;
			default:
				if (IsDigit(c)) {
					ScannerScanNumber();
				}
				else if (IsAlpha(c)) {
					ScannerScanIdentifier();
				} else {
					printf("Unexpected Token");
					return;
				}
		}
	}
}

void Preprocess(char** lines) {
	// Handle Labels
}

#define MAX_LABELS 8
char* label_names[MAX_LABELS];
int label_lines[MAX_LABELS];

int main() {
	char* lines[] = {
		"WRITE WRRITE STORE SUB STTORE SUBB BLEQ BLT BR BLEQQ BLTT BRR BGT BGEQ BGEQQ R1 ADD DIV INC MUL flub MULflub R2 R3 23048 hi",
		"LABEL: LOAD R1, R2",
		"BLT LABEL",
	};
	Preprocess(lines);
	char* s = lines[0];
	// s = "5[]$=100=,10";
	ScannerInit(s);
	Tokenize();
	printf("Tokens added: %zu\n", tokens_added);
	TokensPrint();

	// Cleanup
	ScannerFree();
	/*
		FILE* f = fopen("test.a", "r");
		size_t s = 64;
		char* token = NULL;
		char* line = malloc(s);
		getline(&line, &s, f);
		token = strsep(&line, " ");
		printf("%s\n", token);

		if (eq(token, "ADD")) {
		} else if (eq(token, "SUB")) {
		}
	*/	
}