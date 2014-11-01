
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct arguments {

	// File format options
	char * ff_string;
	int ff_from;
	int ff_to;
	int ff_inc;

	// Output.
	char * output_fn;

	// File format flags.
	int flags;
};

struct arguments read_args(int argc, char ** argv );

#ifdef __cplusplus
} /* extern "C" { */
#endif

