#include "animation.h"

void animation_init(struct animation* animation) {
  memset(animation, 0, sizeof(struct animation));
}


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
void animation_start(struct animation* animation, void* proc, void* context) {
  assert(animation->link == NULL);
  assert(animation->context == NULL);
  CVDisplayLinkCreateWithActiveCGDisplays(&animation->link);
  CVTime refresh_period
            = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(animation->link);
  animation->frame_time = 1e6 * (double)refresh_period.timeValue
                        / (double)refresh_period.timeScale;

  animation->context = context;
  CVDisplayLinkSetOutputCallback(animation->link, proc, animation);
  CVDisplayLinkStart(animation->link);
}

void animation_stop(struct animation* animation) {
  if (animation->link) {
    CVDisplayLinkStop(animation->link);
    CVDisplayLinkRelease(animation->link);
    animation->link = NULL;
  }
  if (animation->context) free(animation->context);
  animation->context = NULL;
}
#pragma clang diagnostic pop
