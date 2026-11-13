#include "VitterCoder.hpp"

#include <cassert>
#include <stdexcept>

namespace {

// BitWriter deliberately takes at most 32 bits at a time. A codeword cannot
// actually exceed ~46 edges (see the Fibonacci bound in the header), but the
// climb below is written for the general case, so split whatever it produces.
inline void put_wide(BitWriter& out, std::uint64_t value, unsigned count) noexcept {
    if (count > 32) [[unlikely]] {
        out.put_bits(static_cast<std::uint32_t>(value >> 32), count - 32);
        count = 32;
        value &= 0xFFFFFFFFull;
    }
    out.put_bits(static_cast<std::uint32_t>(value), count);
}

} // namespace

// ----------------------------------------------------------------------------
//  Construction
// ----------------------------------------------------------------------------

VitterCoder::VitterCoder(std::uint32_t alphabet_size) {
    if (alphabet_size < kMinAlphabetSize || alphabet_size > kMaxAlphabetSize) {
        throw std::invalid_argument("VitterCoder: alphabet size out of range");
    }
    n_ = alphabet_size;
    z_ = 2 * n_ - 1;

    // Slot 0 of every array goes unused so the published 1-based indexing can
    // be transcribed verbatim.
    blocks_.resize(static_cast<std::size_t>(z_) + 1);
    node_block_.resize(static_cast<std::size_t>(z_) + 1);
    alpha_.resize(static_cast<std::size_t>(n_) + 1);
    rep_.resize(static_cast<std::size_t>(n_) + 1);

    reset();
}

void VitterCoder::reset() noexcept {
    m_ = static_cast<std::int32_t>(n_);
    e_ = static_cast<std::int32_t>(std::bit_width(n_)) - 1;
    r_ = static_cast<std::int32_t>(n_ - (1u << e_));

    for (std::uint32_t i = 1; i <= n_; ++i) {
        alpha_[i] = i;
        rep_[i] = i;
    }

    // The initial tree is one leaf: node n, the 0-node, alone in block 1.
    node_block_[n_] = 1;
    Block& zero = blocks_[1];
    zero.weight = 0;
    zero.pp = 0; // no parent, parity 0
    zero.child[kLeft] = 0;
    zero.child[kRight] = 0;
    zero.first = n_;
    zero.last = n_;
    zero.prev = 1;
    zero.next = 1;

    // Blocks 2 .. 2n-1 start out on the free list.
    avail_ = 2;
    for (std::uint32_t b = 2; b < z_; ++b) {
        blocks_[b].next = b + 1;
    }
    blocks_[z_].next = 0;
}

// ----------------------------------------------------------------------------
//  Encoding
// ----------------------------------------------------------------------------

void VitterCoder::encode(std::uint32_t symbol, BitWriter& out) noexcept {
    const std::uint32_t unseen = static_cast<std::uint32_t>(m_);
    const std::uint32_t leaf = rep_[symbol + 1];
    std::uint32_t q = leaf;

    // An unseen symbol is coded as the path to the 0-node followed by a
    // phase-in index picking it out of the `unseen` candidates. The index
    // trails the path, so hold it back and emit it last.
    std::uint32_t escape = 0;
    unsigned escape_bits = 0;
    if (q <= unseen) {
        const std::uint32_t r = static_cast<std::uint32_t>(r_);
        std::uint32_t v = q - 1;
        if (v < 2u * r) {
            escape_bits = static_cast<unsigned>(e_) + 1;
        } else {
            v -= r;
            escape_bits = static_cast<unsigned>(e_);
        }
        escape = v;
        q = unseen;
    }

    const std::uint32_t root = (unseen == n_) ? n_ : z_;
    const Block* const B = blocks();
    const std::uint32_t* const NB = nb();

    // Climb to the root, building the path bottom-up.
    //
    // With pp = 2*parent + parity and delta = first - q, the published
    // "bit = (delta + parity) & 1" and "q = parent - ((delta + 1 - parity)/2)"
    // collapse into one quantity s = pp - delta: its low bit is the branch bit
    // and its upper bits are the parent. (Both halves follow from s and pp - 1
    // agreeing mod 2 with delta + parity.)
    //
    // Iteration i produces the bit at depth len-1-i, so parking it at bit i of
    // `code` and writing `code` most-significant-bit first replays the path in
    // root-to-leaf order with one store.
    std::uint64_t code = 0;
    unsigned len = 0;
    unsigned spilled = 0;

    while (q != root) {
        const Block& b = B[NB[q]];
        const std::uint32_t s = b.pp - b.first + q;
        code |= static_cast<std::uint64_t>(s & 1u) << len;
        q = s >> 1;
        if (++len == 64) [[unlikely]] {
            spill_[spilled & (kSpillWords - 1)] = code;
            ++spilled;
            code = 0;
            len = 0;
        }
    }
    assert(spilled <= kSpillWords);

    put_wide(out, code, len);
    for (unsigned i = spilled; i-- > 0;) {
        put_wide(out, spill_[i & (kSpillWords - 1)], 64);
    }
    if (escape_bits != 0) [[unlikely]] {
        out.put_bits(escape, escape_bits);
    }

    update(leaf);
}

void VitterCoder::encode(std::span<const std::uint32_t> symbols, BitWriter& out) noexcept {
    for (const std::uint32_t symbol : symbols) {
        encode(symbol, out);
    }
}

unsigned VitterCoder::code_length(std::uint32_t symbol) const noexcept {
    const std::uint32_t unseen = static_cast<std::uint32_t>(m_);
    std::uint32_t q = rep_[symbol + 1];

    unsigned len = 0;
    if (q <= unseen) {
        const std::uint32_t r = static_cast<std::uint32_t>(r_);
        len = (q - 1 < 2u * r) ? static_cast<unsigned>(e_) + 1 : static_cast<unsigned>(e_);
        q = unseen;
    }

    const std::uint32_t root = (unseen == n_) ? n_ : z_;
    const Block* const B = blocks();
    const std::uint32_t* const NB = nb();
    while (q != root) {
        const Block& b = B[NB[q]];
        q = (b.pp - b.first + q) >> 1;
        ++len;
    }
    return len;
}

std::uint32_t VitterCoder::weight(std::uint32_t symbol) const noexcept {
    const std::uint32_t q = rep_[symbol + 1];
    if (q <= static_cast<std::uint32_t>(m_)) {
        return 0;
    }
    return blocks_[node_block_[q]].weight;
}

// ----------------------------------------------------------------------------
//  Tree navigation
// ----------------------------------------------------------------------------

std::uint32_t VitterCoder::find_child(const Block* B, const std::uint32_t* NB, std::uint32_t j,
                                      std::uint32_t branch) const noexcept {
    const Block& bj = B[NB[j]];
    // The block already holds both children of its leader, and a descent asks
    // about the leader on ~95% of steps, so load the answer unconditionally and
    // only then check whether it applies. Keeping the check off the dependency
    // chain leaves two loads between successive levels.
    const std::uint32_t child = bj.child[branch];
    if (bj.first == j) [[likely]] {
        return child;
    }
    return find_child_deep(bj, j, branch);
}

std::uint32_t VitterCoder::find_child_deep(const Block& bj, std::uint32_t j,
                                           std::uint32_t branch) const noexcept {
    // `j` sits below its leader. The children of a block's nodes form one
    // descending run of the implicit order that ends at the leader's right
    // child, two children per node, so step down from there. The run can leave
    // the right child's own block, both because blocks are bounded and because
    // leaves and internal nodes occupy disjoint explicit ranges.
    //
    // Reading only child[kRight] here matters: update() calls find_child while
    // a slide has already rewritten child[kRight] but not yet child[kLeft].
    std::uint32_t steps = 2u * (bj.first - j) + 1u - branch;
    std::uint32_t v = bj.child[kRight];
    for (;;) {
        const Block& bv = blocks()[nb()[v]];
        const std::uint32_t gap = v - bv.last;
        if (steps <= gap) {
            return v - steps;
        }
        steps -= gap + 1u;
        v = blocks()[bv.prev].first;
    }
}

std::uint32_t VitterCoder::implicit_pred(std::uint32_t v) const noexcept {
    const Block& bv = blocks()[nb()[v]];
    return (v > bv.last) ? (v - 1) : blocks()[bv.prev].first;
}

void VitterCoder::refresh_left_child(std::uint32_t b) noexcept {
    Block& blk = blocks()[b];
    blk.child[kLeft] = implicit_pred(blk.child[kRight]);
}

// ----------------------------------------------------------------------------
//  Decoding
// ----------------------------------------------------------------------------

std::uint32_t VitterCoder::decode_one(BitReader& in) {
    const auto unseen = static_cast<std::uint32_t>(m_);
    // A copy, not a re-read: update() may write anywhere in the tree, and n_
    // reloaded inside the loop would be one more dependency per level.
    const std::uint32_t n = n_;
    const Block* const B = blocks();
    const std::uint32_t* const NB = nb();

    std::uint32_t q = (unseen == n) ? n : z_;
    while (q > n) {
        q = find_child(B, NB, q, in.get_bit());
    }

    if (q == unseen) {
        // Landed on the 0-node: read the phase-in index of a brand new symbol.
        auto v = static_cast<uint32_t>(in.get_bits(static_cast<unsigned>(e_)));
        if (static_cast<std::int32_t>(v) < r_) {
            v = 2u * v + in.get_bit();
        } else {
            v += static_cast<std::uint32_t>(r_);
        }
        q = v + 1;
    }

    const std::uint32_t symbol = alpha_[q] - 1;
    update(q);
    return symbol;
}

std::uint32_t VitterCoder::decode(BitReader& in) { return decode_one(in); }

void VitterCoder::decode(BitReader& in, std::span<std::uint32_t> out) {
    for (std::uint32_t& symbol : out) {
        symbol = decode_one(in);
    }
}

// ----------------------------------------------------------------------------
//  Model update
// ----------------------------------------------------------------------------

void VitterCoder::interchange_leaves(std::uint32_t e1, std::uint32_t e2) noexcept {
    // A leaf is usually alone in its block, so it usually already is the
    // leader and there is nothing to swap. Checking first keeps four scattered
    // accesses out of the common path.
    if (e1 == e2) {
        return;
    }
    const std::uint32_t a1 = alpha_[e1];
    const std::uint32_t a2 = alpha_[e2];
    rep_[a1] = e2;
    rep_[a2] = e1;
    alpha_[e1] = a2;
    alpha_[e2] = a1;
}

void VitterCoder::update(std::uint32_t leaf) noexcept {
    const std::uint32_t unseen = static_cast<std::uint32_t>(m_);
    if (leaf <= unseen) [[unlikely]] {
        // Happens at most once per alphabet symbol over the whole message.
        update_first_occurrence(leaf);
        return;
    }

    Block* const B = blocks();
    std::uint32_t* const NB = nb();

    // Swap the symbol onto the leader of its block: the only node of that
    // weight from which a slide is legal.
    const std::uint32_t leader = B[NB[leaf]].first;
    interchange_leaves(leaf, leader);

    std::uint32_t q = leader;
    std::uint32_t leaf_to_increment = 0;
    if (q == unseen + 1 && unseen > 0) [[unlikely]] {
        // q is the 0-node's sibling and must not drop below it, so climb from
        // its parent and bump the leaf afterwards.
        leaf_to_increment = q;
        q = B[NB[q]].pp >> 1;
    } else {
        q = slide_and_increment_leaf(q);
    }

    while (q != 0) {
        q = slide_and_increment_internal(q);
    }
    if (leaf_to_increment != 0) [[unlikely]] {
        // Its ancestors were already bumped by the loop above.
        slide_and_increment_leaf(leaf_to_increment);
    }
}

void VitterCoder::update_first_occurrence(std::uint32_t leaf) noexcept {
    Block* const B = blocks();
    std::uint32_t* const NB = nb();

    // Move the symbol into the 0-node's slot and shrink the zero-weight pool,
    // preserving m_ = 2^e_ + r_.
    const std::uint32_t unseen = static_cast<std::uint32_t>(m_);
    interchange_leaves(leaf, unseen);
    if (r_ == 0) {
        r_ = m_ >> 1;
        if (r_ > 0) {
            --e_;
        }
    }
    --m_;
    --r_;

    std::uint32_t q = unseen;
    std::uint32_t leaf_to_increment = 0;
    const std::uint32_t bq = NB[q];

    if (m_ > 0) {
        // Split the 0-node in two: node m_ becomes the new 0-node, node m_+1
        // (== q) the fresh leaf, node m_+n their parent. Both zero-weight
        // leaves stay in the old block; the parent gets a new zero-weight
        // internal block immediately after it.
        const std::uint32_t zero_node = static_cast<std::uint32_t>(m_);
        const std::uint32_t new_parent = zero_node + n_;
        const std::uint32_t old_pp = B[bq].pp;

        NB[zero_node] = bq;
        B[bq].last = zero_node;
        B[bq].pp = 2u * new_parent + 1u; // leader m_+1 is now a right child

        const std::uint32_t b = avail_;
        avail_ = B[b].next;
        B[b].prev = bq;
        B[b].next = B[bq].next;
        B[B[bq].next].prev = b;
        B[bq].next = b;
        B[b].pp = old_pp & ~1u; // inherits the parent, as a left child
        B[b].child[kRight] = q;
        B[b].child[kLeft] = zero_node;
        B[b].weight = 0;
        B[b].first = new_parent;
        B[b].last = new_parent;
        NB[new_parent] = b;

        // Two nodes appeared at the bottom of the implicit order, so whatever
        // used to follow the old 0-node now follows m_+n instead. The leader
        // holding it as a right child needs a fresh left child.
        const std::uint32_t old_parent = old_pp >> 1;
        if (old_parent != 0) {
            refresh_left_child(NB[old_parent]);
        }

        leaf_to_increment = q;
        q = new_parent;
    }

    if (q <= n_) {
        q = slide_and_increment_leaf(q);
    }
    while (q != 0) {
        q = slide_and_increment_internal(q);
    }
    if (leaf_to_increment != 0) {
        slide_and_increment_leaf(leaf_to_increment);
    }
}

std::uint32_t VitterCoder::slide_and_increment_leaf(std::uint32_t q) noexcept {
    Block* const B = blocks();
    Block& bq = B[nb()[q]];
    const std::uint32_t w = bq.weight;
    const Block& next = B[bq.next];

    // A leaf slides only over an internal block of equal weight, and merges
    // only into a leaf block of weight w+1, so one comparison rules out both.
    const std::uint32_t blocking = (next.first <= n_) ? w + 1u : w;
    if (bq.last == q && next.weight != blocking) [[likely]] {
        bq.weight = w + 1u;
        return bq.pp >> 1;
    }
    return slide_and_increment_slow(q, true);
}

std::uint32_t VitterCoder::slide_and_increment_internal(std::uint32_t q) noexcept {
    Block* const B = blocks();
    Block& bq = B[nb()[q]];
    const std::uint32_t w = bq.weight;
    const Block& next = B[bq.next];

    // An internal node both slides and merges only against weight w+1,
    // whatever the next block's kind, so a single comparison rules out both.
    if (bq.last == q && next.weight != w + 1u) [[likely]] {
        bq.weight = w + 1u;
        return bq.pp >> 1;
    }
    return slide_and_increment_slow(q, false);
}

std::uint32_t VitterCoder::slide_and_increment_slow(std::uint32_t q, bool q_is_leaf) noexcept {
    Block* const B = blocks();
    std::uint32_t* const NB = nb();

    // On entry q is the leader (highest-numbered node) of its block.
    const std::uint32_t bq = NB[q];
    std::uint32_t nbq = B[bq].next;
    const std::uint32_t pp = B[bq].pp;
    const std::uint32_t par = pp >> 1;
    const std::uint32_t w = B[bq].weight;
    std::uint32_t old_pp = pp;
    bool slid = false;

    // Blocks whose cached left child must be rebuilt once the structure has
    // settled. Doing it at the end is what keeps the cache honest: a slide
    // reorders the implicit numbering, so implicit_pred is only meaningful
    // after the block list has been relinked.
    std::uint32_t refresh[4] = {0, 0, 0, 0};
    unsigned refresh_count = 0;

    // The invariant puts leaves of weight w ahead of internal nodes of weight
    // w. Raising q to w+1 can therefore force it over the entire next block: a
    // leaf jumps the internals it ties with, an internal jumps the leaves that
    // are about to tie with it.
    if (q_is_leaf ? (B[nbq].first > n_ && B[nbq].weight == w)
                  : (B[nbq].first <= n_ && B[nbq].weight == w + 1u)) {
        slid = true;
        old_pp = B[nbq].pp;

        // A slide moves q above the whole of nbq, which changes exactly three
        // implicit-predecessor relations: those of q, of nbq's bottom node and
        // of the node that ends up just above q. The first two hang off q's
        // old parent (handled with the right-child fixups below); the third
        // hangs off the attachment point q is about to inherit, whose leader
        // keeps its right child and so is otherwise never touched.
        if (old_pp >= 2) {
            refresh[refresh_count++] = NB[old_pp >> 1];
        }

        if (par != 0) {
            // q and the jumped block exchange implicit positions, so the right
            // child recorded one level up has to follow. At most two parent
            // blocks are involved, since the parents of the nodes that moved
            // span at most two implicit slots.
            const std::uint32_t bpar = NB[par];
            std::uint32_t& rt = B[bpar].child[kRight];
            if (rt == q) {
                rt = B[nbq].last;
            } else if (rt == B[nbq].first) {
                rt = q;
            } else {
                rt += 1;
            }
            refresh[refresh_count++] = bpar;

            if (par != z_) {
                const std::uint32_t bpar_next = NB[par + 1];
                if (bpar_next != bpar) {
                    std::uint32_t& rt_next = B[bpar_next].child[kRight];
                    if (rt_next == B[nbq].first) {
                        rt_next = q;
                    } else if (NB[rt_next] == nbq) {
                        rt_next += 1;
                    }
                    refresh[refresh_count++] = bpar_next;
                }
            }
        }

        // Every node of nbq drops one implicit slot. In packed form the pair
        // (parent - 1 + parity, 1 - parity) is exactly pp - 1.
        assert(B[nbq].pp >= 2 && "slid past the root");
        B[nbq].pp -= 1;
        nbq = B[nbq].next;
    }

    if (q_is_leaf == (B[nbq].first <= n_) && B[nbq].weight == w + 1u) {
        // Weight w+1 already has a block of the right kind: join it.
        NB[q] = nbq;
        B[nbq].last = q;
        if (B[bq].last == q) {
            // q was alone in bq, so bq disappears.
            B[B[bq].prev].next = B[bq].next;
            B[B[bq].next].prev = B[bq].prev;
            B[bq].next = avail_;
            avail_ = bq;
        } else {
            // bq keeps the nodes below q, and its leader becomes q-1.
            if (!q_is_leaf) {
                B[bq].child[kRight] = find_child(q - 1, kRight);
                refresh[refresh_count++] = bq;
            }
            B[bq].pp -= 1; // flips parity, and drops parent when parity was 0
            B[bq].first = q - 1;
        }
    } else if (B[bq].last == q) {
        // q is alone in its block, so the block itself just gets heavier.
        if (slid) {
            // Relink bq behind the block it jumped, taking over that block's
            // old attachment point.
            B[B[bq].next].prev = B[bq].prev;
            B[B[bq].prev].next = B[bq].next;
            B[bq].prev = B[nbq].prev;
            B[bq].next = nbq;
            B[nbq].prev = bq;
            B[B[bq].prev].next = bq;
            B[bq].pp = old_pp;
        }
        B[bq].weight = w + 1u;
    } else {
        // q leaves the block it shared for a fresh one of weight w+1, inserted
        // just before nbq.
        const std::uint32_t b = avail_;
        avail_ = B[b].next;
        NB[q] = b;
        B[b].first = q;
        B[b].last = q;

        bool parent_becomes_q = false;
        if (!q_is_leaf) {
            // q keeps its own children; bq's new leader q-1 needs fresh ones.
            B[b].child[kLeft] = B[bq].child[kLeft];
            B[b].child[kRight] = B[bq].child[kRight];
            const std::uint32_t rt = find_child(q - 1, kRight);
            parent_becomes_q = (B[b].child[kRight] == q - 1);
            B[bq].child[kRight] = rt;
            refresh[refresh_count++] = bq;
        }
        if (parent_becomes_q) {
            B[bq].pp = 2u * q + 1u - (B[bq].pp & 1u);
        } else {
            B[bq].pp -= 1;
        }
        B[bq].first = q - 1;

        B[b].prev = B[nbq].prev;
        B[b].next = nbq;
        B[nbq].prev = b;
        B[B[b].prev].next = b;
        B[b].weight = w + 1u;
        B[b].pp = old_pp;
    }

    for (unsigned i = 0; i < refresh_count; ++i) {
        refresh_left_child(refresh[i]);
    }

    // A leaf hands off to the parent it had before any slide; an internal node
    // keeps the parent it started with.
    return q_is_leaf ? (old_pp >> 1) : par;
}
