#pragma once
#include <string>

// Camada de notificacao -- desacoplada de tudo. Se nao houver rede no
// momento, simplesmente nao envia: NUNCA trava nem interfere no uso
// local (semaforo, rotina e pomodoro seguem funcionando offline).
namespace notifications {

// priority 1..5 (5 = urgente, faz som/vibracao no celular)
bool SendNtfy(const std::string& title, const std::string& message, int priority = 3);

}
