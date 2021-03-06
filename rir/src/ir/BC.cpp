#include "BC.h"

#include "utils/Pool.h"
#include <iostream>
#include <iomanip>

#include "R/RList.h"
#include "CodeStream.h"
#include "R/r.h"
#include "R/Funtab.h"

#include "interpreter/deoptimizer.h"

namespace rir {

bool BC::operator==(const BC& other) const {
    if (bc != other.bc)
        return false;

    switch (bc) {
    case BC_t::push_:
    case BC_t::ldfun_:
    case BC_t::ldddvar_:
    case BC_t::ldarg_:
    case BC_t::ldvar_:
    case BC_t::ldlval_:
    case BC_t::stvar_:
    case BC_t::missing_:
    case BC_t::subassign2_:
        return immediate.pool == other.immediate.pool;

    case BC_t::dispatch_:
    case BC_t::call_:
    case BC_t::call_stack_:
    case BC_t::static_call_stack_:
    case BC_t::dispatch_stack_:
        return immediate.call_args.call_id == other.immediate.call_args.call_id;

    case BC_t::guard_env_:
        return immediate.guard_id == other.immediate.guard_id;

    case BC_t::guard_fun_:
        return immediate.guard_fun_args.name ==
                   other.immediate.guard_fun_args.name &&
               immediate.guard_fun_args.expected ==
                   other.immediate.guard_fun_args.expected;

    case BC_t::promise_:
    case BC_t::push_code_:
        return immediate.fun == other.immediate.fun;

    case BC_t::br_:
    case BC_t::brtrue_:
    case BC_t::beginloop_:
    case BC_t::brobj_:
    case BC_t::brfalse_:
    case BC_t::label:
        return immediate.offset == other.immediate.offset;

    case BC_t::pick_:
    case BC_t::pull_:
    case BC_t::is_:
    case BC_t::put_:
    case BC_t::alloc_:
        return immediate.i == other.immediate.i;

    case BC_t::subset2_:
    case BC_t::extract2_:
    case BC_t::subset1_:
    case BC_t::extract1_:
    case BC_t::ret_:
    case BC_t::length_:
    case BC_t::names_:
    case BC_t::set_names_:
    case BC_t::force_:
    case BC_t::pop_:
    case BC_t::close_:
    case BC_t::asast_:
    case BC_t::asbool_:
    case BC_t::dup_:
    case BC_t::dup2_:
    case BC_t::test_bounds_:
    case BC_t::swap_:
    case BC_t::int3_:
    case BC_t::uniq_:
    case BC_t::aslogical_:
    case BC_t::lgl_and_:
    case BC_t::lgl_or_:
    case BC_t::inc_:
    case BC_t::add_:
    case BC_t::mul_:
    case BC_t::div_:
    case BC_t::idiv_:
    case BC_t::mod_:
    case BC_t::pow_:
    case BC_t::sub_:
    case BC_t::lt_:
    case BC_t::seq_:
    case BC_t::return_:
    case BC_t::isfun_:
    case BC_t::invisible_:
    case BC_t::visible_:
    case BC_t::endcontext_:
    case BC_t::subassign_:
        return true;

    case BC_t::invalid_:
    case BC_t::num_of:
        break;
    }

    assert(false);
    return false;
}

void BC::write(CodeStream& cs) const {
    cs.insert(bc);
    switch (bc) {
    case BC_t::push_:
    case BC_t::ldarg_:
    case BC_t::ldfun_:
    case BC_t::ldddvar_:
    case BC_t::ldvar_:
    case BC_t::ldlval_:
    case BC_t::stvar_:
    case BC_t::missing_:
    case BC_t::subassign2_:
        cs.insert(immediate.pool);
        return;

    case BC_t::guard_env_:
        cs.insert(immediate.guard_id);
        return;

    case BC_t::guard_fun_:
        cs.insert(immediate.guard_fun_args);
        return;

    // They have to be inserted by CodeStream::insertCall
    case BC_t::call_:
    case BC_t::dispatch_:
    case BC_t::call_stack_:
    case BC_t::static_call_stack_:
    case BC_t::dispatch_stack_:
        assert(false);
        break;

    case BC_t::promise_:
    case BC_t::push_code_:
        cs.insert(immediate.fun);
        return;

    case BC_t::br_:
    case BC_t::brtrue_:
    case BC_t::beginloop_:
    case BC_t::brobj_:
    case BC_t::brfalse_:
        cs.patchpoint(immediate.offset);
        return;

    case BC_t::pick_:
    case BC_t::pull_:
    case BC_t::is_:
    case BC_t::put_:
    case BC_t::alloc_:
        cs.insert(immediate.i);
        return;

    case BC_t::subset2_:
    case BC_t::extract2_:
    case BC_t::subset1_:
    case BC_t::extract1_:
    case BC_t::ret_:
    case BC_t::length_:
    case BC_t::names_:
    case BC_t::set_names_:
    case BC_t::force_:
    case BC_t::pop_:
    case BC_t::close_:
    case BC_t::asast_:
    case BC_t::asbool_:
    case BC_t::dup_:
    case BC_t::dup2_:
    case BC_t::test_bounds_:
    case BC_t::swap_:
    case BC_t::int3_:
    case BC_t::uniq_:
    case BC_t::aslogical_:
    case BC_t::lgl_and_:
    case BC_t::lgl_or_:
    case BC_t::inc_:
    case BC_t::add_:
    case BC_t::mul_:
    case BC_t::div_:
    case BC_t::idiv_:
    case BC_t::mod_:
    case BC_t::pow_:
    case BC_t::sub_:
    case BC_t::lt_:
    case BC_t::seq_:
    case BC_t::return_:
    case BC_t::isfun_:
    case BC_t::invisible_:
    case BC_t::visible_:
    case BC_t::endcontext_:
    case BC_t::subassign_:
        return;

    case BC_t::invalid_:
    case BC_t::num_of:
    case BC_t::label:
        assert(false);
        return;
    }
}

SEXP BC::immediateConst() { return Pool::get(immediate.pool); }

void BC::printArgs(CallSite cs) {
    Rprintf("[");
    for (unsigned i = 0; i < cs.nargs(); ++i) {
        auto arg = cs.arg(i);
        if (arg == MISSING_ARG_IDX)
            Rprintf(" _");
        else if (arg == DOTS_ARG_IDX)
            Rprintf(" ...");
        else
            Rprintf(" %x", arg);
    }
    Rprintf("] ");
}

void BC::printNames(CallSite cs) {
    if (cs.hasNames()) {
        Rprintf("[");
        for (unsigned i = 0; i < cs.nargs(); ++i) {
            SEXP n = cs.name(i);
            Rprintf(
                " %s",
                (n == nullptr || n == R_NilValue ? "_" : CHAR(PRINTNAME(n))));
        }
        Rprintf("]");
    }
}

void BC::printProfile(CallSite cs) {
    if (cs.hasProfile()) {
        CallSiteProfile* prof = cs.profile();
        Rprintf("           Prof : [");
        if (prof->takenOverflow)
            Rprintf("*, <");
        else
            Rprintf("%u, <", prof->taken);
        if (prof->targetsOverflow)
            Rprintf("*>, ");
        else
            Rprintf("%u> ", prof->numTargets);
        for (int i = 0; i < prof->numTargets; ++i)
            Rprintf("%p(%s) ", prof->targets[i],
                    type2char(TYPEOF(prof->targets[i])));
        Rprintf("]\n");
    }
}

CallSite BC::callSite(Code* code) {
    return CallSite(*this, CallSite_get(code, immediate.call_args.call_id));
}

void BC::print() { print(CallSite()); }

void BC::print(CallSite cs) {
    if (bc != BC_t::label) {
        Rprintf("   ");
        Rprintf("%s ", name(bc));
    }

    switch (bc) {
    case BC_t::invalid_:
    case BC_t::num_of:
        assert(false);
        break;
    case BC_t::dispatch_: {
        if (cs.isValid()) {
            SEXP selector = cs.selector();
            Rprintf(" `%s` ", CHAR(PRINTNAME(selector)));
            Rprintf("\n        # ");
            Rf_PrintValue(cs.call());
            printProfile(cs);
        }
        break;
    }
    case BC_t::call_: {
        if (cs.isValid()) {
            printArgs(cs);
            printNames(cs);
            Rprintf("\n        -> ");
            Rf_PrintValue(cs.call());
            printProfile(cs);
        }
        break;
    }
    case BC_t::call_stack_: {
        num_args_t nargs = immediate.call_args.nargs;
        Rprintf(" %d ", nargs);
        if (cs.isValid()) {
            printNames(cs);
            Rprintf("\n        -> ");
            Rf_PrintValue(cs.call());
            printProfile(cs);
        }
        break;
    }
    case BC_t::static_call_stack_: {
        num_args_t nargs = immediate.call_args.nargs;
        Rprintf(" %d ", nargs);
        if (cs.isValid()) {
            Rprintf(" %p ", cs.target());
            Rprintf("\n        -> ");
            Rf_PrintValue(cs.call());
            printProfile(cs);
        }
        break;
    }
    case BC_t::dispatch_stack_: {
        if (cs.isValid()) {
            Rprintf(" `%s` ", CHAR(PRINTNAME(cs.selector())));
            Rprintf(" %d ", cs.nargs());
            printNames(cs);
            Rprintf("\n        -> ");
            Rf_PrintValue(cs.call());
            printProfile(cs);
        }
        break;
    }
    case BC_t::push_:
        Rprintf(" %u # ", immediate.pool);
        Rf_PrintValue(immediateConst());
        return;
    case BC_t::ldarg_:
    case BC_t::ldfun_:
    case BC_t::ldvar_:
    case BC_t::ldlval_:
    case BC_t::ldddvar_:
    case BC_t::stvar_:
    case BC_t::missing_:
        Rprintf(" %u # %s", immediate.pool, CHAR(PRINTNAME((immediateConst()))));
        break;
    case BC_t::guard_fun_: {
        SEXP name = Pool::get(immediate.guard_fun_args.name);
        Rprintf(" %s == %p", CHAR(PRINTNAME(name)),
                Pool::get(immediate.guard_fun_args.expected));
        break;
    }
    case BC_t::pick_:
    case BC_t::pull_:
    case BC_t::put_:
        Rprintf(" %i", immediate.i);
        break;
    case BC_t::is_:
    case BC_t::alloc_:
        Rprintf(" %s", type2char(immediate.i));
        break;
    case BC_t::guard_env_:
        Deoptimizer_print(immediate.guard_id);
        Rprintf("\n");
        break;
    case BC_t::force_:
    case BC_t::pop_:
    case BC_t::seq_:
    case BC_t::ret_:
    case BC_t::swap_:
    case BC_t::int3_:
    case BC_t::uniq_:
    case BC_t::dup_:
    case BC_t::inc_:
    case BC_t::dup2_:
    case BC_t::test_bounds_:
    case BC_t::asast_:
    case BC_t::asbool_:
    case BC_t::add_:
    case BC_t::mul_:
    case BC_t::div_:
    case BC_t::idiv_:
    case BC_t::mod_:
    case BC_t::pow_:
    case BC_t::sub_:
    case BC_t::lt_:
    case BC_t::return_:
    case BC_t::isfun_:
    case BC_t::invisible_:
    case BC_t::visible_:
    case BC_t::subset2_:
    case BC_t::extract2_:
    case BC_t::subset1_:
    case BC_t::extract1_:
    case BC_t::close_:
    case BC_t::length_:
    case BC_t::names_:
    case BC_t::set_names_:
    case BC_t::endcontext_:
    case BC_t::aslogical_:
    case BC_t::lgl_or_:
    case BC_t::lgl_and_:
    case BC_t::subassign_:
    case BC_t::subassign2_:
        break;
    case BC_t::promise_:
    case BC_t::push_code_:
        Rprintf(" %x", immediate.fun);
        break;
    case BC_t::beginloop_:
    case BC_t::brtrue_:
    case BC_t::brobj_:
    case BC_t::brfalse_:
    case BC_t::br_:
        Rprintf(" %d", immediate.offset);
        break;
    case BC_t::label:
        Rprintf("%d:", immediate.offset);
        break;
    }
    Rprintf("\n");
}

CallSite::CallSite(BC bc, CallSiteStruct* cs) : bc(bc), cs(cs) {
    assert(bc.isCallsite());
}

SEXP CallSite::selector() { return Pool::get(*CallSite_selector(cs)); }

SEXP CallSite::target() { return Pool::get(*CallSite_target(cs)); }

SEXP CallSite::call() { return Pool::get(cs->call); }

SEXP CallSite::name(pool_idx_t i) { return Pool::get(CallSite_names(cs)[i]); }
}
