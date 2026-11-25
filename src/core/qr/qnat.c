/* Quickr's native entries: the code a call of a native function compiles to.
 *
 * # How it works
 * `native function sqrt(x as double) as double` in a module names an entry here; its calls
 * compile to the entry's steps, the arguments pushed where its steps say, already of the declared
 * types. Each is the code QBasic's statement or function compiles to, so a library call costs
 * what the built-in costs. The rest are CALL QX routines (qrt.h). An entry is the VM's service, no
 * module's: the name is the function's, wherever it is declared.
 */
#include "qr/qnat.h"
#include "comp/codegen.h"
#include "num/double.h"
#include "qr/qrt.h"
#include "vm/syscall.h"

#define ARG(k) {QN_ARG, 0, k}
#define OP(o) {QN_OP, O_##o, 0}
#define OPB(o, b) {QN_OPB, O_##o, b}
#define XOP(x) {QN_OPB, O_X, X_##x}
#define INT(v) {QN_INT, 0, v}
#define CALL(k) {QN_CALL, K_##k, 0}
#define SYS(s) {QN_SYS, SC_##s, 0}
#define X(r) {QN_X, 0, QX_##r}
#define NAT(name, ...)                                                                             \
    {name,                                                                                         \
     (const QNatStep[]){__VA_ARGS__},                                                              \
     sizeof((const QNatStep[]){__VA_ARGS__}) / sizeof(QNatStep)}

static const QNat qnat_all[] = {
    /* math: D_FN, as SQR(x#) .. */
    NAT("sqrt", ARG(0), OPB(D_FN, DF_SQR)),
    NAT("sin", ARG(0), OPB(D_FN, DF_SIN)),
    NAT("cos", ARG(0), OPB(D_FN, DF_COS)),
    NAT("tan", ARG(0), OPB(D_FN, DF_TAN)),
    NAT("atan", ARG(0), OPB(D_FN, DF_ATN)),
    NAT("exp", ARG(0), OPB(D_FN, DF_EXP)),
    NAT("log", ARG(0), OPB(D_FN, DF_LOG)),
    NAT("floor", ARG(0), OPB(D_FN, DF_INT)),
    NAT("ceil", ARG(0), OP(D_NEG), OPB(D_FN, DF_INT), OP(D_NEG)),
    /* text */
    NAT("find", ARG(0), ARG(1), X(FIND)),
    NAT("find_from", ARG(0), ARG(1), ARG(2), {QN_X, 0, QX_FIND | QX_FROM}),
    NAT("upper", ARG(0), CALL(UCASE)),
    NAT("lower", ARG(0), CALL(LCASE)),
    NAT("trim", ARG(0), CALL(RTRIM), CALL(LTRIM)),
    NAT("chr", ARG(0), CALL(FN_CHRS)),
    NAT("code", ARG(0), CALL(FN_ASC)),
    NAT("hex", ARG(0), CALL(HEX)),
    NAT("number", ARG(0), CALL(DVAL)),
    /* io: LINE INPUT, LOCATE row, col, COLOR fg, bg, CLS */
    NAT("read_line", CALL(INLN), CALL(SLINE)),
    NAT("at",
        ARG(0),
        INT(1),
        ARG(1),
        INT(1),
        INT(0),
        INT(0),
        INT(0),
        INT(0),
        INT(0),
        INT(0),
        SYS(LOCATE)),
    NAT("color", ARG(0), INT(1), ARG(1), INT(1), INT(0), INT(0), SYS(COLOR)),
    NAT("clear", INT(-1), SYS(CLS)),
    /* random: RANDOMIZE n, RND */
    NAT("seed", ARG(0), CALL(RNDZ)),
    NAT("unit", OPB(F_LD_U8, 1), CALL(FN_RND), OP(F_TOD)),
    NAT("below",
        OPB(F_LD_U8, 1),
        CALL(FN_RND),
        OP(F_TOD),
        ARG(0),
        OP(I_TOD),
        OP(D_MUL),
        OPB(D_FN, DF_INT),
        XOP(D_TOI)),
    /* time: TIMER, DATE$, TIME$ */
    NAT("ticks", CALL(TIMER), OP(F_TOD)),
    NAT("date_text", CALL(DATE)),
    NAT("time_text", CALL(TIME)),
    /* os: COMMAND$, ENVIRON$(name), END */
    NAT("command", CALL(CMD)),
    NAT("env", ARG(0), INT(1), CALL(ENVS)),
    NAT("exit", CALL(END)),
    NAT("run", ARG(0), CALL(SHELL)),
    /* keys: INKEY$, PEN(n) */
    NAT("inkey", CALL(INKEY)),
    NAT("pen", ARG(0), SYS(PEN)),
    /* fs: a file by its number, as OPEN's; an error its negative ERR */
    NAT("file_open", ARG(0), ARG(1), X(FOPEN)),
    NAT("file_read", ARG(0), ARG(1), X(FREAD)),
    NAT("file_write", ARG(0), ARG(1), X(FWRITE)),
    NAT("file_seek", ARG(0), ARG(1), X(FSEEK)),
    NAT("file_size", ARG(0), X(FSIZE)),
    NAT("file_close", ARG(0), X(FCLOSE)),
    NAT("file_list", ARG(0), X(FLIST)),
    NAT("error_text", ARG(0), X(ERRTEXT)),
};

const QNat *qnat_find(const char *const name, const uint32_t len)
{
    for (uint32_t i = 0; i < countof(qnat_all); i++)
        if (strlen(qnat_all[i].name) == len && !memcmp(qnat_all[i].name, name, len))
            return &qnat_all[i];

    return nullptr;
}
