# Relatório de continuidade — Companheiros Afetivos / MUMA board v3

Contexto para o Claude Code retomar o projeto do zip `muma-board-v3.zip`.

## O que é este projeto

Firmware ESP32-S3 (Spotpear MUMA board) para um "companheiro afetivo" infantil:
tamagotchi + timer pomodoro + rotina diária + semáforo de sobrecarga sensorial,
com interface web dupla (`/` pra criança, `/admin` pra responsável, com Basic Auth).
Existe também uma versão Arduino/PlatformIO simulável no Wokwi, além desta em
ESP-IDF pro board real.

## O que mudou nesta passada (v3)

Objetivo da leva: **generalizar o perfil da criança**, que antes era fixo em
duas opções hardcoded ("Alana" / "Clara").

1. **`child_profile.h`/`.cc` (novo arquivo)** — substitui o antigo enum `Profile`
   por identidade real: nome + data de nascimento. Idade é calculada em tempo
   real a cada consulta (não precisa reconfigurar no aniversário). A faixa
   etária (`AgeBracket`) vira **só sugestão inicial** de valores numéricos
   (duração de pomodoro, teto de pontos, limiares de evolução) — nunca restringe
   escolhas.

2. **Catálogo de bichinho unificado** (`tamagotchi_tool.h`) — eram catálogos
   diferentes por perfil; agora são 8 opções, disponíveis pra qualquer idade.

3. **NVS simplificado** — namespaces deixam de ser por "modelo A/B" porque agora
   é uma premissa fixa do projeto: um aparelho = uma criança.

4. **Rotina com rascunho único** (`routine_defaults.h`) — um `kRoutineDefaultGeneric`
   neutro, usado só na primeira inicialização (ou se o salvo for inválido);
   editável por completo em `/admin`.

5. **Semáforo/estratégias de sobrecarga desacoplados de idade** — viraram um
   toggle independente em `/admin` (`ChildProfile::regulation_tools_enabled`),
   porque é uma necessidade específica, não uma questão etária.

6. **Bug corrigido antes de empacotar**: sobrava `Profile profile_ = Profile::ALANA;`
   em `device_config.h` depois da remoção do enum antigo — quebraria a compilação.
   Já corrigido no zip entregue.

## Pendência encontrada na checagem (não reportada antes, achada agora ao reabrir o zip)

Dois arquivos ainda têm **comentários residuais** citando o perfil antigo
"Alana/Clara" que não foram limpos na generalização:

- `tamagotchi_tool.h`: comentário na classe `Tamagotchi` — *"O catálogo depende
  do perfil (Alana/Clara)..."* — desatualizado, já que o catálogo agora é único.
- `semaphore_tool.h`: comentário na classe `OverloadSemaphore` — *"Semáforo de
  sobrecarga (perfil Alana)"* — também desatualizado, já que o semáforo virou
  toggle genérico via `ChildProfile::regulation_tools_enabled()`.
- `device_config.h`, linha 33: comentário `// --- semaforo / alerta (so Alana) ---`
  no bloco de `notify_on_yellow`/`notify_on_red`/etc. — mesmo problema.

Não afeta compilação nem comportamento, só documentação/comentários desalinhados
com o novo design. Vale uma limpeza rápida.

## Pendências já conhecidas (não mudaram nesta passada, ainda em aberto)

- GIFs (assets visuais do bichinho/estados) — não implementados
- Label do cronômetro (pomodoro) — pendente de ajuste
- Integração com Google Sheets — pendente
- Portar as "estratégias" (de regulação sensorial) do protótipo anterior pro
  novo toggle genérico em `/admin` — hoje o toggle existe mas o conteúdo/lista
  de estratégias ainda não foi migrado

## Arquivos do projeto (estado atual do zip)

```
muma-board-v3/
├── CMakeLists.txt
├── README.md
├── board_integration.md
├── child_profile.h / .cc       [novo nesta passada]
├── device_config.h / .cc       [tem bug corrigido + comentário residual pendente]
├── config_server.h / .cc
├── mcp_tools.h / .cc
├── notifications.h / .cc
├── pomodoro_tool.h / .cc
├── routine_engine.h / .cc
├── routine_defaults.h          [novo rascunho único genérico]
├── semaphore_tool.h / .cc      [comentário residual pendente]
├── tamagotchi_tool.h / .cc     [catálogo unificado 8 opções, comentário residual pendente]
├── day_utils.h
└── www/
    ├── index.html   (interface da criança)
    └── admin.html   (interface do responsável, Basic Auth)
```

## Próximos passos sugeridos (ordem sugerida, a confirmar com Bernardo)

1. Limpar os 3 comentários residuais "Alana/Clara" listados acima
2. Portar as estratégias de regulação sensorial pro toggle genérico
3. Resolver label do cronômetro
4. GIFs / assets visuais
5. Integração Google Sheets
