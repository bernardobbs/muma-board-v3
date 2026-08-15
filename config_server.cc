#include "config_server.h"
#include "device_config.h"
#include "routine_engine.h"
#include "tamagotchi_tool.h"
#include "pomodoro_tool.h"
#include "semaphore_tool.h"
#include "child_profile.h"
#include "alarm_tool.h"
#include "breathing_tool.h"
#include "web_assets.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_random.h>
#include <mbedtls/base64.h>
#include <cstring>

#define TAG "ConfigServer"

void ConfigServer::Start(const std::string& admin_password) {
    if (server_ != nullptr) return;
    admin_password_ = admin_password;

    // Um token por boot, nao por request -- simples e suficiente pra um
    // servidor domestico sem sessao de usuario de verdade.
    static const char kHex[] = "0123456789abcdef";
    uint8_t raw[16];
    esp_fill_random(raw, sizeof(raw));
    char token[33];
    for (int i = 0; i < 16; i++) {
        token[i * 2]     = kHex[raw[i] >> 4];
        token[i * 2 + 1] = kHex[raw[i] & 0x0F];
    }
    token[32] = '\0';
    csrf_token_ = token;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 32;      // 22 rotas hoje -- deixa folga pra futuras features
    config.lru_purge_enable = true;
    config.stack_size = 8192;

    if (httpd_start(&server_, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar o servidor HTTP");
        server_ = nullptr;
        return;
    }
    RegisterHandlers();
    ESP_LOGI(TAG, "Servidor de configuracao no ar (porta %d)", config.server_port);
    if (admin_password_.empty())
        ESP_LOGW(TAG, "Sem senha de admin -- /admin ficara BLOQUEADA (403)");
}

void ConfigServer::Stop() {
    if (server_ == nullptr) return;
    httpd_stop(server_);
    server_ = nullptr;
}

// Inicializacao campo a campo em vez de designated initializers --
// evita depender da ordem/versao de C++ do toolchain.
void ConfigServer::Register(const char* uri, httpd_method_t method, esp_err_t (*h)(httpd_req_t*)) {
    httpd_uri_t u;
    memset(&u, 0, sizeof(u));
    u.uri = uri;
    u.method = method;
    u.handler = h;
    u.user_ctx = nullptr;
    esp_err_t err = httpd_register_uri_handler(server_, &u);
    if (err != ESP_OK) ESP_LOGE(TAG, "Falha ao registrar %s (%s)", uri, esp_err_to_name(err));
}

void ConfigServer::RegisterHandlers() {
    // area dela (sem senha)
    Register("/",                  HTTP_GET,  GetIndex);
    Register("/api/routine/today", HTTP_GET,  GetRoutineToday);
    Register("/api/routine/done",  HTTP_POST, PostTaskDone);
    Register("/api/pet",           HTTP_GET,  GetPet);
    Register("/api/pet/choose",    HTTP_POST, PostPetChoose);
    Register("/api/config",        HTTP_GET,  GetConfig);
    Register("/api/config",        HTTP_POST, PostConfig);
    Register("/api/pomodoro",      HTTP_GET,  GetPomodoro);
    Register("/api/pomodoro",      HTTP_POST, PostPomodoro);
    Register("/api/alarms",        HTTP_GET,  GetAlarms);
    Register("/api/alarms/add",    HTTP_POST, PostAlarmAdd);
    Register("/api/alarms/remove", HTTP_POST, PostAlarmRemove);
    Register("/api/alarms/toggle", HTTP_POST, PostAlarmToggle);
    Register("/api/alarms/update", HTTP_POST, PostAlarmUpdate);
    Register("/api/breathing/start", HTTP_POST, PostBreathingStart);
    Register("/api/breathing/stop",  HTTP_POST, PostBreathingStop);
    Register("/api/semaphore",         HTTP_GET,  GetSemaphore);
    Register("/api/semaphore/set",     HTTP_POST, PostSemaphoreSet);
    Register("/api/semaphore/confirm", HTTP_POST, PostSemaphoreConfirm);
    Register("/api/semaphore/cancel",  HTTP_POST, PostSemaphoreCancel);
    // area de voces (com senha)
    Register("/admin",             HTTP_GET,  GetAdminPage);
    Register("/api/admin/routine", HTTP_GET,  GetRoutineDef);
    Register("/api/admin/routine", HTTP_POST, PostRoutineDef);
    Register("/api/admin/config",  HTTP_GET,  GetAdminConfig);
    Register("/api/admin/config",  HTTP_POST, PostAdminConfig);
    Register("/api/admin/csrf",    HTTP_GET,  GetAdminCsrf);
}

// ------------------------------------------------------------- helpers

esp_err_t ConfigServer::SendJson(httpd_req_t* req, const std::string& json) {
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json.c_str());
}

esp_err_t ConfigServer::SendBadRequest(httpd_req_t* req, const char* msg) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    std::string body = std::string("{\"ok\":false,\"erro\":\"") + msg + "\"}";
    return httpd_resp_sendstr(req, body.c_str());
}

bool ConfigServer::ReadBody(httpd_req_t* req, std::string& out, size_t max_len) {
    if (req->content_len == 0 || req->content_len > max_len) return false;
    out.resize(req->content_len);
    size_t received = 0;
    // httpd_req_recv pode voltar parcial -- ler em loop (a versao
    // anterior assumia uma leitura unica e truncava corpos maiores).
    while (received < req->content_len) {
        int r = httpd_req_recv(req, &out[received], req->content_len - received);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) return false;
        received += r;
    }
    out.resize(received);
    return true;
}

bool ConfigServer::RequireAdmin(httpd_req_t* req) {
    auto& self = ConfigServer::GetInstance();

    if (self.admin_password_.empty()) {
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_sendstr(req, "Defina uma senha de admin antes de usar esta area.");
        return false;
    }

    size_t len = httpd_req_get_hdr_value_len(req, "Authorization");
    bool ok = false;
    if (len > 0 && len < 256) {
        std::string header(len + 1, '\0');
        if (httpd_req_get_hdr_value_str(req, "Authorization", &header[0], len + 1) == ESP_OK) {
            static const char* kPrefix = "Basic ";
            if (strncmp(header.c_str(), kPrefix, strlen(kPrefix)) == 0) {
                std::string b64 = header.c_str() + strlen(kPrefix);
                unsigned char decoded[160];
                size_t olen = 0;
                if (mbedtls_base64_decode(decoded, sizeof(decoded) - 1, &olen,
                        (const unsigned char*)b64.data(), b64.size()) == 0) {
                    decoded[olen] = '\0';
                    const char* colon = strchr((const char*)decoded, ':');
                    if (colon != nullptr && self.admin_password_ == (colon + 1)) ok = true;
                }
            }
        }
    }

    if (!ok) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Config\"");
        httpd_resp_sendstr(req, "Acesso restrito.");
        return false;
    }
    return true;
}

bool ConfigServer::RequireCsrf(httpd_req_t* req) {
    auto& self = ConfigServer::GetInstance();
    size_t len = httpd_req_get_hdr_value_len(req, "X-CSRF-Token");
    bool ok = false;
    if (len > 0 && len < 128) {
        std::string token(len + 1, '\0');
        if (httpd_req_get_hdr_value_str(req, "X-CSRF-Token", &token[0], len + 1) == ESP_OK) {
            token.resize(len);
            ok = (token == self.csrf_token_);
        }
    }
    if (!ok) {
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_sendstr(req, "Token CSRF ausente ou invalido.");
        return false;
    }
    return true;
}

// ------------------------------------------------------------- paginas

esp_err_t ConfigServer::GetIndex(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t ConfigServer::GetAdminPage(httpd_req_t* req) {
    if (!RequireAdmin(req)) return ESP_OK;
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, kAdminHtml, HTTPD_RESP_USE_STRLEN);
}

// ------------------------------------------------------------- area dela

esp_err_t ConfigServer::GetRoutineToday(httpd_req_t* req) {
    return SendJson(req, RoutineEngine::GetInstance().TodayJson());
}

esp_err_t ConfigServer::PostTaskDone(httpd_req_t* req) {
    std::string body;
    if (!ReadBody(req, body)) return SendBadRequest(req, "corpo invalido");
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) return SendBadRequest(req, "JSON invalido");
    cJSON* id = cJSON_GetObjectItem(root, "id");
    bool ok = cJSON_IsString(id) && RoutineEngine::GetInstance().MarkDone(id->valuestring);
    cJSON_Delete(root);
    return SendJson(req, ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

esp_err_t ConfigServer::GetPet(httpd_req_t* req) {
    return SendJson(req, Tamagotchi::GetInstance().StatusJson());
}

esp_err_t ConfigServer::PostPetChoose(httpd_req_t* req) {
    std::string body;
    if (!ReadBody(req, body)) return SendBadRequest(req, "corpo invalido");
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) return SendBadRequest(req, "JSON invalido");
    cJSON* id = cJSON_GetObjectItem(root, "id");
    bool ok = cJSON_IsString(id) && Tamagotchi::GetInstance().ChooseSpecies(id->valuestring);
    cJSON_Delete(root);
    return SendJson(req, ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

esp_err_t ConfigServer::GetConfig(httpd_req_t* req) {
    return SendJson(req, DeviceConfig::GetInstance().ToJson());
}

esp_err_t ConfigServer::PostConfig(httpd_req_t* req) {
    std::string body;
    if (!ReadBody(req, body)) return SendBadRequest(req, "corpo invalido");
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) return SendBadRequest(req, "JSON invalido");

    auto& cfg = DeviceConfig::GetInstance();
    cJSON* study = cJSON_GetObjectItem(root, "study_min");
    cJSON* brk   = cJSON_GetObjectItem(root, "break_min");
    if (cJSON_IsNumber(study) && cJSON_IsNumber(brk))
        cfg.set_pomodoro(study->valueint, brk->valueint);   // ja faz clamp na faixa segura

    cJSON* bright = cJSON_GetObjectItem(root, "brightness");
    if (cJSON_IsNumber(bright)) cfg.set_brightness(bright->valueint);

    cJSON* vol = cJSON_GetObjectItem(root, "volume");
    if (cJSON_IsNumber(vol)) cfg.set_volume(vol->valueint);

    // Campos de alerta/semaforo/limiares sao IGNORADOS aqui de proposito:
    // so /api/admin/config muda regra.
    cJSON_Delete(root);
    return SendJson(req, "{\"ok\":true}");
}

esp_err_t ConfigServer::GetPomodoro(httpd_req_t* req) {
    return SendJson(req, PomodoroEngine::GetInstance().StatusJson());
}

esp_err_t ConfigServer::PostPomodoro(httpd_req_t* req) {
    std::string body;
    if (!ReadBody(req, body)) return SendBadRequest(req, "corpo invalido");
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) return SendBadRequest(req, "JSON invalido");
    cJSON* action = cJSON_GetObjectItem(root, "acao");
    std::string a = cJSON_IsString(action) ? action->valuestring : "";
    cJSON_Delete(root);

    auto& p = PomodoroEngine::GetInstance();
    if (a == "iniciar") p.Start();
    else if (a == "pausar") p.Pause();
    else if (a == "retomar") p.Resume();
    else if (a == "parar") p.Stop();
    else return SendBadRequest(req, "acao invalida");

    return SendJson(req, p.StatusJson());
}

esp_err_t ConfigServer::GetAlarms(httpd_req_t* req) {
    return SendJson(req, AlarmEngine::GetInstance().ListJson());
}

esp_err_t ConfigServer::PostAlarmAdd(httpd_req_t* req) {
    std::string body;
    if (!ReadBody(req, body)) return SendBadRequest(req, "corpo invalido");
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) return SendBadRequest(req, "JSON invalido");
    cJSON* hour = cJSON_GetObjectItem(root, "hour");
    cJSON* minute = cJSON_GetObjectItem(root, "minute");
    if (!cJSON_IsNumber(hour) || !cJSON_IsNumber(minute)) {
        cJSON_Delete(root);
        return SendBadRequest(req, "esperado {hour, minute}");
    }
    std::string id = AlarmEngine::GetInstance().AddAlarm(hour->valueint, minute->valueint);
    cJSON_Delete(root);
    return SendJson(req, "{\"ok\":true,\"id\":\"" + id + "\"}");
}

esp_err_t ConfigServer::PostAlarmRemove(httpd_req_t* req) {
    std::string body;
    if (!ReadBody(req, body)) return SendBadRequest(req, "corpo invalido");
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) return SendBadRequest(req, "JSON invalido");
    cJSON* id = cJSON_GetObjectItem(root, "id");
    bool ok = cJSON_IsString(id) && AlarmEngine::GetInstance().RemoveAlarm(id->valuestring);
    cJSON_Delete(root);
    if (!ok) return SendBadRequest(req, "alarme nao encontrado");
    return SendJson(req, "{\"ok\":true}");
}

esp_err_t ConfigServer::PostAlarmToggle(httpd_req_t* req) {
    std::string body;
    if (!ReadBody(req, body)) return SendBadRequest(req, "corpo invalido");
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) return SendBadRequest(req, "JSON invalido");
    cJSON* id = cJSON_GetObjectItem(root, "id");
    cJSON* enabled = cJSON_GetObjectItem(root, "enabled");
    bool ok = cJSON_IsString(id) && cJSON_IsBool(enabled) &&
              AlarmEngine::GetInstance().SetEnabled(id->valuestring, cJSON_IsTrue(enabled));
    cJSON_Delete(root);
    if (!ok) return SendBadRequest(req, "alarme nao encontrado ou corpo invalido");
    return SendJson(req, "{\"ok\":true}");
}

esp_err_t ConfigServer::PostAlarmUpdate(httpd_req_t* req) {
    std::string body;
    if (!ReadBody(req, body)) return SendBadRequest(req, "corpo invalido");
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) return SendBadRequest(req, "JSON invalido");
    cJSON* id = cJSON_GetObjectItem(root, "id");
    cJSON* hour = cJSON_GetObjectItem(root, "hour");
    cJSON* minute = cJSON_GetObjectItem(root, "minute");
    bool ok = cJSON_IsString(id) && cJSON_IsNumber(hour) && cJSON_IsNumber(minute) &&
              AlarmEngine::GetInstance().UpdateAlarm(id->valuestring, hour->valueint, minute->valueint);
    cJSON_Delete(root);
    if (!ok) return SendBadRequest(req, "alarme nao encontrado ou corpo invalido");
    return SendJson(req, "{\"ok\":true}");
}

esp_err_t ConfigServer::PostBreathingStart(httpd_req_t* req) {
    BreathingExercise::GetInstance().Start();
    return SendJson(req, "{\"ok\":true}");
}

esp_err_t ConfigServer::PostBreathingStop(httpd_req_t* req) {
    BreathingExercise::GetInstance().Stop();
    return SendJson(req, "{\"ok\":true}");
}

esp_err_t ConfigServer::GetSemaphore(httpd_req_t* req) {
    return SendJson(req, OverloadSemaphore::GetInstance().StatusJson());
}

esp_err_t ConfigServer::PostSemaphoreSet(httpd_req_t* req) {
    if (!ChildProfile::GetInstance().regulation_tools_enabled())
        return SendBadRequest(req, "ferramenta de regulacao desligada nas configuracoes");
    std::string body;
    if (!ReadBody(req, body)) return SendBadRequest(req, "corpo invalido");
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) return SendBadRequest(req, "JSON invalido");
    cJSON* nivel = cJSON_GetObjectItem(root, "nivel");
    std::string v = cJSON_IsString(nivel) ? nivel->valuestring : "";
    cJSON_Delete(root);

    auto& sem = OverloadSemaphore::GetInstance();
    if (v == "verde") sem.SetLevel(OverloadLevel::VERDE);
    else if (v == "amarelo") sem.SetLevel(OverloadLevel::AMARELO);
    else if (v == "vermelho") sem.SetLevel(OverloadLevel::VERMELHO);
    else return SendBadRequest(req, "nivel invalido -- use verde, amarelo ou vermelho");

    return SendJson(req, sem.StatusJson());
}

esp_err_t ConfigServer::PostSemaphoreConfirm(httpd_req_t* req) {
    if (!ChildProfile::GetInstance().regulation_tools_enabled())
        return SendBadRequest(req, "ferramenta de regulacao desligada nas configuracoes");
    OverloadSemaphore::GetInstance().ConfirmSend();
    return SendJson(req, "{\"ok\":true}");
}

esp_err_t ConfigServer::PostSemaphoreCancel(httpd_req_t* req) {
    if (!ChildProfile::GetInstance().regulation_tools_enabled())
        return SendBadRequest(req, "ferramenta de regulacao desligada nas configuracoes");
    OverloadSemaphore::GetInstance().CancelSend();
    return SendJson(req, "{\"ok\":true}");
}

// ------------------------------------------------------------ area admin

esp_err_t ConfigServer::GetRoutineDef(httpd_req_t* req) {
    if (!RequireAdmin(req)) return ESP_OK;
    return SendJson(req, RoutineEngine::GetInstance().DefinitionJson());
}

esp_err_t ConfigServer::GetAdminCsrf(httpd_req_t* req) {
    if (!RequireAdmin(req)) return ESP_OK;
    auto& self = ConfigServer::GetInstance();
    return SendJson(req, "{\"csrf\":\"" + self.csrf_token_ + "\"}");
}

esp_err_t ConfigServer::PostRoutineDef(httpd_req_t* req) {
    if (!RequireAdmin(req)) return ESP_OK;
    if (!RequireCsrf(req)) return ESP_OK;
    std::string body;
    if (!ReadBody(req, body, 8192)) return SendBadRequest(req, "corpo invalido ou grande demais");
    if (!RoutineEngine::GetInstance().ReplaceDefinition(body))
        return SendBadRequest(req, "JSON invalido -- rotina anterior mantida");
    return SendJson(req, "{\"ok\":true}");
}

esp_err_t ConfigServer::GetAdminConfig(httpd_req_t* req) {
    if (!RequireAdmin(req)) return ESP_OK;
    return SendJson(req, DeviceConfig::GetInstance().ToAdminJson());
}

esp_err_t ConfigServer::PostAdminConfig(httpd_req_t* req) {
    if (!RequireAdmin(req)) return ESP_OK;
    if (!RequireCsrf(req)) return ESP_OK;
    std::string body;
    if (!ReadBody(req, body)) return SendBadRequest(req, "corpo invalido");
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) return SendBadRequest(req, "JSON invalido");

    auto& cfg = DeviceConfig::GetInstance();

    cJSON* cap = cJSON_GetObjectItem(root, "daily_cap");
    cJSON* s2 = cJSON_GetObjectItem(root, "stage2");
    cJSON* s3 = cJSON_GetObjectItem(root, "stage3");
    cJSON* s4 = cJSON_GetObjectItem(root, "stage4");
    if (cJSON_IsNumber(cap) && cJSON_IsNumber(s2) && cJSON_IsNumber(s3) && cJSON_IsNumber(s4))
        cfg.set_tama_rules(cap->valueint, s2->valueint, s3->valueint, s4->valueint);

    cJSON* warn = cJSON_GetObjectItem(root, "warn_sec");
    if (cJSON_IsNumber(warn)) cfg.set_warning_seconds(warn->valueint);

    cJSON* quick_return = cJSON_GetObjectItem(root, "quick_return");
    if (cJSON_IsNumber(quick_return)) cfg.set_quick_return_minutes(quick_return->valueint);

    cJSON* y_on = cJSON_GetObjectItem(root, "y_on");
    cJSON* r_on = cJSON_GetObjectItem(root, "r_on");
    cJSON* y_auto = cJSON_GetObjectItem(root, "y_auto");
    cJSON* r_auto = cJSON_GetObjectItem(root, "r_auto");
    if (cJSON_IsBool(y_on) && cJSON_IsBool(r_on) && cJSON_IsBool(y_auto) && cJSON_IsBool(r_auto))
        cfg.set_alert_rules(cJSON_IsTrue(y_on), cJSON_IsTrue(r_on),
                            cJSON_IsTrue(y_auto), cJSON_IsTrue(r_auto));

    cJSON* srv = cJSON_GetObjectItem(root, "ntfy_srv");
    cJSON* top = cJSON_GetObjectItem(root, "ntfy_top");
    if (cJSON_IsString(srv) && cJSON_IsString(top))
        cfg.set_ntfy(srv->valuestring, top->valuestring);

    cJSON* tz = cJSON_GetObjectItem(root, "tz");
    if (cJSON_IsString(tz)) cfg.set_timezone(tz->valuestring);

    auto& child = ChildProfile::GetInstance();
    cJSON* name = cJSON_GetObjectItem(root, "nome");
    cJSON* bdate = cJSON_GetObjectItem(root, "data_nascimento");
    if (cJSON_IsString(name) && cJSON_IsString(bdate) && name->valuestring[0] != '\0')
        child.Set(name->valuestring, bdate->valuestring);

    cJSON* reg = cJSON_GetObjectItem(root, "regulacao_ativa");
    if (cJSON_IsBool(reg)) child.set_regulation_tools_enabled(cJSON_IsTrue(reg));

    cJSON_Delete(root);
    return SendJson(req, "{\"ok\":true}");
}
