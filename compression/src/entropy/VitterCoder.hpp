#pragma once

// Remark:
// VitterCoder.hpp, VitterCoder.cpp are LLM-generated.


// ============================================================================
//  Adaptive (one-pass) Huffman coding -- Vitter's Algorithm V.
// ============================================================================
//
//  Reference
//  ---------
//  J. S. Vitter, "Algorithm 673: Dynamic Huffman Coding",
//  ACM Transactions on Mathematical Software 15(2), June 1989, 158-167,
//  with the companion analysis
//  J. S. Vitter, "Design and Analysis of Dynamic Huffman Codes",
//  Journal of the ACM 34(4), Oct 1987, 825-845.
//
//  What the algorithm does
//  -----------------------
//  Encoder and decoder each maintain the same Huffman tree for the first t
//  symbols of the message and use it to code symbol t+1, so no probability
//  table is ever transmitted. A symbol costs exactly the depth of its leaf in
//  bits, and the work per symbol is proportional to that depth, so the coder
//  runs in real time.
//
//  Algorithm V differs from the older FGK scheme in two respects, which
//  together make it optimal among one-pass Huffman methods (at most one extra
//  bit per symbol over the two-pass optimum, which is the best possible):
//
//    * nodes carry an *implicit* numbering -- increasing bottom to top and,
//      within a level, left to right;
//    * an invariant is maintained that every leaf of weight w precedes, in
//      that numbering, every internal node of weight w.
//
//  Holding the invariant needs an operation FGK does not have: a node may have
//  to "slide" past a whole run of equally weighted nodes. The floating tree is
//  what makes that slide O(1).
//
//  The floating tree
//  -----------------
//  Nodes carry no explicit parent or child pointers. They are grouped into
//  blocks -- maximal runs of same-weight nodes of the same kind (all leaves or
//  all internal) -- and only the block stores links, relative to its leader
//  (its highest-numbered node). Links for the other nodes of a block are
//  recovered by offset arithmetic, because
//
//    * leaves occupy explicit slots 1..n and internal nodes n+1..2n-1, each
//      range contiguous and ordered consistently with the implicit numbering,
//      and
//    * the parents of implicitly adjacent pairs are themselves adjacent, so
//      walking k nodes down a block moves the parent by k/2.
//
//  Sliding therefore rewrites a constant number of block fields instead of one
//  pointer per node, which is the whole point of the design.
//
//  Zero-weight symbols
//  -------------------
//  Unseen symbols are not in the tree. They share a single weight-zero leaf,
//  the "0-node". Emitting an unseen symbol costs the path to the 0-node plus a
//  phase-in (minimum prefix) code picking one of the M still-unseen symbols:
//  with M = 2^E + R, index v < 2R takes E+1 bits and the rest take E bits. The
//  0-node is then split so the new symbol gets a leaf of its own.
//
//  Departures from the published listing
//  -------------------------------------
//  The algorithm is transcribed faithfully; only the representation differs,
//  in three ways that matter for speed.
//
//    1. All eight per-block fields live in one 32-byte struct rather than
//       eight parallel arrays, so touching a block costs one cache line.
//    2. The published `parent` and `parity` fields are fused into one word,
//       pp = 2*parent + parity. Besides freeing a slot, it collapses the climb
//       to three operations (see encode) and turns two of the fiddlier pointer
//       updates into a plain `--pp`.
//    3. The block records *both* children of its leader, not just the right
//       one. The published FindChild reaches the left child by walking back
//       into the preceding block, and measurement says that walk is taken on
//       roughly half of all descent steps -- it is the common case, not the
//       rare one. Caching it turns a five-load dependency chain into two.
//
//  Limits
//  ------
//  Weights are 32-bit, so one coder instance must not be fed 2^32 or more
//  symbols. That bound also keeps any codeword inside a single 64-bit word: a
//  Huffman leaf at depth d forces a total weight of at least Fibonacci(d+2),
//  and F(47) already exceeds 2^32, so no path runs deeper than ~46 edges. The
//  code still handles longer paths correctly, it just does not optimise them.
//
//  Bitstreams
//  ----------
//  Bits go in and out through the shared io/BitWriter and io/BitReader, so a
//  Vitter-coded section can sit in the same stream as anything else this
//  library writes.

#include <bit>
#include <cstdint>
#include <span>
#include <vector>

#include "io/BitReader.hpp"
#include "io/BitWriter.hpp"

// ============================================================================
//  VitterCoder
// ============================================================================
//
//  Both directions of the code, because they are the same state machine: the
//  tree and its update are identical, only the walk differs. VitterEncoder
//  and VitterDecoder below expose one direction each; prefer those, and reach
//  for VitterCoder itself only when one object really has to do both.
//
//  One instance models one direction of one stream. Encoder and decoder must
//  be built with the same alphabet size and fed the same symbol sequence for
//  their trees to stay in lockstep.
//
//  Symbols are 0-based in the public API (0 .. alphabet_size-1); internally
//  everything is 1-based so the published listing transcribes verbatim.
class VitterCoder {
public:
    // Lower bound: reset() needs a non-degenerate block free list.
    // Upper bound: pp = 2*parent + parity must fit in 32 bits.
    static constexpr std::uint32_t kMinAlphabetSize = 2;
    static constexpr std::uint32_t kMaxAlphabetSize = 1u << 30;

    explicit VitterCoder(std::uint32_t alphabet_size);

    // Returns the model to its initial state (nothing seen yet).
    void reset() noexcept;

    [[nodiscard]] std::uint32_t alphabet_size() const noexcept { return n_; }

    // Writes `symbol` and folds it into the model.
    void encode(std::uint32_t symbol, BitWriter& out) noexcept;

    // Bulk form; identical in output to a loop over encode().
    void encode(std::span<const std::uint32_t> symbols, BitWriter& out) noexcept;

    // Reads one symbol and folds it into the model.
    [[nodiscard]] std::uint32_t decode(BitReader& in) noexcept;

    // Bulk form; fills `out` completely.
    void decode(BitReader& in, std::span<std::uint32_t> out) noexcept;

    // Bits that encode(symbol, ...) would emit right now, without emitting or
    // updating anything. Useful for cost estimation.
    [[nodiscard]] unsigned code_length(std::uint32_t symbol) const noexcept;

    // Times `symbol` has been coded so far.
    [[nodiscard]] std::uint32_t weight(std::uint32_t symbol) const noexcept;

private:
    // Everything the algorithm needs to know about one block, in 32 bytes, so
    // that a block never straddles a cache line and one fill answers both the
    // climb and the descent.
    struct alignas(32) Block {
        std::uint32_t weight;   // common weight of every node in the block
        std::uint32_t pp;       // 2 * parent-of-leader + parity-of-leader
        std::uint32_t child[2]; // leader's left child, leader's right child
        std::uint32_t first;    // leader: highest-numbered node of the block
        std::uint32_t last;     // lowest-numbered node of the block
        std::uint32_t prev;     // previous block on the circular weight-ordered list
        std::uint32_t next;     // next block there, or free-list link when unused
    };
    static_assert(sizeof(Block) == 32);

    // Branch indices into Block::child, matching the bit values a codeword
    // uses: 0 descends left, 1 descends right.
    static constexpr std::uint32_t kLeft = 0;
    static constexpr std::uint32_t kRight = 1;

    [[nodiscard]] Block* blocks() noexcept { return blocks_.data(); }
    [[nodiscard]] const Block* blocks() const noexcept { return blocks_.data(); }
    [[nodiscard]] std::uint32_t* nb() noexcept { return node_block_.data(); }
    [[nodiscard]] const std::uint32_t* nb() const noexcept { return node_block_.data(); }

    // Child of node `j` along `branch`. Nearly every query lands on a block
    // leader, whose two children the block already holds, so the fast path is
    // one indirection plus one indexed load. Always inlined: it is the body of
    // the decoder's descent loop, and out of line the dependency chain picks
    // up a reload of every base pointer.
    //
    // The bases come in as arguments rather than being read from the member
    // vectors because the descent loop contains a call the compiler cannot see
    // through -- the reader's buffer refill -- and would otherwise reload both
    // of them on every bit. Neither vector is ever resized after construction,
    // so hoisting them around a loop is always sound.
    [[nodiscard, gnu::always_inline]] std::uint32_t find_child(const Block* B,
                                                               const std::uint32_t* NB,
                                                               std::uint32_t j,
                                                               std::uint32_t branch) const noexcept;

    [[nodiscard, gnu::always_inline]] std::uint32_t find_child(std::uint32_t j,
                                                               std::uint32_t branch) const noexcept {
        return find_child(blocks(), nb(), j, branch);
    }

    // Cold tail of find_child: `j` sits below its leader, so the child has to
    // be found by walking down the implicit order, possibly across blocks.
    [[nodiscard, gnu::noinline]] std::uint32_t find_child_deep(const Block& bj, std::uint32_t j,
                                                               std::uint32_t branch) const noexcept;

    // Node immediately below `v` in the implicit numbering.
    [[nodiscard]] std::uint32_t implicit_pred(std::uint32_t v) const noexcept;

    // Restores child[kLeft] from child[kRight] after either was disturbed.
    void refresh_left_child(std::uint32_t b) noexcept;

    // Swaps which symbols two equally weighted leaves stand for.
    void interchange_leaves(std::uint32_t e1, std::uint32_t e2) noexcept;

    // Folds one more occurrence of the symbol currently held by leaf `leaf`.
    void update(std::uint32_t leaf) noexcept;

    // The branch of update() that pulls a symbol out of the zero-weight pool
    // and splits the 0-node. It runs at most once per alphabet symbol for a
    // whole message, so it is kept out of line and out of the way.
    [[gnu::noinline]] void update_first_occurrence(std::uint32_t leaf) noexcept;

    // Adds 1 to the weight of leader `q`, sliding it past the next block when
    // the invariant demands it, and returns the node one level up that needs
    // the same treatment, or 0 at the root. An update does at most one leaf
    // step and then only internal ones, so the two are split: the internal
    // form is the loop body and deserves the tighter code.
    [[gnu::always_inline]] std::uint32_t slide_and_increment_leaf(std::uint32_t q) noexcept;
    [[gnu::always_inline]] std::uint32_t slide_and_increment_internal(std::uint32_t q) noexcept;
    [[gnu::noinline]] std::uint32_t slide_and_increment_slow(std::uint32_t q,
                                                             bool q_is_leaf) noexcept;

    // Shared body of decode(). Always inlined: it is three instructions plus
    // the descent, and a call here would cost more than the body.
    [[nodiscard, gnu::always_inline]] std::uint32_t decode_one(BitReader& in) noexcept;

    // Codeword scratch, for the hypothetical path longer than 64 edges. A path
    // cannot in fact exceed ~46 edges while weights fit in 32 bits (see the
    // Fibonacci bound above), so this is a safety net that never fills.
    //
    // It is a member and not a local because a local array makes the compiler
    // fit encode() with a stack-protector prologue, and that is three loads, a
    // store and a compare on every symbol for scratch space that is never used.
    static constexpr unsigned kSpillWords = 4;
    std::uint64_t spill_[kSpillWords] = {};

    std::uint32_t n_ = 0; // alphabet size
    std::uint32_t z_ = 0; // 2n-1: highest node index, and the root once M < n

    std::int32_t m_ = 0; // number of still-unseen symbols; the 0-node is node m_
    std::int32_t e_ = 0; // floor(log2(m_))
    std::int32_t r_ = 0; // m_ - 2^e_, or -1 when m_ == 0

    std::uint32_t avail_ = 0; // head of the free-block list, 0 if empty

    std::vector<Block> blocks_;             // indexed 1 .. 2n-1
    std::vector<std::uint32_t> node_block_; // node -> block id, indexed 1 .. 2n-1
    std::vector<std::uint32_t> alpha_;      // leaf node -> 1-based letter
    std::vector<std::uint32_t> rep_;        // 1-based letter -> leaf node
};

// ============================================================================
//  VitterEncoder / VitterDecoder
// ============================================================================
//
//  One direction each, so neither half of the API is reachable on an object
//  used for the other. There is no second implementation behind them: both
//  are VitterCoder with the wrong direction hidden, which is what private
//  inheritance buys. The tree and its update stay in one place, since encoder
//  and decoder diverging by even one line would corrupt every stream.
//
//  Same size as VitterCoder, no vtable, nothing to call through.

class VitterEncoder : private VitterCoder {
public:
    using VitterCoder::VitterCoder;

    using VitterCoder::alphabet_size;
    using VitterCoder::code_length;
    using VitterCoder::encode;
    using VitterCoder::reset;
    using VitterCoder::weight;

    using VitterCoder::kMaxAlphabetSize;
    using VitterCoder::kMinAlphabetSize;
};

class VitterDecoder : private VitterCoder {
public:
    using VitterCoder::VitterCoder;

    using VitterCoder::alphabet_size;
    using VitterCoder::decode;
    using VitterCoder::reset;
    using VitterCoder::weight;

    using VitterCoder::kMaxAlphabetSize;
    using VitterCoder::kMinAlphabetSize;
};

static_assert(sizeof(VitterEncoder) == sizeof(VitterCoder));
static_assert(sizeof(VitterDecoder) == sizeof(VitterCoder));
