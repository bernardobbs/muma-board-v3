# Como ligar no arquivo do board

Trecho pra colar em `sp-esp32-s3-1.54-muma.cc`.

```cpp
#include "child_profile.h"
#include "device_config.h"
#include "pomodoro_tool.h"
#include "tamagotchi_tool.h"
#include "routine_engine.h"
#include "semaphore_tool.h"
#include "mcp_tools.h"
#include "config_server.h"

// --- no construtor, depois de InitializeButtons() ---
void Spotpear_esp32_s3_lcd_1_54::InitializeFamilyFeatures() {
    ChildProfile::GetInstance().Load();   // nome, nascimento, toggle de regulacao
    auto& cfg = DeviceConfig::GetInstance();
    cfg.Load();                       // le regras (com defaults por faixa etaria) e aplica o fuso

    // Callbacks que faltavam: brilho/volume eram salvos mas nunca aplicados
    cfg.SetOnBrightnessChanged([this](int v) { GetBacklight()->SetBrightness(v); });
    cfg.SetOnVolumeChanged([this](int v) { GetAudioCodec()->SetOutputVolume(v); });

    PomodoroEngine::GetInstance().Initialize();
    Tamagotchi::GetInstance().Initialize();
    RoutineEngine::GetInstance().Initialize();

    // Humor do bichinho -> GIF na tela
    Tamagotchi::GetInstance().SetOnMoodChanged([this](const std::string& mood) {
        GetDisplay()->SetEmotion(mood.c_str());
    });

    // Trocou de especie -> recarrega a colecao de GIFs
    // Tamagotchi::GetInstance().SetOnSpeciesChanged([](const std::string& id) {
    //     auto theme = LvglThemeManager::GetInstance().GetTheme("dark");
    //     theme->set_emoji_collection(CreatePetEmojiCollection(id.c_str()));
    // });

    // Cronometro na tela (parte 2 combinada): usar o OnTick
    // PomodoroEngine::GetInstance().SetOnTick([this](int s) { UpdatePomodoroLabel(s); });

    mcp_tools::RegisterAll();

    // Aplica o brilho/volume salvos ja na partida
    GetBacklight()->SetBrightness(cfg.brightness());
}

// --- APOS o Wi-Fi conectar (nao no construtor) ---
ConfigServer::GetInstance().Start("SENHA-QUE-VOCES-ESCOLHEREM");
```

O `ConfigServer` precisa de rede, entao chame o `Start()` a partir do
callback de conexao do Wi-Fi, nao do construtor do board.

## Primeira vez ligando

Antes de alguém abrir `/admin` e preencher nome/data de nascimento, o
aparelho funciona com uma faixa etária neutra (10-12 anos) como
default -- não trava nem erra, só usa valores genéricos até vocês
configurarem. A saudação pelo nome na página dela (`/`) só aparece
depois que o nome for salvo.
