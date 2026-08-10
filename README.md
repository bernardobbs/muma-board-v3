# muma-board v3 — perfil aberto (nome + idade, não perfis fixos)

Arquivos pra `main/boards/spotpear/sp-esp32-s3-1.54-muma/` no clone do
`78/xiaozhi-esp32`. Essa pasta já existe lá (hardware puro: display,
touch, energia, áudio) -- os arquivos deste repo entram do lado dela.

## Como integrar (confirmado no repo base, não é só suposição)

1. Copie todos os `.h`/`.cc` deste repo (exceto `README.md`,
   `board_integration.md`, `RELATORIO_CONTINUIDADE.md`) pra dentro de
   `main/boards/spotpear/sp-esp32-s3-1.54-muma/` no clone.
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
4. Aplique o trecho de `board_integration.md` em
   `sp-esp32-s3-1.54-muma.cc` (ainda não fizemos isso -- é o próximo
   passo da integração, ver pendências abaixo).
5. Depois de editar `www/index.html` ou `www/admin.html`, rode
   `python3 scripts/gen_web_assets.py` pra regenerar `web_assets.cc`.

## ⚠️ Continua sem compilação — mesmo aviso de sempre

Sem ESP-IDF aqui. Não compilei nem testei. Base sólida, não pronta.
A pasta do board no `78/xiaozhi-esp32` já existe e foi conferida (ver
seção acima), mas o snippet de `board_integration.md` ainda não foi
colado em `sp-esp32-s3-1.54-muma.cc` -- sem isso, o firmware liga como
um xiaozhi genérico, sem nenhuma feature do companheiro.

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

1. GIFs (arte por humor/estágio)
2. Label do cronômetro na tela
3. Selo de estágio (2ª camada visual)
4. Sincronização com Google Sheets
5. Biblioteca de estratégias portada pro ESP-IDF (existe só no
   `family-firmware` Arduino)
6. Basic Auth em HTTP puro -- ok em rede doméstica, não exponha à internet

## Resolvido nesta passada

- **Vocabulário de emoção do bichinho corrigido**: `Tamagotchi::MoodName()`
  usava chaves em português (`"neutro"`, `"focado"`...) que não batem
  com nenhum pacote de emoji do `xiaozhi-esp32`. Confirmado no código
  base que o vocabulário padrão é em inglês (`"neutral"`, `"happy"`,
  `"thinking"`, `"surprised"`, `"funny"`...) e já é reconhecido pelos
  pacotes de emoji embutidos (ex.: `noto-color-emoji_32`, já escolhido
  pra essa placa) — sem precisar de GIF customizado nenhum.
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
