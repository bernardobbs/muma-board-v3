#pragma once
#include "display/lvgl_display/lvgl_image.h"
#include <memory>

// Imagem estatica do tomate (fundo do relogio do pomodoro), gerada por
// scripts/gen_pomodoro_tomato.py a partir de pomodoro_tomato.png.
std::shared_ptr<LvglImage> GetPomodoroTomatoImage();
