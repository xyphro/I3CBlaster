#include "ucli.h"
#include "ucli_priv.h"
#include "ucli_config.h"
#include <stdbool.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include "XiaoNeoPixel.h"


/*
Based on Microrl library
Full credits go to the original Author:
    Eugene Samoylov aka Helius (ghelius@gmail.com)
Extended it by in-line editing, supporting 2 terminal emulations
*/

extern void ucli_print(const char *str);

typedef union {
    intptr_t value_int;
    const char* value_str;
    void* ptr;
} parsed_arg_t;

UCLI_COMMAND_DEF(help, "List all commands, or give details about a specific command",
    UCLI_OPTIONAL_STR_ARG_DEF(command, "The name of the command to give details about")
);

static char s_ucli_linebuf[UCLI_MAXLINELEN+1];
static uint32_t s_ucli_linebuflen=0;
static char s_historybuf[UCLI_MAXLINELEN+1];
static uint32_t s_historybuflen=0;
static uint32_t s_ucli_cpos=0; // cursor position
static bool ucli_echooff = false;

static void s_ucli_printchar(char ch)
{
    char str[2];
    str[0] = ch;
    str[1] = '\0';
    ucli_print(str);
}

static uint8_t esc_decode_state;
static bool    esc_decode_waittilde;
static char s_ucli_process_escape(char ch)
{
    char ret;
    static char potentialkey;
    ret = '\0';
    if (esc_decode_state == 0)
    {
        if ((uint8_t)ch == 0x1b)
            esc_decode_state = 1;
        else
            ret = ch;
    }
    else if (esc_decode_state == 1)
    {
        if (ch == '[')
            esc_decode_state = 2;
        else if (ch == 'O')
            esc_decode_state = 4;
        else
            esc_decode_state = 0;
    }
    else if (esc_decode_state == 4)
    {
        if (ch == 'D')
            ret = KEY_WORDLEFT;
        else if (ch == 'C')
            ret = KEY_WORDRIGHT;
        else if (ch == 'P')
            ret = KEY_F1_TT;
        esc_decode_state = 0;
    }
    else if (esc_decode_state == 2)
    {
        potentialkey = '\0';
        switch(ch)
        {
            case 'D' : ret = KEY_LEFT;  break;
            case 'C' : ret = KEY_RIGHT; break;
            case 'A' : ret = KEY_UP;    break;
            case 'B' : ret = KEY_DN;    break;
            case '3' : ret = KEY_DELRIGHT;  esc_decode_waittilde = true; break;
            case '1' : potentialkey = KEY_HOME; esc_decode_state = 3; esc_decode_waittilde = true; break;
            case '2' : ret = KEY_INS;       esc_decode_waittilde = true; break;
            case '4' : ret = KEY_END;       esc_decode_waittilde = true; break;
            case '5' : ret = KEY_UP;        esc_decode_waittilde = true; break;
            case '6' : ret = KEY_DN;        esc_decode_waittilde = true; break;
            case '7' : // this triggers e.g. on function keys
            case '8' :
            case '9' :
                esc_decode_waittilde = true;
                break;
            default: // stop decoding inc case of unknown ESC code
                esc_decode_state = 0;
                esc_decode_waittilde = false;
        }
        if (ret != '\0')
        {
            if (esc_decode_waittilde)
                esc_decode_state = 3;
            else
                esc_decode_state = 0;
        }
    }
    else if (esc_decode_waittilde)
    {
        if (potentialkey)
        {
            if (ch == '1') // check if key is putty-F1 or VT100-HOME
                ret = KEY_F1_PUTTY;
            else if (ch == '~')
                ret = KEY_HOME;
        }
        if (ch == '~')
        {
            esc_decode_state = 0;
            esc_decode_waittilde = false;
        }
    }
    return ret;
}

static uint32_t m_num_commands;
static ucli_command_def_t m_commands[UCLI_MAX_COMMANDS];

static bool validate_arg_def(const ucli_arg_def_t* arg, bool is_last) {
    switch (arg->type) {
        case UCLI_ARG_TYPE_INT:
        case UCLI_ARG_TYPE_STR:
            return arg->name && (!arg->is_optional || is_last);
        default:
            return false;
    }
}
static const ucli_command_def_t* get_command(const char* name) {
    for (uint32_t i = 0; i < m_num_commands; i++) {
        if (!strcmp(m_commands[i].name, name)) {
            return &m_commands[i];
        }
    }
    return (ucli_command_def_t*)0;
}

static bool parse_arg(const char* arg_str, ucli_arg_type_t type, parsed_arg_t* parsed_arg) {
    switch (type) {
        case UCLI_ARG_TYPE_INT: {
            char* end_ptr = (char*)0;
            long int result = strtol(arg_str, &end_ptr, 0);
            if (result == LONG_MAX || result == LONG_MIN || end_ptr == 0 || *end_ptr != '\0') {
                return false;
            }
            parsed_arg->value_int = (intptr_t)result;
            return true;
        }
        case UCLI_ARG_TYPE_STR:
            parsed_arg->value_str = arg_str;
            return true;
        default:
            // should never get here
            return false;
    }
}

static uint32_t get_num_required_args(const ucli_command_def_t* cmd)
{
    if (cmd->num_args == 0) {
        return 0;
    }
    const ucli_arg_def_t* last_arg = &cmd->args[cmd->num_args-1];
    switch (last_arg->type) {
        case UCLI_ARG_TYPE_INT:
        case UCLI_ARG_TYPE_STR:
            if (last_arg->is_optional) {
                return cmd->num_args - 1;
            } else {
                return cmd->num_args;
            }
        default:
            return 0;
    }
}

static void print_idendet(uint32_t maxwidth, uint32_t ident, const char *str)
{
    uint32_t ln = strlen(str);
    uint32_t maxlen;
    uint32_t spos, slen;
    bool first = true;
    spos = 0;
    maxlen = maxwidth - ident;

    do
    {
        slen = ln; // calculate end position of string;
        if (slen > maxlen)
            slen = maxlen;
        // here: print from spos slen characters
        if (!first)
        {
            for (uint32_t j = 0; j <ident; j++) {
                ucli_print(" ");
            }
        }
        first = false;
        if (str[spos] == ' ') // if next line starts with a blank, skip it for optical reasons
        { 
            spos++;
            slen--;
            ln--;
        }
        printf("%.*s", slen, &(str[spos]));
        ln -= slen;
        spos += slen;
        if (ln > 0)
        {
            ucli_print("\r\n");
        }
    }
    while (ln > 0);
}


static void help_command_handler(const help_args_t* args)
{
    if (args->command != UCLI_STR_ARG_DEFAULT)
        {
        const ucli_command_def_t* cmd_def = get_command(args->command);
        if (!cmd_def)
        {
            ucli_print("\x1b[30;41mERROR: Unknown command (");
            ucli_print(args->command);
            ucli_print(")\x1b[0m\r\n");
            return;
        }
        if (cmd_def->desc) {
            ucli_print("\x1b[1m");
            ucli_print(cmd_def->desc);
            ucli_print("\x1b[0m\r\n");
        }
        ucli_print("\x1b[4mUsage:\x1b[0m ");
        ucli_print(args->command);
        uint32_t max_name_len = 0;
        for (uint32_t i = 0; i < cmd_def->num_args; i++)
        {
            const ucli_arg_def_t* arg_def = &cmd_def->args[i];
            const uint32_t name_len = strlen(arg_def->name);
            if (name_len > max_name_len) {
                max_name_len = name_len;
            }
            if (arg_def->is_optional) {
                ucli_print(" [");
            } else {
                ucli_print(" ");
            }
            ucli_print(cmd_def->args[i].name);
            if (arg_def->is_optional) {
                ucli_print("]");
            }
        }
        ucli_print("\r\n");
        for (uint32_t i = 0; i < cmd_def->num_args; i++)
        {
            const ucli_arg_def_t* arg_def = &cmd_def->args[i];
            ucli_print("  ");
            ucli_print("\x1b[1m");
            ucli_print(arg_def->name);
            ucli_print("\x1b[0m");
            if (arg_def->desc) {
                // pad the description so they all line up
                const uint32_t name_len = strlen(arg_def->name);
                for (uint32_t j = 0; j < max_name_len - name_len; j++) {
                    ucli_print(" ");
                }
                ucli_print(" - ");
                //ucli_print(arg_def->desc);
                print_idendet(80, max_name_len+5, arg_def->desc);
            }
            ucli_print("\r\n");
        }
    } else {
        ucli_print("\x1b[4mAvailable commands:\x1b[0m\r\n");
        // get the max name length for padding
        uint32_t max_name_len = 0;
        for (uint32_t i = 0; i < m_num_commands; i++)
        {
            const ucli_command_def_t* cmd_def = &m_commands[i];
            const uint32_t name_len = strlen(cmd_def->name);
            if (name_len > max_name_len) {
                max_name_len = name_len;
            }
        }
        for (uint32_t i = 0; i < m_num_commands; i++) {
            const ucli_command_def_t* cmd_def = &m_commands[i];
            ucli_print("  ");
            ucli_print(cmd_def->name);
            if (cmd_def->desc) {
                // pad the description so they all line up
                const uint32_t name_len = strlen(cmd_def->name);
                for (uint32_t j = 0; j < max_name_len - name_len; j++) {
                    ucli_print(" ");
                }
                ucli_print(" - ");
                print_idendet(80, max_name_len+5, cmd_def->desc);
            }
            ucli_print("\r\n");
        }
    }
}


void ucli_init(void)
{
    esc_decode_state = 0;
    esc_decode_waittilde = false;

    ucli_cmd_register(help);

    ucli_print("\033[2J"); // clear screen
    ucli_print("\x1b[0m"); // default colours/text formating
    ucli_print("\033[H");
    ucli_print("\033[2J");
    ucli_print("\033[?3l");
    ucli_print(ESC_HIDE_CURSOR);
    ucli_print(UCLI_WELCOMEMSG);
    ucli_print(UCLI_PROMPT_STR);
    ucli_print(ESC_SHOW_CURSOR);

    ucli_echooff = false;

    s_ucli_linebuflen = 0;
    s_historybuflen=0;
}

void s_ucli_setcursor(uint32_t ps)
{
    if (ps == s_ucli_cpos)
        return;
    if (ps > s_ucli_linebuflen)
        ps = s_ucli_linebuflen;
    if (ps < s_ucli_cpos)
    {
        ucli_print(ESC_HIDE_CURSOR);
        while (s_ucli_cpos > ps)
        {
            s_ucli_cpos--;
            s_ucli_printchar('\b');
        }
        ucli_print(ESC_SHOW_CURSOR);
    }
    else
    { // move cursor right
        ucli_print(ESC_HIDE_CURSOR);
        while (s_ucli_cpos < ps)
        {
            s_ucli_printchar(s_ucli_linebuf[s_ucli_cpos]);
            s_ucli_cpos++;
        }
        ucli_print(ESC_SHOW_CURSOR);
    }
}


static void ucli_dispatch(void)
{
    // parse and validate the line
    const ucli_command_def_t* cmd = (ucli_command_def_t*)0;
    uint32_t arg_index = 0;
    const char* current_token = (const char*)0;
    for (uint32_t i = 0; i <= s_ucli_linebuflen; i++)
	{
        const char c = s_ucli_linebuf[i];
        if (c == ' ' || c == '\0')
		{
            // end of a token
            if (!current_token)
			{
                if (cmd)
				{
                    // too much whitespace
                    ucli_print("\x1b[30;41mERROR: Extra whitespace between arguments\x1b[0m\r\n");
                    return;
                }
				else
				{
                    // empty line - silently fail
                    return;
                }
            }
            // process this token
            s_ucli_linebuf[i] = '\0';
            if (!cmd)
			{
                // find the command
                cmd = get_command(current_token);
                if (!cmd)
				{
                    ucli_print("\x1b[30;41mERROR: Command not found (");
                    ucli_print(current_token);
                    ucli_print(")\x1b[0m\r\n");
                    return;
                }
            }
			else
			{
                // this is an argument
                if (arg_index == cmd->num_args)
				{
                    ucli_print("\x1b[30;41mERROR: Too many arguments\x1b[0m\r\n");
                    return;
                }
                // validate the argument
                const ucli_arg_def_t* arg = &cmd->args[arg_index];
                parsed_arg_t parsed_arg;
                if (!parse_arg(current_token, arg->type, &parsed_arg))
				{
                    ucli_print("\x1b[30;41mERROR: Invalid value for '");
                    ucli_print(arg->name);
                    ucli_print("' (");
                    ucli_print(current_token);
                    ucli_print(")\x1b[0m\r\n");
                    return;
                }
                cmd->args_ptr[arg_index] = parsed_arg.ptr;
                arg_index++;
            }
            current_token = (const char*)0;
        }
		else if (!current_token)
		{
            current_token = &s_ucli_linebuf[i];
        }
    }

    if (!cmd) {
        // nothing entered - silently fail
        return;
    } else if (arg_index < get_num_required_args(cmd)) {
        ucli_print("\x1b[30;41mERROR: Too few arguments\x1b[0m\r\n");
        return;
    }

    if (arg_index != cmd->num_args) {
        // set the optional argument to its default value
        switch (cmd->args[arg_index].type) {
            case UCLI_ARG_TYPE_INT:
                cmd->args_ptr[arg_index] = (void*)UCLI_INT_ARG_DEFAULT;
                break;
            case UCLI_ARG_TYPE_STR:
                cmd->args_ptr[arg_index] = (void*)UCLI_STR_ARG_DEFAULT;
                break;
        }
    }

    // run the handler
    arg_index = 0;
    cmd->handler(cmd->args_ptr);
}

// returns -1 if string was not updated
// returns >= 0 if string was updated - this is the cursor position from where onwards the string updated
int32_t s_ucli_tab_extension(void)
{
    uint32_t spos = s_ucli_linebuflen;
    bool done = false;
    uint32_t ln;
    const char *foundstr      = (void*)0;

    if (s_ucli_cpos != s_ucli_linebuflen) // this only works at the end of the string. Skip if cursor is not at the end
        return -1;

    // Step 1: determine startposition of current linebuffer string element (beginning of string or after last whitespace)
    // spos contains the character position of the start character
    while ( (spos > 0) && !done )
    {
        if (s_ucli_linebuf[spos-1] == ' ')
            done = true;
        else
            spos--;
    }

    // search matching command name(s)
    ln = 0;
    if ( spos < s_ucli_linebuflen )
    {
        for (uint32_t i = 0; i < m_num_commands; i++) 
        {
            if ( strncasecmp(&s_ucli_linebuf[spos], m_commands[i].name, strlen(&s_ucli_linebuf[spos])) == 0 )
            {
                if (!foundstr)
                    foundstr = m_commands[i].name;
                else
                {
                    uint32_t lt = 0;
                    while ( (lt < strlen(foundstr)) && (lt < strlen(m_commands[i].name)) && (foundstr[lt] == m_commands[i].name[lt]) )
                    {
                        lt++;
                    }
                    ln = lt;
                }
            }
        }
        if (foundstr)
        {
            if (ln == 0)
                ln = strlen(foundstr);
            for (uint32_t i=0; (i<ln) && ((spos+i) < UCLI_MAXLINELEN); i++)
            {
                s_ucli_linebuf[spos+i] = foundstr[i];
                s_ucli_linebuf[spos+i+1] = '\0';
                s_ucli_linebuflen = spos+i+1;
            }
            return spos;
        }
    }
    return -1;
}

static bool terminalemulationmode_putty = true;
void ucli_process(char ch)
{
    //printf(" %c  0x%02x \r\n", ch, (uint32_t)ch);
    if (ch == 0xff)
        return;
    if ((ch == '@') && (s_ucli_linebuflen == 0))
    {
        ucli_echooff = true;
        return; // don't add @ to linebuffer
    }

    ch = s_ucli_process_escape(ch);
    if (ch == '\0')
        return;

    // handle backspace / del key according to terminal emulation
    if ( !terminalemulationmode_putty )
    {
        if (ch == 0x7f)
            ch = KEY_DELRIGHT;
    }
    if (ch == 0x08)
        ch = 0x7f;

    //printf(" ch= 0x%02x \r\n", (uint32_t)ch);

    if ((ch >= ' ') && ((uint8_t)ch < 0x7f) )
    { // printable character
        if (s_ucli_linebuflen < UCLI_MAXLINELEN)
        {
            if (s_ucli_cpos < s_ucli_linebuflen)
            { // insert character
                uint32_t len = s_ucli_linebuflen-s_ucli_cpos;
                memmove(&s_ucli_linebuf[s_ucli_cpos+1], &s_ucli_linebuf[s_ucli_cpos], len+1);
                s_ucli_linebuf[s_ucli_cpos++] = ch;
                s_ucli_linebuflen++;
                if (!ucli_echooff)
                {
                    s_ucli_printchar(ch);
                    ucli_print(ESC_SAVE_CURSOR);
                    ucli_print(ESC_HIDE_CURSOR);
                    ucli_print(&s_ucli_linebuf[s_ucli_cpos]);
                    ucli_print(ESC_RESTORE_CURSOR);
                    ucli_print(ESC_SHOW_CURSOR);
                }
            }
            else
            { // append character
                s_ucli_linebuf[s_ucli_linebuflen++] = ch;
                if (!ucli_echooff)
                    s_ucli_printchar(ch);
                s_ucli_cpos++;
            }
        }
    }
    else
    {
        switch ((uint8_t)ch)
        {
            case KEY_WORDLEFT :
                {
                    uint32_t searchpos = s_ucli_cpos;
                    bool     done = false;
                    while (!done && (searchpos > 0))
                    {
                        searchpos--;
                        if (searchpos > 0)
                        {
                            if ( ((s_ucli_linebuf[searchpos] != ' ')) && (s_ucli_linebuf[searchpos-1] == ' ') )
                                done = true;

                        }
                    }
                    s_ucli_setcursor(searchpos);
                }
                break;
            case KEY_LEFT     :
                if (s_ucli_cpos > 0)
                    s_ucli_setcursor(s_ucli_cpos-1);
                break;
            case KEY_WORDRIGHT :
                {
                    uint32_t searchpos = s_ucli_cpos;
                    bool     done = false;
                    while (!done && (searchpos < s_ucli_linebuflen))
                    {
                        searchpos++;
                        if (searchpos < s_ucli_linebuflen)
                        {
                            if ( ((s_ucli_linebuf[searchpos+1] != ' ')) && (s_ucli_linebuf[searchpos] == ' ') )
                            {
                                done = true;
                                searchpos++;
                            }
                        }
                    }
                    s_ucli_setcursor(searchpos);
                }
                break;
            case KEY_RIGHT    :
                s_ucli_setcursor(s_ucli_cpos+1);
                break;
            case KEY_UP       : 
                if (s_historybuflen > 0)
                {
                    uint32_t prevlen, newlen;
                    prevlen = s_ucli_linebuflen;
                    newlen = s_historybuflen;
                    s_ucli_setcursor(0);
                    memcpy(s_ucli_linebuf, s_historybuf, s_historybuflen+1);                    
                    s_ucli_linebuflen = s_historybuflen;
                    s_ucli_setcursor(0);
                    s_ucli_setcursor(s_ucli_linebuflen);
                    while (prevlen > newlen)
                    {
                        ucli_print(" ");
                        s_ucli_cpos++;
                        prevlen++;
                    }
                    s_ucli_setcursor(s_ucli_linebuflen);
                }
                break;
            case KEY_DN       : break;
            case KEY_DELRIGHT :
                if (s_ucli_cpos < s_ucli_linebuflen)
                { // delete from right only works if we are not at the end of the line
                    uint32_t len = s_ucli_linebuflen-s_ucli_cpos;
                    memmove(&s_ucli_linebuf[s_ucli_cpos], &s_ucli_linebuf[s_ucli_cpos+1], len+1);
                    ucli_print(ESC_SAVE_CURSOR);
                    ucli_print(ESC_HIDE_CURSOR);
                    ucli_print(&s_ucli_linebuf[s_ucli_cpos]);
                    ucli_print(" ");
                    ucli_print(ESC_SHOW_CURSOR);
                    ucli_print(ESC_RESTORE_CURSOR);
                    s_ucli_linebuflen--;
                }
                break;
            case KEY_HOME     :
                s_ucli_setcursor(0);
                break;
            case KEY_END      :
                s_ucli_setcursor(s_ucli_linebuflen);
                break;
            case KEY_INS      : break;
            case KEY_DEL      :
                if (s_ucli_cpos >= s_ucli_linebuflen)
                { // delete from end of text
                    if (s_ucli_linebuflen > 0)
                        s_ucli_linebuflen--;
                    if (s_ucli_cpos > 0)
                    {
                        s_ucli_cpos--;
                        if (!terminalemulationmode_putty)
                        {
                            ucli_print("\b");
                            ucli_print(ESC_SAVE_CURSOR);
                            ucli_print(ESC_HIDE_CURSOR);
                            ucli_print(" ");
                            ucli_print(ESC_RESTORE_CURSOR);
                            ucli_print(ESC_SHOW_CURSOR);
                        }
                        else
                        {
                            s_ucli_printchar(KEY_DEL);
                        }
                    }
                }
                else
                { // delete from middle of test
                    if (s_ucli_cpos > 0)
                    {
                        uint32_t len = s_ucli_linebuflen-s_ucli_cpos;
                        memmove(&s_ucli_linebuf[s_ucli_cpos-1], &s_ucli_linebuf[s_ucli_cpos], len+1);

                        s_ucli_cpos--;
                        s_ucli_linebuflen--;
                        ucli_print("\b");
                        ucli_print(ESC_SAVE_CURSOR);
                        ucli_print(ESC_HIDE_CURSOR);
                        ucli_print(&s_ucli_linebuf[s_ucli_cpos]);
                        ucli_print(" ");
                        ucli_print(ESC_RESTORE_CURSOR);
                        ucli_print(ESC_SHOW_CURSOR);
                    }
                }
                break;

            case KEY_ENTER    :
                memcpy(s_historybuf, s_ucli_linebuf, s_ucli_linebuflen+1);
                s_historybuflen = s_ucli_linebuflen;
                static char s_historybuf[UCLI_MAXLINELEN+1];

                s_ucli_setcursor(s_ucli_linebuflen);
                if (!ucli_echooff)
                    ucli_print("\r\n");
                ucli_dispatch();
                if (!ucli_echooff)
                    ucli_print(UCLI_PROMPT_STR);
                s_ucli_cpos = 0;
                s_ucli_linebuflen = 0;
                ucli_echooff = false;
                break;
            case KEY_TAB:
                { 
                    int32_t spos = s_ucli_tab_extension();
                    if (spos >= 0)
                    {
                        s_ucli_setcursor((uint32_t)spos);
                        s_ucli_setcursor(s_ucli_linebuflen);

                    }
                }
                break;
            case KEY_F1_TT:
                terminalemulationmode_putty = false;
                break;
            case KEY_F1_PUTTY:
                terminalemulationmode_putty = true;
                break;            
        }
    }

    s_ucli_linebuf[s_ucli_linebuflen] = '\0';
}

bool ucli_cmd_register(const ucli_command_def_t* cmd)
{
    if (m_num_commands >= UCLI_MAX_COMMANDS)
        return false;
    // validate the command
    if (!cmd->name || !cmd->handler || strlen(cmd->name) == 0)
        return false;
    // validate the arguments
    for (uint32_t i = 0; i < cmd->num_args; i++)
    {
        if (!validate_arg_def(cmd->args, i + 1 == cmd->num_args))
            return false;
    }
    // make sure it's not already registered
    if (get_command(cmd->name))
        return false;
    // add the command
    m_commands[m_num_commands++] = *cmd;
    return true;
}


void ucli_error(const char *errmsg)
{
    ucli_print("\x1b[30;41mERROR: ");
    ucli_print(errmsg);
    ucli_print("\x1b[0m\r\n");
}
