# CMakeLists.txt do board sp-esp32-s3-1.54-muma
#
# ATENCAO ao EMBED_TXTFILES vs EMBED_FILES:
#   - HTML  -> EMBED_TXTFILES (o IDF acrescenta o terminador nulo, que
#              e o que permite usar HTTPD_RESP_USE_STRLEN)
#   - GIF   -> EMBED_FILES (binario, sem terminador)
# Trocar os dois de lugar quebra em silencio: o HTML perde o ultimo byte
# ou o GIF ganha um byte a mais.

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
        # "pet_emoji_collection.cc"   # descomentar quando os GIFs existirem
    INCLUDE_DIRS "."
    REQUIRES
        esp_http_server
        esp_http_client
        esp-tls
        mbedtls
        json
        nvs_flash
    EMBED_TXTFILES
        "www/index.html"
        "www/admin.html"
    # EMBED_FILES
    #     "assets/lobo_neutro.gif"
    #     "assets/lobo_focado.gif"
    #     "assets/lobo_aviso.gif"
    #     "assets/lobo_brincalhao.gif"
    #     "assets/lobo_comemorando.gif"
)
