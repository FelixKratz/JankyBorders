#pragma once

#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>

struct app_rule_pattern {
  regex_t regex;
};

struct app_rule {
  bool match_all_windows;
  size_t pattern_count;
  struct app_rule_pattern* patterns;
};

static inline void app_rule_free(struct app_rule* rule) {
  if (!rule) return;
  for (size_t i = 0; i < rule->pattern_count; ++i) {
    regfree(&rule->patterns[i].regex);
  }
  free(rule->patterns);
  free(rule);
}

static inline bool app_rule_matches(const struct app_rule* rule,
                                    const char* title) {
  if (!rule) return false;
  if (rule->match_all_windows) return true;
  if (!title) title = "";
  for (size_t i = 0; i < rule->pattern_count; ++i) {
    if (regexec(&rule->patterns[i].regex, title, 0, NULL, 0) == 0) {
      return true;
    }
  }
  return false;
}
