#include "device_config.h"
#include "settings.h"
#include "child_profile.h"

#include <cJSON.h>
#include <esp_log.h>
#include <stdlib.h>
#include <time.h>

#define TAG "DeviceConfig"
static const char* NS = "device";

void DeviceConfig::Load() {
    Settings s(NS, false);

    // Defaults vem da faixa etaria (ChildProfile) -- SO como ponto de
    // partida, nao como regra fixa. Se ja houver algo salvo, isso tem
    // prioridade (respeita ajuste manual feito em /admin).
    auto ad = ChildProfile::GetInstance().BracketDefaults();
    study_min_   = s.GetInt("study_min",  ad.study_min);
    break_min_   = s.GetInt("break_min",  ad.break_min);
    warning_sec_ = s.GetInt("warn_sec",   ad.warning_sec);
    quick_return_min_ = s.GetInt("quick_return", 5);
    daily_cap_   = s.GetInt("daily_cap",  ad.daily_cap);
    stage2_      = s.GetInt("stage2",     ad.stage2);
    stage3_      = s.GetInt("stage3",     ad.stage3);
    stage4_      = s.GetInt("stage4",     ad.stage4);

    notify_yellow_ = s.GetBool("y_on", false);
    notify_red_    = s.GetBool("r_on", true);
    auto_yellow_   = s.GetBool("y_auto", false);
    auto_red_      = s.GetBool("r_auto", true);

    ntfy_server_ = s.GetString("ntfy_srv", "https://ntfy.sh");
    ntfy_topic_  = s.GetString("ntfy_top", "");

    brightness_ = s.GetInt("bright", 80);
    volume_     = s.GetInt("volume", 70);
    timezone_   = s.GetString("tz", "<-03>3");

    ApplyTimezone();
    ESP_LOGI(TAG, "Pomodoro %d/%dmin | TZ %s", study_min_, break_min_, timezone_.c_str());
}

void DeviceConfig::set_pomodoro(int study_min, int break_min) {
    study_min_ = Clamp(study_min, kMinStudyMin, kMaxStudyMin);
    break_min_ = Clamp(break_min, kMinBreakMin, kMaxBreakMin);
    Settings s(NS, true);
    s.SetInt("study_min", study_min_);
    s.SetInt("break_min", break_min_);
    if (on_pomodoro_rules_) on_pomodoro_rules_();
}

void DeviceConfig::set_quick_return_minutes(int minutes) {
    quick_return_min_ = Clamp(minutes, kMinQuickReturnMin, kMaxQuickReturnMin);
    Settings s(NS, true);
    s.SetInt("quick_return", quick_return_min_);
}

void DeviceConfig::set_warning_seconds(int seconds) {
    // Nunca deixar o aviso previo maior que o proprio bloco de foco
    warning_sec_ = Clamp(seconds, 0, study_min_ * 60 - 30);
    Settings s(NS, true);
    s.SetInt("warn_sec", warning_sec_);
    if (on_pomodoro_rules_) on_pomodoro_rules_();
}

void DeviceConfig::set_tama_rules(int daily_cap, int s2, int s3, int s4) {
    daily_cap_ = Clamp(daily_cap, 1, 100);
    // Garante ordem crescente -- limiares fora de ordem quebrariam a evolucao
    stage2_ = Clamp(s2, 1, 9999);
    stage3_ = Clamp(s3, stage2_ + 1, 9999);
    stage4_ = Clamp(s4, stage3_ + 1, 9999);
    Settings s(NS, true);
    s.SetInt("daily_cap", daily_cap_);
    s.SetInt("stage2", stage2_);
    s.SetInt("stage3", stage3_);
    s.SetInt("stage4", stage4_);
}

void DeviceConfig::set_alert_rules(bool y_on, bool r_on, bool y_auto, bool r_auto) {
    notify_yellow_ = y_on;
    notify_red_ = r_on;
    auto_yellow_ = y_auto;
    auto_red_ = r_auto;
    Settings s(NS, true);
    s.SetBool("y_on", y_on);
    s.SetBool("r_on", r_on);
    s.SetBool("y_auto", y_auto);
    s.SetBool("r_auto", r_auto);
}

void DeviceConfig::set_ntfy(const std::string& server, const std::string& topic) {
    ntfy_server_ = server;
    ntfy_topic_ = topic;
    Settings s(NS, true);
    s.SetString("ntfy_srv", server);
    s.SetString("ntfy_top", topic);
}

void DeviceConfig::set_brightness(int v) {
    brightness_ = Clamp(v, 10, 100);
    Settings s(NS, true);
    s.SetInt("bright", brightness_);
    if (on_brightness_) on_brightness_(brightness_);  // agora APLICA de verdade
}

void DeviceConfig::set_volume(int v) {
    volume_ = Clamp(v, 0, 100);
    Settings s(NS, true);
    s.SetInt("volume", volume_);
    if (on_volume_) on_volume_(volume_);
}

void DeviceConfig::set_timezone(const std::string& tz) {
    timezone_ = tz;
    Settings s(NS, true);
    s.SetString("tz", tz);
    ApplyTimezone();
}

void DeviceConfig::ApplyTimezone() const {
    // Sem isso, localtime_r devolve UTC e a virada do dia aconteceria
    // as 21h no horario do Piaui em vez da meia-noite.
    setenv("TZ", timezone_.c_str(), 1);
    tzset();
}

std::string DeviceConfig::ToJson() const {
    cJSON* r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "nome", ChildProfile::GetInstance().name().c_str());
    cJSON_AddNumberToObject(r, "study_min", study_min_);
    cJSON_AddNumberToObject(r, "break_min", break_min_);
    cJSON_AddNumberToObject(r, "brightness", brightness_);
    cJSON_AddNumberToObject(r, "volume", volume_);
    cJSON_AddNumberToObject(r, "min_study", kMinStudyMin);
    cJSON_AddNumberToObject(r, "max_study", kMaxStudyMin);
    cJSON_AddNumberToObject(r, "min_break", kMinBreakMin);
    cJSON_AddNumberToObject(r, "max_break", kMaxBreakMin);

    // Somente leitura na pagina dela: ela VE que o alerta existe e em que
    // nivel, mas nao muda. Transparencia, nao vigilancia.
    cJSON* a = cJSON_AddObjectToObject(r, "alerta_pais");
    cJSON_AddBoolToObject(a, "amarelo", notify_yellow_);
    cJSON_AddBoolToObject(a, "vermelho", notify_red_);
    cJSON_AddBoolToObject(a, "somente_leitura", true);

    char* str = cJSON_PrintUnformatted(r);
    std::string out(str);
    cJSON_free(str);
    cJSON_Delete(r);
    return out;
}

std::string DeviceConfig::ToAdminJson() const {
    auto& child = ChildProfile::GetInstance();
    cJSON* r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "nome", child.name().c_str());
    cJSON_AddStringToObject(r, "data_nascimento", child.birth_date().c_str());
    cJSON_AddNumberToObject(r, "idade", child.AgeYears());
    cJSON_AddBoolToObject(r, "regulacao_ativa", child.regulation_tools_enabled());
    cJSON_AddNumberToObject(r, "study_min", study_min_);
    cJSON_AddNumberToObject(r, "break_min", break_min_);
    cJSON_AddNumberToObject(r, "warn_sec", warning_sec_);
    cJSON_AddNumberToObject(r, "quick_return", quick_return_min_);
    cJSON_AddNumberToObject(r, "daily_cap", daily_cap_);
    cJSON_AddNumberToObject(r, "stage2", stage2_);
    cJSON_AddNumberToObject(r, "stage3", stage3_);
    cJSON_AddNumberToObject(r, "stage4", stage4_);
    cJSON_AddBoolToObject(r, "y_on", notify_yellow_);
    cJSON_AddBoolToObject(r, "r_on", notify_red_);
    cJSON_AddBoolToObject(r, "y_auto", auto_yellow_);
    cJSON_AddBoolToObject(r, "r_auto", auto_red_);
    cJSON_AddStringToObject(r, "ntfy_srv", ntfy_server_.c_str());
    cJSON_AddStringToObject(r, "ntfy_top", ntfy_topic_.c_str());
    cJSON_AddStringToObject(r, "tz", timezone_.c_str());

    char* str = cJSON_PrintUnformatted(r);
    std::string out(str);
    cJSON_free(str);
    cJSON_Delete(r);
    return out;
}
