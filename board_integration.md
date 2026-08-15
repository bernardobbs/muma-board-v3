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
  precisa de rede. **Correção feita depois de testar em hardware real**:
  a primeira versão usava `SetNetworkEventCallback` (registrado no
  construtor, disparando em `NetworkEvent::Connected`) -- só que
  `Application::Initialize()` (`main/application.cc`) **também** chama
  `board.SetNetworkEventCallback(...)`, depois do construtor do board
  já ter rodado, pra atualizar notificações de UI ("Conectando a
  Wi-Fi..."). Como é um único `std::function` (não uma lista), essa
  chamada **sobrescreve silenciosamente** a nossa -- sem erro, sem log,
  só nunca mais dispara. Isso derrubava o `ConfigServer` inteiro
  (`/` e `/admin` davam "connection refused") sem nenhuma pista no log
  de boot. Trocado por um handler nativo do ESP-IDF
  (`esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ...)`),
  que aceita múltiplos handlers independentes e não conflita com nada
  que a `Application` já faz.
- Humor do bichinho -> emoji na tela usa `Tamagotchi::MoodName()`
  direto. As chaves já são o vocabulário padrão do xiaozhi
  (`"neutral"`, `"happy"`, `"thinking"`, `"surprised"`, `"funny"`),
  reconhecido pelo pacote de emoji padrão que já vem no aparelho
  (confirmado num `assets.bin` real: 21 emoções, PNG estático 32x32,
  todas as 5 que usamos já existem nele). `ApplyPetEmojiCollection`
  troca pra arte customizada por espécie **se** houver GIFs em
  `pet_gifs/` (ver `pet_emoji_collection.h`/`.cc`) -- sem GIFs, o
  pacote padrão continua valendo, nunca trava por falta de arte.
- Volume também é aplicado de verdade agora: `GetAudioCodec()` nesta
  placa devolve sempre a mesma instância `static`, então
  `SetOutputVolume(v)` funciona igual ao `SetBrightness` do backlight
  (confirmado em `main/audio/audio_codec.h`).
- **Cronômetro na tela**: `UpdatePomodoroLabel(s)` cria um
  `lv_label_create` sob demanda (primeiro tick) e mostra `MM:SS` no
  canto superior direito da tela (`LV_ALIGN_TOP_RIGHT`, livre -- os
  widgets padrão do `LcdDisplay` ficam em TOP_MID/CENTER/BOTTOM_MID).
  Criado sob demanda, não no construtor, porque `SetupUI()` do display
  só roda depois do construtor do board (confirmado em
  `Application::Initialize()`, que chama os dois em sequência) --
  `lv_screen_active()` ainda não existiria antes disso. Toda escrita
  na UI passa por `DisplayLockGuard` (classe pública em
  `display/display.h`, dá pra usar de fora mesmo com `Lock`/`Unlock`
  sendo `protected` no `Display`).
- **Selo de estágio**: `UpdateStageBadge()` mostra `Tamagotchi::StageName()`
  ("Ovo"/"Filhote"/"Jovem"/"Forte") como texto puro no canto inferior
  direito (`LV_ALIGN_BOTTOM_RIGHT`, também livre). Texto, não emoji --
  a fonte de texto do board (`font_noto_sans_basic`) é charset básico,
  sem glifo de emoji; emoji só existe como imagem raster no
  `EmojiCollection`. Ligado em `Tamagotchi::SetOnEvolved` (dispara só
  quando o estágio SOBE) + uma chamada inicial dentro do
  `NetworkEventCallback` (mesma razão do label do cronômetro: precisa
  que `SetupUI()` já tenha rodado, e é a única garantia assíncrona que
  temos disso além do próprio tick do pomodoro).

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
