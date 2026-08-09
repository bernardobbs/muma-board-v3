#include "semaphore_tool.h"
#include "notifications.h"
#include "device_config.h"

#include <esp_log.h>

#define TAG "Semaforo"

std::string OverloadSemaphore::LevelName() const {
    switch (level_) {
        case OverloadLevel::VERDE:    return "verde";
        case OverloadLevel::AMARELO:  return "amarelo";
        case OverloadLevel::VERMELHO: return "vermelho";
    }
    return "verde";
}

void OverloadSemaphore::SetLevel(OverloadLevel level) {
    level_ = level;
    ESP_LOGI(TAG, "Nivel definido: %s", LevelName().c_str());
    if (on_level_) on_level_(level_);
    Evaluate(level);
}

void OverloadSemaphore::Evaluate(OverloadLevel level) {
    auto& cfg = DeviceConfig::GetInstance();
    bool should = false, automatic = false;

    if (level == OverloadLevel::AMARELO) {
        should = cfg.notify_on_yellow();
        automatic = cfg.auto_send_yellow();
    } else if (level == OverloadLevel::VERMELHO) {
        should = cfg.notify_on_red();
        automatic = cfg.auto_send_red();
    } else {
        pending_ = false;               // voltou ao verde: cancela pendencia
        return;
    }

    if (!should) return;

    if (automatic) {
        Send(level);
    } else {
        pending_ = true;
        pending_level_ = level;
        ESP_LOGI(TAG, "Aguardando confirmacao dela antes de avisar");
        if (on_confirm_needed_) on_confirm_needed_();
    }
}

void OverloadSemaphore::ConfirmSend() {
    if (!pending_) return;
    Send(pending_level_);
    pending_ = false;
}

void OverloadSemaphore::CancelSend() {
    pending_ = false;
    ESP_LOGI(TAG, "Ok, nao vou avisar");
}

void OverloadSemaphore::Send(OverloadLevel level) {
    // Vermelho ignora qualquer agendamento em lote e vai na hora --
    // alerta de crise nao pode esperar sincronizacao.
    std::string title = "Alerta";
    std::string msg = "Nivel sinalizado: " + LevelName();
    int priority = (level == OverloadLevel::VERMELHO) ? 5 : 3;
    notifications::SendNtfy(title, msg, priority);
}

std::string OverloadSemaphore::StatusJson() const {
    return "{\"nivel\":\"" + LevelName() + "\",\"pendente\":" +
           (pending_ ? "true" : "false") + "}";
}
