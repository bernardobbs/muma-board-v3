#pragma once
#include <string>
#include <functional>

// RECUPERACAO = ela sinalizando que esta voltando ao normal depois de
// um amarelo/vermelho -- nao e "pior que vermelho", e uma desescalada,
// tratada como o verde pra fins de notificacao (ver Evaluate()).
enum class OverloadLevel { VERDE, AMARELO, VERMELHO, RECUPERACAO };

// Semaforo de sobrecarga -- toggle generico em ChildProfile::regulation_tools_enabled().
// Regra de ouro: NUNCA muda de nivel sozinho -- so a crianca ativa, por
// voz ou toque. O dispositivo nunca pergunta como ela esta.
class OverloadSemaphore {
public:
    static OverloadSemaphore& GetInstance() { static OverloadSemaphore i; return i; }

    void SetLevel(OverloadLevel level);
    OverloadLevel level() const { return level_; }
    std::string LevelName() const;
    std::string StatusJson() const;

    bool has_pending_confirmation() const { return pending_; }
    void ConfirmSend();
    void CancelSend();

    void SetOnLevelChanged(std::function<void(OverloadLevel)> cb) { on_level_ = cb; }
    void SetOnConfirmationNeeded(std::function<void()> cb) { on_confirm_needed_ = cb; }

private:
    OverloadSemaphore() = default;
    void Evaluate(OverloadLevel level);
    void Send(OverloadLevel level);

    OverloadLevel level_ = OverloadLevel::VERDE;
    bool pending_ = false;
    OverloadLevel pending_level_ = OverloadLevel::VERDE;
    std::function<void(OverloadLevel)> on_level_;
    std::function<void()> on_confirm_needed_;
};
