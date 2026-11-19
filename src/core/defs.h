/* The constants and the arena's records every part shares (retired/asm32/def.inc).
 *
 * # How it works
 * A record whose layout a BASIC program, the bytecode or a file can see is PACKED, as the asm lays
 * it.
 */
#ifndef CORE_DEFS_H
#define CORE_DEFS_H
#include "base.h"

typedef struct Vm Vm; /* the virtual machine (vm/exec.h) */

/* A compiled expression's static type (cty). */
enum : uint8_t {
    T_INT = 0,
    T_DBL = 0x20, /* a whole cell */
    T_STR = 0x40,
    T_FLT = 0x80,
};

/* A declared type (QBasic's ET_*); a record of TYPE t is ET_REC + t. */
enum : uint8_t {
    ET_IMP = 0, /* none given */
    ET_I2 = 1,
    ET_I4 = 2,
    ET_R4 = 3,
    ET_R8 = 4,
    ET_SD = 5,
    ET_SH = 6, /* SHORT: 2 bytes in a record, else an INT */
    ET_BY = 7, /* BYTE: 0 .. 255, 1 byte in a record, else an INT */
    ET_REC = 0x40,
    EMAX = 254,
};

enum {
    FBIAS = 127,
    SK_DBL = 4,   /* a variable's info: a DOUBLE's */
    VS = 4,       /* a variable's slot, a number's bytes */
    NB = 4,       /* a stored FLOAT's bytes */
    MAXD = 16,    /* dimensions at most */
    NTMP = 48,    /* string temporaries */
    NPROC = 256,  /* procedures, 1 .. NPROC-1 */
    NPPAR = 2048, /* their parameters, all */
    NTYPE = 64,
    NFLD = 512,
    NBLK = 2048, /* block statements' entries */
};

/* A name's flags. NM_REC: declared AS a TYPE somewhere, never cleared. */
enum : uint8_t { NM_LBL = 1, NM_FN = 2, NM_SUB = 4, NM_FUNC = 8, NM_REC = 16 };

/* A binding's flags (mem/scope). */
enum : uint8_t { BF_SHR = 1, BF_REF = 2, BF_STAT = 4, BF_LBL = 8, BF_CON = 16, BF_USE = 32 };
enum : uint16_t { SC_NUM = 0x8000, SC_ARR = 0x4000 };

/* A jump target's mode: the statement, past the JMP it starts with, or the statement after it. */
typedef enum : uint8_t { TO_STMT, PAST_JMP, TO_NEXT_STMT } JumpMode;

/* Line numbers are under LNUM_LIMIT. */
enum : uint32_t { LNUM_LIMIT = 64000 };
/* ON's count byte: the count, ON_GOSUB for GOSUB. */
enum : uint8_t { ON_GOSUB = 0x80, ON_COUNT = 0x7F };
/* LIST's line operand: a line number, or LABEL_LINE plus a label's line. */
enum : uint32_t { LABEL_LINE = 0x10000 };
/* DIM's flag word: the dimension count, then what kind of DIM it is. */
enum : uint32_t { DIM_COUNT = 0x3F, DIM_REDIM = 0x40, DIM_STATIC = 0x80, DIM_PRESERVE = 0x100 };

/* Tokens in a stored line. */
enum : uint8_t { M_VAR = 1, M_INT = 2, REM_Q = 3 };

/* A line's record in the program store, its body after it. */
typedef struct PACKED {
    uint32_t len;  /* the record's bytes, 0 the end */
    uint16_t num;  /* its number, NONUM none */
    uint32_t code; /* its code in the cache, 0 none */
    uint32_t pre;  /* its text before the body, after it */
    uint8_t flags; /* LF_* */
    uint8_t sep;   /* the byte before the body the compiler reads */
    uint16_t src;  /* its first line in its source file, 0 none */
    uint16_t file; /* that file: 0 the main, n the nth include read */
} Line;
static_assert(sizeof(Line) == 20);
enum { HDR = sizeof(Line) };
/* LF_NODEF: decl_line found no DEFtype in it, decl_defevents passes it. */
enum : uint8_t { LF_ASIS = 1, LF_BP = 2, LF_INC = 4, LF_NODEF = 8 };
enum : uint32_t {
    NONUM = 0xFFFF,
    LMAX = 0xFEE0,     /* a line's body at most */
    BOFF = 0xFFFFFFFF, /* a statement map's body offset of the line's end */
    PDIR = 0x80000000, /* a target's flag: the direct line's */
    XSLK = 48,         /* the most one op writes past its size */
    XLIM = 0x13000,    /* a line's code and statement map at most */
};

/* A statement map's entry, before a line's code; their count after them. */
typedef struct PACKED {
    uint32_t body, code;
} Stmt;

/* Channels (io/chan). */
enum { NL = 13, NCH = 16, CBSZ = 4096, RECMAX = 4096 };
enum { RECLEN_DEFAULT = 128, FILENO_MAX = 255 }; /* OPEN's record length; file numbers 1 .. */
enum : uint8_t { CK_CON = 1, CK_DISK = 2, CK_NUL = 3, CK_LPT = 4 }; /* LPT1, unbuffered */
enum : uint8_t { FM_IN = 1, FM_OUT = 2, FM_APP = 4, FM_RND = 8, FM_BIN = 16 };
/* A variable's place (cm_va): its kind (VA_*) in the top byte, its bytes below (a Desc's: its
   STRING * n, 0 none). */
enum : uint8_t { VA_DESC = 1, VA_RAW = 2, VA_I2 = 3 };
static inline uint32_t va_pack(const uint8_t kind, const uint32_t n)
{
    return (uint32_t)kind << 24 | n;
}
static inline uint8_t va_kind(const uint32_t v) { return (uint8_t)(v >> 24); }
static inline uint16_t va_n(const uint32_t v) { return (uint16_t)v; }

enum : uint8_t { SF_BRK = 1, SF_EV = 2 };

/* An event's slot (vm/event). */
typedef struct PACKED {
    uint32_t line;
    uint8_t state, flags;
    uint16_t pad;
} EventSlot;
static_assert(sizeof(EventSlot) == 8);
enum : uint8_t { EK_TIMER, EK_KEY, EK_COM, EK_PEN, EK_STRIG, EK_PLAY, EK_UEVENT };
enum : uint8_t { ES_OFF = 0, ES_ON = 1, ES_STOP = 2 }; /* an event's state */
/* RESUME's operand: the failed statement, the one after it, or a line. */
typedef enum : uint8_t { RESUME_AGAIN, RESUME_NEXT, RESUME_LINE } ResumeMode;

enum : uint8_t { DM_STEP = 1, DM_PSTEP = 2 };

/* A string's Desc. */
typedef struct PACKED {
    uint16_t len;
    Addr ptr;
} Desc;
static_assert(sizeof(Desc) == 6);
enum { DSZ = sizeof(Desc), SMAX = 32767 };
/* A string block's tail, after its chars. */
typedef struct PACKED {
    uint16_t len;
    Addr owner;
} StrTail;
enum { BHDR = sizeof(StrTail) };

/* An array's header: its dimensions, its data, the count of a 1-D, then the counts and the lower
   bounds. */
typedef struct PACKED {
    uint8_t dims;
    Addr data;
    uint32_t fast;
    uint32_t count[];
} ArrayHdr;
/* An array's cell: its header (0 none) and whether REDIM made it (ERASE frees it). */
typedef struct {
    Addr hdr;
    uint32_t dyn;
} ArrSlot;

/* Errors and messages: E_* QBasic's errors, MSG_* the other messages. */
enum : uint8_t {
    E_BRK = 0,
    MSG_READY = 1,
    E_SN = 2,
    E_RG = 3,
    E_OD = 4,
    E_IQ = 5,
    E_OM = 6,
    E_US = 7,
    E_BS = 8,
    E_RD = 9,
    E_DZ = 10,
    E_TM = 11,
    E_CN = 12,
    MSG_OK = 13,
    E_OV = 14,
    E_NF = 15,
    E_ST = 16,
    E_LS = 17,
    E_ID = 18,
    MSG_ERR = 19,
    MSG_IN = 20,
    MSG_REDO = 21,
    MSG_EXTRA = 22,
    E_TMF = 23,
    E_VF = 32,
    E_FD = 33,
    E_UF = 34,
    E_USR = 35, /* ERROR n */
    E_OS = 36,  /* Out of stack space: not trapped */
    E_AF = 37,
    E_DL = 38,
    E_VR = 39,
    E_SPE = 40,
    E_ASF = 41,
    E_AAD = 42,
    E_VND = 43,
    E_PTM = 44,
    E_AND = 45,
    E_FWN = 46,
    E_WWW = 47,
    E_WEW = 48,
    E_BIF = 49,
    E_EIF = 50,
    E_ELS = 51,
    E_DWL = 52,
    E_LWD = 53,
    E_SWE = 54,
    E_ESW = 55,
    E_CWS = 56,
    E_PWE = 57,
    E_EPW = 58,
    E_EXW = 59,
    E_SLC = 60, /* Statements/labels illegal between SELECT CASE and CASE */
    E_SND = 61, /* Subprogram not defined: a SUB DECLAREd only */
    E_ACM = 62, /* Argument-count mismatch */
    E_CNW = 63, /* CONTINUE not within matching block */
    E_LAST = E_CNW,
};

/* QBasic's ERR numbers the runtime raises (fault_qberr), as ERROR n raises them. */
typedef enum : uint8_t {
    QB_NO_FUNCTION = 18,
    QB_RESUME_NO_ERROR = 20,
    QB_NO_SUB = 35,
    QB_FIELD_OVERFLOW = 50,
    QB_BAD_FILE_NUMBER = 52,
    QB_FILE_NOT_FOUND = 53,
    QB_BAD_FILE_MODE = 54,
    QB_FILE_OPEN = 55,
    QB_FILE_EXISTS = 58,
    QB_BAD_RECORD_LENGTH = 59,
    QB_DISK_FULL = 61,
    QB_PAST_END = 62,
    QB_BAD_RECORD_NUMBER = 63,
    QB_BAD_FILE_NAME = 64,
    QB_TOO_MANY_FILES = 67,
    QB_DEVICE_UNAVAILABLE = 68,
    QB_PERMISSION_DENIED = 70,
    QB_NO_ADVANCED = 73,
    QB_PATH_ACCESS = 75,
    QB_PATH_NOT_FOUND = 76,
} QbErr;

/* The memory map: offsets in the program store and the code cache. */
enum {
    DLINE = 0,         /* the direct line, len 0, num FFxxh */
    PROGBASE = 0x0110, /* then the program */
    HSIZE = 1024,      /* the symbols' hash chains */
    DCODE = 0x0100,    /* the direct line's stream */
    CACHE0 = DCODE + 1536,
    HMIN = 0x40000,   /* the heap at least */
    STKSZ = 0x10000,  /* the data stack */
    STKROOM = 0x8000, /* its room for the run */
    CPROOM = 0x4000,  /* the compiler's frames */
    OBSZ = 256,       /* the output buffer */
};

/* Block statements (comp/block), procedures, TYPEs: their records are comp's. */
enum : uint8_t {
    BK_IF = 1,
    BK_ELSEIF,
    BK_ELSE,
    BK_ENDIF,
    BK_DO,
    BK_LOOP,
    BK_WHILE,
    BK_WEND,
    BK_SELECT,
    BK_CASE,
    BK_ENDSEL,
    BK_EXIT,
    BK_FOR,
    BK_NEXT, /* one per variable a NEXT closes; sel its FOR */
    BK_CONT, /* CONTINUE FOR, DO, WHILE */
};
enum : uint8_t { PK_SUB = 1, PK_FUNC = 2, PK_END = 3 };
enum : uint8_t {
    PF_STAT = 1,
    PF_DEF = 2,
    PF_ARRAYS = 4
}; /* PF_ARRAYS: a local array or record in its frame */
#endif
