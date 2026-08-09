#pragma once

// Rascunho inicial UNICO e neutro -- usado so na primeira inicializacao
// (ou se o que estiver salvo for invalido). Nao e mais especifico de
// uma crianca; a ideia e dar um ponto de partida generico que os
// responsaveis ajustam pela pagina /admin pro caso real deles.
//
// days: bitmask, bit0=Domingo ... bit6=Sabado.
//   127 = todos os dias, 62 = seg-sex, 65 = sab+dom

static const char* kRoutineDefaultGeneric = R"JSON({"blocks":[
{"name":"Manha","tasks":[
 {"id":"m_acordar","label":"Acordar","days":62,"points":1},
 {"id":"m_higiene","label":"Higiene (escovar dentes, etc)","days":127,"points":1},
 {"id":"m_cafe","label":"Tomar cafe da manha","days":127,"points":1},
 {"id":"m_sair","label":"Sair pra escola","days":62,"points":1}]},
{"name":"Tarde","tasks":[
 {"id":"t_tarefas","label":"Tarefas da escola","days":62,"points":1},
 {"id":"t_banho","label":"Tomar banho","days":127,"points":1}]},
{"name":"Noite","tasks":[
 {"id":"n_janta","label":"Jantar","days":127,"points":1},
 {"id":"n_estudo","label":"Bloco de estudo (pomodoro)","days":62,"points":1},
 {"id":"n_sono","label":"Ritual do sono","days":127,"points":1}]}
]})JSON";
