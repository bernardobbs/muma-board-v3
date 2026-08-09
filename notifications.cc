#include "notifications.h"
#include "device_config.h"

#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>

#define TAG "Notify"

namespace notifications {

bool SendNtfy(const std::string& title, const std::string& message, int priority) {
    auto& cfg = DeviceConfig::GetInstance();
    if (cfg.ntfy_topic().empty()) {
        ESP_LOGW(TAG, "Topico ntfy nao configurado -- nada enviado");
        return false;
    }

    std::string url = cfg.ntfy_server() + "/" + cfg.ntfy_topic();

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 8000;
    config.crt_bundle_attach = esp_crt_bundle_attach;   // valida o TLS

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(TAG, "Falha ao criar cliente HTTP");
        return false;
    }

    esp_http_client_set_header(client, "Title", title.c_str());
    std::string prio = std::to_string(priority);
    esp_http_client_set_header(client, "Priority", prio.c_str());
    esp_http_client_set_post_field(client, message.c_str(), message.size());

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Sem rede ou falha no envio (%s) -- seguindo offline", esp_err_to_name(err));
        return false;
    }
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "ntfy respondeu HTTP %d", status);
        return false;
    }
    ESP_LOGI(TAG, "Alerta enviado (HTTP %d)", status);
    return true;
}

}
