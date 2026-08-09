#pragma once
#include <string>

// Faixas usadas SO como ponto de partida de valores numericos
// (duracao de pomodoro, teto de pontos, limiares de evolucao) --
// nunca pra restringir o que a crianca pode escolher (catalogo de
// bichinho e o mesmo pra todo mundo, por exemplo).
enum class AgeBracket { ATE_6, DE_7_A_9, DE_10_A_12, DE_13_MAIS };

struct AgeDefaults {
    int study_min;
    int break_min;
    int warning_sec;
    int daily_cap;
    int stage2, stage3, stage4;
};

// Substitui o antigo Profile (ALANA/CLARA fixos) por identidade real:
// nome + data de nascimento. Idade e recalculada a cada consulta --
// nao precisa reconfigurar no aniversario.
class ChildProfile {
public:
    static ChildProfile& GetInstance() { static ChildProfile i; return i; }

    void Load();

    std::string name() const { return name_; }
    std::string birth_date() const { return birth_date_; }  // formato "AAAA-MM-DD"
    void Set(const std::string& name, const std::string& birth_date_iso);

    // -1 se a data de nascimento ainda nao foi configurada ou e invalida
    int AgeYears() const;
    AgeBracket Bracket() const;
    AgeDefaults BracketDefaults() const;

    // Independente de idade -- ver nota no design sobre nao amarrar
    // ferramentas de regulacao sensorial/emocional a faixa etaria.
    bool regulation_tools_enabled() const { return regulation_enabled_; }
    void set_regulation_tools_enabled(bool enabled);

private:
    ChildProfile() = default;
    std::string name_;
    std::string birth_date_;
    bool regulation_enabled_ = false;
};
