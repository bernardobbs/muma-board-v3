#pragma once
#include <string>
#include <vector>

// Aventuras/historias -- camada SEPARADA do Tamagotchi (decisao
// tomada: soma, nao substitui -- ver board_integration.md). Pontos de
// evolucao do bichinho continuam vindo so de pomodoro/rotina;
// "estrelas" aqui sao a moeda do Game Engine, visualmente distinta de
// proposito (pra nao confundir a crianca com dois sistemas de
// recompensa parecidos).
//
// O FIRMWARE guarda o estado real (capitulo, estrelas, personagens,
// missao) -- a IA nunca deve inventar progresso sozinha; ela consulta
// self.game.status e so avanca a historia atraves das tools, que
// escrevem aqui de verdade.
class GameEngine {
public:
    static GameEngine& GetInstance() { static GameEngine i; return i; }

    void Initialize();   // le do NVS

    // Comeca uma aventura nova -- zera capitulo/estrelas/personagens/
    // missao, mesmo se ja tiver uma aventura em andamento (troca).
    void Start(const std::string& game_id);
    // Encerra -- estado fica salvo (capitulo/estrelas nao se perdem),
    // so "active" vira false. Retomar e so chamar Start() nao com o
    // mesmo game_id -- comeca aventura nova; nao existe "continuar" na
    // v1 (ver nota no .cc).
    void End();
    bool active() const { return active_; }

    // Registra a escolha que ela fez, pra self.game.status devolver
    // com precisao -- o firmware NAO interpreta a escolha nem decide
    // consequencia nenhuma sozinho, isso e trabalho da IA.
    void SetChoice(const std::string& choice);

    // stars: quantidade a ADICIONAR (nao o total novo). item: nome
    // livre de personagem/objeto novo (ex.: "coelho", "chave dourada"),
    // ignorado se vazio -- assim uma chamada so cobre "ganhou
    // estrelas", "ganhou um personagem", ou os dois juntos.
    void AddReward(int stars, const std::string& item);

    // Avanca pro proximo capitulo -- separado de AddReward() porque
    // progredir na historia e ganhar premio sao coisas independentes
    // (pode progredir sem premio, ou ganhar premio no meio do mesmo
    // capitulo).
    void AdvanceChapter(const std::string& new_mission);

    std::string StatusJson() const;

private:
    GameEngine() = default;
    void Load();
    void Save() const;

    bool active_ = false;
    std::string game_id_;
    int chapter_ = 0;
    int stars_ = 0;
    std::vector<std::string> characters_;
    std::string mission_;
    std::string last_choice_;
};
