/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2014 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                 --
--                  on all copies and should not be removed.                                                    --
--                                                                                                                               --
--------------------------------------------------------------------------------*/

#ifndef GET_OPTION_H
#define GET_OPTION_H

struct option_t
{
  char *long_opt;
  char short_opt;
  int enable;
};

struct parameter
{
  int cnt;
  char *argument;
  char short_opt;
  char *longOpt;
  int enable;
};

int get_option(int argc, char **argv, struct option_t *option,
               struct parameter *parameter);

#endif
