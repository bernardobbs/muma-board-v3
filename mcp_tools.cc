#include "mcp_tools.h"
#include "mcp_server.h"
#include "device_config.h"
#include "child_profile.h"
#include "pomodoro_tool.h"
#include "tamagotchi_tool.h"
#include "routine_engine.h"
#include "semaphore_tool.h"
#include "alarm_tool.h"
#include "breathing_tool.h"
#include "game_tool.h"
#include "application.h"
#include "assets/lang_config.h"

#include <cJSON.h>
#include <esp_log.h>
#include <cstdio>

#define TAG "McpTools"

namespace mcp_tools {

// Notificacao MCP fire-and-forget de evento do dispositivo (sem "id",
// nunca vira mensagem falsa do usuario no servidor -- ver
// Application::SendMcpNotification). Usada pelos eventos do pomodoro
// abaixo, todos com o mesmo formato: {"jsonrpc":"2.0","method":method,
// "params":{key:value}}.
static void NotifyIntEvent(const char* method, const char* key, int value) {
    cJSON* params = cJSON_CreateObject();
    cJSON_AddNumberToObject(params, key, value);
    Application::GetInstance().SendMcpNotification(method, params);
    cJSON_Delete(params);
}

static void WireEngines() {
    auto& tama = Tamagotchi::GetInstance();
    auto& pomo = PomodoroEngine::GetInstance();
    auto& routine = RoutineEngine::GetInstance();

    // Pomodoro alimenta o bichinho e o humor/GIF. Cada troca de fase toca
    // um som -- sem isso, a unica pista de que o foco acabou e a pausa
    // comecou era o numero no relogio continuar contando (feedback real:
    // "nao avisou que passou pra pausa"). O texto "Foco"/"Pausa" junto do
    // relogio (ver UpdatePomodoroLabel) reforca visualmente.
    //
    // Convencao tipo sineta de escola: UMA batida pra pausa, DUAS pra
    // recomecar o foco. Trocado de OGG_POPUP (bipe suave) pra
    // OGG_OLD_ALARM (bipe estilo despertador antigo, "pi pi pi pi pi")
    // -- pedido pra ser bem mais irritante no fim de cada fase, pra nao
    // dar pra ignorar. PlaySound() enfileira (AudioService::
    // PushPacketToDecodeQueue e uma fila FIFO de verdade, nao substitui
    // o que ja esta tocando), entao chamar duas vezes seguidas toca as
    // duas batidas em sequencia, sem cortar uma na outra.
    // Notificacoes MCP pro servidor/IA reagirem por voz (pedido na
    // especificacao "NUMA v3" -- ver ATUALIZACAO_NUMA.md). Decisao
    // explicita: aviso previo SO antes do fim da PAUSA
    // (pomodoro.break_warning) -- sem aviso antes do fim do foco.
    pomo.SetOnPhaseChanged([&tama](PomodoroState state) {
        switch (state) {
            case PomodoroState::STUDY:   tama.SetMood(TamaMood::FOCADO); break;
            case PomodoroState::BREAK:
                tama.OnPomodoroCompleted();   // +1 ponto por completar o foco
                Application::GetInstance().PlaySound(Lang::Sounds::OGG_OLD_ALARM);  // 1 bipe: comecou a pausa
                NotifyIntEvent("pomodoro.completed", "duration_minutes",
                               DeviceConfig::GetInstance().study_minutes());
                break;
            case PomodoroState::BREAK_WARNING:
                tama.SetMood(TamaMood::AVISO);
                NotifyIntEvent("pomodoro.break_warning", "remaining_seconds",
                               DeviceConfig::GetInstance().warning_seconds());
                break;
            case PomodoroState::IDLE:
                tama.SetMood(TamaMood::NEUTRO);
                break;
            case PomodoroState::PAUSED:  break;
        }
    });

    // +1 ponto extra por CUMPRIR a pausa (nao pular direto pra outro
    // foco) -- so dispara quando a pausa termina sozinha, nunca com Stop().
    // Som aqui (nao no PomodoroState::IDLE generico) porque Stop() TAMBEM
    // levava pra IDLE -- nao queremos tocar "acabou a pausa" quando a
    // crianca so cancelou o ciclo. O novo foco que comeca em seguida
    // (PomodoroEngine::Tick()) e quem encadeia o proximo ciclo sozinho --
    // por isso a IA nao deve convidar a criança a "iniciar o proximo
    // ciclo" ao ouvir esse evento (diferente do que a doc original
    // sugeria pra Knowledge Base): ele ja comecou sozinho.
    pomo.SetOnBreakCompleted([&tama]() {
        tama.OnBreakRespected();
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_OLD_ALARM);  // 2 bipes: recomecou o foco
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_OLD_ALARM);
        NotifyIntEvent("pomodoro.break_completed", "break_duration_minutes",
                       DeviceConfig::GetInstance().break_minutes());
    });

    // Bonus extra por voltar ao foco dentro da janela configurada em
    // /admin (DeviceConfig::quick_return_minutes) -- incentivo pra nao
    // deixar o tempo passar depois da pausa.
    pomo.SetOnQuickReturn([&tama]() {
        tama.AddCustomPoints(1, "Voltou ao foco rapido");
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
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
    auto& cfg = DeviceConfig::GetInstance();

    // ---------------- pomodoro ----------------
    // set_duration faltava -- sem ela, pedir por voz pra mudar o tempo
    // do pomodoro nao tinha NENHUMA tool pra fazer isso de verdade (so
    // start/pause/resume/stop/status, todas sem parametro). A IA so
    // conversava sobre mudar, sem efeito algum -- por isso a
    // configuracao feita na pagina sempre "vencia": era a UNICA que
    // realmente escrevia em algum lugar.
    mcp.AddTool("self.pomodoro.set_duration",
        "Define a duracao do foco e da pausa do pomodoro, em minutos (dentro da faixa seguranca)",
        PropertyList({
            Property("study_min", kPropertyTypeInteger, DeviceConfig::kMinStudyMin, DeviceConfig::kMaxStudyMin),
            Property("break_min", kPropertyTypeInteger, DeviceConfig::kMinBreakMin, DeviceConfig::kMaxBreakMin),
        }),
        [&cfg](const PropertyList& p) -> ReturnValue {
            int study = p["study_min"].value<int>();
            int brk = p["break_min"].value<int>();
            cfg.set_pomodoro(study, brk);
            char buf[64];
            snprintf(buf, sizeof(buf), "Pomodoro ajustado: %d min de foco, %d min de pausa.",
                     cfg.study_minutes(), cfg.break_minutes());
            return std::string(buf);
        });

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
    //
    // As tools ficam SEMPRE registradas; o toggle e checado a cada
    // chamada (nao so uma vez no boot), pra que ligar/desligar em /admin
    // faca efeito na hora, sem precisar reiniciar o aparelho.
    mcp.AddTool("self.semaphore.set_level",
        "Registra o nivel de sobrecarga que ela mesma sinalizou: verde, amarelo, vermelho ou "
        "recuperacao (voltando ao normal depois de um amarelo/vermelho)",
        PropertyList({ Property("nivel", kPropertyTypeString) }),
        [&sem](const PropertyList& p) -> ReturnValue {
            if (!ChildProfile::GetInstance().regulation_tools_enabled())
                return std::string("O semaforo de sobrecarga esta desligado nas configuracoes.");
            auto v = p["nivel"].value<std::string>();
            if (v == "verde") sem.SetLevel(OverloadLevel::VERDE);
            else if (v == "amarelo") sem.SetLevel(OverloadLevel::AMARELO);
            else if (v == "vermelho") sem.SetLevel(OverloadLevel::VERMELHO);
            else if (v == "recuperacao") sem.SetLevel(OverloadLevel::RECUPERACAO);
            else return std::string("Nivel invalido. Use verde, amarelo, vermelho ou recuperacao.");
            return std::string("Anotado: " + sem.LevelName());
        });

    mcp.AddTool("self.semaphore.confirm", "Confirma o envio do aviso aos responsaveis",
        PropertyList(), [&sem](const PropertyList&) -> ReturnValue {
            if (!ChildProfile::GetInstance().regulation_tools_enabled()) return false;
            sem.ConfirmSend(); return true; });

    mcp.AddTool("self.semaphore.cancel", "Cancela o envio do aviso aos responsaveis",
        PropertyList(), [&sem](const PropertyList&) -> ReturnValue {
            if (!ChildProfile::GetInstance().regulation_tools_enabled()) return false;
            sem.CancelSend(); return true; });

    // ---------------- alarme (varios, controlados pela propria crianca) ----------------
    auto& alarm = AlarmEngine::GetInstance();
    mcp.AddTool("self.alarm.add", "Adiciona um novo alarme diario (horario de 0-23h e 0-59min)",
        PropertyList({
            Property("hour", kPropertyTypeInteger, 0, 23),
            Property("minute", kPropertyTypeInteger, 0, 59),
        }),
        [&alarm](const PropertyList& p) -> ReturnValue {
            int h = p["hour"].value<int>();
            int m = p["minute"].value<int>();
            alarm.AddAlarm(h, m);
            char buf[32];
            snprintf(buf, sizeof(buf), "Alarme marcado para %02d:%02d.", h, m);
            return std::string(buf);
        });

    mcp.AddTool("self.alarm.list", "Lista todos os alarmes cadastrados e se estao ativados",
        PropertyList(), [&alarm](const PropertyList&) -> ReturnValue { return alarm.ListJson(); });

    mcp.AddTool("self.alarm.remove", "Remove um alarme cadastrado, pelo id (ver self.alarm.list)",
        PropertyList({ Property("id", kPropertyTypeString) }),
        [&alarm](const PropertyList& p) -> ReturnValue {
            if (!alarm.RemoveAlarm(p["id"].value<std::string>()))
                return std::string("Nao encontrei um alarme com esse id.");
            return std::string("Alarme removido.");
        });

    mcp.AddTool("self.alarm.dismiss", "Desliga o alarme que esta tocando agora",
        PropertyList(), [&alarm](const PropertyList&) -> ReturnValue { alarm.Dismiss(); return true; });

    // ---------------- cantinho da calma (respiracao guiada) ----------------
    auto& breathing = BreathingExercise::GetInstance();
    mcp.AddTool("self.breathing.start",
        "Inicia um exercicio de respiracao guiada na tela, pra ajudar a se calmar",
        PropertyList(), [&breathing](const PropertyList&) -> ReturnValue { breathing.Start(); return true; });

    mcp.AddTool("self.breathing.stop", "Para o exercicio de respiracao guiada",
        PropertyList(), [&breathing](const PropertyList&) -> ReturnValue { breathing.Stop(); return true; });

    // ---------------- aventuras (Game Engine) ----------------
    // Camada separada do bichinho (Tamagotchi) -- decisao tomada: soma,
    // nao substitui. O firmware guarda o estado real (capitulo,
    // estrelas, personagens, missao); a IA NUNCA deve inventar
    // progresso sozinha -- toda mudanca de estado passa por uma destas
    // tools, e self.game.status sempre devolve o que de fato esta
    // salvo.
    auto& game = GameEngine::GetInstance();
    mcp.AddTool("self.game.start",
        "Comeca uma aventura nova (zera capitulo, estrelas, personagens e missao -- "
        "mesmo se ja tiver uma aventura em andamento)",
        PropertyList({ Property("game_id", kPropertyTypeString) }),
        [&game](const PropertyList& p) -> ReturnValue {
            game.Start(p["game_id"].value<std::string>());
            return game.StatusJson();
        });

    mcp.AddTool("self.game.status", "Consulta o estado real da aventura em andamento",
        PropertyList(), [&game](const PropertyList&) -> ReturnValue { return game.StatusJson(); });

    mcp.AddTool("self.game.choice",
        "Registra a escolha que ela fez na aventura (o firmware so guarda, quem decide "
        "a consequencia narrativa e a IA)",
        PropertyList({ Property("choice", kPropertyTypeString) }),
        [&game](const PropertyList& p) -> ReturnValue {
            game.SetChoice(p["choice"].value<std::string>());
            return game.StatusJson();
        });

    mcp.AddTool("self.game.reward",
        "Da uma recompensa: estrelas (numero a somar, pode ser 0) e/ou um personagem/item "
        "novo (nome livre, ex.: 'coelho', 'chave dourada' -- deixe vazio se nao ganhou nenhum)",
        PropertyList({
            Property("stars", kPropertyTypeInteger, 0, 0, 999),
            Property("item", kPropertyTypeString, std::string("")),
        }),
        [&game](const PropertyList& p) -> ReturnValue {
            game.AddReward(p["stars"].value<int>(), p["item"].value<std::string>());
            return game.StatusJson();
        });

    mcp.AddTool("self.game.level",
        "Avanca pro proximo capitulo da aventura, com a missao nova (deixe vazio pra "
        "manter a missao atual)",
        PropertyList({ Property("mission", kPropertyTypeString, std::string("")) }),
        [&game](const PropertyList& p) -> ReturnValue {
            game.AdvanceChapter(p["mission"].value<std::string>());
            return game.StatusJson();
        });

    mcp.AddTool("self.game.end",
        "Encerra a aventura atual (o progresso fica salvo, so para de estar 'em andamento')",
        PropertyList(), [&game](const PropertyList&) -> ReturnValue { game.End(); return true; });

    ESP_LOGI(TAG, "Ferramentas MCP registradas para %s",
             ChildProfile::GetInstance().name().c_str());
}

}
