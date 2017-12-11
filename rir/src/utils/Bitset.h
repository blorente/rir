#ifndef RIR_BITSET_H
#define RIR_BITSET_H

#include <initializer_list>

namespace rir {

template <typename HOLDER, typename BASE_TYPE>
class BitSet {
    static_assert((std::size_t)BASE_TYPE::max < 1 << ((sizeof(HOLDER) * 8) - 1),
                  "Maximal base type does not fit in bitfield");

    HOLDER set_;

    HOLDER asSet(BASE_TYPE t) { return (1 << (std::size_t)t); }

  public:
    BitSet() : set_(0) {}

    BitSet(HOLDER set) : set_(set) {}

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

    bool operator==(BASE_TYPE t) { return asSet(t) == set_; }

    bool operator==(BitSet s) { return set_ == s.set_; }

    BitSet operator|(BitSet s) { return BitSet(s.set_ | set_); }

    std::size_t size() { return __builtin_popcount(set_); }
};
}

#endif
