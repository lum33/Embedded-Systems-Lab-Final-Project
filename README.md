## How to setup and run my code
1. Setup the BB car and map, map is shown in picture below:

  ![](https://github.com/lum33/final/blob/main/folder/map_design%20(1).jpg)
  
2. Plugin the B_L4S5I_IOT01A mbed board to the PC with USB cable that support USB signals and connect powerbank to USB serial receiver of servos.

3. Setup MQTT broker server and run Python file as client.

4. Run the mbed program and observe the results in Python terminal.

## What are the results
1. Basically, BB car moves around the black line which is contructed to a loop, there are two different marker in this map. 

2. When BB car sense the all black marker (1111), it means that BB car need to turn left.

3. Ping function is available, when laser ping sense the distance between obstacle and car is lesser than 10cm, it will u-turn.

4. When BB car sense the all white marker (0000), it means that BB car will u-turn when the distance between obstacle and car is lesser than 18cm.

5. Picture below shows the output results in python terminal which is implemented by connecting MQTT broker server as client:

  ![](https://github.com/lum33/final/blob/main/folder/py1.JPG)
  
  As we can see above, there are three different modes including turn left, u-turn and extend ping range.
