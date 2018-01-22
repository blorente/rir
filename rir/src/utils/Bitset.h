#ifndef RIR_BITSET_H
#define RIR_BITSET_H

#include <initializer_list>

namespace rir {

template <typename STORE, typename BASE_TYPE>
class BitSet {
    static_assert((std::size_t)BASE_TYPE::max <= (sizeof(STORE) * 8),
                  "Maximal base type does not fit in bitfield");
    STORE set_;

    STORE asSet(BASE_TYPE t) { return (1 << (std::size_t)t); }

  public:
    typedef STORE Store;
    typedef BASE_TYPE Base;

    BitSet() : set_(0) {}

    BitSet(STORE set) : set_(set) {}

    BitSet(std::initializer_list<BASE_TYPE> ts) : set_(0) {
        for (auto t : ts)
            set(t);
    }

    BitSet(BASE_TYPE t) : set_(asSet(t)) {}

    void set(BASE_TYPE t) {
        assert(t < BASE_TYPE::max);
        set_ |= asSet(t);
    }

    bool contains(BASE_TYPE t) {
        assert(t < BASE_TYPE::max);
        return set_ & asSet(t);
    }

    bool includes(BitSet s) { return (s.set_ & set_) == s.set_; }

    void operator=(BitSet o) { set_ = o.set_; }

    bool operator==(BASE_TYPE t) { return asSet(t) == set_; }

    bool operator==(BitSet s) { return set_ == s.set_; }

    BitSet operator|(BitSet s) { return BitSet(s.set_ | set_); }

    BitSet operator|(BASE_TYPE t) { return *this | BitSet(t); }

    std::size_t size() { return __builtin_popcount(set_); }
};
}

#endif
