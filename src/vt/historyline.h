#pragma once

#include "cell.h"

#include <cstdint>
#include <cstring>
#include <memory>

namespace vt
{

/**
 * One line of the scrollback, packed into a single block: the characters (16 bits each
 * when they all fit, else 32) and the runs of cells that share colours and attributes.
 * A line of 80 plain characters takes about 190 bytes instead of 80 cells of 16 (and a
 * container of its own). The right half of a wide character is stored as a marker; its
 * left half is the cell before it.
 */
class HistoryLine
{
public:
    HistoryLine() = default;
    /** `cells[0..count)`, as VtCore converts them; `continuation`: the line continues the one above. */
    HistoryLine(const Cell *cells, int count, bool continuation);

    int size() const { return m_data ? header().cols : 0; }
    bool continuation() const { return m_data && (header().flags & kContinuation); }
    /** The cell at `col`; a blank one past the end. */
    Cell cell(int col) const;
    /** Bytes the line takes on the heap (for tests and measurements). */
    std::size_t byteSize() const;

private:
    struct Header
    {
        uint16_t cols;
        uint16_t runs;
        uint8_t flags;
    };
    struct Run
    {
        uint16_t col; // first column of the run
        uint16_t attrs;
        Color fg;
        Color bg;
    };
    static constexpr uint8_t kContinuation = 1;
    static constexpr uint8_t kWide = 2; // 32-bit characters

    // The block is bytes: every field is read with memcpy (no aliasing, no alignment worries).
    Header header() const
    {
        Header h;
        std::memcpy(&h, m_data.get(), sizeof(h));
        return h;
    }

    static constexpr std::size_t kRunsOffset = (sizeof(Header) + alignof(Run) - 1) / alignof(Run) * alignof(Run);

    std::unique_ptr<uint8_t[]> m_data;
};

} // namespace vt
