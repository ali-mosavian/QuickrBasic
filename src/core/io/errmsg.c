/* QBasic's error numbers and messages.
 *
 * # How it works
 * emsg_errnum maps an error code (E_*) to its ERR number: 0 none; FFh Out of stack space, never
 * trapped; E0h.. BC's compile errors, which have no ERR of their own.
 *
 * emsg_text gives a code's message, 0-ended; E_USR's comes from exec's uerr. A number with no
 * message gives "Unprintable error".
 *
 * # asm structure (retired/asm32/io/errmsg.asm)
 * | asm | here | |
 * |---|---|---|
 * | er_text | emsg_text | MIRROR |
 * | g_qberr | emsg_errnum | MIRROR |
 * | g_qbmsg, g_qbunp | static | MIRROR |
 */
#include "io/errmsg.h"
#include "session.h"
#include "vm/exec.h"

const uint8_t emsg_errnum[E_LAST + 1] = {
    0,  0,    2,    3,    4,    5,    7,    8,    9,    10,   11,   13,   17,   0,  6,  1,
    16, 15,   12,   0,    0,    0,    0,    67,   55,   52,   53,   68,   54,   54, 64, 52,
    0,  13,   18,   0,    0xFF, 73,   33,   40,   0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 38, 26, 29,
    30, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 37,
};

static const struct {
    uint8_t num;
    const char *text;
} msgs[] = {
    {1, "NEXT without FOR"},
    {2, "Syntax error"},
    {3, "RETURN without GOSUB"},
    {4, "Out of DATA"},
    {5, "Illegal function call"},
    {6, "Overflow"},
    {7, "Out of memory"},
    {8, "Label not defined"},
    {9, "Subscript out of range"},
    {10, "Duplicate definition"},
    {11, "Division by zero"},
    {12, "Illegal in direct mode"},
    {13, "Type mismatch"},
    {14, "Out of string space"},
    {16, "String formula too complex"},
    {17, "Cannot continue"},
    {18, "Function not defined"},
    {19, "No RESUME"},
    {20, "RESUME without error"},
    {24, "Device timeout"},
    {25, "Device fault"},
    {26, "FOR without NEXT"},
    {27, "Out of paper"},
    {29, "WHILE without WEND"},
    {30, "WEND without WHILE"},
    {33, "Duplicate label"},
    {35, "Subprogram not defined"},
    {37, "Argument-count mismatch"},
    {38, "Array not defined"},
    {40, "Variable required"},
    {50, "FIELD overflow"},
    {51, "Internal error"},
    {52, "Bad file name or number"},
    {53, "File not found"},
    {54, "Bad file mode"},
    {55, "File already open"},
    {56, "FIELD statement active"},
    {57, "Device I/O error"},
    {58, "File already exists"},
    {59, "Bad record length"},
    {61, "Disk full"},
    {62, "Input past end of file"},
    {63, "Bad record number"},
    {64, "Bad file name"},
    {67, "Too many files"},
    {68, "Device unavailable"},
    {69, "Communication-buffer overflow"},
    {70, "Permission denied"},
    {71, "Disk not ready"},
    {72, "Disk-media error"},
    {73, "Advanced feature unavailable"},
    {74, "Rename across disks"},
    {75, "Path/File access error"},
    {76, "Path not found"},
    {224, "Block IF without END IF"},
    {225, "END IF without block IF"},
    {226, "ELSE without IF"},
    {227, "DO without LOOP"},
    {228, "LOOP without DO"},
    {229, "SELECT without END SELECT"},
    {230, "END SELECT without SELECT"},
    {231, "CASE without SELECT"},
    {232, "SUB/FUNCTION without END SUB/FUNCTION"},
    {233, "END SUB/FUNCTION without SUB/FUNCTION"},
    {234, "EXIT not within matching block"},
    {235, "Statements/labels illegal between SELECT CASE and CASE"},
    {240, "Subprogram error"},
    {241, "AS clause required on first declaration"},
    {242, "Array already dimensioned"},
    {243, "Variable not declared"},
    {244, "Parameter type mismatch"},
    {255, "Out of stack space"},
};

const char *emsg_text(const uint16_t user, const uint8_t code)
{
    uint8_t num = emsg_errnum[code];

    if (code == E_USR) {
        if (user > 0xFE)
            return "Unprintable error";

        num = (uint8_t)user;
    }

    for (size_t i = 0; num && i < countof(msgs); i++)
        if (msgs[i].num == num)
            return msgs[i].text;

    return "Unprintable error";
}
