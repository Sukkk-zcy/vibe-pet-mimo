#ifndef CODE_PET_PERSONA_ASSETS_H
#define CODE_PET_PERSONA_ASSETS_H

#include <Arduino.h>
#include <lvgl.h>

const lv_img_dsc_t *codePetPersonaFrame(const String &slug, const String &state, uint8_t frameIndex);
bool codePetPersonaAvailable(const String &slug);

#endif
