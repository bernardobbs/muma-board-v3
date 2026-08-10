#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

#define DAY_ALL 127   // bit0=Domingo ... bit6=Sabado

struct RoutineTask {
    std::string id;
    std::string label;
    uint8_t day_mask = DAY_ALL;
    int points = 1;
    bool done = false;   // estado do dia, separado da definicao
};

struct RoutineBlock {
    std::string name;
    std::vector<RoutineTask> tasks;
};

// Rotina e DADO, nao codigo: JSON no NVS, editavel pela pagina /admin.
// Mesmo motor serve qualquer crianca -- muda so o namespace e o rascunho.
class RoutineEngine {
public:
    static RoutineEngine& GetInstance() { static RoutineEngine i; return i; }

    void Initialize();                    // le perfil do DeviceConfig
    bool MarkDone(const std::string& task_id);
    void ResetToday();

    std::string TodayJson() const;
    std::string DefinitionJson() const;
    bool ReplaceDefinition(const std::string& json);

    int CountDoneToday() const;
    int CountTotalToday() const;

    void SetOnTaskDone(std::function<void(const RoutineTask&)> cb) { on_task_done_ = cb; }

private:
    RoutineEngine() = default;
    bool ParseJson(const std::string& json);
    std::string Serialize() const;
    void SaveDefinition();
    void SaveDoneState();
    void LoadDoneState();

    std::string ns_;
    std::vector<RoutineBlock> blocks_;
    int32_t stored_day_ = 0;
    std::function<void(const RoutineTask&)> on_task_done_;
};
