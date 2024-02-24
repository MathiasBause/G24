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
  #include <FastLED.h>
  #include <CAN.h>
  #include <SPI.h>


  /*-------------PINES DE DATOS-------------*/

  #define LED_PIN 6    //Se define el pin 6 del Arduino como aquel por el que se transmitirán serialmente los datos necesarios a la tira LED.
  #define UPSHIFT 2
  #define DOWNSHIFT 3

  #define TX_GPIO_NUM   21  // Connects to CTX
  #define RX_GPIO_NUM   22  // Connects to CRX


  /*-------------DEFINICIÓN DE LONGITUD DE TIRA LED-------------*/

  #define NUM_LEDS 20  //Se especifica el número de LEDS que tiene la tira que estamos programando


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


  /*-------------DEFINICIÓN DE VARIABLES Y OBJETOS-------------*/

  CRGB leds[NUM_LEDS]; //Se define un array de datos de tipo CRGB (un vector de leds en el que a cada uno se le asocian valores RGB o de otra escala de colores), que representa la tira LED.

  bool ledStripInitialized = false; // Se crea una variable de tipo booleana que sirve como indicador de estado de la inicialización de la tira LED. Por ello, la variable se inicializa como falsa aún.

  byte buttonsData = 0; //Creamos un byte que contendrá la información de los botones a transmitir por CAN.

  volatile bool buttonState = false;
  volatile int pos;

  /*-------------DEFINICIÓN DE FUNCIONES-------------*/

  //Se utiliza para atenuar la intensidad de todos los LEDs de la tira.
  void fadeall() {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].nscale8(175); //Atenúa a un 175/255 de su valor actual la intensidad del LED "i". Para ello, multiplica por el mismo factor los parámetros RGB de dicho LED.
    }
  }

  //Secuencia de inicio que ejecutan los LEDs del volante en el momento en que reciben alimentación.
  //Se produce un efecto de barrido de izquierda a derecha y luego en sentido contrario, alternando los colores azul y naranja para cada uno. Esto se repite 2 veces.
  //Luego se efectúa un rápido parpadeo de los LEDs en azul y naranja.
  void ledsBegin(){
    //Se realizan los barridos arriba mencionados.
    for (int j = 0; j <= 1; j++) {                //La secuencia de barrido se repite 2 veces.
      //El siguiente bucle for produce un efecto en el que parece que un LED azul va recorriendo la tira, dejando una estela.
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(154, 255, 255);            //El LED "i" se coloca en azul (cxpresado en formato HSV).
        FastLED.show();                           //Actualizamos los cambios.
        fadeall();                                //Se reduce la intensidad de todos los LEDs.
        delay(25);                                //Se introduce un retardo de 25ms para que el efecto se aprecie mejor.
      }
      //Se repite el bucle anterior, pero el barrido con la estela se produce en sentido inverso y en color naranja.
      for (int i = (NUM_LEDS)-1; i >= 0; i--) {   
        leds[i] = CRGB(253, 50, 0);
        FastLED.show();
        fadeall();
        delay(25);
      }
    }
    //Se realiza la última parte de la secuencia, en la que, sucesivamente y a intervalos de 100ms, se apagan todos los LEDs, se ponen en azul, se vuelven a apagar, se ponen en naranja, y se vuelven a apagar.
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(100);
    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    FastLED.show();
    delay(100);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(100);
    fill_solid(leds, NUM_LEDS, CRGB(253, 50, 0));
    FastLED.show();
    delay(100);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(100);
  }

  //Lógica de control de la tira LED mientras el coche está encendido
  void ledsVolante(unsigned int rev) {
    if (!ledStripInitialized) {
      return;  //La función termina su ejecución sin realizar nada si la iniciación de la tira LED falla.
    }
    int minRev = 5000;                                      // Valor mínimo de revoluciones
    int maxRev = 11500;                                     // Valor máximo de revoluciones
    int numLedsOn = map(rev, minRev, maxRev, 0, NUM_LEDS);  // El valor de RPM dentro del rango establecido por las 2 variables anteriores se interpola al número de LEDs de la tira a encender.

    if (rev == 0) {                                //Lógica opcional en caso de que se desee que los LEDs realicen algo cuando el coche tiene contacto pero el motor no está arrancado.
    
    } else {                                       //NOTA: Al final de este código se explica en detalle la lógica de la función fill_solid(), que a partir de este punto se usa mucho.
      
      if (rev < minRev) {                          // --- El valor de las RPM está por debajo de minRev ---
        fill_solid(leds, NUM_LEDS, CRGB::Black);   // Apaga todos los LEDs.
      
      } else if (numLedsOn <= NUM_LEDS / 3) {      // --- El valor de las RPM está en el 1er tercio del rango minRev - maxRev (La condición se establece con el número de LEDs a encender, pero recordar que eso está previamente interpolado)---
        fill_solid(leds, NUM_LEDS, CRGB::Black);   // Apaga todos los LEDs para refrescar la tira del estado anterior.
        fill_solid(leds, numLedsOn, CRGB::Green);  // Enciende tantos LEDs como numLedsOn indique, en color VERDE.
      
      } else if (numLedsOn <= 2 * NUM_LEDS / 3) {                                       // --- El valor de las RPM está en el 2er tercio del rango minRev - maxRev ---
        fill_solid(leds, NUM_LEDS, CRGB::Black);                                        // Apaga todos los LEDs para refrescar la tira del estado anterior.
        fill_solid(leds, NUM_LEDS / 3, CRGB::Green);                                    // Enciende los LEDs de todo el 1er tercio de la tira, en color VERDE.
        fill_solid(leds + NUM_LEDS / 3, numLedsOn - NUM_LEDS / 3, CRGB::Red);           // Enciende a partir del 1er LED del 2do tercio de la tira, tantos LEDs como falten para llegar al valor que numLedsOn indique, en color ROJO.
      
      } else if (rev <= maxRev) {                                                       // --- El valor de las RPM está en el 3er tercio del rango minRev - maxRev ---
        fill_solid(leds, NUM_LEDS, CRGB::Black);                                        // Apaga todos los LEDs para refrescar la tira del estado anterior.
        fill_solid(leds, 2 * NUM_LEDS / 3, CRGB::Green);                                // Enciende los LEDs de todo el 1er tercio de la tira, en color VERDE.
        fill_solid(leds + NUM_LEDS / 3, numLedsOn - NUM_LEDS / 3, CRGB::Red);           // Enciende los LEDs de todo el 2do tercio de la tira, en color ROJO.
        fill_solid(leds + 2 * NUM_LEDS / 3, numLedsOn - 2 * NUM_LEDS / 3, CRGB::Blue);  // Enciende a partir del 1er LED del 3er tercio de la tira, tantos LEDs como falten para llegar al valor que numLedsOn indique, en color AZUL.
      
      } else {                                                                          // --- El valor de las RPM está por encima de maxRev ---
        if (millis() % 100 < 50) {                                                      // Se crea un patrón alternado en el que dependiendo de si los milisegundos de ejecución del program al dividirlos entre 100 dan un resultado mayor o menor a 50...
          fill_solid(leds, NUM_LEDS, CRGB::Blue);                                       // ...los LEDs se encienden en color AZUL.
        } else {
          fill_solid(leds, NUM_LEDS, CRGB::Black);                                      // ...los LEDs se apagan.               
        }                                                                               // De esta forma se crea un parpadeo en azul de los LEDS cuando las RPM superan maxRev
      }
      FastLED.show(); //Se actualiza la tira LED para poder visualizar los cambios que se le han hecho desde la última vez que este mismo comando se ejecutó.
      delay(10);
    }
  }

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

    // Se inicializa la tira LED y se revisa que no haya errores.
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS); //Se configura la tira LED como una WS2812, conectada en LED_PIN (definido al inicio del código), y se crea una matriz de tipo CRGB llamada leds, con un número de LEDs igual a NUM_LEDS.
    FastLED.show();              // Se actualizan los cambios para luego comprobar la inicialización.
    ledStripInitialized = true;  // Se asume previamente que la inicialización fue correcta.
    if (leds[0]) {
      Serial.println("Error initializing LED strip!");
      ledStripInitialized = false;  // La inicialización se considera falsa si el valor de leds[0], no es 0, como debería ser al recién estar creada la tira.
    }
    //Si la tira se inicializa correctamente, se fija su brillo, y se ejecuta la secuencia de inicio con ledsBegin()
    if (ledStripInitialized) {
      FastLED.setBrightness(50);
      ledsBegin();
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
        rpm = buf[1] * 256 + buf[2];                                    //Se extrae el dato de las rpm de los bytes correspondientes del búfer.                                    
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
  void toggle(){
    buttonState = !buttonState;
  }
  void writeCan(){
    if(buttonState){
      buttonsData = 128;
    }else{
      buttonsData = 0;
    }
    CAN.beginPacket(0x02);
    CAN.write(buttonsData);
    CAN.endPacket();
  }

  /*-------------FUNCIONES setup() Y loop()-------------*/

  //Gracias al extensivo uso de funciones anidadas entre sí, y arriba explicadas, el setup() y el loop() del programa son muy sencillos:

  //En el setup solo se inicializa el puerto serie en 115200 baudios, que es a el baud rate del CAN programado desde la ECU y se llama a la función start() para inicializar un objeto CAN, la tira LED, y ejecutar la secuencia de inicio de los LED.
  void setup() {
    pinMode(UPSHIFT, INPUT_PULLUP);
    pinMode(DOWNSHIFT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(UPSHIFT), toggle, CHANGE);
    Serial.begin(115200);
    start();
  }

  //En el loop, habiéndose comprobado que todo se inicializó correctamente, se llama a la función readCanBus() para que verifique si hay mensajes de CAN, y cuando los haya, los procese, y reenvíe los datos a la pantalla HMI.
  void loop() {
    readCanBus();
    writeCan();
  }
