#include "breathing_tool.h"
#include <esp_log.h>

#define TAG "Breathing"

void BreathingExercise::TickTrampoline(void* arg) {
    static_cast<BreathingExercise*>(arg)->Tick();
}

void BreathingExercise::Initialize() {
    if (timer_ != nullptr) return;
    esp_timer_create_args_t args = {};
    args.callback = &BreathingExercise::TickTrampoline;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "breathing_tick";
    if (esp_timer_create(&args, &timer_) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar o timer");
    }
}

void BreathingExercise::Start() {
    if (active_ || timer_ == nullptr) return;
    active_ = true;
    phase_ = BreathPhase::INSPIRE;
    phase_start_us_ = esp_timer_get_time();
    esp_timer_start_periodic(timer_, (int64_t)kTickIntervalMs * 1000);
    ESP_LOGI(TAG, "Iniciado");
    Tick();  // primeiro frame na hora, sem esperar o 1o tick do timer
}

void BreathingExercise::Stop() {
    if (!active_) return;
    active_ = false;
    esp_timer_stop(timer_);
    ESP_LOGI(TAG, "Parado");
    if (on_stopped_) on_stopped_();
}

void BreathingExercise::Tick() {
    if (!active_) return;
    int64_t elapsed_us = esp_timer_get_time() - phase_start_us_;
    float progress = (float)elapsed_us / ((float)kPhaseSeconds * 1000000.0f);
    if (progress >= 1.0f) {
        switch (phase_) {
            case BreathPhase::INSPIRE: phase_ = BreathPhase::SEGURA; break;
            case BreathPhase::SEGURA:  phase_ = BreathPhase::SOLTA;  break;
            case BreathPhase::SOLTA:   phase_ = BreathPhase::INSPIRE; break;
        }
        phase_start_us_ = esp_timer_get_time();
        progress = 0.0f;
    }
    if (on_tick_) on_tick_(phase_, progress);
}
