#include "historyline.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>

namespace vt
{

namespace
{
constexpr uint16_t kHalf16 = 0xFFFF;     // the right half of a wide character, 16-bit form
constexpr uint32_t kHalf32 = 0xFFFFFFFF; // the same, 32-bit form

bool sameLook(const Cell &a, const Cell &b)
{
    return a.attrs == b.attrs && a.fg == b.fg && a.bg == b.bg;
}
}

HistoryLine::HistoryLine(const Cell *cells, int count, bool continuation)
{
    const int cols = std::clamp(count, 0, int(std::numeric_limits<uint16_t>::max()));
    int runCount = 0;
    bool wide = false;
    for (int c = 0; c < cols; ++c) {
        if (c == 0 || !sameLook(cells[c], cells[c - 1])) {
            ++runCount;
        }
        // 0xFFFF is the 16-bit marker: a line that holds it (or anything above) takes 32 bits.
        if (cells[c].width != 0 && cells[c].ch >= kHalf16) {
            wide = true;
        }
    }
    const std::size_t charSize = wide ? 4 : 2;
    const std::size_t size = kRunsOffset + std::size_t(runCount) * sizeof(Run) + std::size_t(cols) * charSize;
    m_data.reset(new uint8_t[size]);

    Header h{uint16_t(cols), uint16_t(runCount), uint8_t((continuation ? kContinuation : 0) | (wide ? kWide : 0))};
    std::memcpy(m_data.get(), &h, sizeof(h));

    uint8_t *runOut = m_data.get() + kRunsOffset;
    uint8_t *charOut = runOut + std::size_t(runCount) * sizeof(Run);
    for (int c = 0; c < cols; ++c) {
        const Cell &cell = cells[c];
        if (c == 0 || !sameLook(cell, cells[c - 1])) {
            const Run run{uint16_t(c), cell.attrs, cell.fg, cell.bg};
            std::memcpy(runOut, &run, sizeof(run));
            runOut += sizeof(Run);
        }
        if (wide) {
            const uint32_t ch = cell.width == 0 ? kHalf32 : uint32_t(cell.ch);
            std::memcpy(charOut + std::size_t(c) * 4, &ch, 4);
        } else {
            const uint16_t ch = cell.width == 0 ? kHalf16 : uint16_t(cell.ch);
            std::memcpy(charOut + std::size_t(c) * 2, &ch, 2);
        }
    }
}

Cell HistoryLine::cell(int col) const
{
    Cell out;
    if (!m_data) {
        return out;
    }
    const Header h = header();
    if (col < 0 || col >= h.cols) {
        return out;
    }
    const uint8_t *runBytes = m_data.get() + kRunsOffset;
    const uint8_t *charBytes = runBytes + std::size_t(h.runs) * sizeof(Run);
    const bool wide = h.flags & kWide;
    const auto charAt = [&](int c) -> uint32_t {
        if (wide) {
            uint32_t v;
            std::memcpy(&v, charBytes + std::size_t(c) * 4, 4);
            return v;
        }
        uint16_t v;
        std::memcpy(&v, charBytes + std::size_t(c) * 2, 2);
        return v == kHalf16 ? kHalf32 : v;
    };

    // The run that holds `col`: the last one starting at or before it (runs are few).
    int lo = 0, hi = h.runs - 1;
    while (lo < hi) {
        const int mid = (lo + hi + 1) / 2;
        uint16_t start;
        std::memcpy(&start, runBytes + std::size_t(mid) * sizeof(Run) + offsetof(Run, col), sizeof(start));
        if (start <= col) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    Run run;
    std::memcpy(&run, runBytes + std::size_t(lo) * sizeof(Run), sizeof(run));
    out.attrs = run.attrs;
    out.fg = run.fg;
    out.bg = run.bg;

    const uint32_t ch = charAt(col);
    if (ch == kHalf32) {
        out.ch = 0;
        out.width = 0;
    } else {
        out.ch = char32_t(ch);
        out.width = (col + 1 < h.cols && charAt(col + 1) == kHalf32) ? 2 : 1;
    }
    return out;
}

std::size_t HistoryLine::byteSize() const
{
    if (!m_data) {
        return 0;
    }
    const Header h = header();
    return kRunsOffset + std::size_t(h.runs) * sizeof(Run) + std::size_t(h.cols) * ((h.flags & kWide) ? 4 : 2);
}

} // namespace vt
