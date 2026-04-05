# PizzaController - Controlador de Motor de Passo NEMA23 com ESP32 e FastAccelStepper

Este projeto fornece um sketch completo do Arduino para controlar um motor de passo NEMA23 usando um microcontrolador ESP32 e um driver DM556. O programa usa a biblioteca AccelStepper para aceleração/desaceleração suave e está configurado para microstepping máximo (1/256) para alcançar controle de posicionamento preciso. Inclui um sistema de menu LCD para operação fácil e foi otimizado para melhor manutenção do código.

## Requisitos de Hardware

- Placa de Desenvolvimento ESP32-Wroom
- Motor de Passo NEMA23
- Driver DM556
- Fonte de Alimentação 24Vdc - 5A
- Keypad 5 Botões
- Display LCD 16x2
- Sensor de Home de efeito Hall A3144 
- Botoeira personalizada para funcção de posição direta

## Conexões de Fiação

Conecte o ESP32 ao driver DM556 da seguinte forma:

- ESP32 GPIO 18 → DM556 STEP
- ESP32 GPIO 19 → DM556 DIR
- ESP32 GPIO 21 → DM556 ENABLE (ativo baixo)
- ESP32 GPIO 22 → DM556 MS1
- ESP32 GPIO 23 → DM556 MS2
- ESP32 GPIO 25 → DM556 MS3
- ESP32 GPIO 26 → Sensor de Limite de Home (ativo baixo, conecte um terminal ao GPIO 26 e o outro ao GND)

### Botões de Posição Direta

- ESP32 GPIO 34 → Botoeira de Posição Direta 

### Keypad de Navegação

- ESP32 GPIO 35 → Botoeira de Posição Direta 

### Dysplay LCD 16x2 via I2C

- ESP32 GPIO 25 → SDA (Serial Data)
- ESP32 GPIO 26 → SCL (Serial clock)

### Configuração dos Interruptores DIP do DM556



O driver DM556 usa interruptores DIP para configurar microstepping e outras configurações. Para este projeto (microstepping 1/256), configure os interruptores da seguinte forma:

#### Corrente

Configuração para Motor NEMA23, 4.01A :
- **SW1 (MS1):** OFF
- **SW2 (MS2):** ON
- **SW3 (MS3):** OFF
- **SW4 (MS4):** ON (corrente completa)

#### Steps

Configuração para 1600 pulsos/revolução:

- **SW5 (MS5):** OFF
- **SW6 (MS6):** OFF
- **SW7 (MS7):** ON
- **SW8 (MS8):** ON

### Conexões de Energia

- **Energia do Driver DM556:** fonte de alimentação 24VDC, 5A conectada aos terminais de entrada de energia do DM556 (+V e GND).
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
3. Instale as bibliotecas necessárias: AccelStepper, Preferences, Wire, LiquidCrystal_I2C
4. Abra `PizzaController_FastStepper/PizzaController_FastStepper.ino` no Arduino IDE
5. Selecione a placa ESP32 correta e a porta
6. Faça o upload do sketch

## Configuração

O programa está configurado para:
- **Microstepping:** 1/256 (precisão máxima)
- **Passos por Revolução:** 1600 
- **Velocidade:** Ajustável via constante `SPEED_DELAY` (atualmente 500 microssegundos entre passos)

## Funcionalidade do Programa

O programa inicializa o controlador do motor de passo e monitora continuamente comandos seriais para controlar o motor. Ele suporta funções de jogging, gerenciamento de posição, homing e teste. O motor pode ser controlado remotamente via comandos seriais ou programaticamente usando as funções fornecidas.

## Novos Recursos Adicionados

### Função Jog
- `jog(passos, direção)`: Move o motor um número específico de passos na direção especificada
- Direção: `true` para sentido horário, `false` para sentido anti-horário

### Salvamento de Posição
- `savePosition()`: Salva a posição atual na memória não-volátil (sobrevive a ciclos de energia)
- As posições são carregadas automaticamente na inicialização

### Função de Homing
- `home()`: Move o motor de volta para a posição inicial (posição 0)
- Calcula o movimento necessário baseado na posição atual

### Função de Reset de Home
- `resetHome()`: Define a posição atual como nova posição inicial (0)
- Útil para recalibrar o ponto de referência inicial

### Rastreamento de Posição
- Rastreamento de posição em tempo real relativo ao home
- Armazenamento persistente usando a biblioteca Preferences do ESP32

## Exemplos de Uso

### Jogging Básico
```cpp
// Jog 1000 passos no sentido horário
jog(1000, true);

// Jog 500 passos no sentido anti-horário
jog(500, false);
```

### Gerenciamento de Posição
```cpp
// Mover para uma posição específica
moveSteps(2500, true);

// Salvar a posição atual
savePosition();

// Retornar ao home
home();

// Definir posição atual como novo home
resetHome();
```

### Exemplos de Comandos Seriais
O programa suporta comandos seriais para controle remoto. Abra o Monitor Serial a 115200 baud e envie comandos (sensível a maiúsculas e minúsculas, seguido de nova linha):

- `JOG F <passos>`: Jog para frente (sentido horário) pelo número especificado de passos (ex.: `JOG F 1000`)
- `JOG B <passos>`: Jog para trás (sentido anti-horário) pelo número especificado de passos (ex.: `JOG B 500`)
- `MOVE_TO <posição>`: Mover para uma posição absoluta (ex.: `MOVE_TO 2500`)
- `HOME`: Mover para a posição inicial (posição 0)
- `FIND_HOME`: Encontrar home usando o interruptor de limite conectado ao GPIO 26
- `RESET_HOME`: Definir a posição atual como nova posição inicial
- `SAVE_POS <num>`: Salvar a posição atual no slot 0-4 (ex.: `SAVE_POS 1`)
- `LOAD_POS <num>`: Carregar posição do slot 0-4 (ex.: `LOAD_POS 1`)
- `GET_POS`: Obter a posição atual
- `TEST <passos>`: Iniciar a função de teste com o número especificado de passos (ex.: `TEST 1000`)
- `RUN_TEST <passos>`: Iniciar a função de teste com o número especificado de passos (ex.: `RUN_TEST 1000`)
- `STOP`: Parar a função de teste

Qualquer outra entrada responderá com "Unknown command".

### Exemplos de Funções de Demonstração
Adicione essas chamadas de função à função `loop()` ou chame-as de comandos seriais para demonstração:

```cpp
// Em loop() ou setup() para demonstração automática
demoJog();        // Executa jogging para frente e para trás
delay(2000);      // Esperar 2 segundos
demoSaveAndHome(); // Demonstra salvar posição, homing e redefinir home
```

## Personalização

- Modifique `SPEED_DELAY` para alterar a velocidade do motor (valores menores = mais rápido)
- Ajuste o número de passos nos loops para diferentes ângulos de rotação
- Descomente o include da biblioteca `AccelStepper` para recursos avançados
- Use a função `moveSteps()` para movimentos personalizados

## Depuração Serial

O programa gera mensagens de status no Monitor Serial a 115200 baud. Abra o Monitor Serial no Arduino IDE para visualizar informações de depuração.

## Notas de Segurança

- Garanta classificações adequadas de fonte de alimentação para seu motor e driver
- Verifique as conexões de fiação antes de ligar
- Comece com velocidades baixas e aumente gradualmente conforme necessário
- Monitore a temperatura do motor durante a operação

## Solução de Problemas

- Se o motor não se mover, verifique o pino ENABLE (deve ser LOW para habilitar)
- Verifique se os pinos de microstepping (MS1, MS2, MS3) estão definidos corretamente para o modo 1/256
- Garanta tensão e corrente adequadas da fonte de alimentação
- Verifique o Monitor Serial para mensagens de inicialização

## Otimizações de Código

O código foi otimizado para melhor manutenção e legibilidade:

- **Uso de Enum:** Substituiu números mágicos por valores enum descritivos (`NONE`, `JOG`, `SPEED`, `ACCEL`, `SAVE_POS`, `GOTO`, `GOTO_SAVED`) para estados de menu
- **Biblioteca FastAccelStepper:** Usa a biblioteca FastAccelStepper para controle de motor não-bloqueante de alto desempenho com aceleração/desaceleração suave
- **Teclado Analógico:** Botões de posição direta usam teclado analógico em vez de pinos digitais para melhor integração
- **Homing Orientado por Interrupção:** Homing otimizado com detecção baseada em interrupção do interruptor de limite
- **Bufferização Serial:** Bufferização inteligente de saída serial para evitar bloqueio de operações do motor
- **Configurações Persistentes:** Parâmetros do motor (passos por revolução, velocidade máxima, aceleração) são salvos na memória não-volátil
- **Funções Modulares:** O código é organizado em funções lógicas para melhor legibilidade e manutenção

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
- **Selecionar**: Entrar no sub-menu ou executar ação

### Navegação do Menu

1. Use os botões Cima/Baixo para selecionar um item do menu
2. Pressione Selecionar para entrar no sub-menu desse item
3. Use Esquerda/Direita/Cima/Baixo para ajustar o valor
4. Pressione Selecionar novamente para executar a ação e retornar ao menu principal

**Nota:** O sistema de menu LCD mencionado acima não está implementado no código atual `PizzaController.ino`. Ele pode estar em uma versão separada ou em desenvolvimento.

## Recursos Avançados

Para controle mais avançado, considere:
- Adicionar feedback de posição com encoders
- Implementar perfis de aceleração/desaceleração
- Adicionar chaves de limite para homing
- Integrar com outros sistemas de controle via comunicação serial

## Autor

Eng. Fredy Osorio
