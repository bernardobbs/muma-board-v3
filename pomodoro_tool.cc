#include "pomodoro_tool.h"
#include "device_config.h"

#include <cJSON.h>
#include <esp_log.h>

#define TAG "Pomodoro"

PomodoroEngine::~PomodoroEngine() {
    if (timer_) { esp_timer_stop(timer_); esp_timer_delete(timer_); }
}

void PomodoroEngine::TickTrampoline(void* arg) {
    static_cast<PomodoroEngine*>(arg)->Tick();
}

void PomodoroEngine::Initialize() {
    ReloadRules();
    if (timer_ != nullptr) return;
    esp_timer_create_args_t args = {};
    args.callback = &PomodoroEngine::TickTrampoline;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "pomodoro_tick";
    if (esp_timer_create(&args, &timer_) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar o timer");
        return;
    }
    esp_timer_start_periodic(timer_, 1000000);  // 1s
}

void PomodoroEngine::ReloadRules() {
    // Antes as duracoes eram `constexpr` -- mudar exigia recompilar.
    auto& cfg = DeviceConfig::GetInstance();
    study_seconds_        = cfg.study_minutes() * 60;
    break_seconds_        = cfg.break_minutes() * 60;
    warning_seconds_      = cfg.warning_seconds();
    quick_return_seconds_ = cfg.quick_return_minutes() * 60;
    ESP_LOGI(TAG, "Regras: foco %ds, pausa %ds, aviso %ds antes, volta rapida em %ds",
             study_seconds_, break_seconds_, warning_seconds_, quick_return_seconds_);
}

void PomodoroEngine::EnterPhase(PomodoroState phase, int seconds) {
    state_ = phase;
    phase_start_us_ = esp_timer_get_time();
    phase_duration_us_ = (int64_t)seconds * 1000000;
    warning_fired_ = false;
    if (on_phase_changed_) on_phase_changed_(state_);
}

void PomodoroEngine::EnterIdle() {
    state_ = PomodoroState::IDLE;
    phase_duration_us_ = 0;
    if (on_phase_changed_) on_phase_changed_(state_);
    if (on_tick_) on_tick_(0);
}

void PomodoroEngine::Start() {
    if (state_ != PomodoroState::IDLE) { ESP_LOGI(TAG, "Ja em andamento"); return; }

    // Bonus de "volta rapida": so conta se a ULTIMA pausa foi cumprida
    // (break_completed_at_us_ setado em Tick(), nunca em Stop()) e o
    // foco novo comecou dentro da janela configurada. Consumido aqui --
    // um Start() so pode disparar o bonus uma vez por pausa cumprida,
    // mesmo que a crianca de Stop() e Start() de novo varias vezes.
    if (break_completed_at_us_ >= 0) {
        int64_t elapsed = esp_timer_get_time() - break_completed_at_us_;
        if (elapsed <= (int64_t)quick_return_seconds_ * 1000000) {
            if (on_quick_return_) on_quick_return_();
        }
        break_completed_at_us_ = -1;
    }

    EnterPhase(PomodoroState::STUDY, study_seconds_);
}

void PomodoroEngine::Pause() {
    if (state_ != PomodoroState::STUDY) return;
    int64_t elapsed = esp_timer_get_time() - phase_start_us_;
    paused_remaining_us_ = phase_duration_us_ - elapsed;
    if (paused_remaining_us_ < 0) paused_remaining_us_ = 0;
    state_ = PomodoroState::PAUSED;
    if (on_phase_changed_) on_phase_changed_(state_);
}

void PomodoroEngine::Resume() {
    if (state_ != PomodoroState::PAUSED) return;
    phase_start_us_ = esp_timer_get_time();
    phase_duration_us_ = paused_remaining_us_;
    state_ = PomodoroState::STUDY;
    if (on_phase_changed_) on_phase_changed_(state_);
}

void PomodoroEngine::Stop() {
    // Interrompido no meio = neutro, SEM penalidade. Regra desde o inicio.
    ESP_LOGI(TAG, "Interrompido -- sem penalidade, sem pontos deste ciclo");
    EnterIdle();
}

void PomodoroEngine::Tick() {
    if (state_ == PomodoroState::IDLE || state_ == PomodoroState::PAUSED) return;

    int64_t elapsed = esp_timer_get_time() - phase_start_us_;
    int64_t remaining = phase_duration_us_ - elapsed;
    if (remaining < 0) remaining = 0;

    // Aviso previo antes do fim da PAUSA -- decisao explicita: SEM aviso
    // antes do fim do foco (so essa ponta importava).
    if (state_ == PomodoroState::BREAK && !warning_fired_ &&
        remaining <= (int64_t)warning_seconds_ * 1000000) {
        warning_fired_ = true;
        state_ = PomodoroState::BREAK_WARNING;
        if (on_phase_changed_) on_phase_changed_(state_);
    }

    if (on_tick_) on_tick_((int)(remaining / 1000000));

    if (remaining == 0) {
        if (state_ == PomodoroState::STUDY) {
            EnterPhase(PomodoroState::BREAK, break_seconds_);
        } else if (state_ == PomodoroState::BREAK || state_ == PomodoroState::BREAK_WARNING) {
            // Pausa cumprida ate o fim (Stop() nao passa por aqui, entao
            // nunca aciona isso -- interromper nao e "cumprir a pausa").
            // Antes ia pra IDLE e esperava a crianca mandar "comeca" de
            // novo; agora encadeia direto num novo foco, sem parar --
            // pedido pra o pomodoro rodar em ciclos continuos sozinho,
            // so parando de verdade com Stop().
            if (on_break_completed_) on_break_completed_();
            EnterPhase(PomodoroState::STUDY, study_seconds_);
        }
    }
}

int PomodoroEngine::seconds_remaining() const {
    if (state_ == PomodoroState::IDLE) return 0;
    if (state_ == PomodoroState::PAUSED) return (int)(paused_remaining_us_ / 1000000);
    int64_t remaining = phase_duration_us_ - (esp_timer_get_time() - phase_start_us_);
    return remaining > 0 ? (int)(remaining / 1000000) : 0;
}

std::string PomodoroEngine::StateName() const {
    switch (state_) {
        case PomodoroState::IDLE:    return "parado";
        case PomodoroState::STUDY:   return "focando";
        case PomodoroState::BREAK:   return "pausa";
        case PomodoroState::BREAK_WARNING: return "pausa quase acabando";
        case PomodoroState::PAUSED:  return "pausado";
    }
    return "?";
}

std::string PomodoroEngine::StatusJson() const {
    cJSON* r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "estado", StateName().c_str());
    cJSON_AddNumberToObject(r, "restante_s", seconds_remaining());
    char* s = cJSON_PrintUnformatted(r);
    std::string out(s);
    cJSON_free(s);
    cJSON_Delete(r);
    return out;
}
