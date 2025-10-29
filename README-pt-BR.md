# PizzaController - Controlador de Motor de Passo NEMA23 com ESP32

Este projeto fornece um sketch completo do Arduino para controlar um motor de passo NEMA23 usando um microcontrolador ESP32 e um driver DM556. O programa está configurado para microstepping máximo (1/256) para alcançar controle de posicionamento preciso.

## Requisitos de Hardware

- Placa de Desenvolvimento ESP32
- Motor de Passo NEMA23
- Driver DM556
- Fonte de Alimentação (adequada para seu motor e driver)
- Fios de Conexão

## Conexões de Fiação

Conecte o ESP32 ao driver DM556 da seguinte forma:

- ESP32 GPIO 18 → DM556 STEP
- ESP32 GPIO 19 → DM556 DIR
- ESP32 GPIO 21 → DM556 ENABLE (ativo baixo)
- ESP32 GPIO 22 → DM556 MS1
- ESP32 GPIO 23 → DM556 MS2
- ESP32 GPIO 25 → DM556 MS3
- ESP32 GPIO 26 → Interruptor de Limite de Home (ativo baixo, conecte um terminal ao GPIO 26 e o outro ao GND)

### Configuração dos Interruptores DIP do DM556

O driver DM556 usa interruptores DIP para configurar microstepping e outras configurações. Para este projeto (microstepping 1/256), configure os interruptores da seguinte forma:

- **SW1 (MS1):** LIGADO
- **SW2 (MS2):** LIGADO
- **SW3 (MS3):** LIGADO
- **SW4-SW8:** Consulte o manual do seu DM556 para corrente e outras configurações (tipicamente DESLIGADO para corrente padrão)

**Nota:** Certifique-se de que os interruptores DIP correspondam aos pinos de microstepping definidos no código ESP32 (MS1, MS2, MS3 todos ALTO para 1/256).

### Conexões de Energia

- **Energia do Driver DM556:** Conecte uma fonte de alimentação DC adequada (tipicamente 24V-48V DC, verifique as especificações do seu motor) aos terminais de entrada de energia do DM556 (+V e GND).
- **Energia do Motor de Passo:** As fases do motor (A+, A-, B+, B-) são conectadas diretamente às saídas do driver DM556.
- **Energia do ESP32:** Alimente o ESP32 via USB ou uma fonte separada de 5V/3.3V. Certifique-se de que a fonte de alimentação possa lidar com os requisitos de corrente.

**Nota:** Ajuste os pinos GPIO no código se sua fiação for diferente. Garanta classificações adequadas de energia para evitar danos.

## Configuração de Software

1. Instale o Arduino IDE
2. Instale o suporte à placa ESP32 no Arduino IDE
3. Abra `PizzaController.ino` no Arduino IDE
4. Selecione a placa ESP32 correta e a porta
5. Faça o upload do sketch

## Configuração

O programa está configurado para:
- **Microstepping:** 1/256 (precisão máxima)
- **Passos por Revolução:** 51200 (200 passos completos × 256 microsteps)
- **Velocidade:** Ajustável via constante `SPEED_DELAY` (atualmente 500 microssegundos entre passos)

## Funcionalidade do Programa

O programa principal demonstra controle básico do motor de passo através de:
1. Rotação do motor no sentido horário por meia revolução
2. Pausa de 1 segundo
3. Rotação do motor no sentido anti-horário por meia revolução
4. Pausa de 1 segundo
5. Repetição do ciclo

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

## Recursos Avançados

Para controle mais avançado, considere:
- Adicionar feedback de posição com encoders
- Implementar perfis de aceleração/desaceleração
- Adicionar chaves de limite para homing
- Integrar com outros sistemas de controle via comunicação serial

## Autor

Eng. Fredy Osorio
