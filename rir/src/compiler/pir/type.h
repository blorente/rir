#ifndef COMPILER_TYPE_H
#define COMPILER_TYPE_H

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "utils/Bitset.h"

namespace rir {
namespace pir {

enum class RType : uint8_t {
    unused,

    nil,
    symbol,
    logical,
    closure,
    integer,
    prom,
    cons,
    code,
    env,

    max
};

enum class NativeType : uint8_t {
    unused,

    test,

    max
};

struct PirType {

    typedef BitSet<uint16_t, RType> RTypeSet;
    typedef BitSet<uint8_t, NativeType> NativeTypeSet;

    bool lazy_ = false;
    bool missing_ = false;
    bool rtype_;

    union Type {
        RTypeSet r;
        NativeTypeSet n;
        Type(RTypeSet r) : r(r) {}
        Type(NativeTypeSet n) : n(n) {}
    };
    Type t_;

    PirType(RType t) : rtype_(true), t_(t) {
        assert(t > RType::unused && t < RType::max);
    }
    PirType(NativeType t) : rtype_(false), t_(t) {
        assert(t > NativeType::unused && t < NativeType::max);
    }
    PirType(RTypeSet t) : rtype_(true), t_(t) {}
    PirType(NativeTypeSet t) : rtype_(false), t_(t) {}

    void operator=(PirType o) {
        lazy_ = o.lazy_;
        missing_ = o.missing_;
        rtype_ = o.rtype_;
        if (rtype_)
            t_.r = o.t_.r;
        else
            t_.n = o.t_.n;
    }

    static PirType val() {
        static PirType t = RTypeSet() | RType::symbol | RType::logical |
                           RType::closure | RType::integer | RType::nil;
        return t;
    }
    static PirType valOrMissing() { return val().orMissing(); }
    static PirType valOrLazy() { return val().orLazy(); }
    static PirType list() { return PirType(RType::cons) | RType::nil; }
    static PirType any() { return val().orLazy().orMissing(); }

    bool maybeMissing() { return missing_; }

    bool maybeLazy() { return lazy_; }

    PirType orMissing() {
        assert(rtype_);
        PirType t = *this;
        t.missing_ = true;
        return t;
    }

    PirType orLazy() {
        assert(rtype_);
        PirType t = *this;
        t.lazy_ = true;
        return t;
    }

    PirType baseType() {
        assert(rtype_);
        return PirType(t_.r);
    }

    static PirType voyd() { return NativeTypeSet(); }

    static PirType missing() { return bottom().orMissing(); }

    static PirType bottom() { return PirType(RTypeSet()); }

    PirType operator|(PirType o) {
        if (!rtype_ && !o.rtype_) {
            return t_.n | o.t_.n;
        }
        PirType r = t_.r | o.t_.r;
        if (maybeLazy() || o.maybeLazy())
            r.lazy_ = true;
        if (maybeMissing() || o.maybeMissing())
            r.missing_ = true;
        return r;
    }

    bool operator==(NativeType o) { return !rtype_ && t_.n == o; }

    bool operator==(RType o) {
        return rtype_ && !lazy_ && !missing_ && t_.r == o;
    }

    bool operator!=(PirType o) { return !(*this == o); }

    bool operator==(PirType o) {
        return rtype_ == o.rtype_ && lazy_ == o.lazy_ &&
               missing_ == o.missing_ &&
               (rtype_ ? t_.r == o.t_.r : t_.n == o.t_.n);
    }

    bool operator>=(PirType o) {
        if (rtype_ != o.rtype_) {
            return false;
        }
        if (!rtype_) {
            return t_.n.includes(o.t_.n);
        }
        if (t_.r == o.t_.r) {
            return (lazy_ || !o.lazy_) && (missing_ || !o.missing_);
        }
        if (lazy_ != o.lazy_ || missing_ != o.missing_) {
            return false;
        }
        return t_.r.includes(o.t_.r);
    }

    void print();
};

inline std::ostream& operator<<(std::ostream& out, NativeType t) {
    switch (t) {
    case NativeType::test:
        out << "t";
        break;
    case NativeType::unused:
    case NativeType::max:
        assert(false);
        break;
    }
    return out;
}

inline std::ostream& operator<<(std::ostream& out, RType t) {
    switch (t) {
    case RType::env:
        out << "env";
        break;
    case RType::code:
        out << "code";
        break;
    case RType::cons:
        out << "cons";
        break;
    case RType::prom:
        out << "prom";
        break;
    case RType::nil:
        out << "nil";
        break;
    case RType::closure:
        out << "cls";
        break;
    case RType::symbol:
        out << "sym";
        break;
    case RType::integer:
        out << "int";
        break;
    case RType::logical:
        out << "lgl";
        break;
    case RType::unused:
    case RType::max:
        assert(false);
        break;
    }
    return out;
}

inline std::ostream& operator<<(std::ostream& out, PirType t) {
    if (!t.rtype_) {
        if (t.t_.n.size() == 0) {
            out << "void";
            return out;
        }

        std::vector<NativeType> ts;
        for (NativeType bt = NativeType::unused; bt != NativeType::max;
             bt = (NativeType)((size_t)bt + 1)) {
            if (t.t_.n.includes(bt))
                ts.push_back(bt);
        }
        if (ts.size() > 1)
            out << "(";
        for (auto i = ts.begin(); i != ts.end(); ++i) {
            out << *i;
            if (i + 1 != ts.end())
                out << "|";
        }
        if (ts.size() > 1)
            out << ")";
        return out;
    }

    if (t.baseType() >= PirType::val()) {
        out << "val";
    } else {
        std::vector<RType> ts;
        for (RType bt = RType::unused; bt != RType::max;
             bt = (RType)((size_t)bt + 1)) {
            if (t.t_.r.includes(bt))
                ts.push_back(bt);
        }
        if (ts.size() != 1)
            out << "(";
        for (auto i = ts.begin(); i != ts.end(); ++i) {
            out << *i;
            if (i + 1 != ts.end())
                out << "|";
        }
        if (ts.size() != 1)
            out << ")";
    }

    if (t.maybeLazy()) {
        out << "^";
    }
    if (t.maybeMissing()) {
        out << "?";
    }

    return out;
}
}
}

#endif
