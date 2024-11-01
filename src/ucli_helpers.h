#ifndef UCLI_HELPERS_H_INCLUDED
#define UCLI_HELPERS_H_INCLUDED

/*
Based on Microrl library
Full credits go to the original Author:
    Eugene Samoylov aka Helius (ghelius@gmail.com)
Extended it by in-line editing, supporting 2 terminal emulations
*/

// The _UCLI_NUM_ARGS(...) macro returns the number of arguments passed to it (0-10)
#define _UCLI_NUM_ARGS(...) _UCLI_NUM_ARGS_HELPER1(_, ##__VA_ARGS__, _UCLI_NUM_ARGS_SEQ())
#define _UCLI_NUM_ARGS_HELPER1(...) _UCLI_NUM_ARGS_HELPER2(__VA_ARGS__)
#define _UCLI_NUM_ARGS_HELPER2(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,N,...) N
#define _UCLI_NUM_ARGS_SEQ() 9,8,7,6,5,4,3,2,1,0
_Static_assert(_UCLI_NUM_ARGS() == 0, "_UCLI_NUM_ARGS() != 0");
_Static_assert(_UCLI_NUM_ARGS(1) == 1, "_UCLI_NUM_ARGS(1) != 1");
_Static_assert(_UCLI_NUM_ARGS(1,2,3,4,5,6,7,8,9,10) == 10, "_UCLI_NUM_ARGS(<10_args>) != 10");

// General helper macro for concatentating two tokens
#define _UCLI_CONCAT(A, B) _UCLI_CONCAT2(A, B)
#define _UCLI_CONCAT2(A, B) A ## B

// Helper macro for calling a given macro for each of 0-10 arguments
#define _UCLI_MAP(X, ...) _UCLI_MAP_HELPER(_UCLI_CONCAT(_UCLI_MAP_, _UCLI_NUM_ARGS(__VA_ARGS__)), X, ##__VA_ARGS__)
#define _UCLI_MAP_HELPER(C, X, ...) C(X, ##__VA_ARGS__)
#define _UCLI_MAP_0(X)
#define _UCLI_MAP_1(X,_1) X(_1)
#define _UCLI_MAP_2(X,_1,_2) X(_1) X(_2)
#define _UCLI_MAP_3(X,_1,_2,_3) X(_1) X(_2) X(_3)
#define _UCLI_MAP_4(X,_1,_2,_3,_4) X(_1) X(_2) X(_3) X(_4)
#define _UCLI_MAP_5(X,_1,_2,_3,_4,_5) X(_1) X(_2) X(_3) X(_4) X(_5)
#define _UCLI_MAP_6(X,_1,_2,_3,_4,_5,_6) X(_1) X(_2) X(_3) X(_4) X(_5) X(_6)
#define _UCLI_MAP_7(X,_1,_2,_3,_4,_5,_6,_7) X(_1) X(_2) X(_3) X(_4) X(_5) X(_6) X(_7)
#define _UCLI_MAP_8(X,_1,_2,_3,_4,_5,_6,_7,_8) X(_1) X(_2) X(_3) X(_4) X(_5) X(_6) X(_7) X(_8)
#define _UCLI_MAP_9(X,_1,_2,_3,_4,_5,_6,_7,_8,_9) X(_1) X(_2) X(_3) X(_4) X(_5) X(_6) X(_7) X(_8) X(_9)
#define _UCLI_MAP_10(X,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10) X(_1) X(_2) X(_3) X(_4) X(_5) X(_6) X(_7) X(_8) X(_9) X(_10)

// Helper macros for defining console_arg_def_t instances.
#define _UCLI_ARG_DEF_HELPER(...) _UCLI_ARG_DEF_HELPER2 __VA_ARGS__
#define _UCLI_ARG_DEF_HELPER2(NAME, DESC, ENUM_TYPE, IS_OPTIONAL, C_TYPE) \
    { .name = #NAME, .type = ENUM_TYPE, .is_optional = IS_OPTIONAL },
#define _UCLI_ARG_DEF_WITH_DESC_HELPER(...) _UCLI_ARG_DEF_WITH_DESC_HELPER2 __VA_ARGS__
#define _UCLI_ARG_DEF_WITH_DESC_HELPER2(NAME, DESC, ENUM_TYPE, IS_OPTIONAL, C_TYPE) \
    { .name = #NAME, .desc = DESC, .type = ENUM_TYPE, .is_optional = IS_OPTIONAL },

// Helper macros for defining command *_arg_t types.
#define _UCLI_ARG_TYPE_WITH_DESC_HELPER(...) _UCLI_ARG_TYPE_WITH_DESC_HELPER2 __VA_ARGS__
#define _UCLI_ARG_TYPE_WITH_DESC_HELPER2(NAME, DESC, ENUM_TYPE, IS_OPTIONAL, C_TYPE) \
    C_TYPE NAME;

#endif

