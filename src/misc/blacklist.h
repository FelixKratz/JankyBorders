#pragma once

#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>

struct blacklist_pattern {
  regex_t regex;
  char* pattern;
};

struct blacklist_rule {
  bool match_all_windows;
  size_t pattern_count;
  struct blacklist_pattern* patterns;
};

static inline void blacklist_rule_free(struct blacklist_rule* rule) {
  if (!rule) return;
  for (size_t i = 0; i < rule->pattern_count; ++i) {
    regfree(&rule->patterns[i].regex);
    free(rule->patterns[i].pattern);
  }
  free(rule->patterns);
  free(rule);
}
