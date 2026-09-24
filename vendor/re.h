/* NOTE:
 * This is a modified version of tiny-regex-c.
 * It combines the .h and .c files into one .h file
 * to simplify the amalgamation process in sfte.
 * It also narrows the scope of functions (translation unit)
 * to internal implementation using `static internal` prefixes.
 * All credit of this implementation goes to kokke.
 * The original repository is under github.com/kokke/tiny-regex-c
 *
 * Mini regex-module inspired by Rob Pike's regex code described in:
 *
 * http://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html
 *
 *
 *
 * Supports:
 * ---------
 *   '.'        Dot, matches any character
 *   '^'        Start anchor, matches beginning of string
 *   '$'        End anchor, matches end of string
 *   '*'        Asterisk, match zero or more (greedy)
 *   '+'        Plus, match one or more (greedy)
 *   '?'        Question, match zero or one (non-greedy)
 *   '[abc]'    Character class, match if one of {'a', 'b', 'c'}
 *   '[^abc]'   Inverted class, match if NOT one of {'a', 'b', 'c'} -- NOTE: feature is currently
 * broken!
 *   '[a-zA-Z]' Character ranges, the character set of the ranges { a-z | A-Z }
 *   '\s'       Whitespace, \t \f \r \n \v and spaces
 *   '\S'       Non-whitespace
 *   '\w'       Alphanumeric, [a-zA-Z0-9_]
 *   '\W'       Non-alphanumeric
 *   '\d'       Digits, [0-9]
 *   '\D'       Non-digits
 *
 *
 */

#ifndef _TINY_REGEX_C
#define _TINY_REGEX_C

#ifndef RE_DOT_MATCHES_NEWLINE
/* Define to 0 if you DON'T want '.' to match '\r' + '\n' */
#define RE_DOT_MATCHES_NEWLINE 1
#endif

#define MAX_REGEXP_OBJECTS 30 /* Max number of regex symbols in expression. */
#define MAX_CHAR_CLASS_LEN 40 /* Max length of character-class buffer in.   */

#include <ctype.h>
#include <stdio.h>

enum {
    UNUSED,
    DOT,
    BEGIN,
    END,
    QUESTIONMARK,
    STAR,
    PLUS,
    CHAR,
    CHAR_CLASS,
    INV_CHAR_CLASS,
    DIGIT,
    NOT_DIGIT,
    ALPHA,
    NOT_ALPHA,
    WHITESPACE,
    NOT_WHITESPACE,
    /* BRANCH */
};

typedef struct regex_t {
    unsigned char type; /* CHAR, STAR, etc.                      */
    union {
        unsigned char ch;   /*      the character itself             */
        unsigned char *ccl; /*  OR  a pointer to characters in class */
    } u;
} regex_t;

/* Private function declarations: */
static inline int _re_matchpattern(regex_t *pattern, const char *text, int *matchlength);
static inline int _re_matchcharclass(char c, const char *str);
static inline int _re_matchstar(regex_t p, regex_t *pattern, const char *text, int *matchlength);
static inline int _re_matchplus(regex_t p, regex_t *pattern, const char *text, int *matchlength);
static inline int _re_matchone(regex_t p, char c);
static inline int _re_matchdigit(char c);
static inline int _re_matchalpha(char c);
static inline int _re_matchwhitespace(char c);
static inline int _re_matchmetachar(char c, const char *str);
static inline int _re_matchrange(char c, const char *str);
static inline int _re_matchdot(char c);
static inline int _re_ismetachar(char c);

/* Typedef'd pointer to get abstract datatype. */
typedef struct regex_t *re_t;

/* Public functions: */

/* Find matches of the compiled pattern inside text. */
static inline int re_matchp(re_t pattern, const char *text, int *matchlength) {
    *matchlength = 0;
    if (pattern != 0) {
        if (pattern[0].type == BEGIN) {
            return ((_re_matchpattern(&pattern[1], text, matchlength)) ? 0 : -1);
        } else {
            int idx = -1;

            do {
                idx += 1;

                if (_re_matchpattern(pattern, text, matchlength)) {
                    if (text[0] == '\0') return -1;

                    return idx;
                }
            } while (*text++ != '\0');
        }
    }
    return -1;
}

/* Compile regex string pattern to a regex_t-array. */
static inline re_t re_compile(const char *pattern) {
    /* The sizes of the two static arrays below substantiates the static RAM usage of this module.
       MAX_REGEXP_OBJECTS is the max number of symbols in the expression.
       MAX_CHAR_CLASS_LEN determines the size of buffer for chars in all char-classes in the
       expression. */
    static regex_t re_compiled[MAX_REGEXP_OBJECTS];
    static unsigned char ccl_buf[MAX_CHAR_CLASS_LEN];
    int ccl_bufidx = 1;

    char c;    /* current char in pattern   */
    int i = 0; /* index into pattern        */
    int j = 0; /* index into re_compiled    */

    while (pattern[i] != '\0' && (j + 1 < MAX_REGEXP_OBJECTS)) {
        c = pattern[i];

        switch (c) {
        /* Meta-characters: */
        case '^': {
            re_compiled[j].type = BEGIN;
        } break;
        case '$': {
            re_compiled[j].type = END;
        } break;
        case '.': {
            re_compiled[j].type = DOT;
        } break;
        case '*': {
            re_compiled[j].type = STAR;
        } break;
        case '+': {
            re_compiled[j].type = PLUS;
        } break;
        case '?': {
            re_compiled[j].type = QUESTIONMARK;
        } break;
            /*    case '|': {    re_compiled[j].type = BRANCH;          } break; <-- not working
             * properly */

        /* Escaped character-classes (\s \w ...): */
        case '\\': {
            if (pattern[i + 1] != '\0') {
                /* Skip the escape-char '\\' */
                i += 1;
                /* ... and check the next */
                switch (pattern[i]) {
                /* Meta-character: */
                case 'd': {
                    re_compiled[j].type = DIGIT;
                } break;
                case 'D': {
                    re_compiled[j].type = NOT_DIGIT;
                } break;
                case 'w': {
                    re_compiled[j].type = ALPHA;
                } break;
                case 'W': {
                    re_compiled[j].type = NOT_ALPHA;
                } break;
                case 's': {
                    re_compiled[j].type = WHITESPACE;
                } break;
                case 'S': {
                    re_compiled[j].type = NOT_WHITESPACE;
                } break;

                /* Escaped character, e.g. '.' or '$' */
                default: {
                    re_compiled[j].type = CHAR;
                    re_compiled[j].u.ch = pattern[i];
                } break;
                }
            }
            /* '\\' as last char in pattern -> invalid regular expression. */
            /*
                    else
                    {
                      re_compiled[j].type = CHAR;
                      re_compiled[j].ch = pattern[i];
                    }
            */
        } break;

        /* Character class: */
        case '[': {
            /* Remember where the char-buffer starts. */
            int buf_begin = ccl_bufidx;

            /* Look-ahead to determine if negated */
            if (pattern[i + 1] == '^') {
                re_compiled[j].type = INV_CHAR_CLASS;
                i += 1;                  /* Increment i to avoid including '^' in the char-buffer */
                if (pattern[i + 1] == 0) /* incomplete pattern, missing non-zero char after '^' */
                {
                    return 0;
                }
            } else {
                re_compiled[j].type = CHAR_CLASS;
            }

            /* Copy characters inside [..] to buffer */
            while ((pattern[++i] != ']') && (pattern[i] != '\0')) /* Missing ] */
            {
                if (pattern[i] == '\\') {
                    if (ccl_bufidx >= MAX_CHAR_CLASS_LEN - 1) {
                        // fputs("exceeded internal buffer!\n", stderr);
                        return 0;
                    }
                    if (pattern[i + 1] ==
                        0) /* incomplete pattern, missing non-zero char after '\\' */
                    {
                        return 0;
                    }
                    ccl_buf[ccl_bufidx++] = pattern[i++];
                } else if (ccl_bufidx >= MAX_CHAR_CLASS_LEN) {
                    // fputs("exceeded internal buffer!\n", stderr);
                    return 0;
                }
                ccl_buf[ccl_bufidx++] = pattern[i];
            }
            if (ccl_bufidx >= MAX_CHAR_CLASS_LEN) {
                /* Catches cases such as [00000000000000000000000000000000000000][ */
                // fputs("exceeded internal buffer!\n", stderr);
                return 0;
            }
            /* Null-terminate string end */
            ccl_buf[ccl_bufidx++] = 0;
            re_compiled[j].u.ccl = &ccl_buf[buf_begin];
        } break;

        /* Other characters: */
        default: {
            re_compiled[j].type = CHAR;
            re_compiled[j].u.ch = c;
        } break;
        }
        /* no buffer-out-of-bounds access on invalid patterns - see
         * https://github.com/kokke/tiny-regex-c/commit/1a279e04014b70b0695fba559a7c05d55e6ee90b */
        if (pattern[i] == 0) { return 0; }

        i += 1;
        j += 1;
    }
    /* 'UNUSED' is a sentinel used to indicate end-of-pattern */
    re_compiled[j].type = UNUSED;

    return (re_t)re_compiled;
}

/* Find matches of the txt pattern inside text (will compile automatically first). */
static inline int re_match(const char *pattern, const char *text, int *matchlength) {
    return re_matchp(re_compile(pattern), text, matchlength);
}

static inline void re_print(regex_t *pattern) {
    const char *types[] = {"UNUSED",         "DOT",       "BEGIN", "END",        "QUESTIONMARK",
                           "STAR",           "PLUS",      "CHAR",  "CHAR_CLASS", "INV_CHAR_CLASS",
                           "DIGIT",          "NOT_DIGIT", "ALPHA", "NOT_ALPHA",  "WHITESPACE",
                           "NOT_WHITESPACE", "BRANCH"};

    int i;
    int j;
    char c;
    for (i = 0; i < MAX_REGEXP_OBJECTS; ++i) {
        if (pattern[i].type == UNUSED) { break; }

        printf("type: %s", types[pattern[i].type]);
        if (pattern[i].type == CHAR_CLASS || pattern[i].type == INV_CHAR_CLASS) {
            printf(" [");
            for (j = 0; j < MAX_CHAR_CLASS_LEN; ++j) {
                c = pattern[i].u.ccl[j];
                if ((c == '\0') || (c == ']')) { break; }
                printf("%c", c);
            }
            printf("]");
        } else if (pattern[i].type == CHAR) {
            printf(" '%c'", pattern[i].u.ch);
        }
        printf("\n");
    }
}

/* Private functions: */
static inline int _re_matchdigit(char c) {
    return isdigit(c);
}
static inline int _re_matchalpha(char c) {
    return isalpha(c);
}
static inline int _re_matchwhitespace(char c) {
    return isspace(c);
}
static inline int _re_matchalphanum(char c) {
    return ((c == '_') || _re_matchalpha(c) || _re_matchdigit(c));
}
static inline int _re_matchrange(char c, const char *str) {
    return ((c != '-') && (str[0] != '\0') && (str[0] != '-') && (str[1] == '-') &&
            (str[2] != '\0') && ((c >= str[0]) && (c <= str[2])));
}
static inline int _re_matchdot(char c) {
#if defined(RE_DOT_MATCHES_NEWLINE) && (RE_DOT_MATCHES_NEWLINE == 1)
    (void)c;
    return 1;
#else
    return c != '\n' && c != '\r';
#endif
}
static inline int _re_ismetachar(char c) {
    return ((c == 's') || (c == 'S') || (c == 'w') || (c == 'W') || (c == 'd') || (c == 'D'));
}

static inline int _re_matchmetachar(char c, const char *str) {
    switch (str[0]) {
    case 'd': return _re_matchdigit(c);
    case 'D': return !_re_matchdigit(c);
    case 'w': return _re_matchalphanum(c);
    case 'W': return !_re_matchalphanum(c);
    case 's': return _re_matchwhitespace(c);
    case 'S': return !_re_matchwhitespace(c);
    default: return (c == str[0]);
    }
}

static inline int _re_matchcharclass(char c, const char *str) {
    do {
        if (_re_matchrange(c, str)) {
            return 1;
        } else if (str[0] == '\\') {
            /* Escape-char: increment str-ptr and match on next char */
            str += 1;
            if (_re_matchmetachar(c, str)) {
                return 1;
            } else if ((c == str[0]) && !_re_ismetachar(c)) {
                return 1;
            }
        } else if (c == str[0]) {
            if (c == '-') {
                return ((str[-1] == '\0') || (str[1] == '\0'));
            } else {
                return 1;
            }
        }
    } while (*str++ != '\0');

    return 0;
}

static inline int _re_matchone(regex_t p, char c) {
    switch (p.type) {
    case DOT: return _re_matchdot(c);
    case CHAR_CLASS: return _re_matchcharclass(c, (const char *)p.u.ccl);
    case INV_CHAR_CLASS: return !_re_matchcharclass(c, (const char *)p.u.ccl);
    case DIGIT: return _re_matchdigit(c);
    case NOT_DIGIT: return !_re_matchdigit(c);
    case ALPHA: return _re_matchalphanum(c);
    case NOT_ALPHA: return !_re_matchalphanum(c);
    case WHITESPACE: return _re_matchwhitespace(c);
    case NOT_WHITESPACE: return !_re_matchwhitespace(c);
    default: return (p.u.ch == c);
    }
}

static inline int _re_matchstar(regex_t p, regex_t *pattern, const char *text, int *matchlength) {
    int prelen = *matchlength;
    const char *prepoint = text;
    while ((text[0] != '\0') && _re_matchone(p, *text)) {
        text++;
        (*matchlength)++;
    }
    while (text >= prepoint) {
        if (_re_matchpattern(pattern, text--, matchlength)) return 1;
        (*matchlength)--;
    }

    *matchlength = prelen;
    return 0;
}

static inline int _re_matchplus(regex_t p, regex_t *pattern, const char *text, int *matchlength) {
    const char *prepoint = text;
    while ((text[0] != '\0') && _re_matchone(p, *text)) {
        text++;
        (*matchlength)++;
    }
    while (text > prepoint) {
        if (_re_matchpattern(pattern, text--, matchlength)) return 1;
        (*matchlength)--;
    }

    return 0;
}

static inline int _re_matchquestion(regex_t p, regex_t *pattern, const char *text,
                                    int *matchlength) {
    if (p.type == UNUSED) return 1;
    if (_re_matchpattern(pattern, text, matchlength)) return 1;
    if (*text && _re_matchone(p, *text++)) {
        if (_re_matchpattern(pattern, text, matchlength)) {
            (*matchlength)++;
            return 1;
        }
    }
    return 0;
}

#if 0

/* Recursive matching */
static inline int _re_matchpattern(regex_t* pattern, const char* text, int *matchlength)
{
  int pre = *matchlength;
  if ((pattern[0].type == UNUSED) || (pattern[1].type == QUESTIONMARK))
  {
    return _re_matchquestion(pattern[1], &pattern[2], text, matchlength);
  }
  else if (pattern[1].type == STAR)
  {
    return _re_matchstar(pattern[0], &pattern[2], text, matchlength);
  }
  else if (pattern[1].type == PLUS)
  {
    return _re_matchplus(pattern[0], &pattern[2], text, matchlength);
  }
  else if ((pattern[0].type == END) && pattern[1].type == UNUSED)
  {
    return text[0] == '\0';
  }
  else if ((text[0] != '\0') && matchone(pattern[0], text[0]))
  {
    (*matchlength)++;
    return _re_matchpattern(&pattern[1], text+1);
  }
  else
  {
    *matchlength = pre;
    return 0;
  }
}

#else

/* Iterative matching */
static inline int _re_matchpattern(regex_t *pattern, const char *text, int *matchlength) {
    int pre = *matchlength;
    do {
        if ((pattern[0].type == UNUSED) || (pattern[1].type == QUESTIONMARK)) {
            return _re_matchquestion(pattern[0], &pattern[2], text, matchlength);
        } else if (pattern[1].type == STAR) {
            return _re_matchstar(pattern[0], &pattern[2], text, matchlength);
        } else if (pattern[1].type == PLUS) {
            return _re_matchplus(pattern[0], &pattern[2], text, matchlength);
        } else if ((pattern[0].type == END) && pattern[1].type == UNUSED) {
            return (text[0] == '\0');
        }
        /*  Branching is not working properly
            else if (pattern[1].type == BRANCH)
            {
              return (matchpattern(pattern, text) || matchpattern(&pattern[2], text));
            }
        */
        (*matchlength)++;
    } while ((text[0] != '\0') && _re_matchone(*pattern++, *text++));

    *matchlength = pre;
    return 0;
}

#endif

#endif /* ifndef _TINY_REGEX_C */
