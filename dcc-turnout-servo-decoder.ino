/*
 * DCC Turnout Servo Decoder
 * -------------------------
 * Arduino-based DCC accessory decoder that moves model railway turnouts
 * (points) with hobby servos.
 *
 * How it works:
 *   1. The DCC signal from the track goes through a 6N137 optocoupler
 *      into Arduino pin 2 (see docs/schematic.png).
 *   2. The DCC_Decoder library decodes accessory packets sent by the
 *      command station (e.g. "switch turnout 7 to thrown").
 *   3. If the address matches one of the servos below, that servo slowly
 *      moves to its "on" or "off" angle.
 *
 * Required libraries:
 *   - DCC_Decoder (MynaBay)
 *   - Servo (built into the Arduino IDE)
 *
 * Configuration:
 *   - NUMSERVOS and SERVOSPEED: right below this comment
 *   - DCC addresses, servo pins and angles: in setup()
 */

#define NUMSERVOS   2 // Number of servos (must match the entries configured in setup())
#define SERVOSPEED 30 // Time in ms between 1-degree servo steps; lower = faster movement

// GO TO setup() TO CONFIGURE DCC ADDRESSES, PIN NUMBERS AND SERVO ANGLES

#include <DCC_Decoder.h>
#include <Servo.h>

unsigned long timetomove; // Time (millis) of the next servo step

// Everything the sketch needs to know about one servo-driven accessory
typedef struct DCCAccessoryData {
  int   address;   // [config]   DCC accessory address this servo listens to
  byte  outputpin; // [config]   Optional extra Arduino output, follows DCC state (e.g. LED / frog relay)
  byte  dccstate;  // [internal] Last DCC command received: 1 = on (thrown), 0 = off (closed)
  byte  angle;     // [internal] Current servo angle
  byte  setpoint;  // [internal] Target angle the servo is moving towards
  byte  offangle;  // [config]   Servo angle for DCC state = 0
  byte  onangle;   // [config]   Servo angle for DCC state = 1
  Servo servo;     // Servo object from the Servo library
};
DCCAccessoryData servo[NUMSERVOS];

// ---------------------------------------------------------------------------
// Called by the DCC_Decoder library every time a basic accessory packet
// arrives on the track. Converts the raw packet into a normal accessory
// address and stores the requested state for the matching servo.
// ---------------------------------------------------------------------------
void BasicAccDecoderPacket_Handler(int address, boolean activate, byte data) {
  // A DCC accessory decoder has 4 outputs. The library gives us the decoder
  // address; the two bits (data & 0x06) select one of its 4 outputs.
  // These lines turn that into the address you type on your command station.
  address -= 1;
  address *= 4;
  address += 1;
  address += (data & 0x06) >> 1;
  // address = address - 4 // uncomment this line for Roco Maus or Z21 (they number addresses with an offset of 4)

  // Lowest bit = direction: 1 = on (thrown), 0 = off (closed)
  boolean enable = (data & 0x01) ? 1 : 0;

  // Debugging: print every received address and state to the Serial Monitor (9600 baud)
Serial.begin(9600);
Serial.println("test");
  Serial.print("Received Address: ");
  Serial.print(address);
  Serial.print(" Enable: ");
  Serial.println(enable);

  // Find the servo with this address and remember the requested state.
  // The actual movement happens later, in loop().
  for (int i=0; i<NUMSERVOS; i++) {
    if (address == servo[i].address) {
      if (enable) servo[i].dccstate = 1;
      else servo[i].dccstate = 0;
    }
  }
}

void setup() {
// ---------------------------------------------------------------------------
// SERVO CONFIGURATION
// One block per servo. To add a servo: copy a block, increase the array
// index ([0], [1], [2] ...) and increase NUMSERVOS at the top of the file.
//
// Tip: set offangle / onangle so the servo just reaches the end of the
// turnout throw. Too far and the servo will strain and buzz.
// ---------------------------------------------------------------------------

  // Servo 0
  servo[0].address   =   7 ; // DCC address
  // servo[0].outputpin =  14 ; // Optional extra output pin (LED / relay)
  servo[0].servo.attach( 9); // Arduino pin the servo signal wire is connected to
  servo[0].offangle  =  110 ; // Servo angle for DCC state = 0 (closed)
  servo[0].onangle   = 75 ; // Servo angle for DCC state = 1 (thrown)

  // Servo 1
  servo[1].address   =   8 ; // DCC address
  // servo[1].outputpin =  14 ; // Optional extra output pin (LED / relay)
  servo[1].servo.attach( 10); // Arduino pin the servo signal wire is connected to
  servo[1].offangle  =  110 ; // Servo angle for DCC state = 0 (closed)
  servo[1].onangle   = 80 ; // Servo angle for DCC state = 1 (thrown)

  // Start the DCC decoder: register our packet handler and
  // read the DCC signal on interrupt 0 (= Arduino pin 2)
  DCC.SetBasicAccessoryDecoderPacketHandler(BasicAccDecoderPacket_Handler, true);
  DCC.SetupDecoder( 0x00, 0x00, 0 );

  // Move every servo to its "off" position at power-up.
  // Servos are started one by one to avoid a current spike on the 5 V supply.
  for(byte i=0; i<NUMSERVOS; i++) {
    pinMode     (servo[i].outputpin, OUTPUT);
    digitalWrite(servo[i].outputpin, LOW);
    servo[i].angle = servo[i].offangle;
    servo[i].servo.write(servo[i].angle);
    delay(1000); // wait 1 second before activating the next servo
  }
}

void loop() {

  // 1) Read DCC data and set each servo's target angle from its DCC state
  for(byte i=0; i<NUMSERVOS; i++) {
    DCC.loop(); // Call to library function that reads the DCC data
    if (servo[i].dccstate == 1) {
      digitalWrite(servo[i].outputpin, HIGH);
      servo[i].setpoint = servo[i].onangle;
    }
    else {
      digitalWrite(servo[i].outputpin, LOW);
      servo[i].setpoint = servo[i].offangle;
    }
  }

  // 2) Every SERVOSPEED ms, move each servo 1 degree towards its target.
  //    This gives the slow, realistic turnout movement.
  if (millis() > timetomove) {
    timetomove = millis() + (unsigned long)SERVOSPEED;
    for (byte i=0; i<NUMSERVOS; i++) {
     if (servo[i].angle < servo[i].setpoint) servo[i].angle++;
      if (servo[i].angle > servo[i].setpoint) servo[i].angle--;
      servo[i].servo.write(servo[i].angle);
    }
  }
}
