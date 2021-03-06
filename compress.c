#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "file_io.h"
#include "LZW.h"

int main(int argc, char **argv)
{
	// parse arguments
	const char *file_name_input = NULL;
	const char *file_name_alphabet = NULL;
	const char *file_name_output = "output.lzw";
	const char *file_name_dictionary = NULL;
	const char *file_name_tuples = NULL;
	uint32_t reverse_input = 0;
	if (argc > 1)
	{
		uint32_t args_iterator = 1;
		while (args_iterator < argc)
		{
			if (argv[args_iterator][0] == '-')
			{
				if (args_iterator + 1 != argc)
				{
					if (argv[args_iterator][1] == 'a')
					{
						file_name_alphabet = argv[++args_iterator];
					}
					else if (argv[args_iterator][1] == 'o')
					{
						file_name_output = argv[++args_iterator];
					}
					else if (argv[args_iterator][1] == 'd')
					{
						file_name_dictionary = argv[++args_iterator];
					}
					else if (argv[args_iterator][1] == 't')
					{
						file_name_tuples = argv[++args_iterator];
					}
					else if (argv[args_iterator][1] == 'r')
					{
						reverse_input = 1;
					}
					else
					{
						ERROR("Option '%s' not defined.", argv[args_iterator]);
					}
				}
				else
				{
					FATAL("Option '%s' passed but no file provided.", argv[args_iterator - 1]);
				}
			}
			else if (file_name_input == NULL)
			{
				file_name_input = argv[args_iterator];
			}
			else
			{
				FATAL("File name defined twice in arguments.");
			}
			args_iterator++;
		}
	}
	else
	{
		FATAL("No file provided.");
	}

	FILE *file_tuples = NULL;
	if (file_name_tuples != NULL)
	{
		file_tuples = fopen(file_name_tuples, "w");
		if(file_tuples == NULL)
		{
			ERROR("Couldn't open tuples file.\n");
		}
	}

	// read file
	char *input_data = NULL;
	file_read((void *)&input_data, file_name_input);
	uint32_t input_data_len = strlen(input_data);

	if (reverse_input)
	{
		char *temp = malloc(input_data_len + 1);
		temp[input_data_len] = '\0';
		for (uint32_t i = 0; i < input_data_len; i++)
		{
			temp[i] = input_data[input_data_len - i - 1];
		}
		free(input_data);
		input_data = temp;
	}

	// define alphabet
	char *alphabet;
	if (file_name_alphabet != NULL)
	{
		file_read((void *)&alphabet, file_name_alphabet);
	}
	else
	{
		const char *default_alphabet_string = "ABCDEFGHIJKLMNPQRSTVWXY\n";
		alphabet = malloc(strlen(default_alphabet_string) + 1);
		strcpy(alphabet, default_alphabet_string);
	}

	// create dictionary
	Dictionary dictionary;
	Dictionary_create(&dictionary, 128);

	// add terminating character to dictionary
	const char *_null_ref = "\0";
	SubString _null_string = {&_null_ref, 0, 1};
	Dictionary_insert(&dictionary, &_null_string);

	// add alphabet to dictionary
	for (uint32_t i = 0; i < strlen(alphabet); i++)
	{
		SubString temp = {&alphabet, i, i + 1};
		Dictionary_insert(&dictionary, &temp);
	}

	// create output data
	BufferBit output_codes;
	BufferBit_create(&output_codes, 128);

	// compress
	uint32_t bit_length = log2_uint(dictionary.count_c - 1);
	SubString pattern = {.ref = &input_data, .start = 0, .end = 1};
	uint32_t code_count = 0;
	uint32_t percent_counter = 0;
	do
	{
		SubString p_c = {.ref = &input_data, .start = pattern.start, .end = pattern.end + 1};
		uint32_t found_in_dictionary = Dictionary_doesEntryExist(&dictionary, &p_c);
		if (!found_in_dictionary)
		{
			// add code to output
			uint32_t code = Dictionary_findIndex(&dictionary, &pattern);
			BufferBit_insert(&output_codes, code, bit_length);
			if (file_tuples != NULL)
			{
				for (uint32_t i = pattern.start; i < pattern.end; i++)
					fprintf(file_tuples, "%c", (*pattern.ref)[i]);
				fprintf(file_tuples, " -> %d\n", code);
			}
			code_count++;

			// add p_c to dictionary
			Dictionary_insert(&dictionary, &p_c);

			// increase bit length if necessary
			bit_length = log2_uint(dictionary.count_c - 1);

			// p = c
			pattern.start = pattern.end;
		}
		pattern.end++;

		if ((uint32_t)((float)pattern.start / (float)input_data_len * 100.0f) > percent_counter)
		{
			fprintf(stdout, "%d%% done\n", ++percent_counter);
		}
	} while ((*pattern.ref)[pattern.start] != '\0');

	if (file_tuples != NULL)
	{
		fprintf(file_tuples, "%d tuples were generated.\n", code_count);
		fclose(file_tuples);
	}

	// insert ending code
	BufferBit_insert(&output_codes, 0, bit_length);

	file_write(output_codes.data, sizeof(*(output_codes.data)) * (output_codes.index_uint + 1), file_name_output);

	if (file_name_dictionary != NULL)
		Dictionary_output(&dictionary, file_name_dictionary);

	// cleanup
	{
		Dictionary_destroy(&dictionary);
		BufferBit_destroy(&output_codes);

		free(input_data);
		free(alphabet);
	}
}