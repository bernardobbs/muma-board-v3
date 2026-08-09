#include "child_profile.h"
#include "day_utils.h"
#include "settings.h"

#include <esp_log.h>
#include <time.h>
#include <cstdlib>

#define TAG "ChildProfile"
static const char* NS = "child";

void ChildProfile::Load() {
    Settings s(NS, false);
    name_ = s.GetString("name", "");
    birth_date_ = s.GetString("birth_date", "");
    regulation_enabled_ = s.GetBool("regulation_on", false);
    ESP_LOGI(TAG, "Perfil: %s (nasc. %s) -- ferramentas de regulacao: %s",
             name_.empty() ? "(sem nome ainda)" : name_.c_str(),
             birth_date_.empty() ? "?" : birth_date_.c_str(),
             regulation_enabled_ ? "ligadas" : "desligadas");
}

void ChildProfile::Set(const std::string& name, const std::string& birth_date_iso) {
    name_ = name;
    birth_date_ = birth_date_iso;
    Settings s(NS, true);
    s.SetString("name", name_);
    s.SetString("birth_date", birth_date_);
}

void ChildProfile::set_regulation_tools_enabled(bool enabled) {
    regulation_enabled_ = enabled;
    Settings s(NS, true);
    s.SetBool("regulation_on", enabled);
}

int ChildProfile::AgeYears() const {
    if (birth_date_.size() != 10) return -1;   // espera "AAAA-MM-DD"
    if (!day_utils::ClockIsSynced()) return -1;  // sem relogio, sem conta confiavel

    int y, m, d;
    if (sscanf(birth_date_.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return -1;
    if (y < 1900 || m < 1 || m > 12 || d < 1 || d > 31) return -1;

    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    int current_year = ti.tm_year + 1900;
    int current_month = ti.tm_mon + 1;
    int current_day = ti.tm_mday;

    int age = current_year - y;
    if (current_month < m || (current_month == m && current_day < d)) age--;
    return age < 0 ? -1 : age;
}

AgeBracket ChildProfile::Bracket() const {
    int age = AgeYears();
    if (age < 0) return AgeBracket::DE_10_A_12;  // meio-termo neutro se idade desconhecida
    if (age <= 6) return AgeBracket::ATE_6;
    if (age <= 9) return AgeBracket::DE_7_A_9;
    if (age <= 12) return AgeBracket::DE_10_A_12;
    return AgeBracket::DE_13_MAIS;
}

// Pontos de partida, nao regra clinica -- tudo ajustavel depois em /admin.
// Escalas com base em atencao sustentada tipica por faixa e em ciclos de
// recompensa mais curtos pra criancas menores.
AgeDefaults ChildProfile::BracketDefaults() const {
    switch (Bracket()) {
        case AgeBracket::ATE_6:
            return {8, 3, 45, 10, 3, 8, 15};
        case AgeBracket::DE_7_A_9:
            return {12, 4, 60, 9, 4, 10, 20};
        case AgeBracket::DE_10_A_12:
            return {18, 5, 90, 8, 5, 12, 25};
        case AgeBracket::DE_13_MAIS:
        default:
            return {25, 5, 120, 8, 5, 15, 30};
    }
}
