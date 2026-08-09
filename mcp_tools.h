#pragma once

// Registra todas as ferramentas MCP (pomodoro, bichinho, rotina,
// semaforo) e liga os ganchos entre elas. Fica separado do arquivo do
// board pra nao inchar o construtor -- o board so chama
// mcp_tools::RegisterAll().
namespace mcp_tools {
void RegisterAll();
}
