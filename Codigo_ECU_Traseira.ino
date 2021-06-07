/* 
   EQUIPE IMPERADOR DE BAJA-SAE UTFPR
   AUTOR: Juliana Moreira e Matheus Henrique Orsini da Silva
   31/05/2021
   Codigo ECU Central
   INPUTS:   COMBUSTÍVEL-1, COMBUSTÍVEL-2, COMBUSTÍVEL-3, RPM_MOTOR, VELOCIDADE
   OUTPUTS:  MsgCAN{Velocidade, RPM, COMBUSTÍVEL-1, COMBUSTÍVEL-2, COMBUSTÍVEL-3, CAN_ID}
   Método de envio: Utilização de módulo CAN MCP2515
*/
// INCLUDE DAS BIBLIOTECAS
#include "mcp2515_can.h" // Biblioteca módulo CAN
#include <SPI.h> // Biblioteca de comunicação do módulo CAN

// PROTÓTIPO DAS FUNÇÕES
void PulsoRPM(); //RPM
void PulsoVelocidade(); // Pulsos Velocidade
unsigned int Velocidade();

//COMBUSTIVEL
#define PIN_COMB1 9
#define PIN_COMB2 8
#define PIN_COMB3 7
float valorTotal; //VARIAVEL PARA CALCULO DO TEMPO TOTAL
float desceLed; //VARIAVEL PARA DESCER OS LEDS
int ledsComb; //NÚMERO DE LEDS ACESOS
bool primeira = true;   //VARIAVEL PARA PRIMEIRO CICLO E ENCHIMENTO DO TANQUE
int tempComp = 0, tempMais = 0, tempMenos = 0, tempComp2 = 0, tempComp3 = 0, tempComp4 = 0; //VARIAVEIS PARA COMPARAR O TEMPO IDEAL COM O TEMPO USADO PARA CONSUMO
bool flagCima = false, flagMedio = false, flagBaixo = false, flagMBaixo = false, flagCompTempo = false, flagCompTempo2 = false;   //VARIAVEIS PARA VERIFICAR AONDE ESTA O NIVEL DE COMBUSTIVEL
int flagDesceCima = 0, flagDesceMedio = 0, flagDesceBaixo = 0, flagDesceMBaixo = 0, flagSobe = 0, flagVariacao = 0;

// Módulo CAN
#define CAN_ID 0x02
#define SPI_CS 10
mcp2515_can CAN(SPI_CS); // Cria classe da CAN
unsigned char MsgCAN[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // Vetor da MSG CAN
unsigned long int Tempo = 0;

//RPM
#define PIN_RPM 2
volatile unsigned long int RPM = 0;
volatile unsigned long int TempoRPM = 0; // Tempo do último pulso
// Essas variáveis são declaradas como volatile para garantir seu correto funcionamento na ISR e código normal



//VELOCIDADE
#define PIN_Vel 3 
#define DIAMETRO 0.51916  // Diametro efetivo da roda em metros
#define COMPRIMENTO (PI * DIAMETRO) // Comprimento da roda
#define CRISTAS 3 // Homocinética tem 3 cristas que vão ser captadas pelo sensor indutivo
unsigned int Vel = 0;
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
  
  // Cria Comunicação Serial
  SERIAL_PORT_MONITOR.begin(115200);
  // Verifica se a Serial foi iniciada
  while(!Serial){};
  // Verifica se a CAN foi iniciada
  while (CAN_OK != CAN.begin(CAN_500KBPS)) 
  {             
      SERIAL_PORT_MONITOR.println("CAN Falhou, tentando novamente...");
      delay(100);
  }
  SERIAL_PORT_MONITOR.println("CAN Iniciada, Tudo OK!"); 
}

void loop() 
{
  Tempo = millis();
  noInterrupts(); // Desativa interrupções para enviar a MsgCAN
  if(Tempo%215 == 0) // Leitura de dados a cada 215 milissegundos
  {
    Vel = Velocidade();
   /* if(TempCVT >= 90)
      Critico_Temp = 1;
    else
      Critico_Temp = 0;
    // Escreve os dados na mensagem CAN
    MsgCAN[0] = ;
    MsgCAN[1] = ;
    MsgCAN[2] = ;*/
    MsgCAN[3] = RPM;
    MsgCAN[4] = Vel;
    MsgCAN[5] = CAN_ID;
    // Envia a Mensagem conforme a forma do cabeçalho
    CAN.sendMsgBuf(CAN_ID, 0, 8, MsgCAN);
  }
  interrupts(); // Ativa interrupções

}

/*
    Função para leitura da quantida de pulsos da homocinética
    Utilizamos de interrupção para medir  , isso significa que cada pulso emitido será analisado.
    Essa função será passada como a ISR(Rotina a ser seguida) da interrupção por isso seu retorno e 
    parâmetros são VOID (seguindo regras de interrupção do Arduino)
    Parâmetros : VOID
    Return : VOID, Modifica
 */
void PulsoVelocidade() 
{
  /* 
      Para calcular o RPM da homocinética é feita a seguinte fórmula:
              RPM = [(1 / Período) * 1000 * 60] / 3
      No nosso caso o período e dado pelo tempo entre os dois pulsos e que sai em milissegundos.
      Sendo assim, para poder achar o RPM precisamos multiplicar por 1000 e depois por 60.
      Como essa interrupção será chamada 3 vezes a cada volta completa da homocinética será
      necessário dividir por 3 para ter o RPM da volta.
  */
  RPM_Homo = (60000/CRISTAS)/ (millis()- TempoVel);
  TempoVel = millis();
}

/*
    Função para o cálculo da velocidade angular do pneu
    Parâmetros : VOID
    Return : Inteiro do valor da velocidade
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
   return (RPM_Homo * COMPRIMENTO / 60) * 3.6;
}

/*
    Função para leitura do RPM
    Utilizamos de interrupção para medir o RPM, isso significa que cada pulso emitido será analisado.
    Essa função será passada como a ISR(Rotina a ser seguida) da interrupção por isso seu retorno e 
    parâmetros são VOID (seguindo regras de interrupção do Arduino)
    Parâmetros : VOID
    Return : VOID, Modifica o valor do RPM
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
