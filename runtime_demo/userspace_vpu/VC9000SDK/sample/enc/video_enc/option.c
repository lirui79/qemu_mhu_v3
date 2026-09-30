#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "error.h"
#include "option.h"
#include "math.h"
#include <dlfcn.h>
#include <string.h>
#include <ctype.h>

static int long_option(int argc, char **argv, struct option_t *option,
                       struct parameter *parameter, char **p);
static int short_option(int argc, char **argv, struct option_t *option,
                        struct parameter *parameter, char **p);
static int parse(int argc, char **argv, struct option_t *option,
                 struct parameter *parameter, char **p, unsigned int lenght);
static int get_next(int argc, char **argv, struct parameter *parameter, char **p);

static int isdigitstr(char *str);

/*------------------------------------------------------------------------------
  get_option parses command line options. This function should be called
  with argc and argv values that are parameters of main(). The function
  will parse the next parameter from the command line. The function
  returns the next option character and stores the current place to
  structure argument. Structure option contain valid options
  and structure parameter contains parsed option.

  option options[] = {
    {"help",           'H', 0}, // No argument
    {"input",          'i', 1}, // Argument is compulsory
    {"output",         'o', 2}, // Argument is optional
    {NULL,              0,  0}  // Format of last line
  }

  Command line format can be
  --input filename
  --input=filename
  --inputfilename
  -i filename
  -i=filename
  -ifilename

  Input argc  Argument count as passed to main().
    argv  Argument values as passed to main().
    option  Valid options and argument requirements structure.
    parameter Option and argument return structure.

  Return  1 Unknown option.
    0 Option and argument are OK.
    -1  No more options.
    -2  Option match but argument is missing.
------------------------------------------------------------------------------*/
int get_option(int argc, char **argv, struct option_t *option,
               struct parameter *parameter)
{
  char *p = NULL;
  int ret;

  parameter->argument = "?";
  parameter->short_opt = '?';
  parameter->enable = 0;

  if (get_next(argc, argv, parameter, &p))
  {
    return -1;  /* End of options */
  }

  /* Long option */
  ret = long_option(argc, argv, option, parameter, &p);
  if (ret != 1) return ret;

  /* Short option */
  ret = short_option(argc, argv, option, parameter, &p);
  if (ret != 1)  return ret;

  /* This is unknow option but option anyway so argument must return */
  parameter->argument = p;

  return 1;
}

/*------------------------------------------------------------------------------
  long_option
------------------------------------------------------------------------------*/
int long_option(int argc, char **argv, struct option_t *option,
                struct parameter *parameter, char **p)
{
  int i = 0;
  unsigned int length;

  if (strncmp("--", *p, 2) != 0)
  {
    return 1;
  }

  while (option[i].long_opt != NULL)
  {
    length = strlen(option[i].long_opt) > strlen(*p + 2) ? strlen(option[i].long_opt) : strlen(*p + 2);
    if (strncmp(option[i].long_opt, *p + 2, length) == 0)
    {
      goto match;
    }
    i++;
  }
  return 1;

match:
  length += 2;    /* Because option start -- */
  if (parse(argc, argv, &option[i], parameter, p, length) != 0)
  {
    return -2;
  }

  return 0;
}

/*------------------------------------------------------------------------------
  short_option
------------------------------------------------------------------------------*/
int short_option(int argc, char **argv, struct option_t *option,
                 struct parameter *parameter, char **p)
{
  int i = 0;
  char short_opt;
  // printf("short_option, *p %s.\n", *p);
  if (strncmp("-", *p, 1) != 0)
  {
    return 1;
  }

  //strncpy(&short_opt, *p + 1, 1);
  short_opt = *(*p + 1);
  // printf("short_option, short_opt %c, long_opt %s.\n", short_opt, option[i].long_opt);
  while (option[i].long_opt != NULL)
  {
    if (option[i].short_opt  == short_opt)
    {
      goto match;
    }
    i++;
  }
  return 1;

match:
  if (parse(argc, argv, &option[i], parameter, p, 2) != 0)
  {
    return -2;
  }

  return 0;
}

/*------------------------------------------------------------------------------
  parse
------------------------------------------------------------------------------*/
int parse(int argc, char **argv, struct option_t *option,
          struct parameter *parameter, char **p, unsigned int lenght)
{
  char *arg;

  parameter->short_opt = option->short_opt;
  parameter->longOpt = option->long_opt;
  arg = *p + lenght;

  /* Argument and option are together */
  if (strlen(arg) != 0)
  {
    /* There should be no argument */
    if (option->enable == 0)
    {
      return -1;
    }

    /* Remove = */
    if (strncmp("=", arg, 1) == 0)
    {
      arg++;
    }
    parameter->enable = 1;
    parameter->argument = arg;
    return 0;
  }

  /* Argument and option are separately */
  if (get_next(argc, argv, parameter, p))
  {
    /* There is no more parameters */
    if (option->enable == 1)
    {
      parameter->enable = 1;
      return -1;
    }
    return 0;
  }

  /* Parameter is missing if next start with "-" but next time this
   * option is OK so we must fix parameter->cnt */
  if (strncmp("-", *p,  1) == 0 && !isdigitstr(*p + 1))
  {
    parameter->cnt--;
    if (option->enable == 1)
    {
      parameter->enable = 1;
      return -1;
    }
    return 0;
  }

  /* There should be no argument */
  if (option->enable == 0)
  {
    return -1;
  }

  parameter->enable = 1;
  parameter->argument = *p;

  return 0;
}

/*------------------------------------------------------------------------------
  get_next
------------------------------------------------------------------------------*/
int get_next(int argc, char **argv, struct parameter *parameter, char **p)
{
  /* End of options */
  if ((parameter->cnt >= argc) || (parameter->cnt < 0))
  {
    return -1;
  }
  *p = argv[parameter->cnt];
  parameter->cnt++;

  return 0;
}
/*------------------------------------------------------------------------------
isdigitstr
------------------------------------------------------------------------------*/
int isdigitstr(char *str) {
  int len = strlen(str);
  int i = 0;
  for (i = 0; i < len; i++) {
    if (!(isdigit(str[i])))
      return 0;
  }
  return 1;
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
int ParseDelim(char *optArg, char delim)
{
  int i;

  for (i = 0; i < (int)strlen(optArg); i++)
    if (optArg[i] == delim)
    {
      optArg[i] = 0;
      return i;
    }

  return -1;
}

int HasDelim(char *optArg, char delim)
{
  int i;

  for (i = 0; i < (int)strlen(optArg); i++)
    if (optArg[i] == delim)
    {
      return 1;
    }

  return 0;
}
