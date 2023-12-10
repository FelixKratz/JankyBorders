#pragma once
#include "border.h"

#define BORDER_UPDATE_MASK_NONE      0
#define BORDER_UPDATE_MASK_ACTIVE   (1 << 0)
#define BORDER_UPDATE_MASK_INACTIVE (1 << 1)
#define BORDER_UPDATE_MASK_ALL      (BORDER_UPDATE_MASK_ACTIVE \
                                     | BORDER_UPDATE_MASK_INACTIVE)

#define BORDER_UPDATE_MASK_RECREATE_ALL (1 << 2)


uint32_t parse_settings(struct settings* settings, int count, char** arguments);
