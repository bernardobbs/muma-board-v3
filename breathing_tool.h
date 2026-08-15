#pragma once
#include <esp_timer.h>
#include <functional>

enum class BreathPhase { INSPIRE, SEGURA, SOLTA };

// Exercicio de respiracao guiada ("cantinho da calma") -- ferramenta de
// regulacao que a PROPRIA crianca aciona (ou a IA sugere por voz), por
// isso mora em "/" sem senha, e nao depende do toggle de "ferramentas
// de regulacao" do semaforo (esse e sobre TEA/TDAH/ansiedade
// especificamente; respirar pra se calmar serve pra qualquer crianca,
// e o equivalente digital do "cantinho da calma" comum em escola
// infantil).
class BreathingExercise {
public:
    static BreathingExercise& GetInstance() { static BreathingExercise i; return i; }

    void Initialize();
    void Start();
    void Stop();
    bool active() const { return active_; }

    // progress: 0.0-1.0 dentro da fase atual -- a UI so precisa ler e
    // desenhar (crescer/encolher o circulo), sem calcular nada.
    void SetOnTick(std::function<void(BreathPhase phase, float progress)> cb) { on_tick_ = cb; }
    void SetOnStopped(std::function<void()> cb) { on_stopped_ = cb; }

private:
    BreathingExercise() = default;
    void Tick();
    static void TickTrampoline(void* arg);

    esp_timer_handle_t timer_ = nullptr;
    bool active_ = false;
    int64_t phase_start_us_ = 0;
    BreathPhase phase_ = BreathPhase::INSPIRE;

    static constexpr int kPhaseSeconds = 4;      // inspira 4s, segura 4s, solta 4s
    static constexpr int kTickIntervalMs = 200;  // 5 atualizacoes/s -- suave sem pesar

    std::function<void(BreathPhase, float)> on_tick_;
    std::function<void()> on_stopped_;
};
