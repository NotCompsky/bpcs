#ifndef __COMPSKY_COMPSKY_CL
#define __COMPSKY_COMPSKY_CL
namespace compsky::cl {
#ifdef _WIN32
static const char* CL_PURPLE              = "";
static const char* CL_CYAN                = "";
static const char* CL_DCYAN               = "";
static const char* CL_GREEN               = "";
static const char* CL_YELLOW              = "";
static const char* CL_RED                 = "";
static const char* CL_BOLD                = "";
static const char* CL_UNDERLINED          = "";
static const char* CL_END                 = "";

static const char* CL_WHITE               = "";
static const char* CL_BLUE                = "";
static const char* CL_LBLUE               = "";
static const char* CL_LGREEN              = "";
static const char* CL_LCYAN               = "";
static const char* CL_LRED                = "";
static const char* CL_LPURPLE             = "";
static const char* CL_ORANGE              = "";
static const char* CL_BROWN               = "";
static const char* CL_GREY                = "";
static const char* CL_LGREY               = "";
#else
// Terminal colours
static const char* CL_PURPLE              = "\033[0;0;95m";
static const char* CL_CYAN                = "\033[0;0;96m";
static const char* CL_DCYAN               = "\033[0;0;36m";
static const char* CL_GREEN               = "\033[0;0;92m";
static const char* CL_YELLOW              = "\033[0;0;93m";
static const char* CL_RED                 = "\033[0;0;91m";
static const char* CL_BOLD                = "\033[0;0;1m";
static const char* CL_UNDERLINED          = "\033[0;0;4m";
static const char* CL_END                 = "\033[0;0;0m";

static const char* CL_WHITE               = "\033[1;37m";
static const char* CL_BLUE                = "\033[0;34m";
static const char* CL_LBLUE               = "\033[1;34m";
static const char* CL_LGREEN              = "\033[1;32m";
static const char* CL_LCYAN               = "\033[1;36;40m";
static const char* CL_LRED                = "\033[1;31m";
static const char* CL_LPURPLE             = "\033[1;35m";
static const char* CL_ORANGE              = "\033[0;33;40m";
static const char* CL_BROWN               = "\033[0;0;33m";
static const char* CL_GREY                = "\033[0;0;37m";
static const char* CL_LGREY               = "\033[0;0;37m";

/*
FORMAT: _BG_FG
static const char* CL__BLACK_WHITE        = "\033[0;30;47m";

static const char* CL__BLUE_YELLOW        = "\033[1;33;44m";
static const char* CL__BLUE_WHITE         = "\033[1;37;46m";
static const char* CL__BLUE_RED           = "\033[1;34;41m";

static const char* CL__CYAN_WHITE         = "\033[1;37;46m";

static const char* CL__GREEN_WHITE        = "\033[1;37;42m";

static const char* CL__RED_BLACK          = "\033[7;31;40m";
static const char* CL__RED_BLUE           = "\033[7;31;44m";
static const char* CL__PURPLE_WHITE       = "\033[1;37;45m";
static const char* CL__PURPLE_BLACK       = "\033[6;30;45m";
static const char* CL__PURPLE_PURPLE      = "\033[7;35;44m";
static const char* CL__BLACK_RED          = "\033[7;30;41m";
static const char* CL__ORANGE_BLACK       = "\033[0;30;43m";

static const char* CL__FLASH_ORANGE_BLACK = "\033[5;30;43m";
*/

// Logger colours
static const char* CL_CRIT   = CL_LRED;
static const char* CL_ERR    = CL_RED;
static const char* CL_WARN   = CL_YELLOW;
static const char* CL_INFO   = CL_GREEN;
static const char* CL_DBG    = CL_BLUE;
static const char* CL_TEDIUM = CL_END;
#endif
}
#endif
