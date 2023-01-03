#include "mbed.h"
#include "bbcar.h"
#include "PwmIn.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include <string>
using namespace std;

#define left 1                                                                  // Left mode
#define uturn 2                                                                 // U turn mode
#define extend 3                                                                // End mode

Ticker servo_ticker;                                                            // BB car
Ticker servo_feedback_ticker;
PwmOut pin9(D11), pin10(D10);                                                   
PwmIn pin11(D9), pin12(D12);
BBCar car(pin9, pin11, pin10, pin12, servo_ticker, servo_feedback_ticker);
BusInOut qti_pin(D4,D5,D6,D7);
InterruptIn btn1(BUTTON1);
EventQueue queue_qti(32 * EVENTS_EVENT_SIZE);
Thread t_track;

DigitalInOut pin8(D8);                                                          // Laser ping

WiFiInterface *wifi;                                                            // WIFI and MQTT
EventQueue queue_mqtt;                                                      
Thread t_mqtt;
Thread t_mqttpublish;
Thread t_mqttclose;
NetworkInterface* net = wifi;
MQTTNetwork mqttNetwork(net);
MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

/**** function prototype ****/
void qti_track();                                                               // Track path by QTI sensing
void turn_left();                                                               // Turn left function
void u_turn();                                                                  // U turn
void messageArrived(MQTT::MessageData& md);
void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client);
void close_mqtt();

// Global shared variables
volatile int message_num = 0;
volatile int arrivedcount = 0;
volatile bool closed = false;
const char* topic = "Mbed";
int mode = 0;
int mode_buff = 0;

void u_turn()
{
    car.spin(100,1);
    ThisThread::sleep_for(1200ms);
    car.stop();
    ThisThread::sleep_for(1s);
}

void qti_track()
{
    parallax_qti qti1(qti_pin);
    parallax_laserping ping1(pin8);
    int pattern;
    int distance = 10;
    car.goStraight(50);

    while(1) {
        pattern = (int)qti1;

        switch (pattern) {
            case 0b1000: car.turn(40, 0.1); break;
            case 0b1100: car.turn(40, 0.5); break;
            case 0b0100: car.turn(50, 0.7); break;
            case 0b0110: car.goStraight(50); break;
            case 0b0010: car.turn(50, -0.7); break;
            case 0b0011: car.turn(40, -0.5); break;
            case 0b0001: car.turn(40, -0.1); break;
            case 0b0000: distance = 18; mode = extend; break;               // extend ping distance
            case 0b1111:                                                    // turn left mode
                car.stop();
                mode = left;
                turn_left();
            default: car.goStraight(50);
        }

        if((float)ping1 < distance)                                         // U turn 
        {
            mode = uturn;
            u_turn();
            if(distance == 18) {
                distance = 10;                                              // back to original ping range 
            }
        }

        ThisThread::sleep_for(10ms);                                        // sampling rate
    }
}

void turn_left()
{
    car.spin(100,1);
    ThisThread::sleep_for(575ms);
    car.stop();
    ThisThread::sleep_for(1500ms);
}

void messageArrived(MQTT::MessageData& md) {
    MQTT::Message &message = md.message;
    char msg[300];
    ThisThread::sleep_for(2000ms);
    char payload[300];
    ++arrivedcount;
}
void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client) {
    message_num++;
    MQTT::Message message;
    char buff[100];
    
    if(mode == left) sprintf(buff, "Operation #%d: Turning left...", message_num);
    else if(mode == uturn) sprintf(buff, "Operation #%d: Meet Obstacle! U-turning...", message_num);
    else if(mode == extend) sprintf(buff, "Operation #%d: Extend ping range!", message_num);
    else sprintf(buff, "Operation #%d: Normal operation.", message_num);

    mode = mode_buff;                                                           // back to normal mode 
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*) buff;
    message.payloadlen = strlen(buff) + 1;
    int rc = client->publish(topic, message);

    //printf("rc:  %d\r\n", rc);
    //printf("Puslish message: %s\r\n", buff);

}
void close_mqtt() {
    closed = true;
}

int main() {
    t_track.start((callback(&queue_qti, &EventQueue::dispatch_forever))); 
    queue_qti.call(qti_track);                                                  // Initiate QTI path sensing

    wifi = WiFiInterface::get_default_instance();                               // Initiate WIFI
    if (!wifi) {
            printf("ERROR: No WiFiInterface found.\r\n");
            return -1;
    }


    printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
            printf("\nConnection error: %d\r\n", ret);
            return -1;
    }

	NetworkInterface* net = wifi;
    MQTTNetwork mqttNetwork(net);
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    //TODO: revise host to your IP
    const char* host = "192.168.43.169";
    const int port=1883;
    printf("Connecting to TCP network...\r\n");
    printf("address is %s/%d\r\n", host, port);

    int rc = mqttNetwork.connect(host, port);//(host, 1883);
    if (rc != 0) {
            printf("Connection error.");
            return -1;
    }
    printf("Successfully connected!\r\n");

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "Mbed";

    if ((rc = client.connect(data)) != 0){
            printf("Fail to connect MQTT\r\n");
    }
    if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0){
            printf("Fail to subscribe\r\n");
    }

    t_mqtt.start(callback(&queue_mqtt, &EventQueue::dispatch_forever));
    //t_mqttpublish.start(queue_mqtt.event(&publish_message, &client));   
    queue_mqtt.call_every(2s, &publish_message, &client);

    //btn3.rise(&close_mqtt);

    int num = 0;
    while (num != 5) {
            client.yield(100);
            ++num;
    }

    while (1) {
            if (closed) break;
            client.yield(500);
            ThisThread::sleep_for(1500ms);
    }

    printf("Ready to close MQTT Network......\n");

    if ((rc = client.unsubscribe(topic)) != 0) {
            printf("Failed: rc from unsubscribe was %d\n", rc);
    }
    if ((rc = client.disconnect()) != 0) {
    printf("Failed: rc from disconnect was %d\n", rc);
    }

    mqttNetwork.disconnect();
    printf("Successfully closed!\n");

    return 0;

}