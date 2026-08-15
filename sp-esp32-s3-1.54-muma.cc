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
#include <ctime>
#include <esp_netif.h>

// --- companheiro afetivo: features de familia (nao faziam parte do
// board original) ---
#include "child_profile.h"
#include "device_config.h"
#include "pomodoro_tool.h"
#include "tamagotchi_tool.h"
#include "routine_engine.h"
#include "semaphore_tool.h"
#include "alarm_tool.h"
#include "breathing_tool.h"
#include "mcp_tools.h"
#include "config_server.h"
#include "pet_emoji_collection.h"
#include "pomodoro_tomato.h"
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
    lv_obj_t* pomodoro_label_ = nullptr;   // relogio digital do cronometro, criado sob demanda
    lv_obj_t* pomodoro_tomato_ = nullptr;  // imagem do tomate (pomodoro_tomato.png) ao fundo do relogio
    lv_obj_t* stage_badge_ = nullptr;      // selo de estagio, criado sob demanda
    lv_obj_t* alarm_banner_ = nullptr;     // tela cheia enquanto o alarme toca, criada sob demanda
    lv_obj_t* qr_overlay_ = nullptr;       // tela cheia com QR code, criada sob demanda (toggle no long-press)
    lv_obj_t* qr_code_ = nullptr;
    lv_obj_t* qr_label_ = nullptr;
    lv_obj_t* breathing_overlay_ = nullptr;  // "cantinho da calma", criado sob demanda
    lv_obj_t* breathing_circle_ = nullptr;
    lv_obj_t* breathing_label_ = nullptr;
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

    // Cronometro do pomodoro no MEIO da tela, sobre um "tomate" --
    // pomodoro_tomato.png (gerado por scripts/gen_pomodoro_tomato.py),
    // decodificado como PNG normal via LvglRawImage, mesmo mecanismo do
    // pacote de emoji padrao. Como o emoji de humor do bichinho tambem
    // fica no CENTER (widgets padrao
    // do LcdDisplay: top_bar_/status_bar_ em TOP_MID, emoji em CENTER,
    // bottom_bar_ em BOTTOM_MID), o tomate+relogio SOBREPOEM o rosto do
    // bichinho só enquanto o pomodoro estiver rodando -- escondidos
    // (LV_OBJ_FLAG_HIDDEN) o resto do tempo, revelando o rosto de novo.
    // Criados na primeira chamada, nao no construtor: SetupUI() do
    // display roda depois do construtor do board (Application::Initialize()
    // chama os dois em sequencia), entao lv_screen_active() ainda nao
    // existiria se a gente tentasse criar isto antes.
    void UpdatePomodoroLabel(int seconds_remaining) {
        DisplayLockGuard lock(display_);
        if (pomodoro_tomato_ == nullptr) {
            // Imagem real do tomate (pomodoro_tomato.png, embutida por
            // scripts/gen_pomodoro_tomato.py) -- LvglRawImage decodifica
            // via LV_COLOR_FORMAT_RAW_ALPHA, mesmo mecanismo que o
            // pacote de emoji padrao usa pros PNGs dele.
            pomodoro_tomato_ = lv_image_create(lv_screen_active());
            lv_image_set_src(pomodoro_tomato_, GetPomodoroTomatoImage()->image_dsc());
            lv_obj_align(pomodoro_tomato_, LV_ALIGN_CENTER, 0, 0);
            lv_obj_add_flag(pomodoro_tomato_, LV_OBJ_FLAG_HIDDEN);

            // Relogio digital, criado DEPOIS do tomate -> desenhado por
            // cima dele (ordem dos filhos = ordem de desenho no LVGL).
            pomodoro_label_ = lv_label_create(lv_screen_active());
            lv_obj_set_style_text_color(pomodoro_label_, lv_color_white(), 0);
            // Sem fonte grande de digitos embutida no firmware ainda --
            // aumenta o texto normal via transform em vez de depender
            // de um asset de fonte novo (fica um pouco mais "pixelado"
            // que uma fonte nativa nesse tamanho, mas funciona sem
            // precisar gerar/embutir nada).
            lv_obj_set_style_transform_scale_x(pomodoro_label_, 640, 0);  // 2.5x -- ficou pequeno em 1.5x
            lv_obj_set_style_transform_scale_y(pomodoro_label_, 640, 0);
            lv_obj_align(pomodoro_label_, LV_ALIGN_CENTER, 0, 0);
            lv_obj_add_flag(pomodoro_label_, LV_OBJ_FLAG_HIDDEN);
        }
        if (seconds_remaining <= 0) {
            lv_obj_add_flag(pomodoro_tomato_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(pomodoro_label_, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        lv_obj_clear_flag(pomodoro_tomato_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pomodoro_label_, LV_OBJ_FLAG_HIDDEN);
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
    // e BOTTOM_MID, o tomate+relogio do pomodoro ficam no CENTER).
    void UpdateStageBadge() {
        DisplayLockGuard lock(display_);
        if (stage_badge_ == nullptr) {
            stage_badge_ = lv_label_create(lv_screen_active());
            lv_obj_align(stage_badge_, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
        }
        lv_label_set_text(stage_badge_, Tamagotchi::GetInstance().StageName().c_str());
    }

    // Alarme: cobre a tela toda enquanto estiver tocando (AlarmEngine::
    // firing() == true) -- some com qualquer outra coisa na tela ate
    // desligar, de proposito (e pra chamar atencao mesmo). Desligar e
    // via boot_button_ (ver InitializeButtons) ou por voz
    // (self.alarm.dismiss), ou sozinho depois de kMaxRingSeconds.
    void ShowAlarmBanner() {
        DisplayLockGuard lock(display_);
        if (alarm_banner_ == nullptr) {
            alarm_banner_ = lv_obj_create(lv_screen_active());
            lv_obj_remove_style_all(alarm_banner_);
            lv_obj_set_size(alarm_banner_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
            lv_obj_set_style_bg_color(alarm_banner_, lv_color_hex(0xFF9800), 0);
            lv_obj_set_style_bg_opa(alarm_banner_, LV_OPA_COVER, 0);
            lv_obj_center(alarm_banner_);

            lv_obj_t* label = lv_label_create(alarm_banner_);
            lv_label_set_text(label, "ALARME!\n\nToque no botao\npara desligar");
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
            lv_obj_center(label);
        }
        lv_obj_clear_flag(alarm_banner_, LV_OBJ_FLAG_HIDDEN);
    }

    void HideAlarmBanner() {
        DisplayLockGuard lock(display_);
        if (alarm_banner_ != nullptr) lv_obj_add_flag(alarm_banner_, LV_OBJ_FLAG_HIDDEN);
    }

    // IP da estacao Wi-Fi -- direto do esp_netif, sem depender de nenhum
    // getter do componente de wifi (nao achamos um exposto). Vazio se a
    // rede ainda nao tiver IP (ex: antes de conectar).
    static std::string GetDeviceIpAddress() {
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif == nullptr) return "";
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK || ip_info.ip.addr == 0) return "";
        char buf[16];
        snprintf(buf, sizeof(buf), IPSTR, IP2STR(&ip_info.ip));
        return std::string(buf);
    }

    // QR code com a URL da pagina dela ("/", sem senha) -- long press no
    // boot_button_ mostra/esconde (ver InitializeButtons). Cobre a tela
    // toda pra ficar grande o suficiente pra escanear de perto.
    void ToggleQrCode() {
        DisplayLockGuard lock(display_);
        if (qr_overlay_ != nullptr && !lv_obj_has_flag(qr_overlay_, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(qr_overlay_, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        std::string ip = GetDeviceIpAddress();
        if (ip.empty()) return;  // sem rede ainda -- nada pra mostrar
        std::string url = "http://" + ip + "/";

        if (qr_overlay_ == nullptr) {
            qr_overlay_ = lv_obj_create(lv_screen_active());
            lv_obj_remove_style_all(qr_overlay_);
            lv_obj_set_size(qr_overlay_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
            lv_obj_set_style_bg_color(qr_overlay_, lv_color_white(), 0);
            lv_obj_set_style_bg_opa(qr_overlay_, LV_OPA_COVER, 0);
            lv_obj_center(qr_overlay_);

            qr_code_ = lv_qrcode_create(qr_overlay_);
            lv_qrcode_set_size(qr_code_, 170);
            lv_qrcode_set_dark_color(qr_code_, lv_color_black());
            lv_qrcode_set_light_color(qr_code_, lv_color_white());
            lv_obj_align(qr_code_, LV_ALIGN_CENTER, 0, -14);

            qr_label_ = lv_label_create(qr_overlay_);
            lv_obj_set_style_text_color(qr_label_, lv_color_black(), 0);
            lv_obj_align(qr_label_, LV_ALIGN_BOTTOM_MID, 0, -8);
        }
        lv_qrcode_update(qr_code_, url.c_str(), url.size());
        lv_label_set_text(qr_label_, url.c_str());
        lv_obj_clear_flag(qr_overlay_, LV_OBJ_FLAG_HIDDEN);
    }

    // "Cantinho da calma": circulo que cresce/encolhe seguindo
    // BreathingExercise::SetOnTick. Fundo azul claro (cor calma, nao a
    // paleta padrao) pra ficar visualmente distinto de qualquer outro
    // overlay. Tamanho MIN/MAX escolhido pra caber com folga nos 240x240.
    static constexpr int kBreathMinSize = 60;
    static constexpr int kBreathMaxSize = 170;

    void UpdateBreathingOverlay(BreathPhase phase, float progress) {
        DisplayLockGuard lock(display_);
        if (breathing_overlay_ == nullptr) {
            breathing_overlay_ = lv_obj_create(lv_screen_active());
            lv_obj_remove_style_all(breathing_overlay_);
            lv_obj_set_size(breathing_overlay_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
            lv_obj_set_style_bg_color(breathing_overlay_, lv_color_hex(0xE3F2FD), 0);
            lv_obj_set_style_bg_opa(breathing_overlay_, LV_OPA_COVER, 0);
            lv_obj_center(breathing_overlay_);

            breathing_circle_ = lv_obj_create(breathing_overlay_);
            lv_obj_remove_style_all(breathing_circle_);
            lv_obj_set_style_radius(breathing_circle_, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(breathing_circle_, lv_color_hex(0x64B5F6), 0);
            lv_obj_set_style_bg_opa(breathing_circle_, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(breathing_circle_, 0, 0);
            lv_obj_center(breathing_circle_);

            breathing_label_ = lv_label_create(breathing_overlay_);
            lv_obj_set_style_text_color(breathing_label_, lv_color_hex(0x0D47A1), 0);
            lv_obj_align(breathing_label_, LV_ALIGN_BOTTOM_MID, 0, -18);
        }

        int size;
        const char* text;
        switch (phase) {
            case BreathPhase::INSPIRE:
                size = kBreathMinSize + (int)((kBreathMaxSize - kBreathMinSize) * progress);
                text = "Inspire...";
                break;
            case BreathPhase::SEGURA:
                size = kBreathMaxSize;
                text = "Segure...";
                break;
            default:  // SOLTA
                size = kBreathMaxSize - (int)((kBreathMaxSize - kBreathMinSize) * progress);
                text = "Solte...";
                break;
        }
        lv_obj_set_size(breathing_circle_, size, size);
        lv_label_set_text(breathing_label_, text);
        lv_obj_clear_flag(breathing_overlay_, LV_OBJ_FLAG_HIDDEN);
    }

    void HideBreathingOverlay() {
        DisplayLockGuard lock(display_);
        if (breathing_overlay_ != nullptr) lv_obj_add_flag(breathing_overlay_, LV_OBJ_FLAG_HIDDEN);
    }

    // ConfigServer precisa de rede -- so sobe depois que o Wi-Fi conectar
    // e a ativacao com o backend terminar.
    //
    // Tentativa 1 (SetNetworkEventCallback): Application::Initialize()
    // TAMBEM chama board.SetNetworkEventCallback(...) depois do
    // construtor do board rodar, pra notificacoes de UI -- e como e um
    // unico std::function (nao uma lista), sobrescrevia silenciosamente
    // a nossa. Confirmado em log real (ConfigServer nunca subia).
    //
    // Tentativa 2 (esp_event_handler_instance_register no IP_EVENT):
    // registro nao dava erro nenhum, mas o callback tambem nunca
    // disparava -- provavelmente alguma sutileza de timing do event
    // loop padrao do ESP-IDF que nao deu pra confirmar so lendo log.
    //
    // Solucao atual: polling simples, sem depender de nenhum mecanismo
    // de callback/evento assincrono. Um timer periodico checa
    // Application::GetDeviceState() (API que este proprio arquivo ja
    // usa em InitializeButtons()) ate ver kDeviceStateIdle -- estado
    // que so acontece depois que Wi-Fi conectou E a ativacao com o
    // backend (OTA + MQTT) terminou, confirmado no log:
    // "StateMachine: State: activating -> idle".
    esp_timer_handle_t network_check_timer_ = nullptr;

    static void CheckNetworkReady(void* arg) {
        // Log só no primeiro tick, pra confirmar que o timer de fato
        // dispara -- sem isso, um timer que nunca roda e um timer que
        // roda mas nunca ve o estado certo ficam indistinguiveis no log.
        static bool logged_first_tick = false;
        if (!logged_first_tick) {
            ESP_LOGI(TAG, "CheckNetworkReady: primeiro tick, estado atual = %d (idle = %d)",
                     (int)Application::GetInstance().GetDeviceState(), (int)kDeviceStateIdle);
            logged_first_tick = true;
        }
        auto& board = (Spotpear_esp32_s3_lcd_1_54&)Board::GetInstance();
        if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) return;
        ESP_LOGI(TAG, "CheckNetworkReady: estado idle detectado, subindo ConfigServer");
        ConfigServer::GetInstance().Start(FAMILY_ADMIN_PASSWORD);
        board.UpdateStageBadge();  // mostra o estagio ja salvo, sem esperar a proxima evolucao

        // Diagnostico do horario: este fork NAO usa NTP -- o relogio vem
        // de "server_time" (timestamp + timezone_offset) que o backend
        // devolve na ativacao (ver Ota::CheckVersion em main/ota.cc,
        // settimeofday()). Risco real: se o servidor JA aplicar o
        // timezone_offset no timestamp, e a nossa propria TZ (ver
        // DeviceConfig::ApplyTimezone) aplicar de novo via localtime_r,
        // o horario fica deslocado em dobro. So compilando e comparando
        // com o relogio real dá pra confirmar -- por isso este log.
        time_t now = time(nullptr);
        struct tm ti;
        localtime_r(&now, &ti);
        char time_buf[32];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &ti);
        ESP_LOGI(TAG, "Horario local calculado pelo aparelho: %s -- compare com o relogio real",
                 time_buf);

        esp_timer_stop(board.network_check_timer_);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            // Alarme tocando tem prioridade sobre tudo -- botao so desliga
            // ele, nao entra em modo de configuracao nem alterna o chat.
            if (AlarmEngine::GetInstance().firing()) {
                AlarmEngine::GetInstance().Dismiss();
                return;
            }
            // Cantinho da calma ativo -- botao so para o exercicio, nao
            // alterna o chat (senao nao teria como sair sem falar).
            if (BreathingExercise::GetInstance().active()) {
                BreathingExercise::GetInstance().Stop();
                return;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        // Segurar o botao mostra/esconde um QR code com a URL da pagina
        // dela ("/"), pra escanear com o celular sem digitar o IP. Se o
        // alarme estiver tocando, o long press so desliga ele tambem --
        // nao teria como ver o QR por cima do aviso mesmo.
        boot_button_.OnLongPress([this]() {
            if (AlarmEngine::GetInstance().firing()) {
                AlarmEngine::GetInstance().Dismiss();
                return;
            }
            ToggleQrCode();
        });
    }

    // --- companheiro afetivo -------------------------------------------
    // Tudo que NAO depende de rede: perfil, regras, engines e as tools
    // MCP. O ConfigServer entra separado, via polling em CheckNetworkReady(),
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
        AlarmEngine::GetInstance().Initialize();

        // Alarme: tela cheia + som ao disparar, som repetido enquanto
        // toca (self.alarm.dismiss ou o boot_button_ desligam, ver
        // InitializeButtons), tela some quando desligar.
        AlarmEngine::GetInstance().SetOnFired([this]() {
            ShowAlarmBanner();
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_EXCLAMATION);
        });
        AlarmEngine::GetInstance().SetOnRingTick([]() {
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_EXCLAMATION);
        });
        AlarmEngine::GetInstance().SetOnDismissed([this]() { HideAlarmBanner(); });

        // Cantinho da calma: circulo crescendo/encolhendo na tela,
        // acionado pela propria crianca (botao em "/" ou voz).
        BreathingExercise::GetInstance().Initialize();
        BreathingExercise::GetInstance().SetOnTick([this](BreathPhase phase, float progress) {
            UpdateBreathingOverlay(phase, progress);
        });
        BreathingExercise::GetInstance().SetOnStopped([this]() { HideBreathingOverlay(); });

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
        // e mostrado separado, em CheckNetworkReady()
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

        // ConfigServer precisa de rede -- polling em vez de
        // callback/evento assincrono (ver comentario acima de
        // CheckNetworkReady() pro historico de por que).
        esp_timer_create_args_t network_timer_args = {};
        network_timer_args.callback = &CheckNetworkReady;
        network_timer_args.arg = nullptr;
        network_timer_args.dispatch_method = ESP_TIMER_TASK;
        network_timer_args.name = "network_check";
        esp_err_t create_err = esp_timer_create(&network_timer_args, &network_check_timer_);
        if (create_err != ESP_OK) {
            ESP_LOGE(TAG, "esp_timer_create (network_check) falhou: %s", esp_err_to_name(create_err));
        }
        esp_err_t start_err = esp_timer_start_periodic(network_check_timer_, 1000000);  // checa a cada 1s
        if (start_err != ESP_OK) {
            ESP_LOGE(TAG, "esp_timer_start_periodic (network_check) falhou: %s", esp_err_to_name(start_err));
        }
        // Log incondicional (nao so em erro) -- prova que este bloco de
        // codigo de fato executou nesta build, independente do timer
        // depois disparar ou nao.
        ESP_LOGI(TAG, "network_check_timer_ criado (handle=%p) create_err=%s start_err=%s",
                 (void*)network_check_timer_, esp_err_to_name(create_err), esp_err_to_name(start_err));
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
