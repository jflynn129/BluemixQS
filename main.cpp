//#define MQTT_DEBUG

#include "mbed.h"
#include "MQTTClient.h"
#include "MQTTFormat.h"

//#include "MQTTEthernet.h"
#include "MQTTWNCInterface.h"
#include "rtos.h"
#include "k64f.h"
#include "HTS221.h"

I2C i2c(PTC11, PTC10);         //SDA, SCL -- define the I2C pins being used
MODSERIAL pc(USBTX,USBRX,256,256);

#include "hardware.h"


// connect options for MQTT broker
#define CLIENT      "quickstart"
#define URL         CLIENT ".messaging.internetofthings.ibmcloud.com"
#define CONFIGTYPE  "d:" CLIENT ":MicroZed:%s"

#define PORT        1883                           // MQTT broker port number
#define CLIENTID    "96430312d8f7"                 // use MAC address without colons
#define USERNAME ""                                    // not required for demo app
#define PASSWORD ""                                    // not required for demo app
#define PUBLISH_TOPIC "iot-2/evt/status/fmt/json"              // MQTT topic
#define SUBSCRIBTOPIC "iot-2/cmd/+/fmt/+"

#define HTS221_STR  "  Temp  is: %0.2f F \n\r  Humid is: %02d %%\n\r"

Queue<uint32_t, 6> messageQ;

struct rcvd_errs{
    int err;
    char *er;
    };
    
rcvd_errs response[] = {
    200, "200 OK - Request has succeeded.",
    201, "201 Created - Request has been fulfilled and a new resource created.",
    202, "202 Accepted - The request has been accepted for processing, but the processing will be completed asynchronously",
    204, "204 No Content - The server has fulfilled the request but does not need to return an entity-body.",
    400, "400 Bad Request - Bad request (e.g. sending an array when the API is expecting a hash.)",
    401, "401 Unauthorized - No valid API key provided.",
    403, "403 Forbidden - You tried to access a disabled device, or your API key is not allowed to access that resource, etc.",
    404, "404 Not Found - The requested item could not be found.",
    405, "405 Method Not Allowed - The HTTP method specified is not allowed.",
    415, "415 Unsupported Media Type - The requested media type is currently not supported.",
    422, "422 Unprocessable Entity - Can result from sending invalid fields.",
    429, "429 Too Many Requests - The user has sent too many requests in a given period of time.",
    500, "500 Server errors - Something went wrong in the M2X server",
    502, "502 Server errors - Something went wrong in the M2X server",
    503, "503 Server errors - Something went wrong in the M2X server",
    504, "504 Server errors - Something went wrong in the M2X server",
    };
#define RCMAX   sizeof(response)/sizeof(rcvd_errs)

char * response_str(int rc) {
    static char *unkown = "Unknown error code...";
    int i=0;
    while( response[i].err != rc && i < RCMAX)
        i++;
    return (i<RCMAX? response[i].er : unkown);
}

// LED color control function
void controlLED(color_t led_color) {
    switch(led_color) {
        case red :
            greenLED = blueLED = 1;          
            redLED = 0.7;
            break;
        case green :
            redLED = blueLED = 1;
            greenLED = 0.7;
            break;
        case blue :
            redLED = greenLED = 1;
            blueLED = 0.7;
            break;
        case off :
            redLED = greenLED = blueLED = 1;
            break;
    }
}
    
// Switch 2 interrupt handler
void sw2_ISR(void) {
    messageQ.put((uint32_t*)22);
}

// Switch3 interrupt handler
void sw3_ISR(void) {
    messageQ.put((uint32_t*)33);
}
 
// MQTT message arrived callback function
void messageArrived(MQTT::MessageData& md) {
    MQTT::Message &message = md.message;
    printf("Receiving MQTT message:  %.*s\r\n", message.payloadlen, (char*)message.payload);
    
    if (message.payloadlen == 3) {
        if (strncmp((char*)message.payload, "red", 3) == 0)
            controlLED(red);
        
        else if(strncmp((char*)message.payload, "grn", 3) == 0)
            controlLED(green);
        
        else if(strncmp((char*)message.payload, "blu", 3) == 0)
            controlLED(blue);
        
        else if(strncmp((char*)message.payload, "off", 3) == 0)
            controlLED(off);
    }        
}

int main() {
    int rc, qStart=0, txSel=0, good = 0;
    Timer tmr;
    char* topic = PUBLISH_TOPIC;
    char clientID[100], buf[100], uniqueID[50];

    HTS221 hts221;

    pc.baud(115200);
    rc = hts221.init();
    if ( rc  ) {
        pc.printf(BLU "HTS221 Detected (0x%02X)\n\r",rc);
        pc.printf(HTS221_STR, CTOF(hts221.readTemperature()), hts221.readHumidity()/10);
    }
    else {
        pc.printf(RED "HTS221 NOT DETECTED!\n\r");
    }


    // turn off LED  
    controlLED(blue);
    
    // set SW2 and SW3 to generate interrupt on falling edge 
    switch2.fall(&sw2_ISR);
    switch3.fall(&sw3_ISR);

    // initialize ethernet interface
    MQTTwnc ipstack = MQTTwnc();
    
    // get and display client network info
    WNCInterface& eth = ipstack.getEth();
    
    // construct the MQTT client
    MQTT::Client<MQTTwnc, Countdown> client = MQTT::Client<MQTTwnc, Countdown>(ipstack);
    
    char* hostname = URL;
    int port = PORT;
    char *tptr = eth.getMACAddress();
    strncpy(buf,tptr,strlen(tptr));
    for( int x=3, i=2; i<strlen(tptr)-2; x+=3,i++ ){
        buf[i] = buf[x];
        buf[x]=buf[x+1];
        }
    sprintf(uniqueID, "Mz-%s-7010", buf);
    sprintf(clientID, CONFIGTYPE, uniqueID);
    printf("ClientID='%s'\r\n",clientID);

    printf("      _____\r\n");
    printf("     *     *\r\n");
    printf("    *____   *____             Bluemix IIoT Demo using\r\n");
    printf("   * *===*   *==*             the AT&T IoT Starter Kit\r\n");
    printf("  *___*===*___**  AVNET\r\n");
    printf("       *======*\r\n");
    printf("        *====*\r\n");
    printf("\r\n");
    printf("This demonstration program operates the same as the original \r\n");
    printf("MicroZed IIoT Starter Kit except it only reads from the HTS221 \r\n");
    printf("temp sensor (no 31855 currently present and no generated data).\r\n");
    printf("\r\n");
    printf("Local network info...\r\n");    
    printf("IP address is %s\r\n", eth.getIPAddress());
    printf("MAC address is %s\r\n", eth.getMACAddress());
    printf("Gateway address is %s\r\n", eth.getGateway());
    printf("Your <uniqueID> is: %s\r\n", uniqueID);
    printf("---------------------------------------------------------------\r\n");

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       

    int tries;
    
    while( !good ) {    
        tries=0;
        // connect to TCP socket and check return code
        tmr.start();
        rc = 1;
        while( rc && tries < 3) {
            printf("\r\n\r\n(%d) Attempting TCP connect to %s:%d:  ", tries++, hostname, port);
            rc = ipstack.connect(hostname, port);
            if( rc ) {
                printf("Failed (%d)!\r\n",rc);
                while( tmr.read_ms() < 5000 ) ;
                tmr.reset();
            }
            else {
                printf("Success!\r\n");
                rc = 0;
            }
        }
        if( tries < 3 )
          tries = 0;
        else
          continue;
        
        data.willFlag = 0;  
        data.MQTTVersion = 3;

        data.clientID.cstring = clientID;
    //    data.username.cstring = USERNAME;
    //    data.password.cstring = PASSWORD;
        data.keepAliveInterval = 10;
        data.cleansession = 1;
    
        rc = 1;
        tmr.reset(); 
        while( !client.isConnected() && rc && tries < 3) {
            printf("(%d) Attempting MQTT connect to %s:%d: ", tries++, hostname, port);
            rc = client.connect(data);
            if( rc ) {
                printf("Failed (%d)!\r\n",rc);
                while( tmr.read_ms() < 5000 );
                tmr.reset();
            }
            else
                printf("Success!\r\n");
        }

        if( tries < 3 )
          tries = 0;
        else
          continue;

#if 0
//Only need to subscribe if we are not running quickstart        
        // subscribe to MQTT topic
        tmr.reset();
        rc = 1;
        while( rc && client.isConnected() && tries < 3) {
            printf("(%d) Attempting to subscribing to MQTT topic %s: ", tries, SUBSCRIBTOPIC);
            rc = client.subscribe(SUBSCRIBTOPIC, MQTT::QOS0, messageArrived);
            if( rc ) {
                printf("Failed (%d)!\r\n", rc);
                while( tmr.read_ms() < 5000 );
                tries++;
                tmr.reset();
            }
            else {
                good=1;
                printf("Subscribe successful!\r\n");
            }
        }
#else
    good = 1;
#endif
        while (!good);
    }        
    
    MQTT::Message message;
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    
    while(true) {
        osEvent switchEvent = messageQ.get(100);
      
        if( switchEvent.value.v == 22 )  //switch between sending humidity & temp
          txSel = !txSel;
          
        if( switchEvent.value.v == 33)   //user wants to run in Quickstart of Demo mode
          qStart = !qStart;
                    
        memset(buf,0x00,sizeof(buf));
        if( txSel ) {
            rc = hts221.readHumidity();
            sprintf(buf, "{\"d\" : {\"humd\" : \"%2d.%d\" }}", rc/10, rc-((rc/10)*10));
            printf("Publishing MQTT message '%s' ", (char*)message.payload);
            }
        else {
             sprintf(buf, "{\"d\" : {\"temp\" : \"%5.1f\" }}", CTOF(hts221.readTemperature()));
             printf("Publishing MQTT message '%s' ", (char*)message.payload);
            }
         message.payloadlen = strlen(buf);
         printf("(%d)\r\n",message.payloadlen);
         rc = client.publish(topic, message);
         if( rc ) {
             printf("Publish request failed! (%d)\r\n",rc);
             FATAL_WNC_ERROR(resume);
             }

        client.yield(6000);
    }           
}
