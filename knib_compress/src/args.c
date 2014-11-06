
#include "args.h"

#include <stdio.h>
#include <stdlib.h>
#include <argp.h>
#include <string.h>
#include <knib_read.h>

const char *argp_program_version = "knib_compress 0.1";
const char *argp_program_bug_address = "chris.stones@gmail.com";
static char doc[] = "knib_compress -- a program to create Knib video files (.kib).";
static char args_doc[] = "INPUT_FILE_FORMAT(in scanf format) OUTPUT_FILE";

static struct argp_option options[] = {

  {"DXT1",     'D', 0,              OPTION_ARG_OPTIONAL,  "Use DXT1 texture compression" },
  {"ETC1",     'E', 0,              OPTION_ARG_OPTIONAL,  "Use ETC1 texture compression" },
  {"LZ4",      'L', 0,              OPTION_ARG_OPTIONAL,  "Use LZ4 file compression" },

  {"quality",         'q', "HI|MED|LO", 0, "Texture compression Quality." },
  {"from-frame",      'f', "FRAME#",    0, "First Frame Number"   },
  {"to-frame",        't', "FRAME#",    0, "Last Frame Number"    },
  {"increment-frame", 'i', "COUNT" ,    0, "Increment Number.(1)" },

  { 0 }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = (struct arguments *)state->input;

  switch (key)
  {
    case 'D':
    	arguments->flags |= KNIB_TEX_DXT1;
    	break;
    case 'E':
    	arguments->flags |= KNIB_TEX_ETC1;
    	break;
    case 'L':
    	arguments->flags |= KNIB_DATA_LZ4;
    	break;
    case 'q':
    	{
    		if(strcasecmp("HI", arg)==0)
    			arguments->quality = COPY_QUALITY_HIGHEST;
    		else if(strcasecmp("MED", arg)==0)
    			arguments->quality = COPY_QUALITY_MEDIUM;
    		else if(strcasecmp("LO", arg)==0)
    			arguments->quality = COPY_QUALITY_LOWEST;
    		else
    			argp_usage (state);
    	}
    	break;
    case 'f':
    	arguments->ff_from = atoi(arg);
    	break;
    case 't':
    	arguments->ff_to = atoi(arg);
    	break;
    case 'i':
    	arguments->ff_inc = atoi(arg);
    	break;

    case ARGP_KEY_ARG:
    	{
		  switch(state->arg_num) {
		  default:
			  argp_usage (state);
			  break;
		  case 0:
			  arguments->ff_string = arg;
			  break;
		  case 1:
			  arguments->output_fn = arg;
			  break;
		  }
		  break;
    	}
    case ARGP_KEY_END:
    {
    	int err=0;
    	if (state->arg_num < 2)
    		err=1;

    	if(arguments->ff_inc==0)
    		err=2;

    	if(!arguments->output_fn)
    		err=3;

    	if(!arguments->ff_string)
    	    err=4;

    	if(!arguments->flags)
    		err=5;

    	if(err)
    		argp_usage (state);

    	break;
    }

    default:
    	return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

struct arguments read_args(int argc, char ** argv ) {

  struct arguments args;

  memset(&args,0,sizeof args);

  // defaults
  args.ff_inc = 1;
  args.quality = COPY_QUALITY_HIGHEST;

  argp_parse (&argp, argc, argv, 0, 0, &args);

  return args;
}

