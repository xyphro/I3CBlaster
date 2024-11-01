#ifndef UCLI_H_INCLUDED
#define UCLI_H_INCLUDED

/*
Based on Microrl library
Full credits go to the original Author:
    Eugene Samoylov aka Helius (ghelius@gmail.com)
Extended it by in-line editing, supporting 2 terminal emulations
*/

#include <stdint.h>
#include <stdbool.h>
#include "ucli_helpers.h"

#define UCLI_INT_ARG_DEFAULT ((intptr_t)-1)
#define UCLI_STR_ARG_DEFAULT ((const char*)0)

typedef void(*ucli_cmd_handler_t)(const void*);

typedef enum
{
    // An integer argument (of type "intptr_t")
    UCLI_ARG_TYPE_INT,
    // A string argument
    UCLI_ARG_TYPE_STR,
} ucli_arg_type_t;

typedef struct
{
    // The name of the argument
    const char* name;
    // A description of the argument for display by the "help" command
    const char* desc;
    // The type of the argument
    ucli_arg_type_t type;
    // Whether or not the arg is optional (can ONLY be set for the last argument)
    bool is_optional;
} ucli_arg_def_t;

typedef struct
{
    // The name of the command (must be a valid C symbol name, consist of only lower-case alphanumeric characters, and be unique)
    const char* name;
    // A description of the command for display by the "help" command
    const char* desc;
    // The command handler
    ucli_cmd_handler_t handler;
    // List of argument definitions
    const ucli_arg_def_t* args;
    // The number of arguments
    uint32_t num_args;
    // A pointer which the console library uses internally to store arguments for this command
    void** args_ptr;
} ucli_command_def_t;

// Defines a ucli command
#define UCLI_COMMAND_DEF(CMD, DESC, ...) \
    typedef struct { \
        _UCLI_MAP(_UCLI_ARG_TYPE_WITH_DESC_HELPER, ##__VA_ARGS__) \
        const void* const __dummy; /* dummy entry so the struct isn't empty */ \
    } CMD##_args_t; \
    _Static_assert(sizeof(CMD##_args_t) == (_UCLI_NUM_ARGS(__VA_ARGS__) + 1) * sizeof(void *), \
        "Compiler created *_args_t struct of unexpected size"); \
    static void CMD##_command_handler(const CMD##_args_t* args); \
    static const ucli_arg_def_t _##CMD##_ARGS_DEF[] = { \
        _UCLI_MAP(_UCLI_ARG_DEF_WITH_DESC_HELPER, ##__VA_ARGS__) \
    }; \
    static void* _##CMD##_ARGS[_UCLI_NUM_ARGS(__VA_ARGS__)]; \
    static const ucli_command_def_t _##CMD##_DEF = { \
        .name = #CMD, \
        .desc = DESC, \
        .handler = (ucli_cmd_handler_t)CMD##_command_handler, \
        .args = _##CMD##_ARGS_DEF, \
        .num_args = sizeof(_##CMD##_ARGS_DEF) / sizeof(ucli_arg_def_t), \
        .args_ptr = _##CMD##_ARGS, \
    }; \
    static const ucli_command_def_t* const CMD = &_##CMD##_DEF;\
    static void CMD##_command_handler(const CMD##_args_t* args)


// Defines an integer argument of a console command
#define UCLI_INT_ARG_DEF(NAME, DESC) (NAME, DESC, UCLI_ARG_TYPE_INT, false, intptr_t)

// Defines a string argument of a console command
#define UCLI_STR_ARG_DEF(NAME, DESC) (NAME, DESC, UCLI_ARG_TYPE_STR, false, const char*)

// Defines an optional integer argument of a console command (can ONLY be used for the last argument)
// The argument will have a value of `CONSOLE_INT_ARG_DEFAULT` when not specified
#define UCLI_OPTIONAL_INT_ARG_DEF(NAME, DESC) (NAME, DESC, UCLI_ARG_TYPE_INT, true, intptr_t)

// Defines an optional string argument of a console command (can ONLY be used for the last argument)
// The argument will have a value of `CONSOLE_STR_ARG_DEFAULT` when not specified
#define UCLI_OPTIONAL_STR_ARG_DEF(NAME, DESC) (NAME, DESC, UCLI_ARG_TYPE_STR, true, const char*)

// Initialize CLI (prints also welcome message)
void ucli_init(void);

// Registers a console command with the console library (returns true on success)
bool ucli_cmd_register(const ucli_command_def_t* cmd);

// print an error message to console
void ucli_error(const char *errmsg);

// process a received character
void ucli_process(char ch);

#endif // UCLI_H_INCLUDED
