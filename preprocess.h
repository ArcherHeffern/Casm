
#ifndef PREPROCESSOR_H_
#define PREPROCESSOR_H_

extern char* preprocess_error_msg; // No newline
// Returns number of labels or -1 if there was an error
int Preprocess(char** lines, int num_lines, char** label_names, int* label_locations);

#endif
