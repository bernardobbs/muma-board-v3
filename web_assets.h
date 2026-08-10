#pragma once

// HTML das duas paginas, como string C++ em vez de EMBED_TXTFILES.
//
// O build real do xiaozhi-esp32 nao chama idf_component_register por
// board -- e um unico component "main" pro projeto inteiro, e cada
// board só tem seus .cc/.c capturados por um file(GLOB) automatico
// (ver main/CMakeLists.txt). Nao existe gancho por-board pra
// EMBED_TXTFILES, entao o HTML vira string aqui: fica dentro de um
// .cc normal, que O GLOB JA PEGA, sem precisar tocar no CMakeLists.txt
// do projeto base.
extern const char kIndexHtml[];
extern const char kAdminHtml[];
