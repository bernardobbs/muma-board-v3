#pragma once
#include <string>
#include <functional>

enum class OverloadLevel { VERDE, AMARELO, VERMELHO };

// Semaforo de sobrecarga (perfil Alana). Regra de ouro: NUNCA muda de
// nivel sozinho -- so ela ativa, por voz ou toque. O dispositivo nunca
// pergunta como ela esta.
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
