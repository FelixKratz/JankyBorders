#include "parse.h"
#include "border.h"
#include "hashtable.h"

static bool str_starts_with(char* string, char* prefix) {
  if (!string || !prefix) return false;
  if (strlen(string) < strlen(prefix)) return false;
  if (strncmp(prefix, string, strlen(prefix)) == 0) return true;
  return false;
}

static bool parse_list(struct table* list, char* token) {
  uint32_t token_len = strlen(token) + 1;
  char copy[token_len];
  memcpy(copy, token, token_len);

  char* name;
  char* cursor = copy;
  bool entry_found = false;

  table_clear(list);
  while((name = strsep(&cursor, ","))) {
    if (strlen(name) > 0) {
      _table_add(list, name, strlen(name) + 1, (void*)true);
      entry_found = true;
    }
  }
  return entry_found;
}

// Parses a comma separated list of colors, terminated by `terminator`, into one
// color per side: a single color applies to the whole border, two colors set the
// top and the bottom, four colors set every side individually. Sides that the
// list leaves out stay transparent and are not drawn.
static bool parse_color_list(uint32_t* colors, char* token, char terminator) {
  uint32_t parsed[BORDER_SIDE_COUNT];
  uint32_t count = 0;
  int end = 0;

  while (count < BORDER_SIDE_COUNT) {
    if (sscanf(token, " 0x%x%n", &parsed[count], &end) != 1) return false;
    count++;
    token += end + strspn(token + end, " \t");
    if (*token != ',') break;
    token++;
  }

  // Anything left over is either garbage or a fifth color
  if (token[strspn(token, " \t")] != terminator) return false;

  if (count == 1) {
    for (uint32_t i = 0; i < BORDER_SIDE_COUNT; i++) colors[i] = parsed[0];
  } else if (count == 2) {
    colors[BORDER_SIDE_TOP] = parsed[0];
    colors[BORDER_SIDE_BOTTOM] = parsed[1];
    colors[BORDER_SIDE_RIGHT] = 0;
    colors[BORDER_SIDE_LEFT] = 0;
  } else if (count == BORDER_SIDE_COUNT) {
    for (uint32_t i = 0; i < BORDER_SIDE_COUNT; i++) colors[i] = parsed[i];
  } else {
    return false;
  }
  return true;
}

static bool parse_color(struct color_style* style, char* token) {
  static char glow[] = "=glow(";

  if (token[0] == '=' && parse_color_list(style->colors, token + 1, '\0')) {
    style->stype = COLOR_STYLE_SOLID;
    return true;
  }
  else if (str_starts_with(token, glow)
           && parse_color_list(style->colors, token + strlen(glow), ')')) {
    style->stype = COLOR_STYLE_GLOW;
    return true;
  }
  else if (sscanf(token,
             "=gradient(top_left=0x%x,bottom_right=0x%x)",
             &style->gradient.color1,
             &style->gradient.color2) == 2) {
    style->stype = COLOR_STYLE_GRADIENT;
    style->gradient.direction = TL_TO_BR;
    return true;
  }
  else if (sscanf(token,
             "=gradient(top_right=0x%x,bottom_left=0x%x)",
             &style->gradient.color1,
             &style->gradient.color2) == 2) {
    style->stype = COLOR_STYLE_GRADIENT;
    style->gradient.direction = TR_TO_BL;
    return true;
  }
  else printf("[?] Borders: Invalid color argument color%s\n", token);

  return false;
}

uint32_t parse_settings(struct settings* settings, int count, char** arguments) {
  static char active_color[] = "active_color";
  static char inactive_color[] = "inactive_color";
  static char background_color[] = "background_color";
  static char blacklist[] = "blacklist=";
  static char whitelist[] = "whitelist=";

  char order = 'a';
  uint32_t update_mask = 0;
  for (int i = 0; i < count; i++) {
    if (str_starts_with(arguments[i], active_color)) {
      if (parse_color(&settings->active_window,
                                 arguments[i] + strlen(active_color))) {
        update_mask |= BORDER_UPDATE_MASK_ACTIVE;
      }
    }
    else  if (str_starts_with(arguments[i], inactive_color)) {
      if (parse_color(&settings->inactive_window,
                                 arguments[i] + strlen(inactive_color))) {
        update_mask |= BORDER_UPDATE_MASK_INACTIVE;
      }
    }
    else if (str_starts_with(arguments[i], background_color)) {
      if (parse_color(&settings->background,
                                 arguments[i] + strlen(background_color))) {
        update_mask |= BORDER_UPDATE_MASK_ALL;
        settings->show_background = settings->background.colors[0] & 0xff000000;
      }
    }
    else if (str_starts_with(arguments[i], blacklist)) {
      settings->blacklist_enabled = parse_list(&settings->blacklist,
                                               arguments[i]
                                               + strlen(blacklist));
      update_mask |= BORDER_UPDATE_MASK_RECREATE_ALL;
    }
    else if (str_starts_with(arguments[i], whitelist)) {
      settings->whitelist_enabled = parse_list(&settings->whitelist,
                                               arguments[i]
                                               + strlen(whitelist));
      update_mask |= BORDER_UPDATE_MASK_RECREATE_ALL;
    }
    else if (sscanf(arguments[i], "width=%f", &settings->border_width) == 1) {
      update_mask |= BORDER_UPDATE_MASK_ALL;
    }
    else if (sscanf(arguments[i], "order=%c", &order) == 1) {
      if (order == 'a') settings->border_order = BORDER_ORDER_ABOVE;
      else settings->border_order = BORDER_ORDER_BELOW;
      update_mask |= BORDER_UPDATE_MASK_ALL;
    }
    else if (sscanf(arguments[i], "style=%c", &settings->border_style) == 1) {
      update_mask |= BORDER_UPDATE_MASK_ALL;
    }
    else if (strcmp(arguments[i], "hidpi=on") == 0) {
      update_mask |= BORDER_UPDATE_MASK_RECREATE_ALL;
      settings->hidpi = true;
    }
    else if (strcmp(arguments[i], "hidpi=off") == 0) {
      update_mask |= BORDER_UPDATE_MASK_RECREATE_ALL;
      settings->hidpi = false;
    }
    else if (strcmp(arguments[i], "ax_focus=on") == 0) {
      settings->ax_focus = true;
      update_mask |= BORDER_UPDATE_MASK_SETTING;
    }
    else if (strcmp(arguments[i], "ax_focus=off") == 0) {
      settings->ax_focus = false;
      update_mask |= BORDER_UPDATE_MASK_SETTING;
    }
    else if (sscanf(arguments[i], "apply-to=%d", &settings->apply_to) == 1) {
      update_mask |= BORDER_UPDATE_MASK_SETTING;
    }
    else {
      printf("[?] Borders: Invalid argument '%s'\n", arguments[i]);
    }
  }
  return update_mask;
}
