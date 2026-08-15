#pragma once
#include <functional>
#include <string>

// Antes, cada valor configuravel vivia num lugar: duracoes do pomodoro
// eram `constexpr` (nao dava pra mudar), brilho/volume eram salvos mas
// nunca aplicados, limiares do tamagotchi estavam hardcoded no board.
// Tudo isso passa por aqui agora.
//
// Nao ha mais perfil fixo -- os valores abaixo comecam
// com o que a faixa etaria da crianca sugere (ChildProfile), e dali em
// diante sao 100% editaveis por /admin, independente da idade.
class DeviceConfig {
public:
    static DeviceConfig& GetInstance() { static DeviceConfig i; return i; }

    void Load();

    // --- pomodoro (minutos) ---
    int study_minutes() const { return study_min_; }
    int break_minutes() const { return break_min_; }
    int warning_seconds() const { return warning_sec_; }
    void set_pomodoro(int study_min, int break_min);
    void set_warning_seconds(int seconds);

    // Bonus por voltar ao foco logo depois de uma pausa CUMPRIDA (nao
    // interrompida) -- incentivo pra ela nao deixar o tempo passar
    // depois da pausa. Ver PomodoroEngine::Start().
    int quick_return_minutes() const { return quick_return_min_; }
    void set_quick_return_minutes(int minutes);

    // --- tamagotchi ---
    int daily_point_cap() const { return daily_cap_; }
    int stage2() const { return stage2_; }
    int stage3() const { return stage3_; }
    int stage4() const { return stage4_; }
    void set_tama_rules(int daily_cap, int s2, int s3, int s4);

    // --- semaforo / alerta (gated por ChildProfile::regulation_tools_enabled) ---
    bool notify_on_yellow() const { return notify_yellow_; }
    bool notify_on_red() const { return notify_red_; }
    bool auto_send_yellow() const { return auto_yellow_; }
    bool auto_send_red() const { return auto_red_; }
    void set_alert_rules(bool y_on, bool r_on, bool y_auto, bool r_auto);

    std::string ntfy_server() const { return ntfy_server_; }
    std::string ntfy_topic() const { return ntfy_topic_; }
    void set_ntfy(const std::string& server, const std::string& topic);

    // --- aparencia / hardware ---
    int brightness() const { return brightness_; }
    int volume() const { return volume_; }
    void set_brightness(int v);
    void set_volume(int v);

    std::string timezone() const { return timezone_; }
    void set_timezone(const std::string& tz);
    void ApplyTimezone() const;  // chama setenv("TZ") + tzset()

    // O board registra estes callbacks; assim o servidor web nao precisa
    // conhecer Board/Display -- foi o que fazia brilho/volume nao serem
    // aplicados antes.
    void SetOnBrightnessChanged(std::function<void(int)> cb) { on_brightness_ = cb; }
    void SetOnVolumeChanged(std::function<void(int)> cb) { on_volume_ = cb; }
    void SetOnPomodoroRulesChanged(std::function<void()> cb) { on_pomodoro_rules_ = cb; }

    std::string ToJson() const;          // pagina dela (sem segredos)
    std::string ToAdminJson() const;     // pagina /admin (tudo)

    // Faixa segura que ela pode ajustar sozinha
    static constexpr int kMinStudyMin = 10;
    static constexpr int kMaxStudyMin = 50;
    static constexpr int kMinBreakMin = 2;
    static constexpr int kMaxBreakMin = 15;
    static constexpr int kMinQuickReturnMin = 1;
    static constexpr int kMaxQuickReturnMin = 30;

private:
    DeviceConfig() = default;
    static int Clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

    int study_min_ = 25, break_min_ = 5, warning_sec_ = 120;
    int quick_return_min_ = 5;
    int daily_cap_ = 8, stage2_ = 5, stage3_ = 15, stage4_ = 30;
    bool notify_yellow_ = false, notify_red_ = true;
    bool auto_yellow_ = false, auto_red_ = true;
    std::string ntfy_server_ = "https://ntfy.sh";
    std::string ntfy_topic_;
    int brightness_ = 80, volume_ = 70;
    std::string timezone_ = "<-03>3";  // Piaui (America/Fortaleza, sem horario de verao)

    std::function<void(int)> on_brightness_, on_volume_;
    std::function<void()> on_pomodoro_rules_;
};
