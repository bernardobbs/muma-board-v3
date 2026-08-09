#include "mcp_tools.h"
#include "mcp_server.h"
#include "device_config.h"
#include "child_profile.h"
#include "pomodoro_tool.h"
#include "tamagotchi_tool.h"
#include "routine_engine.h"
#include "semaphore_tool.h"

#include <esp_log.h>

#define TAG "McpTools"

namespace mcp_tools {

static void WireEngines() {
    auto& tama = Tamagotchi::GetInstance();
    auto& pomo = PomodoroEngine::GetInstance();
    auto& routine = RoutineEngine::GetInstance();

    // Pomodoro alimenta o bichinho e o humor/GIF
    pomo.SetOnPhaseChanged([&tama](PomodoroState state) {
        switch (state) {
            case PomodoroState::STUDY:   tama.SetMood(TamaMood::FOCADO); break;
            case PomodoroState::WARNING: tama.SetMood(TamaMood::AVISO); break;
            case PomodoroState::BREAK:   tama.OnPomodoroCompleted(); break;   // +1 ponto
            case PomodoroState::IDLE:    tama.SetMood(TamaMood::NEUTRO); break;
            case PomodoroState::PAUSED:  break;
        }
    });

    // Tarefa concluida vira ponto
    routine.SetOnTaskDone([&tama](const RoutineTask& t) {
        tama.AddCustomPoints(t.points, t.label);
    });

    // Duracoes mudaram na pagina web -> recarrega no motor
    DeviceConfig::GetInstance().SetOnPomodoroRulesChanged([&pomo]() { pomo.ReloadRules(); });
}

void RegisterAll() {
    WireEngines();

    auto& mcp = McpServer::GetInstance();
    auto& pomo = PomodoroEngine::GetInstance();
    auto& tama = Tamagotchi::GetInstance();
    auto& routine = RoutineEngine::GetInstance();
    auto& sem = OverloadSemaphore::GetInstance();

    // ---------------- pomodoro ----------------
    mcp.AddTool("self.pomodoro.start", "Inicia um ciclo de pomodoro (foco + pausa)",
        PropertyList(), [&pomo](const PropertyList&) -> ReturnValue { pomo.Start(); return true; });

    mcp.AddTool("self.pomodoro.pause", "Pausa o pomodoro em andamento",
        PropertyList(), [&pomo](const PropertyList&) -> ReturnValue { pomo.Pause(); return true; });

    mcp.AddTool("self.pomodoro.resume", "Retoma um pomodoro pausado",
        PropertyList(), [&pomo](const PropertyList&) -> ReturnValue { pomo.Resume(); return true; });

    mcp.AddTool("self.pomodoro.stop", "Interrompe o pomodoro atual, sem penalidade",
        PropertyList(), [&pomo](const PropertyList&) -> ReturnValue { pomo.Stop(); return true; });

    mcp.AddTool("self.pomodoro.status", "Consulta quanto falta no pomodoro atual",
        PropertyList(), [&pomo](const PropertyList&) -> ReturnValue { return pomo.StatusJson(); });

    // ---------------- bichinho ----------------
    mcp.AddTool("self.pet.status", "Mostra o estado do bichinho: nome, estagio, humor e pontos",
        PropertyList(), [&tama](const PropertyList&) -> ReturnValue { return tama.StatusJson(); });

    mcp.AddTool("self.pet.choose", "Escolhe qual bichinho sera o companheiro",
        PropertyList({ Property("id", kPropertyTypeString) }),
        [&tama](const PropertyList& p) -> ReturnValue {
            auto id = p["id"].value<std::string>();
            if (!tama.ChooseSpecies(id)) return std::string("Nao achei esse bichinho no catalogo.");
            return std::string("Agora seu companheiro e o " + tama.pet_name() + "!");
        });

    // ---------------- rotina ----------------
    mcp.AddTool("self.routine.today", "Lista as tarefas da rotina de hoje",
        PropertyList(), [&routine](const PropertyList&) -> ReturnValue { return routine.TodayJson(); });

    mcp.AddTool("self.routine.mark_done", "Marca uma tarefa da rotina como concluida",
        PropertyList({ Property("id", kPropertyTypeString) }),
        [&routine](const PropertyList& p) -> ReturnValue {
            auto id = p["id"].value<std::string>();
            if (!routine.MarkDone(id)) return std::string("Nao encontrei essa tarefa.");
            return std::string("Marquei como feita!");
        });

    // ---------------- semaforo ----------------
    // Independente de idade -- e sobre necessidade especifica (regulacao
    // sensorial/emocional), nao sobre quantos anos a crianca tem. Liga/
    // desliga em /admin, decisao dos responsaveis, nao do firmware.
    if (ChildProfile::GetInstance().regulation_tools_enabled()) {
        mcp.AddTool("self.semaphore.set_level",
            "Registra o nivel de sobrecarga que ela mesma sinalizou: verde, amarelo ou vermelho",
            PropertyList({ Property("nivel", kPropertyTypeString) }),
            [&sem](const PropertyList& p) -> ReturnValue {
                auto v = p["nivel"].value<std::string>();
                if (v == "verde") sem.SetLevel(OverloadLevel::VERDE);
                else if (v == "amarelo") sem.SetLevel(OverloadLevel::AMARELO);
                else if (v == "vermelho") sem.SetLevel(OverloadLevel::VERMELHO);
                else return std::string("Nivel invalido. Use verde, amarelo ou vermelho.");
                return std::string("Anotado: " + sem.LevelName());
            });

        mcp.AddTool("self.semaphore.confirm", "Confirma o envio do aviso aos responsaveis",
            PropertyList(), [&sem](const PropertyList&) -> ReturnValue {
                sem.ConfirmSend(); return true; });

        mcp.AddTool("self.semaphore.cancel", "Cancela o envio do aviso aos responsaveis",
            PropertyList(), [&sem](const PropertyList&) -> ReturnValue {
                sem.CancelSend(); return true; });
    }

    ESP_LOGI(TAG, "Ferramentas MCP registradas para %s",
             ChildProfile::GetInstance().name().c_str());
}

}
