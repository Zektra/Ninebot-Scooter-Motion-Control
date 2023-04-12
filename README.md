# This is a fork! I took the version @kearfy made using the original repo by PsychoMnts / Glenn, and made it work with the Ninebot Max scooters using an ESP32 instead.

## Ninebot-Scooter-Motion-Control
Modification to legalise the Ninebot Max Scooters in The Netherlands.

The idea is to make an small hardware modification on the Ninebot scooters so they comply with the Dutch law. 

To use a scooter (or how we call it here: e-step) in The Netherlands, you must comply with the following rules:
- There must NO throttle button.
- The motor must be limited to 250 watts.
- The motor can only give a "boost" when you kick with your feet.
- The motor should stop working when stop kicking. (fade-out)
- Max speed is 25 km/h.

Example:
Micro has e-steps which are modified to comply with Dutch rules with an deactivated throttle, like the MICRO M1 COLIBRI.
https://www.micro-step.nl/nl/emicro-m1-colibri-nl.html

The best scooter to do this modification is the Xiaomi Mi Electric Scooter Essential:
- The motor is 250 watts
- Max speed is 20 km/h, which is already fast to give push offs with your feet.

Check out [stepombouw.nl](https://stepombouw.nl) for more information and guides!

# Librarys

- https://github.com/contrem/arduino-timer (For 3.1 and 3.2)

# How it works

An ESP32 will be used to read out the serial-bus of the Ninebot Max Scooter.
The speed will be monitored to check for any increases that fit a kick. When one is detected, the throttle will be opened at a suitable percentage for 8 seconds.
When the brake handle is squeezed, throttle will be released immediately. The Max scooter itself also disables throttle upon braking, but this way it won't suddenly accelerate as soon as you release it.

# This fork

As I own a Ninebot Max scooter rather than a Xiaomi, as well as wanting to use an ESP32 over an Arduino Micro, I had to make some modifications to the code to make it work. As the ESP32 lacks any true analog outputs, one will need to use a digipot. This is a tiny chip which converts a digital signal to the preferred analog signal. As digipots are rather scarce, I have decided to use an MCP4725 DAC for now.

The fork is based of off [Kearfy's V3.4.3](https://github.com/kearfy/Xiaomi-Scooter-Motion-Control/tree/v3.4.3).

This firmware will get a couple of new features I would like to use on my scooter, and I will try to add the necessary information for you to use them yourself as well.
Some of the new (planned) features include:
- Wireless updates
- Blinkers
- Presence detection (todo)

**The following section is configurable. Can be set to one kick aswell.**

One kick while throttling will reset the driving timer, two kicks will put the vehicle into INCREASINGSTATE. Whilst in INCREASINGSTATE, the speed of the vehicle can be increased (duh...). When you accelerate the vehicle by making a kick, it will adjust to a new speed. If a defined time has passed without the driver making a new kick, the vehicle will be put back into the DRIVINGSTATE and the driving timer will be started again.
The vehicle will also be switched to INCREASINGSTATE when you first start driving, or after you have released the break and accelerate again.

Once the driving timer expires, the throttle will be released. When you make a new kick the vehicle will adjust to the averageSpeed of the history of speeds recordings.

# Wireless updates
To update your ESP32 wirelessly, fill in the SSID and password of the network where you'd like to update it. Upon boot you will have a 10 second window to upload the sketch. I usually set it to upload and turn it on somewhere during compiling, which works for me. But depending on the speed of your PC you might want to turn it on earlier/later.
Note: it will not drive until those 10 seconds have passed.

# Issues
Unknown for now.

# Releases

- V3.1
    
    This is the first release of V3 (This variation of the firmware).
    It is a proof of concept and works unexpectedly well. The two issues that exist in this build are:
    - that the expected speeds are not consistent with the actual speed obtain from the vehicle.
    - and, and issue in the concept, that it's a little bit hard to speed up above 18 ~ 20km/u.

- V3.2

    The issues have been resolved. It is now possible to percentually lower the speedBump per km/u speed with the lowerSpeedBump option.

    A few recommended values have been provided.

- V3.3

    Skipped due to the amount of issues.

- V3.4

    - Added a option to set a minimum speed for when the minimumSpeedIncreasment will fire at first.
    - Added baseThrottle and additionalSpeed to be able to easily adjust the calculatedSpeed within ThrottleSpeed.
    - Added a kickDelay. This is the minimum time before another kick can be registered again.
    - Switched to timers based on the millis() function. No memory exhaustion anymore!
    - Added a DRIVEOUTSTATE to prevent false kicks after the boost has expired.
    - Probably more changes, cleaned up a lot of stuff.

- V3.4.1

    - Changed default value for lowerSpeedBump to 0.9875 since kicks at higher speeds were impossible.
    - Added forgetSpeed since you could currently drive out from 25km/u to 6km/u, make a kick and go full speed again.
    - Made small changes in the behaviour of the DRIVEOUTSTATE.

- V3.4.2

    - Updated default config options to ideal values. Default values should now be sufficient for all Xiaomi models.
    - Updated kick detection in driveout state as it calculated the speedbump based on actual speed instead of average speed.

- V3.4.3
    - Patched issue where total time driving was driveTime + increasmentTime, thus driveTime should now be accurate.

- V1.0.0
    - Initial version working with the Ninebot Max and ESP32
    - Tweak some values
    - Add OTA updates
    - Add blinkers

# Other firmwares

Feel free to try these other firmwares, I will try to include as many firmwares that are out there.

- Kearfy: https://github.com/kearfy/Xiaomi-Scooter-Motion-Control
- Glenn: https://github.com/PsychoMnts/Xiaomi-Scooter-Motion-Control
- Jelle: https://github.com/jelzo/Xiaomi-Scooter-Motion-Control
- Job: https://github.com/mysidejob/step-support 

# Hardware

- Seeed studio XIAO ESP32C3
- Adafruit MCP4725

For blinkers:
- 3-way switch
- Two LEDs and fitting resistor

# Wiring

![alt text](https://github.com/Zektra/Ninebot-Scooter-Motion-Control/blob/Main/Wiring%20Scheme.png?raw=true)

# Supported models
Limit motor modification required:
- Ninebot Max

# IenW over steps met stepondersteuning

"We stellen ons echter op het standpunt dat een tweewielig voertuig, dat met eigen spierkracht wordt voortbewogen en dat duidelijk op het fietspad thuishoort, in de categorie fiets hoort te vallen. (...)  Door de aard van de ondersteuning vallen deze steppen dus ook in de categorie ‘fiets met trapondersteuning’ en hoeven ze niet apart als bijzondere bromfiets te worden toegelaten. U mag hiermee tot maximaal 25 km/u op de openbare weg rijden."

DE MINISTER VAN INFRASTRUCTUUR EN WATERSTAAT,

Namens deze,

Hoofd afdeling Verkeersveiligheid en Wegvervoer

drs. M.N.E.J.G. Philippens


