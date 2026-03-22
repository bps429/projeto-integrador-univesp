const firebaseConfig = {
  apiKey:            "AIzaSyAZku1jcVLXtxzR_d-lR2y4liR_qFCLN_0",
  authDomain:        "integrador-univesp.firebaseapp.com",
  databaseURL:       "https://integrador-univesp-default-rtdb.firebaseio.com",
  projectId:         "integrador-univesp",
  storageBucket:     "integrador-univesp.firebasestorage.app",
  messagingSenderId: "1033756329950",
  appId:             "1:1033756329950:web:ea2585d83ac3f1bca13f4f"
};

firebase.initializeApp(firebaseConfig);
const db = firebase.database();

const alertCard   = document.getElementById("alertCard");
const alertIcon   = document.getElementById("alertIcon");
const alertTitle  = document.getElementById("alertTitle");
const alertMsg    = document.getElementById("alertMsg");
const estaturaVal = document.getElementById("estaturaVal");
const statusVal   = document.getElementById("statusVal");
const ultimaLeit  = document.getElementById("ultimaLeitura");
const statusPill  = document.getElementById("statusPill");
const statusPillT = document.getElementById("statusPillText");
const historico   = document.getElementById("historico");
const toast       = document.getElementById("toast");
const toastMsg    = document.getElementById("toastMsg");
const muteBtn     = document.getElementById("muteBtn");
const testarBtn   = document.getElementById("testarBtn");
const notifBtn    = document.getElementById("notifBtn");
const limparBtn   = document.getElementById("limparBtn");
const toastClose  = document.getElementById("toastClose");

let muted        = false;
let audioCtx     = null;
let toastTimer   = null;
let ultimoAlerta = false;

function beep(freq = 880, durMs = 600, vol = 0.1) {
  if (muted) return;
  if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
  const osc = audioCtx.createOscillator();
  const gain = audioCtx.createGain();
  osc.type = "sine";
  osc.frequency.value = freq;
  gain.gain.value = vol;
  osc.connect(gain);
  gain.connect(audioCtx.destination);
  osc.start();
  setTimeout(() => osc.stop(), durMs);
}

function beepAlerta() {
  beep(1000, 200, 0.12);
  setTimeout(() => beep(1000, 200, 0.12), 300);
  setTimeout(() => beep(1000, 400, 0.12), 600);
}

function mostrarToast(mensagem) {
  toastMsg.textContent = mensagem;
  toast.classList.add("visivel");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => toast.classList.remove("visivel"), 8000);
}

toastClose.addEventListener("click", () => toast.classList.remove("visivel"));

function pedirPermissaoNotificacao() {
  if (!("Notification" in window)) {
    alert("Seu navegador não suporta notificações.");
    return;
  }
  Notification.requestPermission().then(perm => {
    if (perm === "granted") {
      notifBtn.textContent = "✅ Notificações ativas";
      notifBtn.disabled = true;
    }
  });
}

function notificarSistema(titulo, corpo) {
  if (Notification.permission === "granted") {
    new Notification(titulo, {
      body: corpo,
      icon: "https://cdn-icons-png.flaticon.com/512/564/564619.png"
    });
  }
}

function adicionarHistorico(estatura, alerta) {
  const vazio = historico.querySelector(".vazio");
  if (vazio) vazio.remove();
  const agora = new Date().toLocaleTimeString("pt-BR");
  const li = document.createElement("li");
  if (alerta) {
    li.className = "alerta-item";
    li.textContent = "🚨 " + agora + " — Criança detectada! Estatura: " + estatura + " cm";
  } else {
    li.textContent = "✅ " + agora + " — Porta livre. Estatura: " + estatura + " cm";
  }
  historico.insertBefore(li, historico.firstChild);
  while (historico.children.length > 30) {
    historico.removeChild(historico.lastChild);
  }
}

function atualizarUI(dados) {
  const alerta   = !!dados.alerta;
  const estatura = dados.estatura_cm !== undefined ? dados.estatura_cm : "--";
  const agora    = new Date().toLocaleTimeString("pt-BR");

  estaturaVal.textContent = estatura;
  statusVal.textContent   = alerta ? "⚠️ ALERTA" : "✅ Normal";
  ultimaLeit.textContent  = agora;

  statusPill.className    = "status-pill " + (alerta ? "alerta" : "online");
  statusPillT.textContent = alerta ? "ALERTA ATIVO" : "Online";

  if (alerta) {
    alertCard.classList.add("ativo");
    alertIcon.textContent  = "🚨";
    alertTitle.textContent = "ATENÇÃO — Criança na Porta!";
    alertMsg.textContent   = "Estatura detectada: " + estatura + " cm. Verifique a entrada imediatamente!";
    if (!ultimoAlerta) {
      beepAlerta();
      mostrarToast("Criança detectada! Estatura: " + estatura + " cm");
      notificarSistema("🚨 CEMEI Zacarelli — ALERTA", "Criança detectada na porta! Estatura: " + estatura + " cm");
      adicionarHistorico(estatura, true);
    }
  } else {
    alertCard.classList.remove("ativo");
    alertIcon.textContent  = "✅";
    alertTitle.textContent = "Porta Livre";
    alertMsg.textContent   = "Nenhuma criança detectada no momento.";
    if (ultimoAlerta) adicionarHistorico(estatura, false);
  }

  ultimoAlerta = alerta;
}

// Escuta o último evento em /leituras (Firebase Realtime Database)
db.ref("leituras").limitToLast(1).on("child_added", function(snap) {
  const dados = snap.val();
  if (!dados) return;
  atualizarUI(dados);
}, function(erro) {
  statusPill.className    = "status-pill";
  statusPillT.textContent = "Erro de conexão";
  console.error("Firebase erro:", erro);
});

muteBtn.addEventListener("click", function() {
  muted = !muted;
  muteBtn.textContent = muted ? "🔕 Som: Desligado" : "🔔 Som: Ligado";
});

testarBtn.addEventListener("click", function() {
  atualizarUI({ alerta: true, estatura_cm: 110 });
  setTimeout(function() { atualizarUI({ alerta: false, estatura_cm: 175 }); }, 6000);
});

notifBtn.addEventListener("click", pedirPermissaoNotificacao);

limparBtn.addEventListener("click", function() {
  historico.innerHTML = '<li class="vazio">Nenhum evento registrado ainda.</li>';
});

if (Notification.permission === "default") {
  setTimeout(pedirPermissaoNotificacao, 2000);
}