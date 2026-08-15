#pragma once
#include <esp_timer.h>
#include <functional>
#include <string>

enum class PomodoroState { IDLE, STUDY, WARNING, BREAK, PAUSED };

class PomodoroEngine {
public:
    static PomodoroEngine& GetInstance() { static PomodoroEngine i; return i; }

    void Initialize();          // cria o timer; le duracoes do DeviceConfig
    void ReloadRules();         // chamado quando a config muda pela pagina web

    void Start();
    void Pause();
    void Resume();
    void Stop();

    PomodoroState state() const { return state_; }
    int seconds_remaining() const;
    std::string StateName() const;
    std::string StatusJson() const;

    void SetOnPhaseChanged(std::function<void(PomodoroState)> cb) { on_phase_changed_ = cb; }
    // Emitido a cada segundo enquanto rodando -- pra atualizar o label do
    // cronometro na tela sem a UI precisar de timer proprio.
    void SetOnTick(std::function<void(int)> cb) { on_tick_ = cb; }
    // Emitido SO quando a pausa termina naturalmente (chegou a zero),
    // nunca quando Stop() interrompe -- diferente de SetOnPhaseChanged,
    // que dispara IDLE nos dois casos e nao da pra distinguir um do
    // outro. Usado pro bonus de "cumpriu a pausa".
    void SetOnBreakCompleted(std::function<void()> cb) { on_break_completed_ = cb; }
    // Emitido quando ela inicia um novo foco DENTRO da janela de "volta
    // rapida" depois da ultima pausa CUMPRIDA (nunca apos Stop() --
    // mesma logica de SetOnBreakCompleted, pra nao dar pra ganhar o
    // bonus so reiniciando o ciclo no meio). So dispara uma vez por
    // pausa cumprida (ver Start()).
    //
    // DORMENTE desde que a pausa passou a encadear direto num foco novo
    // (Tick()) em vez de esperar em IDLE: nao existe mais o momento de
    // "a crianca decidiu voltar" pra recompensar -- break_completed_at_us_
    // nunca mais e setado. Deixado registrado (nao removido) porque
    // ainda pode fazer sentido reaproveitar como bonus de sequencia (N
    // ciclos seguidos), se um dia quiserem isso.
    void SetOnQuickReturn(std::function<void()> cb) { on_quick_return_ = cb; }

private:
    PomodoroEngine() = default;
    ~PomodoroEngine();
    void Tick();
    static void TickTrampoline(void* arg);
    void EnterPhase(PomodoroState phase, int seconds);
    void EnterIdle();

    esp_timer_handle_t timer_ = nullptr;
    PomodoroState state_ = PomodoroState::IDLE;
    int64_t phase_start_us_ = 0, phase_duration_us_ = 0, paused_remaining_us_ = 0;
    int64_t break_completed_at_us_ = -1;  // -1 = nenhuma pausa cumprida pendente de bonus
    bool warning_fired_ = false;
    int study_seconds_ = 25 * 60, break_seconds_ = 5 * 60, warning_seconds_ = 120;
    int quick_return_seconds_ = 5 * 60;

    std::function<void(PomodoroState)> on_phase_changed_;
    std::function<void(int)> on_tick_;
    std::function<void()> on_break_completed_;
    std::function<void()> on_quick_return_;
};
