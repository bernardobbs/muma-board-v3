#pragma once
#include <esp_timer.h>
#include <functional>
#include <string>

// Alarme de horario (HH:MM), configurado em /admin, dispara uma vez por
// dia. So depende do RTC local (mesmo mecanismo que ChildProfile/
// RoutineEngine ja usam pra virada de dia) -- nenhuma dependencia de rede
// nem de bridge externo.
class AlarmEngine {
public:
    static AlarmEngine& GetInstance() { static AlarmEngine i; return i; }

    void Initialize();   // le do NVS e cria o timer de checagem
    void Set(int hour, int minute, bool enabled);
    void Dismiss();      // crianca confirma (ex: botao) -- para o som/tela

    int hour() const { return hour_; }
    int minute() const { return minute_; }
    bool enabled() const { return enabled_; }
    bool firing() const { return firing_; }
    std::string ToJson() const;

    // Disparado UMA VEZ quando o alarme comeca a tocar -- quem ouvir cria
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
    void Check();
    static void CheckTrampoline(void* arg);

    esp_timer_handle_t timer_ = nullptr;
    int hour_ = 7, minute_ = 0;
    bool enabled_ = false;
    bool firing_ = false;
    int last_fired_yday_ = -1;       // tm_yday da ultima vez que disparou -- evita repetir no mesmo dia
    int64_t firing_started_us_ = 0;
    int64_t last_ring_us_ = 0;

    static constexpr int kCheckIntervalSec = 5;
    static constexpr int kRingIntervalSec = 3;    // repete o som a cada 3s enquanto tocando
    static constexpr int kMaxRingSeconds = 60;    // desiste sozinho depois de 1min sem ninguem desligar

    std::function<void()> on_fired_, on_ring_tick_, on_dismissed_;
};
