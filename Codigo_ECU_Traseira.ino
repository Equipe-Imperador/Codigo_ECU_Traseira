/* 
   EQUIPE IMPERADOR DE BAJA-SAE UTFPR
   AUTOR: Juliana Moreira e Matheus Henrique Orsini da Silva
   31/05/2021
   Codigo ECU Traseira
   INPUTS:   Comb-1, Comb-2, Comb-3, RPM_MOTOR, VELOCIDADE
   OUTPUTS:  MsgCAN{Velocidade, RPM-Milhar, RPM-Dezena, Comb-1, Comb-2, Comb-3, Litros, Millitros}
   Método de envio: Utilização de módulo CAN MCP2515
*/
// Include de bibliotecas
#include "mcp2515_can.h" // Biblioteca módulo CAN
#include <SPI.h> // Biblioteca de comunicação do módulo CAN

// Protótipo das funções
void PulsoRPM(); //RPM
void PulsoVelocidade(); // Pulsos Velocidade
unsigned int Velocidade(); // Calcular velocidade
void Combustivel();
void CalcRPM(unsigned short int*, unsigned short int*); // Separar RPM
void CalcTanque(unsigned short int*, unsigned short int*); // Separar Litros do tanque

// Combustivel
#define PIN_COMB1 9
#define PIN_COMB2 8
#define PIN_COMB3 7
#define CONSUMO 20
#define TANQUE 5200 // Volume do total tanque
#define TANQUE1 2200 // Volume do tanque até 2 sensor
#define TANQUE2 720 // Volume do tanque até 3 sensor
unsigned int LitrosTanque = 0;// Variável de aproximação de mililitros do tanque com base no consumo
unsigned short int Litros = 0, Mililitros = 0; // Variável para separar valores decimais do litro
unsigned long int TempoAbas = 0, TempoComb1 = 0; // Variável para o tempo do ultimo abastecimento
short int CombVerdadeiro[3] = {0,0,0}; // Vetor dos sensores do tanque (Esquerda o sensor mais alto)
// Vetores de comparação
short int CombAtual[3] = {0,0,0}; // Estado atual dos sensores
short int CombPassado[3] = {0,0,0}; // Ultimo estado dos sensores
short int CombRetrasado[3] = {0,0,0}; // Penultimo estado dos sensores
short int CombVerdadeiroPassado[3] = {0,0,0}; // Ultimo estado do CombVerdadeiro

// Módulo CAN
#define SPI_CS 10
mcp2515_can CAN(SPI_CS); // Cria classe da CAN
uint32_t CAN_ID = 0x10;
unsigned char MsgCAN[8] = {0}; // Vetor da MSG CAN 0-255
unsigned long int Tempo = 0;

// RPM
#define PIN_RPM 2
volatile unsigned long int RPM = 0;
volatile unsigned long int TempoRPM = 0; // Tempo do último pulso
// Essas variáveis são declaradas como volatile para garantir seu correto funcionamento na ISR e código normal
unsigned short int RPM_Mil = 0, RPM_Dez = 0; // Variáveis para separar o valor de 4 casas do RPM em 2 valores de 2 casas

// Velocidade
#define PIN_Vel 3 
#define DIAMETRO 0.51916  // Diametro efetivo da roda em metros
#define COMPRIMENTO (PI * DIAMETRO) // Comprimento da roda
#define DENTES 1 // Dentes na roda fônica que vão ser lidos pelo sensor
unsigned short int Vel = 0;
volatile unsigned long int RPM_Homo = 0; // Variável para salvar o RPM da homocinetica
volatile unsigned long int TempoVel = 0; // Tempo do último pulso

void setup() 
{
  /*
      attachInterrupt() é uma função do próprio Arduino que recebe como parâmetros:
      (Pino_Interrupção, Instrução_Seguida, Modo_Interrupção) Verificar função no fórum do Arduino
      O modo utilizado chama a interrupção quando o estado o pino for de LOW para HIGH
   */
  attachInterrupt(digitalPinToInterrupt(PIN_RPM), PulsoRPM, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_Vel), PulsoVelocidade, RISING);
  // Definição dos pinos
  pinMode(PIN_COMB1, INPUT);
  pinMode(PIN_COMB2, INPUT);
  pinMode(PIN_COMB3, INPUT);
  // Cria Comunicação Serial
  SERIAL_PORT_MONITOR.begin(115200);
  // Verifica se a Serial foi iniciada
  /*while(!Serial){};
  // Verifica se a CAN foi iniciada
  while (CAN_OK != CAN.begin(CAN_500KBPS)) 
  {             
      SERIAL_PORT_MONITOR.println("CAN Falhou, tentando novamente...");
      delay(100);
  }
  SERIAL_PORT_MONITOR.println("CAN Iniciada, Tudo OK!"); */
}

void loop() 
{
  Tempo = millis();
  noInterrupts(); // Desativa interrupções para enviar a MsgCAN
  if(Tempo%100 == 0) // Leitura de dados a cada 100 milissegundos
  {
    Vel = Velocidade();
    Combustivel();
    CalcRPM(&RPM_Mil, &RPM_Dez);
    CalcTanque(&Litros, &Mililitros);
    MsgCAN[0] = Vel;
    MsgCAN[1] = RPM_Mil;
    MsgCAN[2] = RPM_Dez;
    MsgCAN[3] = CombVerdadeiro[0];
    MsgCAN[4] = CombVerdadeiro[1];
    MsgCAN[5] = CombVerdadeiro[2];
    MsgCAN[6] = Litros;
    MsgCAN[7] = Mililitros;
    RPM_Homo = 0;
    // Envia a Mensagem conforme a forma do cabeçalho
    SERIAL_PORT_MONITOR.println(LitrosTanque);
    CAN.sendMsgBuf(CAN_ID, 0, 8, MsgCAN);
  }
  interrupts(); // Ativa interrupções

}

/*
    Função para separar o RPM.
    Como um casa do vetor da CAN aceita somente um numero entre 0 e 255 não podemos enviar o RPM inteiro,
    para isso será separado as primeiras duas casas das últimas duas casas em 2 variáveis.
    Parâmetros : Endereço da Casa dos milhares, Endereço da Casa das dezenas.
    Return : VOID, Modifica por referência os valores das variáveis de parâmetro.
 */
void CalcRPM(unsigned short int* Milhar, unsigned short int* Dezena)
{
   *Milhar = (unsigned short int)RPM / 100;
   *Dezena = RPM % 100;
}

/*
    Função para separar o nível de combustível do tanque.
    Como um casa do vetor da CAN aceita somente um numero entre 0 e 255 não podemos enviar o nível inteiro,
    para isso será separado as primeiras duas casas das últimas duas casas em 2 variáveis.
    Parâmetros : Valor do nível do tanque, Endereço da Casa dos Litros , Endereço da Casa dos Mililitros.
    Return : VOID, Modifica por referência os valores das variáveis de parâmetro.
 */
void CalcTanque(unsigned short int* Litros, unsigned short int* Mililitros)
{
  *Litros = (unsigned short int)LitrosTanque / 100;
  *Mililitros = LitrosTanque % 100;
}

/*
    Função para leitura dos sensores capacitivos do tanque.
    Vão ser utilizados 3 vetores de comparação para verificar a veracidade das informações,
    pois pode ocorrer mudanças bruscas de nivel do tanque devido ao terreno acidentado
    Além disso, será analisado se houve um reabastecimento e calculado um valor aproximado em mL para o tanque.
    Parâmetros : VOID.
    Return : VOID, Modifica os valores das variáveis dos sensores ( 1 = Ligado/Com combustível, 0 = Desligado)
                   Modifica o tempo do último abastecimento
                   Modifica o valor aproximado do tanque.
 */
void Combustivel()
{
  // Laço de repetição para que as leituras atuais sejam passadas para os vetores antigos
  for(int i = 0; i < 3; i++)
  {
    CombRetrasado[i] = CombPassado[i];
    CombPassado[i] = CombAtual[i];
  }
  CombAtual[0] = digitalRead(PIN_COMB1);
  CombAtual[1] = digitalRead(PIN_COMB2);
  CombAtual[2] = digitalRead(PIN_COMB3);
  
  /* 
      Se meus vetores de comparação tiverem o mesmo número quer dizer q nas últimas três medidas do sensor
      o combustível estava naquela região, ou seja, quer dizer que o nível real do combustível é esse e que não
      foi apenas um chacoalhão ocasionado pelo terreno, então posso atribuir esse valor para o meu CombVerdadeiro
   */
  if(CombRetrasado[0] == CombPassado[0] && CombPassado[0] == CombAtual[0])
  {
    CombVerdadeiroPassado[0] = CombVerdadeiro[0];
    CombVerdadeiro[0] = CombAtual[0];
  }
  if(CombRetrasado[1] == CombPassado[1] && CombPassado[1] == CombAtual[1])
  {
    CombVerdadeiroPassado[1] = CombVerdadeiro[1];
    CombVerdadeiro[1] = CombAtual[1];
  }
  if(CombRetrasado[2] == CombPassado[2] && CombPassado[2] == CombAtual[2])
  {
    CombVerdadeiroPassado[2] = CombVerdadeiro[2];
    CombVerdadeiro[2] = CombAtual[2];
  }
  /*
      Essa parte do código serve para verificar se ocorreu um enchimento do tanque e salvar o tempo dessa ocorrência
      para que a parte de consumo consiga realizar seus códigos corretamente
      Se minha leitura verídica atual mostra que tem combustível em todos os sensores e a leitura verídica passada mostrava que não havia quer dizer que temos um abastecimento
      Funcional na condição que o carro foi abastecido depois de ter apagado todos os sensores (Condição ideal)
      Caso não seja nessa condição será necessário reiniciar o código
  */
  if(CombVerdadeiro[0] == 1 && CombVerdadeiro[1] == 1 && CombVerdadeiro[2] == 1)
    TempoAbas = millis() / 1000; // Salva o tempo do ultimo momento com 3 sensores
  
  /*  
      A partir de agora é a parte do código que será responsável por criar uma aproximação de quantos litros
      possuimos no tanque, esse cálculo será feito com base no dado de que o tanque cheio é capaz de rodas
      por 2 hora e 30 minutos. Essa aproximação será feita para fazermos um acionamento mais suave dos LEDs
      barra de 12 segmentos, pois se fosse utilizado somente os sensores para acender os LEDs, seu acionamento
      seria em "pulos" e não ficaria agradável para o piloto

      Faremos a aproximação de uma forma linear, ou seja:
                Consumo = Total de Litros / Tempo de Consumo
      Total de litros do tanque = 5200 mL
      Tempo de consumo = 2 horas e 30 minutos =  7.200.000 milissegundos + 1.800.00 milissegundos = 5.400.000 ms
                Consumo = 5200 mL / 9 000 000 =  0,0006 mL por ms ou 0,6 mL a cada 1s
   */
   // Caso os sensores tiverem ativados sabemos exatamente qual o volume
   if(CombVerdadeiro[0] == 1)  
      LitrosTanque = TANQUE;
   else
   {
      if(CombVerdadeiro[1] == 1)
      {
         if(LitrosTanque > TANQUE1) // Enquanto for maior que volume teorico do segundo sensor
          LitrosTanque = TANQUE - ((int)((millis()/1000) - TempoAbas) * CONSUMO * 10);
        /*
            Para fazer uma aproximação do volume de combustível iremos retirar o consumo a cada 1s, ou seja,
            se eu verificar qual é o tempo desde o último enchimento do tanque irei obter quantas vezes
            foram consumidas os 0,6 mL, então terei que pegar essa quantidade de vezes
            multiplica-la por 0,6 e retirar esse valor do volume total do tanque
         */
          else
            LitrosTanque = TANQUE1;
          TempoComb1 = millis()/1000;
      }
      else
      {
        if(CombVerdadeiro[2] == 1)
        {
          if(LitrosTanque > TANQUE2) // Enquanto for maior que volume teorico do terceiro sensor
            LitrosTanque = TANQUE1 - ((int)((millis()/1000) - TempoComb1) * CONSUMO * 10);
          else
            LitrosTanque = TANQUE2;
        }
        else
          LitrosTanque = 0;
      }
   }
   
}

/*
    Função para leitura da quantida de pulsos da homocinética.
    Utilizamos de interrupção para medir  , isso significa que cada pulso emitido será analisado.
    Essa função será passada como a ISR(Rotina a ser seguida) da interrupção por isso seu retorno e 
    parâmetros são VOID (seguindo regras de interrupção do Arduino).
    Parâmetros : VOID.
    Return : VOID, Modifica valor do RPM_Homo.
 */
void PulsoVelocidade() 
{
  /* 
      Para calcular o RPM da homocinética é feita a seguinte fórmula:
              RPM = [(1 / Período) * 1000 * 60] / 1
      No nosso caso o período e dado pelo tempo entre os dois pulsos e que sai em milissegundos.
      Sendo assim, para poder achar o RPM precisamos multiplicar por 1000 e depois por 60.
      Como essa interrupção será chamada 1 vez(es) a cada volta completa da homocinética será
      necessário dividir por 1 para ter o RPM da volta.
  */
  RPM_Homo = (60000/DENTES)/ (millis()- TempoVel);
  TempoVel = millis();
}

/*
    Função para o cálculo da velocidade angular do pneu.
    Parâmetros : VOID.
    Return : Inteiro do valor da velocidade.
 */
unsigned int Velocidade()
{
  /*
      Para calcular a velocidade em Km/h fazemos:
                Vel = (RPM_Homo * COMPRIMENTO) / 60 Velocidade em metros por segundo
                Vel = Vel * 3,6 Temos a velocidade em Km/h
      O RPM mostra quantas voltas foram dadas em 1 minuto, se multimplicarmos pelo comprimento da roda temos
      a distância em metros percorrida pela roda em 1 minuto, ao dividir por 60 teremos a distância em metros por segundo.
      Então é feita a transformação para Km/h                
   */
   RPM_Homo /= 7; // Temos que dividir pela faixa fixa de redução da caixa pois pegamos na entrada do primeiro eixo.
   return (RPM_Homo * COMPRIMENTO / 60) * 3.6;
}

/*
    Função para leitura do RPM.
    Utilizamos de interrupção para medir o RPM, isso significa que cada pulso emitido será analisado.
    Essa função será passada como a ISR(Rotina a ser seguida) da interrupção por isso seu retorno e 
    parâmetros são VOID (seguindo regras de interrupção do Arduino).
    Parâmetros : VOID.
    Return : VOID, Modifica o valor do RPM.
 */
void PulsoRPM() 
{
  /*
      Para calcular o RPM é feita a seguinte fórmula:
              RPM = (1 / Período) * 1000 * 60
      No nosso caso o período e dado pelo tempo entre os dois pulsos e que sai em milissegundos.
      Sendo assim, para poder achar o RPM precisamos multiplicar por 1000 e depois por 60.
   */
  RPM = 60000/ (millis()- TempoRPM);
  TempoRPM = millis();
}
