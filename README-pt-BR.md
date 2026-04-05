# PizzaController - Controlador de Motor de Passo NEMA23 com ESP32 e FastAccelStepper

Este projeto fornece um sketch completo do Arduino para controlar um motor de passo NEMA23 usando um microcontrolador ESP32 e um driver DM556. O programa usa a biblioteca FastAccelStepper para aceleração/desaceleração suave e está configurado para microstepping (1/8) para alcançar controle de posicionamento preciso e veloz ao mesmo tempo. Inclui um sistema de menu LCD para operação fácil e foi otimizado para melhor manutenção do código.

## Requisitos de Hardware

- Placa de Desenvolvimento ESP32-Wroom
- Motor de Passo NEMA23
- Driver DM556
- Fonte de Alimentação 24Vdc - 5A
- Keypad 5 Botões
- Display LCD 16x2
- Sensor de Home de efeito Hall A3144, alimentado com 3.3V
- Botoeira personalizada para função de posição direta

## Conexões de Fiação

Conecte o ESP32 ao driver DM556 da seguinte forma:

- ESP32 GPIO 18 → DM556 STEP
- ESP32 GPIO 19 → DM556 DIR
- ESP32 GPIO 21 → DM556 ENABLE (ativo baixo)
- ESP32 GPIO 27 → Sensor de Limite de Home

### Botões de Posição Direta

- ESP32 GPIO 34 → Botoeira de Posição Direta de 5 posições (leitura analógica)

### Keypad de Navegação

- ESP32 GPIO 35 → Botoeira de Posição Navegação (leitura analógica)

### Display LCD 16x2 via I2C

- ESP32 GPIO 25 → SDA (Serial Data)
- ESP32 GPIO 26 → SCL (Serial clock)


## Mapa de Conexões

| Componente           | Pino    | Conectado a                                     |
| -------------------- | ------- | ----------------------------------------------- |
| ESP32                | GPIO 18 | DM556 STEP                                      |
| ESP32                | GPIO 19 | DM556 DIR                                       |
| ESP32                | GPIO 21 | DM556 ENABLE (ativo em nível baixo)             |
| ESP32                | GPIO 25 | LCD I2C SDA                                     |
| ESP32                | GPIO 26 | LCD I2C SCL                                     |
| ESP32                | GPIO 27 | Sensor Hall A3144 Home (3,3 V)                  |
| ESP32                | GPIO 34 | Botões de Posição Direta (analógico 5 posições) |
| ESP32                | GPIO 35 | Keypad de Navegação (analógico)                 |
| ESP32                | 5V      | Saída +5 V do LM2596 (out+)                     |
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

O driver DM556 usa interruptores DIP para configurar microstepping e outras configurações. Para este projeto (microstepping 1/8), configure os interruptores da seguinte forma:

#### Corrente

Configuração para Motor NEMA23, 4.01A:

- **SW1 (MS1):** OFF
- **SW2 (MS2):** ON
- **SW3 (MS3):** OFF
- **SW4 (MS4):** ON (corrente completa)

#### Steps

Configuração para pulsos por revolução (padrão 1600 pul/rev):

- **SW5 (MS5):** OFF
- **SW6 (MS6):** OFF
- **SW7 (MS7):** ON
- **SW8 (MS8):** ON

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
- **Microstepping:** 1/8 
- **Passos por Revolução:** 1600 
- **Velocidade:** Ajustável via constante `SPEED_DELAY` (atualmente 500 microssegundos entre passos)

## Funcionalidade do Programa

O programa inicializa o controlador do motor de passo e monitora continuamente comandos seriais para controlar o motor. Ele suporta funções de jogging, gerenciamento de posição, homing e teste. O motor pode ser controlado remotamente via comandos seriais ou programaticamente usando as funções fornecidas.

### Comandos Seriais
O programa suporta comandos seriais para controle remoto. Abra o Monitor Serial a 115200 baud e envie comandos (maiúsculas, terminados com enter). 

- `JOG F <passos>`: Jog frente (horário) (1-50000)
- `JOG B <passos>`: Jog trás (anti-horário) (1-50000)
- `MOVE_TO <posição>`: Para posição absoluta (±1M máx)
- `HOME`: Para posição 0
- `RESET_HOME`: Posição atual como home (0)
- `SAVE_POS <0-4>`: Salvar em slot 0-4
- `LOAD_POS <0-4>`: Ir para slot 0-4
- `GET_POS`: Posição atual
- `FIND_HOME`: Busca home não-bloqueante com sensor limite
- `TEST <passos>`: Teste contínuo (frente/home repetir)
- `STOP`: Para teste/movimento
- `SET_STEPS <valor>`: Passos/rev (salva NVM)
- `SET_MAX_SPEED <0-50000>`: Vel máx passos/seg (salva)
- `SET_ACCELERATION <0-50000>`: Acel passos/seg² (salva)
- `SET_HOLD_TIME <0-10000>`: Tempo hold ms após movimento (salva)
- `SET_SPEED <0-50000>`: Vel home (salva)
- `SET_HOME_DIR <-1/+1>`: Dir home (salva)
- `GET_INFO`: Mostra config (passos, vel, acel, hold, posições)
- `GET_SWITCH`: Status sensor home
- `HELP`: Mostra lista de comandos

Outros: "Unknown command". 
Exs: `JOG F 1000`, `SET_MAX_SPEED 10000`, `GET_INFO`.


## Personalização

- Modifique `SPEED_DELAY` para alterar a velocidade do motor (valores menores = mais rápido)
- Ajuste o número de passos nos loops para diferentes ângulos de rotação
- Use a função `moveSteps()` para movimentos personalizados

## Depuração Serial

O programa gera mensagens de status no Monitor Serial a 115200 baud. Abra o Monitor Serial no Arduino IDE para visualizar informações de depuração.

## Notas de Segurança

- Garanta classificações adequadas de fonte de alimentação para seu motor e driver
- Verifique as conexões de fiação antes de ligar
- Comece com velocidades baixas e aumente gradualmente conforme necessário
- Monitore a temperatura do motor durante a operação


## Sistema de Menu LCD

O controlador inclui um sistema de menu LCD amigável para operação fácil sem um computador. O LCD I2C de 16x2 exibe opções de menu, e um teclado analógico de 5 botões permite navegação e entrada.

### Opções de Menu

1. **Jog**: Mover manualmente o motor inserindo o número de passos
2. **Alterar Velocidade**: Ajustar a configuração de velocidade máxima
3. **Alterar Aceleração**: Ajustar a configuração de aceleração
4. **Home**: Mover o motor para a posição inicial (0)
5. **Reset Home**: Definir a posição atual como novo home (requer confirmação)
6. **Salvar Posição**: Salvar a posição atual em um dos 5 slots (requer confirmação)
7. **Ir para Posição**: Mover para uma posição absoluta (pode ser negativa)
8. **Ir para Posição Salva**: Carregar uma posição salva de um dos 5 slots

### Controles do Teclado

- **Cima/Baixo**: Navegar pelos itens do menu
- **Esquerda/Direita**: Ajustar valores em sub-menus (incrementar/decrementar por 10)
- **Cima/Baixo no sub-menu**: Ajustar valores por 100
- **Selecionar (Vermelho)**: Entrar no sub-menu ou executar ação

### Navegação do Menu

1. Use os botões Cima/Baixo para selecionar um item do menu
2. Pressione Selecionar para entrar no sub-menu desse item
3. Use Esquerda/Direita/Cima/Baixo para ajustar o valor
4. Pressione Selecionar novamente para executar a ação e retornar ao menu principal


## Autor

**Desenvolvido para o Laboratório LOEM, Departamento de Física, PUC-Rio.**

Eng. Fredy Osorio  
ing.fredyosorio@gmail.com  
Rio de Janeiro - Brasil, abril de 2026.


