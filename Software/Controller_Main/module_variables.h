/***************************************************************************************
   Panel Control Support Variables
    Defines bit masks for button press & LED locations in the registers
    Defines variables for button and LED outputs
****************************************************************************************/
uint8_t ledPanel = 0;
uint8_t prevButtonPanel;
uint8_t newButtonPanel;
int16_t enc1_pos = 0;
int16_t prev_enc1_pos = 0;
int16_t enc2_pos = 0;
int16_t prev_enc2_pos = 0;
const uint8_t pPanelMode = 0b00000011;    //Panel Control Input
const uint8_t pPanelButt01 = 0b00000100;  //Panel Control Button 01
const uint8_t pPanelButt02 = 0b00001000;  //Panel Control Button 02
const uint8_t pPanelButt03 = 0b00010000;  //Panel Control Button 03
const uint8_t pExtCam = 0b00100000;       //External Camera Rotary Switch
const uint8_t pCtrlGrp = 0b01000000;      //Control Group Rotary Switch


/***************************************************************************************
   UI Control Module Mask Definitions
    Defining bit masks for button press & LED locations in the registers
****************************************************************************************/
uint16_t prevButtonUI;
uint16_t newButtonUI;
const uint16_t pMap = 0b0000000000000001;           //Toggle Map Screen Button
const uint16_t pNavBall = 0b0000000000000010;       //Toggle Nav Ball Button
const uint16_t pMapRev = 0b0000000000000100;        //Cycle Map -
const uint16_t pCycleShipRev = 0b0000000000001000;  //Cycle Ship -
const uint16_t pMapFwd = 0b0000000000010000;        //Cycle Map +
const uint16_t pCycleShipFwd = 0b0000000000100000;  //Cycle Ship +
const uint16_t pUIVisible = 0b0000000001000000;     //Toggle User Interface
const uint16_t pIVA = 0b0000000010000000;           //Enter IVA Mode
const uint16_t pCycleCam = 0b0000000100000000;      //Cycle Camera
const uint16_t pResetCam = 0b0000001000000000;      //Reset Camera
const uint16_t pScreenShot = 0b0000010000000000;    //Take Screen Shot
const uint16_t pDebugWindow = 0b0000100000000000;   //Enter KSP Debug Panel


/***************************************************************************************
   Stability Control Module Mask Definitions
    Defining bit masks for button press & LED locations in the registers
****************************************************************************************/
uint16_t ledStabCtrl = 0;
uint16_t prevButtonStabCtrl;
uint16_t newButtonStabCtrl;
const uint16_t pStabAssist = 0b0000000000000001;  //SAS Stability Assist Button
const uint16_t pManeuver = 0b0000000000000010;    //SAS Maneuver Button
const uint16_t pPrograde = 0b0000000000000100;    //SAS Prograde Button
const uint16_t pRetrograde = 0b0000000000001000;  //SAS Retrograde Button
const uint16_t pNormal = 0b0000000000010000;      //SAS Normal Button
const uint16_t pAntiNormal = 0b0000000000100000;  //SAS Anti-normal Button
const uint16_t pRadialIn = 0b0000000001000000;    //SAS Radial In Button
const uint16_t pRadialOut = 0b0000000010000000;   //SAS Radial Out Button
const uint16_t pTarget = 0b0000000100000000;      //SAS Target Button
const uint16_t pAntiTarget = 0b0000001000000000;  //SAS Anti-Target Button
const uint16_t pPrecision = 0b0000010000000000;   //Precision Switch
const uint16_t pInvertSAS = 0b0000100000000000;   //Invert SAS Button
const uint16_t pSASEnable = 0b0001000000000000;   //SAS Enabled Switch
const uint16_t pRCSEnable = 0b0010000000000000;   //RCS Enabled Switch
// Mask of everything that is game indicated
const uint16_t pStabAsstGrp = pStabAssist + pManeuver + pPrograde + pRetrograde + pNormal + pAntiNormal + pRadialIn + pRadialOut + pTarget + pAntiTarget;


/***************************************************************************************
   Function Control Module Mask Definitions
    Defining bit masks for button press & LED locations in the registers
****************************************************************************************/
uint16_t ledFuncCtrl = 0;
uint16_t prevButtonFuncCtrl;
uint16_t newButtonFuncCtrl;
const uint16_t pLES = 0b0000000000000001;         //Launch Escape button; Custom Action Group 18
const uint16_t pCollectSci = 0b0000000000000010;  //Science Collect button; Custom Action Group 20
const uint16_t pScience1 = 0b0000000000000100;    //Science Group 1; Custom Action Group 21
const uint16_t pScience2 = 0b0000000000001000;    //Science Group 2; Custom Action Group 22
const uint16_t pEngine1 = 0b0000000000010000;     //Engine Group 1; Custom Action Group 30
const uint16_t pEngine2 = 0b0000000000100000;     //Engine Group 2; Custom Action Group 31
const uint16_t pEngineMode = 0b0000000001000000;  //Engine Mode Toggle; Custom Action Group 32
const uint16_t pIntake = 0b0000000010000000;      //Intake Toggle; Custom Action Group 33
const uint16_t pCtrlPt1 = 0b0000000100000000;     //Control Point 1 Select; Custom Action Group 40
const uint16_t pCtrlPt2 = 0b0000001000000000;     //Control Point 2 Select; Custom Action Group 41
// Mask of everything that is game indicated
const uint16_t pActPnl1 = pLES + pCollectSci + pScience1 + pScience2 + pEngine1 + pEngine2 + pEngineMode + pIntake;


/***************************************************************************************
   EVA Module Mask Definitions
    Defining bit masks for button press & LED locations in the registers
****************************************************************************************/
uint16_t ledEVA = 0;
uint8_t prevButtonEVA;
uint8_t newButtonEVA;
const uint8_t pEVA_Up = 0b00000001;     //EVA Thrust Up
const uint8_t pEVA_Down = 0b00000010;   //EVA Thrust Down
const uint8_t pEVA_Board = 0b00000100;  //EVA Board
const uint8_t pEVA_Grab = 0b00001000;   //EVA Grab
const uint8_t pEVA_Jump = 0b00010000;   //EVA Jump
const uint8_t pEVA_Const = 0b00100000;  //EVA Construction
const uint8_t pEVA_Light = 0b01000000;  //EVA Light
const uint8_t pEVA_Jet = 0b10000000;    //EVA Jet
// Mask of everything that is game indicated
const uint8_t pEVA = pEVA_Light + pEVA_Jet;


/***************************************************************************************
   Translation Module Mask Definitions
    Defining bit masks for button press & LED locations in the registers
****************************************************************************************/
uint8_t prevButtonTransButt;
uint8_t newButtonTransButt;
const uint8_t pTranslation_Butt = 0b00000001;  //Translation Joystick Button


/***************************************************************************************
   Throttle Module Mask Definitions
    Defining bit masks for button press & LED locations in the registers
****************************************************************************************/
uint8_t ledThrtl = 0;
uint8_t prevButtonThrtl;
uint8_t newButtonThrtl;
const uint8_t pThrottle100 = 0b00000001;   //Throttle to full button
const uint8_t pThrottleUp = 0b00000010;    //Throttle Up Button
const uint8_t pThrottleDown = 0b00000100;  //Throttle Down Button
const uint8_t pThrottle0 = 0b00001000;     //Throttle to Off button


/***************************************************************************************
   Action Control Module Mask Definitions
    Defining bit masks for button press & LED locations in the registers
****************************************************************************************/
uint16_t ledActCtrl = 0;
uint16_t prevButtonActCtrl;
uint16_t newButtonActCtrl;
const uint16_t pAG1 = 0b0000000000000001;   //Action Group 1; Custom Action Group 1
const uint16_t pAG2 = 0b0000000000000010;   //Action Group 2; Custom Action Group 2
const uint16_t pAG3 = 0b0000000000000100;   //Action Group 3; Custom Action Group 3
const uint16_t pAG4 = 0b0000000000001000;   //Action Group 4; Custom Action Group 4
const uint16_t pAG5 = 0b0000000000010000;   //Action Group 5; Custom Action Group 5
const uint16_t pAG6 = 0b0000000000100000;   //Action Group 6; Custom Action Group 6
const uint16_t pAG7 = 0b0000000001000000;   //Action Group 7; Custom Action Group 7
const uint16_t pAG8 = 0b0000000010000000;   //Action Group 8; Custom Action Group 8
const uint16_t pAG9 = 0b0000000100000000;   //Action Group 9; Custom Action Group 9
const uint16_t pAG10 = 0b0000001000000000;  //Action Group 10; Custom Action Group 10
// Mask of everything that is game indicated
const uint16_t pActPnl2 = pAG1 + pAG2 + pAG3 + pAG4 + pAG5 + pAG6 + pAG7 + pAG8 + pAG9 + pAG10;


/***************************************************************************************
   Switch Module Mask Definitions
    Defining bit masks for button press & LED locations in the registers
****************************************************************************************/
uint8_t ledSwitch = 0;
uint8_t prevButtonSwitch;
uint8_t newButtonSwitch;
const uint8_t pSCEtoAUX = 0b00000001;  //SCE Switch set to AUX
const uint8_t pAudio = 0b00000010;     //Audio Enabled Switch
const uint8_t pSwitch3 = 0b00000100;   //Switch 3
const uint8_t pSwitch4 = 0b00001000;   //Switch 4


/***************************************************************************************
   Rotation Module Mask Definitions
    Defining bit masks for button press & LED locations in the registers
****************************************************************************************/
uint8_t prevButtonRot;
uint8_t newButtonRot;
const uint8_t pRotation_Butt = 0b00000001;  //Translation Joystick Button
const uint8_t pVehMode = 0b00000110;        //Vehicle Mode Switch Position
const uint8_t pResetTrim = 0b00001000;      //Reset Trim Button
const uint8_t pTrim = 0b00010000;           //Trim Button


/***************************************************************************************
   Vehicle Control Module Mask Definitions
    Defining bit masks for button press & LED locations in the registers
****************************************************************************************/
uint16_t ledVehCtrl = 0;
uint16_t prevButtonVehCtrl;
uint16_t newButtonVehCtrl;
const uint16_t pLights = 0b0000000000000001;      //Lights button
const uint16_t pGear = 0b0000000000000010;        // Gear button
const uint16_t pSolarArray = 0b0000000000000100;  // Solar Array button; Custom Action Group 11
const uint16_t pRadiator = 0b0000000000001000;    // Radiator button; Custom Action Group 14
const uint16_t pDrogue = 0b00000000000100000;     // Drogue Chute button; Custom Action Group 15
const uint16_t pBrakes = 0b0000000000100000;      // Brakes button
const uint16_t pLadder = 0b0000000001000000;      // Ladder button; Custom Action Group 17
const uint16_t pAntenna = 0b0000000010000000;     // Antenna button; Custom Action Group 12
const uint16_t pCargoDoor = 0b0000000100000000;   // Cargo Door button; Custom Action Group 13
const uint16_t pParachute = 0b0000001000000000;   //Parachute button; Custom Action Group 16
const uint16_t pLightsLock = 0b0000010000000000;  // Lights Lock switch
const uint16_t pGearLock = 0b0000100000000000;    // Gear Lock switch
const uint16_t pBrakeLock = 0b0001000000000000;   // Brake Lock switch
const uint16_t pChutesAuto = 0b0010000000000000;  // Enable Parachutes to Auto; applies logic to auto command parachute deployment
// Mask of everything that is game indicated
const uint16_t pGameVehCtrl = pLights + pGear + pBrakes + pLadder + pSolarArray + pAntenna + pRadiator + pCargoDoor + pParachute + pDrogue;


/***************************************************************************************
   Time Module Mask Definitions
    Defining bit masks for button press & LED locations in the registers
****************************************************************************************/
uint8_t ledTime = 0;
uint16_t prevButtonTime;
uint16_t newButtonTime;
const uint16_t pSave = 0b0000000000000001;      //Quick Save Button
const uint16_t pPause = 0b0000000000000010;     //Pause Button
const uint16_t pLoad = 0b0000000000000100;      //Quick Load Button
const uint16_t pWarpSOI = 0b0000000000001000;   //Time Warp to SOI
const uint16_t pWarpMorn = 0b0000000000010000;  //Time Warp to Next Morning
const uint16_t pWarpAp = 0b0000000000100000;    //Time Warp to Apoapsis
const uint16_t pWarpPe = 0b0000000001000000;    //Time Warp to Periapsis
const uint16_t pWarpMnvr = 0b0000000010000000;  //Time Warp to Manuever
const uint16_t pPhysTW = 0b0000000100000000;    //Physical Time Warp Override
const uint16_t pWarpDec = 0b0000001000000000;   //Decrease Time Warp
const uint16_t pWarpInc = 0b0000010000000000;   //Increase Time Warp
const uint16_t pCancelTW = 0b0000100000000000;  //Increase Time Warp