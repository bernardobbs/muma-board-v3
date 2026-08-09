#include "routine_engine.h"
#include "routine_defaults.h"
#include "device_config.h"
#include "day_utils.h"
#include "settings.h"

#include <cJSON.h>
#include <esp_log.h>

#define TAG "Routine"

void RoutineEngine::Initialize() {
    // Namespace fixo -- cada aparelho serve UMA crianca, nao um perfil
    // pre-fixado. Ver mesma decisao em tamagotchi_tool.cc.
    ns_ = "rotina";

    Settings s(ns_, false);
    std::string saved = s.GetString("def", "");
    stored_day_ = s.GetInt("day", 0);

    if (saved.empty() || !ParseJson(saved)) {
        ESP_LOGI(TAG, "Sem rotina salva (ou invalida) -- usando rascunho generico");
        ParseJson(kRoutineDefaultGeneric);
        SaveDefinition();
    }

    int32_t today = day_utils::TodayStamp();
    if (today != 0 && today == stored_day_) {
        LoadDoneState();               // mesmo dia: restaura o que ja foi feito
    } else if (today != 0) {
        ResetToday();                  // dia novo
    }
    // today == 0 (sem relogio): nao mexe -- nao apaga o progresso por engano
}

bool RoutineEngine::ParseJson(const std::string& json) {
    cJSON* root = cJSON_Parse(json.c_str());
    if (root == nullptr) return false;

    cJSON* blocks = cJSON_GetObjectItem(root, "blocks");
    if (!cJSON_IsArray(blocks)) { cJSON_Delete(root); return false; }

    std::vector<RoutineBlock> parsed;
    cJSON* b = nullptr;
    cJSON_ArrayForEach(b, blocks) {
        RoutineBlock block;
        cJSON* name = cJSON_GetObjectItem(b, "name");
        block.name = cJSON_IsString(name) ? name->valuestring : "?";

        cJSON* tasks = cJSON_GetObjectItem(b, "tasks");
        cJSON* t = nullptr;
        cJSON_ArrayForEach(t, tasks) {
            cJSON* id = cJSON_GetObjectItem(t, "id");
            cJSON* label = cJSON_GetObjectItem(t, "label");
            if (!cJSON_IsString(id) || !cJSON_IsString(label)) continue;
            if (id->valuestring[0] == '\0') continue;   // id vazio quebraria MarkDone

            RoutineTask task;
            task.id = id->valuestring;
            task.label = label->valuestring;
            cJSON* days = cJSON_GetObjectItem(t, "days");
            cJSON* pts = cJSON_GetObjectItem(t, "points");
            task.day_mask = cJSON_IsNumber(days) ? (uint8_t)(days->valueint & DAY_ALL) : DAY_ALL;
            task.points = cJSON_IsNumber(pts) ? pts->valueint : 1;
            if (task.points < 0) task.points = 0;
            block.tasks.push_back(task);
        }
        parsed.push_back(block);
    }
    cJSON_Delete(root);

    if (parsed.empty()) return false;
    blocks_ = parsed;
    return true;
}

std::string RoutineEngine::Serialize() const {
    cJSON* root = cJSON_CreateObject();
    cJSON* blocks = cJSON_AddArrayToObject(root, "blocks");
    for (const auto& b : blocks_) {
        cJSON* jb = cJSON_CreateObject();
        cJSON_AddStringToObject(jb, "name", b.name.c_str());
        cJSON* jt = cJSON_AddArrayToObject(jb, "tasks");
        for (const auto& t : b.tasks) {
            cJSON* o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "id", t.id.c_str());
            cJSON_AddStringToObject(o, "label", t.label.c_str());
            cJSON_AddNumberToObject(o, "days", t.day_mask);
            cJSON_AddNumberToObject(o, "points", t.points);
            cJSON_AddItemToArray(jt, o);
        }
        cJSON_AddItemToArray(blocks, jb);
    }
    char* s = cJSON_PrintUnformatted(root);
    std::string out(s);
    cJSON_free(s);
    cJSON_Delete(root);
    return out;
}

void RoutineEngine::SaveDefinition() {
    Settings s(ns_, true);
    s.SetString("def", Serialize());
}

void RoutineEngine::SaveDoneState() {
    std::string csv;
    for (const auto& b : blocks_)
        for (const auto& t : b.tasks)
            if (t.done) { if (!csv.empty()) csv += ","; csv += t.id; }

    Settings s(ns_, true);
    s.SetString("done", csv);
    int32_t today = day_utils::TodayStamp();
    if (today != 0) { s.SetInt("day", today); stored_day_ = today; }
}

void RoutineEngine::LoadDoneState() {
    Settings s(ns_, false);
    std::string csv = s.GetString("done", "");
    size_t start = 0;
    while (start <= csv.size() && !csv.empty()) {
        size_t comma = csv.find(',', start);
        std::string id = csv.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!id.empty())
            for (auto& b : blocks_)
                for (auto& t : b.tasks)
                    if (t.id == id) t.done = true;
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
}

bool RoutineEngine::ReplaceDefinition(const std::string& json) {
    auto backup = blocks_;
    if (!ParseJson(json)) {
        blocks_ = backup;               // JSON ruim nao apaga a rotina existente
        ESP_LOGW(TAG, "JSON invalido -- definicao anterior mantida");
        return false;
    }
    SaveDefinition();
    LoadDoneState();                    // reaplica o "feito hoje" nos ids que sobreviveram
    ESP_LOGI(TAG, "Rotina atualizada pela pagina web");
    return true;
}

bool RoutineEngine::MarkDone(const std::string& task_id) {
    for (auto& b : blocks_) {
        for (auto& t : b.tasks) {
            if (t.id != task_id) continue;
            if (t.done) return true;    // ja feita: nao soma pontos de novo
            t.done = true;
            SaveDoneState();
            if (on_task_done_) on_task_done_(t);
            return true;
        }
    }
    return false;
}

void RoutineEngine::ResetToday() {
    for (auto& b : blocks_)
        for (auto& t : b.tasks) t.done = false;
    SaveDoneState();
    ESP_LOGI(TAG, "Tarefas do dia resetadas");
}

std::string RoutineEngine::TodayJson() const {
    int wd = day_utils::Weekday();
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "weekday", wd);
    cJSON* blocks = cJSON_AddArrayToObject(root, "blocks");

    for (const auto& b : blocks_) {
        cJSON* jt = cJSON_CreateArray();
        for (const auto& t : b.tasks) {
            // wd < 0: relogio sem sincronia -- mostra tudo. Melhor do que
            // esconder a rotina inteira por causa do relogio.
            if (wd >= 0 && !(t.day_mask & (1 << wd))) continue;
            cJSON* o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "id", t.id.c_str());
            cJSON_AddStringToObject(o, "label", t.label.c_str());
            cJSON_AddBoolToObject(o, "done", t.done);
            cJSON_AddItemToArray(jt, o);
        }
        if (cJSON_GetArraySize(jt) == 0) { cJSON_Delete(jt); continue; }
        cJSON* jb = cJSON_CreateObject();
        cJSON_AddStringToObject(jb, "name", b.name.c_str());
        cJSON_AddItemToObject(jb, "tasks", jt);
        cJSON_AddItemToArray(blocks, jb);
    }

    cJSON_AddNumberToObject(root, "done", CountDoneToday());
    cJSON_AddNumberToObject(root, "total", CountTotalToday());

    char* s = cJSON_PrintUnformatted(root);
    std::string out(s);
    cJSON_free(s);
    cJSON_Delete(root);
    return out;
}

std::string RoutineEngine::DefinitionJson() const { return Serialize(); }

int RoutineEngine::CountDoneToday() const {
    int wd = day_utils::Weekday(), n = 0;
    for (const auto& b : blocks_)
        for (const auto& t : b.tasks)
            if ((wd < 0 || (t.day_mask & (1 << wd))) && t.done) n++;
    return n;
}

int RoutineEngine::CountTotalToday() const {
    int wd = day_utils::Weekday(), n = 0;
    for (const auto& b : blocks_)
        for (const auto& t : b.tasks)
            if (wd < 0 || (t.day_mask & (1 << wd))) n++;
    return n;
}
