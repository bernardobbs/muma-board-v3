#include "web_assets.h"

// GERADO por scripts/gen_web_assets.py -- nao editar a mao.
// Fonte real: www/index.html e www/admin.html.

const char kIndexHtml[] = R"MUMA_HTML(
<!doctype html><html lang="pt-BR"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Meu companheiro</title><style>
:root{--bg:#f4f1ea;--card:#fff;--ink:#23211c;--soft:#6f6a60;--ac:#4a9e8f;--line:#e2ddd2}
*{box-sizing:border-box}body{margin:0;padding:18px 14px 40px;font:15px/1.5 system-ui,sans-serif;background:var(--bg);color:var(--ink)}
h1{font-size:20px;margin:0 0 2px}h2{font-size:13px;margin:0 0 10px;color:var(--soft);text-transform:uppercase;letter-spacing:.06em}
.sub{color:var(--soft);font-size:13px;margin:0 0 18px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-bottom:14px}
.pet{display:flex;align-items:center;gap:14px}
.pet .big{font-size:36px}.pet b{font-size:17px}.pet small{display:block;color:var(--soft)}
.bar{height:7px;background:var(--line);border-radius:99px;overflow:hidden;margin-top:8px}
.bar i{display:block;height:100%;background:var(--ac);transition:width .4s}
.timer{font:600 34px/1 ui-monospace,monospace;text-align:center;letter-spacing:.02em}
.tstate{text-align:center;color:var(--soft);font-size:13px;margin-top:6px}
.btns{display:flex;gap:8px;margin-top:14px}
.btns button{flex:1;padding:11px;border:1px solid var(--line);background:#fff;border-radius:10px;font:inherit;cursor:pointer}
.btns button.pri{background:var(--ac);border-color:var(--ac);color:#fff;font-weight:600}
.task{display:flex;align-items:center;gap:10px;padding:9px 0;border-bottom:1px solid var(--line);cursor:pointer}
.task:last-child{border:0}
.task .box{width:20px;height:20px;border:2px solid var(--line);border-radius:6px;flex:0 0 auto;display:grid;place-items:center;font-size:12px}
.task.done .box{background:var(--ac);border-color:var(--ac);color:#fff}
.task.done span:last-child{opacity:.45;text-decoration:line-through}
.blk{font-size:12px;color:var(--soft);text-transform:uppercase;letter-spacing:.06em;margin:14px 0 4px}
.opts{display:flex;flex-wrap:wrap;gap:8px}
.opt{border:1.5px solid var(--line);background:#fff;border-radius:10px;padding:9px 13px;cursor:pointer;font:inherit}
.opt.sel{border-color:var(--ac);background:#eef7f5;font-weight:600}
label{display:block;font-size:13px;color:var(--soft);margin:12px 0 4px}
input[type=range]{width:100%}.val{font-weight:600;color:var(--ink)}
.note{background:#fbf6e8;border:1px solid #ecdcb4;border-radius:10px;padding:11px 13px;font-size:13px;color:#6b5a2e}
button.save{width:100%;padding:12px;border:0;border-radius:10px;background:var(--ink);color:#fff;font:inherit;font-weight:600;cursor:pointer;margin-top:14px}
.ok{color:var(--ac);font-size:13px;text-align:center;margin-top:8px;min-height:18px}
.alarm{display:flex;align-items:center;gap:10px;padding:9px 0;border-bottom:1px solid var(--line)}
.alarm:last-child{border:0}
.alarm .hhmm{flex:1;font:600 17px ui-monospace,monospace}
.alarm .del{background:none;border:0;color:#a6402f;cursor:pointer;font-size:19px;padding:0 4px;line-height:1}
.addrow{display:flex;gap:8px;margin-top:10px}
.addrow input{flex:1}
.addrow button{flex:0 0 auto;padding:9px 14px;border:1px dashed var(--line);background:#faf5ea;border-radius:10px;cursor:pointer;font:inherit;color:var(--soft)}
.adminlink{text-align:center;margin-top:22px}
.adminlink a{color:var(--soft);font-size:12px;text-decoration:none}
</style></head><body>

<p class="sub" id="greeting" style="margin-bottom:2px">Oi!</p>
<h1 id="petName">Carregando…</h1>
<p class="sub">Esta página é sua. Aqui você escolhe como quer usar.</p>

<div class="card pet">
  <div class="big" id="petIcon">🥚</div>
  <div style="flex:1">
    <b id="petStage">—</b><small id="petPts">—</small>
    <div class="bar"><i id="petBar" style="width:0%"></i></div>
  </div>
</div>

<div class="card">
  <h2>Pomodoro</h2>
  <div class="timer" id="timer">--:--</div>
  <div class="tstate" id="tstate">parado</div>
  <div class="btns">
    <button class="pri" id="bStart">Começar</button>
    <button id="bPause">Pausar</button>
    <button id="bStop">Parar</button>
  </div>
</div>

<div class="card">
  <h2>Cantinho da calma</h2>
  <p class="sub" style="margin:0 0 10px">Um exercício de respiração guiada, pra quando precisar se calmar.</p>
  <button class="save" id="breathBtn" style="margin-top:0">Começar a respirar</button>
</div>

<div class="card">
  <h2>Meu companheiro</h2>
  <div class="opts" id="species"></div>
</div>

<div class="card">
  <h2>Minha rotina de hoje</h2>
  <div id="routine">Carregando…</div>
</div>

<div class="card">
  <h2>Meus alarmes</h2>
  <div id="alarms">Carregando…</div>
  <div class="addrow">
    <input type="time" id="newAlarmTime" value="07:00">
    <button id="addAlarm">+ adicionar</button>
  </div>
</div>

<div class="card">
  <h2>Meus ajustes</h2>
  <label>Tempo de foco: <span class="val" id="vStudy">–</span> min</label>
  <input type="range" id="study" step="5">
  <label>Tempo de pausa: <span class="val" id="vBreak">–</span> min</label>
  <input type="range" id="brk" step="1">
  <label>Brilho da tela: <span class="val" id="vBright">–</span>%</label>
  <input type="range" id="bright" min="10" max="100" step="5">
  <label>Volume: <span class="val" id="vVol">–</span>%</label>
  <input type="range" id="vol" min="0" max="100" step="5">
  <button class="save" id="save">Salvar</button>
  <div class="ok" id="okMsg"></div>
</div>

<div class="card" id="semCard" style="display:none">
  <h2>Semáforo</h2>
  <p class="sub" style="margin:0 0 10px">Como você está se sentindo agora?</p>
  <div class="btns">
    <button id="semVerde">🟢 Verde</button>
    <button id="semAmarelo">🟡 Amarelo</button>
    <button id="semVermelho">🔴 Vermelho</button>
  </div>
  <div class="note" id="semPending" style="display:none;margin-top:12px">
    Quer avisar seus pais sobre isso?
    <div class="btns" style="margin-top:8px">
      <button class="pri" id="semConfirm">Sim, avisar</button>
      <button id="semCancel">Não, obrigado</button>
    </div>
  </div>
</div>

<div class="card">
  <h2>Aviso pros seus pais</h2>
  <div class="note" id="alertNote">Carregando…</div>
</div>

<script>
const ICON={Ovo:"🥚",Filhote:"🐣",Jovem:"🧒",Forte:"🌟"};
const $=s=>document.querySelector(s);
const get=u=>fetch(u).then(r=>r.json());
const post=(u,d)=>fetch(u,{method:"POST",body:JSON.stringify(d)}).then(r=>r.json());
const pad=n=>String(n).padStart(2,"0");

async function loadPet(){
  const p=await get("/api/pet");
  $("#petName").textContent=p.nome;
  $("#petStage").textContent=p.estagio;
  $("#petIcon").textContent=ICON[p.estagio]||"🥚";
  $("#petPts").textContent=p.pontos+" pontos • "+p.hoje+"/"+p.teto+" hoje";
  const pct=p.proximo_estagio?Math.min(100,p.pontos/p.proximo_estagio*100):100;
  $("#petBar").style.width=pct+"%";

  const el=$("#species");el.innerHTML="";
  (p.catalogo||[]).forEach(c=>{
    const b=document.createElement("button");
    b.className="opt"+(c.id===p.especie?" sel":"");
    b.textContent=c.nome+" · "+c.especie;
    b.onclick=async()=>{await post("/api/pet/choose",{id:c.id});loadPet()};
    el.appendChild(b);
  });
}

async function loadRoutine(){
  const r=await get("/api/routine/today");
  const el=$("#routine");el.innerHTML="";
  if(!r.blocks||!r.blocks.length){el.textContent="Nada por hoje.";return}
  r.blocks.forEach(b=>{
    const h=document.createElement("div");h.className="blk";h.textContent=b.name;el.appendChild(h);
    b.tasks.forEach(t=>{
      const d=document.createElement("div");
      d.className="task"+(t.done?" done":"");
      d.innerHTML='<span class="box">'+(t.done?"✓":"")+'</span><span>'+t.label+'</span>';
      if(!t.done)d.onclick=async()=>{await post("/api/routine/done",{id:t.id});loadRoutine();loadPet()};
      el.appendChild(d);
    });
  });
}

async function loadAlarms(){
  const list=await get("/api/alarms");
  const el=$("#alarms");el.innerHTML="";
  if(!list.length){el.innerHTML='<p style="color:var(--soft);font-size:13px;margin:0">Nenhum alarme ainda.</p>';return}
  list.forEach(a=>{
    const row=document.createElement("div");row.className="alarm";
    const t=document.createElement("span");t.className="hhmm";
    t.textContent=pad(a.hour)+":"+pad(a.minute);
    const sw=document.createElement("input");sw.type="checkbox";sw.checked=a.enabled;
    sw.onchange=()=>post("/api/alarms/toggle",{id:a.id,enabled:sw.checked});
    const del=document.createElement("button");del.className="del";del.textContent="×";
    del.onclick=async()=>{await post("/api/alarms/remove",{id:a.id});loadAlarms()};
    row.append(t,sw,del);el.appendChild(row);
  });
}

$("#addAlarm").onclick=async()=>{
  const [h,m]=$("#newAlarmTime").value.split(":").map(Number);
  await post("/api/alarms/add",{hour:h||0,minute:m||0});
  loadAlarms();
};

let breathing=false;
$("#breathBtn").onclick=async()=>{
  breathing=!breathing;
  await post(breathing?"/api/breathing/start":"/api/breathing/stop",{});
  $("#breathBtn").textContent=breathing?"Parar":"Começar a respirar";
};

async function loadPomodoro(){
  const p=await get("/api/pomodoro");
  const s=p.restante_s||0;
  $("#timer").textContent=s?pad(Math.floor(s/60))+":"+pad(s%60):"--:--";
  $("#tstate").textContent=p.estado;
}

$("#bStart").onclick=async()=>{await post("/api/pomodoro",{acao:"iniciar"});loadPomodoro()};
$("#bPause").onclick=async()=>{await post("/api/pomodoro",{acao:"pausar"});loadPomodoro()};
$("#bStop").onclick=async()=>{await post("/api/pomodoro",{acao:"parar"});loadPomodoro()};

async function loadConfig(){
  const c=await get("/api/config");
  if(c.nome)$("#greeting").textContent="Oi, "+c.nome+"!";
  const st=$("#study"),bk=$("#brk");
  st.min=c.min_study;st.max=c.max_study;st.value=c.study_min;$("#vStudy").textContent=c.study_min;
  bk.min=c.min_break;bk.max=c.max_break;bk.value=c.break_min;$("#vBreak").textContent=c.break_min;
  $("#bright").value=c.brightness;$("#vBright").textContent=c.brightness;
  $("#vol").value=c.volume;$("#vVol").textContent=c.volume;

  const a=c.alerta_pais,on=[];
  if(a.amarelo)on.push("amarelo");if(a.vermelho)on.push("vermelho");
  $("#alertNote").textContent=on.length
    ? "Quando você sinalizar "+on.join(" ou ")+", seus pais recebem um aviso. Só eles podem mudar isso."
    : "Nenhum aviso automático está ligado agora.";

  // Semaforo so aparece se os responsaveis ligaram em /admin -- mesmo
  // gate que as tools de voz self.semaphore.* usam.
  if(c.regulacao_ativa){
    $("#semCard").style.display="";
    loadSemaphore();
    if(!semPoll)semPoll=setInterval(loadSemaphore,3000);
  }
}

const SEM_BTN={verde:"semVerde",amarelo:"semAmarelo",vermelho:"semVermelho"};
let semPoll=null;
async function loadSemaphore(){
  const s=await get("/api/semaphore");
  Object.entries(SEM_BTN).forEach(([nivel,id])=>{
    $("#"+id).classList.toggle("pri",nivel===s.nivel);
  });
  $("#semPending").style.display=s.pendente?"block":"none";
}
Object.entries(SEM_BTN).forEach(([nivel,id])=>{
  $("#"+id).onclick=async()=>{await post("/api/semaphore/set",{nivel});loadSemaphore()};
});
$("#semConfirm").onclick=async()=>{await post("/api/semaphore/confirm",{});loadSemaphore()};
$("#semCancel").onclick=async()=>{await post("/api/semaphore/cancel",{});loadSemaphore()};

[["study","vStudy"],["brk","vBreak"],["bright","vBright"],["vol","vVol"]].forEach(([id,lbl])=>{
  $("#"+id).oninput=e=>$("#"+lbl).textContent=e.target.value;
});

$("#save").onclick=async()=>{
  await post("/api/config",{study_min:+$("#study").value,break_min:+$("#brk").value,
    brightness:+$("#bright").value,volume:+$("#vol").value});
  $("#okMsg").textContent="Salvo!";setTimeout(()=>$("#okMsg").textContent="",2000);
  loadConfig();
};

loadPet();loadRoutine();loadConfig();loadPomodoro();loadAlarms();
setInterval(loadPomodoro,1000);
</script>
<p class="adminlink"><a href="/admin">⚙ Configurações (área dos responsáveis)</a></p>
</body></html>
)MUMA_HTML";

const char kAdminHtml[] = R"MUMA_HTML(
<!doctype html><html lang="pt-BR"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Configuração</title><style>
:root{--bg:#eceef1;--card:#fff;--ink:#1e2126;--soft:#6b7280;--ac:#3f6ea8;--line:#dfe3e8;--warn:#a6402f}
*{box-sizing:border-box}body{margin:0;padding:18px 14px 40px;font:15px/1.5 system-ui,sans-serif;background:var(--bg);color:var(--ink)}
h1{font-size:20px;margin:0 0 2px}h2{font-size:13px;margin:0 0 12px;color:var(--soft);text-transform:uppercase;letter-spacing:.06em}
.sub{color:var(--soft);font-size:13px;margin:0 0 18px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px;margin-bottom:14px}
.blk{border:1px solid var(--line);border-radius:10px;padding:12px;margin-bottom:10px}
input,select{width:100%;padding:8px 10px;border:1px solid var(--line);border-radius:8px;font:inherit;background:#fff}
.row{display:flex;gap:8px;align-items:center;margin:8px 0}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.days{display:flex;gap:3px;flex-wrap:wrap;margin-bottom:6px}
.day{width:30px;height:30px;border:1px solid var(--line);border-radius:7px;background:#fff;font-size:11px;cursor:pointer;padding:0}
.day.on{background:var(--ac);color:#fff;border-color:var(--ac)}
.x{background:none;border:0;color:var(--warn);cursor:pointer;font-size:18px;padding:0 4px;line-height:1}
.add{background:#f3f5f8;border:1px dashed var(--line);border-radius:8px;padding:8px;width:100%;cursor:pointer;font:inherit;color:var(--soft)}
label{display:block;font-size:12px;color:var(--soft);margin:10px 0 3px}
.chk{display:flex;align-items:center;gap:8px;margin:8px 0;font-size:14px}
.chk input{width:auto}
button.save{width:100%;padding:12px;border:0;border-radius:10px;background:var(--ink);color:#fff;font:inherit;font-weight:600;cursor:pointer;margin-top:12px}
.msg{font-size:13px;text-align:center;margin-top:8px;min-height:18px}
.msg.err{color:var(--warn)}.msg.ok{color:#2f7d5e}
.hint{font-size:12px;color:var(--soft);margin-top:8px}
</style></head><body>

<h1>Configuração</h1>
<p class="sub">Área dos responsáveis. Aqui ficam as regras — a página dela controla só o uso.</p>

<div class="card">
  <h2>Quem vai usar este aparelho</h2>
  <label>Nome</label><input type="text" id="cname" placeholder="Nome da criança/adolescente">
  <label>Data de nascimento</label><input type="date" id="cbirth">
  <p class="hint" id="ageHint"></p>
  <div class="chk"><input type="checkbox" id="regOn"><label style="margin:0">Ativar semáforo de sobrecarga e biblioteca de estratégias</label></div>
  <p class="hint">Isso é sobre necessidade específica (regulação sensorial/emocional), não sobre idade — liga só se fizer sentido pra essa criança.</p>
</div>

<div class="card">
  <h2>Rotina</h2>
  <div id="blocks"></div>
  <button class="add" id="addBlock">+ adicionar bloco</button>
  <button class="save" id="saveRoutine">Salvar rotina</button>
  <div class="msg" id="msgR"></div>
  <p class="hint">Se o JSON der erro, a versão anterior é mantida.</p>
</div>

<div class="card">
  <h2>Regras do bichinho</h2>
  <div class="grid">
    <div><label>Teto de pontos por dia</label><input type="number" id="cap"></div>
    <div><label>Aviso prévio (segundos)</label><input type="number" id="warn"></div>
    <div><label>Pontos p/ Filhote</label><input type="number" id="s2"></div>
    <div><label>Pontos p/ Jovem</label><input type="number" id="s3"></div>
    <div><label>Pontos p/ Forte</label><input type="number" id="s4"></div>
    <div><label>Volta ao foco em até (min)</label><input type="number" id="quickReturn"></div>
  </div>
  <p class="hint">Progresso nunca regride — não há como zerar pontos por aqui, e isso é proposital.</p>
  <p class="hint">"Volta ao foco": ponto bônus se ela iniciar um novo ciclo dentro desse tempo depois de cumprir uma pausa até o fim (não conta se ela parar o ciclo no meio e recomeçar).</p>
</div>

<div class="card">
  <h2>Alerta de sobrecarga</h2>
  <div class="chk"><input type="checkbox" id="yon"><label style="margin:0">Avisar no amarelo</label></div>
  <div class="chk"><input type="checkbox" id="yauto"><label style="margin:0">Amarelo envia sem pedir confirmação</label></div>
  <div class="chk"><input type="checkbox" id="ron"><label style="margin:0">Avisar no vermelho</label></div>
  <div class="chk"><input type="checkbox" id="rauto"><label style="margin:0">Vermelho envia sem pedir confirmação</label></div>
  <label>Servidor ntfy</label><input type="text" id="nsrv" placeholder="https://ntfy.sh">
  <label>Tópico ntfy</label><input type="text" id="ntop" placeholder="use algo longo e aleatório">
  <p class="hint">Quem souber o nome do tópico consegue ler os avisos. Use algo difícil de adivinhar, ou um tópico com senha.</p>
</div>

<div class="card">
  <h2>Sistema</h2>
  <label>Fuso horário (formato POSIX TZ)</label>
  <input type="text" id="tz" placeholder="&lt;-03&gt;3">
  <p class="hint">Piauí: <code>&lt;-03&gt;3</code>. Sem isso, a virada do dia acontece no horário errado.</p>
  <button class="save" id="saveCfg">Salvar regras</button>
  <div class="msg" id="msgC"></div>
</div>

<script>
const D=["D","S","T","Q","Q","S","S"],DN=["Dom","Seg","Ter","Qua","Qui","Sex","Sáb"];
let data={blocks:[]};
let csrf="";
const $=s=>document.querySelector(s);
const uid=()=>"t"+Math.random().toString(36).slice(2,7);

function render(){
  const root=$("#blocks");root.innerHTML="";
  data.blocks.forEach((b,bi)=>{
    const d=document.createElement("div");d.className="blk";
    const head=document.createElement("div");head.className="row";
    const nm=document.createElement("input");nm.type="text";nm.value=b.name;
    nm.style.fontWeight="600";nm.oninput=e=>b.name=e.target.value;
    const del=document.createElement("button");del.className="x";del.textContent="×";
    del.onclick=()=>{data.blocks.splice(bi,1);render()};
    head.append(nm,del);d.appendChild(head);

    b.tasks.forEach((t,ti)=>{
      const r=document.createElement("div");r.className="row";
      const lb=document.createElement("input");lb.type="text";lb.value=t.label;
      lb.oninput=e=>t.label=e.target.value;
      const rm=document.createElement("button");rm.className="x";rm.textContent="×";
      rm.onclick=()=>{b.tasks.splice(ti,1);render()};
      r.append(lb,rm);d.appendChild(r);
      const dd=document.createElement("div");dd.className="days";
      for(let i=0;i<7;i++){
        const btn=document.createElement("button");
        btn.className="day"+((t.days>>i&1)?" on":"");
        btn.textContent=D[i];btn.title=DN[i];
        btn.onclick=()=>{t.days^=(1<<i);render()};
        dd.appendChild(btn);
      }
      d.appendChild(dd);
    });
    const at=document.createElement("button");at.className="add";at.textContent="+ tarefa";
    at.onclick=()=>{b.tasks.push({id:uid(),label:"Nova tarefa",days:127,points:1});render()};
    d.appendChild(at);root.appendChild(d);
  });
}

$("#addBlock").onclick=()=>{data.blocks.push({name:"Novo bloco",tasks:[]});render()};

function show(el,ok,txt){el.className="msg "+(ok?"ok":"err");el.textContent=txt;setTimeout(()=>el.textContent="",3000)}

$("#saveRoutine").onclick=async()=>{
  const r=await fetch("/api/admin/routine",{method:"POST",headers:{"X-CSRF-Token":csrf},body:JSON.stringify(data)}).then(r=>r.json());
  show($("#msgR"),r.ok,r.ok?"Rotina salva.":(r.erro||"Falhou."));
};

$("#saveCfg").onclick=async()=>{
  const body={nome:$("#cname").value,data_nascimento:$("#cbirth").value,
    regulacao_ativa:$("#regOn").checked,
    daily_cap:+$("#cap").value,warn_sec:+$("#warn").value,
    quick_return:+$("#quickReturn").value,
    stage2:+$("#s2").value,stage3:+$("#s3").value,stage4:+$("#s4").value,
    y_on:$("#yon").checked,r_on:$("#ron").checked,y_auto:$("#yauto").checked,r_auto:$("#rauto").checked,
    ntfy_srv:$("#nsrv").value,ntfy_top:$("#ntop").value,tz:$("#tz").value};
  const r=await fetch("/api/admin/config",{method:"POST",headers:{"X-CSRF-Token":csrf},body:JSON.stringify(body)}).then(r=>r.json());
  show($("#msgC"),r.ok,r.ok?"Regras salvas.":(r.erro||"Falhou."));
  loadCfg();
};

async function loadCfg(){
  const c=await fetch("/api/admin/config").then(r=>r.json());
  $("#cname").value=c.nome||"";$("#cbirth").value=c.data_nascimento||"";
  $("#regOn").checked=!!c.regulacao_ativa;
  $("#ageHint").textContent=c.idade>=0
    ? c.idade+" anos -- os valores abaixo foram sugeridos com base nessa idade, mas edite à vontade."
    : "Data de nascimento não confirmada ainda, ou relógio do aparelho não sincronizou.";
  $("#cap").value=c.daily_cap;$("#warn").value=c.warn_sec;
  $("#quickReturn").value=c.quick_return;
  $("#s2").value=c.stage2;$("#s3").value=c.stage3;$("#s4").value=c.stage4;
  $("#yon").checked=c.y_on;$("#ron").checked=c.r_on;
  $("#yauto").checked=c.y_auto;$("#rauto").checked=c.r_auto;
  $("#nsrv").value=c.ntfy_srv;$("#ntop").value=c.ntfy_top;$("#tz").value=c.tz;
}

fetch("/api/admin/csrf").then(r=>r.json()).then(d=>csrf=d.csrf);
fetch("/api/admin/routine").then(r=>r.json()).then(d=>{data=d;render()});
loadCfg();
</script></body></html>
)MUMA_HTML";
