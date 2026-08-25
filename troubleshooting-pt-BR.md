# Guia de Solução de Problemas - PizzaController

Este guia ajuda a diagnosticar e resolver problemas comuns no PizzaController com ESP32, driver DM556 e motor de passo NEMA23.  
Siga os passos por categoria de problema, do mais simples para o mais provável.

---

## Motor Não Se Move

1. **Alimentação e aterramento**
   - Confirme 24 VDC entre +V e GND do DM556 (dentro da faixa recomendada do driver).  
   - Verifique se o LM2596 está ajustado para 5 V estáveis na alimentação do ESP32/LCD (teste com multímetro em carga).  
   - GND da fonte, DM556, ESP32 e LM2596 devem estar todos em comum (terra único).

2. **Fiação do motor**
   ```md
   | Fase | Motor | DM556 |
   |------|-------|-------|
   | A+   | Verm. | Verm. |
   | A-   | Preto | Preto |
   | B+   | Verde | Azul  |
   | B-   | Amar. | Branco|
   ```
   - Se o motor apenas vibra/treme e não gira, provavelmente as bobinas estão ligadas em pares errados.  
   - Revise o pareamento das bobinas com multímetro (continuidade) ou documentação do motor.

3. **Sinais de comando (STEP/DIR/ENABLE)**
   - GPIO 21 (ENABLE) deve estar em nível baixo para habilitar o DM556 (ativo em nível baixo).  
   - Confirme que o firmware está usando os pinos corretos (18 = STEP, 19 = DIR) e que a FastAccelStepper está inicializada.  
   - Como teste rápido, pode-se colocar um LED + resistor entre STEP e GND para ver pulsos ao executar `JOG F 100`.

4. **DIP switches do DM556 (1/32 microstep, ~4,0 A de pico, 12800 pulsos/rev)**
   ```md
   Corrente: SW1 OFF, SW2 ON, SW3 OFF, SW4 ON
   Steps:    SW5 OFF, SW6 OFF, SW7 ON,  SW8 ON
   ```
   - Corrente muito baixa → pouco torque, motor pode “cantar” sem girar.  
   - Corrente muito alta → motor/driver aquecem muito (ver seção Aquecimento Excessivo).

5. **Firmware rodando**
   - No Monitor Serial (115200 baud), após reset, verifique se aparecem as mensagens:  
     - `========================================`  
     - `Pizza Controller - LOEM PUC-Rio`  
     - `========================================`  
     - Bloco de configuração do motor (steps, velocidade, acel, hold, direção de homing, posições salvas)  
     - Bloco de ajuda `Serial Commands`  
     - `System ready!`  
   - O sketch **não** faz homing automaticamente no boot. Para restabelecer uma referência conhecida, execute `FIND_HOME` (serial) ou use o item de menu **Home** — se o homing não completar, confira o sensor de home e o `HOME_DIRECTION`.  
   - Se nada aparece, verifique cabo USB de dados, porta COM e seleção de placa no Arduino IDE.

6. **Brown-out / reset ao tentar mover**
   - Se o ESP32 reinicia quando o motor começa a girar, pode ser queda de tensão (fonte subdimensionada) ou GND mal conectado.  
   - Teste com `SET_MAX_SPEED` e `SET_ACCELERATION` menores e confira a fonte 24 V / 5 A.

---

## Botões Diretos / Keypad / E-Stop Não Respondem

1. **Pinos e alimentação**
   - GPIO 34: Botões diretos de 7 posições (analógico, VCC 3,3 V, GND comum). Teclas 1-5 = slots 0-4; teclas 6/7 = jog CCW/CW em direção ao slot selecionado.
   - GPIO 35: Keypad de navegação (analógico, 3,3 V, 5 teclas).
   - GPIO 33: E-Stop (digital, INPUT_PULLUP, ativo em LOW).
   - Meça a tensão em cada GPIO vs GND ao pressionar cada botão; os níveis devem ser diferentes (divisor resistivo).
   - **GPIO 32 NÃO é entrada de botão** — é uma saída digital para o LED de status do home (espelha o `GET_SWITCH`).

2. **Limiares analógicos no código**
   ```md
   KEYPAD:  220 / 800 / 1400 / 2300 / 3600          (5 teclas, GPIO 35)
   DIRECT:  130 / 570 / 1170 / 1740 / 2370 / 3100 / 3700  (7 teclas, GPIO 34)
   HOME:    <1500                                    (GPIO 27, A3144)
   ```
   - Se as leituras ADC (via Serial) não baterem com esses intervalos, ajuste as constantes de limiar no início do sketch.

3. **Teste via Serial**
   - Pressionar um botão direto → Serial: `Position slot N` (teclas 1-5) ou `Going to saved position ... D: CW/CCW` (teclas 6/7).
   - Pressionar uma tecla de navegação → Serial: navega o menu / altera `inputValue` (sem linha de debug a menos que você adicione).
   - Confirme também se os slots 0–4 carregam posições salvas quando o botão correspondente é acionado.

---

## LCD Não Aparece

1. **Fiação I2C (LCD único)**
   - **LCD (principal, 0x27):** SDA GPIO 25, SCL GPIO 26 (Wire/I2C1)
   - VCC → 5V LM2596, GND comum
   - Adicione pullups 4,7k para 3,3V no par SDA/SCL se a exibição estiver instável
   - Verifique sem inversões, GND comum
   - O firmware atual inicializa **apenas um** LCD no barramento Wire padrão (endereço 0x27). Os GPIOs 22/23 ficam livres.

2. **Endereço I2C**
   - Endereço padrão usado no código: `0x27`.  
   - Alguns módulos utilizam `0x3F`; use um “I2C scanner” no ESP32 para descobrir o endereço real e ajuste o `#define LCD_ADDR` no início do sketch.

3. **Inicialização**
   - Se o Serial mostra o banner de boot, mas o LCD fica apagado:
     - Problema provável é hardware (alimentação/I2C) ou endereço incorreto.  
   - Ajuste o endereço e a inicialização da `LiquidCrystal_I2C` conforme o módulo.

---

## Sensor Home Não Funciona

1. **Conexões do Hall A3144**
   - Sinal → GPIO 27.  
   - VCC → 3,3 V, GND → GND comum.  
   - Verifique polaridade (alguns módulos têm serigrafia invertida ou difícil de ler).

2. **Leitura e limiar**
   - Threshold HOME: valores ADC `< 1500` são considerados “acionado” (conforme o código atual).  
   - Use o comando `GET_SWITCH` no Serial para testar:  
     - Observe o valor/estado com e sem o ímã próximo ao sensor.

3. **Rotina FIND_HOME**
   - O comando `FIND_HOME` move o motor até o sensor ser acionado, com debounce ~50 ms.  
   - Se o motor passa direto pelo sensor:
     - Verifique a direção de homing (`SET_HOME_DIR`).  
     - Verifique a polarização do imã.  
     - Reduza a velocidade de homing (`SET_SPEED`) para melhorar a precisão.

---

## Comandos Seriais Não Funcionam

1. **Configuração do Monitor Serial**
   - Baud: 115200, 8N1, sem controle de fluxo.  
   - Comandos em **maiúsculas**, terminados com Enter (CR/LF conforme configuração).

2. **Comandos básicos de teste**
   - `HELP` → lista todos os comandos disponíveis.  
   - `JOG F 1000`, `JOG B 1000` → teste rápido de movimento.  
   - `SET_MAX_SPEED 10000`, `SET_ACCELERATION 5000` → ajustam dinâmica.  
   - `GET_INFO` → mostra steps, velocidade, aceleração, hold e posições salvas.

3. **Sem eco no terminal**
   - Se nada aparece ao digitar:
     - Verifique se a porta COM está correta.  
     - Use um cabo USB com dados (não apenas carga).  
     - Feche outros programas que possam estar usando a mesma porta.

---

## Upload/Compilação Falha

1. **Bibliotecas necessárias**
   - Instaladas: `FastAccelStepper`, `Preferences`, `Wire`, `LiquidCrystal_I2C`.  
   - Evite ter múltiplas versões da mesma biblioteca em pastas diferentes do Arduino IDE.

2. **Seleção de placa e porta**
   - Placa: “ESP32 Dev Module” (ou equivalente compatível).  
   - Porta: selecione a porta correta em **Ferramentas > Porta**.

3. **Problemas comuns de upload**
   - Porta não aparece: driver USB faltando ou cabo sem dados.  
   - Erros persistentes de conexão:  
     - Tente outro cabo USB, outra porta do PC ou um hub USB alimentado.  
     - Em alguns casos, é preciso pressionar o botão BOOT/EN no ESP32 durante o início do upload.

---

## Aquecimento Excessivo

1. **Motor e driver**
   - Corrente configurada no DM556 (~4,0 A de pico) pode ser alta para alguns NEMA23; confira a corrente nominal do seu motor e reduza a corrente via DIP (SW1–SW4) se aquecer demais.  
   - Reduza a corrente nos DIP switches se o motor ficar muito quente ao toque (não deve queimar a mão em poucos segundos).

2. **Condições de operação**
   - Motor parado energizado (segurando posição) gera mais calor.  
   - Use `SET_HOLD_TIME` para reduzir o tempo em que o motor fica energizado após o movimento.  
   - Garanta ventilação adequada para o DM556 e o motor (dissipadores/ventoinha, se necessário).

3. **Fonte de alimentação**
   - Fonte subdimensionada pode trabalhar no limite, esquentar e causar queda de tensão.  
   - Utilize fonte 24 V ≥ 5 A de boa qualidade.

---

## Posição Não Persiste

1. **Gravação em NVM (Preferences)**
   - Após `SAVE_POS <slot>`, reinicie o ESP32 e use `GET_INFO` para conferir se a posição foi recarregada.  
   - Verifique se o namespace de `Preferences` no código não foi alterado entre versões (isso zera os dados antigos).

2. **Procedimento recomendado**
   - Sempre faça homing (`HOME` ou `FIND_HOME`) após ligar o sistema, antes de confiar em posições salvas.  
   - Depois de homing, mova para a posição desejada e só então use `SAVE_POS`.

---

## Menu LCD Congela

1. **Debounce e ruído no ADC**
   - O menu usa debounce em torno de 300 ms (`lastKeyTime`) para evitar múltiplos cliques.  
   - Se o ADC estiver muito ruidoso, o sistema pode interpretar cliques falsos ou ficar preso em estados intermediários.


---

## Testes Rápidos Recomendados

```md
Serial:
GET_POS
GET_INFO
GET_SWITCH
JOG F 50        (teste pequeno de movimento)
FIND_HOME       (verificar sensor de home)
TEST 100        (loop frente/home para stress-test)
SET_POS 0       (redefine a posição rastreada sem mover, útil após movimentos manuais)

Menu:
Navegar com Up/Down → Select (tecla 5) para executar ações
```

---

## Verificações Gerais

- Todos os GNDs em comum (fonte, DM556, ESP32, LM2596).  
- LM2596 regulado e estável em 5 V sob carga.  
- Fonte 24 V com corrente suficiente (≥ 5 A).  
- Conectores e parafusos firmes, sem fios soltos ou mal crimpados.  
- Reset do ESP32 após alterações de fiação ou parâmetros críticos.

Se o problema persistir, registre:
- Fotos nítidas da fiação (fonte, DM556, ESP32, motor, LCD, sensores).  
- Log completo do Monitor Serial (boot + comandos de teste).  

Em seguida, abra uma issue no repositório GitHub do projeto para análise detalhada.

Eng. Fredy Osorio  
ing.fredyosorio@gmail.com
Rio de Janeiro, 2026