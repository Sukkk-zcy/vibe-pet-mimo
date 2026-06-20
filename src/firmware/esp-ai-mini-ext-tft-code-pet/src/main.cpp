// Shared display firmware implementation; board-specific flags live in platformio.ini.
#include "backlight_control.h"

#if defined(CODE_PET_HAS_BLE)
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#endif

#if defined(CODE_PET_USE_LVGL)
#include <lvgl.h>
#include "persona_assets.h"
#endif

#include "../../esp-display-code-pet/src/main.cpp"
