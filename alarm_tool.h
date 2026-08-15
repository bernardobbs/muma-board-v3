#pragma once
#include <esp_timer.h>
#include <functional>
#include <string>
#include <vector>

struct AlarmEntry {
    std::string id;
    int hour = 7;
    int minute = 0;
    bool enabled = true;
    int last_fired_yday = -1;   // runtime, nao persistido -- evita repetir no mesmo dia
};

// Varios alarmes de horario (HH:MM), configurados pela PROPRIA crianca
// em "/" (sem senha -- e sobre como ela usa o aparelho, nao uma regra
// dos responsaveis, por isso nao mora em /admin). Cada alarme dispara
// uma vez por dia. So depende do RTC local -- nenhuma dependencia de
// rede nem de bridge externo.
class AlarmEngine {
public:
    static AlarmEngine& GetInstance() { static AlarmEngine i; return i; }

    void Initialize();   // le do NVS e cria o timer de checagem
    std::string AddAlarm(int hour, int minute);       // devolve o id gerado
    bool RemoveAlarm(const std::string& id);
    bool SetEnabled(const std::string& id, bool enabled);
    bool UpdateAlarm(const std::string& id, int hour, int minute);
    std::string ListJson() const;

    bool firing() const { return firing_; }
    void Dismiss();      // crianca confirma (ex: botao) -- para o som/tela

    // Disparado UMA VEZ quando um alarme comeca a tocar -- quem ouvir cria
    // a tela de alerta e toca o som a primeira vez.
    void SetOnFired(std::function<void()> cb) { on_fired_ = cb; }
    // Disparado a cada poucos segundos ENQUANTO estiver tocando -- so pra
    // repetir o som, sem recriar a tela (SetOnFired ja fez isso).
    void SetOnRingTick(std::function<void()> cb) { on_ring_tick_ = cb; }
    // Disparado quando o alarme para de tocar (Dismiss() ou timeout) --
    // quem ouvir esconde a tela de alerta.
    void SetOnDismissed(std::function<void()> cb) { on_dismissed_ = cb; }

private:
    AlarmEngine() = default;
    void Load();
    void Save() const;
    void Check();
    static void CheckTrampoline(void* arg);
    static std::string GenId();

    std::vector<AlarmEntry> alarms_;
    esp_timer_handle_t timer_ = nullptr;
    bool firing_ = false;
    int64_t firing_started_us_ = 0;
    int64_t last_ring_us_ = 0;

    static constexpr int kCheckIntervalSec = 5;
    static constexpr int kRingIntervalSec = 3;    // repete o som a cada 3s enquanto tocando
    static constexpr int kMaxRingSeconds = 60;    // desiste sozinho depois de 1min sem ninguem desligar

    std::function<void()> on_fired_, on_ring_tick_, on_dismissed_;
};
