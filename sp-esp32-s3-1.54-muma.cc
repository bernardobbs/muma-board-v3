#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include <esp_log.h>
#include <driver/i2c_master.h>
#include "system_reset.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include <esp_lcd_panel_vendor.h>
#include <esp_io_expander_tca9554.h>
#include <driver/spi_common.h>
#include "i2c_device.h"
#include <esp_timer.h>
#include "power_manager.h"
#include "power_save_timer.h"
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <cstdio>
#include <esp_event.h>
#include <esp_netif.h>

// --- companheiro afetivo: features de familia (nao faziam parte do
// board original) ---
#include "child_profile.h"
#include "device_config.h"
#include "pomodoro_tool.h"
#include "tamagotchi_tool.h"
#include "routine_engine.h"
#include "semaphore_tool.h"
#include "mcp_tools.h"
#include "config_server.h"
#include "pet_emoji_collection.h"
#include "display/lvgl_display/lvgl_theme.h"

#define TAG "Spotpear_esp32_s3_lcd_1_54"

// TROQUEM por uma senha de verdade antes de flashear -- ver
// ConfigServer::RequireAdmin: sem senha, /admin fica 403 pra sempre.
#define FAMILY_ADMIN_PASSWORD "SENHA-QUE-VOCES-ESCOLHEREM"

// So troca a colecao de emoji se a especie tiver GIFs de verdade
// (pet_emoji_collection.cc); senao mantem o pacote padrao, que ja
// funciona sozinho com as chaves de humor padrao do xiaozhi.
static void ApplyPetEmojiCollection(const std::string& species_id) {
    auto collection = CreatePetEmojiCollection(species_id);
    if (collection == nullptr) return;
    auto& theme_manager = LvglThemeManager::GetInstance();
    if (auto* light = theme_manager.GetTheme("light")) light->set_emoji_collection(collection);
    if (auto* dark = theme_manager.GetTheme("dark")) dark->set_emoji_collection(collection);
}

class Cst816d : public I2cDevice {
public:
    struct TouchPoint_t {
        int num = 0;
        int x = -1;
        int y = -1;
    };
    Cst816d(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        uint8_t chip_id = ReadReg(0xA3);
        ESP_LOGI(TAG, "Get chip ID: 0x%02X", chip_id);
        read_buffer_ = new uint8_t[6];
    }

    ~Cst816d() {
        delete[] read_buffer_;
    }

    void UpdateTouchPoint() {
        ReadRegs(0x02, read_buffer_, 6);
        tp_.num = read_buffer_[0] & 0x0F;
        tp_.x = ((read_buffer_[1] & 0x0F) << 8) | read_buffer_[2];
        tp_.y = ((read_buffer_[3] & 0x0F) << 8) | read_buffer_[4];
    }

    const TouchPoint_t& GetTouchPoint() {
        return tp_;
    }

private:
    uint8_t* read_buffer_ = nullptr;
    TouchPoint_t tp_;
};

class Spotpear_esp32_s3_lcd_1_54 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Display* display_;
    esp_timer_handle_t touchpad_timer_;
    Cst816d* cst816d_;
    lv_obj_t* pomodoro_label_ = nullptr;   // label do cronometro, criado sob demanda
    lv_obj_t* stage_badge_ = nullptr;      // selo de estagio, criado sob demanda
    esp_io_expander_handle_t io_expander_ = NULL;
    esp_lcd_panel_handle_t panel_ = nullptr;

    PowerManager* power_manager_;
    PowerSaveTimer* power_save_timer_;
    void InitializePowerManager() {
        power_manager_ = new PowerManager(GPIO_NUM_41);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        rtc_gpio_init(GPIO_NUM_3);
        rtc_gpio_set_direction(GPIO_NUM_3, RTC_GPIO_MODE_OUTPUT_ONLY);
        rtc_gpio_set_level(GPIO_NUM_3, 1);

        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(1);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutting down");
            rtc_gpio_set_level(GPIO_NUM_3, 0);
            // 启用保持功能，确保睡眠期间电平不变
            rtc_gpio_hold_en(GPIO_NUM_3);
            esp_lcd_panel_disp_on_off(panel_, false); //关闭显示
            esp_deep_sleep_start();
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeCodecI2c_Touch() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
            .sda_io_num = TP_PIN_NUM_TP_SDA,
            .scl_io_num = TP_PIN_NUM_TP_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    static void touchpad_timer_callback(void* arg) {
        auto& board = (Spotpear_esp32_s3_lcd_1_54&)Board::GetInstance();
        auto touchpad = board.GetTouchpad();
        static bool was_touched = false;
        static int64_t touch_start_time = 0;
        const int64_t TOUCH_THRESHOLD_MS = 500;  // 触摸时长阈值，超过500ms视为长按

        touchpad->UpdateTouchPoint();
        auto touch_point = touchpad->GetTouchPoint();
        // 检测触摸开始
        if (touch_point.num > 0 && !was_touched) {
            was_touched = true;
            touch_start_time = esp_timer_get_time() / 1000; // 转换为毫秒
        }
        // 检测触摸释放
        else if (touch_point.num == 0 && was_touched) {
            was_touched = false;
            int64_t touch_duration = (esp_timer_get_time() / 1000) - touch_start_time;

            // 只有短触才触发
            if (touch_duration < TOUCH_THRESHOLD_MS) {
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateStarting) {
                    board.EnterWifiConfigMode();
                    return;
                }
                app.ToggleChatState();
            }
        }
    }

    void InitializeCst816DTouchPad() {
        ESP_LOGI(TAG, "Init Cst816D");
        cst816d_ = new Cst816d(i2c_bus_, 0x15);

        // 创建定时器，10ms 间隔
        esp_timer_create_args_t timer_args = {
            .callback = touchpad_timer_callback,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "touchpad_timer",
            .skip_unhandled_events = true,
        };

        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &touchpad_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(touchpad_timer_, 10 * 1000)); // 10ms = 10000us
    }

    void EnableLcdCs() {
        if(io_expander_ != NULL) {
            esp_io_expander_set_level(io_expander_, DISPLAY_SPI_CS_PIN, 0);// 置低 LCD CS
        }
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_SPI_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 60 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_RESET_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        EnableLcdCs();
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));

        // uint8_t data_0xBB[] = { 0x3F };
        // esp_lcd_panel_io_tx_param(panel_io, 0xBB, data_0xBB, sizeof(data_0xBB));

        uint8_t data_0xBB[] = { 0x38 };
        esp_lcd_panel_io_tx_param(panel_io, 0xBB, data_0xBB, sizeof(data_0xBB));

        display_ = new SpiLcdDisplay(panel_io, panel,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // Cronometro do pomodoro no canto (TOP_RIGHT nao e usado por
    // nenhum widget padrao do LcdDisplay -- top_bar_/status_bar_ ficam
    // em TOP_MID, emoji em CENTER, bottom_bar_ em BOTTOM_MID). Criado
    // na primeira chamada, nao antes: SetupUI() do display roda depois
    // do construtor do board (Application::Initialize() chama os dois
    // em sequencia), entao lv_screen_active() ainda nao existiria se a
    // gente tentasse criar isto no construtor.
    void UpdatePomodoroLabel(int seconds_remaining) {
        DisplayLockGuard lock(display_);
        if (pomodoro_label_ == nullptr) {
            pomodoro_label_ = lv_label_create(lv_screen_active());
            lv_obj_align(pomodoro_label_, LV_ALIGN_TOP_RIGHT, -4, 4);
        }
        if (seconds_remaining <= 0) {
            lv_label_set_text(pomodoro_label_, "");
            return;
        }
        // buf[16], nao buf[8]: na pratica "MM:SS" nunca passa de 5 chars
        // (pomodoro clampado a kMaxStudyMin=40min), mas o GCC (com
        // -Werror=format-truncation) calcula o pior caso teorico pra um
        // int generico, que passa de 8 bytes -- warning tratado como
        // erro no build. 16 bytes elimina o warning sem mudar nada em
        // tempo de execucao.
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d", seconds_remaining / 60, seconds_remaining % 60);
        lv_label_set_text(pomodoro_label_, buf);
    }

    // Selo de estagio (2a camada visual, junto do rosto/GIF de humor):
    // texto simples com StageName() ("Ovo"/"Filhote"/"Jovem"/"Forte"),
    // nao emoji -- a fonte de texto do board (font_noto_sans_basic) e
    // "basic charset", sem glifo de emoji; o emoji fica so na imagem
    // raster do EmojiCollection. BOTTOM_RIGHT fica livre (bottom_bar_
    // e BOTTOM_MID, pomodoro_label_ ja ocupa TOP_RIGHT).
    void UpdateStageBadge() {
        DisplayLockGuard lock(display_);
        if (stage_badge_ == nullptr) {
            stage_badge_ = lv_label_create(lv_screen_active());
            lv_obj_align(stage_badge_, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
        }
        lv_label_set_text(stage_badge_, Tamagotchi::GetInstance().StageName().c_str());
    }

    // ConfigServer precisa de rede -- so sobe quando o Wi-Fi conectar de
    // verdade. NAO usamos SetNetworkEventCallback pra isso: descoberto
    // via log real que Application::Initialize() (main/application.cc)
    // TAMBEM chama board.SetNetworkEventCallback(...) depois do
    // construtor do board rodar, pra notificacoes de UI ("Conectando a
    // Wi-Fi..."). Como e um unico std::function (nao uma lista), essa
    // chamada SOBRESCREVE silenciosamente qualquer callback que a gente
    // registre aqui -- sem erro nenhum, so nunca dispara. O evento
    // nativo do ESP-IDF aceita multiplos handlers e nao tem esse
    // conflito.
    static void OnGotIp(void* arg, esp_event_base_t base, int32_t event_id, void* event_data) {
        auto& board = (Spotpear_esp32_s3_lcd_1_54&)Board::GetInstance();
        ConfigServer::GetInstance().Start(FAMILY_ADMIN_PASSWORD);
        board.UpdateStageBadge();  // mostra o estagio ja salvo, sem esperar a proxima evolucao
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    // --- companheiro afetivo -------------------------------------------
    // Tudo que NAO depende de rede: perfil, regras, engines e as tools
    // MCP. O ConfigServer entra separado, em SetNetworkEventCallback,
    // porque precisa da rede de pe.
    void InitializeFamilyFeatures() {
        ChildProfile::GetInstance().Load();   // nome, nascimento, toggle de regulacao
        auto& cfg = DeviceConfig::GetInstance();
        cfg.Load();                       // le regras (com defaults por faixa etaria) e aplica o fuso

        // Callbacks que faltavam: brilho/volume eram salvos mas nunca aplicados
        cfg.SetOnBrightnessChanged([this](int v) { GetBacklight()->SetBrightness(v); });
        cfg.SetOnVolumeChanged([this](int v) { GetAudioCodec()->SetOutputVolume(v); });

        PomodoroEngine::GetInstance().Initialize();
        Tamagotchi::GetInstance().Initialize();
        RoutineEngine::GetInstance().Initialize();

        // Humor do bichinho -> emoji na tela. As chaves de MoodName() ja
        // seguem o vocabulario padrao do xiaozhi (neutral/happy/thinking/
        // surprised/funny), entao isso funciona com o pacote de emoji
        // padrao mesmo sem nenhum GIF customizado -- e SE a especie
        // escolhida tiver GIFs proprios (pet_emoji_collection.cc), eles
        // substituem o rosto generico automaticamente.
        Tamagotchi::GetInstance().SetOnMoodChanged([this](const std::string& mood) {
            GetDisplay()->SetEmotion(mood.c_str());
        });

        // Trocou de especie -> troca a colecao de emoji tambem, se essa
        // especie tiver GIFs proprios. CreatePetEmojiCollection devolve
        // nullptr pra especies sem GIF ainda -- nesse caso NAO troca
        // nada, e o pacote padrao (rosto generico) continua valendo.
        Tamagotchi::GetInstance().SetOnSpeciesChanged([](const std::string& id) {
            ApplyPetEmojiCollection(id);
        });
        ApplyPetEmojiCollection(Tamagotchi::GetInstance().species_id());  // especie ja salva, se houver

        // Cronometro na tela: label criado sob demanda no primeiro tick
        // (nao dá pra criar aqui ainda -- SetupUI() do display so roda
        // depois do construtor do board, ver Application::Initialize()).
        PomodoroEngine::GetInstance().SetOnTick([this](int s) { UpdatePomodoroLabel(s); });

        // Selo de estagio: atualiza a cada evolucao. O estagio JA
        // carregado do NVS (se o bichinho ja evoluiu antes do reboot)
        // e mostrado separado, la no NetworkEventCallback do construtor
        // -- mesma razao do cronometro, UpdateStageBadge() tambem cria
        // um lv_obj_t e SetupUI() ainda nao rodou aqui.
        Tamagotchi::GetInstance().SetOnEvolved([this](TamaStage) { UpdateStageBadge(); });

        mcp_tools::RegisterAll();

        // Aplica o brilho/volume salvos ja na partida -- sem isso, o
        // aparelho comeca com o ultimo valor que o driver tinha em cache,
        // nao com o que foi configurado em /admin ou na pagina dela.
        GetBacklight()->SetBrightness(cfg.brightness());
        GetAudioCodec()->SetOutputVolume(cfg.volume());
    }

public:

    Spotpear_esp32_s3_lcd_1_54() :boot_button_(BOOT_BUTTON_GPIO){
        gpio_set_direction(TP_PIN_NUM_TP_INT, GPIO_MODE_INPUT);
        int level = gpio_get_level(TP_PIN_NUM_TP_INT);
        if (level == 1) {
            InitializeCodecI2c_Touch();
            InitializeCst816DTouchPad();
        }
        InitializePowerSaveTimer();
        InitializeCodecI2c();
        InitializeSpi();
        InitializePowerManager();
        InitializeSt7789Display();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();

        InitializeFamilyFeatures();

        // ConfigServer precisa de rede -- so sobe quando o Wi-Fi conectar
        // de verdade. Ver OnGotIp() pra explicacao de por que isso NAO
        // usa SetNetworkEventCallback (a Application tambem usa esse
        // slot e sobrescreveria o nosso).
        //
        // O WifiManager (que cria o event loop padrao do ESP-IDF) so
        // inicializa DEPOIS do construtor do board rodar -- confirmado
        // em log real (nossas features de familia aparecem antes de
        // "WifiManager: Initializing..."). Sem isso, o registro abaixo
        // falhava CALADO (sem log de erro nenhum) porque o event loop
        // ainda nao existia. esp_netif_init()/esp_event_loop_create_default()
        // sao seguras de chamar de novo depois -- retornam
        // ESP_ERR_INVALID_STATE na segunda vez, que a gente ignora.
        esp_err_t netif_err = esp_netif_init();
        if (netif_err != ESP_OK && netif_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "esp_netif_init falhou: %s", esp_err_to_name(netif_err));
        }
        esp_err_t loop_err = esp_event_loop_create_default();
        if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "esp_event_loop_create_default falhou: %s", esp_err_to_name(loop_err));
        }
        esp_err_t reg_err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &OnGotIp, nullptr, nullptr);
        if (reg_err != ESP_OK) {
            ESP_LOGE(TAG, "Falha ao registrar handler de IP_EVENT_STA_GOT_IP: %s", esp_err_to_name(reg_err));
        }
    }

    virtual Led* GetLed() override {
        static SingleLed led_strip(BUILTIN_LED_GPIO);
        return &led_strip;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    Cst816d* GetTouchpad() {
        return cst816d_;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = power_manager_->GetBatteryLevel();
        return true;
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveLevel(level);
    }
};

DECLARE_BOARD(Spotpear_esp32_s3_lcd_1_54);
