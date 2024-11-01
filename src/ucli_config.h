#ifndef UCLI_CONFIG_H_INCLUDED
#define UCLI_CONFIG_H_INCLUDED


#define UCLI_MAXLINELEN     (256)
#define UCLI_MAX_COMMANDS   (32)
#define UCLI_PROMPT_STR     ("> ")

#define UCLI_WELCOMEMSG "+------------------------------------------+\r\n"\
"|   \x1b[32m-----+       +-------->\x1b[37m                |\r\n"\
"|    \x1b[33m+-+ \x1b[32m|  \x1b[33m+-+  \x1b[32m| \x1b[33m+-+                     \x1b[37m|\r\n"\
"|    \x1b[33m| | \x1b[32m|  \x1b[33m| |  \x1b[32m| \x1b[33m| |\x1b[37m                     |\r\n"\
"|    \x1b[33m| | \x1b[32m+--+\x1b[33m-\x1b[32m+--+ \x1b[33m| |                     \x1b[37m|\r\n"\
"|  \x1b[33m -+ +----+ +----+ +---->\x1b[37m                |\r\n"\
"|                                          |\r\n"\
"|   -+- --+ +--                            |\r\n"\
"|    |   -+ |   Blaster                    |\r\n"\
"|   -+- --+ +-- by XyphroLabs              |\r\n"\
"|                                          |\r\n"\
"|   https://github.com/xyphro/I3CBlaster   |\r\n"\
"+------------------------------------------+\r\n" \
"Type \x1b[5mhelp\033[0m to see command list. Press \x1b[5m<F1>\033[0m to autoidentify terminal emulation\r\n"

#endif // UCLI_CONFIG_H_INCLUDED
