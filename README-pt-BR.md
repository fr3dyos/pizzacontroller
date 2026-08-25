# PizzaController - Controlador de Motor de Passo NEMA23 com ESP32 e FastAccelStepper

Este projeto fornece um sketch completo do Arduino para controlar um motor de passo NEMA23 usando um microcontrolador ESP32 e um driver DM556. O programa usa a biblioteca FastAccelStepper para aceleração/desaceleração suave e está configurado para microstepping (1/32) (DM556 SW5–SW8 = OFF/OFF/ON/ON) para alcançar controle de posicionamento preciso e veloz ao mesmo tempo. O padrão do firmware é `STEPS_PER_REV = 12800` (32× os 400 passos naturais do motor). Inclui um sistema de menu LCD para operação fácil e foi otimizado para melhor manutenção do código.

## Requisitos de Hardware

- Placa de Desenvolvimento ESP32-Wroom
- Motor de Passo NEMA23
- Driver DM556
- Fonte de Alimentação 24Vdc - 5A
- Keypad analógico de 5 botões (navegação)
- Painel analógico de 7 botões diretos (slots 0–4 + jog CW/CCW)
- Display LCD I2C 16x2 (endereço 0x27)
- Sensor de Home de efeito Hall A3144, alimentado com 3,3V
- Opcional: LED no GPIO 32 para indicação de status do home

## Conexões de Fiação

Conecte o ESP32 ao driver DM556 da seguinte forma:

- ESP32 GPIO 18 → DM556 STEP
- ESP32 GPIO 19 → DM556 DIR
- ESP32 GPIO 21 → DM556 ENABLE (ativo baixo)
- ESP32 GPIO 27 → Sensor de Limite de Home

### Botões de Posição Direta (Seleção de Slot + Jog CW/CCW)

- ESP32 GPIO 34 → Botões diretos de 7 posições (analógico) - seleciona slots 0-4 e dispara jog CW/CCW (teclas 6/7) em direção ao slot selecionado

### LED de Status do Home (Opcional)

- ESP32 GPIO 32 → Saída para LED que indica o estado do sensor de home (acende quando o sensor Hall é acionado)

### Botão de Parada de Emergência

- ESP32 GPIO 33 → Botão E-Stop (digital, ativo LOW) - parada imediata do motor

### Keypad de Navegação

- ESP32 GPIO 35 → Botoeira de Navegação (leitura analógica)

### Display LCD via I2C

**LCD (menu principal, 0x27):**
- ESP32 GPIO 25 → SDA
- ESP32 GPIO 26 → SCL (I2C1/Wire)

Apenas um LCD é usado pelo firmware (endereço 0x27 no barramento Wire padrão). Os GPIOs 22/23 não são acionados por este sketch.


## Mapa de Conexões

| Componente           | Pino    | Conectado a                                     |
| -------------------- | ------- | ----------------------------------------------- |
| ESP32                | GPIO 18 | DM556 STEP                                      |
| ESP32                | GPIO 19 | DM556 DIR                                       |
| ESP32                | GPIO 21 | DM556 ENABLE (ativo em nível baixo)             |
| ESP32                | GPIO 25 | LCD I2C SDA                                     |
| ESP32                | GPIO 26 | LCD I2C SCL                                     |
| ESP32                | GPIO 27 | Sensor Hall A3144 Home (3,3 V)                  |
| ESP32                | GPIO 32 | LED de status do Home (saída digital)           |
| ESP32                | GPIO 33 | Botão E-Stop                                    |
| ESP32                | GPIO 34 | Botões de Posição Direta (analógico 7 posições) |
| ESP32                | GPIO 35 | Keypad de Navegação (analógico)                 |
| ESP32                | 5V      | Saída +5 V do LM2596 (out+)                     |
| Botão E-Stop         | Sinal   | ESP32 GPIO 33                                   |
| Botão E-Stop         | VCC     | 3,3 V                                           |
| Botão E-Stop         | GND     | GND                                             |
| LED Home             | Ânodo   | ESP32 GPIO 32 (via resistor)                    |
| LED Home             | Cátodo  | GND                                             |
| ESP32                | GND     | GND LM2596 (out-), GND da Fonte de Alimentação  |
| Fonte de Alimentação | +24V    | DM556 +V, LM2596 in+                            |
| Fonte de Alimentação | GND     | DM556 GND, GND ESP32, LM2596 in-                |
| Fonte de Alimentação | L, N    | 85–256 VAC 50/60 Hz                             |
| DM556                | A+      | Motor Vermelho                                  |
| DM556                | A-      | Motor Preto                                     |
| DM556                | B+      | Motor Verde                                     |
| DM556                | B-      | Motor Amarelo                                   |
| Sensor Hall          | Sinal   | ESP32 GPIO 27                                   |
| Sensor Hall          | VCC     | 3,3 V                                           |
| Sensor Hall          | GND     | GND                                             |
| LCD I2C              | SDA     | ESP32 GPIO 25                                   |
| LCD I2C              | SCL     | ESP32 GPIO 26                                   |
| LCD I2C              | VCC     | 5 V (LM2596)                                    |
| LCD I2C              | GND     | GND                                             |
| Botões Diretos       | Sinal   | ESP32 GPIO 34                                   |
| Botões Diretos       | VCC     | 3,3 V                                           |
| Botões Diretos       | GND     | GND                                             |
| Keypad Navegação     | Sinal   | ESP32 GPIO 35                                   |
| Keypad Navegação     | VCC     | 3,3 V                                           |
| Keypad Navegação     | GND     | GND                                             |

### Configuração dos Interruptores DIP do DM556

O driver DM556 usa interruptores DIP para configurar microstepping e outras configurações. Para este projeto (microstepping 1/32), configure os interruptores da seguinte forma:

#### Corrente

Configuração para Motor NEMA23, ~4,0 A de pico:

- **SW1 (MS1):** OFF
- **SW2 (MS2):** ON
- **SW3 (MS3):** OFF
- **SW4 (MS4):** ON (corrente completa)

#### Steps

Configuração para pulsos por revolução (SW5–SW8 = OFF/OFF/ON/ON no DM556 seleciona microstep 1/32, 12800 pulsos/rev). O firmware usa como padrão `STEPS_PER_REV = 12800` (32× os 400 passos naturais do motor):

- **SW5 (MS5):** OFF
- **SW6 (MS6):** OFF
- **SW7 (MS7):** ON
- **SW8 (MS8):** ON

> Nota: o firmware inicia com `STEPS_PER_REV = 12800` (32× os 400 passos naturais do motor, pois o DM556 está em microstepping 1/32). Ajuste em tempo de execução com `SET_STEPS <valor>` se a sua configuração do driver for diferente (ex.: `SET_STEPS 3200` para microstep 1/8); o valor é salvo na NVM.

### Conexões de Energia

- **Energia do Driver DM556:** fonte de alimentação 24VDC 5A, conectada aos terminais de entrada de energia do DM556 (+V e GND).
- **Energia do Motor de Passo:** As fases do motor (A+, A-, B+, B-) são conectadas diretamente às saídas do driver DM556, por meio de um conector Permak (binoculo 4 vias), seguindo o código de cores explicado na tabela.

| Fases | Motor     | DM556     |
|-------|-----------|-----------|
| A+    | Vermelho  | Vermelho  |
| A-    | Preto     | Preto     |
| B+    | Verde     | Azul      |
| B-    | Amarelo   | Branco    |

- **Energia do ESP32:** o ESP32 é alimentado pela mesma fonte, porém, passando por uma placa de regulação de voltagem LM2596 Step-Down, ajustada para aplicar 5V na porta de alimentação. Se for trocado certifique que a nova placa esteja regulada para 5V antes de qualquer conexão com a ESP32.

## Configuração de Software

1. Instale o Arduino IDE
2. Instale o suporte à placa ESP32 no Arduino IDE
3. Instale as bibliotecas necessárias: FastAccelStepper, Preferences, Wire, LiquidCrystal_I2C
4. Abra `PizzaController_FastStepper/PizzaController_FastStepper.ino` no Arduino IDE
5. Selecione a placa ESP32 correta e a porta
6. Faça o upload do sketch

## Configuração

O programa está configurado para:
- **Microstepping:** 1/32 (definido pelos DIP switches do DM556)
- **Passos por Revolução (padrão do firmware):** 12800 — ajuste em tempo de execução com `SET_STEPS <valor>` (salva na NVS)
- **Velocidade Máxima:** 8000 passos/seg — ajuste com `SET_MAX_SPEED <valor>` (salva na NVS, máx 50000)
- **Aceleração:** 4000 passos/seg² — ajuste com `SET_ACCELERATION <valor>` (salva na NVS, máx 50000)
- **Velocidade de Homing:** 2000 passos/seg — ajuste com `SET_SPEED <valor>` (salva na NVS, máx 50000)
- **Tempo de Hold do Motor:** 300 ms (obsoleto — `SET_HOLD_TIME` é mantido apenas para compatibilidade de NVM e não é mais honrado; o motor agora segura a posição indefinidamente entre movimentos)
- **Direção de Homing:** -1 (negativa) por padrão — ajuste com `SET_HOME_DIR <-1|+1>` (salva na NVS)
- **Slots de Posição Salvos:** 5 slots persistidos na NVS (`SAVE_POS <0-4>` / `LOAD_POS <0-4>`)

Veja a tabela completa de comandos seriais abaixo para todas as opções de configuração.

## Funcionalidade do Programa

O programa inicializa o controlador do motor de passo e monitora continuamente comandos seriais para controlar o motor. Ele suporta funções de jogging, gerenciamento de posição, homing e teste. O motor pode ser controlado remotamente via comandos seriais ou programaticamente usando as funções fornecidas.

### Comandos Seriais
O programa suporta comandos seriais para controle remoto. Abra o Monitor Serial a 115200 baud e envie comandos (maiúsculas, terminados com enter). 

- `JOG F <passos>`: Jog frente (horário) (1-50000)
- `JOG B <passos>`: Jog trás (anti-horário) (1-50000)
- `MOVE_TO <posição>`: Para posição absoluta (±1.000.000 máx)
- `HOME`: Move para posição 0 (movimento absoluto, sem sensor)
- `RESET_HOME`: Define a posição atual como novo home (0)
- `SAVE_POS <0-4>`: Salva a posição atual no slot 0-4
- `LOAD_POS <0-4>`: Move para a posição salva no slot 0-4
- `GET_POS`: Imprime a posição atual (atualizada do stepper)
- `FIND_HOME`: Busca home não-bloqueante usando o sensor Hall (debounce 50 ms, timeout 60 s)
- `TEST <passos>`: Teste contínuo — alterna entre `<passos>` e home (repete até `STOP`)
- `STOP`: Para teste/movimento
- `SET_STEPS <valor>`: Passos por revolução (salva NVM)
- `SET_MAX_SPEED <0-50000>`: Velocidade máxima passos/seg (salva NVM)
- `SET_ACCELERATION <0-50000>`: Aceleração passos/seg² (salva NVM)
- `SET_HOLD_TIME <0-10000>`: Tempo de hold ms após movimento (salva NVM)
- `SET_SPEED <0-50000>`: Velocidade de homing passos/seg (salva NVM)
- `SET_HOME_DIR <-1|+1>`: Direção de homing (salva NVM)
- `SET_HYST_POS <0-50000>`: Passos de compensação de histerese para rotação positiva (salva NVM)
- `SET_HYST_NEG <0-50000>`: Passos de compensação de histerese para rotação negativa (salva NVM)
- `SET_POS <passos>`: Sobrescreve a posição atual para `<passos>` sem mover o motor (salva NVM)
- `GET_INFO`: Mostra configuração e posições salvas
- `GET_SWITCH`: Lê o status do sensor de home e atualiza o LED
- `HELP`: Mostra a lista completa de comandos

Outros: "Unknown command". 
Exs: `JOG F 1000`, `SET_MAX_SPEED 10000`, `GET_INFO`.


## Personalização

A maioria dos parâmetros é exposta em tempo de execução via comandos seriais e persiste na NVM (namespace `"stepper"` do `Preferences`), portanto não é necessário recompilar para alterá-los:

| Comando Serial       | Efeito                                                    |
|----------------------|-----------------------------------------------------------|
| `SET_STEPS <v>`      | Passos por revolução                                      |
| `SET_MAX_SPEED <v>`  | Velocidade máxima (passos/seg)                            |
| `SET_ACCELERATION <v>` | Aceleração (passos/seg²)                                |
| `SET_HOLD_TIME <ms>` | Tempo de hold do motor após cada movimento                |
| `SET_SPEED <v>`      | Velocidade de homing                                      |
| `SET_HOME_DIR <d>`   | Direção de homing (-1 ou +1)                              |
| `SET_HYST_POS <v>`   | Compensação de histerese para rotação positiva            |
| `SET_HYST_NEG <v>`   | Compensação de histerese para rotação negativa            |
| `SET_POS <passos>`   | Sobrescreve a posição rastreada sem mover o motor         |

Se desejar recompilar com outros padrões, edite as constantes no início do sketch:

- `STEPS_PER_REV` — padrão de passos por revolução (padrão 12800)
- `MAX_SPEED` — velocidade máxima padrão em passos/seg (padrão 8000)
- `ACCELERATION` — aceleração padrão em passos/seg² (padrão 4000)
- `HOMING_SPEED` — velocidade de homing padrão em passos/seg (padrão 2000)
- `MOTOR_HOLD_TIME` — tempo de hold padrão em ms (padrão 300)
- `HOME_DIRECTION` — direção de homing padrão (padrão -1)
- `JOG_STEPS` — contagem de passos de jog padrão para o menu (padrão 50)

Use as funções auxiliares `startMotorMovement(targetPos)`, `moveToPosition(target)`, `home()` e `resetHome()` para movimentos personalizados no seu próprio código.

## Depuração Serial

O programa gera mensagens de status no Monitor Serial a 115200 baud. Abra o Monitor Serial no Arduino IDE para visualizar informações de depuração.

## Notas de Segurança

- Garanta classificações adequadas de fonte de alimentação para seu motor e driver
- Verifique as conexões de fiação antes de ligar
- Comece com velocidades baixas e aumente gradualmente conforme necessário
- Monitore a temperatura do motor durante a operação


## Sistema de Menu LCD

**LCD (0x27):** Sistema principal de menu - operação standalone (GPIO 25/26 I2C, endereço `0x27`, 16 colunas × 2 linhas).

O controlador inclui um sistema de menu LCD amigável para operação fácil sem um computador. O LCD I2C de 16x2 exibe opções de menu, e um teclado analógico de 5 botões permite navegação e entrada. Um segundo teclado analógico (7 posições) fornece seleção direta de slot e botões de jog CW/CCW.

Os 8 itens do menu, na ordem exibida, são:

1. **Go to Saved Pos** — carrega uma das 5 posições salvas
2. **Change Speed** — ajusta a velocidade máxima
3. **Change Accel** — ajusta a aceleração
4. **Home** — move para posição 0 (sem sensor)
5. **Reset Home** — define a posição atual como novo home (requer confirmação)
6. **Save Position** — salva a posição atual em um dos 5 slots
7. **Go to Pos** — move para uma posição absoluta (pode ser negativa)
8. **Jog** — ajusta a contagem de passos de jog usada pelas teclas seriais `JOG F`/`JOG B`

### Controles do Teclado

O keypad de navegação (GPIO 35) é lido com 5 limiares — teclas 1-5. Mapeamento:

- **Tecla 1**: Cima / decremento / alterna em sub-menus
- **Tecla 2**: Baixo / incremento / alterna em sub-menus
- **Tecla 3**: Decrementa 100 (em sub-menus numéricos)
- **Tecla 4**: Incrementa 100 / cancela sub-menu
- **Tecla 5**: Selecionar — entra no sub-menu ou executa ação

No menu principal: teclas 1/2 navegam, tecla 5 seleciona.
Em sub-menus numéricos (Speed, Accel, Go-to, Jog): tecla 1 = -10, tecla 2 = +10, tecla 3 = -100, tecla 4 = +100, tecla 5 = confirmar.
Nos sub-menus de seleção de slot (Goto Saved, Save Pos): teclas 1/2 alternam 0–4, tecla 5 = confirmar.
No sub-menu de confirmação (Reset Home): teclas 1/2 alternam Sim/Não, tecla 5 = confirmar.

### Painel de Botões Diretos (GPIO 34)

O painel de 7 teclas é lido por meio de 7 limiares ADC:

- **Teclas 1-5**: selecionam o slot de posição (slot = tecla - 1, ou seja, tecla 1 = slot 0 ... tecla 5 = slot 4). A linha superior do LCD mostra o slot e a posição salva; a linha inferior mostra posição atual / Moving / Homing.
- **Tecla 6**: jog CCW em direção à posição salva atualmente selecionada (caminho mais curto)
- **Tecla 7**: jog CW em direção à posição salva atualmente selecionada (caminho mais curto)

Depois que o jog termina, o firmware usa `SET_POS` internamente para atualizar a posição rastreada para o slot de destino, para que o controlador nunca perca a referência absoluta após um jog com wrap-around.

### Navegação do Menu

1. Use os botões Cima/Baixo para selecionar um item do menu
2. Pressione Selecionar para entrar no sub-menu desse item
3. Use o teclado para ajustar o valor (ou alternar Sim/Não)
4. Pressione Selecionar para executar e voltar ao menu principal, ou pressione Tecla 4 para cancelar

O firmware **não** faz homing automaticamente no boot. A posição lida da NVM é restaurada como está; execute `FIND_HOME` (serial) ou selecione o item **Home** no menu e use o sensor para encontrar a referência física se precisar de uma posição conhecida antes de operar.


## Autor

**Desenvolvido para o Laboratório LOEM, Departamento de Física, PUC-Rio.**

Eng. Fredy Osorio  
ing.fredyosorio@gmail.com  
Rio de Janeiro - Brasil, 2026.


