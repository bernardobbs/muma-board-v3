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
  precisa de rede. **Duas tentativas testadas em hardware real e
  descartadas antes da que ficou**:
  1. `SetNetworkEventCallback` (registrado no construtor, disparando em
     `NetworkEvent::Connected`) -- `Application::Initialize()`
     (`main/application.cc`) **também** chama
     `board.SetNetworkEventCallback(...)`, depois do construtor do board
     já ter rodado, pra atualizar notificações de UI ("Conectando a
     Wi-Fi..."). Como é um único `std::function` (não uma lista), essa
     chamada **sobrescreve silenciosamente** a nossa -- sem erro, sem
     log, só nunca mais dispara. Confirmado no log de boot real:
     `ConfigServer` nunca subia (`/` e `/admin` davam "connection
     refused"), sem nenhuma pista de erro.
  2. `esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ...)`
     -- a suposição era que um evento nativo do ESP-IDF, que aceita
     múltiplos handlers independentes, não teria o mesmo conflito.
     Também testado em hardware real, com `esp_netif_init()`/
     `esp_event_loop_create_default()` chamados manualmente antes do
     registro (achávamos que o event loop padrão só existia depois do
     construtor do board, pela ordem dos logs). O registro não dava
     erro nenhum, mas o handler **também nunca disparava** -- log de 30+
     segundos de boot sem nenhuma linha do `ConfigServer` e sem erro.
     Causa exata não confirmada (provavelmente alguma sutileza de
     timing do event loop que não dá pra ver só lendo log).
  3. **Solução atual: polling. Confirmada funcionando em hardware
     real.** Abandona qualquer callback/evento assíncrono. Um
     `esp_timer` periódico (1s), criado no construtor, chama
     `CheckNetworkReady()` que checa
     `Application::GetInstance().GetDeviceState() == kDeviceStateIdle`
     -- estado que a própria `Application` já expõe e que só é atingido
     depois que o Wi-Fi conecta **e** a ativação com o backend (OTA +
     MQTT) termina (confirmado no log: `StateMachine: State: activating
     -> idle`). Quando fica `true`, sobe o `ConfigServer` e o timer se
     autodesliga (`esp_timer_stop`). Sem depender de nenhum mecanismo de
     evento do ESP-IDF, então não tem como ter o mesmo tipo de bug de
     timing/sobrescrita das duas tentativas anteriores. Log real de
     confirmação:
     ```
     I (16740) StateMachine: State: activating -> idle
     I (17510) Spotpear_esp32_s3_lcd_1_54: CheckNetworkReady: estado idle detectado, subindo ConfigServer
     I (17510) ConfigServer: Servidor de configuracao no ar (porta 80)
     ```
     A causa raiz de por que as duas tentativas anteriores pareciam
     "não fazer nada" mesmo depois de corrigidas: o `git pull` no
     dispositivo de teste não estava de fato trazendo os commits novos
     (branch local ficava atrás sem aviso claro, bloqueada pela edição
     local de `FAMILY_ADMIN_PASSWORD` não commitada) -- o binário
     testado continuava sendo uma versão anterior. Lição: depois de
     qualquer `git pull` em cima de mudança local, confirme com `git
     log --oneline -1` antes de compilar.
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
- **Cronômetro na tela (revisado)**: `UpdatePomodoroLabel(s)` cria, sob
  demanda no primeiro tick, uma imagem (`pomodoro_tomato_`, PNG real de
  `pomodoro_tomato.png` via `LvglRawImage`) + um label (`pomodoro_label_`,
  texto "MM:SS" aumentado 2.5x via `lv_obj_set_style_transform_scale_x/y`),
  os dois centralizados (`LV_ALIGN_CENTER`). Como o rosto de humor do
  bichinho também usa CENTER, tomate+relógio ficam com
  `LV_OBJ_FLAG_HIDDEN` fora do pomodoro, e SOBREPÕEM o rosto só
  enquanto o cronômetro roda (versão original mostrava um label pequeno
  em `LV_ALIGN_TOP_RIGHT`; trocado por feedback: "ficou pequeno" e "o
  tomate não ficou legal" -- formas LVGL puras não convenceram, PNG
  real sim). Criado sob demanda, não no construtor, porque `SetupUI()`
  do display só roda depois do construtor do board (confirmado em
  `Application::Initialize()`, que chama os dois em sequência) --
  `lv_screen_active()` ainda não existiria antes disso. Toda escrita na
  UI passa por `DisplayLockGuard` (classe pública em `display/display.h`,
  dá pra usar de fora mesmo com `Lock`/`Unlock` sendo `protected` no
  `Display`).
- **Alarmes (múltiplos, controlados pela própria criança)**: `AlarmEngine`
  guarda uma lista (`std::vector<AlarmEntry>`, JSON no NVS, mesmo padrão
  do `RoutineEngine`) em vez de um único horário -- decisão de design:
  isso é sobre COMO a criança usa o aparelho, não uma regra dos
  responsáveis, então mora em `/` (sem senha, endpoints `/api/alarms*`)
  e não em `/admin`. Um `esp_timer` (5s) compara cada alarme habilitado
  contra o RTC local; ao bater horário+minuto E ainda não ter disparado
  nesse dia (`last_fired_yday`, por alarme, não persistido), mostra
  `alarm_banner_` (tela cheia laranja) e toca `Lang::Sounds::OGG_EXCLAMATION`,
  repetindo o som a cada 3s até `Dismiss()` (pelo `boot_button_`, por
  voz via `self.alarm.dismiss`, ou sozinho depois de 60s). Se dois
  alarmes baterem no mesmo minuto, o segundo dispara só depois que o
  primeiro for desligado (checagem de `firing_` no topo de `Check()`).
- **QR code pra achar a página sem digitar IP**: segurar o `boot_button_`
  (`OnLongPress`) mostra/esconde um QR code de tela cheia com
  `http://<ip>/`. O IP vem direto de `esp_netif_get_ip_info` (netif
  `"WIFI_STA_DEF"`), sem depender de nenhum getter do componente de
  wifi. **Desenhado na mão** com `mumaqr_encodeText`/`mumaqr_getModule`
  (`mumaqr.h`/`.cc`, cópia renomeada do gerador de QR já usado pela
  LVGL) + `lv_canvas` -- NÃO usa o widget `lv_qrcode` da própria LVGL.
  Motivo, confirmado em build real: ligar `CONFIG_LV_USE_QRCODE=y` dá
  "multiple definition of qrcodegen_makeEci" contra o `qrcodegen.c` que
  o componente `espressif2022__esp_emote_gfx` (Expression Emote,
  já presente no projeto base) embute por conta própria -- duas cópias
  do mesmo gerador terceirizado colidindo no link. Renomear todos os
  identificadores pra `mumaqr_*` evita esse conflito de vez, sem
  depender de nenhuma opção de Kconfig. Ver `README.md` seção 7.
- **Selo de estágio**: `UpdateStageBadge()` mostra `Tamagotchi::StageName()`
  ("Ovo"/"Filhote"/"Jovem"/"Forte") como texto puro no canto inferior
  direito (`LV_ALIGN_BOTTOM_RIGHT`, também livre). Texto, não emoji --
  a fonte de texto do board (`font_noto_sans_basic`) é charset básico,
  sem glifo de emoji; emoji só existe como imagem raster no
  `EmojiCollection`. Ligado em `Tamagotchi::SetOnEvolved` (dispara só
  quando o estágio SOBE) + uma chamada inicial dentro do
  `CheckNetworkReady()` (mesma razão do label do cronômetro: precisa
  que `SetupUI()` já tenha rodado, e é a única garantia assíncrona que
  temos disso além do próprio tick do pomodoro).
- **Pomodoro em ciclos contínuos, aviso tipo sineta de escola**:
  `PomodoroEngine::Tick()` não vai mais pra `IDLE` quando a pausa
  termina sozinha -- encadeia direto num foco novo
  (`EnterPhase(STUDY, ...)`), sem esperar a criança mandar "começa" de
  novo. Só para de verdade com `Stop()` (por voz ou toque). Fim do foco
  toca UMA batida de `OGG_POPUP` (começou a pausa); fim da pausa toca
  DUAS batidas seguidas do mesmo som (recomeçou o foco) -- convenção
  simples de "uma vez = pausa, duas vezes = volta a focar", sem precisar
  de nenhum áudio novo (`AudioService::PushPacketToDecodeQueue` é fila
  FIFO de verdade, então duas chamadas de `PlaySound()` em sequência
  tocam as duas batidas uma atrás da outra em vez de cortar uma na
  outra). Chegou a existir uma versão com anúncio por voz (TTS,
  `OGG_POMODORO_FOCUS_DONE`/`OGG_POMODORO_BREAK_DONE` gerados fora do
  firmware) -- revertida por decisão de manter simples, sem depender de
  gerar/instalar áudio novo.
- **Semáforo de sobrecarga ajustável pela página dela também**: antes só
  dava pra mudar de nível por voz (`self.semaphore.*` em `mcp_tools.cc`)
  -- `/` era só leitura (mostrava as regras de aviso aos pais, nunca o
  nível nem um jeito de mudar). Agora `www/index.html` tem um card
  "Semáforo" com os 3 botões (verde/amarelo/vermelho) e a confirmação de
  envio quando pendente, batendo com os 4 endpoints novos em
  `config_server.cc`: `GET /api/semaphore` (status, sempre liberado --
  só leitura) e `POST /api/semaphore/set|confirm|cancel` (mutam,
  **gated por `ChildProfile::regulation_tools_enabled()`**, checado a
  cada chamada -- mesmo padrão das tools de voz, pra ligar/desligar em
  `/admin` valer na hora sem reiniciar). O card inteiro fica
  `display:none` até `/api/config` confirmar `regulacao_ativa: true`
  (mesmo campo que já existia, exposto em `DeviceConfig::ToJson()`) --
  sem senha nenhuma aqui, é sobre COMO ela usa, não uma regra dos
  responsáveis (mesma lógica dos alarmes). Poll a cada 3s
  (`loadSemaphore`) só liga quando o card aparece, pra pegar uma
  confirmação pendente que tenha surgido por voz enquanto ela está com
  a página aberta. Editar `www/index.html` sempre exige rodar
  `python3 scripts/gen_web_assets.py` depois -- é ele que gera
  `web_assets.cc` (o HTML vira string C++ embutida, ver comentário no
  próprio arquivo pra entender por quê).

  Efeito colateral aceito: o bônus de "voltar rápido da pausa"
  (`SetOnQuickReturn`, `DeviceConfig::quick_return_minutes`) ficou
  dormente -- ele recompensava a criança por reiniciar o foco sozinha
  logo depois de uma pausa cumprida, mas agora isso acontece sozinho,
  sempre, então não sobrou nenhum "voltar" pra recompensar. O código
  (e o campo em `/admin`) não foi removido, só parou de disparar --
  ver comentário em `pomodoro_tool.h`.

## Arquivo `.cc`/`.h` novo -- precisa de `idf.py reconfigure`

Achado testando em hardware real: o `file(GLOB boards/${BOARD_DIR}/*.cc)`
do `main/CMakeLists.txt` só é avaliado quando o CMake reconfigura, não a
cada `idf.py build`. Editar um arquivo que já existe funciona com build
normal; mas ao adicionar um `.cc` novo na pasta do board (ex:
`alarm_tool.cc`, `pomodoro_tomato.cc`), um `idf.py build` direto falha
no LINK com "undefined reference" pras funções desse arquivo -- o
ninja nem sabe que o arquivo existe, porque a lista de fontes ficou
cacheada de antes dele existir. Rode `idf.py reconfigure` (mais rápido
que `fullclean`, só refaz o passo de configuração) antes do `build`
sempre que um arquivo novo for adicionado.

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
