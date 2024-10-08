//====================================================================
//Project Lynxmotion Phoenix
//Description: Phoenix, control file.
//The control input subroutine for the phoenix software is placed in this file.
//Can be used with V2.0 and above
//Software Version: 4.1
//Last Updated Date: October 5th 2024 
//Programmers:  Shane Easton (GravityDeficient) (Implimented Mandible and Tail code)
//              Eric Shi (tronicsos) (Implemented 24 DoF Apod Hexapod code)
//              Jeroen Janssen [aka Xan]
//              Kurt Eckhardt(KurtE) converted to C and Arduino
//
//Hardware setup: PS2 version
// 
//NEW IN V1.0
//- First Release
//
//Walk method 1:
//- [Left Stick] Strafe / Walk 
//- [Right Stick] On the spot rotation 
//
//Walk method 2:
//- [Left] Stick] Disable
//- [Right Stick] Walk/Rotate
//
//
//PS2 CONTROLS:
//[Common Controls]
//- [Start] Toggle the robot between ON & OFF
//- [L1] Toggle Shift mode
//- [L2] Toggle Rotate mode
//- [Circle] Toggle Single leg mode
//  [Square] Toggle Balance mode
//- [Triangle] Move body to 35 mm from the ground (walk pos) 
//and back to the ground
//- [D-Pad up] Body up 10 mm
//- [D-Pad down] Body down 10 mm
//- [D-Pad left] decrease speed with 50mS
//- [D-Pad right] increase speed with 50mS
//
//[Walk Controls]
//- [Select] Toggle Between Gaits
//- [Left Stick] (Walk mode 1) Walk/Strafe
// (Walk mode 2) Disable
//- Right Stick(Walk mode 1) Rotate, 
//(Walk mode 2) Walk/Rotate
//- [R1] Toggle Double gait travel speed
//- [R2] Toggle Double gait travel length
//
//[Shift Controls]
//- [Left Stick] Shift body X/Z
//- [Right Stick] Shift body Y and rotate body Y
//
//[Rotate Controls]
//- [Left Stick] Rotate body X/Z
//- [Right Stick] Rotate body Y
//
//[Single leg Controls]
//- [Select] Switch legs
//- [Left Stick] Move Leg X/Z (relative)
//- [Right Stick] Move Leg Y (absolute)
//- [R2] Hold/release leg position
//
//[GP Player Controls]
//- [select] Switch Sequences
//- [R2] Start Sequence
//
//====================================================================
// [Include files]
#if ARDUINO>99
#include <Arduino.h> // Arduino 1.0
#else
#include <Wprogram.h> // Arduino 0022
#endif
#include "Hex_Globals.h"
#ifdef USEPS2
#include <PS2X_lib.h>

//[CONSTANTS]
#define WALKMODE          0
#define TRANSLATEMODE     1
#define ROTATEMODE        2
#define SINGLELEGMODE     3
#define GPPLAYERMODE      4

// New and untested
#define AUXILIARYMODE     4


#define cTravelDeadZone 4      //The deadzone for the analog input from the remote
#define  MAXPS2ERRORCNT  5     // How many times through the loop will we go before shutting off robot?
//#define DBGSerial         Serial
#define debugConsoleOn 1

//=============================================================================
// Global - Local to this file only...
//=============================================================================
PS2X ps2x; // create PS2 Controller Class

// Define an instance of the Input Controller...
InputController  g_InputController;       // Our Input controller 

static short      g_BodyYOffset; 
static short      g_sPS2ErrorCnt;
static short       g_BodyYShift;
static byte        ControlMode;
static bool        DoubleHeightOn;
static bool        DoubleTravelOn;
static bool        WalkMethod;
byte            GPSeq;             //Number of the sequence

// some external or forward function references.
extern void MSound(uint8_t _pin, byte cNotes, ...);
extern void PS2TurnRobotOff(void);

//==============================================================================
// This is The function that is called by the Main program to initialize
//the input controller, which in this case is the PS2 controller
//process any commands.
//==============================================================================

// If both PS2 and XBee are defined then we will become secondary to the xbee
void InputController::Init(void)
{
  //DBGSerial.begin(57600)
  
    int error;

    //error = ps2x.config_gamepad(57, 55, 56, 54);  // Setup gamepad (clock, command, attention, data) pins
    error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT);  // Setup gamepad (clock, command, attention, data) pins

    g_BodyYOffset = 65;  // 0 - Devon wanted...
    g_BodyYShift = 0;
    g_sPS2ErrorCnt = 0;  // error count

    ControlMode = WALKMODE;
    DoubleHeightOn = false;
    DoubleTravelOn = false;
    WalkMethod = false;

    g_InControlState.SpeedControl = 100;    // Sort of migrate stuff in from Devon.
}

//==============================================================================
// This function is called by the main code to tell us when it is about to
// do a lot of bit-bang outputs and it would like us to minimize any interrupts
// that we do while it is active...
//==============================================================================
void InputController::AllowControllerInterrupts(boolean fAllow)
{
// We don't need to do anything...
}

//==============================================================================
// This is The main code to input function to read inputs from the PS2 and then
//process any commands.
//==============================================================================
void InputController::ControlInput(void)
{
    // Then try to receive a packet of information from the PS2.
    ps2x.read_gamepad();          //read controller and set large motor to spin at 'vibrate' speed

    // Wish the library had a valid way to verify that the read_gamepad succeeded... Will hack for now
    if ((ps2x.Analog(1) & 0xf0) == 0x70) {
        // In an analog mode so should be OK...
        g_sPS2ErrorCnt = 0;    // clear out error count...
        
        if (ps2x.ButtonPressed(PSB_START)) { //Start button toggles the robot on and off 
        #ifdef debugConsoleOn
        DBGSerial.write("[PS2 Control Action]: Start Button Triggered\n");
        #endif 
            if (g_InControlState.fHexOn) {
                PS2TurnRobotOff();
            } else {
                //Turn on
                g_InControlState.fHexOn = 1;
            }
        }

        if (g_InControlState.fHexOn) { //Based on global control object, hexapod is currently on
            // [SWITCH MODES]
    
             //Translate mode
            if (ps2x.ButtonPressed(PSB_L1) && ControlMode != SINGLELEGMODE) {// L1 Button Test
                #ifdef debugConsoleOn
                  DBGSerial.write("[PS2 Control Action]: L1 Button Triggered - Entering Translation Mode\n");
                #endif 
                MSound(SOUND_PIN, 1, 50, 2000);  //sound SOUND_PIN, [50\4000]
                if (ControlMode != TRANSLATEMODE )
                    ControlMode = TRANSLATEMODE;
                else {
                    if (g_InControlState.SelectedLeg==255) 
                        ControlMode = WALKMODE;
                    else
                        ControlMode = SINGLELEGMODE;
                }
            }
  
            //Rotate mode
            if (ps2x.ButtonPressed(PSB_L2)) {    // L2 Button Test
                            #ifdef debugConsoleOn
                  DBGSerial.write("[PS2 Control Action]: L2 Button Triggered - Entering Rotation Mode\n");
                #endif
                MSound(SOUND_PIN, 1, 50, 2000);  //sound SOUND_PIN, [50\4000]
                if (ControlMode != ROTATEMODE)
                    ControlMode = ROTATEMODE;
                else {
                    if (g_InControlState.SelectedLeg == 255) 
                        ControlMode = WALKMODE;
                    else
                        ControlMode = SINGLELEGMODE;
                }
            }
    
            //Single leg mode fNO
            if (ps2x.ButtonPressed(PSB_CIRCLE)) {// O - Circle Button Test
                if (abs(g_InControlState.TravelLength.x)<cTravelDeadZone && abs(g_InControlState.TravelLength.z)<cTravelDeadZone 
                        && abs(g_InControlState.TravelLength.y*2)<cTravelDeadZone )   {
                                #ifdef debugConsoleOn
                    DBGSerial.write("[PS2 Control Action]: Circle Button Triggered - Entering Single Leg Mode\n");
                    #endif
                    MSound(SOUND_PIN, 1, 50, 2000);  //sound SOUND_PIN, [50\4000]
                    if (ControlMode != SINGLELEGMODE) {
                        ControlMode = SINGLELEGMODE;
                            if (g_InControlState.SelectedLeg == 255)  //Select leg if none is selected
                                g_InControlState.SelectedLeg = 6; //Startleg
                    } else {
                        ControlMode = WALKMODE;
                        g_InControlState.SelectedLeg=255;
                    }
                }
            }      

#ifdef OPT_GPPLAYER
            // GP Player Mode X
            if (ps2x.ButtonPressed(PSB_CROSS)) { // X - Cross Button Test
                MSound(SOUND_PIN, 1, 50, 2000);  //sound SOUND_PIN, [50\4000]
                if (ControlMode != GPPLAYERMODE) {
                    ControlMode = GPPLAYERMODE;
                    GPSeq=0;
                } else
                    ControlMode = WALKMODE;
            }
#endif // OPT_GPPLAYER

            //[Common functions]
            //Switch Balance mode on/off 
            if (ps2x.ButtonPressed(PSB_SQUARE)) { // Square Button Test
                g_InControlState.BalanceMode = !g_InControlState.BalanceMode;
                if (g_InControlState.BalanceMode) {
                    MSound(SOUND_PIN, 1, 250, 1500);  //sound SOUND_PIN, [250\3000]
                } else {
                    MSound(SOUND_PIN, 2, 100, 2000, 50, 4000);
                }
            }

            //Stand up, sit down  
            if (ps2x.ButtonPressed(PSB_TRIANGLE)) { // Triangle - Button Test
                if (g_BodyYOffset>0) 
                    g_BodyYOffset = 0;
                else
                    g_BodyYOffset = 35;
            }

            if (ps2x.Button(PSB_PAD_UP))// D-Up - Button Test
                g_BodyYOffset += 10;

            if (ps2x.Button(PSB_PAD_DOWN))// D-Down - Button Test
                g_BodyYOffset -= 10;

            if (ps2x.Button(PSB_PAD_RIGHT)) { // D-Right - Button Test
                if (g_InControlState.SpeedControl>0) {
                    g_InControlState.SpeedControl = g_InControlState.SpeedControl - 50;
                    MSound(SOUND_PIN, 1, 50, 2000);  //sound SOUND_PIN, [50\4000]
                }
            }

            if (ps2x.Button(PSB_PAD_LEFT)) { // D-Left - Button Test
                if (g_InControlState.SpeedControl<2000 ) {
                    g_InControlState.SpeedControl = g_InControlState.SpeedControl + 50;
                    MSound(SOUND_PIN, 1, 50, 2000);  //sound SOUND_PIN, [50\4000]
                }
            }

            //[Walk functions]
            if (ControlMode == WALKMODE) {
                //Switch gates
                if (ps2x.ButtonPressed(PSB_SELECT)            // Select Button Test
                        && abs(g_InControlState.TravelLength.x)<cTravelDeadZone //No movement
                        && abs(g_InControlState.TravelLength.z)<cTravelDeadZone 
                        && abs(g_InControlState.TravelLength.y*2)<cTravelDeadZone  ) {
                    g_InControlState.GaitType = g_InControlState.GaitType+1;                    // Go to the next gait...
                    if (g_InControlState.GaitType<NUM_GAITS) {                 // Make sure we did not exceed number of gaits...
                        MSound(SOUND_PIN, 1, 50, 2000);  //sound SOUND_PIN, [50\4000]
                    } else {
                        MSound (SOUND_PIN, 2, 50, 2000, 50, 2250); 
                        g_InControlState.GaitType = 0;
                    }
                    GaitSelect();
                }
  
                //Double leg lift height
                if (ps2x.ButtonPressed(PSB_R1)) { // R1 Button Test
                    MSound(SOUND_PIN, 1, 50, 2000);  //sound SOUND_PIN, [50\4000]
                    DoubleHeightOn = !DoubleHeightOn;
                    if (DoubleHeightOn)
                        g_InControlState.LegLiftHeight = 100;
                    else
                        g_InControlState.LegLiftHeight = 50;
                }
  
                //Double Travel Length
                if (ps2x.ButtonPressed(PSB_R2)) {// R2 Button Test
                    MSound (SOUND_PIN, 1, 50, 2000);  //sound SOUND_PIN, [50\4000]
                    DoubleTravelOn = !DoubleTravelOn;
                }
  
                // Switch between Walk method 1 && Walk method 2
                if (ps2x.ButtonPressed(PSB_R3)) { // R3 Button Test
                    MSound (SOUND_PIN, 1, 50, 2000);  //sound SOUND_PIN, [50\4000]
                    WalkMethod = !WalkMethod;
                }
  
                //Walking
                if (WalkMethod)  //(Walk Methode) 
                    g_InControlState.TravelLength.z = (ps2x.Analog(PSS_RY)-128); //Right Stick Up/Down  

                else {
                    g_InControlState.TravelLength.x = -(ps2x.Analog(PSS_LX) - 128);
                    g_InControlState.TravelLength.z = (ps2x.Analog(PSS_LY) - 128);
                }

                if (!DoubleTravelOn) {  //(Double travel length)
                    g_InControlState.TravelLength.x = g_InControlState.TravelLength.x/2;
                    g_InControlState.TravelLength.z = g_InControlState.TravelLength.z/2;
                }

                g_InControlState.TravelLength.y = -(ps2x.Analog(PSS_RX) - 128)/4; //Right Stick Left/Right 
            }

            //[Translate functions]
            g_BodyYShift = 0;
            if (ControlMode == TRANSLATEMODE) {
                g_InControlState.BodyPos.x = (ps2x.Analog(PSS_LX) - 128)/2;
                g_InControlState.BodyPos.z = -(ps2x.Analog(PSS_LY) - 128)/3;
                g_InControlState.BodyRot1.y = (ps2x.Analog(PSS_RX) - 128)*2;
                g_BodyYShift = (-(ps2x.Analog(PSS_RY) - 128)/2);

                g_InControlState.ManPos.x = (ps2x.Analog(PSS_RY) - 128) * 2; // Right stick up/down; mandible up/down
                g_InControlState.ManPos.z = -(ps2x.Analog(PSS_LX) - 128) * 2; // Left stick left/right; mandible rotate CCW/CW

                g_InControlState.TailPos.x = (ps2x.Analog(PSS_LY) - 128) * 2; // Right stick up/down; tail left/right
                g_InControlState.TailPos.y = (ps2x.Analog(PSS_RX) - 128) * 2; // Right stick left/right; tail up/down
            }

            //[Rotate functions]
            if (ControlMode == ROTATEMODE) {
                g_InControlState.BodyRot1.x = (ps2x.Analog(PSS_LY) - 128);
                g_InControlState.BodyRot1.y = (ps2x.Analog(PSS_RX) - 128)*2;
                g_InControlState.BodyRot1.z = (ps2x.Analog(PSS_LX) - 128);
                g_BodyYShift = (-(ps2x.Analog(PSS_RY) - 128)/2);

                g_InControlState.ManPos.z = -(ps2x.Analog(PSS_LX) - 128) * 2; // Left stick left/right; mandible rotate CCW/CW
            }

            //[Single leg functions and Mandible mode]
            if (ControlMode == SINGLELEGMODE) {
              
                //Switch mandible mode on/off
                //Switch leg for single leg control
                if (ps2x.ButtonPressed(PSB_SELECT)) { // Select Button Test
                    MSound (SOUND_PIN, 1, 50, 2000);  //sound SOUND_PIN, [50\4000]
                    if (g_InControlState.SelectedLeg < 6)
                        g_InControlState.SelectedLeg = g_InControlState.SelectedLeg+1;
                    else
                        g_InControlState.SelectedLeg = 0;
                }
                
                // Check if the Mandible is selected (index 6)
                if (g_InControlState.SelectedLeg == MANDIBLE_INDEX) {
                    if (ps2x.Button(PSB_R1)) { // R1 Button Test
                        g_InControlState.ManClos.x += 100;  // Increment by a smaller amount
                        if (g_InControlState.ManClos.x > cMandLeftMAX1)  // Set a reasonable upper limit based on the range
                            g_InControlState.ManClos.x = cMandLeftMAX1;
                        g_InControlState.ManClos.y += 100;  // Increment by a smaller amount
                        if (g_InControlState.ManClos.y > cMandRightMAX1)  // Set a reasonable upper limit based on the range
                            g_InControlState.ManClos.y = cMandRightMAX1;
                    }
                    if (ps2x.Button(PSB_L1)) { // L1 Button Test
                        g_InControlState.ManClos.x -= 100;  // Decrement by a smaller amount
                        if (g_InControlState.ManClos.x < cMandLeftMIN1)  // Set a reasonable lower limit based on the range
                            g_InControlState.ManClos.x = cMandLeftMIN1;
                        g_InControlState.ManClos.y -= 100;  // Decrement by a smaller amount
                        if (g_InControlState.ManClos.y < cMandRightMIN1)  // Set a reasonable lower limit based on the range
                            g_InControlState.ManClos.y = cMandRightMIN1;
                    }

                    g_InControlState.ManPos.x = -(ps2x.Analog(PSS_RY) - 128) * 2; // Right stick up/down; mandible up/down
                    g_InControlState.TailPos.x = -(ps2x.Analog(PSS_RY) - 128) * 2; // Right stick up/down; tail left/right

                    g_InControlState.ManPos.y = (ps2x.Analog(PSS_RX) - 128) * 2; // Right stick left/right; mandible left/right
                    g_InControlState.TailPos.y = (ps2x.Analog(PSS_RX) - 128) * 2; // Right stick left/right; tail up/down
                    
                    g_InControlState.ManPos.z = (ps2x.Analog(PSS_LX) - 128) * 2; // Left stick left/right; mandible rotate CCW/CW
                } else {

                    g_InControlState.SLLeg.x= (ps2x.Analog(PSS_LX) - 128) / 2; //Left Stick Right/Left
                    g_InControlState.SLLeg.y= (ps2x.Analog(PSS_RY) - 128) / 10; //Right Stick Up/Down
                    g_InControlState.SLLeg.z = (ps2x.Analog(PSS_LY) - 128) / 2; //Left Stick Up/Down
                }

                // Hold single leg in place
                if (ps2x.ButtonPressed(PSB_R2)) { // R2 Button Test
                    MSound (SOUND_PIN, 1, 50, 2000);  //sound SOUND_PIN, [50\4000]
                    g_InControlState.fSLHold = !g_InControlState.fSLHold;
                }
            }

#ifdef OPT_GPPLAYER
            //[GPPlayer functions]
            if (ControlMode == GPPLAYERMODE) {

                //Switch between sequences
                if (ps2x.ButtonPressed(PSB_SELECT)) { // Select Button Test
                    if (!g_ServoDriver.FIsGPSeqActive() ) {
                        if (GPSeq < 5) {  //Max sequence
                            MSound (SOUND_PIN, 1, 50, 1500);  //sound SOUND_PIN, [50\3000]
                            GPSeq = GPSeq+1;
                        } else {
                            MSound (SOUND_PIN, 2, 50, 2000, 50, 2250);//Sound SOUND_PIN,[50\4000, 50\4500]
                            GPSeq=0;
                        }
                    }
                }
                //Start Sequence
                if (ps2x.ButtonPressed(PSB_R2))// R2 Button Test
                    g_ServoDriver.GPStartSeq(GPSeq);
            }
#endif // OPT_GPPLAYER

            //Calculate walking time delay
            g_InControlState.InputTimeDelay = 128 - max(max(abs(ps2x.Analog(PSS_LX) - 128), abs(ps2x.Analog(PSS_LY) - 128)), abs(ps2x.Analog(PSS_RX) - 128));
        }
  
        //Calculate g_InControlState.BodyPos.y
        g_InControlState.BodyPos.y = max(g_BodyYOffset + g_BodyYShift,  0);
    } else {
      // We may have lost the PS2... See what we can do to recover...
      if (g_sPS2ErrorCnt < MAXPS2ERRORCNT)
          g_sPS2ErrorCnt++;    // Increment the error count and if to many errors, turn off the robot.
      else if (g_InControlState.fHexOn)
          PS2TurnRobotOff();
       //This line is only required for use with older version of the PS2 library.
       //ps2x.reconfig_gamepad();
    }
}

//==============================================================================
// PS2TurnRobotOff - code used couple of places so save a little room...
//==============================================================================
void PS2TurnRobotOff(void)
{
   //Turn off
    g_InControlState.BodyPos.x = 0;
    g_InControlState.BodyPos.y = 0;
    g_InControlState.BodyPos.z = 0;
    g_InControlState.BodyRot1.x = 0;
    g_InControlState.BodyRot1.y = 0;
    g_InControlState.BodyRot1.z = 0;
    g_InControlState.TravelLength.x = 0;
    g_InControlState.TravelLength.z = 0;
    g_InControlState.TravelLength.y = 0;
    g_BodyYOffset = 0;
    g_BodyYShift = 0;
    g_InControlState.SelectedLeg = 255;
    g_InControlState.fHexOn = 0;
}


#endif //USEPS2


