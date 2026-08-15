#pragma once
#include <esp_http_server.h>
#include <string>

// Servidor de configuracao na rede de casa, com DOIS NIVEIS de acesso:
//
//   /       -> pagina DELA, sem senha. Controla COMO usa: bichinho,
//              rotina, pomodoro (faixa segura), brilho, volume.
//              VE o alerta pros pais e em que nivel esta, sem poder
//              mudar -- transparencia, nao vigilancia.
//
//   /admin  -> pagina DE VOCES, com senha. Controla AS REGRAS: rotina,
//              limiares de evolucao, teto diario, semaforo, ntfy, fuso.
//
// Ela nao desliga o alerta de crise nem zera o proprio progresso.
// Voces nao sao chamados pra ela trocar de bichinho.
class ConfigServer {
public:
    static ConfigServer& GetInstance() { static ConfigServer i; return i; }

    // Se a senha vier vazia, /admin responde 403 -- melhor negar do que
    // servir a area de regras sem protecao.
    void Start(const std::string& admin_password);
    void Stop();
    bool IsRunning() const { return server_ != nullptr; }

private:
    ConfigServer() = default;
    void RegisterHandlers();
    void Register(const char* uri, httpd_method_t method, esp_err_t (*h)(httpd_req_t*));

    // --- paginas ---
    static esp_err_t GetIndex(httpd_req_t* req);
    static esp_err_t GetAdminPage(httpd_req_t* req);
    // --- area dela ---
    static esp_err_t GetRoutineToday(httpd_req_t* req);
    static esp_err_t PostTaskDone(httpd_req_t* req);
    static esp_err_t GetPet(httpd_req_t* req);
    static esp_err_t PostPetChoose(httpd_req_t* req);
    static esp_err_t GetConfig(httpd_req_t* req);
    static esp_err_t PostConfig(httpd_req_t* req);
    static esp_err_t GetPomodoro(httpd_req_t* req);
    static esp_err_t PostPomodoro(httpd_req_t* req);
    // Alarme: varios, controlados pela propria crianca -- por isso mora
    // aqui do lado dela, sem senha, e nao na "area de voces" abaixo.
    static esp_err_t GetAlarms(httpd_req_t* req);
    static esp_err_t PostAlarmAdd(httpd_req_t* req);
    static esp_err_t PostAlarmRemove(httpd_req_t* req);
    static esp_err_t PostAlarmToggle(httpd_req_t* req);
    static esp_err_t PostAlarmUpdate(httpd_req_t* req);
    // --- area de voces ---
    static esp_err_t GetRoutineDef(httpd_req_t* req);
    static esp_err_t PostRoutineDef(httpd_req_t* req);
    static esp_err_t GetAdminConfig(httpd_req_t* req);
    static esp_err_t PostAdminConfig(httpd_req_t* req);
    static esp_err_t GetAdminCsrf(httpd_req_t* req);

    static bool ReadBody(httpd_req_t* req, std::string& out, size_t max_len = 4096);
    static bool RequireAdmin(httpd_req_t* req);
    // So faz sentido chamar DEPOIS de RequireAdmin: com Basic Auth, o
    // navegador reenvia as credenciais em qualquer POST pro mesmo site,
    // entao uma pagina maliciosa aberta no mesmo navegador conseguiria
    // disparar POSTs de admin sem saber a senha -- exceto que ela nao
    // sabe o token, que so sai em resposta a uma chamada JS autenticada.
    static bool RequireCsrf(httpd_req_t* req);
    static esp_err_t SendJson(httpd_req_t* req, const std::string& json);
    static esp_err_t SendBadRequest(httpd_req_t* req, const char* msg);

    httpd_handle_t server_ = nullptr;
    std::string admin_password_;
    std::string csrf_token_;
};
