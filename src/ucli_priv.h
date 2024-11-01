#ifndef UCLI_PRIV_H_INCLUDED
#define UCLI_PRIV_H_INCLUDED

#include <stdint.h>

#define KEY_LEFT      (0x81)  // 1b [D
#define KEY_RIGHT     (0x82)  // 1b [C
#define KEY_UP        (0x84)  // 1b [A   pgup: 1b [5~
#define KEY_DN        (0x85)  // 1b [B   pgdn: 1b [6~
#define KEY_DELRIGHT  (0x86)  // 1b [3~
#define KEY_HOME      (0x87)  // 1b [1~
#define KEY_END       (0x88)  // 1b [4~
#define KEY_WORDLEFT  (0x8f)  // 1b OD
#define KEY_INS       (0x8e)  // 1b [2~
#define KEY_WORDRIGHT (0x90)  // 1b OC

#define KEY_ENTER     (0x0d)  // 0x0d
#define KEY_TAB       (0x09)  // 1b [2~
#define KEY_DEL       (0x7f)  // 0x7f
#define KEY_CTRL_C    (0x03)  // 0x03
#define KEY_CTRL_V    (0x16)  // 0x16

#define KEY_F1_TT     (0x91)
#define KEY_F1_PUTTY  (0x92)

#define ESC_F1_TT      "\033OP"
#define ESC_F1_PUTTY   "\033[11~]"

#define ESC_LEFT         "\033[D"
#define ESC_RIGHT        "\033[C"
#define ESC_SAVE_CURSOR  "\033[s"
#define ESC_RESTORE_CURSOR  "\033[u"
#define ESC_HIDE_CURSOR  "\033[?25l"
#define ESC_SHOW_CURSOR  "\033[?25h"
#define ESC_DELRIGHT     "\033[3~"
#define ESC_SET80     "\033[?31"




#endif // UCLI_PRIV_H_INCLUDED
