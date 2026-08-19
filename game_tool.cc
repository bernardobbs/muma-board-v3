#include "game_tool.h"
#include "settings.h"

#include <cJSON.h>
#include <esp_log.h>
#include <algorithm>

#define TAG "GameEngine"
static const char* NS = "game";

void GameEngine::Load() {
    Settings s(NS, false);
    active_ = s.GetBool("active", false);
    game_id_ = s.GetString("game_id", "");
    chapter_ = s.GetInt("chapter", 0);
    stars_ = s.GetInt("stars", 0);
    mission_ = s.GetString("mission", "");
    last_choice_ = s.GetString("last_choice", "");

    characters_.clear();
    std::string saved = s.GetString("chars", "");
    if (!saved.empty()) {
        cJSON* root = cJSON_Parse(saved.c_str());
        if (root != nullptr) {
            cJSON* item = nullptr;
            cJSON_ArrayForEach(item, root) {
                if (cJSON_IsString(item)) characters_.push_back(item->valuestring);
            }
            cJSON_Delete(root);
        }
    }
}

void GameEngine::Save() const {
    Settings s(NS, true);
    s.SetBool("active", active_);
    s.SetString("game_id", game_id_);
    s.SetInt("chapter", chapter_);
    s.SetInt("stars", stars_);
    s.SetString("mission", mission_);
    s.SetString("last_choice", last_choice_);

    cJSON* root = cJSON_CreateArray();
    for (const auto& c : characters_) cJSON_AddItemToArray(root, cJSON_CreateString(c.c_str()));
    char* str = cJSON_PrintUnformatted(root);
    s.SetString("chars", str);
    cJSON_free(str);
    cJSON_Delete(root);
}

void GameEngine::Initialize() {
    Load();
    ESP_LOGI(TAG, "Estado carregado: game_id=%s capitulo=%d estrelas=%d ativo=%d",
             game_id_.c_str(), chapter_, stars_, active_);
}

void GameEngine::Start(const std::string& game_id) {
    // Troca de aventura zera tudo -- nao existe "continuar de onde
    // parou" numa aventura DIFERENTE na v1. Comecar a MESMA aventura de
    // novo tambem zera de proposito (recomecar do zero e uma escolha
    // valida, nao um bug).
    active_ = true;
    game_id_ = game_id;
    chapter_ = 1;
    stars_ = 0;
    characters_.clear();
    mission_.clear();
    last_choice_.clear();
    Save();
    ESP_LOGI(TAG, "Aventura iniciada: %s", game_id_.c_str());
}

void GameEngine::End() {
    if (!active_) return;
    active_ = false;
    Save();
    ESP_LOGI(TAG, "Aventura encerrada (estado mantido): %s", game_id_.c_str());
}

void GameEngine::SetChoice(const std::string& choice) {
    last_choice_ = choice;
    Save();
    ESP_LOGI(TAG, "Escolha registrada: %s", choice.c_str());
}

void GameEngine::AddReward(int stars, const std::string& item) {
    if (stars > 0) stars_ += stars;
    if (!item.empty() &&
        std::find(characters_.begin(), characters_.end(), item) == characters_.end()) {
        characters_.push_back(item);
    }
    Save();
    ESP_LOGI(TAG, "Recompensa: +%d estrelas (total %d), item=%s", stars, stars_, item.c_str());
}

void GameEngine::AdvanceChapter(const std::string& new_mission) {
    chapter_++;
    if (!new_mission.empty()) mission_ = new_mission;
    Save();
    ESP_LOGI(TAG, "Capitulo %d, missao: %s", chapter_, mission_.c_str());
}

std::string GameEngine::StatusJson() const {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ativo", active_);
    cJSON_AddStringToObject(root, "jogo", game_id_.c_str());
    cJSON_AddNumberToObject(root, "capitulo", chapter_);
    cJSON_AddNumberToObject(root, "estrelas", stars_);
    cJSON_AddStringToObject(root, "missao", mission_.c_str());
    cJSON_AddStringToObject(root, "ultima_escolha", last_choice_.c_str());
    cJSON* chars = cJSON_AddArrayToObject(root, "personagens");
    for (const auto& c : characters_) cJSON_AddItemToArray(chars, cJSON_CreateString(c.c_str()));
    char* s = cJSON_PrintUnformatted(root);
    std::string out(s);
    cJSON_free(s);
    cJSON_Delete(root);
    return out;
}
