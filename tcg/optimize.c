/*
 * Optimizations for Tiny Code Generator for QEMU
 *
 * Copyright (c) 2010 Kirill Batuzov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#ifdef _WIN32
#include <malloc.h>
#endif
#ifdef _AIX
#include <alloca.h>
#endif

#include "qemu-common.h"
#include "cache-utils.h"
#include "host-utils.h"
#define NO_CPU_IO_DEFS
#include "cpu.h"
#include "exec-all.h"

#include "tcg-op.h"
#include "elf.h"

#if defined(CONFIG_USE_GUEST_BASE) && !defined(TCG_TARGET_HAS_GUEST_BASE)
#error GUEST_BASE not supported on this host.
#endif

typedef enum { TCG_TEMP_UNDEF = 0, TCG_TEMP_CONST, TCG_TEMP_COPY, TCG_TEMP_ANY } tcg_temp_state;

const int mov_opc[] = {
    INDEX_op_mov_i32,
#if TCG_TARGET_REG_BITS == 64
    INDEX_op_mov_i64,
#endif
};

static int op_to_opi(int op)
{
    switch(op) {
        case INDEX_op_mov_i32: return INDEX_op_movi_i32;
#if TCG_TARGET_REG_BITS == 64
        case INDEX_op_mov_i64: return INDEX_op_movi_i64;
#endif
        default:
            fprintf(stderr, "INTERNAL ERROR: TCG optimizer encountered an "
                            "unrecognized operation in op_to_opi.\n");
            exit(1);
    }
}

static int op_bits(int op)
{
    switch(op) {
        case INDEX_op_mov_i32:
        case INDEX_op_add_i32:
        case INDEX_op_sub_i32:
        case INDEX_op_mul_i32:
        case INDEX_op_and_i32:
        case INDEX_op_or_i32:
        case INDEX_op_xor_i32:
        case INDEX_op_shl_i32:
        case INDEX_op_shr_i32:
        case INDEX_op_sar_i32:
        case INDEX_op_rotl_i32:
        case INDEX_op_rotr_i32:
            return 32;
#if TCG_TARGET_REG_BITS == 64
        case INDEX_op_mov_i64:
        case INDEX_op_add_i64:
        case INDEX_op_sub_i64:
        case INDEX_op_mul_i64:
        case INDEX_op_and_i64:
        case INDEX_op_or_i64:
        case INDEX_op_xor_i64:
        case INDEX_op_shl_i64:
        case INDEX_op_shr_i64:
        case INDEX_op_sar_i64:
        case INDEX_op_rotl_i64:
        case INDEX_op_rotr_i64:
            return 64;
#endif
        default:
            fprintf(stderr, "INTERNAL ERROR: TCG optimizer encountered an "
                            "unrecognized operation in op_bits.\n");
            exit(1);
    }
}

static int op_to_movi(int op)
{
    if (op_bits(op) == 32)
        return INDEX_op_movi_i32;
#if TCG_TARGET_REG_BITS == 64
    if (op_bits(op) == 64)
        return INDEX_op_movi_i64;
#endif
    fprintf(stderr, "INTERNAL ERROR: TCG optimizer encountered an "
                    "unrecognized operation in op_to_movi.\n");
    exit(1);
}

static TCGArg do_constant_folding_2(int op, TCGArg x, TCGArg y)
{
    TCGArg r;
    switch(op) {
        case INDEX_op_add_i32:
#if TCG_TARGET_REG_BITS == 64
        case INDEX_op_add_i64:
#endif
            return x + y;

        case INDEX_op_sub_i32:
#if TCG_TARGET_REG_BITS == 64
        case INDEX_op_sub_i64:
#endif
            return x - y;

        case INDEX_op_mul_i32:
#if TCG_TARGET_REG_BITS == 64
        case INDEX_op_mul_i64:
#endif
            return x * y;

        case INDEX_op_and_i32:
#if TCG_TARGET_REG_BITS == 64
        case INDEX_op_and_i64:
#endif
            return x & y;

        case INDEX_op_or_i32:
#if TCG_TARGET_REG_BITS == 64
        case INDEX_op_or_i64:
#endif
            return x | y;

        case INDEX_op_xor_i32:
#if TCG_TARGET_REG_BITS == 64
        case INDEX_op_xor_i64:
#endif
            return x ^ y;

        case INDEX_op_shl_i32:
#if TCG_TARGET_REG_BITS == 64
            y &= 0xffffffff;
        case INDEX_op_shl_i64:
#endif
            return x << y;

        case INDEX_op_shr_i32:
#if TCG_TARGET_REG_BITS == 64
            x &= 0xffffffff;
            y &= 0xffffffff;
        case INDEX_op_shr_i64:
#endif
            /* Assuming TCGArg to be unsigned */
            return x >> y;

        case INDEX_op_sar_i32:
#if TCG_TARGET_REG_BITS == 64
            x &= 0xffffffff;
            y &= 0xffffffff;
#endif
            r = x & 0x80000000;
            x &= ~0x80000000;
            x >>= y;
            r |= r - (r >> y);
            x |= r;
            return x;

#if TCG_TARGET_REG_BITS == 64
        case INDEX_op_sar_i64:
            r = x & 0x8000000000000000ULL;
            x &= ~0x8000000000000000ULL;
            x >>= y;
            r |= r - (r >> y);
            x |= r;
            return x;
#endif

        case INDEX_op_rotr_i32:
#if TCG_TARGET_REG_BITS == 64
            x &= 0xffffffff;
            y &= 0xffffffff;
#endif
            x = (x << (32 - y)) | (x >> y);
            return x;

#if TCG_TARGET_REG_BITS == 64
        case INDEX_op_rotr_i64:
            x = (x << (64 - y)) | (x >> y);
            return x;
#endif

        case INDEX_op_rotl_i32:
#if TCG_TARGET_REG_BITS == 64
            x &= 0xffffffff;
            y &= 0xffffffff;
#endif
            x = (x << y) | (x >> (32 - y));
            return x;

#if TCG_TARGET_REG_BITS == 64
        case INDEX_op_rotl_i64:
            x = (x << y) | (x >> (64 - y));
            return x;
#endif

        default:
            fprintf(stderr, "INTERNAL ERROR: TCG optimizer encountered an "
                            "unrecognized operation in do_constant_folding.\n");
            exit(1);
    }
}

static TCGArg do_constant_folding(int op, TCGArg x, TCGArg y)
{
    TCGArg res = do_constant_folding_2(op, x, y);
#if TCG_TARGET_REG_BITS == 64
    if (op_bits(op) == 32) {
        res &= 0xffffffff;
    }
#endif
    return res;
}

static void reset_temp(tcg_temp_state *state, tcg_target_ulong *vals,
        TCGArg temp, int nb_temps, int nb_globals)
{
    int i;
    TCGArg new_base;
    new_base = (TCGArg)-1;
#if 0
    for (i = 0; i < nb_temps; i++) {
        if (state[i] == TCG_TEMP_COPY && vals[i] == temp) {
            if (new_base == ((TCGArg)-1)) {
                new_base = (TCGArg)i;
                state[i] = TCG_TEMP_ANY;
            } else {
                vals[i] = new_base;
            }
        }
    }
#else
    for (i = nb_globals; i < nb_temps; i++) {
        if (state[i] == TCG_TEMP_COPY && vals[i] == temp) {
            if (new_base == ((TCGArg)-1)) {
                new_base = (TCGArg)i;
                state[i] = TCG_TEMP_ANY;
            } else {
                vals[i] = new_base;
            }
        }
    }
    for (i = 0; i < nb_globals; i++) {
        if (state[i] == TCG_TEMP_COPY && vals[i] == temp) {
            if (new_base == ((TCGArg)-1)) {
                state[i] = TCG_TEMP_ANY;
            } else {
                vals[i] = new_base;
            }
        }
    }
#endif
    state[temp] = TCG_TEMP_ANY;
}

static TCGArg *tcg_constant_folding(TCGContext *s, uint16_t *tcg_opc_ptr,
                                    TCGArg *args, TCGOpDef *tcg_op_defs)
{
    int i, nb_ops, op_index, op, nb_temps, nb_globals;
    const TCGOpDef *def;
    TCGArg *gen_args;
    tcg_target_ulong *vals;
    tcg_temp_state *state;

    nb_temps = s->nb_temps;
    nb_globals = s->nb_globals;
    state = (tcg_temp_state *)malloc(sizeof(tcg_temp_state) * nb_temps);
    vals = (tcg_target_ulong *)malloc(sizeof(tcg_target_ulong) * nb_temps);
    memset(state, 0, nb_temps * sizeof(tcg_temp_state));

    //	gen_opc_ptr++; /* Skip end? What is this? */

    nb_ops = tcg_opc_ptr - gen_opc_buf;
    gen_args = args;
    for (op_index = 0; op_index < nb_ops; op_index++) {
        op = gen_opc_buf[op_index];
        def = &tcg_op_defs[op];
        /* Do copy propagation */
        if (op != INDEX_op_call) {
            for (i = def->nb_oargs; i < def->nb_oargs + def->nb_iargs; i++) {
                if (state[args[i]] == TCG_TEMP_COPY
                    && !(def->args_ct[i].ct & TCG_CT_IALIAS)
                    /* TODO: is this check really needed??? */
                    && (def->args_ct[i].ct & TCG_CT_REG)) {
/*                    if (vals[args[i]] < s->nb_globals)
                        printf("Fuuu\n");*/
                    args[i] = vals[args[i]];
                }
            }
        }

        /* Propagate constants through copy operations and do constant
           folding.  Constants will be substituted to arguments by register
           allocator where needed and possible.  Also detect copies. */
        switch(op) {
            case INDEX_op_mov_i32:
#if TCG_TARGET_REG_BITS == 64
            case INDEX_op_mov_i64:
#endif
                if ((state[args[1]] == TCG_TEMP_COPY
                    && vals[args[1]] == args[0])
                    || args[0] == args[1]) {
                    args += 2;
                    gen_opc_buf[op_index] = INDEX_op_nop;
                    break;
                }
                if (state[args[1]] != TCG_TEMP_CONST) {
                    reset_temp(state, vals, args[0], nb_temps, nb_globals);
                    if (args[1] >= s->nb_globals) {
                        state[args[0]] = TCG_TEMP_COPY;
                        vals[args[0]] = args[1];
                    }
                    gen_args[0] = args[0];
                    gen_args[1] = args[1];
                    gen_args += 2;
                    args += 2;
                    break;
                } else {
                    /* Source argument is constant.  Rewrite the operation and
                       let movi case handle it. */
                    op = op_to_opi(op);
                    gen_opc_buf[op_index] = op;
                    args[1] = vals[args[1]];
                    /* fallthrough */
                }
            case INDEX_op_movi_i32:
#if TCG_TARGET_REG_BITS == 64
            case INDEX_op_movi_i64:
#endif
                reset_temp(state, vals, args[0], nb_temps, nb_globals);
                state[args[0]] = TCG_TEMP_CONST;
                vals[args[0]] = args[1];
                gen_args[0] = args[0];
                gen_args[1] = args[1];
                gen_args += 2;
                args += 2;
                break;
            case INDEX_op_or_i32:
            case INDEX_op_and_i32:
#if TCG_TARGET_REG_BITS == 64
            case INDEX_op_and_i64:
            case INDEX_op_or_i64:
#endif
                if (args[1] == args[2]) {
                    if (args[1] == args[0]) {
                        args += 3;
                        gen_opc_buf[op_index] = INDEX_op_nop;
                    } else {
                        reset_temp(state, vals, args[0], nb_temps, nb_globals);
                        if (args[1] >= s->nb_globals) {
                            state[args[0]] = TCG_TEMP_COPY;
                            vals[args[0]] = args[1];
                        }
                        gen_opc_buf[op_index] = mov_opc[op_bits(op) / 32 - 1];
                        gen_args[0] = args[0];
                        gen_args[1] = args[1];
                        gen_args += 2;
                        args += 3;
                    }
                    break;
                }
                /* Proceede with default binary operation handling */
            case INDEX_op_add_i32:
            case INDEX_op_sub_i32:
            case INDEX_op_xor_i32:
            case INDEX_op_mul_i32:
            case INDEX_op_shl_i32:
            case INDEX_op_shr_i32:
            case INDEX_op_sar_i32:
            case INDEX_op_rotl_i32:
            case INDEX_op_rotr_i32:
#if TCG_TARGET_REG_BITS == 64
            case INDEX_op_add_i64:
            case INDEX_op_sub_i64:
            case INDEX_op_xor_i64:
            case INDEX_op_mul_i64:
            case INDEX_op_shl_i64:
            case INDEX_op_shr_i64:
            case INDEX_op_sar_i64:
            case INDEX_op_rotl_i64:
            case INDEX_op_rotr_i64:
#endif
                if (state[args[1]] == TCG_TEMP_CONST
                    && state[args[2]] == TCG_TEMP_CONST)
                {
                    gen_opc_buf[op_index] = op_to_movi(op);
                    gen_args[0] = args[0];
                    gen_args[1] = do_constant_folding(op, vals[args[1]], vals[args[2]]);
                    reset_temp(state, vals, gen_args[0], nb_temps, nb_globals);
                    state[gen_args[0]] = TCG_TEMP_CONST;
                    vals[gen_args[0]] = gen_args[1];
                    gen_args += 2;
                    args += 3;
                    break;
                } else {
                    reset_temp(state, vals, args[0], nb_temps, nb_globals);
                    gen_args[0] = args[0];
                    gen_args[1] = args[1];
                    gen_args[2] = args[2];
                    gen_args += 3;
                    args += 3;
                    break;
                }
            case INDEX_op_call:
            case INDEX_op_jmp:
            case INDEX_op_br:
            case INDEX_op_brcond_i32:
            case INDEX_op_set_label:
#if TCG_TARGET_REG_BITS == 64
            case INDEX_op_brcond_i64:
#endif
                memset(state, 0, nb_temps * sizeof(tcg_temp_state));
                i = (op == INDEX_op_call) ?
                    (args[0] >> 16) + (args[0] & 0xffff) + 3 :
                    def->nb_args;
                while (i) {
                    *gen_args = *args;
                    args++;
                    gen_args++;
                    i--;
                }
                break;
            default:
                /* Default case: we do know nothing about operation so no
                   propagation is done.  We only trash output args.  */
                for (i = 0; i < def->nb_oargs; i++) {
                    reset_temp(state, vals, args[i], nb_temps, nb_globals);
                }
                for (i = 0; i < def->nb_args; i++) {
                    gen_args[i] = args[i];
                }
                args += def->nb_args;
                gen_args += def->nb_args;
                break;
        }
    }

	if (vals)
		free(vals);
	if (state)
		free(state);

    return gen_args;
}

TCGArg *tcg_optimize(TCGContext *s, uint16_t *tcg_opc_ptr,
        TCGArg *args, TCGOpDef *tcg_op_defs)
{
    TCGArg *res;
    res = tcg_constant_folding(s, tcg_opc_ptr, args, tcg_op_defs);
    return res;
}
