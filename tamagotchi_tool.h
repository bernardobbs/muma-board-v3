#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

enum class TamaStage { OVO, FILHOTE, JOVEM, FORTE };
enum class TamaMood  { NEUTRO, FOCADO, AVISO, BRINCALHAO, COMEMORANDO };

struct SpeciesOption {
    std::string id;
    std::string species_label;
    std::string suggested_name;
};

class Tamagotchi {
public:
    static Tamagotchi& GetInstance() { static Tamagotchi i; return i; }

    // Catalogo unico pra qualquer idade; os limiares vem do
    // DeviceConfig, entao nao ha mais TamaConfig separado.
    void Initialize();
    void ReloadRules() {}   // limiares sao lidos do DeviceConfig a cada uso

    void OnPomodoroCompleted();
    void OnBreakRespected();
    void AddCustomPoints(int amount, const std::string& reason);
    void SetMood(TamaMood mood);

    TamaStage stage() const { return stage_; }
    TamaMood mood() const { return mood_; }
    int points() const { return total_points_; }
    int points_today() const { return today_points_; }

    std::string pet_name() const;
    std::string species_id() const;
    std::string StageName() const;
    std::string MoodName() const;   // chave usada no EmojiCollection
    std::string StatusJson() const; // inclui especie + catalogo (a pagina precisa)

    bool ChooseSpecies(const std::string& id);
    const std::vector<SpeciesOption>& catalog() const { return catalog_; }

    void SetOnMoodChanged(std::function<void(const std::string&)> cb) { on_mood_ = cb; }
    void SetOnEvolved(std::function<void(TamaStage)> cb) { on_evolved_ = cb; }
    void SetOnSpeciesChanged(std::function<void(const std::string&)> cb) { on_species_ = cb; }

private:
    Tamagotchi() = default;
    void AddPoints(int amount);
    void RecalcStage();
    void CheckDayRollover();
    int IndexForId(const std::string& id) const;

    std::string ns_;
    std::vector<SpeciesOption> catalog_;
    int species_index_ = 0;
    TamaStage stage_ = TamaStage::OVO;
    TamaMood mood_ = TamaMood::NEUTRO;
    int total_points_ = 0, today_points_ = 0;
    int32_t stored_day_ = 0;

    std::function<void(const std::string&)> on_mood_, on_species_;
    std::function<void(TamaStage)> on_evolved_;
};
