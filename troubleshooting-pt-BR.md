# Guia de Solução de Problemas do PizzaController

Este guia ajuda você a diagnosticar e resolver problemas comuns com o controlador de motor de passo ESP32 PizzaController. Siga os passos abaixo para cada área de problema.

## Motor Não Se Move

1. **Verifique a Fonte de Alimentação:**
   - Certifique-se de que o driver DM556 recebe 24-48V DC com corrente suficiente para seu motor.
   - Verifique se a polaridade está correta (+V e GND).

2. **Verifique a Fiação:**
   - Verifique todas as conexões entre ESP32, driver DM556 e motor.
   - Certifique-se de que as fases do motor de passo (A+, A-, B+, B-) estão conectadas corretamente às saídas do DM556.

3. **Verifique o Pino ENABLE:**
   - GPIO 21 deve estar LOW para habilitar o driver DM556.
   - Verifique isso no código e na fiação.

4. **Configuração de Microstepping:**
   - Confirme os interruptores DIP do DM556: SW1 (MS1)=ON, SW2 (MS2)=ON, SW3 (MS3)=ON para microstepping 1/256.
   - Certifique-se de que os pinos ESP32 GPIO 22, 23, 25 estão HIGH para MS1, MS2, MS3.

5. **Monitor Serial:**
   - Abra o Monitor Serial a 115200 baud.
   - Verifique mensagens de inicialização como "Stepper initialized" ou "AccelStepper initialized".

## Botões de Posição Direta Não Funcionam

1. **Verifique a Fiação:**
   - Os botões devem estar conectados aos GPIO 12-16 e GND.
   - Certifique-se da configuração ativa baixa (botão pressionado = LOW).

2. **Configuração do Código:**
   - Verifique se os pull-ups internos estão habilitados para GPIO 12-16.
   - Verifique se o debouncing está implementado na função handleDirectButtons().

3. **Saída Serial:**
   - Pressione os botões e procure por mensagens "Direct button X pressed" no Monitor Serial.
   - Se nenhuma mensagem aparecer, verifique a fiação e o código.

4. **Funcionalidade do Botão:**
   - Certifique-se de que posições salvas existem (use o comando SAVE_POS).
   - Teste o comando LOAD_POS para verificar se o carregamento de posição funciona.

## Problemas de Comunicação Serial

1. **Taxa de Baud:**
   - Configure o Monitor Serial para 115200 baud.

2. **Porta COM:**
   - Selecione a porta COM correta para seu ESP32 no Arduino IDE.

3. **Conexão USB:**
   - Verifique se o cabo USB e os drivers estão funcionando.
   - Tente uma porta USB diferente ou cabo.

4. **Upload do Código:**
   - Certifique-se de que o código seja carregado com sucesso antes de testar comandos seriais.
   - Verifique erros de compilação.

## Display LCD Não Funciona

1. **Conexões I2C:**
   - SDA: GPIO 25
   - SCL: GPIO 26

2. **Endereço LCD:**
   - Verifique se o endereço I2C do LCD é 0x27 ou 0x3F (endereços comuns).

3. **Fonte de Alimentação:**
   - Certifique-se de que o LCD recebe 5V.
   - Verifique conexões VCC e GND.

4. **Biblioteca:**
   - Confirme se a biblioteca LiquidCrystal_I2C está instalada.

5. **Saída Serial:**
   - Verifique mensagens de erro relacionadas ao LCD no Monitor Serial.

## Problemas de Fonte de Alimentação

1. **Níveis de Tensão:**
   - ESP32: 5V/3.3V
   - Driver DM556: 24-48V DC
   - LCD: 5V

2. **Polaridade:**
   - Verifique novamente todas as conexões de energia para polaridade correta.

3. **Capacidade de Corrente:**
   - Certifique-se de que as fontes de alimentação podem lidar com a corrente necessária para todos os componentes.

4. **Terra Comum:**
   - Verifique se todos os componentes compartilham uma conexão de terra comum.

## Erros de Compilação de Código

1. **Bibliotecas Necessárias:**
   - Instale: AccelStepper, Preferences, Wire, LiquidCrystal_I2C

2. **Seleção de Placa:**
   - Selecione "ESP32 Dev Module" no Arduino IDE em Ferramentas > Placa.

3. **Erros de Sintaxe:**
   - Verifique pontos e vírgulas, colchetes ou erros de digitação ausentes.
   - Certifique-se de que todas as variáveis estão declaradas.

4. **Declarações Include:**
   - Verifique se todos os cabeçalhos necessários estão incluídos no topo do sketch.

## Ajuda Adicional

Se estes passos não resolverem seu problema:
- Revise o README.md para instruções detalhadas de configuração.
- Verifique a saída serial para mensagens de erro específicas.
- Verifique se todas as conexões de hardware correspondem aos diagramas de fiação.
- Teste com um sketch de exemplo simples para isolar o problema.

Para suporte da comunidade, forneça:
- Detalhes da sua configuração de hardware
- Versão do código e modificações
- Mensagens de saída serial
- Sintomas específicos do problema
