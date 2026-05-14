#include "parse.h"
#include "border.h"
#include "hashtable.h"
#include "misc/app_rules.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool str_starts_with(char* string, char* prefix) {
  if (!string || !prefix) return false;
  if (strlen(string) < strlen(prefix)) return false;
  if (strncmp(prefix, string, strlen(prefix)) == 0) return true;
  return false;
}

static char* trim_whitespace(char* text) {
  if (!text) return text;
  while (*text && isspace((unsigned char)*text)) text++;
  char* end = text + strlen(text);
  while (end > text && isspace((unsigned char)*(end - 1))) {
    --end;
    *end = '\0';
  }
  return text;
}

static void rules_reset(struct table* list) {
  for (int i = 0; i < list->capacity; ++i) {
    struct bucket* bucket = list->buckets[i];
    while (bucket) {
      if (bucket->value) {
        app_rule_free((struct app_rule*)bucket->value);
        bucket->value = NULL;
      }
      bucket = bucket->next;
    }
  }
  table_clear(list);
}

static void prune_empty_rule(struct table* list,
                             const char* app,
                             struct app_rule* rule) {
  if (!rule) return;
  if (rule->match_all_windows) return;
  if (rule->pattern_count > 0) return;

  app_rule_free(rule);
  table_remove(list, (void*)app);
}

static bool parse_app_rules(struct table* list,
                            char* token,
                            const char* list_name) {
  uint32_t token_len = strlen(token) + 1;
  char copy[token_len];
  memcpy(copy, token, token_len);

  char* entry = NULL;
  char* cursor = copy;
  bool entry_found = false;

  rules_reset(list);

  while ((entry = strsep(&cursor, ","))) {
    char* trimmed = trim_whitespace(entry);
    if (!trimmed || strlen(trimmed) == 0) continue;

    char* app = trimmed;
    char* regex = strchr(trimmed, ':');
    if (regex) {
      *regex = '\0';
      regex = trim_whitespace(regex + 1);
    }
    app = trim_whitespace(app);
    if (!app || strlen(app) == 0) continue;

    struct app_rule* rule = table_find(list, app);
    if (!rule) {
      rule = calloc(1, sizeof(*rule));
      if (!rule) {
        printf("[!] Borders: Out of memory while parsing %s entry '%s'\n",
               list_name,
               app);
        continue;
      }
      _table_add(list, app, strlen(app) + 1, rule);
    }

    if (!regex || strlen(regex) == 0) {
      rule->match_all_windows = true;
      entry_found = true;
      continue;
    }

    regex_t compiled;
    int error = regcomp(&compiled, regex, REG_EXTENDED | REG_NOSUB);
    if (error) {
      char error_buffer[128];
      regerror(error, &compiled, error_buffer, sizeof(error_buffer));
      printf("[?] Borders: Invalid %s regex '%s' for '%s': %s\n",
             list_name,
             regex,
             app,
             error_buffer);
      prune_empty_rule(list, app, rule);
      continue;
    }

    struct app_rule_pattern* patterns = realloc(rule->patterns,
                                                (rule->pattern_count + 1)
                                                * sizeof(struct app_rule_pattern));
    if (!patterns) {
      printf("[!] Borders: Failed to allocate memory for %s regex '%s'\n",
             list_name,
             regex);
      regfree(&compiled);
      prune_empty_rule(list, app, rule);
      continue;
    }

    rule->patterns = patterns;
    rule->patterns[rule->pattern_count].regex = compiled;
    rule->pattern_count++;
    entry_found = true;
  }

  return entry_found;
}

static bool parse_color(struct color_style* style, char* token) {
  if (sscanf(token, "=0x%x", &style->color) == 1) {
    style->stype = COLOR_STYLE_SOLID;
    return true;
  }
  else if (sscanf(token, "=glow(0x%x)", &style->color) == 1) {
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
        settings->show_background = settings->background.color & 0xff000000;
      }
    }
    else if (str_starts_with(arguments[i], blacklist)) {
      settings->blacklist_enabled = parse_app_rules(&settings->blacklist,
                                                    arguments[i]
                                                    + strlen(blacklist),
                                                    "blacklist");
      update_mask |= BORDER_UPDATE_MASK_RECREATE_ALL;
    }
    else if (str_starts_with(arguments[i], whitelist)) {
      settings->whitelist_enabled = parse_app_rules(&settings->whitelist,
                                                    arguments[i]
                                                    + strlen(whitelist),
                                                    "whitelist");
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
