# muma-board v3 — perfil aberto (nome + idade, não Alana/Clara fixos)

Arquivos pra `main/boards/spotpear/sp-esp32-s3-1.54-muma/` no clone do
`78/xiaozhi-esp32`.

## ⚠️ Continua sem compilação — mesmo aviso de sempre

Sem ESP-IDF aqui. Não compilei nem testei. Base sólida, não pronta.

## O que mudou desde a v2

**Saiu:** `enum class Profile { ALANA, CLARA }` fixo, catálogos de
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

Antes, Alana escolhia entre 4 opções e Clara entre outras 4. Agora é
**uma lista só com as 8**, disponível pra qualquer idade -- lobo, raposa,
gato, dragão, unicórnio, coelha, panda, pintinho. Gosto não é algo que
se determine por faixa etária; quem escolhe é a criança.

## Simplificação de brinde

Como cada aparelho físico agora serve **uma criança**, não um "modelo"
entre dois fixos, os namespaces do NVS pararam de precisar de sufixo
(`tama_alana`/`tama_clara` -> só `tama`). Menos código, menos chance de
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
| `www/admin.html` | Campos de nome/data no lugar do seletor Alana/Clara |
| `www/index.html` | Saudação com o nome real da criança |

## O que ainda não existe (herdado da v2, sem mudança)

1. GIFs (arte por humor/estágio)
2. Label do cronômetro na tela
3. Selo de estágio (2ª camada visual)
4. Sincronização com Google Sheets
5. Biblioteca de estratégias portada pro ESP-IDF (existe só no
   `family-firmware` Arduino)
6. Basic Auth em HTTP puro -- ok em rede doméstica, não exponha à internet
7. Sem CSRF nos POSTs
