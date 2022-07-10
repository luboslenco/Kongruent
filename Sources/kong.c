#include "log.h"
#include "parser.h"
#include "tokenizer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

const char *filename = "in/test.kong";

int main(int argc, char **argv) {
	FILE *file = fopen(filename, "rb");

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *data = (char *)malloc(size + 1);
	assert(data != NULL);
	fread(data, 1, size, file);
	fclose(file);
	data[size] = 0;

	tokens_t tokens = tokenize(data);

	free(data);

	parse(&tokens);

	log(LOG_LEVEL_INFO, "Functions:");
	for (size_t i = 0; i < functions.size; ++i) {
		log(LOG_LEVEL_INFO, "%s", functions.f[i]->function.name);
	}
	log(LOG_LEVEL_INFO, "");

	log(LOG_LEVEL_INFO, "Structs:");
	for (size_t i = 0; i < structs.size; ++i) {
		log(LOG_LEVEL_INFO, "%s", structs.s[i]->structy.name);
	}

	return 0;
}
