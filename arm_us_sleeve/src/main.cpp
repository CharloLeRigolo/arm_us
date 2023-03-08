#include "Arduino.h"
#include "Wire.h"
#include <ros.h>
#include <std_msgs/Int16MultiArray.h>
#include <arm_us_accel_lib.h>

//Constant / Macros
#define ACCEL_ARRAY_SIZE 3
#define ACCEL_ADDRESS_SHOULDER 0x69
#define ACCEL_ADDRESS_WRIST 0x68 //If you we need more accel use pin AD0 as selector 0x68->0x69
#define NB_ACCEL 2

const unsigned short MSG_PERIOD_MS = 100; //ms
// Fct prototypes
void send_msg();

// Global objects
ros::NodeHandle n;
std_msgs::Int16MultiArray accel_msg;

// Accel Shoulder
ros::Publisher pub_accel_shoulder("accel_pos_shoulder", &accel_msg);
arm_us::accel_lib accel_shoulder(ACCEL_ADDRESS_SHOULDER);
// Accel Wrist
ros::Publisher pub_accel_wrist("accel_pos_wrist", &accel_msg);
arm_us::accel_lib accel_wrist(ACCEL_ADDRESS_WRIST);

arm_us::accel_lib *accels[NB_ACCEL] = {&accel_shoulder, &accel_wrist};


//Global variables
unsigned long prev_millis = 0;

void setup()
{
    // ROS
    n.initNode();
    n.advertise(pub_accel_shoulder);
    accel_shoulder.init();
}

void loop()
{
    accel_shoulder.acquire_accel_values();
    
    if (millis() - prev_millis > MSG_PERIOD_MS)
    {
        send_msg(&accel_shoulder, &pub_accel_shoulder);
        n.spinOnce();
        prev_millis = millis();
    }
}

//should be a class method
void send_msg(arm_us::accel_lib *accelerometer, ros::Publisher *pub)
{
    std_msgs::Int16MultiArray msg;
    
    int16_t angle_values[ACCEL_ARRAY_SIZE];

    for (short i = 0; i < ACCEL_ARRAY_SIZE; i++)
    {
        angle_values[i] = accelerometer->get_accel_values_processed(i);
    }

    msg.data = angle_values;
    msg.data_length = ACCEL_ARRAY_SIZE;

    pub->publish(&msg);
}
