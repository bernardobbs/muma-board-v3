# muma-board v3 — perfil aberto (nome + idade, não perfis fixos)

> **Quer só compilar e testar?** Já existe um clone pronto do
> `78/xiaozhi-esp32` com estes arquivos mesclados dentro dele:
> [bernardobbs/Numa-firmware](https://github.com/bernardobbs/Numa-firmware)
> (ver `FLASHING.md` lá pro passo a passo de build/flash/teste). Este
> repo aqui é o histórico de desenvolvimento das features -- não
> precisa fazer a mesclagem manual, ela já está feita.

Arquivos pra `main/boards/spotpear/sp-esp32-s3-1.54-muma/` no clone do
`78/xiaozhi-esp32`. Essa pasta já existe lá (hardware puro: display,
touch, energia, áudio) -- os arquivos deste repo entram do lado dela.

## Como integrar (confirmado no repo base, não é só suposição)

1. Copie todos os `.h`/`.cc` deste repo (exceto `README.md`,
   `board_integration.md`, `RELATORIO_CONTINUIDADE.md`) pra dentro de
   `main/boards/spotpear/sp-esp32-s3-1.54-muma/` no clone, **substituindo**
   o `sp-esp32-s3-1.54-muma.cc` que já existe lá -- o daqui já vem com
   as features de família integradas (ver `board_integration.md`).
2. **Não precisa de `CMakeLists.txt` próprio.** O build do
   `xiaozhi-esp32` não usa `idf_component_register` por board -- é um
   único component `main` pro projeto inteiro (`main/CMakeLists.txt`),
   e cada board só tem seus `.cc`/`.c` capturados por um
   `file(GLOB boards/<board>/*.cc)` automático. Qualquer arquivo novo
   na pasta do board entra sozinho.
3. **`esp_http_server`, `esp_http_client`, `cJSON`, NVS (via `Settings`)
   e `esp_crt_bundle_attach` (via `mbedtls`) já estão disponíveis**,
   sem precisar declarar `REQUIRES` em lugar nenhum: confirmado que
   `main/boards/otto-robot/` e `main/boards/electron-bot/` já incluem
   `esp_http_server.h` direto na pasta do board, sem nenhum
   `CMakeLists.txt` próprio -- e `mbedtls`/`json`/NVS já são usados
   amplamente em `main/` (settings, OTA, protocolos).
4. Antes de flashear, troque `FAMILY_ADMIN_PASSWORD` no topo do
   `sp-esp32-s3-1.54-muma.cc` pela senha real de admin.
5. Depois de editar `www/index.html` ou `www/admin.html`, rode
   `python3 scripts/gen_web_assets.py` pra regenerar `web_assets.cc`.

## ⚠️ Continua sem compilação — mesmo aviso de sempre

Sem ESP-IDF aqui. Não compilei nem testei, e não tenho o aparelho pra
validar em hardware real. A pasta do board no `78/xiaozhi-esp32` já
existe e foi conferida, e `sp-esp32-s3-1.54-muma.cc` já vem com as
features de família integradas (ver seção acima e
`board_integration.md`) -- mas isso é revisão de código, não teste em
placa. Primeira coisa a fazer com o hardware em mãos: compilar de
verdade e ver se o boot completa sem crash.

## O que mudou desde a v2

**Saiu:** `enum class Profile` fixo com duas crianças hardcoded, catálogos de
bichinho diferentes por criança, rotinas hardcoded pra cada uma.

**Entrou:** `ChildProfile` — nome + data de nascimento, configurados em
`/admin`. A idade é **calculada em tempo real** (não precisa reconfigurar
no aniversário) e vira **ponto de partida** pra duração de pomodoro,
teto de pontos e limiares de evolução — tudo continua 100% editável
depois, a idade só sugere um valor inicial razoável.

| Faixa | Foco | Pausa | Aviso prévio | Teto/dia |
|---|---|---|---|---|
| até 6 anos | 8min | 3min | 45s | 10 |
| 7-9 anos | 12min | 4min | 60s | 9 |
| 10-12 anos | 18min | 5min | 90s | 8 |
| 13+ anos | 25min | 5min | 120s | 8 |

Não são recomendação clínica -- são chute inicial razoável, ajustável a
qualquer momento observando como a criança reage.

## Uma decisão de design que separei de propósito

O semáforo de sobrecarga e a biblioteca de estratégias **não são mais
amarrados a um perfil fixo nem a idade** -- viraram um toggle
independente em `/admin` ("Ativar semáforo de sobrecarga..."). É sobre
necessidade específica (regulação sensorial/emocional), não sobre
quantos anos a criança tem. Uma criança de 7 anos pode precisar tanto
quanto uma de 15, ou nenhuma das duas precisar.

## Catálogo de bichinho: unificado, não mais por idade

Antes, cada criança escolhia entre 4 opções fixas do seu perfil. Agora é
**uma lista só com as 8**, disponível pra qualquer idade -- lobo, raposa,
gato, dragão, unicórnio, coelha, panda, pintinho. Gosto não é algo que
se determine por faixa etária; quem escolhe é a criança.

## Simplificação de brinde

Como cada aparelho físico agora serve **uma criança**, não um "modelo"
entre dois fixos, os namespaces do NVS pararam de precisar de sufixo
(`tama_<perfil>` -> só `tama`). Menos código, menos chance de
esquecer de atualizar os dois lados quando alguma coisa muda.

## Arquivos novos/alterados

| Arquivo | O quê |
|---|---|
| `child_profile.h/.cc` | **Novo.** Nome, nascimento, idade calculada, faixa etária, toggle de regulação |
| `device_config.*` | Lê defaults da faixa etária em vez do profile fixo |
| `tamagotchi_tool.cc` | Catálogo unificado, namespace fixo |
| `routine_engine.cc` | Namespace fixo, rascunho único genérico |
| `routine_defaults.h` | Um rascunho neutro em vez de dois específicos |
| `mcp_tools.cc` | Semáforo gated por toggle, não por perfil |
| `config_server.cc` | Endpoint admin recebe nome/nascimento/toggle |
| `www/admin.html` | Campos de nome/data no lugar do seletor de perfil fixo |
| `www/index.html` | Saudação com o nome real da criança |

## O que ainda não existe (herdado da v2, sem mudança)

1. Sincronização com Google Sheets -- **precisa de credenciais do
   Google Cloud** (projeto + OAuth ou service account) que só vocês
   podem gerar
2. Biblioteca de estratégias portada pro ESP-IDF (existe só no
   `family-firmware` Arduino) -- **precisa do conteúdo original**, não
   está neste repo nem no `78/xiaozhi-esp32`
3. Basic Auth em HTTP puro -- ok em rede doméstica, não exponha à internet

## GIFs do bichinho: infraestrutura pronta, arte pendente

Confirmado no `assets.bin` real do aparelho e no código do board
(`main/display/lcd_display.cc`): o rosto já reage ao humor **sem
nenhum GIF customizado** (usa o pacote padrão de 21 emoções estáticas
que já vem no aparelho). GIF animado de verdade também funciona nessa
placa -- o `LcdDisplay` detecta os bytes mágicos "GIF" e anima sozinho
via `LvglGif`, sem precisar de nenhum player customizado.

O que falta é só a **arte por espécie**:

1. Gere os `.gif` (`pet_gifs/<especie>_<humor>.gif`, uma por espécie×humor)
   -- ver os scripts de geração via Stable Diffusion no chat.
2. Rode `python3 scripts/gen_pet_emoji_collection.py` pra empacotar
   os GIFs em `pet_emoji_collection.cc` (vira array de bytes C, pego
   pelo `file(GLOB)` do board como qualquer outro `.cc`).
3. Sem GIFs em `pet_gifs/`, `CreatePetEmojiCollection()` devolve
   `nullptr` e o aparelho usa o pacote padrão -- nunca trava por falta
   de arte.

## Resolvido nesta passada

- **Integração com o board real aplicada**: `sp-esp32-s3-1.54-muma.cc`
  (raiz deste repo) é o arquivo completo do board com
  `InitializeFamilyFeatures()` chamado no construtor e
  `ConfigServer::Start()` disparado via `SetNetworkEventCallback` em
  `NetworkEvent::Connected` -- confirmado no código de
  `main/boards/common/wifi_board.h`/`.cc` do repo base que é esse o
  mecanismo real de "depois que o Wi-Fi conectar". Ver
  `board_integration.md` pro detalhe de cada peça.
- **Label do cronômetro na tela**: `UpdatePomodoroLabel(s)` mostra
  `MM:SS` no canto superior direito, criado sob demanda no primeiro
  tick (não dá pra criar no construtor -- `SetupUI()` do display só
  roda depois, ver `board_integration.md`). Ligado em
  `PomodoroEngine::SetOnTick`.
- **Selo de estágio**: `UpdateStageBadge()` mostra "Ovo"/"Filhote"/
  "Jovem"/"Forte" como texto no canto inferior direito, ligado em
  `Tamagotchi::SetOnEvolved`.
- **Vocabulário de emoção do bichinho corrigido**: `Tamagotchi::MoodName()`
  usava chaves em português (`"neutro"`, `"focado"`...) que não batem
  com nenhum pacote de emoji do `xiaozhi-esp32`. Confirmado no código
  base que o vocabulário padrão é em inglês (`"neutral"`, `"happy"`,
  `"thinking"`, `"surprised"`, `"funny"`...) e já é reconhecido pelos
  pacotes de emoji embutidos (ex.: `noto-color-emoji_32`, já escolhido
  pra essa placa) — sem precisar de GIF customizado nenhum.
- **Plumbing pra GIF customizado por espécie**: `pet_emoji_collection.h/.cc`
  (o `.cc` é gerado por `scripts/gen_pet_emoji_collection.py` a partir
  de `pet_gifs/<especie>_<humor>.gif`) e o board troca a coleção de
  emoji ao trocar de espécie (`ApplyPetEmojiCollection`, ligado no
  `SetOnSpeciesChanged`). Ainda sem arte gerada -- por enquanto
  `CreatePetEmojiCollection` sempre devolve `nullptr` e o pacote padrão
  continua valendo.
- **HTML das páginas não depende mais de `EMBED_TXTFILES`**: o build
  real do `xiaozhi-esp32` não tem esse gancho por board (só um
  `idf_component_register` único pro projeto inteiro). O conteúdo de
  `www/index.html` e `www/admin.html` agora vive em `web_assets.cc`
  como string C++, gerado por `scripts/gen_web_assets.py` — rode esse
  script depois de editar qualquer um dos dois HTMLs.
- **CSRF nos POSTs de `/admin`**: `GET /api/admin/csrf` devolve um token
  gerado por boot; `POST /api/admin/routine` e `POST /api/admin/config`
  agora exigem esse token no header `X-CSRF-Token` (além da senha).
  Isso fecha o CSRF clássico de Basic Auth: uma página maliciosa aberta
  no mesmo navegador não sabe o token, então não consegue forjar esses
  POSTs mesmo que o navegador reenvie as credenciais automaticamente.
- **Toggle do semáforo passou a ser dinâmico**: `self.semaphore.*` no
  MCP agora checa `regulation_tools_enabled()` a cada chamada, não só
  no boot -- ligar/desligar em `/admin` faz efeito na hora.
