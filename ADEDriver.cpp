#include "ADEDriver.h"
extern struct EnergyRegs energyVals;  //Energy register values are read and stored in EnergyRegs structure
extern struct PowerRegs powerVals;    //Metrology data can be accessed from these structures
extern struct RMSRegs rmsVals;  
extern struct PQRegs pqVals;
extern struct AcalRegs acalVals;
extern struct Temperature tempVal;

int ledState = LOW;
int inputState = LOW;
unsigned long lastReport = 0; //Instante de tiempo en el que se hizo la ultima actualización
const long reportInterval = 250; //Periodo de actualizacion de registros (originalmente 250)
const long blinkInterval = 500;

ADE9153AClass ade9153A;

void initADE9153A(void){
  pinMode(USER_INPUT, INPUT);
  pinMode(ADE9153A_RESET_PIN, OUTPUT);
  digitalWrite(ADE9153A_RESET_PIN, HIGH); 

  resetADE9153A();  //Reset ADE9153A for clean startup
  delay(1000);

  bool commscheck = ade9153A.SPI_Init(SPI_SPEED,CS_PIN); //Initialize SPI
  if (!commscheck) {
    Serial.println("ADE9153A Shield not detected. Plug in Shield and reset the Arduino");
    while (!commscheck) {     //Hold until arduino is reset
      delay(1000);
    }
  }

  ade9153A.SetupADE9153A(); //Setup ADE9153A according to ADE9153AAPI.h
  /* Read and Print Specific Register using ADE9153A SPI Library */
  Serial.println(String(ade9153A.SPI_Read_32(REG_VERSION_PRODUCT), HEX)); // Version of IC
  ade9153A.SPI_Write_32(REG_AIGAIN, -268435456); //AIGAIN to -1 to account for IAP-IAN swap ORIGINAL
  delay(500);
}

void readandwrite() //metodo que lee los registros de medida y actualiza las variables
{ 
  unsigned long currentReport = micros();
  if ((currentReport - lastReport) >= reportInterval){
    lastReport = currentReport;
    /* Read and Print WATT Register using ADE9153A Read Library */
    /* Read and Print WATT Register using ADE9153A Read Library */
    ade9153A.ReadPowerRegs(&powerVals);   // Read power registers
    ade9153A.ReadRMSRegs(&rmsVals);     // Read RMS registers
    ade9153A.ReadPQRegs(&pqVals);       // Read PQ registers
    ade9153A.ReadTemperature(&tempVal);  // Read temperature
  }
  
  inputState = digitalRead(USER_INPUT); //< UNICAMENTE SE HACE AUTOCALIBRACION CUANDO SE PULSA USER INPUT 

  if(inputState == LOW){
    autocalibrateADE9153A();
  }

}

void resetADE9153A(void) //< Reset del ADE9153A: Falling edge en su pin de RESET
{
 digitalWrite(ADE9153A_RESET_PIN, LOW); 
 delay(100);
 digitalWrite(ADE9153A_RESET_PIN, HIGH);
 delay(1000);
 Serial.println("ADE9153A reset Done");
}

void autocalibrateADE9153A(void){
    //ade9153A.SetupADE9153A(); //Setup ADE9153A according to ADE9153AAPI.h
    Serial.println("Autocalibrating Current Channel");
    if(!ade9153A.StartAcal_AINormal()) //< Empieza calibracion del canal de corriente A en modo normal
    Serial.println("Error en calibracion CH A I");
    runLength(20); //< Parpadea el led durante 20 segundos ?
    ade9153A.StopAcal(); //< PFinaliza la autocalibración que se este dando en el canal A
    Serial.println("Autocalibrating Voltage Channel");
    if(!ade9153A.StartAcal_AV()) //< Empieza la autocalibración del canal de tension A
    Serial.println("Error en calibracion CH A V");
    runLength(40);//< Parpadea el led durante 40 segundos
    ade9153A.StopAcal(); //< Finaliza la autocalibración que se este dando en el canal A
    delay(100); //< Se introduce un delay de 100 ms
    
    ade9153A.ReadAcalRegs(&acalVals); //< Lee los registros de calibración del canal A
    Serial.print("AICC: ");
    Serial.println(acalVals.AICC);
    Serial.print("AICERT: ");
    Serial.println(acalVals.AcalAICERTReg);
    Serial.print("AVCC: ");
    Serial.println(acalVals.AVCC);
    Serial.print("AVCERT: ");
    Serial.println(acalVals.AcalAVCERTReg);
    //Target conversion constant: TARGET_AICC = 838.19082 nA/Code
    long Igain = (-(acalVals.AICC / 838.19082) - 1) * 134217728; //< Ganancia de corriente para la calibración
    //Target conversion constant: TARGET_AVCC = 13424.43526 nV/Code
    long Vgain = ((acalVals.AVCC / 13424.43526) - 1) * 134217728; //< Ganancia de tension para la calibración
    ade9153A.SPI_Write_32(REG_AIGAIN, Igain); //Escribe la ganancia en los registros
    ade9153A.SPI_Write_32(REG_AVGAIN, Vgain); //Escribe la ganancia en los registros
    
    Serial.println("Autocalibration Complete"); //Una vez se ha escrito en los registros, se completa la autocalibración
    delay(2000);
  }


void runLength(long seconds)
{
  unsigned long startTime = millis();
  
  while (millis() - startTime < (seconds*1000)){
    //digitalWrite(LED_BUILTIN, HIGH);
    delay(blinkInterval);
    //digitalWrite(LED_BUILTIN, LOW);
    delay(blinkInterval);
  }  
}