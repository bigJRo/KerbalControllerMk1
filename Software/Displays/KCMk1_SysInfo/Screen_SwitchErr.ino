/***************************************************************************************
   Screen_SwitchErr.ino -- SWITCH ERR (design §5)

   Takes the whole screen while the master's last SWITCH_ERR frame had n > 0. Under the
   red title, VESSEL IS:, then one row per switch: its name (white) left, the vessel's
   state (red) right. Five rows a page; more page every SWERR_PAGE_MS with PAGE i/n.
   An id the table does not know shows as SW nn, POS 0 / POS 1.
****************************************************************************************/
static const uint8_t SWERR_ROWS = 5;

void drawSwitchErr(uint32_t now) {
  titleBar("SWITCH ERR", nullptr, C_RED);
  gfxText(SI_MARGIN, 42, F32, "VESSEL IS:", C_GREY, AL_LEFT);
  uint8_t pages = (uint8_t)((g_m.swN + SWERR_ROWS - 1) / SWERR_ROWS);
  uint8_t page = (uint8_t)((now / SWERR_PAGE_MS) % pages);
  for (uint8_t r = 0; r < SWERR_ROWS; r++) {
    uint8_t i = page * SWERR_ROWS + r;
    if (i >= g_m.swN) break;
    const SwErr &e = g_m.sw[i];
    char nm[12], st[12];
    if (e.id > 0 && e.id < SW_NAMED) {
      strcpy(nm, SW_NAME[e.id].name);
      strcpy(st, e.actual ? SW_NAME[e.id].pos1 : SW_NAME[e.id].pos0);
    } else {
      snprintf(nm, sizeof nm, "SW %u", e.id);
      snprintf(st, sizeof st, "POS %u", e.actual ? 1 : 0);
    }
    int16_t y = 82 + r * TEXT_PITCH;
    gfxText(SI_MARGIN, y, F32, nm, C_WHITE, AL_LEFT);
    gfxText(SI_W - SI_MARGIN, y, F32, st, C_RED, AL_RIGHT);
  }
  if (pages > 1) {
    char t[16];
    snprintf(t, sizeof t, "PAGE %u/%u", page + 1, pages);
    gfxText(SI_W / 2, 282, F32, t, C_GREY, AL_CENTRE);
  }
}
