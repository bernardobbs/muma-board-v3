# Como ligar no arquivo do board

**Aplicado.** `sp-esp32-s3-1.54-muma.cc` (raiz deste repo) já é o
arquivo completo do board com as features de família integradas --
copie ele por cima do arquivo homônimo em
`main/boards/spotpear/sp-esp32-s3-1.54-muma/sp-esp32-s3-1.54-muma.cc`
no clone do `78/xiaozhi-esp32`.

Este documento passa a registrar **onde cada peça entra** (útil se um
merge upstream do fork conflitar com essas mudanças):

- Os `#include` das features de família (`child_profile.h`,
  `device_config.h`, `pomodoro_tool.h`, `tamagotchi_tool.h`,
  `routine_engine.h`, `semaphore_tool.h`, `mcp_tools.h`,
  `config_server.h`) foram adicionados no topo, junto dos includes
  originais do board.
- `InitializeFamilyFeatures()` é um método privado novo da classe
  `Spotpear_esp32_s3_lcd_1_54`, chamado no construtor **depois** de
  `InitializeButtons()`. Carrega `ChildProfile`/`DeviceConfig`,
  inicializa os 3 engines (pomodoro, bichinho, rotina), liga os
  callbacks de brilho/volume/humor e registra as tools MCP.
- O `ConfigServer::Start(...)` **não** entra no construtor -- ele
  precisa de rede. Vai dentro de um `SetNetworkEventCallback` também
  registrado no construtor, que só chama `Start()` quando o callback
  disparar com `NetworkEvent::Connected`. Confirmado lendo
  `main/boards/common/wifi_board.h`/`.cc` no repo base: é esse o
  mecanismo real de "depois que o Wi-Fi conectar" (não existia uma
  chamada solta pra colar fora do construtor como a v3 anterior sugeria
  -- é um callback assíncrono).
- Humor do bichinho -> emoji na tela usa `Tamagotchi::MoodName()`
  direto, sem `pet_emoji_collection.cc`: as chaves já são o vocabulário
  padrão do xiaozhi (`"neutral"`, `"happy"`, `"thinking"`,
  `"surprised"`, `"funny"`), reconhecido pelo pacote de emoji padrão
  (`noto-color-emoji_32`, já escolhido pra essa placa). Sem GIF
  customizado necessário.
- Volume também é aplicado de verdade agora: `GetAudioCodec()` nesta
  placa devolve sempre a mesma instância `static`, então
  `SetOutputVolume(v)` funciona igual ao `SetBrightness` do backlight
  (confirmado em `main/audio/audio_codec.h`).
- **Ainda pendente** (fora do escopo desta rodada): o cronômetro na
  tela via `PomodoroEngine::SetOnTick` continua comentado -- falta um
  método tipo `UpdatePomodoroLabel(s)` nesta classe.

## Senha de admin

`FAMILY_ADMIN_PASSWORD` no topo do `.cc` ainda é o placeholder
`"SENHA-QUE-VOCES-ESCOLHEREM"`. Troquem antes de flashear -- sem senha
real, `ConfigServer::RequireAdmin` bloqueia `/admin` com 403 pra
sempre (proposital, é a proteção contra rodar sem senha).

## Primeira vez ligando

Antes de alguém abrir `/admin` e preencher nome/data de nascimento, o
aparelho funciona com uma faixa etária neutra (10-12 anos) como
default -- não trava nem erra, só usa valores genéricos até vocês
configurarem. A saudação pelo nome na página dela (`/`) só aparece
depois que o nome for salvo.
