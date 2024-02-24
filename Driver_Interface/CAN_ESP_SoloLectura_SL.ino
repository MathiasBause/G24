/*-------------CÓDIGO DE PANTALLA Y LUCES LED G24-------------

  Autores: Raúl Arcos, Francisco Sánchez, Pablo Aguilar, Mathias Bause
  Descripción: El código siguiente sirve para procesar los datos de la ECU de un monoplaza de Formula Student, recibidos por CAN BUS, para mostrarlos al piloto a través de una
  tira de luces LED y de una pantalla HMI.
  */


  /*-------------LIBRERÍAS-------------*/

  #include <Ticker.h>
  #include <Wire.h>
  #include <stdlib.h>
  #include <ArduinoSort.h>
  #include <CAN.h>
  #include <SPI.h>


  /*-------------PINES DE DATOS-------------*/


  #define TX_GPIO_NUM   21  // Connects to CTX
  #define RX_GPIO_NUM   22  // Connects to CRX




  /*-------------DEFINICIÓN DE IDs DE VARIABLES PARA PASO DE MENSAJES A LA PANTALLA-------------*/

  /*Las siguientes definiciones de bytes vienen dadas por la programación de la pantalla HMI T5L0 ASIC del salpicadero.
  Cada constante corresponde a un parámetro recibido por CAN BUS de parte de la ECU, y se usan para especificar qué parámetro se envía a la pantalla en cada trama de datos.
  NOTA: En la Wiki se puede encontrar la explicación detallada de la programación de la pantalla en: G22 EVO -> Salpicadero.
  */
  #define RPM_ID 0x51 //RPM
  #define ECT_IN_ID 0x52 //Temperatura entrada del radiador
  #define GEAR_ID 0x53 //Marcha
  #define TPS_ID 0x54 //Posición pedal acelerador
  #define BPS_ID 0x55 //Presión de freno
  #define BVOLT_ID 0x56 //Voltaje de batería
  #define LAMBDA_ID 0x57 //Lambda
  #define LR_WS_ID 0x58 //Velocidad de rueda LR
  #define RR_WS_ID 0x59 //Velocidad de rueda RR
  #define LF_WS_ID 0x60 //Velocidad de rueda LF
  #define RF_WS_ID 0x61 //Velocidad de rueda RF
  #define ECT_OUT_ID 0x62 //Temperatura salida del radiador

  //Función en la que se inicializan un objeto CAN y la tira LED
  void start() {

    CAN.setPins (RX_GPIO_NUM, TX_GPIO_NUM);
    //Se intenta inicializar un objeto CAN hasta conseguirlo.
    if (!CAN.begin (250E3)) {
      //Serial.println ("Starting CAN failed!");
      while (1);
    }
    else {
      //Serial.println ("CAN Initialized");
    }
  }
  //Función para generar un arreglo de bytes, con todos los valores necesarios, para poder enviar un dato de hasta 2 bytes de longitud a la pantalla HMI.
  void send_serial(byte type, unsigned int value) {                     //Como parámetros se pasan el ID (type), que es el ID establecido al inicio del código para el dato que se quiera enviar. Ej: RPM_ID -> 0x51; y se envía el valor de dicho dato.
    byte dato[8] = { 0x5A, 0xA5, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00 };  //Se establece un arreglo de bytes con los primeros datos necesarios para que la pantalla lo interprete como mensaje (En la Wiki hay tutoriales que lo explican a fondo), como ser la longitud y el tipo de mensaje.
    dato[4] = type;                                                     //Se configura en el mensaje el ID correspondiente al dato a enviar.
    dato[6] = (value >> 8) & 0xFF;                                      //Se configura el dato en los últimos 2 bytes.
    dato[7] = value & 0xFF;

    Serial.write(dato, 8);                                              //Se envía serialmente el mensaje, indicando su longituden bytes para ello.
  }

  void clearBuffer(char buf[]){
    for (int i = 0; i <= 7; i++){
      buf[i] = 0;
    }
  }

  //Función para leer los valores que envía la ECU por CAN BUS, y transmitirlos a la pantalla HMI, llamando a la función send_serial().
  void readCanBus() {
    //Se crean las variables necesarias para almacenar los datos recibidos por CAN BUS
    unsigned char len = 0;                                              //Variable para almacenar la longitud de la trama de CAN BUS.
    unsigned char buf[8];                                               //Búfer para almacenar la trama recibida.
    unsigned int rpm, ectIn, ectOut, gear, bvolt, lambda, lrWs;                 //Variables para almacenar los distintos datos que componen la trama. 

    int bytePos;
    int packetSize = CAN.parsePacket();                                 //Se verifica si existe alguna trama recibida para leer. Una vez sucede esto...
    if (packetSize) {
      bytePos = 0;
      while (CAN.available()){
        buf[bytePos] = CAN.read();
        bytePos++;
      }                           
      if (buf[0] == 1) {                                                //Se revisa el primer byte del búfer, que corresponde a la ID del frame, como debería ser en los mensajes dirigidos a la placa. 
        
        //Si la ID del Frame es 1, se leen los siguientes datos: RPM, ECT_IN, .
        ledsVolante(rpm);                                               //Se pasa el dato de las rpm como parámetro a ledsVolante para que la tira LED funcione según las rpm.
        //Serial.print("RPM: ");                                          //En el puerto serie se imprime para verificar el valor de rpm.
        //Serial.println(rpm);
        send_serial(RPM_ID, rpm);                                       //Se llama a la función send_serial() para enviar el dato de las rpm con su respectiva ID a la pantalla HMI.
        //Con la salvedad de que solo con las rpm se realiza algo con la tira LED, para los demás parámetros recibidos por CAN se repite el proceso anterior: Extraerlos del búfer, imprimirlos para comprobar en el puerto serie, y enviarlos a la pantalla HMI.
        //NOTA: De qué byte(s) del búfer se extre cada parámetro, tiene que ver con cómo se configuró la trama para su envío del lado de la ECU. El video de Raúl al respecto lo explica muy bien.
        ectIn = buf[3] * 256 + buf[4];
        //Serial.print("ECT_IN: ");
        //Serial.println(ectIn);
        send_serial(ECT_IN_ID, ectIn);
        gear = buf[5];
        //Serial.print("GEAR: ");
        //Serial.println(gear);
        send_serial(GEAR_ID, gear);
        bvolt = buf[6] * 256 + buf[7];
        //Serial.print("BVOLT: ");
        //Serial.println(bvolt);
        send_serial(BVOLT_ID, bvolt);
        //Serial.println("Recibido: 1");
      }
      if (buf[0] == 2) {                                                
        
        //Si la ID del frame es 2, se leen los siguientes datos: LAMBDA, LR_WHEEL_SPEED y ECT_OUT.
        lambda = buf[1] * 256 + buf[2];                                                                  
        //Serial.print("LAMBDA: ");                                        
        //Serial.println(lambda);
        send_serial(LAMBDA_ID, rpm);                                       
        lrWs = buf[3] * 256 + buf[4];
        //Serial.print("LR_WS: ");
        //Serial.println(lrWs);
        send_serial(LR_WS_ID, lrWs);
        ectOut = buf[5];
        //Serial.print("ECT_OUT: ");
        //Serial.println(ectOut);
        send_serial(ECT_OUT_ID, ectOut);
        //Serial.println("Recibido: 2");
      }
      clearBuffer(buf);
    }
  }

  /*-------------FUNCIONES setup() Y loop()-------------*/

  //Gracias al extensivo uso de funciones anidadas entre sí, y arriba explicadas, el setup() y el loop() del programa son muy sencillos:

  //En el setup solo se inicializa el puerto serie en 115200 baudios, que es a el baud rate del CAN programado desde la ECU y se llama a la función start() para inicializar un objeto CAN, la tira LED, y ejecutar la secuencia de inicio de los LED.
  void setup() {
    Serial.begin(115200);
    start();
  }

  //En el loop, habiéndose comprobado que todo se inicializó correctamente, se llama a la función readCanBus() para que verifique si hay mensajes de CAN, y cuando los haya, los procese, y reenvíe los datos a la pantalla HMI.
  void loop() {
    readCanBus();
  }