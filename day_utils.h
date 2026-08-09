#pragma once
#include <time.h>
#include <cstdint>

// A logica de "que dia e hoje" estava duplicada em 3 lugares no
// tamagotchi e em 3 na rotina, cada uma com sua propria copia do
// piso de sanidade. Centralizado aqui.
namespace day_utils {

// Piso de sanidade: antes do NTP sincronizar, time() volta perto de 1970.
// 1672531200 = 2023-01-01.
static constexpr time_t kMinValidEpoch = 1672531200;

inline bool ClockIsSynced() { return time(nullptr) >= kMinValidEpoch; }

// Dias desde epoch no fuso LOCAL. Retorna 0 se o relogio ainda nao
// sincronizou -- 0 e tratado como "desconhecido" por quem chama, nunca
// como um dia valido.
inline int32_t TodayStamp() {
    time_t now = time(nullptr);
    if (now < kMinValidEpoch) return 0;
    struct tm ti;
    localtime_r(&now, &ti);
    struct tm midnight = ti;
    midnight.tm_hour = 0;
    midnight.tm_min = 0;
    midnight.tm_sec = 0;
    return (int32_t)(mktime(&midnight) / 86400);
}

// 0=Domingo .. 6=Sabado; -1 se o relogio ainda nao sincronizou.
inline int Weekday() {
    time_t now = time(nullptr);
    if (now < kMinValidEpoch) return -1;
    struct tm ti;
    localtime_r(&now, &ti);
    return ti.tm_wday;
}

}  // namespace day_utils
