#include "tamagotchi_tool.h"
#include "device_config.h"
#include "child_profile.h"
#include "day_utils.h"
#include "settings.h"

#include <cJSON.h>
#include <esp_log.h>
#include <algorithm>

#define TAG "Tamagotchi"

void Tamagotchi::Initialize() {
    // Namespace fixo: cada aparelho pertence a UMA crianca agora (nao
    // mais um "modelo Alana" vs "modelo Clara"), entao nao precisa mais
    // de sufixo por perfil.
    ns_ = "tama";

    // Catalogo UNICO pra qualquer idade -- as opcoes do documento
    // "Family Companion OS" foram unificadas numa lista so. Nao
    // restringimos por faixa etaria: quem escolhe e a crianca, nao o
    // firmware, e gosto nao e coisa que se determine por idade.
    catalog_ = {
        {"lobo","Lobo","Atlas"},        {"raposa","Raposa","Fox"},
        {"gato","Gato","Nino"},         {"dragao","Dragao","Draco"},
        {"unicornio","Unicornio","Lili"},{"coelha","Coelha","Lua"},
        {"panda","Panda","Pingo"},      {"pintinho","Pintinho","Bibi"},
    };

    Settings s(ns_, false);
    total_points_ = s.GetInt("points", 0);
    today_points_ = s.GetInt("today", 0);
    stored_day_   = s.GetInt("day", 0);

    std::string saved = s.GetString("species_id", "");
    int idx = saved.empty() ? -1 : IndexForId(saved);
    species_index_ = (idx < 0) ? 0 : idx;

    RecalcStage();
    CheckDayRollover();
    ESP_LOGI(TAG, "%s: %d pontos, estagio %s", pet_name().c_str(), total_points_, StageName().c_str());
}

int Tamagotchi::IndexForId(const std::string& id) const {
    for (size_t i = 0; i < catalog_.size(); i++)
        if (catalog_[i].id == id) return (int)i;
    return -1;
}

std::string Tamagotchi::pet_name() const {
    return catalog_.empty() ? "?" : catalog_[species_index_].suggested_name;
}
std::string Tamagotchi::species_id() const {
    return catalog_.empty() ? "" : catalog_[species_index_].id;
}

bool Tamagotchi::ChooseSpecies(const std::string& id) {
    int idx = IndexForId(id);
    if (idx < 0) return false;
    species_index_ = idx;
    Settings s(ns_, true);
    s.SetString("species_id", id);
    ESP_LOGI(TAG, "Companheiro agora e %s, o %s",
             pet_name().c_str(), catalog_[idx].species_label.c_str());
    if (on_species_) on_species_(id);   // board recarrega os GIFs da especie
    return true;
}

std::string Tamagotchi::StageName() const {
    switch (stage_) {
        case TamaStage::OVO: return "Ovo";
        case TamaStage::FILHOTE: return "Filhote";
        case TamaStage::JOVEM: return "Jovem";
        case TamaStage::FORTE: return "Forte";
    }
    return "?";
}

// ESTAS strings sao as chaves exatas do EmojiCollection -- manter em
// sincronia com os AddEmoji() de pet_emoji_collection.cc
std::string Tamagotchi::MoodName() const {
    switch (mood_) {
        case TamaMood::NEUTRO:      return "neutro";
        case TamaMood::FOCADO:      return "focado";
        case TamaMood::AVISO:       return "aviso";
        case TamaMood::BRINCALHAO:  return "brincalhao";
        case TamaMood::COMEMORANDO: return "comemorando";
    }
    return "neutro";
}

void Tamagotchi::SetMood(TamaMood mood) {
    if (mood_ == mood) return;          // evita redesenhar a toa
    mood_ = mood;
    if (on_mood_) on_mood_(MoodName());
}

void Tamagotchi::CheckDayRollover() {
    int32_t today = day_utils::TodayStamp();
    if (today == 0) return;             // relogio nao sincronizou -- nao mexe

    if (stored_day_ == 0) {             // 1a vez com relogio valido: so registra
        stored_day_ = today;
        Settings s(ns_, true);
        s.SetInt("day", today);
        return;
    }
    if (today != stored_day_) {
        today_points_ = 0;
        stored_day_ = today;
        Settings s(ns_, true);
        s.SetInt("today", 0);
        s.SetInt("day", today);
        ESP_LOGI(TAG, "Novo dia -- contador diario zerado");
    }
}

void Tamagotchi::AddPoints(int amount) {
    CheckDayRollover();
    int cap = DeviceConfig::GetInstance().daily_point_cap();
    if (today_points_ >= cap) {
        ESP_LOGI(TAG, "Teto diario atingido -- sem pontos extra hoje (proposital)");
        return;
    }
    int allowed = std::min(amount, cap - today_points_);
    total_points_ += allowed;
    today_points_ += allowed;
    Settings s(ns_, true);
    s.SetInt("points", total_points_);
    s.SetInt("today", today_points_);
    RecalcStage();
}

void Tamagotchi::OnPomodoroCompleted() {
    AddPoints(1);
    SetMood(TamaMood::COMEMORANDO);
}
void Tamagotchi::OnBreakRespected() { AddPoints(1); }

void Tamagotchi::AddCustomPoints(int amount, const std::string& reason) {
    AddPoints(amount);
    ESP_LOGI(TAG, "+%d pontos (%s)", amount, reason.c_str());
}

void Tamagotchi::RecalcStage() {
    auto& cfg = DeviceConfig::GetInstance();
    TamaStage ns = TamaStage::OVO;
    if (total_points_ >= cfg.stage4()) ns = TamaStage::FORTE;
    else if (total_points_ >= cfg.stage3()) ns = TamaStage::JOVEM;
    else if (total_points_ >= cfg.stage2()) ns = TamaStage::FILHOTE;

    if ((int)ns > (int)stage_) {        // SO SOBE, nunca regride
        stage_ = ns;
        ESP_LOGI(TAG, "%s EVOLUIU para %s!", pet_name().c_str(), StageName().c_str());
        if (on_evolved_) on_evolved_(stage_);
    }
}

std::string Tamagotchi::StatusJson() const {
    cJSON* r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "nome", pet_name().c_str());
    cJSON_AddStringToObject(r, "especie", species_id().c_str());
    cJSON_AddStringToObject(r, "estagio", StageName().c_str());
    cJSON_AddStringToObject(r, "humor", MoodName().c_str());
    cJSON_AddNumberToObject(r, "pontos", total_points_);
    cJSON_AddNumberToObject(r, "hoje", today_points_);
    cJSON_AddNumberToObject(r, "teto", DeviceConfig::GetInstance().daily_point_cap());

    // A pagina precisa disto pra montar os botoes de escolha -- estava
    // faltando na versao anterior, entao a secao ficava vazia.
    cJSON* cat = cJSON_AddArrayToObject(r, "catalogo");
    for (const auto& c : catalog_) {
        cJSON* it = cJSON_CreateObject();
        cJSON_AddStringToObject(it, "id", c.id.c_str());
        cJSON_AddStringToObject(it, "nome", c.suggested_name.c_str());
        cJSON_AddStringToObject(it, "especie", c.species_label.c_str());
        cJSON_AddItemToArray(cat, it);
    }

    // Proximo limiar, pra barra de progresso da pagina
    auto& cfg = DeviceConfig::GetInstance();
    int next = (total_points_ < cfg.stage2()) ? cfg.stage2()
             : (total_points_ < cfg.stage3()) ? cfg.stage3()
             : (total_points_ < cfg.stage4()) ? cfg.stage4() : 0;
    cJSON_AddNumberToObject(r, "proximo_estagio", next);

    char* s = cJSON_PrintUnformatted(r);
    std::string out(s);
    cJSON_free(s);
    cJSON_Delete(r);
    return out;
}
