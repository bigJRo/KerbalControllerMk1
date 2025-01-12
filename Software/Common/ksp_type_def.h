#ifndef _KSP_TYPE_DEF_H
#define _KSP_TYPE_DEF_H

/***************************************************************************************
   Create definitions for the vesselType checks, Ref: https://www.kerbalspaceprogram.com/ksp/api/_vessel_8cs.html
****************************************************************************************/
/**** Vessel Types enum: 0 = Debris, 1 = Space Object, 2 = Unknown, 3 = Probe, 4 = Relay, 5 = Rover, 6 = Lander, 7 = Ship,
  8 = Plane, 9 = Station, 10 = Base, 11 = EVA Kerbal, 12 = Flag, 13 = Deployed Science Controller, 14 = Deployed Science Part,
  15 = Dropped Part, 16 = Deployed Ground Part. Ref: https://www.kerbalspaceprogram.com/ksp/api/_vessel_8cs.html ****/
const uint8_t type_Debris = 0;
const uint8_t type_Object = 1;
const uint8_t type_Unknown = 2;
const uint8_t type_Probe = 3;
const uint8_t type_Relay = 4;
const uint8_t type_Rover = 5;
const uint8_t type_Lander = 6;
const uint8_t type_Ship = 7;
const uint8_t type_Plane = 8;
const uint8_t type_Station = 9;
const uint8_t type_Base = 10;
const uint8_t type_EVA = 11;
const uint8_t type_Flag = 12;
const uint8_t type_SciCtrlr = 13;
const uint8_t type_SciPart = 14;
const uint8_t type_Part = 15;
const uint8_t type_GndPart = 16;


/***************************************************************************************
   Create definitions for the vesselSituation checks, Ref: https://www.kerbalspaceprogram.com/ksp/api/class_vessel.html
****************************************************************************************/
const uint8_t sit_Landed = 1;
const uint8_t sit_Splashed = 2;
const uint8_t sit_PreLaunch = 4;
const uint8_t sit_Flying = 8;
const uint8_t sit_SubOrb = 16;
const uint8_t sit_Orbit = 32;
const uint8_t sit_Escaping = 64;
const uint8_t sit_Docked = 128;

#endif  // _KSP_TYPE_DEF_H
