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

//COMBUSTIVEL
#define S1 PB4
#define S2 PB3
#define S3 PA15
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
volatile unsigned long int TempoPassado = 0; 
// essas variáveis são declaradas como volatile para garantir seu correto funcionamento na ISR e código normal



//VELOCIDADE
#define VEL_IN PB9 //LEITURA DO SINAL DO SENSOR
float diametro = 0.51916;  //DIAMETRO EFETIVO DA RODA EM METROS
float pi = 3.1416;
float comp_total = pi * diametro; //COMPRIMENTO DO CIRCULO DO SENSOR CAPACITIVO
float comp_parcial = comp_total / 3.00; //COMO O CIRCULO POSSUI 3 GOMOS IGUAIS, PODEMOS DIVIDILO EM 3
int vel = 0; //VELOCIDADE EM KM/HR
int vel1 = 0; //VELOCIDADE EM T INSTANTES ANTES
int tempoVelRPM = 0;


void setup() 
{
  /*
      attachInterrupt() é uma função do próprio Arduino que recebe como parâmetros:
      (Pino_Interrupção, Instrução_Seguida, Modo_Interrupção) Verificar função no fórum do Arduino
      O modo utilizado chama a interrupção quando o estado o pino for de LOW para HIGH
   */
  attachInterrupt(digitalPinToInterrupt(PIN_RPM), PulsoRPM, RISING);
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
 noInterrupts(); // Desativa interrupções para enviar a MsgCAN
 interrupts(); // Ativa interrupções

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
  RPM = 60000/ (millis()- TempoPassado);
  TempoPassado = millis();
}
