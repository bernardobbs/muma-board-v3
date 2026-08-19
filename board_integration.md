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
- **GIFs do bichinho: rosto redondo estilo emoji, fundo corrigido**.
  Gato e Lobo trocaram de "personagem chibi de corpo inteiro" (64px)
  pra rosto redondo estilo emoji (128px), cobrindo as 21 chaves do
  pacote padrão (não só as 5 do Tamagotchi) -- assim a arte customizada
  vale em qualquer humor que a conversa com a IA disparar, não só nos
  momentos do pomodoro. Bug real encontrado testando em hardware: o
  fundo nunca saía transparente de verdade -- o Pillow descarta o canal
  alpha silenciosamente ao converter RGBA→P (obrigatório pro formato
  GIF) sem reservar um índice de cor pra transparência e passar
  `transparency=<índice>` pro `save()`. Corrigido com color-key: compara
  cada pixel contra os 4 cantos da imagem individualmente (não a média
  deles -- fundo com sombra em degrade tem tons diferentes nos cantos, a
  média não bate com nenhum) e apaga o que for parecido. Das 42 imagens
  geradas nessa rodada (2 espécies × 21 humores), 15 saíram com
  composição ruim da própria geração por IA (padrão/ladrilho de vários
  rostinhos em vez de UM rosto, sem relação com o bug de fundo) --
  mantido o GIF antigo de 64px nesses casos até serem regenerados, em
  vez de descartar e cair no genérico.
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
- **Mesmo QR code, agora sensível ao estado -- também serve pra entrar
  no Wi-Fi**. Resolve o item §15.3 da proposta "NUMA v3" (que a própria
  auditoria do documento já tinha corrigido: "O Numa já possui
  infraestrutura própria de QR Code" da v2 não era verdade -- só existia
  o QR de achar a página). `ToggleQrCode()` agora checa
  `Application::GetDeviceState()`: em `kDeviceStateWifiConfiguring`
  (aparelho novo, ou perdeu a rede -- ver `WifiBoard`/`esp-wifi-connect`,
  biblioteca externa, não vendored), mostra um QR no formato padrão de
  Wi-Fi (`WIFI:T:nopass;S:<ssid>;;`, que o celular reconhece sozinho e
  oferece conectar) com o SSID vindo de `WifiManager::GetApSsid()`; no
  resto do tempo, continua mostrando o QR da página dela, como antes.
  **`T:nopass` (sem senha) confirmado lendo o header/README reais do
  componente no GitHub antes de escrever isto** (não dava pra supor):
  `WifiManagerConfig` não tem nenhum campo de senha pra AP, e o README
  não menciona senha em nenhum momento do fluxo -- a rede de
  configuração é aberta de propósito (não faria sentido exigir senha
  pra configurar a senha). Não há getter público pra senha de AP no
  componente -- se um dia ele passar a ter senha, isso quebra
  silenciosamente (o `WIFI:T:nopass` ficaria errado); não tem como
  `static_assert` isso, só confirmar de vez em quando se o componente
  mudou.
- **Reentrada manual no modo Wi-Fi -- gatilho físico que faltava
  (§15.10)**. `EnterWifiConfigMode()` (`WifiBoard`, classe base) já
  existia e já era seguro chamar a partir de idle/listening/speaking
  (reseta o protocolo, espera 1s, só depois entra em modo config -- lido
  direto em `wifi_board.cc`) -- só não tinha nenhum gesto físico ligado
  a ela fora do boot. Agora **triplo clique** no `boot_button_` chama
  ela a qualquer momento (clique simples e toque longo já tinham outro
  significado). Deixa trocar de rede sem apagar o resto da configuração
  do aparelho.

  **Resto do §15 (captive portal, fallback `192.168.4.1`, preservar
  credenciais até validar a nova, voltar sozinho ao normal) -- CONFIRMADO
  que já vem pronto no `esp-wifi-connect`**, lendo o README real do
  componente (não vendored neste repo, então não dava pra grep): o fluxo
  descrito lá já é exatamente esse -- conecta no AP, abre
  `http://192.168.4.1`, portal mostra redes disponíveis, credenciais só
  são salvas depois de preenchidas, sai da configuração sem reiniciar. Nada disso
  precisou ser construído.
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
- **Hora automática via NTP -- corrige um bug real de fuso em dobro**.
  Havia um diagnóstico deixado no código (sem correção) suspeitando de
  double-shift de fuso horário; **confirmado**: `Ota::CheckVersion`
  (`main/ota.cc`, upstream, não é nosso) faz
  `ts += timezone_offset * 60 * 1000` **antes** de `settimeofday()` --
  ou seja, o epoch que o servidor de ativação manda já vem deslocado
  pelo fuso. Nossa própria `DeviceConfig::ApplyTimezone`
  (`setenv("TZ", ...)` + `tzset()`) desloca de novo em `localtime_r` --
  resultado: hora local errada em dobro (confirmado numa foto real de
  teste, com o relógio mostrando um horário destoante). `CheckNetworkReady()`
  agora também inicia `esp_netif_sntp` (mesmo ponto onde o `ConfigServer`
  sobe -- rede já confirmada) com `pool.ntp.org`. NTP devolve epoch UTC
  de verdade, sem nenhum fuso embutido, então aplicar nossa TZ por cima
  fica certo, uma vez só. Resync é automático e periódico por conta do
  próprio lwip (a cada 1h por padrão) -- corrige sozinho mesmo se o OTA
  rodar de novo depois e escrever a hora errada por cima.
- **Bipe de despertador antigo (alarme + fim de cada fase do pomodoro)**.
  Trocado `OGG_EXCLAMATION`/`OGG_POPUP` por `OGG_OLD_ALARM` (pedido pra
  ser bem mais irritante, proposital) em `AlarmEngine::SetOnFired`/
  `SetOnRingTick` (`sp-esp32-s3-1.54-muma.cc`) e nos dois pontos do
  pomodoro em `mcp_tools.cc` (mantida a convenção de 1 bipe pra pausa,
  2 pra recomeçar o foco -- só troca o som, não a contagem). O arquivo
  **não mora neste repo** -- é um som (Freesound, "Bedside Clock Alarm")
  convertido pra Ogg/Opus e colocado direto em
  `main/assets/common/old_alarm.ogg` no clone do `78/xiaozhi-esp32`
  (mesma pasta dos outros sons genéricos: `success.ogg`, `popup.ogg`
  etc.) -- `scripts/gen_lang.py` gera a constante `OGG_OLD_ALARM`
  automaticamente a partir do nome do arquivo, sem precisar editar
  Kconfig/CMake. **Sem esse arquivo o build quebra** (símbolo
  inexistente) -- diferente dos GIFs do bichinho, que são opcionais.
  Detalhe de formato: veio direto em Opus 48kHz (os outros sons daqui
  são 16kHz) -- funciona igual, porque `AudioService::PlaySound` lê a
  taxa de amostragem do próprio cabeçalho Opus, não assume um valor
  fixo.

  **Achado testando em hardware real, e pior do que parecia**: `.ogg`
  novo em `main/assets/common/` exige forçar a regeneração de
  `assets/lang_config.h`, senão o build QUEBRA (símbolo inexistente,
  tipo `OGG_OLD_ALARM` não declarado). Causa: o `add_custom_command` que
  gera esse header no `main/CMakeLists.txt` só lista `language.json` e
  `gen_lang.py` como `DEPENDS` -- a pasta `main/assets/common/` não
  entra nessa lista, então o ninja (que decide se re-executa a regra
  comparando datas dos arquivos em `DEPENDS`, não por reconfigurar o
  CMake) não tem motivo pra regenerar o header quando um `.ogg` novo
  aparece ali.

  **`idf.py reconfigure` sozinho NÃO resolve** -- confirmado tentando em
  hardware real: reconfigure refaz as regras do `build.ninja`, mas quem
  decide se a regra roda de novo é o ninja comparando datas, e nenhuma
  das dependências listadas mudou. O que funciona de verdade é apagar o
  arquivo gerado, forçando o ninja a recriar do zero (sem ele existir,
  não tem "data" pra comparar):
  ```
  rm main/assets/lang_config.h   # ou Remove-Item no PowerShell
  idf.py build
  ```
- **Reboot voltava pro rosto genérico, mesmo com espécie customizada
  escolhida**. Mesma causa-raiz do bug já corrigido na troca de
  bichinho pela página: `Tamagotchi::SetMood()` só dispara o redraw
  quando o humor MUDA. No boot, o humor carregado do NVS já nasce igual
  ao valor inicial -- nunca "muda" -- então `SetEmotion()` nunca era
  chamado com a coleção customizada certa; só acontecia por acidente na
  primeira mudança real de humor depois de ligar (ex.: primeiro
  pomodoro do dia). `ApplyPetEmojiCollection` já rodava no construtor,
  mas só troca a coleção nos temas -- não redesenha nada sozinha, e não
  dava pra chamar `SetEmotion()` ali mesmo (`SetupUI()` do display ainda
  não tinha rodado). Corrigido igual ao selo de estágio: forçando
  `GetDisplay()->SetEmotion(...)` com o humor atual dentro de
  `CheckNetworkReady()`, mesmo ponto onde `UpdateStageBadge()` já fazia
  isso pelo mesmo motivo.
- **Notificações MCP de evento do dispositivo (Pomodoro → IA), Fase 1 da
  proposta "NUMA v3"**. Diferente de tudo antes desta linha, mexe em
  **arquivos do core** (`main/application.h`/`.cc`, compartilhados por
  todas as placas do fork) -- por isso **não têm cópia neste repo**
  (`muma-board-v3` só espelha `main/boards/spotpear/sp-esp32-s3-1.54-muma/`),
  só registrados aqui pra rastreabilidade. `Application::SendMcpNotification(method, params)`
  é função nova (`application.h`/`.cc`), NÃO reaproveita
  `Application::SendMcpMessage(payload)` existente -- essa é o
  transporte de respostas JSON-RPC correlacionadas por `id` (só chamada
  por `McpServer::ReplyResult`/`ReplyError`), contrato diferente de uma
  notificação fire-and-forget sem `id`. A nova função monta o envelope
  `{"jsonrpc":"2.0","method":...,"params":...}` com cJSON e despacha
  pelo `SendMcpMessage()` já existente por dentro (transporte
  reaproveitado, só a API pública é nova).

  Ligada em `mcp_tools.cc` (`WireEngines()`), usando os callbacks que o
  `PomodoroEngine` já tinha (`on_phase_changed_`/`on_break_completed_`):
  - `pomodoro.completed` (`duration_minutes`) -- fim do foco.
  - `pomodoro.break_completed` (`break_duration_minutes`) -- fim da
    pausa. **Divergência da proposta original**: a especificação sugeria
    que a IA devia "convidar o usuário a iniciar o próximo ciclo" nesse
    evento -- não faz mais sentido desde que o pomodoro passou a
    encadear os ciclos sozinho (ver bullet da sineta acima); o texto da
    Knowledge Base do servidor precisa refletir isso.
  - `pomodoro.break_warning` (`remaining_seconds`) -- **decisão
    explícita, revisada depois da primeira implementação**: só existe
    aviso prévio antes do fim da PAUSA, nunca antes do fim do foco (a
    primeira versão desta feature tinha os dois -- `pomodoro.focus_warning`
    incluído -- removida a pedido). Isso exigiu um estado novo em
    `PomodoroState` (`pomodoro_tool.h`): o antigo `WARNING` genérico
    (que só cobria o fim do foco) foi **removido**, substituído por
    `BREAK_WARNING` (só existe pro lado da pausa). Sem essa separação, o
    texto "Foco"/"Pausa" na tela (`UpdatePomodoroLabel`) e a notificação
    MCP não teriam como saber qual das duas fases o aviso genérico
    estava anunciando.

  O lado do servidor (tratar a notificação como evento do dispositivo,
  não como fala do usuário) **não foi tocado** -- fora do escopo deste
  repo, ver `ATUALIZACAO_NUMA.md` seção 4.

## Decisões de arquitetura -- proposta "NUMA v3" (Fase 2.5)

As 3 decisões que a própria `ATUALIZACAO_NUMA.md` (§17) listava como
bloqueio antes de qualquer Fase 3 em diante -- **decididas**:

1. **Perfil único por aparelho, mantido.** Não reverte pra
   multi-perfil-por-device. Um Numa continua sendo de uma criança só.
   Isso torna as seções 6/7 do documento (Voice Identification,
   `UserProfile`, Primary/Authorized Users) **fora de escopo** -- não
   fazem sentido no modelo atual.
2. **Game Engine se soma ao Tamagotchi, não substitui.** O bichinho
   continua sendo "o bichinho" (pontos/evolução); o Game Engine, quando
   construído, é uma camada separada ("as aventuras"), com recompensas
   visualmente distintas (ver §10.4 do documento) pra não confundir a
   criança.
3. **Tudo fica na placa.** Nenhuma camada nova (Game/Regulation/
   Transition/Safety Engine) é promovida pro núcleo compartilhado do
   fork -- continua tudo dentro de
   `main/boards/spotpear/sp-esp32-s3-1.54-muma/`, mesmo padrão de hoje.

**Sobreposição encontrada, ainda não resolvida**: o "Regulation Engine"
proposto (§11, estados `NORMAL/AMARELO/VERMELHO/RECUPERACAO`) descreve
quase exatamente o `OverloadSemaphore` que **já existe** neste repo
(`semaphore_tool.h`/`.cc`, estados `VERDE/AMARELO/VERMELHO`, já com
tools de voz, endpoints web e notificação ntfy) -- a diferença real é
só o estado `RECUPERACAO`, que não existe hoje. Antes de construir um
"Regulation Engine" do zero, faz mais sentido **estender** o
`OverloadSemaphore` existente com esse estado a mais do que duplicar o
sistema inteiro. Mesma lógica pro "Transition Engine" (§12): o aviso de
`BREAK_WARNING` no pomodoro já cobre parte do que essa seção descreve.
- **`OverloadSemaphore` ganha o estado `RECUPERACAO`** (decisão tomada:
  estender em vez de criar um "Regulation Engine" paralelo, ver bullet
  acima). Sinaliza que ela está voltando ao normal depois de um
  amarelo/vermelho -- continua a mesma regra de ouro (só ela ativa, o
  aparelho nunca muda de nível sozinho). Tratado como o verde pra fins
  de notificação (`Evaluate()` em `semaphore_tool.cc`): nunca dispara
  aviso automático pros pais -- desescalar não é urgente -- e cancela
  qualquer confirmação pendente de um nível anterior. Chave de voz/web:
  `"recuperacao"` (`self.semaphore.set_level` e `POST /api/semaphore/set`).
  Botão novo em `/` (🔵 Melhor) -- o resto do JS (`SEM_BTN`) já era
  genérico, só precisou de uma entrada a mais no mapa.
- **Game Engine (aventuras) -- arquivo novo, `game_tool.h`/`.cc`**.
  Decisão tomada: soma ao Tamagotchi, não substitui -- os dois vivem
  lado a lado, sem nenhuma referência cruzada. "Estrelas" (moeda do
  Game Engine) são inteiramente separadas dos pontos de evolução do
  bichinho, de propósito (pedido explícito: não confundir a criança com
  dois sistemas de recompensa parecidos). `GameEngine` guarda o estado
  real (`active`, `game_id`, `capítulo`, `estrelas`, `personagens`,
  `missão`, `última escolha`) no NVS, mesmo padrão JSON-em-string que
  `AlarmEngine`/`RoutineEngine` já usavam pra listas.

  6 tools MCP (`self.game.*`): `start` (zera tudo, começa aventura
  nova), `status` (estado real, pra IA nunca inventar progresso),
  `choice` (só registra, quem decide a consequência narrativa é a IA),
  `reward` (estrelas + item/personagem, os dois opcionais numa chamada
  só), `level` (avança capítulo, com missão nova opcional), `end`
  (encerra sem apagar o progresso).

  **Simplificação em relação à proposta original**: a doc listava
  `self.game.save`/`self.game.load` como tools separadas -- removidas
  daqui porque nenhum outro sistema deste projeto (Tamagotchi, rotina,
  alarmes, semáforo) tem um passo de "salvar" manual -- tudo já
  persiste sozinho a cada mutação, e manter essa mesma convenção evita
  um jeito diferente de fazer a mesma coisa só pro Game Engine.

  **Arquivo `.cc` novo -- precisa de `idf.py reconfigure` antes do
  próximo build** (ver seção abaixo).

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
