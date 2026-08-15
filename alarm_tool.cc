#include "alarm_tool.h"
#include "settings.h"

#include <cJSON.h>
#include <esp_log.h>
#include <time.h>

#define TAG "Alarm"
static const char* NS = "alarm";

void AlarmEngine::Load() {
    Settings s(NS, false);
    hour_ = s.GetInt("hour", 7);
    minute_ = s.GetInt("minute", 0);
    enabled_ = s.GetBool("enabled", false);
}

void AlarmEngine::Set(int hour, int minute, bool enabled) {
    hour_ = hour < 0 ? 0 : (hour > 23 ? 23 : hour);
    minute_ = minute < 0 ? 0 : (minute > 59 ? 59 : minute);
    enabled_ = enabled;
    Settings s(NS, true);
    s.SetInt("hour", hour_);
    s.SetInt("minute", minute_);
    s.SetBool("enabled", enabled_);
    ESP_LOGI(TAG, "Configurado: %02d:%02d (%s)", hour_, minute_, enabled_ ? "ativado" : "desativado");
}

void AlarmEngine::CheckTrampoline(void* arg) {
    static_cast<AlarmEngine*>(arg)->Check();
}

void AlarmEngine::Initialize() {
    Load();
    if (timer_ != nullptr) return;
    esp_timer_create_args_t args = {};
    args.callback = &AlarmEngine::CheckTrampoline;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "alarm_check";
    if (esp_timer_create(&args, &timer_) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar o timer");
        return;
    }
    esp_timer_start_periodic(timer_, (int64_t)kCheckIntervalSec * 1000000);
}

void AlarmEngine::Check() {
    int64_t now_us = esp_timer_get_time();

    if (firing_) {
        if (now_us - firing_started_us_ >= (int64_t)kMaxRingSeconds * 1000000) {
            ESP_LOGI(TAG, "Ninguem desligou -- parando sozinho depois de %ds", kMaxRingSeconds);
            Dismiss();
            return;
        }
        if (now_us - last_ring_us_ >= (int64_t)kRingIntervalSec * 1000000) {
            last_ring_us_ = now_us;
            if (on_ring_tick_) on_ring_tick_();
        }
        return;
    }

    if (!enabled_) return;

    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    if (ti.tm_hour != hour_ || ti.tm_min != minute_) return;
    if (last_fired_yday_ == ti.tm_yday) return;  // ja disparou hoje

    last_fired_yday_ = ti.tm_yday;
    firing_ = true;
    firing_started_us_ = now_us;
    last_ring_us_ = now_us;
    ESP_LOGI(TAG, "Disparado (%02d:%02d)", hour_, minute_);
    if (on_fired_) on_fired_();
}

void AlarmEngine::Dismiss() {
    if (!firing_) return;
    firing_ = false;
    ESP_LOGI(TAG, "Desligado");
    if (on_dismissed_) on_dismissed_();
}

std::string AlarmEngine::ToJson() const {
    cJSON* r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "hour", hour_);
    cJSON_AddNumberToObject(r, "minute", minute_);
    cJSON_AddBoolToObject(r, "enabled", enabled_);
    char* s = cJSON_PrintUnformatted(r);
    std::string out(s);
    cJSON_free(s);
    cJSON_Delete(r);
    return out;
}
