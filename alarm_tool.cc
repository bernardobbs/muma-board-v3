#include "alarm_tool.h"
#include "settings.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_random.h>
#include <time.h>

#define TAG "Alarm"
static const char* NS = "alarm";

std::string AlarmEngine::GenId() {
    static const char kHex[] = "0123456789abcdef";
    uint8_t raw[4];
    esp_fill_random(raw, sizeof(raw));
    char buf[9];
    for (int i = 0; i < 4; i++) {
        buf[i * 2] = kHex[raw[i] >> 4];
        buf[i * 2 + 1] = kHex[raw[i] & 0x0F];
    }
    buf[8] = '\0';
    return std::string(buf);
}

void AlarmEngine::Load() {
    Settings s(NS, false);
    std::string saved = s.GetString("list", "");
    alarms_.clear();
    if (saved.empty()) return;
    cJSON* root = cJSON_Parse(saved.c_str());
    if (root == nullptr) return;
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, root) {
        cJSON* id = cJSON_GetObjectItem(item, "id");
        cJSON* hour = cJSON_GetObjectItem(item, "hour");
        cJSON* minute = cJSON_GetObjectItem(item, "minute");
        cJSON* enabled = cJSON_GetObjectItem(item, "enabled");
        if (!cJSON_IsString(id) || !cJSON_IsNumber(hour) || !cJSON_IsNumber(minute)) continue;
        AlarmEntry e;
        e.id = id->valuestring;
        e.hour = hour->valueint;
        e.minute = minute->valueint;
        e.enabled = cJSON_IsBool(enabled) ? cJSON_IsTrue(enabled) : true;
        alarms_.push_back(e);
    }
    cJSON_Delete(root);
}

void AlarmEngine::Save() const {
    cJSON* root = cJSON_CreateArray();
    for (const auto& a : alarms_) {
        cJSON* o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "id", a.id.c_str());
        cJSON_AddNumberToObject(o, "hour", a.hour);
        cJSON_AddNumberToObject(o, "minute", a.minute);
        cJSON_AddBoolToObject(o, "enabled", a.enabled);
        cJSON_AddItemToArray(root, o);
    }
    char* str = cJSON_PrintUnformatted(root);
    Settings s(NS, true);
    s.SetString("list", str);
    cJSON_free(str);
    cJSON_Delete(root);
}

std::string AlarmEngine::AddAlarm(int hour, int minute) {
    AlarmEntry e;
    e.id = GenId();
    e.hour = hour < 0 ? 0 : (hour > 23 ? 23 : hour);
    e.minute = minute < 0 ? 0 : (minute > 59 ? 59 : minute);
    e.enabled = true;
    alarms_.push_back(e);
    Save();
    ESP_LOGI(TAG, "Adicionado %02d:%02d (id=%s)", e.hour, e.minute, e.id.c_str());
    return e.id;
}

bool AlarmEngine::RemoveAlarm(const std::string& id) {
    for (auto it = alarms_.begin(); it != alarms_.end(); ++it) {
        if (it->id == id) {
            alarms_.erase(it);
            Save();
            return true;
        }
    }
    return false;
}

bool AlarmEngine::SetEnabled(const std::string& id, bool enabled) {
    for (auto& a : alarms_) {
        if (a.id == id) {
            a.enabled = enabled;
            Save();
            return true;
        }
    }
    return false;
}

bool AlarmEngine::UpdateAlarm(const std::string& id, int hour, int minute) {
    for (auto& a : alarms_) {
        if (a.id == id) {
            a.hour = hour < 0 ? 0 : (hour > 23 ? 23 : hour);
            a.minute = minute < 0 ? 0 : (minute > 59 ? 59 : minute);
            Save();
            return true;
        }
    }
    return false;
}

std::string AlarmEngine::ListJson() const {
    cJSON* root = cJSON_CreateArray();
    for (const auto& a : alarms_) {
        cJSON* o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "id", a.id.c_str());
        cJSON_AddNumberToObject(o, "hour", a.hour);
        cJSON_AddNumberToObject(o, "minute", a.minute);
        cJSON_AddBoolToObject(o, "enabled", a.enabled);
        cJSON_AddItemToArray(root, o);
    }
    char* s = cJSON_PrintUnformatted(root);
    std::string out(s);
    cJSON_free(s);
    cJSON_Delete(root);
    return out;
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

    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);

    for (auto& a : alarms_) {
        if (!a.enabled) continue;
        if (ti.tm_hour != a.hour || ti.tm_min != a.minute) continue;
        if (a.last_fired_yday == ti.tm_yday) continue;  // ja disparou hoje

        a.last_fired_yday = ti.tm_yday;
        firing_ = true;
        firing_started_us_ = now_us;
        last_ring_us_ = now_us;
        ESP_LOGI(TAG, "Disparado (%02d:%02d)", a.hour, a.minute);
        if (on_fired_) on_fired_();
        return;  // um por vez -- se dois baterem no mesmo minuto, o
                 // segundo dispara sozinho no proximo Check(), depois
                 // que este for desligado
    }
}

void AlarmEngine::Dismiss() {
    if (!firing_) return;
    firing_ = false;
    ESP_LOGI(TAG, "Desligado");
    if (on_dismissed_) on_dismissed_();
}
