# ✈️ Aero Hub — Simulador de Voo & Treino de Pilotos

> Projeto Interdisciplinar — PUC Minas | 1º Período — Ciência da Computação | 2026

---

## 📌 Visão Geral

O **Aero Hub** é um simulador de voo embarcado desenvolvido com Arduino Uno, inspirado no treinamento técnico de pilotos da aviação civil. O projeto integra três módulos temáticos em um único sistema funcional, onde cada módulo representa um dos temas transversais do trabalho interdisciplinar.

O piloto controla a aeronave via potenciômetro (manche), gerencia a velocidade dos motores por botões físicos e precisa reagir a falhas mecânicas geradas aleatoriamente pelo sistema — tudo monitorado em tempo real via Serial Monitor e Bluetooth.

---

## 🧩 Arquitetura do Sistema

```
                        ┌─────────────────────────────────┐
                        │         ARDUINO UNO              │
                        │                                  │
  [Trimpot A0] ────────►│ Integrante 1: Computador de Bordo│──► LEDs (9, 10, 11)
  [Botões S1/S2] ──────►│ Integrante 2: Otimizador         │──► Buzzer (7)
  [Botão S3] ──────────►│ Integrante 3: Jogo de Pane       │──► Serial / Bluetooth
                        └─────────────────────────────────┘
```

O loop principal roda **sem nenhum `delay()` bloqueante** — todo o controle de tempo é feito com `millis()`, garantindo que os três módulos executem de forma paralela e assíncrona.

---

## ⚙️ Módulos Técnicos

### Módulo 1 — Sistemas Inteligentes: Computador de Bordo

**Responsabilidade:** Leitura do manche, filtragem de sinal e controle visual/sonoro de altitude.

**Sensor analógico:** Trimpot no pino `A0`, lido via `analogRead()` e convertido para graus de pitch no intervalo de **-90° a +90°** usando `map()`.

**Filtro de Média Móvel:**
O sinal bruto do potenciômetro é suavizado por um filtro de janela deslizante de 10 amostras. A cada leitura, a amostra mais antiga é descartada e a nova é inserida, calculando sempre a média do conjunto. Isso elimina ruídos elétricos e simula o amortecimento mecânico real de um manche de aeronave.

```cpp
// Implementação do filtro — O(1) por leitura, sem recalcular toda a soma
soma -= leituras[indice];
leituras[indice] = analogRead(PINO_TRIMPOT);
soma += leituras[indice];
indice = (indice + 1) % TAMANHO_FILTRO;
return soma / TAMANHO_FILTRO;
```

**Zonas de voo e atuadores:**

| Pitch | Status | LED | Buzzer |
|---|---|---|---|
| -30° a +30° | Estável | 🔵 Azul (pino 9) | Silencioso |
| -60° a +60° | Atenção | 🟡 Amarelo (pino 10) | 800 Hz |
| Além de ±60° | Perigo crítico | 🔴 Vermelho (pino 11) | 1500 Hz |

Durante uma pane ativa (Módulo 3), o controle de LEDs deste módulo é **suspenso** para ceder espaço ao alerta de emergência.

---

### Módulo 2 — Sustentabilidade: Otimizador de Combustível

**Responsabilidade:** Avaliar a qualidade dos comandos do piloto e calcular consumo de combustível e emissão de CO₂ fictícios.

**Interface de entrada:** Botões S1 (pino 2) e S2 (pino 3) com `INPUT_PULLUP` — ajustam a velocidade do motor de 0% a 100% em incrementos de 5%, com debounce de software de 200ms via `millis()`.

**Cálculo de consumo:**
O consumo por segundo é determinado por dois fatores combinados:

1. **Brusquidão do manche** — diferença absoluta entre o pitch atual e o anterior:
   - Variação < 10° → consumo base 0.1 L/s (voo suave)
   - Variação 10°–30° → consumo base 0.3 L/s (instável)
   - Variação > 30° → consumo base 0.7 L/s (movimento brusco)

2. **Arrasto aerodinâmico** — inclinações extremas aumentam o consumo proporcionalmente ao ângulo absoluto de pitch, simulando o esforço extra dos motores para compensar a perda de sustentação.

```cpp
float fatorInclinacao = 1.0 + (abs(pitch) / 30.0);
float fatorVelocidade = 1.0 + (velocidadeMotor / 100.0);
return consumoBase * fatorInclinacao * fatorVelocidade;
```

**Emissão de CO₂:** Estimada multiplicando o consumo por segundo por um fator fictício de 2.3 kg/L, acumulado ao longo do voo.

**Status de eficiência exibido:**

| Consumo/s | Status |
|---|---|
| ≤ 0.15 L/s | ECONÔMICO |
| ≤ 0.45 L/s | MODERADO |
| \> 0.45 L/s | INEFICIENTE |

---

### Módulo 3 — Serious Games: Desafio de Pane de Emergência

**Responsabilidade:** Gerar falhas mecânicas imprevisíveis e avaliar o tempo de reação do piloto.

**Geração de aleatoriedade:**
A semente do gerador de números aleatórios é inicializada com `analogRead(A5)` — leitura de um pino analógico flutuante (sem componente conectado), que capta ruído elétrico do ambiente e fornece entropia física real para o `random()`.

```cpp
randomSeed(analogRead(A5));
proximoTempoPane = millis() + random(5000, 10000);
```

**Fluxo de uma pane:**
1. Sistema agenda a pane com intervalo aleatório (5–10s na primeira, 8–15s nas seguintes)
2. Quando o tempo chega, `emPane = true` — LED vermelho pisca alternado com o buzzer em dois tons (1000 Hz / 400 Hz)
3. Piloto deve pressionar S3 (pino 4) para desarmar
4. Sistema mede o tempo de reação com `millis()` e aplica pontuação

**Sistema de pontuação:**

| Tempo de reação | Resultado | Pontos |
|---|---|---|
| < 500ms | Reação excepcional | +200 pts |
| 500ms – 1500ms | Estabilização concluída | +50 pts |
| > 1500ms | Atraso crítico | -150 pts |

O piloto começa com **1000 pontos de reputação**. A pontuação acumula ao longo da sessão de voo e é transmitida via Serial/Bluetooth.

---

## 🛠️ Hardware

| Componente | Quantidade | Função |
|---|---|---|
| Arduino Uno | 1 | Microcontrolador principal |
| Potenciômetro (Trimpot) | 1 | Simulação do manche |
| LED Azul + resistor 220Ω | 1 | Voo estável |
| LED Amarelo + resistor 220Ω | 1 | Zona de atenção |
| LED Vermelho + resistor 220Ω | 1 | Perigo / Pane |
| Buzzer | 1 | Alertas sonoros |
| Botão push | 3 | S1, S2 (motor), S3 (emergência) |
| Módulo HC-05 | 1 | Telemetria via Bluetooth |
| Multi-Function Shield | 1 | Integração física final |

---

## 📋 Pinagem

| Componente | Pino Arduino |
|---|---|
| Trimpot (manche) | A0 |
| Entropia aleatória | A5 (flutuante) |
| LED Azul | 9 |
| LED Amarelo | 10 |
| LED Vermelho | 11 |
| Buzzer | 7 |
| Botão S1 (+ velocidade) | 2 |
| Botão S2 (- velocidade) | 3 |
| Botão S3 (emergência) | 4 |

---

## 📁 Estrutura do Repositório

```
aero-hub/
├── codigo/
│   └── aerohub.ino          # Código fonte completo
├── circuito/
│   └── tinkercad.png        # Screenshot da montagem no Tinkercad
├── video/
│   └── pitch.mp4            # Vídeo de apresentação (até 3 min)
└── README.md
```

---

## 🚀 Como Executar

1. Abra o arquivo `codigo/aerohub.ino` na Arduino IDE
2. Selecione a placa **Arduino Uno** e a porta correta
3. Faça o upload para o Arduino
4. Abra o **Serial Monitor** em **9600 baud**
5. Gire o trimpot para simular o manche
6. Use S1/S2 para ajustar a velocidade dos motores
7. Aguarde a pane aleatória e pressione S3 para reagir

---

## 📊 Requisitos Técnicos Atendidos

| Requisito | Implementação |
|---|---|
| Sensor analógico + potenciômetro | Trimpot A0 como manche |
| Filtro de média móvel | Janela deslizante de 10 amostras |
| Sensor digital | Botões com INPUT_PULLUP |
| Entrada do usuário | S1, S2, S3 |
| 3 atuadores | LEDs azul, amarelo, vermelho + buzzer |
| Conectividade Bluetooth | HC-05 transmitindo telemetria |
| Gamificação | Sistema de pontuação e reação à pane |
| Modularidade | Funções separadas por módulo/integrante |
| Sem delay bloqueante | Controle total via millis() |

---

## 👥 Integrantes

| Nome | Módulo |
|---|---|
| [Thiago Duarte Machado] | Sistemas Inteligentes |
| [Samuel Bernardes de Castro] | Sustentabilidade |
| [Caio Giovanni Roberto Damasceno] | Serious Games |

---

## 🔗 Links do Projeto

- 🔧 [Simulação no Tinkercad](https://www.tinkercad.com/things/9YEymLnhGF7-aerohubv1?sharecode=emhYj8ox_bCAFRUfIVIQsJ4YC16IRF1TLjf8LtDwH4s)
- 📄 [Relatório Técnico (Download)](https://github.com/Thz002/AeroHub/raw/main/Relatorio_AeroHub_Grupo7%20(1).docx)

---

## 🎥 Vídeo de Apresentação
[Assistir no YouTube](https://youtu.be/Cbyn_gmVWpA)

---

## 🏫 Informações Acadêmicas

- **Instituição:** PUC Minas
- **Curso:** Ciência da Computação
- **Disciplina:** Trabalho Interdisciplinar — TI
- **Semestre:** 1º Semestre 2026
