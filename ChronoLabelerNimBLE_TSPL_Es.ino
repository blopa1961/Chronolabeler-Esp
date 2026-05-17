/*********************************************************
  Chrono Labeler BLE TSPL
  (c) 2026 Pablo Montoreano

  @file       ChronoLabelerNimBLE_TSPL.ino
  @brief      Impresora de Fecha y Hora
  @author     Pablo Montoreano
  @copyright  2026 Pablo Montoreano
  @version    1.0 - 10/May/26

  Procesador ESP32 Dev Module (opcional: ESP32 mini)
  compilar con: Arduino IDE 2.3.8 partición No OTA (2MB APP/2MB SPIFFS)

  Librerias de Arduino: WiFi, WebServer, Time, Ticker, LittleFS, ESPmDNS

  Librerias de 3ras partes:
  * NimBLE-Arduino por h2zero (Ryan Powell)
  * Rutina de renderizado de fuentes extraida de Thermal_Printer_Library por bitbank2 (Larry Bank)
  * Fuentes importadas de Adafruit GFX library

  Nota: no se utilizan acentos en los comentarios para no generar problemas con unicode o similares formatos
  Sin embargo, las paginas WEB de Chronolabeler respetan los acentos mediante el uso de HTML Entities (ej: "&aacute;", etc.)

*********************************************************/

#define MYGMTOFFSET -10800;     // -3x3600 hora GMT de Argentina
#define MYTIMEZONE "<+03><+03>" // Time Zone de Argentina (default)
#define FORCETP // escanear dispositivos BLE que comienzan con "TP" solamente
#define MYPASS "Blopa1961" // password para la configuracion del Access Point
#define FWVERSION "1.0" // version de firmware
#define NTP_MIN_VALID_EPOCH 1777689600 // Epoch correspondiente a 1 de Mayo de 2026 para validar NTP

/*
  Impresoras termicas G&G o TPL, servicios BLE y UUIDs

// servicio characteristic Serial (aqui es donde enviamos los datos)
static BLEUUID serviceUUID("0000FF10-0000-1000-8000-00805F9B34FB");  // primer characteristic reportada
static BLEUUID charUUID("FFF1");    // Characteristic RX (recepcion serial)

// SERVICE (desconocido)
static BLEUUID serviceUUID("49535343-FE7D-4AE5-8FA9-9FAFD205E455"); // (2da UUID reportada por la impresora)
static BLEUUID charUUID("49535343-8841-43f4-a8d4-ecbe74729bb3");    // (WRITE, WRITE NO RESPONSE)
static BLEUUID charUUID("49535343-1e4d-4bd9-ba61-23c647249616");    // (NOTIFY) config 0x2902

// SERVICE (desconocido)
static BLEUUID serviceUUID("0000FF12-0000-1000-8000-00805F9B34FB"); // (3er UUID reportado por la impresora)
static BLEUUID charUUID("FF13"); // (WRITE, WRITE NO RESPONSE)
static BLEUUID charUUID("FF14"); // (NOTIFY) config 0x2902
static BLEUUID charUUID("FF16"); // (NOTIFY, READ) config 0x2902
static BLEUUID charUUID("FF15"); // (READ, WRITE NO RESPONSE)

// Servicio de Bateria
static BLEUUID serviceUUID("180F");
static BLEUUID charUUID("2a19"); // nivel de bateria (NOTIFY, READ) config 0x2902

// Potencia de transmision
static BLEUUID serviceUUID("1804");
static BLEUUID charUUID("2A07"); // (NOTIFY, READ) config 0x2902

// Informacion del dispositivo
static BLEUUID serviceUUID("180A");
static BLEUUID charUUID("2A24"); // (READ) Numero de Modelo (String)
static BLEUUID charUUID("2A25"); // (READ) Numero de Serie (String)
static BLEUUID charUUID("2A28"); // (READ) Revision de Software (String)
static BLEUUID charUUID("2A29"); // (READ) Nombre del Fabricante (String)
*/

enum WifiStates {
  WIFIOFF,  // Estado WIFI apagado
  WIFIAPM,  // Estado WIFI modo AP
  WIFION,   // Estado WIFI intentando conectar a Router
  WIFICON   // Estado WIFI conectado a Router
};

enum prnStates {
  STATNOPAIR,   // no vinculada
  STATDISCON,   // desconectada
  STATOFFLINE,  // fuera de linea
  STATINVALTIME,  // hora invalida
  STATONLINE,   // en linea
  STATPRINTING  // imprimiendo
};

// Global definitions
static const unsigned int GPIObtn= 23;    // EL BOTON - con pullup de 10K
static const unsigned int GPIOblue= 26;   // LED WiFi (resistor de 1K)
static const unsigned int GPIOyellow= 18; // LED NTP  (resistor de 1K)
static const unsigned int GPIOgreen= 19;  // LED Impresion (resistor de 1K)
static const unsigned int DEFAULTWIFI= WIFIAPM;  // este es el default de Wifi que se guarda en LittleFS si el archivo no existe
static const unsigned int LEDON= HIGH;      // Estado de LED encendido
static const unsigned int LEDOFF= LOW;      // Estado de LED apagado
static const unsigned int LEDWIFION= LEDON; // si el wifi esta conectado el led azul esta prendido/apagado
static const unsigned int LEDWIFIOFF= LEDOFF;
static const unsigned int LEDBLINKOFF= 2; // modo del LED azul, titilando (apagado)
static const unsigned int LEDBLINKON= 3;  // modo del LED azul, titilando (encendido)
static const unsigned int LEDFLASHOFF= 4; // modo del LED azul, flasheando (apagado)
static const unsigned int LEDFLASHON= 5;  // modo del LED azul, flasheando (encendido)
static const unsigned int MYCHANNEL= 6;   // canal de WiFi en modo AP
static const unsigned int MAXCONNCT= 2;   // maxima cantidad de conexiones en modo AP
static const unsigned int BTNPRESSED= LOW;     // apretar un boton manda a GND la señal
static const unsigned int BTNRELEASED= HIGH;
static const unsigned long DEBTIME= 15;        // tiempo de debounce en milisegundos
static const unsigned long HOLDTIME= 1500;     // boton sostenido N mS > funcion "hold" (cambio de modo WiFi)
static const unsigned long BLINKINGTIME= 250;  // titilar N mS
static const unsigned long BLUEBLINK= 500;     // tiempo de encendido y apagado del LED azul en modo titilando
static const unsigned long FLASHTIMEON= 50;    // tiempo de encendido de LED azul flasheando (modo AP)
static const unsigned long FLASHTIMEOFF= 1500; // tiempo apagado del LED azul flasheando
static const unsigned long DEFAULTTIMEOUT= 0;  // timeout de conexion infinito por defecto
static const unsigned long EPOCHRETRY= 500;    // tiempo de retardo para reintento de conexion NTP
// parametros de la impresora
static const byte BLACK= 0;    // color negro de las fuentes
static const byte WHITE= 0xff; // y blanco
static const unsigned int chunkSize= 240; // tamaño de los paquetes transmitidos por BLE
static const unsigned int MAXWIDTHPIXELS= 480;  // 57mm area imprimible x 8 pixels por mm = 456, ajustado a 32bit para los BMPs el ancho debe ser divisible por 4
static const unsigned int MAXHEIGHTPIXELS= 480; // 50mm+10mm espacio de pie de pagina extra x 8 pixels por mm (sin restricciones de multiplos)
static const unsigned int MAXBUFFERSIZE= MAXWIDTHPIXELS / 8 * MAXHEIGHTPIXELS;  // tamaño maximo del buffer en bytes
static const unsigned int MAXPROFILES= 12; // cantidad maxima de perfiles
static const unsigned long INITDELAY= 350;

// filenames
#define MYLOGO "/BlopaLogo.webp"      // mi logo
#define ICON32 "/Favicon32.png"       // Favicon para PC
#define ICON192 "/Favicon192.png"     // Favicon para Android (probada en Firefox y Chrome)
#define FILEPRNCFG "/PrnCfg.ini"      // archivo de configuracion de impresoras
#define FILEDATETIME "/DateTime.ini"  // archivo de configuracion Dia/Fecha/NTP
#define FILEWIFI "/WifiCfg.ini"       // archivo de configuracion de WiFi
#define FILESSID "/SsId.ini"          // archivo de nombre del SSID

// includes
#include "ChonoLabeler.h" // paginas WEB
#include "WifiConf.h"     // pagina WEB de configuracion de WiFi
#include "about.h"        // pagina WEB acerca de
#include <time.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Ticker.h>
#include <LittleFS.h>
#include <NimBLEDevice.h>

// variables bluetooth
static const unsigned int BLETIMEOUT= 12000;  // tiempo maximo de espera de conexion de impresora en mS
static NimBLEUUID serviceUUID("0000FF10-0000-1000-8000-00805F9B34FB");  // primera "characteristic" reportada por la TP-220 y TP-110
static NimBLEUUID charUUID("FFF1"); // RX Characteristic
static NimBLERemoteCharacteristic* pCharacteristic;
static NimBLEClient* pClient;
static NimBLERemoteService* pService;
static NimBLEScan* pScan;
static const unsigned int SCANTIME= 5000;   // tiempo maximo de busqueda de dispositivos BLE
static const unsigned int MAXDEVICES= 20;   // maxima cantidad de dispositivos BLE a escanear
static volatile unsigned int numDevices;    // numero de dispositivos BLE encontrados (pueden ser filtrados por FORCETP)
static const unsigned int minBleNameLength= 4;  // cantidad minima de caracteres para considerar valido el nombre BLE de la impresora ("TP-x")
static String foundDevices[MAXDEVICES];     // deberia ser volatile pero el compilador no lo permite
static NimBLEAddress foundMacs[MAXDEVICES]; // direcciones MAC de los dispositivos BLE encontrados
static const NimBLEAddress nullAddr{};      // direccion Nimble null (impresoras no vinculadas)

struct tm timeinfo; // variables NTP

struct swiCfg {
  unsigned int initialWifi; // modo inicial de WiFi
  unsigned long retryWifi;  // intentos de reconexion WiFi (0= infinito)
  byte staticIP[4];   // IP fijo (o 0.0.0.0 para DHCP)
  byte myGateway[4];  // gateway del router
  byte subnet[4];     // mascara de subred
};

struct prnCfg {
  unsigned int WIDTHmm;   // ancho maximo del sticker en mm
  unsigned int HEIGHTmm;  // alto maximo del sticker en mm
  unsigned int extraFooter;   // pie de pagina extra
  unsigned int topMargin[2];  // margen superior lineas 1 y 2
  unsigned int leftMargin;    // margen izquierdo
  unsigned int chunkDelay;    // retardo de impreson entre paquetes BLE
  unsigned int resetDelay;    // retardo de reconexion
  unsigned int fontNumber[2]; // fuente seleccionada (se pueden usar distintas fuentes para las 2 lineas)
  uint8_t address[6]; // MAC address de la impresora en uso
  char prnBtName[30]; // nombre BLE de la impresora
  char prnGap[10];    // codigos de separacion de etiquetas
  char prnLabel[26];  // nombre del perfil de esta impresora
  char prnInit[11];   // secuencia de inicializacion
  bool landscape;     // tipo de impresora (TP-110 apaisada, TP-220 vertical)
  bool blackBackground; // el fondo es negro?
  bool centered;      // centrado (ignorar margen izquierdo)
};

struct prnCalc {  // variables que no hace falta guardar en la configuracion (se calculan durante la carga)
  int32_t bitmapWidth;           // (WIDHmm+ExtraFooter)*8
  int32_t bitmapHeight;          // HEIGHTmm*8
  unsigned int myBufferSize;     // tamaño en uso del buffer (bitmapWidth*bitmapHeight/8)
  unsigned int prnBitmapWidth;   // landscape ? WIDTHmm : HEIGHTmm
  unsigned int prnBitmapHeight;  // landscape ? bitmapHeight : bitmapWidth
  String prnBitHeader;  // Header del bitmap para la impresora
  String sPrnInit;      // inicializacion de la impresora (esc !S)
  String prnHeader;     // encabezado de impresion (SIZE 48...)
  String prnFooter;     // pie de impresion (PRINT 1,1...)
};

struct dateTimeCfg {
  byte dtFormats[8];    // formatos de fecha y hora
  long gmtOffset;       // UTC en milisegundos
  long daylight;        // horario de verano, para agregar 1 hora cambiar a 3600
  char ntpServer[31];   // nombre del server NTP
  char timeZone[21];    // time Zone
};

// definicion de encabezado BMP
static const unsigned int BMPHEADERSIZE= 62;
#pragma pack(push, 1)     // Asegurar alineacion a byte (no 32 bits)
struct BMPHeader {
    uint16_t bfType;          // Magic identifier: 'BM' (0x4D42)
    uint32_t bfSize;          // Tamaño del archivo en bytes
    uint16_t bfReserved1;     // Reservado, tiene que ser 0
    uint16_t bfReserved2;     // Reservado, tiene que ser 0
    uint32_t bfOffBits;       // Offset a datos de pixel
    uint32_t biSize;          // Tamaño del encabezado (40 bytes)
    int32_t  biWidth;         // Ancho de la imagen en pixels
    int32_t  biHeight;        // Alto de la imagen en pixels (positivo = de abajo hacia arriba)
    uint16_t biPlanes;        // tiene que ser 1
    uint16_t biBitCount;      // 1 para 1bpp  (2^1)
    uint32_t biCompression;   // 0 = BI_RGB (no comprimido)
    uint32_t biSizeImage;     // Tamaño de dato del pixel (puede ser 0 si no tiene compresion)
    int32_t  biXPelsPerMeter; // X resolucion (BPP)
    int32_t  biYPelsPerMeter; // Y resolucion (BPP)
    uint32_t biClrUsed;       // 0 = 2^biBitCount (para 1bpp, 2 colores)
    uint32_t biClrImportant;  // 0 = todos los colores son importantes
    uint32_t colorBlack;      // 0
    uint32_t colorWhite;      // 255,255,255,0 (little Endian -> 0x0FFFFFF)
};
#pragma pack(pop) // fin de alineacion empacada

// bitmap para prvisualizacion
static BMPHeader myBmpHeader; // encabezado
static uint8_t myBitmap[MAXBUFFERSIZE]; // reserva de RAM para el buffer
// configuracion global de dia y hora
static dateTimeCfg myDT, tmpDT;  // configuracion de fecha y hora (archivo FILEDATETIME)
// variables de la impresora
static prnCfg myPrn, tmpPrn;  // todos los parametros no calculados de la impresora
static prnCalc myPrnCalc;     // y los calculados
static prnCfg prnProfiles[MAXPROFILES]; // perfiles de impresora (archivo FILEPRNCFG)
static unsigned int myStatus;       // estado actual (No Vinculada, En linea, etc.)
static unsigned int numProfiles;    // numero de perfiles definidos
static unsigned int selectedPrn;    // impresora seleccionada
static unsigned int bmpWidth4;      // ancho del bitmap en multiplos de 4 bytes
static unsigned int fullBufferSize; // tamaño del footer en bytes
static bool prnIsOnline;    // flag de impresora conectada por BLE
static unsigned int bfrPos; // posicion del buffer de impresion
static unsigned long ptrSize, chunks; // tamaño del paquete BLE actual
static uint8_t* ptrStart;   // puntero al principio del buffer de impresion
static String lineaMain[2]; // textos a imprimir (de la pagina WEB principal)

// fuentes y rutina de rendering
#include "PrintFonts.h" // este archivo depende de declaraciones de variables, no moverlo al principio
#include "Fonts/FreeSansBold18pt7b.h" // alto= 25 pixels
#include "Fonts/FreeSansBold24pt7b.h" // alto= 35 pixels
#include "Fonts/FreeMonoBold18pt7b.h" // alto= 28 pixels
#include "Fonts/FreeMonoBold24pt7b.h" // alto= 31 pixels

static const char fontNames[][17]= {"Sans Bold 18","Sans Bold 24","Mono Bold 18","Mono Bold 24"};
static const unsigned int numFonts= 4;  // cantidad de fuentes
static const char *mySSID = "Chrono Labeler";  // nombre de mi ESP32
static const char *passwordAP= MYPASS;  //clave de configuracion

IPAddress staticIP(0,0,0,0);      // IP de router (0 -> DHCP)
IPAddress myGateway(0,0,0,0);
IPAddress subnet(255,255,255,0);
IPAddress staticIPAP(10,0,0,1);   // IP en modo access point
IPAddress myGatewayAP(10,0,0,1);
IPAddress subnetAP(255,255,255,0);

static const char statusNames[][22]= {"No Vinculada", "Desconectada", "Fuera de l&iacute;nea", "Hora Inv&aacute;lida", "En l&iacute;nea", "Imprimiendo"};
static const char diaSem[][3]= {"Do","Lu","Ma","Mi","Ju","Vi","Sa"};  // dias de la semana
static const char dateFormats[][3]= {"DD","MM","AA","--"}; // formatos fechas
static const char separators[][8]= {"/", ".", ",", ":", "-", "espacio", "letra","--"}; // separadores de dia y hora
static const unsigned int numSeparators= sizeof(separators) / sizeof(separators[0]) ; // cantidad de separadores
static const char hourFormats[][4]= {"H12","H24","MM","--"}; // formato de horas
static const unsigned int numHourFormats= sizeof(hourFormats) / sizeof(hourFormats[0]); // cantidad de formatos de hora
static const char letterFormats[][2]= {"d", "m", "a", "h", "m"};  // letra como separador ej: 10h23m
/*
contenido de dtFormats:
    | 0       | 1       | 2       | 3       | 4       | 5        | 6        | 7      |
PRG | pulldt0 | pulldt1 | pulldt2 | pulldt3 | pulldt4 | pulldt5  | pulldt6  | diasem |
WEB | PULLD1R | PULLD2R | PULLD3R | PULLT1R | PULLT2R | PULLDSEP | PULLTSEP | SEMANA |
----+---------+---------+ --------+ --------+ --------+ ---------+ ---------+------- +
V 0 | --      | --      | --      | --      | --      | /        | /        | T/F    |
a 1 | DD      | DD      | DD      | H12     | H12     | .        | .        |        |
l 2 | MM      | MM      | MM      | H24     | H24     | ,        | ,        |        |
o 3 | AA      | AA      | AA      | MM      | MM      | :        | :        |        |
r 4 |         |         |         |         |         | -        | -        |        |
e 5 |         |         |         |         |         | espacio  | espacio  |        |
s 6 |         |         |         |         |         | letra    | letra    |        |
*/
static const byte defaultFormats[8]= {0,1,2,1,2,0,3,1};   // formato por defecto DD/MM/AA - Dia de la semana - H24:MM
//static const byte defaultFormats[8]= {1,0,2,0,2,4,3,1}; // formato americano MM-DD-AA - Dia de la semana - H12:MM
static const char commonNTPs[][16]={"ar.pool.ntp.org", "pool.ntp.org", "us.pool.ntp.org", "time.nist.gov"};
static const char numNTPs= sizeof(commonNTPs) / sizeof(commonNTPs[0]);  // cantidad de servidores NTP definidos
static const unsigned int DEFAULTNTP= 0;    // NTP por defecto es el de Argentina
static const unsigned int MINNTPLENGTH= 12; // minima longitud del server NTP
static const long DEFAULTGMTOFFSET= MYGMTOFFSET; // local time GMT offset
static const int DEFAULTDAYLIGHT= 0;  // horario de verano por defecto
static const int MINGMT= -12; // valor minimo de huso horario
static const int MAXGMT= 14;  // valor maximo
static const int MINDAYLIGHT= -1; // valor minimo de horario de verano
static const int MAXDAYLIGHT= 2;  // maximo
static GFXfont* fontPtr[numFonts];  // punteros a fuentes
static unsigned int fontHeight[numFonts]= {25, 35, 28, 31};  // alturas de las fuentes en pixels (para calcular los margenes, la fuentes GFX se dibujan de abajo hacia arriba)
static time_t now;  // hora NTP
static String myDate[2]; // fecha y hora actuales formateadas
static bool timeIsValid, yelIsBlinking; // validacion de NTP y estado del LED amarillo
static bool mDNS; // flag para inicializar mDNS una sola vez
Ticker blinkerBlue, blinkerYellow, blinkerGreen;  // variables para hacer titilar los LEDs
static int oldInp;    // ultimo estado del boton
static int boton;     // estado actual del boton
static bool btnHeld;  // si mantuvo el boton apretado
static unsigned long debounce;      // tiempo transcurrido desde ultimo cambio de estado del boton
static unsigned long milliLoop;     // variable temporal de timer durante loop principal (contiene los millis al inicio del loop)
static unsigned long debounceTime;  // variable para calcular delta tiempo (boton)
static unsigned long retryWifi;     // reintento de conexion WiFi
static unsigned long lastTmTry;     // reintento de obtener la hora por NTP
static int lectura;   // lectura del port del boton
static unsigned int wifiState; // estado actual del WiFi (ver enum WifiStates)
static volatile unsigned int blinkStateB, blinkStateG, blinkStateY; // estado de los LEDs
static File dataFile;   // archivo en uso
static unsigned int bytesRead;   // cantidad de bytes leidos
static String fileName; // nombre del archivo en uso (solo 1 abierto por vez)
static swiCfg savedCfg; // archivo de configuracion de WiFi (FILEWIFI)
static String ssid;     // SSID guardada (archivo FILESSID)
static String password; // password del router (archivo FILESSID)
static int i, j; // contadores generales

WebServer server(80); // server de mi pagina WEB

class MyScanCallbacks : public NimBLEScanCallbacks {  // rutina callback de escaneo de dispositivos BLE
String myName;

  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
    std::string deviceName= advertisedDevice->getName();
    myName= String(deviceName.c_str());
#ifdef FORCETP
    if ((myName.length() >= 2) && (myName.startsWith("TP"))) {  // filtrar nombres BLE que no comienzan con "TP"
#else
    if (myName.length() != 0) {
#endif      
      if (numDevices < MAXDEVICES) {  // se encontro un dispositivo, guardarlo
        foundDevices[numDevices]= myName;
        foundMacs[numDevices]= advertisedDevice->getAddress();
        numDevices= numDevices + 1; // no se puede usar ++ en variables volatiles
      }
      else { // limite de dispositivos superados
        foundDevices[numDevices - 1]= "Demasiados Dispositivos";
        foundMacs[numDevices - 1]= nullAddr;
        pScan->stop();
      }
    }
  }
};

// titilar LED amarillo
void IRAM_ATTR blinkeaY() {
  blinkStateY= (blinkStateY == LEDON) ? LEDOFF : LEDON;
  digitalWrite(GPIOyellow, blinkStateY);
}

// titilar LED verde
void IRAM_ATTR blinkeaG() {
  blinkStateG= (blinkStateG == LEDON) ? LEDOFF : LEDON;
  digitalWrite(GPIOgreen, blinkStateG);
}

// titilar o flashear LED azul
void IRAM_ATTR blinkeaB() {
  switch (blinkStateB) {
    case LEDBLINKON:
      digitalWrite(GPIOblue, LEDOFF);
      blinkStateB= LEDBLINKOFF;
      break;
    case LEDBLINKOFF:
      digitalWrite(GPIOblue, LEDON);
      blinkStateB= LEDBLINKON;
      break;
    case LEDFLASHON:
      blinkerBlue.attach_ms(FLASHTIMEOFF , blinkeaB);
      digitalWrite(GPIOblue, LEDOFF);
      blinkStateB= LEDFLASHOFF;
      break;
    case LEDFLASHOFF:
      blinkerBlue.attach_ms(FLASHTIMEON , blinkeaB);
      digitalWrite(GPIOblue, LEDON);
      blinkStateB= LEDFLASHON;
      break;
    default:
      blinkerBlue.detach();
      digitalWrite(GPIOblue, LEDOFF);
      blinkStateB= LEDOFF;
      break;
  }
}

// actualizacion del LED azul
void updateBlueLED(int newState) {
  blinkStateB= newState;
  blinkerBlue.detach();
  switch (newState) {
    case LEDOFF:
      digitalWrite(GPIOblue, LEDOFF);
      break;
    case LEDON:
      blinkStateB= LEDON;
      digitalWrite(GPIOblue, LEDON);
      break;
    case LEDBLINKON:
      blinkerBlue.attach_ms(BLUEBLINK , blinkeaB);
      digitalWrite(GPIOblue, LEDON);
      break;
    case LEDFLASHON:
      blinkerBlue.attach_ms(FLASHTIMEON , blinkeaB);
      digitalWrite(GPIOblue, LEDON);
      break;
    default:
      newState= LEDOFF;
      digitalWrite(GPIOblue, LEDOFF);
      break;
  }  
}

// validacion de mascara de WiFi, guardar en IPAddress
bool validMask(String &mascara, IPAddress &validado) {
  unsigned int posi;
  unsigned int indice;
  bool expectDigit= true;
  if ((mascara.length() < 7) || (mascara.length() > 15)) return false;
  posi= 0;
  for (indice= 0; indice < mascara.length(); indice++) {
    if (expectDigit){
      expectDigit= false;
      if (isDigit(mascara.charAt(indice)))
        validado[posi]= (mascara.charAt(indice) - '0');
      else return false;
    }
    else {
      if (isDigit(mascara.charAt(indice))) {
        if (validado[posi] > 25) return false;
        validado[posi]*= 10;
        if ((validado[posi] == 250) && ((mascara.charAt(indice)) > '5')) return false;
        validado[posi]+= (mascara.charAt(indice) - '0');
      }
      else {
        if (mascara.charAt(indice) != '.') return false;
        expectDigit= true;
        posi++;
        if (posi>3) return false;
      }
    }
  }
  return true;
}

bool esAsteriscos(String &clave) {  // chequear si la clave es todo asteriscos
unsigned int ind;

  for (ind= 0; ind < clave.length(); ind++) {
    if (clave.charAt(ind) != '*') return false;
  }
  return true;
}

String typeOfEnc(int tipo) {  // tipo de encripcion de WiFi
  switch (tipo) {
  case WIFI_AUTH_OPEN:
    return "&nbsp;---";
    break;
  case WIFI_AUTH_WEP:
    return "WEP";
    break;
  case WIFI_AUTH_WPA_PSK:
    return "WPA";
    break;
  case WIFI_AUTH_WPA2_PSK:
    return "WPA2";
    break;
  case  WIFI_AUTH_WPA_WPA2_PSK:
    return "WPA+WPA2";
    break;
  case WIFI_AUTH_WPA3_PSK:
    return "WPA3";
    break;
  }
  return "ERROR";
}

void respondeConfigWifi() { // Generar pagina de configuracion de WiFi
  unsigned int cantIDs;
  unsigned int ilista;
  String listaSSIDs;
  String miConfig= wifiCfg;

  miConfig.replace("defaultSSID", ssid);
  miConfig.replace("defaultPASS", (password == "") ? "" : "********");
  if (staticIP[0] == 0) {
    miConfig.replace("staticIP", "");
    miConfig.replace("myGateway", "");
  }
  else {
    miConfig.replace("staticIP", staticIP.toString());
    miConfig.replace("myGateway", myGateway.toString());
  }
  miConfig.replace("subnet", (subnet[0] == 0) ? "" : subnet.toString());
  miConfig.replace("TOUT", String(savedCfg.retryWifi / 60000));
  switch(savedCfg.initialWifi) {
    case WIFIAPM:
      miConfig.replace("CHECKAPM"," checked=\"checked\"");
      miConfig.replace("CHECKRTR","");
      miConfig.replace("CHECKOFF","");
      break;
    case WIFION:
      miConfig.replace("CHECKAPM","");
      miConfig.replace("CHECKRTR"," checked=\"checked\"");
      miConfig.replace("CHECKOFF","");
      break;
    default:
      miConfig.replace("CHECKAPM","");
      miConfig.replace("CHECKRTR","");
      miConfig.replace("CHECKOFF"," checked=\"checked\"");
      break;
  }
  if (server.arg("scan") == "1") {
    listaSSIDs= F("<br><p style=\"font-size:20px\"><b>Redes WiFi detectadas:</b><br>");
    cantIDs= WiFi.scanNetworks();
    if (cantIDs != -1) {
      listaSSIDs+= F("<table><tr><th>Seleccione el SSID</th><th>Se&ntilde;al</th><th>Encripci&oacute;n</th></tr>");
      for(ilista=0; ilista<cantIDs; ilista++) {
        listaSSIDs+= F("<tr><td><p style=\"font-size:15px\">&nbsp;");
        listaSSIDs+= F("<a href= \"javascript:window.scrollTo(0,0);\" onclick=\"document.getElementById('elssid').value = this.innerText\">");
        listaSSIDs+= WiFi.SSID(ilista);
        listaSSIDs+= F("</a>&nbsp;</td><td>&nbsp;");
        listaSSIDs+= WiFi.RSSI(ilista);
        listaSSIDs+= F("dBm&nbsp;</td><td>&nbsp;");
        listaSSIDs+= typeOfEnc(WiFi.encryptionType(ilista));
        listaSSIDs+= "</td><tr>";
      }
      listaSSIDs+= "</table></p>";
    }
    else listaSSIDs+= F("***Ninguna***</p>");
    listaSSIDs+= F("<br><input type=\"button\" value=\"Escanear Redes\" onclick=\"location.reload();\">");
    listaSSIDs+= ScrollEnd;
  }
  else listaSSIDs= F("<br><input type=\"button\" value=\"Escanear Redes\" onclick=\"location.replace('/configWifi?scan=1')\">");
  miConfig.replace("PLACEHLD", listaSSIDs);
  server.send(200, "text/html", miConfig);
}

void respondeGuardaWifi() {  // validar y guardar configuracion WiFi
  IPAddress newIP, newGate, newMask;
  bool todoValido, reconectar, cambioExplorador;
  String newSSID, newPass, newCfg;
  unsigned int newIniWifi;
  unsigned long newRetryWifi;
  String iniWifi, ipTemp, gateTemp, maskTemp;
  String invalpar= F("<td>&nbsp;Inv&aacute;lido&nbsp;</td>");

  reconectar= false;
  cambioExplorador= false;
  todoValido= true;
  newCfg= CfgHeader;
  newCfg+= F("<table><tr><td><b>SSID: </b></td>");
  newSSID= server.arg("mySSID");
  if (newSSID.length() == 0) {
    newCfg+= invalpar;
    todoValido= false;
  }
  else newCfg+= "<td>&nbsp;" + newSSID + "&nbsp;</td>";
  newCfg+= F("</tr><tr><td><b>Password:&nbsp;</b></td>");
  newPass= server.arg("myPASS");
  if (newPass.length() == 0 ) {
    newCfg+= invalpar;
    todoValido= false;
  }
  else newCfg+= F("<td>&nbsp;********&nbsp;</td>");
  ipTemp= server.arg("myIP");
  maskTemp= server.arg("myMask");
  gateTemp= server.arg("myGate");
  if ((validMask(ipTemp, newIP)) && (validMask(gateTemp, newGate)) && (validMask(maskTemp, newMask))) {
    if ((newIP[0] == 0) || (newIP[3] == 0) || (newGate[0] == 0) || (newGate[3] == 0) || (newMask[0] == 0)) {
      // invalidos! -> DHCP!
      newIP= IPAddress(0,0,0,0);
      newCfg+= F("</tr><tr><td><b>IP:&nbsp;</b></td><td>&nbsp;DHCP&nbsp;</td>");
    }
    else {
      newCfg+= F("</tr><tr><td><b>IP:&nbsp;</b></td>");
      newCfg+= "<td>&nbsp;" + newIP.toString() + "&nbsp;</td>";
      newCfg+= F("</tr><tr><td><b>Gateway:&nbsp;</b></td>");
      newCfg+= "<td>&nbsp;" + newGate.toString() + "&nbsp;</td>";
      newCfg+= F("</tr><tr><td><b>M&aacute;scara:&nbsp;</b></td>");
      newCfg+= "<td>&nbsp;" + newMask.toString() + "&nbsp;</td>";
    }
  }
  else {
    // DHCP!
    newIP= IPAddress(0,0,0,0);
    newCfg+= F("</tr><tr><td><b>IP:&nbsp;</b></td><td>&nbsp;DHCP&nbsp;</td>");
  }
  newRetryWifi= server.arg("timeout").toInt() * 60000;
  newCfg+= F("</tr><tr><td><b>Timeout (min):&nbsp;</b></td><td>&nbsp;");
  if (newRetryWifi == 0)
    newCfg+= "Infinito";
  else newCfg+= "&nbsp;" + String(newRetryWifi / 60000);
  newCfg+= "&nbsp;</td>";
  iniWifi= server.arg("inicio");
  newCfg+= F("</tr><tr><td><b>Inicio WiFi:&nbsp;</b></td><td>&nbsp;");
  if (iniWifi == "router") {
    if (todoValido) {
      newIniWifi=WIFION;
      newCfg+= F("Conexi&oacute;n a Router");
    }
    else {
      newIniWifi=WIFIAPM;
      newCfg+= F("Local");
    }
  }
  else if (iniWifi == "ap") {
    newIniWifi= WIFIAPM;
    newCfg+= "Modo Access Point";
  }
  else {
    newIniWifi= WIFIOFF;
    newCfg+= "Apagado";
  }
  newCfg+="&nbsp;</td></tr></table></p>";
  if (esAsteriscos(newPass)) newPass= password;
  if (todoValido) {
    if ((staticIP == newIP) && (myGateway == newGate) && (subnet == newMask) && (savedCfg.retryWifi == newRetryWifi) && (savedCfg.initialWifi == newIniWifi) && (ssid == newSSID) && (password == newPass))
      newCfg+= F("<br><p style=\"font-size:20px\"><b>La configuraci&oacute;n no ha cambiado.</b><br><br><input type=\"button\" value=\"Volver a configuraci&oacute;n\" onclick=\"history.back();\">");
    else {
      reconectar= true;
      newCfg+= F("<br><p style=\"font-size:20px\"><b>La nueva configuraci&oacute;n ha sido<br>guardada</b>");
      if (((wifiState == WIFICON) && (newIniWifi != WIFION)) || ((wifiState != WIFICON) && (wifiState != newIniWifi))) {
        cambioExplorador= true;
        newCfg+= F("<br>Cambiando de Modo.");
      }
      else if ((staticIP != newIP) || (ssid != newSSID) || (password != newPass)) {
        cambioExplorador= true;
        newCfg+= F("<br>Cambiando de Router o IP.");
      }
      else newCfg+= F("<br><br><b></p><input type=\"button\" value=\"Aceptar\" onclick=\"history.go(-2);\"><br>");
    }
  }
  else {
    if (newSSID.length() == 0) {
      newIP= IPAddress(0,0,0,0);
      newGate= IPAddress(0,0,0,0);
      newMask= IPAddress(255,255,255,0);
      newIniWifi= WIFIAPM;
      newRetryWifi= DEFAULTTIMEOUT;
      newSSID="";
      newPass="";
      if ((staticIP == newIP) && (myGateway == newGate) && (subnet == newMask) && (savedCfg.retryWifi == newRetryWifi) && (savedCfg.initialWifi == newIniWifi) && (ssid == newSSID) && (password == newPass))
        newCfg+= F("<br><p style=\"font-size:20px\"><b>La configuraci&oacute;n no ha cambiado.</b><br><br><input type=\"button\" value=\"Volver a configuraci&oacute;n\" onclick=\"history.back();\">");
      else {
        newCfg= CfgHeader;
        newCfg+= F("<br><p style=\"font-size:20px\"><b>Configuraci&oacute;n Borrada.</b></p><br><br>");
        reconectar= true;
        if (wifiState == WIFIAPM)
          newCfg+= F("<b><input type=\"button\" value=\"Aceptar\" onclick=\"history.go(-2);\"><br>");
        else {
          newCfg+= F("<br>Cambiando a Modo AP.");
          cambioExplorador= true;
        }
      }
    }
    else newCfg+= F("<br><p style=\"font-size:20px\"><b>Configuraci&oacute;n Inv&aacute;lida.</b><br><br><input type=\"button\" value=\"Volver a configuraci&oacute;n\" onclick=\"history.back();\">");
  }
  if (cambioExplorador) newCfg+= F(" Esta p&aacute;gina<br>ya no es v&aacute;lida.<br><br><b>*** Cierre el explorador ***</b><br>");
  newCfg+="</body></html>";
  server.send(200, "text/html", newCfg);
  if (reconectar) {
    staticIP= newIP;
    myGateway= newGate;
    subnet= newMask;
    for (i=0; i<4; i++) {
      savedCfg.staticIP[i]= staticIP[i];
      savedCfg.myGateway[i]= myGateway[i];
      savedCfg.subnet[i]= subnet[i];
    }
    savedCfg.initialWifi= newIniWifi;
    savedCfg.retryWifi= newRetryWifi;
    ssid= newSSID;
    password= newPass;
    if (newSSID.length() == 0) {
      fileName= FILEWIFI;
      if (LittleFS.exists(fileName)) LittleFS.remove(fileName.c_str());
      fileName= FILESSID;
      if (LittleFS.exists(fileName)) LittleFS.remove(fileName.c_str());
      if (cambioExplorador) {
        delay(2000);
        nuevoModoWiFi(WIFIOFF);
        delay(2000);
        nuevoModoWiFi(savedCfg.initialWifi);
      }
    }
    else {
      fileName= FILEWIFI;
      dataFile = LittleFS.open(fileName.c_str(), "w");
      dataFile.write((byte *)&savedCfg, sizeof(savedCfg));
      dataFile.close();
      fileName= FILESSID;
      dataFile= LittleFS.open(fileName.c_str(), "w");
      dataFile.println(ssid);
      dataFile.println(password);
      dataFile.close();
      if (cambioExplorador) delay(2000);
      nuevoModoWiFi(WIFIOFF);
      delay(2000);
      nuevoModoWiFi(savedCfg.initialWifi);
    }
  }
}

void respondePreview() {  // generar BMP para mostrar en la pagina de previsualizacion utilizando tmpPrn en lugar de myPrn
  String miWeb= Preview;

  lineaMain[0]= server.arg("line0");
  lineaMain[1]= server.arg("line1");
  switch (server.arg("prvw").toInt()) {
    case 7: // previsualizacion de pagina principal
      selectedPrn= server.arg("selPrn").toInt();
      tmpPrn= prnProfiles[selectedPrn];
      tmpPrn.blackBackground= (server.arg("color").toInt() == 1);
      tmpPrn.centered= (server.arg("centrar") == "on");
      tmpPrn.fontNumber[0]= server.arg("pullf0").toInt();;
      tmpPrn.fontNumber[1]= server.arg("pullf1").toInt();
      recalcPrinterHeaders(tmpPrn);
      if (lineaMain[0].length() == 0) // si el texto de la primer linea de la pagina principal esta vacio mostrar la hora
        formatDT(myDT, tmpPrn);
      else
        buildBitmap(tmpPrn, lineaMain);
      break;
    case 8: // previsualizacion de configuracion impresora
      loadPrnFromWeb(tmpPrn);
      recalcPrinterHeaders(tmpPrn);
      formatDT(myDT, tmpPrn);
      break;
    case 9: // previsualizacion de configuracion fecha y hora
      loadDtFromWeb(tmpDT);
      tmpPrn= prnProfiles[0];
      recalcPrinterHeaders(tmpPrn);
      formatDT(tmpDT, tmpPrn);
      break;
  }
  miWeb.replace("myWI", String(myPrnCalc.bitmapWidth));
  miWeb.replace("myHE", String(myPrnCalc.bitmapHeight));
  server.send(200, "text/html", miWeb);
}

void countProfiles() {  // contar perfiles
  numProfiles= 0;
  for (i=0; i < MAXPROFILES; i++) {  // contar perfiles en uso
    if (prnProfiles[i].prnLabel[0] == 0) break;
    numProfiles++;
  }
}

void setPrnOffline() {  // sacar de linea la impresora (para cambiar de perfil)
  if (pClient != nullptr) {
    if (pClient-> isConnected()) pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);
    pClient= nullptr;
  }
  prnIsOnline= false;
}

void updateStatus() { // actualizar el estado que se muestra en la pagina WEB principal
  if (macIsZeros(myPrn.address))
    myStatus= STATNOPAIR;
  else if (prnIsOnline)
    myStatus= STATONLINE;
  else
    myStatus= STATDISCON;
}

void respondeConfigPrn() {  // Generar pagina WEB de configuracion de impresora
  unsigned int ilista, newPrn;
  String listaPRNs, selecc;

  String miWeb= ConfPrn;
  newPrn= server.arg("selCfgPrn").toInt();
  if (newPrn != 0) {
    selectedPrn= newPrn - 1;
    setPrnOffline();
    myPrn= prnProfiles[selectedPrn];
    recalcPrinterHeaders(myPrn);
  }
  selecc= String(selectedPrn + 1)+"/"+String(numProfiles)+" (Max="+String(MAXPROFILES)+")";
  miWeb.replace("NUMPERF", selecc);
  miWeb.replace("myPROF", String(myPrn.prnLabel));
  miWeb.replace("myNAME", String(myPrn.prnBtName));
  miWeb.replace("PAIRED", macIsZeros(myPrn.address) ? "&nbsp;No" : "");
  miWeb.replace("WMM", String(myPrn.WIDTHmm));
  miWeb.replace("HMM", String(myPrn.HEIGHTmm));
  miWeb.replace("EXFT", String(myPrn.extraFooter));
  miWeb.replace("MAXSUP", String(myPrn.HEIGHTmm << 3));
  miWeb.replace("MS1", String(myPrn.topMargin[0]));
  miWeb.replace("MS2", String(myPrn.topMargin[1]));
  miWeb.replace("MIZQ", String(myPrn.leftMargin));
  miWeb.replace("CENTERCHECK", myPrn.centered ? "checked" : "");
  miWeb.replace("CDELAY", String(myPrn.chunkDelay));
  miWeb.replace("RDELAY", String(myPrn.resetDelay));
  miWeb.replace("CHECKWHITE", myPrn.blackBackground ? "" : " checked=\"checked\"");
  miWeb.replace("CHECKBLACK", myPrn.blackBackground ? " checked=\"checked\"" : "");
  miWeb.replace("CHECKLAND", myPrn.landscape ? " checked=\"checked\"" : "");
  miWeb.replace("CHECKPORT", myPrn.landscape ? "" : " checked=\"checked\"");
// pulldown perfiles
  selecc= "";
  for (i= 0; i < numProfiles; i++) {
    selecc+= "<option value=\""+String(i+1)+"\"";
    if (selectedPrn == i) selecc+= " selected";
    selecc+= ">"+String(prnProfiles[i].prnLabel)+"</option>\r\n";
  }
  miWeb.replace("PULLCFG", selecc);
  // pulldowns fuentes
  for (j= 0; j < 2; j++) {
    selecc= "";
    for (i= 0; i < numFonts; i++) {
      selecc+= "<option value=\""+String(i)+"\"";
      if (myPrn.fontNumber[j] == i) selecc+= " selected";
      selecc+= ">"+String(fontNames[i])+"</option>\r\n";
    }
    miWeb.replace("PULLFR"+String(j), selecc);
  }
// escanear dispositivos BLE
  numDevices= 0;
  if (server.arg("scan")== "1") {
    pScan= NimBLEDevice::getScan();
    pScan->clearResults();  // en caso de que ya hayan escaneado
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(100);
    pScan->setScanCallbacks(new MyScanCallbacks()); 
    pScan->start(SCANTIME);
    listaPRNs= F("<br><p style=\"font-size:20px\"><b>Dispositivos BLE detectados:</b><br>");
    while (pScan->isScanning()) yield();  // esperar a que termine el escaneo BLE asincronico
    if (numDevices != 0) {  // generar links de la lista de impresoras encontradas
      listaPRNs+= F("<table><tr><th><p style=\"font-size:20px\">&nbsp;Seleccione la Impresora&nbsp;&nbsp;</th></tr>");
      for(ilista=0; ilista < numDevices; ilista++) {
        listaPRNs+= F("<tr><td>&nbsp;");
        listaPRNs+= F("<a href= \"javascript:window.scrollTo(0,0);\" onclick=\"document.getElementById('elname').value = this.innerText; document.getElementById('vincu').innerHTML = \'Vinculada\';\">");
        listaPRNs+= foundDevices[ilista];
        listaPRNs+= F("</a>&nbsp;</td><tr>");
      }
      listaPRNs+= "</table></p>";
    }
    else listaPRNs+= F("***Ninguno en rango***</p>");
    listaPRNs+= F("<br><input type=\"button\" value=\"Vincular\" onclick=\"location.reload();\">");
    listaPRNs+= ScrollEnd;
  }
  else listaPRNs= F("<br><input type=\"button\" value=\"Vincular\" onclick=\"location.replace('/configPrn?scan=1')\">");
  miWeb.replace("PLACEHLD", listaPRNs);
  server.send(200, "text/html", miWeb);
}

void respondeConfigDate() { // Generar pagina de configuracion de fecha y hora
  String selecc;
  String miWeb= ConfDate;
  // dia mes año
  for (j= 0; j < 3; j++) {
    selecc= "";
    for (i= 0; i < 4; i++) {
      selecc+= "<option value=\""+String(i)+"\"";
      if (myDT.dtFormats[j] == i) selecc+= " selected";
      selecc+= ">"+String(dateFormats[i])+"</option>\r\n";
    }
    miWeb.replace("PULLDR"+String(j), selecc);
  }
  // horas y minutos
  for (j= 0; j < 2; j++) {
    selecc= "";
    for (i= 0; i < numHourFormats; i++) {
      selecc+= "<option value=\""+String(i)+"\"";
      if (myDT.dtFormats[j+3] == i) selecc+= " selected";
      selecc+= ">"+String(hourFormats[i])+"</option>\r\n";
    }
    miWeb.replace("PULLTR"+String(j), selecc);
  }
  // separadores de fecha y hora
  for (j= 0; j < 2; j++) {
    selecc= "";
    for (i= 0; i < numSeparators; i++) {
      selecc+= "<option value=\""+String(i)+"\"";
      if (myDT.dtFormats[j+5] == i) selecc+= " selected";
      selecc+= ">"+String(separators[i])+"</option>\r\n";
    }
    miWeb.replace("PULLSEP"+String(j), selecc);
  }
  miWeb.replace("DIASECHECK", (myDT.dtFormats[7] == 0) ? "" :"checked");  // checkmark del dia de la semana
  miWeb.replace("NTPR",myDT.ntpServer); // servidor NTP
  selecc= "";
  for (i= 0; i < numNTPs; i++) {  // pulldown NTP
    selecc+= "<option value=\""+String(i)+"\"";
    if (String(commonNTPs[i]) == String(myDT.ntpServer)) selecc+= " selected";
    selecc+= ">"+String(commonNTPs[i])+"</option>\r\n";
  }
  miWeb.replace("PULLNTP", selecc);
  miWeb.replace("NTPTZR", myDT.timeZone); // time zone NTP
  miWeb.replace("HUSOR",String(myDT.gmtOffset/3600)); // huso horario
  miWeb.replace("VERAR",String(myDT.daylight/3600));  // horario de verano
  server.send(200, "text/html", miWeb);
}

void loadDtFromWeb(dateTimeCfg &thisDT) { // cargar parametros de configuracion de dia y hora de la pagina WEB
String ntpt, ext;

  for (i= 0; i < 7; i++) thisDT.dtFormats[i]=server.arg("pulldt"+String(i)).toInt();
  thisDT.dtFormats[7]= (server.arg("diasema") == "on") ? 1 : 0;
  ntpt= server.arg("myNtp");
  i= ntpt.lastIndexOf('.');
  j= ntpt.substring(i + 1).length();
  if ((ntpt.length()< MINNTPLENGTH) || (ntpt.indexOf("..") != -1) || (i <0) || (j < 2))
    strcpy(thisDT.ntpServer, commonNTPs[DEFAULTNTP]);
  else
    strcpy(thisDT.ntpServer, ntpt.c_str());
  strcpy(thisDT.timeZone, server.arg("myTZ").c_str());
  i= server.arg("huso").toInt();
  thisDT.gmtOffset= ((i < MINGMT) || (i > MAXGMT)) ? DEFAULTGMTOFFSET : i * 3600;
  i= server.arg("verano").toInt();
  thisDT.daylight= ((i < MINDAYLIGHT) || (i > MAXDAYLIGHT)) ? DEFAULTDAYLIGHT : i * 3600;
}

void loadPrnFromWeb(prnCfg &thisPrn) {  // cargar configuracion de impresora de la pagina WEB
  thisPrn.landscape= server.arg("prnType").toInt();
  thisPrn.WIDTHmm= server.arg("anchomm").toInt();
  thisPrn.HEIGHTmm= server.arg("altomm").toInt();
  thisPrn.extraFooter= server.arg("piemm").toInt();
  thisPrn.topMargin[0]= server.arg("margensup1").toInt();
  thisPrn.topMargin[1]= server.arg("margensup2").toInt();
  thisPrn.leftMargin= server.arg("margenizq").toInt();
  thisPrn.centered= (server.arg("centrar") == "on");
  thisPrn.chunkDelay= server.arg("chunkdelay").toInt();
  thisPrn.resetDelay= server.arg("resetdelay").toInt();
  thisPrn.fontNumber[0]= server.arg("pullf0").toInt();
  thisPrn.fontNumber[1]= server.arg("pullf1").toInt();
  thisPrn.blackBackground= (server.arg("color").toInt() == 1);
}

void respondeRoot() { // Generar pagina WEB principal
  String miWeb; // pagina web
  String selecc;
  unsigned int opcion;
  int destino;

  miWeb= PaginaWeb; // cargar de memoria flash
  lineaMain[0]= server.arg("line0");
  lineaMain[1]= server.arg("line1");
  opcion= server.arg("accion").toInt();
  i=  server.arg("selPrn").toInt();
  switch (opcion) {
    case 0: // carga inicial de pagina WEB o cambio de pulldown (seleccion de impresora)
      selectedPrn= server.arg("selPrn").toInt();
      myPrn= prnProfiles[selectedPrn];
      recalcPrinterHeaders(myPrn);
      setPrnOffline();
      updateStatus();
      break;
    case 1: // Imprimir (pagina principal)
      myPrn.blackBackground= (server.arg("color").toInt() == 1);
      myPrn.centered= (server.arg("centrar") == "on");
      myPrn.fontNumber[0]= server.arg("pullf0").toInt();;
      myPrn.fontNumber[1]= server.arg("pullf1").toInt();
      recalcPrinterHeaders(myPrn);
      if (lineaMain[0].length() == 0) { // si no hay texto intentar imprimir la hora
        if (timeIsValid)
          formatDT(myDT, myPrn);
        else myStatus= STATINVALTIME;
      }
      else
        buildBitmap(myPrn, lineaMain);  // hay texto
      if (timeIsValid || (lineaMain[0].length() != 0)) {
        if ((!prnIsOnline) || (!pClient->isConnected())) prnIsOnline= connect2prn();
        if (prnIsOnline) {
          blinkStateG= LEDON;
          digitalWrite(GPIOgreen, blinkStateG);
          myStatus= STATPRINTING;
// server.handleClient permitiria actualizar el estado
//          server.handleClient();
// pero puede crear problemas con codigo reentrante y habria que poner restricciones en todas las paginas WEB
// la actualizacion de myStatus se hace por si en el futuro se convierte la impresion en asincronica
          printBuffer();
          myStatus= STATONLINE;
        }
        else myStatus= STATOFFLINE;
      }
      blinkStateG= LEDOFF;
      digitalWrite(GPIOgreen, blinkStateG);
      break;
    case 2: // Guardar configuracion impresora (de pagina /ConfigPrn)
      selecc= server.arg("myProfile");  // Nombre del perfil
      selecc.trim();
      destino= -1;
      for (i= 0; i < numProfiles; i++) {  // buscar el perfil por nombre en los perfiles guardados
        if (selecc == String(prnProfiles[i].prnLabel)) {
          destino= i;
          break;
        }
      }
      if (destino < 0) {  // no está,
        if (numProfiles < MAXPROFILES) {  // si hay lugar, crear un perfil nuevo
          destino= numProfiles;
          numProfiles++;
        }
        else
          destino= selectedPrn;  // si no hay lugar sobreescribir el perfil seleccionado
      }
      if (destino != selectedPrn) prnProfiles[destino]= prnProfiles[selectedPrn]; // si el destino no es el perfil seleccionado, reemplazarlo
      if (destino != 0) { // seleccionar este perfil
        myPrn= prnProfiles[destino];
        prnProfiles[destino]= prnProfiles[0]; // y mover perfil[0] al destino
      }
      if (selecc.length() != 0) strcpy(myPrn.prnLabel, selecc.c_str());  // si es invalido se toma el que estaba seleccionado
      selecc= server.arg("myBleName");  // nombre de la impresora
      selecc.trim();
      if (selecc.length() >= minBleNameLength) { // si el nombre BLE de la impresora tiene al menos 4 caracteres (TP-x), procesarlo
        if (numDevices == 0) {  // el usuario hizo click en el boton Vincular y se encontraron dispositivos BLE?
          if (selecc != String(myPrn.prnBtName)) {
            strcpy(myPrn.prnBtName, selecc.c_str());  // no, guardar este nombre
            clearMac(myPrn.address);  // y limpiar la MAC
          }
//        else -> actualizando perfil existente
        }
        else {  // tenemos lista de dispositivos
          j= -1;
          for (i= 0; i < numDevices; i++) { // buscar nombre exacto
            if (selecc == foundDevices[i]) {
              j= i;
              break;
            }
          }
          if (j == -1) {  // no se encontro el nombre exacto, buscar nombre que empiece asi (en mayusculas)
            selecc.toUpperCase();
            for (i= 0; i < numDevices; i++) {
              foundDevices[i].toUpperCase();
              if (foundDevices[i].startsWith(selecc)) {
                j= i;
                break;
              }
            }
          }
          if (j != -1) {  // se encontró esta impresora en la lista BLE (y está la MAC en la lista de MACs)
            strcpy(myPrn.prnBtName, foundDevices[j].c_str()); // guardar
            foundMacs[j].reverseByteOrder();  // Nimble guarda la MAC en little Endian
            if (memcmp(myPrn.address, foundMacs[j].getVal(), 6) != 0) {  // la MAC es distinta a la de la impresora actual?
              setPrnOffline();
              myStatus= STATDISCON;
              memcpy(myPrn.address, foundMacs[j].getVal(), 6);
            }
            foundMacs[j].reverseByteOrder();  // dejar la MAC Nimble como estaba
          }
// no hay else porque se deja la MAC original ya que se puede modificar un perfil con la impresora apagada
        }
      }
      else {  // si el nombre de la impresora tiene menos de 4 caracteres se considera nulo y borramos la impresora del perfil
        setPrnOffline();
        clearMac(myPrn.address);
        myPrn.prnBtName[0]= 0;
        myStatus= STATNOPAIR;
      }
      loadPrnFromWeb(myPrn);  // cargar los datos de la pagina
      prnProfiles[0]= myPrn;  // guardar el nuevo perfil como perfil por defecto
      recalcPrinterHeaders(myPrn);  // actualizar
      saveProfiles();
      selectedPrn= 0;
      updateStatus();
      break;
    case 3: // eliminar perfil de impresora (de pagina /ConfigPrn)
      if (numProfiles == 1) // si hay 1 solo perfil cargar los perfiles por defecto
        loadDefaultProfiles();
      else {
        if (selectedPrn != (numProfiles - 1)) { // si el perfil a borrar no es el ultimo
          for (i= selectedPrn + 1; i < numProfiles; i++) prnProfiles[i - 1]= prnProfiles[i]; // mover los perfiles restantes
        }
        numProfiles--;
        prnProfiles[numProfiles].prnLabel[0]= 0;  // marcar el ultimo como borrado (el array comienza en 0)
      }
      selectedPrn= 0;
      myPrn= prnProfiles[0];
      recalcPrinterHeaders(myPrn);
      saveProfiles();
      updateStatus();
      break;
    case 4: // cancelar configuracion impresora  (de pagina /ConfigPrn)
      setPrnOffline();
      myPrn= prnProfiles[selectedPrn];
      recalcPrinterHeaders(myPrn);
      updateStatus();
      break;
    case 5: // Guardar configuracion fecha y hora (de pagina /configDate)
      loadDtFromWeb(myDT);
      saveDateTime();
      break;
    case 6: // Cancelar configuracion fecha y hora  (de pagina /configDate), se podria obviar el CASE dado que no hay "default:"
      break;
  }
// generar la pagina WEB
// pulldown perfiles
  selecc= "";
  for (i= 0; i < numProfiles; i++) {
    selecc+= "<option value=\""+String(i)+"\"";
    if (selectedPrn == i) selecc+= " selected";
    selecc+= ">"+String(prnProfiles[i].prnLabel)+"</option>\r\n";
  }
  miWeb.replace("PULLOPTIONS", selecc);
  // pulldowns fuentes
  for (j= 0; j < 2; j++) {
    selecc= "";
    for (i= 0; i < numFonts; i++) {
      selecc+= "<option value=\""+String(i)+"\"";
      if (myPrn.fontNumber[j] == i) selecc+= " selected";
      selecc+= ">"+String(fontNames[i])+"</option>\r\n";        
    }
    miWeb.replace("PULLFR"+String(j), selecc);
    miWeb.replace("MYLINE"+String(j), lineaMain[j]);
  }
  miWeb.replace("CENTERCHECK", prnProfiles[selectedPrn].centered ? "checked" : ""); // si va centrado
  miWeb.replace("CHECKWHITE", prnProfiles[selectedPrn].blackBackground ? "" : " checked=\"checked\"");  // actualizar radio del color
  miWeb.replace("CHECKBLACK", prnProfiles[selectedPrn].blackBackground ? " checked=\"checked\"" : "");
  miWeb.replace("PRINTENA", macIsZeros(myPrn.address) ? " disabled" : "");  // deshabilitar el boton de impresion si la impresora no esta vinculada
  if (macIsZeros(myPrn.address)) {
    miWeb.replace("Desconectada", statusNames[0]);  // forzar estado "No Vinculada" (mucho cuidado si cambian este texto en la pagina WEB)
    myStatus= STATNOPAIR;
  }
  server.send(200, "text/html", miWeb); // enviar pagina web inicial al cliente
}

void actualizaEstado() {
  server.send(200, "text/plane", statusNames[myStatus]); // actualizar estado
}

void responde404() {  // responder accesos a paginas no definidas
  server.send(404, "text/html", F("<p style=\"font-size:30px\">404: P&aacute;gina Inexistente"));
}

void respondeAbout() { // responder "acerca de"
  String miWeb= About;
  miWeb.replace("VERNUM", FWVERSION);
  server.send(200, "text/html", miWeb);
}

void enviaFavi32(){  // enviar favicon 32x32 de Windows al cliente
  fileName= ICON32;
  dataFile = LittleFS.open(fileName.c_str(), "r");
  server.streamFile(dataFile, "image/png");
  dataFile.close();
}

void enviaFavi192(){  // enviar favicon 192x192 de Android al cliente
  fileName= ICON192;
  dataFile = LittleFS.open(fileName.c_str(), "r");
  server.streamFile(dataFile, "image/png");
  dataFile.close();
}

void enviaLogo() { // enviar mi logo al cliente
  fileName= MYLOGO;
  dataFile= LittleFS.open(fileName.c_str(), "r");
  server.streamFile(dataFile, "image/webp");
  dataFile.close();
}

void enviaPreview() {  // enviar previsualizacion
  uint8_t* ptrFrom;
  uint8_t* ptrTo;

  if (!tmpPrn.blackBackground) {  // si el fondo no es negro dibujar recuadro
    drawLine(0, 0, myPrnCalc.bitmapWidth-1, 0, BLACK);  // linea horizontal
    drawLine(0, myPrnCalc.bitmapHeight-1, myPrnCalc.bitmapWidth-1, myPrnCalc.bitmapHeight-1, BLACK);  // linea horizontal inferior
    drawLine(0, 0, 0, myPrnCalc.bitmapHeight-1, BLACK); // linea vertical izquierda
    drawLine(myPrnCalc.bitmapWidth-1, 0, myPrnCalc.bitmapWidth-1, myPrnCalc.bitmapHeight-1, BLACK); // linea vertical derecha
  }
  // ajustar lineas del bitmap a 32 bits (dentro del mismo buffer)
  if ((tmpPrn.WIDTHmm % 4) != 0) {
    ptrFrom= &myBitmap[0];
    ptrTo= ptrFrom+myPrnCalc.bitmapHeight*bmpWidth4;  // puntero destino ultima linea bitmap ajustado a 32 bits
    ptrFrom+= myPrnCalc.bitmapHeight*tmpPrn.WIDTHmm;  // puntero fuente ultima linea buffer
    for (i= 0; i < myPrnCalc.bitmapHeight; i++) { // repetir para todas las lineas
      ptrFrom-= tmpPrn.WIDTHmm;
      ptrTo-= bmpWidth4;
      memmove(ptrTo, ptrFrom, tmpPrn.WIDTHmm);  // no se puede usar memcpy() porque fuente y destino se superponen al principio del buffer
    }
  }
  // enviar bitmap al server
  server.setContentLength(myBmpHeader.biSizeImage + BMPHEADERSIZE); // definir tamaño del contenido
  server.send(200, "image/bmp", "");  // definir contenido
  server.sendContent((const char*)&myBmpHeader, BMPHEADERSIZE);  // enviar encabezado del BMP
  server.sendContent((const char*)&myBitmap, myBmpHeader.biSizeImage);  // enviar contenido del BMP ajustado a 32 bits por linea
}

void iniciaPagina() { // iniciar pagina WEB
  server.on("/", respondeRoot);   // manejar pagina principal
  server.on("/updEstado", actualizaEstado);  // actualizar cambios de estado en otro cliente
  server.on("/configWifi", respondeConfigWifi);
  server.on("/save_Wifi_config" , HTTP_POST, respondeGuardaWifi);
  server.on("/configPrn", respondeConfigPrn);
  server.on("/configDate", respondeConfigDate);
  server.on("/preview", respondePreview);
  server.on("/bitmap.bmp", enviaPreview);
  server.on("/favicon32.png", enviaFavi32);
  server.on("/favicon192.png", enviaFavi192);
  server.on("/logo.webp", enviaLogo);
  server.on("/about", respondeAbout);
  server.onNotFound(responde404);
  server.begin();
}

void nuevoModoWiFi(int modoWiFi) { // cambiar el modo de WiFi (AP, router, apagado)
  // primero desconectar
  timeIsValid= false;
  blinkerYellow.detach();
  digitalWrite(GPIOyellow, LEDOFF);
  if ((wifiState == WIFION) || (wifiState == WIFICON)) {
    updateBlueLED(LEDWIFIOFF);
    WiFi.disconnect();
    delay(2000);
  }
  if (wifiState == WIFIAPM) {
    updateBlueLED(LEDWIFIOFF);
    WiFi.softAPdisconnect();
    delay(2000);
  }
  // ahora establecer nueva conexion
  switch (modoWiFi) {
    case WIFION:
      if (ssid.length() == 0) {   // si router es invalido pasar a modo AP
        updateBlueLED(LEDFLASHON);
        WiFi.softAPConfig(staticIPAP, myGatewayAP, subnetAP);
        WiFi.softAP(mySSID, passwordAP, MYCHANNEL, false, MAXCONNCT);
        iniciaPagina();
        wifiState= WIFIAPM;
      }
      else {
        WiFi.hostname(mySSID);  // conectar a Router
        updateBlueLED(LEDBLINKON);
        WiFi.mode(WIFI_STA);
        WiFi.setHostname("ChronoLabeler");
        WiFi.begin(ssid.c_str(),password.c_str());
        if ((staticIP[0] != 0) && (staticIP[3] != 0) && (myGateway[0] != 0) && (myGateway[3] != 0)) WiFi.config(staticIP, myGateway, subnet, myGateway); // usar el gateway como DNS1
        wifiState= WIFION;
        retryWifi= milliLoop;
      }
      break;
    case WIFIAPM:  // modo AP
      updateBlueLED(LEDFLASHON);
      WiFi.softAPConfig(staticIPAP, myGatewayAP, subnetAP);
      WiFi.softAP(mySSID, passwordAP, MYCHANNEL, false, MAXCONNCT);
      iniciaPagina();
      wifiState= WIFIAPM;
      break;
    default:
      WiFi.mode(WIFI_OFF);
      wifiState= WIFIOFF;
      updateBlueLED(LEDWIFIOFF);
      break;
  }
}

void rotate90cw(uint8_t* source, int width, int height) {  // rotar bitmap 90° para impresoras apaisadas
  unsigned int in_bytes,out_bytes;
  unsigned int x,y,destX;
  uint8_t rotatedBitmap[myPrnCalc.myBufferSize];

  in_bytes = width >> 3;
  out_bytes = height >> 3;
  memset(rotatedBitmap, 0, myPrnCalc.myBufferSize);  // borrar buffer
  //in_bytes * height
  for (y = 0; y < height; y++) {
    destX = height - y - 1;
    for (x = 0; x < width; x++) {
      if (source[y * in_bytes + (x >> 3)] & (1 << (7 - (x % 8)))) // pixel encendido?
        rotatedBitmap[x * out_bytes + (destX >> 3)] |= (1 << (7 - (destX % 8))); // encender el de destino
    }
  }
  memcpy(source, rotatedBitmap, myPrnCalc.myBufferSize);  // mover el bitmap rotado al buffer original (se libera la RAM utilizada por esta funcion)
}

String mac2str(uint8_t addr[6]) {  // convertir MAC address en string
String tmp;
  tmp="";
  for (i= 0; i<6; i++) {
    tmp+= String(addr[i],HEX);
    if (i<5) tmp+= ":";
  }
  return tmp;
}

bool macIsZeros(uint8_t* thisAddress) {  // chequear si la MAC es ceros
  for (j= 0; j < 6; j++) if (thisAddress[j] != 0) return false;
  return true;
}

void clearMac(uint8_t addr[6]) {  // borra la MAC
  memset(addr, 0, 6);
}

bool connect2prn() {  // conectar a impresora
  if (macIsZeros(myPrn.address)) {  // esta vinculada?
    myStatus= STATNOPAIR;
    return false;
  }
  blinkStateG= LEDON;
  digitalWrite(GPIOgreen, blinkStateG);
  blinkerGreen.attach_ms(BLINKINGTIME , blinkeaG);  // LED verde titilando mientras conectamos
  if (pClient != nullptr) { // reconectar
//  reset de la conexion
    if (!pClient->connect()) {
      blinkerGreen.detach();
      return false;
    }
    delay(myPrn.resetDelay);  // la TP-110 se resetea mas rapido que la TP-220
  }
  else {  // crear conexion nueva
    pClient= NimBLEDevice::createClient();
    if (pClient == nullptr) {
      blinkerGreen.detach();
      return false;
    }
    pClient->setConnectTimeout(BLETIMEOUT);
    NimBLEAddress printerAddress((uint8_t *)myPrn.address, BLE_ADDR_PUBLIC); // MAC de mi impresora BLE
    if (!pClient->connect(printerAddress)) {
      blinkerGreen.detach();
      return false;
    }
  }
  // obtener servicio
  pService = pClient->getService(serviceUUID);
  if (pService == nullptr) {
    blinkerGreen.detach();
    return false;
  }
  // obtener characteristic
  pCharacteristic = pService->getCharacteristic(charUUID);
  blinkerGreen.detach();
  return (pCharacteristic != nullptr);  // devuelve true si el puntero no es null
}

String toStr2(unsigned int value) { // formato de 2 caracteres para fechas y horas
char fmtBuf[5];
  sprintf(fmtBuf, "%02d", value);
  return String(fmtBuf);
}

void buildBitmap(prnCfg &thisPrn, String line[2]) { // generacion del bitmap a imprimir
  unsigned int myLeftMargin[2];
  unsigned int myTopMargin;
  int width, top, bottom;
  bool cropped;

  recalcPrinterHeaders(thisPrn);
  memset(myBitmap, thisPrn.blackBackground ? BLACK : WHITE, myPrnCalc.myBufferSize);
  for (i= 0; i < 2; i++) {
    cropped= false;
    do {  // acortar el string hasta que entre en el ancho de la impresora
      getStringBox(fontPtr[thisPrn.fontNumber[i]], (char *)line[i].c_str(), &width, &top, &bottom);
      if (width > myPrnCalc.bitmapWidth) {
        cropped= true;
        line[i].remove(line[i].length() - 1);
      }
    } while (width > myPrnCalc.bitmapWidth);
    myLeftMargin[i]= (thisPrn.centered)? (myPrnCalc.bitmapWidth - width) >> 1 : thisPrn.leftMargin; // si va centrado calcular centro, si no utilizar margen
    if (cropped || ((myPrnCalc.bitmapWidth - width) <= fontPitch)) myLeftMargin[i]= 0;  // si tuvimos que recortar el texto cancelar el margen izquierdo
    myTopMargin= thisPrn.topMargin[i]+fontHeight[thisPrn.fontNumber[i]];  // el margen mas la altura de la fuente seleccionada
    // promediar el margen superior si una de las lineas esta en blanco
    if ((line[0].length() == 0) || (line[1].length() == 0)) myTopMargin= (thisPrn.topMargin[0]+fontHeight[thisPrn.fontNumber[0]]+thisPrn.topMargin[1]+fontHeight[thisPrn.fontNumber[1]]) >> 1;
    if (line[i].length() != 0) drawCustomText(fontPtr[thisPrn.fontNumber[i]], myLeftMargin[i], myTopMargin, (char *)line[i].c_str());
  }
}

void formatDT(dateTimeCfg &thisDT, prnCfg &thisPrn) { // crear bitmap de los textos a imprimir
String fh[3];

  if (timeIsValid) {
    getLocalTime(&timeinfo);
    fh[0]= toStr2(timeinfo.tm_mday);
    fh[1]= toStr2(timeinfo.tm_mon+1);
    fh[2]= toStr2(timeinfo.tm_year-100);
    // formatear dia/mes/año
    myDate[0]="";
    for (i= 0; i < 3; i++) {  // DD/MM/YY - MM-DD-YY, etc
      if (i > 0) {  // agregar separador
        if ((myDate[0].length() !=0) && (thisDT.dtFormats[i] != 3)) {
          switch (thisDT.dtFormats[5]) {  // separadores de fecha
            case 5:
              myDate[0]+= " ";  // espacio
              break;
            case 6: // letra
  //            myDate[0]+= letterFormats[thisDT.dtFormats[i-1]]; // caso especial, se maneja mas abajo
              break;
            case 7: // "--"
              break;
            default:
              myDate[0]+= separators[thisDT.dtFormats[5]];
              break; 
          }
        }
      }
      if (thisDT.dtFormats[i] != 3) {
        myDate[0]+= fh[thisDT.dtFormats[i]];  // dtFormats[3] -> "--""
        if (thisDT.dtFormats[5] == 6) myDate[0]+= letterFormats[thisDT.dtFormats[i]]; // D/M/Y
      }
    }
    fh[0]= toStr2((timeinfo.tm_hour <= 12) ? timeinfo.tm_hour :  timeinfo.tm_hour - 12);
    fh[1]= toStr2(timeinfo.tm_hour);
    fh[2]= toStr2(timeinfo.tm_min);
    // formatear fecha y hora
    myDate[1]= "";
    if (thisDT.dtFormats[7] != 0) { // dia de la semana
      myDate[1]+= String(diaSem[timeinfo.tm_wday]);
      if (((thisDT.dtFormats[3] != 0) && (thisDT.dtFormats[4] != 0)) || (thisDT.dtFormats[3] == 3) || (thisDT.dtFormats[4] == 3)) myDate[1]+= " "; // si el formato de hora no es AM/PM agregar espacio
    }
    for (i= 3; i <= 4; i++) {  // HH:MM - HH:MM am/pm, etc
      if (thisDT.dtFormats[i] != 3) {
        myDate[1]+= fh[thisDT.dtFormats[i]];
        if (thisDT.dtFormats[6] == 6) // separador = letra?
          myDate[1]+= (thisDT.dtFormats[i] == 2) ? letterFormats[4] : letterFormats[3];  // m o h
        else if ((i == 3) && (thisDT.dtFormats[4] != 3)) {  // separador no va despues de los minutos si no es letra o no hay minutos
          switch (thisDT.dtFormats[6]) { // separadores de hora
            case 5:
              myDate[1]+= " ";
              break;
            case 7: // "--"
              break;
            default:
              myDate[1]+= separators[thisDT.dtFormats[6]];
              break; 
          }
        }
      }
    }
    if ((thisDT.dtFormats[3] == 0) || (thisDT.dtFormats[4] == 0)) {
      if ((thisDT.dtFormats[7] == 0) || (thisDT.dtFormats[3] == 3) || (thisDT.dtFormats[4] == 3)) myDate[1]+= " ";
      myDate[1]+= (timeinfo.tm_hour <= 12) ? "AM" : "PM";
    }
  }
  else {
    myDate[0]= F("Hora");
    myDate[1]= F("no disponible");
  }
  buildBitmap(thisPrn, myDate);
}

void printBuffer() {  // imprimir el buffer
  if (myPrn.landscape) rotate90cw(myBitmap, myPrnCalc.bitmapWidth, myPrnCalc.bitmapHeight); // si es apaisada rotar 90 grados
  pCharacteristic->writeValue(myPrnCalc.sPrnInit.c_str(), myPrnCalc.sPrnInit.length()); // inicializar impresora
  delay(INITDELAY);
  pCharacteristic->writeValue(myPrnCalc.prnHeader.c_str(), myPrnCalc.prnHeader.length()); // enviar encabezado TSPL a impresora (SIZE 50mm, etc)
  pCharacteristic->writeValue(myPrnCalc.prnBitHeader.c_str(), myPrnCalc.prnBitHeader.length());
  ptrStart= &myBitmap[0] + myPrnCalc.myBufferSize;
  fullBufferSize= myPrn.extraFooter * myPrnCalc.prnBitmapWidth;  // tamaño de pie de pagina
  memset(ptrStart, myPrn.blackBackground ? BLACK : WHITE, fullBufferSize);  // borrar pie de pagina en el buffer
  fullBufferSize+= myPrnCalc.myBufferSize; // agregar el total del bitmap a la longitud
  ptrStart= &myBitmap[0]; // preparar acceso al bitmap
  ptrSize= chunkSize;
  chunks= fullBufferSize/ptrSize; // dividir bitmap en paquetes de no mas de chunkSize (240 bytes)
  bfrPos= 0;
  for (i= 0; i<chunks;i++) {  // enviar paquetes
    pCharacteristic->writeValue((uint8_t*)ptrStart, ptrSize);
    delay(myPrn.chunkDelay);  // esperar a que la impresora guarde el paquete BLE
    ptrStart+=chunkSize;
    bfrPos+= chunkSize;
  }
  ptrSize= fullBufferSize % chunkSize;
  if (ptrSize != 0) { // si el tamaño del bitmap no es multiplo de chunkSize enviar el resto
    pCharacteristic->writeValue((uint8_t*)ptrStart, ptrSize);
    delay(myPrn.chunkDelay);
  }
  pCharacteristic->writeValue(myPrnCalc.prnFooter.c_str(), myPrnCalc.prnFooter.length()); // enviar comando TSPL imprimir "PRINT 1,1" etc
}

void loadDefaultDateTime(dateTimeCfg &thisDT) {
  memcpy(thisDT.dtFormats, defaultFormats, sizeof(thisDT.dtFormats));
  thisDT.gmtOffset= DEFAULTGMTOFFSET; // UTC -3hs en segundos correspondiente a Argentina (-3*3600)
  thisDT.daylight= DEFAULTDAYLIGHT;  // horario de verano, para agregar 1 hora cambiar a 3600
  strcpy(thisDT.ntpServer, commonNTPs[DEFAULTNTP]);
  strcpy(thisDT.timeZone, MYTIMEZONE); // time zone de Buenos Aires, Argentina (puede ser "ART3" tambien)
}

// configuraciones de impresoras por defecto
void loadDefaultPrnParams(int whichPrn) {
  /*
  borrar totalmente los strings (no hace falta porque nunca se comparan las struct)
  memset(myPrn.prnLabel, 0, sizeof(myPrn.prnLabel));
  memset(myPrn.prnBtName, 0, sizeof(myPrn.prnBtName));
  memset(myPrn.prnInit, 0, sizeof(myPrn.prnInit));
  memset(myPrn.prnGap, 0, sizeof(myPrn.prnGap));
  */
  myPrn.fontNumber[0]= 0;
  myPrn.fontNumber[1]= 0;
  myPrn.blackBackground= false;
  myPrn.centered= true;
  clearMac(myPrn.address);  // por defecto esta impresora no esta vinculada
  switch (whichPrn) {
    case 0:
      strcpy(myPrn.prnLabel, "TP-110 (15x30mm)");
      strcpy(myPrn.prnBtName, "TP-110");
      myPrn.WIDTHmm= 30;    // tamaño maximo del sticker en mm
      myPrn.HEIGHTmm= 12;   // altura del sticker en mm
      myPrn.extraFooter= 0; // pie de pagina
      myPrn.chunkDelay= 20; // retardo entre paquetes BLE
      myPrn.resetDelay= 500;  // retardo de inicializacion
      myPrn.topMargin[0]= 14; // margen superior linea 1 (no incluye altura de la fuente)
      myPrn.topMargin[1]= 50; // margen superior linea 2
      myPrn.leftMargin= 16; // margen izquierdo
      strcpy(myPrn.prnInit, "\x1b!S");  // escape de reset inicial
      strcpy(myPrn.prnGap, "2 mm,0 mm");  // separacion de etiquetas en mm (la TP-220 no acepta mm)
      myPrn.landscape= true;  // tipo de impresoara apaisada
      break;
    case 1:
      strcpy(myPrn.prnLabel, "TP-110 (15x50mm)");
      strcpy(myPrn.prnBtName, "TP-110");
      myPrn.WIDTHmm= 48;
      myPrn.HEIGHTmm= 12;
      myPrn.extraFooter= 48;
      myPrn.chunkDelay= 20;
      myPrn.resetDelay= 500;
      myPrn.topMargin[0]= 14;
      myPrn.topMargin[1]= 50;
      myPrn.leftMargin= 16;
      strcpy(myPrn.prnInit, "\x1b!S");
      strcpy(myPrn.prnGap, "2 mm,0 mm");
      myPrn.landscape= true;
      break;
    case 2:
      strcpy(myPrn.prnLabel, "TP-220 (50x25mm)");
      strcpy(myPrn.prnBtName, "TP-220");
      myPrn.WIDTHmm= 48;
      myPrn.HEIGHTmm= 25;
      myPrn.extraFooter= 0;
      myPrn.chunkDelay= 40;
      myPrn.resetDelay= 3500;
      myPrn.topMargin[0]= 15;
      myPrn.topMargin[1]= 55;
      myPrn.leftMargin= 16;
      strcpy(myPrn.prnInit, "\x1b!S");
      strcpy(myPrn.prnGap, "0.10,0");
      myPrn.landscape= false;
      break;
    default:
      myPrn.prnLabel[0]= 0; // invalidar perfil (esto marca el fin de los perfiles)
      break;
  }
}

// calculo de parametros que no se guardan
void recalcPrinterHeaders(prnCfg &thisPrn) {
  myPrnCalc.bitmapWidth= thisPrn.WIDTHmm << 3;
  myPrnCalc.bitmapHeight= thisPrn.HEIGHTmm << 3;
  myPrnCalc.myBufferSize= thisPrn.WIDTHmm * myPrnCalc.bitmapHeight;
  if (thisPrn.landscape) {
    myPrnCalc.prnBitmapWidth= thisPrn.HEIGHTmm;
    myPrnCalc.prnBitmapHeight= myPrnCalc.bitmapWidth + thisPrn.extraFooter;
    myPrnCalc.prnHeader= "SIZE "+String(thisPrn.HEIGHTmm)+" mm,"+String(thisPrn.WIDTHmm);
  }
  else {
    myPrnCalc.prnBitmapWidth= thisPrn.WIDTHmm;
    myPrnCalc.prnBitmapHeight= myPrnCalc.bitmapHeight + thisPrn.extraFooter;
    myPrnCalc.prnHeader= "SIZE "+String(thisPrn.WIDTHmm)+" mm,"+String(thisPrn.HEIGHTmm);
  }
  fontPitch= (myPrnCalc.bitmapWidth + 7) >> 3;
  myPrnCalc.prnHeader+= " mm\r\nDENSITY 8\r\nSPEED 4.0\r\nGAP "+String(thisPrn.prnGap)+"\r\nCLS\r\n";
  myPrnCalc.prnBitHeader= "BITMAP 0,0,"+String(myPrnCalc.prnBitmapWidth)+","+String(myPrnCalc.prnBitmapHeight)+",0,";
  myPrnCalc.sPrnInit= String(thisPrn.prnInit);
  myPrnCalc.prnFooter= "\r\nPRINT 1,1\r\n";
// actualizar encabezados BMP
  bmpWidth4= ((thisPrn.WIDTHmm + 3) >> 2) << 2; // convertir a multiplo de 4
  myBmpHeader.bfType= 0x4D42; // 'BM'
  myBmpHeader.biSizeImage= bmpWidth4 * myPrnCalc.bitmapHeight;
  myBmpHeader.bfSize= myBmpHeader.biSizeImage + BMPHEADERSIZE;  // tamaño total del "archivo" BMP
  myBmpHeader.bfReserved1= 0;
  myBmpHeader.bfReserved2= 0;
  myBmpHeader.bfOffBits= BMPHEADERSIZE; // 54 + 2*4 byte tabla de colores + 2 bytes (alineado a 32 bits)
  myBmpHeader.biSize= 40;  // tamaño del header (40 bytes)
  myBmpHeader.biWidth= myPrnCalc.bitmapWidth;
  myBmpHeader.biHeight= -myPrnCalc.bitmapHeight; // Positivo -> de abajo hacia arriba
  myBmpHeader.biPlanes= 1;
  myBmpHeader.biBitCount= 1; // 1 bit por pixel
  myBmpHeader.biCompression= 0;
  myBmpHeader.biXPelsPerMeter= 0; // resolucion horizontal
  myBmpHeader.biYPelsPerMeter= 0; // resolucion vertical
  myBmpHeader.biClrUsed= 0;       // numero de colores en la paleta (0 para 1bpp)
  myBmpHeader.biClrImportant= 0;
  myBmpHeader.colorBlack= 0;  // valor del pixel color negro
  myBmpHeader.colorWhite= 0x0FFFFFF;  // color blanco
}

void loadDefaultProfiles() {   // inicializar los perfiles por defecto
  numProfiles= 0;
  for (i= 0; i < MAXPROFILES; i++) {
    loadDefaultPrnParams(i);
    prnProfiles[i]= myPrn;
    if (myPrn.prnLabel[0] == 0) break;
    numProfiles++;
  }
  selectedPrn= 0;
  myPrn= prnProfiles[0];
  recalcPrinterHeaders(myPrn);
}

void saveProfiles() { // guardar perfiles en LittleFS
  fileName= FILEPRNCFG;
  dataFile = LittleFS.open(fileName.c_str(), "w");
  dataFile.write((byte *)&prnProfiles, sizeof(prnProfiles));
  dataFile.close();
}

void saveDateTime() { // guardar configuracion de fecha y hora
  fileName= FILEDATETIME;
  dataFile = LittleFS.open(fileName.c_str(), "w");
  dataFile.write((byte *)&myDT, sizeof(myDT));
  dataFile.close();
}

void setup() {
  pinMode(GPIObtn, INPUT_PULLUP); // el pullup no es necesario porque el hardware tiene un pullup de 10K, pero se activa por si corre el software sin hardware
  pinMode(GPIOblue, OUTPUT);
  pinMode(GPIOyellow, OUTPUT);
  pinMode(GPIOgreen, OUTPUT);
  digitalWrite(GPIOblue, LEDOFF);
  digitalWrite(GPIOyellow, LEDOFF);
  digitalWrite(GPIOgreen, LEDOFF);
  /* debug &&
  Serial.begin(115200);
  while (!Serial) yield();
  delay(500);
  Serial.println();
  Serial.println();
  Serial.println("Ready");
  end debug
  */
  oldInp= BTNRELEASED;  // ultimo estado del pin boton no presionado
  boton= BTNRELEASED;   // estado del boton (con debounce)
  btnHeld= false;       // boton no ha sido sostenido
  wifiState= WIFIOFF;   // modo de WiFi por defecto
  WiFi.persistent(false);  // la configuracion la guardamos nosotros, persistent gasta la memoria flash
  if (!LittleFS.begin()) {  // iniciar filesystem
    LittleFS.format();
    LittleFS.begin();
  }
  // cargar perfiles
  prnProfiles[0].prnLabel[0]= 0;
  fileName= FILEPRNCFG;
  bytesRead= 0;
  if (LittleFS.exists(fileName)) {
    dataFile = LittleFS.open(fileName.c_str(), "r");
    if (dataFile) {
      bytesRead= dataFile.read((byte *)&prnProfiles, sizeof(prnProfiles));
      dataFile.close();
    }
  }
  if (bytesRead != sizeof(prnProfiles)) prnProfiles[0].prnLabel[0]= 0;
  // debug para limpiar perfiles a por defecto
  // prnProfiles[0].prnLabel[0]= 0;
  // fin debug
  if (prnProfiles[0].prnLabel[0] == 0) {  // si no hay perfiles utilizar predeterminados
    loadDefaultProfiles();
    saveProfiles();
  }
  else countProfiles();
  // cargar configuracion de fecha y hora
  fileName= FILEDATETIME;
  bytesRead= 0;
  if (LittleFS.exists(fileName)) {
    dataFile = LittleFS.open(fileName.c_str(), "r");
    if (dataFile) {
      bytesRead= dataFile.read((byte *)&myDT, sizeof(myDT));
      dataFile.close();
    }
  }
  if (bytesRead != sizeof(myDT)) {  // si no hay configuracion de fecha y hora cargar por defecto
    loadDefaultDateTime(myDT);
    saveDateTime();
  }
  fileName= MYLOGO;
  if (!LittleFS.exists(fileName)) {
    dataFile = LittleFS.open(fileName.c_str(), "w");
    dataFile.write((byte *)myPhoto, sizeof(myPhoto));
    dataFile.close();
  }
  fileName= ICON32;
  if (!LittleFS.exists(fileName)) {
    dataFile = LittleFS.open(fileName.c_str(), "w");
    dataFile.write((byte *)Favi32, sizeof(Favi32));
    dataFile.close();
  }
  fileName= ICON192;
  if (!LittleFS.exists(fileName)) {
    dataFile = LittleFS.open(fileName.c_str(), "w");
    dataFile.write((byte *)Favi192, sizeof(Favi192));
    dataFile.close();
  }
// lectura de Configuracion WiFi
  bytesRead= 0;
  savedCfg.staticIP[0]= 0;
  fileName= FILEWIFI;
  if (LittleFS.exists(fileName)) {
    dataFile = LittleFS.open(fileName.c_str(), "r");
    if (dataFile) {
      bytesRead= dataFile.read((byte *)&savedCfg, sizeof(savedCfg));
      dataFile.close();
    }
  }
  if (bytesRead != sizeof(savedCfg)) {
    staticIP= IPAddress(0,0,0,0);
    myGateway= staticIP;
    subnet= IPAddress(255,255,255,0);
    savedCfg.initialWifi= WIFIAPM;
    savedCfg.retryWifi= DEFAULTTIMEOUT;
    ssid="";
    password="";
  }
  else {
    for (i=0; i < 4; i++) {
      staticIP[i]= savedCfg.staticIP[i];
      myGateway[i]= savedCfg.myGateway[i];
      subnet[i]= savedCfg.subnet[i];
    }
    if ((staticIP[0] != 0) && ((myGateway[0] == 0) || (subnet[0] == 0))) {
      myGateway= staticIP;
      myGateway[3]= 1;
      subnet= IPAddress(255,255,255,0);
    }
    // lectura de SSID y password
    fileName= FILESSID;
    if (LittleFS.exists(fileName)) {
      dataFile = LittleFS.open(fileName.c_str(), "r");
      if (dataFile) {
        ssid= dataFile.readStringUntil('\n');
        if (ssid.length() > 0) ssid.remove(ssid.length()-1);
        password= dataFile.readStringUntil('\n');
        if (password.length() > 0) password.remove(password.length()-1);
        dataFile.close();
      }
      else {
        ssid="";
        password="";
      }
    }
    else {
      ssid="";
      password="";
    }
  }
  wifiState= WIFIOFF;
  mDNS= false;
  nuevoModoWiFi(savedCfg.initialWifi);
  timeIsValid= false;
  yelIsBlinking= false;
  pBackBuffer= (uint8_t *)myBitmap;  // puntero para rutina de renderizado de fonts
  prnIsOnline= false;
  selectedPrn= 0;
  myPrn= prnProfiles[0];
  recalcPrinterHeaders(myPrn);
  fontPtr[0]= (GFXfont *)&FreeSansBold18pt7b;  // Sans Bold 18 (altura= 25)
  fontPtr[1]= (GFXfont *)&FreeSansBold24pt7b;  // Sans Bold 24 (altura= 35)
  fontPtr[2]= (GFXfont *)&FreeMonoBold18pt7b;  // Mono Bold 18
  fontPtr[3]= (GFXfont *)&FreeMonoBold24pt7b;  // Mono Bold 24
  updateStatus();
  lineaMain[0]= "";
  lineaMain[1]= "";
  NimBLEDevice::init("");
  pClient= nullptr;
//  NimBLEAddress localAddress = NimBLEDevice::getAddress();  // mac del ESP32
//  Serial.print("ESP32 MAC: ");
//  Serial.println(localAddress.toString().c_str());
  retryWifi= 0;
  milliLoop= millis();
  if (milliLoop == 0) milliLoop++;
  lastTmTry= milliLoop - EPOCHRETRY;  // restar para que inicie sin esperar
  debounce= milliLoop; // boton no cambio
}

void loop() {
  milliLoop= millis();   // usamos el mismo tiempo transcurrido en toda la rutina
  if (milliLoop == 0) milliLoop++; // esto puede suceder cada 49 dias (puuf) se saltea el 0 para poder comparar tiempos 0 como indefinidos
// leer boton
  lectura= digitalRead(GPIObtn);  // tomar una lectura del boton
  debounceTime= milliLoop - debounce;
  // procesar boton
  if (lectura != oldInp) {  // cambio en el estado del boton
    oldInp= lectura;        // el nuevo "viejo-estado" es el valor de port
    debounce= milliLoop;    // comienzo tiempo de debounce
  }
  else {
    if (debounceTime >= DEBTIME) { // boton apretado mas de DEBTIME (debounced)
      if (lectura == boton) {  // si el port es igual al estado actual del boton
        if ((debounceTime > HOLDTIME) && (boton == BTNPRESSED) && (!btnHeld)) {  // veamos si el boton fue sostenido
          // boton sostenido
          btnHeld= true;
          switch (wifiState) {
            case WIFIOFF:  // estaba apagado -> pasar a WIFION
              nuevoModoWiFi(WIFION);
              break;
            case WIFION:  // estaba encendido desconectado -> pasar a modo AP
              nuevoModoWiFi(WIFIAPM);
              break;
            case WIFICON:  // estaba encendido y conectado -> pasar a modo AP
              nuevoModoWiFi(WIFIAPM);
              break;
            default: // estaba en modo AP (o desconocido) -> apagar WiFi
              nuevoModoWiFi(WIFIOFF);
              break;
          }
        }
      }
      else { // el port es distinto del estado actual del boton y ya paso el tiempo DEBOUNCE
        boton= lectura;
        if (boton == BTNRELEASED) {  // solto el boton
            if (!btnHeld) { // no estaba sostenido
              // presionaron el boton!  Imprimir!
              if (timeIsValid) {
                if ((!prnIsOnline) || (!pClient->isConnected())) prnIsOnline= connect2prn();
                if (prnIsOnline) {
                  blinkStateG= LEDON;
                  digitalWrite(GPIOgreen, blinkStateG);
                  myStatus= STATPRINTING;
                  formatDT(myDT, myPrn);
                  printBuffer();
                  myStatus= STATONLINE;
                }
                else {
                  myStatus= STATDISCON;
                }
                blinkStateG= LEDOFF;
                digitalWrite(GPIOgreen, blinkStateG);
              }
            }
            else btnHeld= false; // solto boton despues de btnHeld
          }
        }
      }
    }
  // segun el estado del WiFi atendemos la pagina
  switch (wifiState) {
    case WIFION:                              // intentando conexion a Router
      if (WiFi.status() == WL_CONNECTED) {    // se conectó!
        retryWifi= 0;
        wifiState= WIFICON;                   // pasar a modo "conectado"
        // iniciar NTP
        configTime(myDT.gmtOffset, myDT.daylight, myDT.ntpServer);
        setenv("TZ", myDT.timeZone, 0);
        tzset();
        iniciaPagina();
        updateBlueLED(LEDWIFION);
        server.handleClient();
      }
      else if ((savedCfg.retryWifi != 0) && (retryWifi != 0)) {   // timeout infinito?
        if ((milliLoop - retryWifi) >= savedCfg.retryWifi) nuevoModoWiFi(WIFIAPM);  // timeout, volver a modo AP
      }
      break;
    case WIFICON:
      if (WiFi.status() == WL_CONNECTED) {
        server.handleClient();  // si seguimos conectados al Router atender pagina
        if ((!timeIsValid) && ((milliLoop - lastTmTry) > EPOCHRETRY)) {  // si la hora es invalida, reintentar NTP
          if (!yelIsBlinking) {
            yelIsBlinking= true;
            digitalWrite(GPIOyellow, LEDON);
            blinkerYellow.attach_ms(BLINKINGTIME , blinkeaY);
          }
          lastTmTry= milliLoop;
          now= time(nullptr);
          if (now > NTP_MIN_VALID_EPOCH) {
            timeIsValid= true;
            if (myStatus == STATINVALTIME) updateStatus();
            blinkerYellow.detach();
            digitalWrite(GPIOyellow, LEDON);
            yelIsBlinking= false;
          }
          if (!mDNS) {
            mDNS= MDNS.begin("chronolabeler"); // setup mdns para poder conectarse mediante "http://chronolabeler.local"
//          if (mDNS) MDNS.addService("http","tcp",80);  // opcional, agregar mDNS como servicio
          }
        }
      }
      else {
        nuevoModoWiFi(WIFION);  // se desconectó, pasar a modo reintentar conexion
        timeIsValid= false;
        blinkerYellow.detach();
        digitalWrite(GPIOyellow, LEDOFF);
        yelIsBlinking= false;
      }
      break;
    case WIFIAPM:
      server.handleClient();    // estamos en modo AP, atender la página
      break;
  }
}