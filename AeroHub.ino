// =====================================================================
// AERO HUB — (Versão Arduino IDE)
// Adaptado para hardware físico: substitui o feedback via Bluetooth
// por um Display LCD 16x2 (módulo I2C), exibindo uma informação
// por vez, a cada 3 segundos.
// =====================================================================

#include <LCD-I2C.h>

// --- Endereço do módulo I2C do LCD ---
// O endereço mais comum é 0x27. Se o display não ligar/mostrar nada,
// rode um "I2C Scanner" (procure no Google) para descobrir o endereço
// correto do seu módulo (outro valor comum é 0x3F).
LCD_I2C lcd(0x27, 16, 2);

// --- Pinos de Entrada e Saída ---
const int PINO_TRIMPOT   = A0;  // Manche (Potenciômetro)
const int LED_AZUL       = 9;   // LED indicador de voo estável
const int LED_AMARELO    = 10;  // LED indicador de atenção
const int LED_VERMELHO   = 11;  // LED indicador de limite crítico
const int BUZZER         = 7;   // Sirene e bips de feedback
const int BOTAO_S1       = 2;   // S1: Aumentar velocidade do motor
const int BOTAO_S2       = 3;   // S2: Diminuir velocidade do motor
const int BOTAO_S3       = 4;   // S3: Botão de Desarme de Emergência

// --- Filtro de Média Móvel ---
const int TAMANHO_FILTRO = 10;
int leituras[TAMANHO_FILTRO];
int indice = 0;
long soma  = 0;

// --- Variáveis de Voo ---
int pitchAtual    = 0;
int pitchAnterior = 0;

// --- Variáveis de Combustível e Motor ---
float combustivel       = 100.0;
int   velocidadeMotor   = 50;
float consumoPorSegundo = 0.0;
float emissaoTotal      = 0.0;

// --- Variáveis do Jogo de Pane ---
bool emPane = false;                  // Estado de emergência ativo ou inativo
unsigned long tempoInicioPane = 0;    // Guarda o milissegundo em que a pane começou
unsigned long proximoTempoPane = 0;   // Agenda quando a próxima pane aleatória vai acontecer
long scorePiloto = 1000;              // Sistema de pontuação/reputação do piloto

// --- Controle de Tempo Assíncrono (Substitutos de delay) ---
unsigned long ultimoTempoVoo = 0;     // Controle de 1 segundo para o combustível
unsigned long ultimoBotao     = 0;     // Debounce para leitura dos botões S1 e S2
unsigned long ultimoTelemetria = 0;   // Controle de tempo para exibição no Serial Monitor

// --- Controle do Display LCD (uma informação por vez) ---
unsigned long ultimaTelaLCD = 0;      // Controla a troca de tela a cada 3 segundos
const unsigned long INTERVALO_LCD = 3000; // 3 segundos entre cada informação
int telaAtualLCD = 0;                 // Qual das telas está sendo exibida agora
const int TOTAL_TELAS_LCD = 6;        // Quantidade de informações no ciclo

// =====================================================================
void setup() {

  Serial.begin(9600);
  Serial.println("--- SIMULADOR DE VOO AERO-HUB CONFIGURADO ---");
  pinMode(LED_AZUL,    OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  pinMode(LED_VERMELHO,OUTPUT);
  pinMode(BUZZER,      OUTPUT);
  
  // Configurando botões com Pull-Up interno do Arduino (ativa ao conectar com GND)
  pinMode(BOTAO_S1,    INPUT_PULLUP);
  pinMode(BOTAO_S2,    INPUT_PULLUP);
  pinMode(BOTAO_S3,    INPUT_PULLUP);

  for (int i = 0; i < TAMANHO_FILTRO; i++) {
    leituras[i] = 0;
  }
  
  // Semente aleatória usando o pino analógico flutuante A1 para gerar imprevisibilidade
  // IMPORTANTE: A1 precisa ficar SEM NADA CONECTADO (flutuando) para isso funcionar.
  // Trocamos de A5 para A1 porque A4/A5 agora são usados pelo LCD (SDA/SCL).
  randomSeed(analogRead(A1));
  
  // Agenda a primeira pane aleatória para acontecer entre 5 e 10 segundos
  proximoTempoPane = millis() + random(5000, 10000);

  // --- Inicialização do LCD ---
  Wire.begin();
  lcd.begin(&Wire);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("AERO HUB");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");
  delay(1500); // delay aqui é aceitável, pois é só uma vez, na tela de boas-vindas
  lcd.clear();
  
  Serial.println("--- SIMULADOR DE VOO AERO-HUB CONFIGURADO ---");
}

// =====================================================================

// =====================================================================
int lerManche() {
  soma -= leituras[indice];
  leituras[indice] = analogRead(PINO_TRIMPOT);
  soma += leituras[indice];
  indice = (indice + 1) % TAMANHO_FILTRO;
  return soma / TAMANHO_FILTRO;
}

int calcularPitch(int valorFiltrado) {
  return map(valorFiltrado, 0, 1023, -90, 90);
}

void desligarLEDs() {
  digitalWrite(LED_AZUL,    LOW);
  digitalWrite(LED_AMARELO, LOW);
  digitalWrite(LED_VERMELHO,LOW);
}

void atualizarAlertas(int pitch) {
  // Se estiver em pane, o controle de LEDs normal é suspenso para piscar o alerta de falha
  if (emPane) return; 

  desligarLEDs();
  if (pitch >= -30 && pitch <= 30) {
    digitalWrite(LED_AZUL, HIGH);
    noTone(BUZZER);
  }
  else if (pitch > -60 && pitch < 60) {
    digitalWrite(LED_AMARELO, HIGH);
    tone(BUZZER, 800);
  }
  else {
    digitalWrite(LED_VERMELHO, HIGH);
    tone(BUZZER, 1500);
  }
}

// =====================================================================

// =====================================================================
void lerBotoes() {
  unsigned long agora = millis();
  if (agora - ultimoBotao < 200) return; // Filtro de bounce (evita cliques múltiplos rápidos)

  if (digitalRead(BOTAO_S1) == LOW) {
    velocidadeMotor = min(100, velocidadeMotor + 5);
    ultimoBotao = agora;
  }
  if (digitalRead(BOTAO_S2) == LOW) {
    velocidadeMotor = max(0, velocidadeMotor - 5);
    ultimoBotao = agora;
  }
}

float calcularConsumo(int difPitch, int pitch) {
  float consumoBase;
  if (difPitch < 10)      consumoBase = 0.1;
  else if (difPitch < 30) consumoBase = 0.3;
  else                    consumoBase = 0.7;

  // Adiciona o arrasto aerodinâmico do pitch absoluto.
  // Voar com bico empinado (pitch próximo a +90 ou -90) exige muito mais energia!
  float fatorInclinacao = 1.0 + (abs(pitch) / 30.0); // Ângulos críticos multiplicam o consumo

  float fatorVelocidade = 1.0 + (velocidadeMotor / 100.0);
  return consumoBase * fatorInclinacao * fatorVelocidade;
}

void atualizarCombustivel() {
  unsigned long agora = millis();
  if (agora - ultimoTempoVoo >= 1000) {
    int difPitch = abs(pitchAtual - pitchAnterior);
    consumoPorSegundo = calcularConsumo(difPitch, pitchAtual); // Passando o pitch atual corrigido
    combustivel = max(0.0, combustivel - consumoPorSegundo);
    emissaoTotal += consumoPorSegundo * 2.3; // Estimativa simples de CO2
    ultimoTempoVoo = agora;
  }
}

// =====================================================================

// =====================================================================
void gerenciarPane(unsigned long tempoAtual) {
  // Se o sistema não está em pane, monitora se chegou o momento de disparar a próxima falha
  if (!emPane && tempoAtual >= proximoTempoPane) {
    emPane = true;
    tempoInicioPane = tempoAtual;
    desligarLEDs();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("!! EMERGENCIA !!");
    lcd.setCursor(0, 1);
    lcd.print("Aperte S3 agora!");
    Serial.println("\n[ALERTA DE EMERGENCIA: PANE DE MOTOR!]");
    Serial.println("Pressione S3 (Botao no pino 4) rapidamente para estabilizar!");
  }

  // Comportamento durante o estado de pane
  if (emPane) {
    // Pisca o LED vermelho em sincronia com o bip sonoro da sirene
    if ((tempoAtual / 150) % 2 == 0) {
      digitalWrite(LED_VERMELHO, HIGH);
      tone(BUZZER, 1000);
    } else {
      digitalWrite(LED_VERMELHO, LOW);
      tone(BUZZER, 400);
    }

    // Verifica se o piloto apertou o botão S3 de desarme
    if (digitalRead(BOTAO_S3) == LOW) {
      noTone(BUZZER);
      desligarLEDs();
      emPane = false;
      
      // Mede o tempo de reação em milissegundos
      unsigned long tempoReacao = tempoAtual - tempoInicioPane;
      
      Serial.print("-> PANE RESOLVIDA! Tempo de reacao: ");
      Serial.print(tempoReacao);
      Serial.println(" ms");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Pane resolvida!");
      lcd.setCursor(0, 1);

      // Sistema de Recompensas e Pontuação (Gamificação)
      if (tempoReacao < 500) { // Menos de meio segundo: Reação excepcional
        scorePiloto += 200;
        Serial.println("Excelente! Manobra digna de piloto profissional. (+200 pts)");
        lcd.print("Excelente! +200");
        tone(BUZZER, 2000, 150);
      } else if (tempoReacao < 1500) { // Até 1.5 segundos: Reação normal
        scorePiloto += 50;
        Serial.println("Estabilizacao concluida com sucesso. (+50 pts)");
        lcd.print("Estabilizado +50");
        tone(BUZZER, 1200, 200);
      } else { // Demorou demais
        scorePiloto -= 150;
        Serial.println("Atraso critico de correcao! Perda de altitude. (-150 pts)");
        lcd.print("Atraso! -150 pts");
        tone(BUZZER, 250, 600);
      }

      // Próxima pane programada entre 8 e 15 segundos
      proximoTempoPane = tempoAtual + random(8000, 15000);

      // Mantém a mensagem de resultado visível por 2 segundos antes de
      // voltar ao ciclo normal de telas de telemetria
      delay(2000);
      lcd.clear();
      ultimaTelaLCD = millis(); // reinicia o ciclo de telas a partir de agora
      telaAtualLCD = 0;
    }
  }
}

// =====================================================================
// EXIBIÇÃO NO SERIAL MONITOR (mantido para depuração via cabo USB)
// =====================================================================
void exibirTelemetria(unsigned long tempoAtual) {
  if (tempoAtual - ultimoTelemetria >= 1000) {
    if (!emPane) {
      Serial.println("\n=== AERO HUB TELEMETRIA ===");
      Serial.print("Pitch: ");        Serial.print(pitchAtual);        Serial.println(" deg");
      Serial.print("Velocidade: ");   Serial.print(velocidadeMotor);  Serial.println(" %");
      Serial.print("Consumo/s: ");    Serial.print(consumoPorSegundo);Serial.println(" L/s");
      Serial.print("Combustivel: ");  Serial.print(combustivel);      Serial.println(" L");
      Serial.print("Emissao CO2: ");  Serial.print(emissaoTotal);      Serial.println(" kg");
      Serial.print("Reputacao: ");    Serial.print(scorePiloto);      Serial.println(" pts");

      if (consumoPorSegundo <= 0.15)      Serial.println("Status de Voo: ECONOMICO");
      else if (consumoPorSegundo <= 0.45) Serial.println("Status de Voo: MODERADO");
      else                                Serial.println("Status de Voo: INEFICIENTE");
      Serial.println("===========================");
    }
    ultimoTelemetria = tempoAtual;
  }
}

// =====================================================================
// EXIBIÇÃO NO LCD: uma informação por vez, alternando a cada 3 segundos
// (substitui o feedback que seria enviado via Bluetooth)
// =====================================================================
String statusDeVoo() {
  if (consumoPorSegundo <= 0.15)      return "ECONOMICO";
  else if (consumoPorSegundo <= 0.45) return "MODERADO";
  else                                return "INEFICIENTE";
}

void mostrarTelaLCD(int tela) {
  lcd.clear();
  switch (tela) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Pitch atual:");
      lcd.setCursor(0, 1);
      lcd.print(pitchAtual);
      lcd.print(" graus");
      break;

    case 1:
      lcd.setCursor(0, 0);
      lcd.print("Velocidade:");
      lcd.setCursor(0, 1);
      lcd.print(velocidadeMotor);
      lcd.print(" %");
      break;

    case 2:
      lcd.setCursor(0, 0);
      lcd.print("Consumo atual:");
      lcd.setCursor(0, 1);
      lcd.print(consumoPorSegundo);
      lcd.print(" L/s");
      break;

    case 3:
      lcd.setCursor(0, 0);
      lcd.print("Combustivel:");
      lcd.setCursor(0, 1);
      lcd.print(combustivel);
      lcd.print(" L");
      break;

    case 4:
      lcd.setCursor(0, 0);
      lcd.print("Emissao CO2:");
      lcd.setCursor(0, 1);
      lcd.print(emissaoTotal);
      lcd.print(" kg");
      break;

    case 5:
      lcd.setCursor(0, 0);
      lcd.print("Voo: ");
      lcd.print(statusDeVoo());
      lcd.setCursor(0, 1);
      lcd.print("Score: ");
      lcd.print(scorePiloto);
      break;
  }
}

void atualizarTelaLCD(unsigned long tempoAtual) {
  // Durante uma pane, o LCD já está sendo controlado por gerenciarPane(),
  // então o ciclo normal de telas fica pausado.
  if (emPane) return;

  if (tempoAtual - ultimaTelaLCD >= INTERVALO_LCD) {
    mostrarTelaLCD(telaAtualLCD);
    telaAtualLCD = (telaAtualLCD + 1) % TOTAL_TELAS_LCD;
    ultimaTelaLCD = tempoAtual;
  }
}

// =====================================================================
// LOOP PRINCIPAL (Sem delay bloqueante, exceto onde indicado)
// =====================================================================
void loop() {
  unsigned long tempoAtual = millis();

  // Executa o processamento do manche (Sistemas Inteligentes)
  int valorFiltrado = lerManche();
  pitchAnterior     = pitchAtual;
  pitchAtual        = calcularPitch(valorFiltrado);
  atualizarAlertas(pitchAtual);

  // Executa o processamento dos controles e motor (Sustentabilidade)
  lerBotoes();
  atualizarCombustivel();

  // Gerencia o jogo de reação e as panes de emergência (Serious Games)
  gerenciarPane(tempoAtual);

  // Exibe os dados no Serial Monitor (para depuração via USB)
  exibirTelemetria(tempoAtual);

  // Exibe os dados no LCD, uma informação por vez, a cada 3 segundos
  atualizarTelaLCD(tempoAtual);
}