# libvterm 0.3.3 as used by win11terminal

Upstream: https://www.leonerd.org.uk/code/libvterm/ (`libvterm-0.3.3.tar.gz`, sha256
`src/unicode.c`'s width table is by Markus Kuhn (permissive, in that file's header).
Changes (search for "win11terminal patch"):

1. **Faint** (SGR 2, cleared by 22) and **overline** (SGR 53/55): new `VTERM_ATTR_FAINT` and
   `VTERM_ATTR_OVERLINE`, the `faint` and `overline` bits of `VTermScreenCellAttrs`, the pen in
   `pen.c`/`screen.c`, the attribute masks.
2. **Ring buffer for the primary screen**: `getcell()` reads row `(r + shift) % rows`; a scroll of the
   whole screen upward only advances `shift` (`moverect_internal`) instead of `memmove`-ing every cell;
   the buffer is put back in order (`normalize_primary`) before a resize.
3. **`sb_pushline` gets only the used cells**: the `cols` argument is the number of cells up to the last
   one with content or a background (upstream: always the screen width).
4. **Continuation flag of scrolled-off rows**: `state.c` remembers, before `scroll()` shifts `lineinfo`, whether the rows
   leaving the top continued the row above (`pushed_cont`); `vterm_screen_pushed_continuation()` gives it inside
   `sb_pushline`, so the scrollback keeps wrapped lines joined.
5. **Row pointer once per row** in `erase_internal` and `sb_pushline_from_row`: a row's cells are
   contiguous (also in the ring), so the row is found once instead of calling `getcell()` per cell (the two
   loops were 70% of the time of a stream of short lines).
6. **`vterm_state_get_bracketpaste()`**: whether the program turned bracketed paste on, for the paste
   warning (Windows Terminal warns about a multi-line paste only when the program cannot tell it from typing).
