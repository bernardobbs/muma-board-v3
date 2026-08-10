# AVISO: este arquivo NAO reflete como o build real do xiaozhi-esp32
# funciona (ver item #3 do plano de integracao) -- o projeto base usa
# um idf_component_register UNICO pro component "main" inteiro, e cada
# board so tem seus .cc/.c capturados por um file(GLOB) automatico em
# main/CMakeLists.txt. Nao ha idf_component_register por board.
# Mantido por enquanto so como registro de quais REQUIRES ESP-IDF esse
# conjunto de arquivos precisa; sera substituido por uma nota de
# integracao quando o item #3 for resolvido.
#
# HTML das paginas: NAO usa mais EMBED_TXTFILES (ver web_assets.h) --
# vira string C++ em web_assets.cc, gerado por scripts/gen_web_assets.py.
#
# GIFs (quando existirem): usar EMBED_FILES (binario, sem terminador
# nulo automatico -- diferente de EMBED_TXTFILES).

idf_component_register(
    SRCS
        "sp-esp32-s3-1.54-muma.cc"
        "child_profile.cc"
        "device_config.cc"
        "pomodoro_tool.cc"
        "tamagotchi_tool.cc"
        "routine_engine.cc"
        "semaphore_tool.cc"
        "notifications.cc"
        "mcp_tools.cc"
        "config_server.cc"
        "web_assets.cc"
        # "pet_emoji_collection.cc"   # descomentar se um dia quisermos GIFs customizados
    INCLUDE_DIRS "."
    REQUIRES
        esp_http_server
        esp_http_client
        esp-tls
        mbedtls
        json
        nvs_flash
    # EMBED_FILES
    #     "assets/lobo_neutro.gif"
    #     "assets/lobo_focado.gif"
    #     "assets/lobo_aviso.gif"
    #     "assets/lobo_brincalhao.gif"
    #     "assets/lobo_comemorando.gif"
)
