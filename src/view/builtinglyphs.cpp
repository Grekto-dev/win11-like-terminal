#include "builtinglyphs.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace
{
// Weights of an arm: 0 none, 1 light, 2 heavy, 3 double.
struct Arms
{
    int l = 0, r = 0, u = 0, d = 0;
};

constexpr Arms box(int l, int r, int u, int d) { return {l, r, u, d}; }

// U+2500..U+257F as [left, right, up, down]. Dashes, arcs and diagonals are handled apart.
Arms armsOf(char32_t ch)
{
    switch (ch) {
    case 0x2500: return box(1, 1, 0, 0);  case 0x2501: return box(2, 2, 0, 0);
    case 0x2502: return box(0, 0, 1, 1);  case 0x2503: return box(0, 0, 2, 2);
    case 0x250C: return box(0, 1, 0, 1);  case 0x250D: return box(0, 2, 0, 1);
    case 0x250E: return box(0, 1, 0, 2);  case 0x250F: return box(0, 2, 0, 2);
    case 0x2510: return box(1, 0, 0, 1);  case 0x2511: return box(2, 0, 0, 1);
    case 0x2512: return box(1, 0, 0, 2);  case 0x2513: return box(2, 0, 0, 2);
    case 0x2514: return box(0, 1, 1, 0);  case 0x2515: return box(0, 2, 1, 0);
    case 0x2516: return box(0, 1, 2, 0);  case 0x2517: return box(0, 2, 2, 0);
    case 0x2518: return box(1, 0, 1, 0);  case 0x2519: return box(2, 0, 1, 0);
    case 0x251A: return box(1, 0, 2, 0);  case 0x251B: return box(2, 0, 2, 0);
    case 0x251C: return box(0, 1, 1, 1);  case 0x251D: return box(0, 2, 1, 1);
    case 0x251E: return box(0, 1, 2, 1);  case 0x251F: return box(0, 1, 1, 2);
    case 0x2520: return box(0, 1, 2, 2);  case 0x2521: return box(0, 2, 2, 1);
    case 0x2522: return box(0, 2, 1, 2);  case 0x2523: return box(0, 2, 2, 2);
    case 0x2524: return box(1, 0, 1, 1);  case 0x2525: return box(2, 0, 1, 1);
    case 0x2526: return box(1, 0, 2, 1);  case 0x2527: return box(1, 0, 1, 2);
    case 0x2528: return box(1, 0, 2, 2);  case 0x2529: return box(2, 0, 2, 1);
    case 0x252A: return box(2, 0, 1, 2);  case 0x252B: return box(2, 0, 2, 2);
    case 0x252C: return box(1, 1, 0, 1);  case 0x252D: return box(2, 1, 0, 1);
    case 0x252E: return box(1, 2, 0, 1);  case 0x252F: return box(2, 2, 0, 1);
    case 0x2530: return box(1, 1, 0, 2);  case 0x2531: return box(2, 1, 0, 2);
    case 0x2532: return box(1, 2, 0, 2);  case 0x2533: return box(2, 2, 0, 2);
    case 0x2534: return box(1, 1, 1, 0);  case 0x2535: return box(2, 1, 1, 0);
    case 0x2536: return box(1, 2, 1, 0);  case 0x2537: return box(2, 2, 1, 0);
    case 0x2538: return box(1, 1, 2, 0);  case 0x2539: return box(2, 1, 2, 0);
    case 0x253A: return box(1, 2, 2, 0);  case 0x253B: return box(2, 2, 2, 0);
    case 0x253C: return box(1, 1, 1, 1);  case 0x253D: return box(2, 1, 1, 1);
    case 0x253E: return box(1, 2, 1, 1);  case 0x253F: return box(2, 2, 1, 1);
    case 0x2540: return box(1, 1, 2, 1);  case 0x2541: return box(1, 1, 1, 2);
    case 0x2542: return box(1, 1, 2, 2);  case 0x2543: return box(2, 1, 2, 1);
    case 0x2544: return box(1, 2, 2, 1);  case 0x2545: return box(2, 1, 1, 2);
    case 0x2546: return box(1, 2, 1, 2);  case 0x2547: return box(2, 2, 2, 1);
    case 0x2548: return box(2, 2, 1, 2);  case 0x2549: return box(2, 1, 2, 2);
    case 0x254A: return box(1, 2, 2, 2);  case 0x254B: return box(2, 2, 2, 2);
    case 0x2550: return box(3, 3, 0, 0);  case 0x2551: return box(0, 0, 3, 3);
    case 0x2552: return box(0, 3, 0, 1);  case 0x2553: return box(0, 1, 0, 3);
    case 0x2554: return box(0, 3, 0, 3);  case 0x2555: return box(3, 0, 0, 1);
    case 0x2556: return box(1, 0, 0, 3);  case 0x2557: return box(3, 0, 0, 3);
    case 0x2558: return box(0, 3, 1, 0);  case 0x2559: return box(0, 1, 3, 0);
    case 0x255A: return box(0, 3, 3, 0);  case 0x255B: return box(3, 0, 1, 0);
    case 0x255C: return box(1, 0, 3, 0);  case 0x255D: return box(3, 0, 3, 0);
    case 0x255E: return box(0, 3, 1, 1);  case 0x255F: return box(0, 1, 3, 3);
    case 0x2560: return box(0, 3, 3, 3);  case 0x2561: return box(3, 0, 1, 1);
    case 0x2562: return box(1, 0, 3, 3);  case 0x2563: return box(3, 0, 3, 3);
    case 0x2564: return box(3, 3, 0, 1);  case 0x2565: return box(1, 1, 0, 3);
    case 0x2566: return box(3, 3, 0, 3);  case 0x2567: return box(3, 3, 1, 0);
    case 0x2568: return box(1, 1, 3, 0);  case 0x2569: return box(3, 3, 3, 0);
    case 0x256A: return box(3, 3, 1, 1);  case 0x256B: return box(1, 1, 3, 3);
    case 0x256C: return box(3, 3, 3, 3);
    case 0x2574: return box(1, 0, 0, 0);  case 0x2575: return box(0, 0, 1, 0);
    case 0x2576: return box(0, 1, 0, 0);  case 0x2577: return box(0, 0, 0, 1);
    case 0x2578: return box(2, 0, 0, 0);  case 0x2579: return box(0, 0, 2, 0);
    case 0x257A: return box(0, 2, 0, 0);  case 0x257B: return box(0, 0, 0, 2);
    case 0x257C: return box(1, 2, 0, 0);  case 0x257D: return box(0, 0, 1, 2);
    case 0x257E: return box(2, 1, 0, 0);  case 0x257F: return box(0, 0, 2, 1);
    default: return box(-1, 0, 0, 0);
    }
}

struct Ctx
{
    QImage &img;
    QPainter p;
    int w, h, t, heavy;
    explicit Ctx(QImage &i, int thickness)
        : img(i), p(&i), w(i.width()), h(i.height()), t(std::max(1, thickness)), heavy(std::max(2, 2 * std::max(1, thickness)))
    {
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.setRenderHint(QPainter::Antialiasing, false);
    }
    void rect(int x0, int y0, int x1, int y1)   // [x0, x1) x [y0, y1), clipped
    {
        x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(w, x1); y1 = std::min(h, y1);
        if (x1 > x0 && y1 > y0) {
            p.fillRect(QRect(x0, y0, x1 - x0, y1 - y0), Qt::white);
        }
    }
    int thick(int weight) const { return weight == 1 ? t : weight == 2 ? heavy : weight == 3 ? 3 * t : 0; }
};

// Lines of light, heavy and double weight, by the rules of the font designers (see the notes in NOTES.md).
void drawArms(Ctx &c, const Arms &a)
{
    const int V = std::max(c.thick(a.u), c.thick(a.d));   // the vertical group's width
    const int H = std::max(c.thick(a.l), c.thick(a.r));   // the horizontal group's height
    const int vx0 = (c.w - V) / 2, vx1 = vx0 + V;
    const int hy0 = (c.h - H) / 2, hy1 = hy0 + H;
    const bool anyV = V > 0, anyH = H > 0;

    // A horizontal arm (left: dir = -1, right: +1) of `weight`; `same` is the perpendicular arm on each side.
    const auto horizontal = [&](int weight, int dir, int upper, int lower) {
        if (weight == 0) {
            return;
        }
        const int outer = dir < 0 ? 0 : c.w;
        // Where a strip stops at the junction.
        const auto stop = [&](int sameSide, int oppSide) {
            if (sameSide > 0) {
                const int gt = c.thick(sameSide);
                if (sameSide == 3 && weight == 3) {
                    return dir < 0 ? vx0 + c.t : vx1 - c.t;      // it meets the double stroke's inner strip
                }
                return dir < 0 ? (c.w + gt) / 2 : (c.w - gt) / 2;  // it runs to the middle of the stroke
            }
            Q_UNUSED(oppSide)
            return dir < 0 ? vx1 : vx0;                            // outer corner: across the whole group
        };
        if (weight == 3) {
            const int y0 = (c.h - 3 * c.t) / 2;
            const int yU = y0, yL = y0 + 2 * c.t;
            const int eU = anyV ? stop(upper, lower) : (dir < 0 ? vx1 : vx0);
            const int eL = anyV ? stop(lower, upper) : (dir < 0 ? vx1 : vx0);
            dir < 0 ? c.rect(outer, yU, eU, yU + c.t) : c.rect(eU, yU, outer, yU + c.t);
            dir < 0 ? c.rect(outer, yL, eL, yL + c.t) : c.rect(eL, yL, outer, yL + c.t);
        } else {
            const int th = c.thick(weight);
            const int y0 = (c.h - th) / 2;
            // Meeting a double stroke the arm stops at its edge, else it runs through the group.
            const bool vDouble = upper == 3 || lower == 3;
            const int e = vDouble ? (dir < 0 ? vx0 : vx1) : (dir < 0 ? vx1 : vx0);
            dir < 0 ? c.rect(outer, y0, e, y0 + th) : c.rect(e, y0, outer, y0 + th);
        }
    };
    const auto vertical = [&](int weight, int dir, int left, int right) {
        if (weight == 0) {
            return;
        }
        const int outer = dir < 0 ? 0 : c.h;
        const auto stop = [&](int sameSide) {
            if (sameSide > 0) {
                const int gt = c.thick(sameSide);
                if (sameSide == 3 && weight == 3) {
                    return dir < 0 ? hy0 + c.t : hy1 - c.t;
                }
                return dir < 0 ? (c.h + gt) / 2 : (c.h - gt) / 2;
            }
            return dir < 0 ? hy1 : hy0;
        };
        if (weight == 3) {
            const int x0 = (c.w - 3 * c.t) / 2;
            const int xL = x0, xR = x0 + 2 * c.t;
            const int eL = anyH ? stop(left) : (dir < 0 ? hy1 : hy0);
            const int eR = anyH ? stop(right) : (dir < 0 ? hy1 : hy0);
            dir < 0 ? c.rect(xL, outer, xL + c.t, eL) : c.rect(xL, eL, xL + c.t, outer);
            dir < 0 ? c.rect(xR, outer, xR + c.t, eR) : c.rect(xR, eR, xR + c.t, outer);
        } else {
            const int th = c.thick(weight);
            const int x0 = (c.w - th) / 2;
            const bool hDouble = left == 3 || right == 3;
            const int e = hDouble ? (dir < 0 ? hy0 : hy1) : (dir < 0 ? hy1 : hy0);
            dir < 0 ? c.rect(x0, outer, x0 + th, e) : c.rect(x0, e, x0 + th, outer);
        }
    };
    // The perpendicular weights per side: for the horizontal arms the vertical ones are `u` (upper) and `d` (lower).
    horizontal(a.l, -1, a.u, a.d);
    horizontal(a.r, +1, a.u, a.d);
    vertical(a.u, -1, a.l, a.r);
    vertical(a.d, +1, a.l, a.r);
    Q_UNUSED(anyV)
    Q_UNUSED(anyH)
}

// n dashes along a line of `len` pixels, `gap` pixels apart.
void dashes(Ctx &c, bool horizontalLine, int n, int weight)
{
    const int th = c.thick(weight);
    const int len = horizontalLine ? c.w : c.h;
    const int gap = std::max(1, len / (n * 4));
    for (int i = 0; i < n; ++i) {
        const int a = (len * i) / n + (i == 0 ? 0 : gap / 2);
        const int b = (len * (i + 1)) / n - (i == n - 1 ? 0 : (gap + 1) / 2);
        if (horizontalLine) {
            const int y0 = (c.h - th) / 2;
            c.rect(a, y0, b, y0 + th);
        } else {
            const int x0 = (c.w - th) / 2;
            c.rect(x0, a, x0 + th, b);
        }
    }
}

void arc(Ctx &c, char32_t ch)
{
    // Rounded corners: a quarter circle of the line's width between the two edge midpoints.
    const qreal cx = c.w / 2.0, cy = c.h / 2.0;
    const qreal th = c.t;
    QPainterPath path;
    const qreal r = std::min(cx, cy);
    switch (ch) {
    case 0x256D: path.moveTo(c.w, cy); path.lineTo(cx + r, cy); path.arcTo(cx, cy, 2 * r, 2 * r, 90, 90); path.lineTo(cx, c.h); break;   // down and right
    case 0x256E: path.moveTo(0, cy); path.lineTo(cx - r, cy); path.arcTo(cx - 2 * r, cy, 2 * r, 2 * r, 90, -90); path.lineTo(cx, c.h); break; // down and left
    case 0x256F: path.moveTo(0, cy); path.lineTo(cx - r, cy); path.arcTo(cx - 2 * r, cy - 2 * r, 2 * r, 2 * r, 270, 90); path.lineTo(cx, 0); break; // up and left
    default: path.moveTo(c.w, cy); path.lineTo(cx + r, cy); path.arcTo(cx, cy - 2 * r, 2 * r, 2 * r, 270, -90); path.lineTo(cx, 0); break;   // up and right
    }
    c.p.setBrush(Qt::NoBrush);
    c.p.setPen(QPen(Qt::white, th, Qt::SolidLine, Qt::FlatCap));
    c.p.setRenderHint(QPainter::Antialiasing, true);
    c.p.drawPath(path);
}

void diagonal(Ctx &c, char32_t ch)
{
    c.p.setBrush(Qt::NoBrush);
    c.p.setPen(QPen(Qt::white, c.t, Qt::SolidLine, Qt::FlatCap));
    c.p.setRenderHint(QPainter::Antialiasing, true);
    if (ch == 0x2571 || ch == 0x2573) {
        c.p.drawLine(QPointF(c.w, 0), QPointF(0, c.h));
    }
    if (ch == 0x2572 || ch == 0x2573) {
        c.p.drawLine(QPointF(0, 0), QPointF(c.w, c.h));
    }
}

void block(Ctx &c, char32_t ch)
{
    const int w = c.w, h = c.h;
    const auto fx = [&](int n, int d) { return (w * n + d / 2) / d; };   // n/d of the width, rounded
    const auto fy = [&](int n, int d) { return (h * n + d / 2) / d; };
    switch (ch) {
    case 0x2580: c.rect(0, 0, w, fy(1, 2)); break;
    case 0x2581: case 0x2582: case 0x2583: case 0x2584: case 0x2585: case 0x2586: case 0x2587: {
        const int n = int(ch - 0x2580);                      // eighths of the height, from the bottom
        c.rect(0, fy(8 - n, 8), w, h);   // the top edge rounds like the upper half's bottom, so the two tile
        break;
    }
    case 0x2588: c.rect(0, 0, w, h); break;
    case 0x2589: case 0x258A: case 0x258B: case 0x258C: case 0x258D: case 0x258E: case 0x258F: {
        const int n = 8 - int(ch - 0x2588);                  // eighths of the width, from the left
        c.rect(0, 0, fx(n, 8), h);
        break;
    }
    case 0x2590: c.rect(fx(1, 2), 0, w, h); break;
    case 0x2591: case 0x2592: case 0x2593: {
        const int a = ch == 0x2591 ? 64 : ch == 0x2592 ? 128 : 192;
        c.p.fillRect(QRect(0, 0, w, h), QColor(255, 255, 255, a));
        break;
    }
    case 0x2594: c.rect(0, 0, w, fy(1, 8)); break;
    case 0x2595: c.rect(w - fx(1, 8), 0, w, h); break;
    default: {   // 2596..259F: quadrants (UL, UR, LL, LR)
        static const int quad[10] = {0b0100, 0b1000, 0b0010, 0b1101, 0b1001, 0b0111, 0b1110, 0b0001, 0b0110, 0b1011};
        const int m = quad[ch - 0x2596];
        const int mx = fx(1, 2), my = fy(1, 2);
        if (m & 0b0001) c.rect(0, 0, mx, my);        // upper left
        if (m & 0b0010) c.rect(mx, 0, w, my);        // upper right
        if (m & 0b0100) c.rect(0, my, mx, h);        // lower left
        if (m & 0b1000) c.rect(mx, my, w, h);        // lower right
        break;
    }
    }
}
} // namespace

namespace BuiltinGlyphs
{
bool handles(char32_t ch)
{
    return (ch >= 0x2500 && ch <= 0x259F);
}

bool draw(QImage &cell, char32_t ch, int thickness)
{
    if (!handles(ch)) {
        return false;
    }
    Ctx c(cell, thickness);
    if (ch >= 0x2580) {
        block(c, ch);
        return true;
    }
    switch (ch) {
    case 0x2504: dashes(c, true, 3, 1); return true;   case 0x2505: dashes(c, true, 3, 2); return true;
    case 0x2506: dashes(c, false, 3, 1); return true;  case 0x2507: dashes(c, false, 3, 2); return true;
    case 0x2508: dashes(c, true, 4, 1); return true;   case 0x2509: dashes(c, true, 4, 2); return true;
    case 0x250A: dashes(c, false, 4, 1); return true;  case 0x250B: dashes(c, false, 4, 2); return true;
    case 0x254C: dashes(c, true, 2, 1); return true;   case 0x254D: dashes(c, true, 2, 2); return true;
    case 0x254E: dashes(c, false, 2, 1); return true;  case 0x254F: dashes(c, false, 2, 2); return true;
    case 0x256D: case 0x256E: case 0x256F: case 0x2570: arc(c, ch); return true;
    case 0x2571: case 0x2572: case 0x2573: diagonal(c, ch); return true;
    default: break;
    }
    const Arms a = armsOf(ch);
    if (a.l < 0) {
        return false;
    }
    drawArms(c, a);
    return true;
}
}
