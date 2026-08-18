# ECE4180final
ECE 4180: Embedded Systems Design Final Project, Bowling Speed Detector

Project Report: 
Our ECE 4180 project was a bowling speed detector, which was composed of ESPs, IR LEDs and receivers, a capacitive keypad, a distance sensor, a LCD screen, a potentiometer, and LEDs. Essentially, our project had 4 breadboards where one had the TinyS3, the ultrasonic distance sensor, the capacitive keypad, IR receiver, and LED. One other breadboard was positioned directly across from it and used an IR led to create an IR beam. Then, a certain distance away, we set up the other two breadboards and created another IR beam. Additionally, the breadboard with the other IR reciever needed to have an ESP on it in order to send the time the beam gets broken to the Tiny S3 using ESPNOW. With this setup, we have the fully completed circuit. Basically, with the capacitive keypad, you can manually enter the distance between the two IR beams, or get the ultrasonic sensor to automatically get the distance, or look at the non-volatile leaderboard. Then, after getting the distance, we wait for the beams to be broken by an object passing through them, like a bowling ball. We record the time the beams were broken, and using the formula speed equals distance divided by time, we are able to calculate speed as we know the distance between the lasers and the time it took to break both lasers. The TinyS3 used its two cores to manage the inputs and ESPNOW, the IR LEDs were used to create a IR beam and the IR receivers were used to see whether or not the beam was visible or broken, the LCD was used to show the speed, distance, scores, and button choices, the potentiometer control the screen brightness, and the LEDS were used to indicate whether the beam was broken. 

However, the project was not simple to make as we ran into some major problems. For instance, initially we used a uLCD screen, but for no reason every uLCD screen we tried fried, so we had to switch to a different LCD screen which worked perfectly. Additionally, one problem we struggled with was the sensor alignment as we couldn’t tell where the laser actually was pointing as IR is not visible by humans. However, this problem was averted by adding an LED that lit up if we were not aligned, which allowed us to know when we had proper alignment.

Additionally, this project is similar to how they detect speeds in bowling and industrial conveyor belts as in both those fields they track when the object has broken beams and determine the speed based on distance divided by time. However, our implementation is different as in real bowling they use a great deal more sensors to track the bowling ball the entire time, while our implementation uses two specific points to find the speed. Additionally, the industrial conveyor industry measures the speed of objects using the conveyor belt and photoelectric sensors, so they may miss an extremely fast object if it is not in complete contact with the belt, but our implementation would get the speed of every object regardless of its speed due to the beams.

Finally, if there were more time and resources in the project, the final product could be improved significantly. Firstly, we could add a much bigger screen and get a physical stand, so that people don't have to bend down to see the LCD screen and the LCD screen could be at eye level. Moreover, we could add a mechanical lock to completely lock the breadboards in place once you properly position the beams, and this would ensure that they couldn’t be knocked out of alignment by anyone.

Circuit Diagrams:

This is the IR LED circuit which was placed on two breadboards directly facing the reciever breadbaords.
<img width="554" height="529" alt="irLED_circuit" src="https://github.com/user-attachments/assets/630434a2-5d73-4ccc-bbd0-59e2e73dfff8" />

This is a IR reciever circuit on one of the breadboard, which kept track of the second IR beam.
<img width="715" height="549" alt="irREC_circuit" src="https://github.com/user-attachments/assets/b63d602c-b66e-452f-868a-09d9f2ab586a" />

This is the main circuit with the TinyS3, screen, capacitive keypad, and much more, including a IR reciever, which kept track of the first IR beam.
However, keeping track was simply one of the functions of this circuit as this was the main part of the project.
<img width="768" height="527" alt="zoomout_irLED_circuit" src="https://github.com/user-attachments/assets/d9987cc3-c4f0-4ec9-b113-94762b371195" />



