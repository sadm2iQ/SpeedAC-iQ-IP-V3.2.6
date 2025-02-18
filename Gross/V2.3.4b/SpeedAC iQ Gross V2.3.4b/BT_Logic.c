﻿/*-----------------------------------------------------------------------------
-- File Name: BT_Logic.c 
--
-- Copyright 2015 PremierTech as an unpublished work. 
-- All Rights Reserved.
--
-- The information contained herein is confidential property of 
-- Premier Tech.  The use, copying, transfer or disclosure of such
-- information is prohibited except by express written agreement with
-- Premier Tech.
--
-- First written on August, 2015 by Steve Santerre.
--
-- Module Description:
-- This file is dedicated to all control logic and peripheral functions
------------------------------------------------------------------------------*/

#define EXTERNAL_BAG_COUNTER_ON_TIME 0.5  // 0.5 seconds

//Global Variables
int SeqModeManagerMEM, SeqMasterMEM, SeqFeederMEM,SeqFeedSTDMEM,SeqKickerMEM, SeqLockingDeviceMEM, SeqLanceMEM;
int SeqModeManagerNext, SeqMasterNext, SeqSpoutNext, SeqWeighPanNext, SeqFeederNext,SeqFeedSTDNext,SeqManDischargeNext,SeqKickerNext;

bool OutOfTol[2];
bool ApplyLockingDevice,RemoveLockingDevice=OFF;
bool ResetLance=OFF;
bool OK4BulkFeed,OK4IntermFeed,OK4DribbleFeed,FirstWeighment=ON;
bool I_StartMem,I_StopMem,DischReqMem,DischReq,I_ResetOutofTolAlarmMem,BagAtKickerMem,KickerRetractedOffMem,
     I_ClearFaultsMem,I_TglEmptyHopper,I_HopperEmptyMem,I_ManDischMem,BulkFeedMem,DribbleFeedMem;
int I;
bool WeighingDataRefreshFlag;
bool KnockHammerFlag,KnockHammerOut,KnockHammerFlipped,FeedCycleCancelled;
decimal WPMDuration%6.2;
bool ReleaseAcknowledge;
bool ReleaseAcknowledgeInputMEM;
bool ForceZero;
bool BagInPlace;
bool VibrationMem;

/*------------------------------------------------------------------------- 
--  Procedure Name    : GetAoutVibration                             
--  Created by        : Frédéric Allard                                     
--  Last modified on  : 2018/03                                             
--                                                                          
--  Purpose           : Called during dribble feed. Returns the analog pct
--                      to make the catch gate vibrate
--                                                                                                                                                                                                           
--------------------------------------------------------------------------*/

float GetAoutVibration(void)
{
   decimal Temp%6.2;
   
   if(T_Dn(T_VibrationPeriod) || !T_EN[T_VibrationPeriod])
   {
      T_EnOff(T_VibrationPeriod);
      T_PRE[T_VibrationPeriod] = WP.VibrationPeriod;
      T_EnOn(T_VibrationPeriod);
      VibrationMem = !VibrationMem;
   }
   
   if(VibrationMem)
   {
      Temp = WP.MinFeeding + (WP.VibrationAmplitude);
      if(Temp > 100.00)
      {
         Temp = 100.00;
      }
   }
   else
   {
      Temp = WP.MinFeeding;
   }
   return  float(Temp);
}   

/****************Discharge Request Check***************************/
void DischReqCheck(void)
{
   int SeqModeManagerTemp;
   
   if(!IOTestFlag)
   {
      if(Input_State(I_DischReq))
      {
         if(!DischReq &&(WP.FeedAlgoType != FeediQSim)) //|| (WP.FeedAlgoType == FeediQSim && SeqSpout == stp_INIT))) 
         {  
            if((!DischReqInputOn && SeqModeManager == stp_OFF_MODE) || SeqModeManager != stp_OFF_MODE) //Use to make sure that input is taken in account only when in accepted modes
            { 
               DischReq=ON;
            }
         } 
         DischReqInputOn = ON;
         if(DischReq && SeqModeManager == stp_OFF_MODE && SeqModeManagerTemp != SeqModeManager)
         {
         DischReq=OFF;
         //DischReqMem=OFF;
         } 
      }
      
      if(DischReqMem || DischReqInputOn)
      {
         if(!Input_State(I_DischReq))// || (WP.FeedAlgoType == FeediQSim && SeqMaster != stp_RDYTODISCH))
         {
            if(!T_EN[T_DischReqDebouncing])   
            if(T_PRE[T_DischReqDebouncing]!=0) T_EnOn(T_DischReqDebouncing);
            
            if(T_Dn(T_DischReqDebouncing)|| T_PRE[T_DischReqDebouncing]==0)  
            {
               DischReqMem=OFF;
               DischReq=OFF;
               DischReqInputOn = OFF;
               if(T_EN[T_DischReqDebouncing])
               {
                  T_EnOff(T_DischReqDebouncing);
               }
            }
         }
      }
      if(SeqModeManagerTemp != SeqModeManager)
      SeqModeManagerTemp = SeqModeManager;
   }
} 

/****************Hopper Low Level Fault***************************/
bool CheckLowLevel(void)
{
   bool LowLevelOK=OFF;
   if(SeqModeManager!=stp_OFF_MODE && SeqModeManager!=stp_MANUAL_MODE)
   {
      if(LowLevel[Scale1] && !EmptyHopper)
      {
         LowLevelOK=OFF;
         if(!FaultState(FltW_HopperLowLevel)) 
         FaultHandler(FltW_HopperLowLevel,"");
      }
      else 
      {
         if(FaultState(FltW_HopperLowLevel))
         {
            if(!LowLevelManReset || EmptyHopper)FaultResetReq();
         }
         else
            LowLevelOK=ON;
      }
   }
   return(LowLevelOK);   
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : OutReset();
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08
--  
--  Purpose           : This function is called to Refresh The Databases
--                     used in Background Task
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--                      
--------------------------------------------------------------------------*/
void OutReset(void)
{
   int i;

   for(i=37;i<=MaxArrayIODbase;i++) 
   {
      if(i!=O_Fault) Output_OFF(i);
      IO_Force[i] = OFF;
      IO_TestState[i] = OFF;
   }
   StopAnalogOut();
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : ClearComparators();
--  Created by        : Steve Santerre
--  Last modified on  : 2017/01
--  
--  Purpose           : This function is called to Clear the Comparators
--                     
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--                      
--------------------------------------------------------------------------*/
void ClearComparators(void)
{
   clrcomparator(SCALE_NUMBER,Scale1CmpBulk);
   clrcomparator(SCALE_NUMBER,Scale1CmpInterm);
   clrcomparator(SCALE_NUMBER,Scale1CmpDribble);
   clrcomparator(SCALE_NUMBER,Scale1CmpCatchGate);
   clrcomparator(SCALE_NUMBER,Scale1CmpCatchGateInterm);
   Scale1CmpBulkActive = OFF;
   Scale1CmpIntermActive = OFF;
   Scale1CmpDribbleActive = OFF;
   Scale1CmpCatchGateActive = OFF;
   Scale1CmpCatchGateIntermActive = OFF;
   clrcomparator(SCALE_NUMBER,Scale1CmpAout1);
   clrcomparator(SCALE_NUMBER,Scale1CmpAout2);
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : ClearAnalogOuts();
--  Created by        : Steve Santerre
--  Last modified on  : 2017/04
--  
--  Purpose           : This function is called to Clear the Analog Outputs
--                     
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--                      
--------------------------------------------------------------------------*/
void ClearAnalogOuts(void)
{
   if(AOUTActive[AIO1]) aout(AIO1,0);
   if(AOUTActive[AIO2]) aout(AIO2,0);
} 

/*------------------------------------------------------------------------- 
--  Procedure Name    : StreamRegHigh
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08/28
--  
--  Purpose           : Raise Stream Regulator and Start Delay
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void StreamRegRaise(void)
{
   if(!T_EN[T_StreamRegHigh])
   {
      Output_ON(O_StreamRegulator);
      T_EnOn(T_StreamRegHigh);
   }
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : StreamRegLower
--  Created by        : Steve Santerre
--  Last modified on  : 2017/10/02
--  
--  Purpose           : Raise Stream Regulator and Start Delay
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void StreamRegLower(void)
{
   Output_OFF(O_StreamRegulator);
   T_EnOff(T_StreamRegHigh);
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : FeedSTDReset();
--  Created by        : Steve Santerre
--  Last modified on  : 2017/01
--  
--  Purpose           : This function is called to Clear the Comparators
--                     
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--                      
--------------------------------------------------------------------------*/
void FeedSTDReset(void)
{
   ClearComparators();
   ClearAnalogOuts();
   Output_OFF(O_BulkFeed);
   Output_OFF(O_IntermFeed);
   Output_OFF(O_DribbleFeed);
   StreamRegLower();
   if(WP.CatchGateDelay < CATCHGATEDELAYMAX) 
   {
      Output_OFF(O_CatchGate);
      Output_OFF(O_CatchGateInterm);
   }
   T_EnOff(T_BulkBlankT);
   T_EnOff(T_IntermBlankT);
   T_EnOff(T_DribbleBlankT);
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : InitStandardFeed();
--  Created by        : Steve Santerre
--  Last modified on  : 2017/04
--  
--  Purpose           : This function is called to Initialize Standard Feed
--                     
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--                      
--------------------------------------------------------------------------*/
void InitStandardFeed(bool setfilter)
{
   bool Pass[10];
  
   OpenConnection();
   Pass[0]=SetiQFeedAlgorithm (IQ_FEEDING_ALGORITHM_SCALE);
   Pass[1]=iQOutRelease();
   if(setfilter)
   {
      Pass[2]=iQSetSlowFilterSize (SCALE_CHANNEL, SCALE_NUMBER,Lim((SMFilterSize-5),(SMFilterSize+5),MP.StdFilterSize));
   } 
   if(WP.CatchGateDelay == CATCHGATEDELAYMAX) 
   {
      Output_ON(O_CatchGate);
      Output_ON(O_CatchGateInterm);
   }
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : HoldCycle
--  Created by        : Steve Santerre 
--  Last modified on  : 2017/24/01
--  
--  Purpose           : Hold Cycle for Standard Feed
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void HoldCycle(void)
{
   FeedSTDReset();
   Output_ON(O_CycleInHold);
   CycleInHoldMem = ON;
   HoldCycleTgl = OFF;
   Start_PauseBtn = StartBtn;

}

/*------------------------------------------------------------------------- 
--  Procedure Name    : UnHoldCycle
--  Created by        : Steve Santerre 
--  Last modified on  : 2017/24/01
--  
--  Purpose           : Hold Cycle for Standard Feed
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/

bool UnHoldCycle(void)
{
   bool UnHoldInDribble;
    
   if(NetWeight[Scale1] > (0.9*WP.BulkCutoff)) UnHoldInDribble = ON; 
   else UnHoldInDribble = OFF;
   Output_OFF(O_CycleInHold);
   CycleInHoldMem = OFF;
   HoldCycleTgl = OFF;
   Start_PauseBtn = PauseBtn;
   return(UnHoldInDribble);
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : LimitActuator
--  Created by        : Steve Santerre 
--  Last modified on  : 2017/02
--  
--  Purpose           : Limit of Actuator
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void LimitActuator(void)
{
 float LowLimit,HighLimit;
 int LowMin = 2, HighMax = 98;
 
      if(!MC.ActuatorPosInvert)
       {
        LowLimit = LowMin;
        if(MC.ActuatorExtendLimit < HighMax && MC.ActuatorExtendLimit > 0)
         HighLimit = MC.ActuatorExtendLimit;
        else
         HighLimit = HighMax; 
       }
      else
       {
        HighLimit = HighMax;
        if(MC.ActuatorExtendLimit > LowMin)
         LowLimit = MC.ActuatorExtendLimit;
        else
         LowLimit = LowMin; 
       } 
       
 if(!Simul)
  ActuatorLivePos = ain(AIO1);
 
    if(MC.ActuatorPosInvert)
     ActuatorPosition = 100 - ActuatorLivePos;
    else
     ActuatorPosition = ActuatorLivePos;
 
 if(IO_Enable[O_ExtendActuator] && IO_Enable[O_RetractActuator])
  if(IO_State[O_ExtendActuator] || IO_TestState[O_ExtendActuator] || IO_State[O_RetractActuator] || IO_TestState[O_RetractActuator])
   {
      
    if(!MC.ActuatorPosInvert && ActuatorPosition >= HighLimit || MC.ActuatorPosInvert && ActuatorPosition <= LowLimit)
    {
     IO_Force[O_ExtendActuator]     = OFF;
     IO_TestState[O_ExtendActuator] = OFF;
     Output_OFF(O_ExtendActuator);
    }
    if(MC.ActuatorPosInvert && ActuatorPosition >= HighLimit || !MC.ActuatorPosInvert && ActuatorPosition <= LowLimit)
    {
     IO_Force[O_RetractActuator]     = OFF;
     IO_TestState[O_RetractActuator] = OFF;
     Output_OFF(O_RetractActuator);
    }  
   } 
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : Actuator
--  Created by        : Steve Santerre 
--  Last modified on  : 2017/02
--  
--  Purpose           : Positionning of Actuator
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
bool Actuator(void)
{
   bool  ActuatorDone,pulse;
  float HysterVal = 1.0,Hyster;
  float OffsetVal = 8.0,Offset;
   int ActStep;

 
   if(IO_Enable[O_ExtendActuator] && IO_Enable[O_ExtendActuator])
   {
      if(!T_EN[T_ActuatorON]) T_EnOn(T_ActuatorON);
      if(T_Dn(T_ActuatorON) && !T_EN[T_ActuatorOFF]) T_EnOn(T_ActuatorOFF);
      if(T_Dn(T_ActuatorOFF)){T_EnOff(T_ActuatorON);T_EnOff(T_ActuatorOFF);}
      pulse = !T_EN[T_ActuatorOFF];
      
      
      ActuatorDone = OFF;
      MessageDisplay(lsub(TX_MSGACTUATORMOVING),Step_Message);
      
      if(!Simul)
    ActuatorLivePos = ain(AIO1);
  
    if(MC.ActuatorPosInvert)
     {
      ActuatorPosition = 100 - ActuatorLivePos;
      Hyster = HysterVal;// * -1;
      Offset = OffsetVal;// * -1;
     } 
     else
     {
      ActuatorPosition = ActuatorLivePos;
      Hyster = HysterVal;
      Offset = OffsetVal;
     }
     WP.ActuatorPos = LimActuator(5,95,WP.ActuatorPos);
      if(ActuatorPosition < (WP.ActuatorPos - Hyster))
    {
     if(ActuatorPosition < (WP.ActuatorPos - Offset)) 
      {
       if(!MC.ActuatorPosInvert)
        ActStep = Act_Ext;
       else
        ActStep = Act_Ret; 
      }
     else
      {
       if(!MC.ActuatorPosInvert)
        ActStep = Act_ExtPulsed;
       else
        ActStep =  Act_RetPulsed; 
      }
    }
    
    else if(ActuatorPosition > (WP.ActuatorPos + Hyster))
    {  
     if(ActuatorPosition > (WP.ActuatorPos + Offset))
      {
       if(!MC.ActuatorPosInvert)
        ActStep =  Act_Ret;
       else
        ActStep = Act_Ext; 
      } 
     else
      {
       if(!MC.ActuatorPosInvert)
        ActStep =  Act_RetPulsed;
       else
        ActStep = Act_ExtPulsed;  
      }
    }  
      
    else if(ActuatorPosition >= (WP.ActuatorPos - Hyster) && ActuatorPosition <= (WP.ActuatorPos + Hyster))
     {
      ActStep =  Act_Positioned;
     }  
    
    switch(ActStep)
    {
     case Act_Ext:
      Output_OFF(O_RetractActuator);
      Output_ON(O_ExtendActuator);
      break;
     
     case Act_ExtPulsed:
      Output_OFF(O_RetractActuator);
      if(pulse) 
       Output_ON(O_ExtendActuator);
      else      
       Output_OFF(O_ExtendActuator); 
      break;
     
     case Act_Positioned:
       Output_OFF(O_ExtendActuator);
       Output_OFF(O_RetractActuator); 
       ActuatorDone = ON;
       break; 
      
     case Act_RetPulsed:
      Output_OFF(O_ExtendActuator);
      if(pulse) 
       Output_ON(O_RetractActuator);
      else      
       Output_OFF(O_RetractActuator); 
      break;
      
     case Act_Ret:
      Output_OFF(O_ExtendActuator);
      Output_ON(O_RetractActuator);
      break; 
    
    } 
    
  }
 else ActuatorDone = ON;
 return(ActuatorDone); 
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : WP_IOs
--  Created by        : Steve Santerre 
--  Last modified on  : 2017/02
--  
--  Purpose           : Indicate actual WP to Outputs
--                      & change WP according to inputs
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/

void WP_IOs(void)
{

  if(IO_Enable[I_ChangeWPIN1])
   {
    if(Input_State(I_ChangeWPIN1)) setbit(0,RecipeFromInputs);
    else                           clrbit(0,RecipeFromInputs);
    
    if(Input_State(I_ChangeWPIN2)) setbit(1,RecipeFromInputs);
    else                           clrbit(1,RecipeFromInputs);
    
    if(Input_State(I_ChangeWPIN3)) setbit(2,RecipeFromInputs);
    else                           clrbit(2,RecipeFromInputs);
    
    if(Input_State(I_ChangeWPIN4)) setbit(3,RecipeFromInputs);
    else                           clrbit(3,RecipeFromInputs);

   if(RecipeFromInputs != MemRecipe)
    if(SeqModeManager == stp_OFF_MODE && RecipeFromInputs != 0)
     {
      LoadWPFromInputsFlag = Execute;
     }                         
   }

  if(IO_Enable[O_WPNumbOUT1])
  {
    if(SeqModeManager == stp_AUTO_MODE)
    {
     if(MemRecipe!=MemRecipeIO)
     {
      if(MemRecipe>0&&MemRecipe<=15)
       {
        if(bit(0,MemRecipe))Output_ON(O_WPNumbOUT1);
        else Output_OFF(O_WPNumbOUT1);
        if(bit(1, MemRecipe)) Output_ON(O_WPNumbOUT2);
        else Output_OFF(O_WPNumbOUT2);
        if(bit(2, MemRecipe)) Output_ON(O_WPNumbOUT3);
        else Output_OFF(O_WPNumbOUT3);
        if(bit(3, MemRecipe)) Output_ON(O_WPNumbOUT4);
        else Output_OFF(O_WPNumbOUT4);
       }
      
      else
       {
       Output_OFF(O_WPNumbOUT1);
       Output_OFF(O_WPNumbOUT2);
       Output_OFF(O_WPNumbOUT3);
       Output_OFF(O_WPNumbOUT4);
       }
     MemRecipeIO = MemRecipe;
     }
    }
   else MemRecipeIO = 0;  
  }
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : iQSimFeed
--  Created by        : Steve Santerre
--  Last modified on  : 2017/02
--  
--  Purpose           : Prepare and Start Simulated Weight for Feeding
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void iQSimFeed(void)
{
   decimal BulkDuration%3.1,DribbleDuration%3.1;
   decimal BulkM%6.3,BulkB%6.2,DribbleM%6.3,DribbleB%6.2;
   string BulkFormula[64],DribbleFormula[64];
   
   if(WP.TopUpOFFTime > 0) BulkDuration = WP.TopUpOFFTime;
   else                    BulkDuration = 1.5;
   if(WP.TopUpONTime > 0)  DribbleDuration = WP.TopUpONTime;
   else                    DribbleDuration = 2.5;
   if(WP.BulkCutoff > 0)   BulkCO = WP.BulkCutoff;                
   else                    BulkCO = TargetWeight[Scale1] * 0.75;
   BulkM = (BulkCO*UnitIQtoADM) / BulkDuration;
   BulkB = 0;
   DribbleM = ((TargetWeight[Scale1] - BulkCO)*UnitIQtoADM) / DribbleDuration;
   DribbleB = BulkCO*UnitIQtoADM;
   BulkFormula = concat(string(BulkM)," * t + ",string(BulkB));
   DribbleFormula = concat(string(DribbleM)," * t + ",string(DribbleB));
   iQSimFeedProg = concat(iQSimFormatProgram (BulkFormula, BulkDuration), ":",iQSimFormatProgram (DribbleFormula, DribbleDuration));
   iQSimSetProgram (SCALE_CHANNEL, SCALE_NUMBER, iQSimFeedProg);
   iQSimSetWeight(SCALE_CHANNEL, SCALE_NUMBER,(TargetWeight[Scale1]*UnitIQtoADM));
   iQSimStartProgram (SCALE_CHANNEL, SCALE_NUMBER);   
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : KickerCheck
--  Created by        : Steve Santerre 
--  Last modified on  : 2017/02
--  
--  Purpose           : Positionning of Actuator
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
bool KickerCheck(void)
{
   bool NoBag,Retracted;
   if(IO_Enable[I_BagAtKicker])
   {
      if(Input_State(I_BagAtKicker))
      {
         NoBag = OFF;
         MessageDisplay(lsub(TX_MSGBAGSTILLATKICKER),Step_Message);
      }
      else NoBag = ON;
   }
   else NoBag = ON; 
 
   if(IO_Enable[I_KickerRetracted])
   {
      if(!Input_State(I_KickerRetracted))
      {
         Retracted = OFF;
         MessageDisplay(lsub(TX_MSGBKICKERNOTRETRACTED),Step_Message);
      }
      else Retracted = ON;
   }
   else Retracted = ON;

   return(NoBag && Retracted);
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : LanceCheck
--  Created by        : Frédéric Allard
--  Last modified on  : 2018/02
--  
--  Purpose           : Returns ON if the lance is in high position
--                      Called in prep mode
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/

bool LanceCheck(void)
{
   if(SeqLance == stp_LANCEHIGH || !IO_Enable[I_LanceHighPosition])
   {
      return ON;
   }
   else
   {
      MessageDisplay(lsub(TX_MSGLANCENOTHIGH),Step_Message);
      return OFF;
   }      
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : LockingDeviceCheck
--  Created by        : Frédéric Allard
--  Last modified on  : 2018/02
--  
--  Purpose           : Returns ON if the locking device is removed
--                      Called in prep mode
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
bool LockingDeviceCheck(void)
{
   if(SeqLockingDevice == stp_LDOFF || !IO_Enable[O_LockingDevice])
   {
      return ON;
   }
   else
   {
      return OFF;
   }
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : BulkCutoffCorrection
--  Created by        : Steve Santerre
--  Last modified on  : 2017/04
--  
--  Purpose           : This function is called to Calculate Bulk Cutoff
--                      base on desired Dribble Time & correction
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--                      
--------------------------------------------------------------------------*/
void BulkCutoffCorrection(void)
{
   float TimeDiff,TimePct,BulkCorrection;
   
   //Bulk Cutoff correction only if Dribble time is between 75% and 125% of Desired time)
   if(ProdDribbleDuration[Scale1] >= (WP.DFTime * 0.75) && ProdDribbleDuration[Scale1] <= (WP.DFTime * 1.25))
   {
      TimeDiff = ProdDribbleDuration[Scale1] - WP.DFTime; //+ if dribble too long
      TimePct = TimeDiff/WP.DFTime;
      BulkCorrection = (MP.DFWeightCorr/100) * TimePct;
      WP.BulkCutoff = WP.BulkCutoff *(1 + BulkCorrection);
      WP.BulkCutoff = Lim((TargetWeight[Scale1] - WP.Cutoff),TargetWeight[Scale1],WP.BulkCutoff);
      SaveWPFlag = Execute;
      RefreshWPScreen_No = SCREEN_RECIPE1;
   }  
}
 
 
 /*------------------------------------------------------------------------- 
--  Procedure Name    : CutoffCorrection
--  Created by        : Steve Santerre
--  Last modified on  : 2016/02
--  
--  Purpose           : This function is called to Calculate Cutoff
--                      based on desired correction
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--                      
--------------------------------------------------------------------------*/
void CutoffCorrection(void)
{
    float tempo,tempo1,tempo2,min,max; // we could try tempo3%8.dp 
    float tempo3;
 
   if(!MP.CorrOnAverage) tempo1 = (TargetWeight[Scale1] - CorrWeight[Scale1]);
   else 
   {
      SumCheckedWeight[Scale1] = SumCheckedWeight[Scale1] + CorrWeight[Scale1];
      NbrCheckedWeight[Scale1] ++;
      AvgCheckedWeight[Scale1] = SumCheckedWeight[Scale1] / NbrCheckedWeight[Scale1];
      tempo1 = 0;
     
      if(LastCheckedWeight[Scale1]) 
      {
         tempo1 = TargetWeight[Scale1] - AvgCheckedWeight[Scale1];
         AvgCheckedWeight[Scale1] = 0;
         SumCheckedWeight[Scale1] = 0;
         NbrCheckedWeight[Scale1] = 0;
         LastCheckedWeight[Scale1] = OFF; 
      }
   }
   tempo2 = (MP.CutoffCorr/100);
   tempo =  tempo1 * tempo2;
   min = (((100.0 - MP.CutoffCorrLimit)/100)* WP.NominalWeight);
   max = (((100.0 + MP.CutoffCorrLimit)/100)* WP.NominalWeight);
   tempo3 = Lim(min, max,(WP.Cutoff+tempo));
   CutoffCorrOffset[Scale1]= (WP.Cutoff+CutoffCorrOffset[Scale1]) - tempo3;
   RefreshWPScreen_No = SCREEN_RECIPE1;
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : Var_Init_Cycle
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08/28
--  
--  Purpose           : Variables to be Initialize Each Weighing Cycle
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void VarInitCycle(void)
{

     CheckOIML();
   

   /*
clrtare(Scale1Adm);*/

   ZeroFreqVal = MP.ZeroFreq * MP.SampleFreq;
   if (ZeroFreqCnt >= ZeroFreqVal) ZeroFreqCnt=0;
   if (SampleFreqCnt >= MP.SampleFreq) SampleFreqCnt=0;

   T_PRE[T_StabTimeLimit]=Lim(0.01,20,MP.StabTimeLimit);
   T_PRE[T_IntermBlankT] = MC.DribBlankT;
   T_PRE[T_DribbleBlankT] = MC.DribBlankT;
   T_PRE[T_TopUpONTime]=WP.TopUpONTime;
   T_PRE[T_TopUpOFFTime]=WP.TopUpOFFTime;
   T_PRE[T_OutOfTolTime]=MP.OutOfTolTime;
   T_PRE[T_CatchGateMaxTime]=MP.CatchGateMaxTime;
   T_PRE[T_SlowCycleTime] = MP.SlowCycleTime;
   T_PRE[T_DischReqDebouncing] = MC.DischReqDebouncing;
   T_PRE[T_EvacuationTime] = WP.EvacuationTime;
   T_PRE[T_BagDetectionTime] = MP.BagDetectionTime;
   T_PRE[T_KnockingTime] = WP.KnockingTime;
   T_PRE[T_CatchGateDelay] = WP.CatchGateDelay;
   T_PRE[T_RefillingTimeT] = MC.RefillingTimeT;
   T_PRE[T_LowLevelDebounce] = MC.LowLevelDebounceT;
   T_PRE[T_EvacuationDelay] = WP.EvacuationDelay;
   T_PRE[T_LanceBlowOffTime] = WP.LanceBlowOffTime;
   T_PRE[T_KickerCenterTime] = WP.KickerCenterTime;
   T_PRE[T_KickerActivateTime] = WP.KickerActivateTime;
   T_PRE[T_LockingDeviceTime] = MP.LockingDeviceTime;
   T_PRE[T_FeedDelay] = WP.FeedDelayTime;
   T_PRE[T_PostFillingTime] = WP.PostFillingTime;
   T_PRE[T_DelayedBagRelease] = WP.BagReleaseDelay;
   T_PRE[T_AspirationTime] = WP.AspirationTime;
   T_PRE[T_ExternalBagCounter] = EXTERNAL_BAG_COUNTER_ON_TIME;
   T_PRE[T_VibrationPeriod] = WP.VibrationPeriod;

   FlexIO_SetTPresetsFlag = Execute;
   Reset_FlexIOFlag = Execute;
   BagInPlace = OFF;
   OutOfTol[Scale1] = OFF;
   if(T_EN[T_DischReqiQSim]) T_EnOff(T_DischReqiQSim);
   

   if(LowLevel[Scale1] && WP.FeedAlgoType != FeediQSim) 
   {
      ZeroFreqCnt = 0;
      SampleFreqCnt = 0;
   }
   
   if(MC.FeederType == FeederGHS && (WP.FeedAlgoType == FeedStd || WP.FeedAlgoType == FeediQSim) && WP.VibrationAmplitude > 0 && WP.VibrationPeriod > 0)
   {
      VibrationEnabled = ON;
   }
   else
   {
      VibrationEnabled = OFF;
   }
  
  
   RefreshGrayoutFlag = ON;
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : Var_Init_OFF_Mode
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08/28
--  
--  Purpose           : Variables to be Initialize In Off Mode
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void VarInitOff(void)
{
   int i,j;
   ZeroFreqCnt = 0;
   SampleFreqCnt = 0;
   CleanoutWeighment = OFF;
   IsCleanoutDone = OFF;
   LastCheckedWeight[Scale1] = OFF;
   AvgCheckedWeight[Scale1] = 0;
   SumCheckedWeight[Scale1] = 0;
   NbrCheckedWeight[Scale1] = 0;
   ProdCycleUnitsPerMin[Scale1] = 0;
   FirstWeighment=ON;
   KnockHammerFlag=OFF;
   iQFeedWaitSettled = OFF;

   OutReset();
   if(Fault_Type[FLT_GEN] == 0) FaultResetReq();
  
   for(I=1;I<TMAX;I++) T_EnOff(I);
   
   if(ScaleStatus[Scale1]!= 11 && ScaleStatus[Scale1]!= 14 && ScaleStatus[Scale1] < 24)
    {                      
      ClearComparators();
    }
     
#ifndef __SIMULATION__
   if(WP.FeedAlgoType == FeediQSim)
   { 
      iQSimOpen(SCALE_CHANNEL, SCALE_NUMBER,"127.0.0.1");
   }
   else
   {
      iQSimClose(SCALE_CHANNEL, SCALE_NUMBER);
   }
#endif
   
   if(WP.FeedAlgoType != FeedStd && WP.FeedAlgoType != FeediQSim)
   {
      FeedingStop();
      GetFillingData(ON);
   }
   InitiQSmart = ON;
   InitiQFlash = ON;
   InitPLoop   = ON;

   
   if(SeqModeManager == stp_OFF_MODE)
   {
      SeqMaster=stp_INIT;
      SeqFeeder=stp_INIT;
      SeqFeedSTD=stp_INIT;
      Hopper_State = HopperFill;
      Discharge_State = DischOff;
      CheckWeight_State = ReleaseWeight;
      SeqLockingDevice = stp_INIT;
   }
   
   T_PRE[T_BagRelease]=0.50;
   T_PRE[T_BulkDuration]=99.99;
   T_PRE[T_DribbleDuration]=99.99;
   T_PRE[T_CycleDuration]=999.99;
   T_PRE[T_StreamRegHigh]=0.7;
   T_PRE[T_WPMDuration]= 6000.0;
   T_PRE[T_FaultDisplay] = 3.0;
   T_PRE[T_ActuatorON] = 0.3;
   T_PRE[T_ActuatorOFF] = 0.15;
}

 
/*------------------------------------------------------------------------- 
--  Procedure Name    : Low Level
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08/28
--  
--  Purpose           : Variables to Set Low Level
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void LowLevelStatus(void)
{
  bool LowLevelMem[2];
   if(!IOTestFlag)
   {
      if(!Input_State(I_LowLevelSensor))
      { 
         //LowLevel is ON when Input is OFF                            
         if(MC.LowLevelDebounceT>0 && !T_EN[T_LowLevelDebounce]) T_EnOn(T_LowLevelDebounce);
         if(MC.LowLevelDebounceT<=0 ||T_Dn(T_LowLevelDebounce))  
         {
            LowLevel[Scale1]=ON;
            Output_ON(O_LowLevel);
         } 
      } 
      else 
      {    
         I_LowLevelSensorMEM = ON;
         LowLevel[Scale1]=OFF;
         Output_OFF(O_LowLevel);
         if(T_EN[T_LowLevelDebounce]) T_EnOff(T_LowLevelDebounce);
      }
      
    if(LowLevelMem[Scale1] != LowLevel[Scale1])
    {
     ButtonDisplayRefreshed[SCREEN_AUTOMATIC]= OFF;
     LowLevelMem[Scale1] = LowLevel[Scale1];
    }  

   }
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : Fault Monitor
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08/28
--  
--  Purpose           : Variables to Set Low Level
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void FaultMonitor(void)
{
  bool O_CatchGateMem,CatchGateAlreadyOpen;
     
   if(T_Dn(T_FirstScan))
   {
      //E-Stop Fault
      if (IO_Enable[I_EStop] && !Input_State(I_EStop) && !FaultState(FltG_EmergencyStop))
      {
         FaultHandler(FltG_EmergencyStop,"");
      }
            
      //Low Air pressure Fault
      if (IO_Enable[I_PressureSW] && !Input_State(I_PressureSW) && !FaultState(FltG_LowAirPressure))
      {
         FaultHandler(FltG_LowAirPressure,"");
      }
      
      //Catch Gate Not Open Fault
     if(IO_Enable[I_CatchGateOpen])
    {
     
     if(IO_State[O_CatchGate])
     { 
      if(!Input_State(I_CatchGateOpen) || CatchGateAlreadyOpen)
       {
       if (!T_EN[T_CatchGateMaxTime])
        {
         T_EnOn(T_CatchGateMaxTime);
        }
       
       if(!FaultState(FltG_CatchGateNotOpen) && (T_Dn(T_CatchGateMaxTime) || CatchGateAlreadyOpen)) 
        {
         FaultHandler(FltG_CatchGateNotOpen,"");
         T_EnOff(T_CatchGateMaxTime);
        }
       }
     }
      else  T_EnOff(T_CatchGateMaxTime);     
    } 


      //Exceeded Capacity Fault
      if(SeqModeManager == stp_OFF_MODE || SeqModeManager == stp_MANUAL_MODE)
      {
         if((NetWeight[Scale1] > MaxWeight[Scale1]) && !FaultState(FltG_ExceededCapacity))
         {
            FaultHandler(FltB_ExceededCapacity,"");
         }
      }
      else if((NetWeight[Scale1] > MaxWeight[Scale1]) && !FaultState(FltG_ExceededCapacity))
      {
         FaultHandler(FltG_ExceededCapacity,"");
      }
   
      //Motor Failure Fault   
      if (IO_Enable[I_MotorFailure] && !Input_State(I_MotorFailure) && !FaultState(FltG_MotorFailure))
      {
         FaultHandler(FltG_MotorFailure,"");
      }
      
      //Slow cycle Fault
      if((SeqMaster == stp_FEED && !CycleInHoldMem && OK4IntermFeed && OK4DribbleFeed)|| SeqMaster == stp_TOPUP)
      {
         if (!FaultState(FltW_SlowCycle))
         {
            if(T_PRE[T_SlowCycleTime]!= 0 && !T_EN[T_SlowCycleTime]) 
            {
               T_EnOn(T_SlowCycleTime);
            }
            if((T_Dn(T_SlowCycleTime) || T_PRE[T_SlowCycleTime] == 0)) 
            {        
               FaultHandler(FltW_SlowCycle,"");
               T_EnOff(T_SlowCycleTime);
               SlowCycleOccured = ON;
               if(EmptyHopper && LowLevel[Scale1])
               {
                  IsCleanoutDone = ON;
                  Output_ON(O_CleanoutCompleted);
               }
            }
         }
         else if(!SlowCycleMem) 
         {
            SlowCycleMem=ON;
            CheckOnce = ON;
            Discharge_State = DischOn;
         }
      }         
      else
      { 
         if(FaultState(FltW_SlowCycle)||T_EN[T_SlowCycleTime])
         {
            if(!Fault_Type[FLT_GEN] && !Fault_Type[FLT_BLK] && !Fault_Type[FLT_STOP]) 
            {
               T_EnOff(T_SlowCycleTime);
               FaultResetReq();
            }
         }
        
         if(SlowCycleMem) 
         {
            SlowCycleMem=OFF;
         } 
      }
      
      // Bag lost fault
      if(!Input_State(I_DischReq) && BagInPlace && !FaultState(FltG_BagLost))
      {
         FaultHandler(FltG_BagLost,"");
      }
      
      //Clear Fault Input
      if(IO_Enable[I_ClearFaults] && Input_State(I_ClearFaults) && !I_ClearFaultsMem)
      {
         I_ClearFaultsMem = ON;
         if(Fault_Type[FLT_GEN]||Fault_Type[FLT_BLK]||Fault_Type[FLT_STOP]||Fault_Type[FLT_WRN])FaultResetReq();
         if(Discharge_State == DischUnder || Discharge_State == DischOver) DischargeHMI=ON;
      }
       
      if(IO_Enable[I_ClearFaults] && !Input_State(I_ClearFaults) && I_ClearFaultsMem) I_ClearFaultsMem =OFF;                    
   
     
      if(Fault_Type[FLT_GEN] && OutResetFlag != Execute) OutResetFlag = Execute;
   }         
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : WeighingDataRefresh
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08/25
--  
--  Purpose           : Production Report Values Refresh
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void WeighingDataRefresh(void)
{
   double ValueToSqrt;
   bool ValueNegFlag;
   string weighingdata_weight[12];

   if(WeightSampled[Scale1] && !LowLevel[Scale1] && !WeighmentAborted && SqareSumWeight[Scale1]<2000000000)
   {
      NbrChecked[Scale1]++; 
      SumWeight[Scale1] = SumWeight[Scale1] + CheckedWeight[Scale1];
      AvgWeight[Scale1] = SumWeight[Scale1]/NbrChecked[Scale1];
      SqareSumWeight[Scale1] = SqareSumWeight[Scale1] + (CheckedWeight[Scale1]*CheckedWeight[Scale1]);
   
      //Excel StdevP Formula (entire population)
      ValueToSqrt = (SqareSumWeight[Scale1] - (NbrChecked[Scale1]*(AvgWeight[Scale1]*AvgWeight[Scale1])))/NbrChecked[Scale1];
      StdDev[Scale1] = sqrt(ValueToSqrt);
   }
   
      if((NbrChecked[Scale1] > 2 && (AvgWeight[Scale1] < (WP.NominalWeight*0.75)) || (AvgWeight[Scale1] > (WP.NominalWeight*1.25))) || NbrChecked[Scale1] < 0)
       {
        ResetData(ON);
       }    

   ProdLastTolerance[Scale1] = "=";
   
   if(ProdUnderWeightFlag[Scale1])
   {
      ProdUnderWeightCnt[Scale1]++;
      if(Batch.Preset > 0)
         Batch.UnderWeightCnt++;
      ProdLastTolerance[Scale1] = "-";
      ProdUnderWeightFlag[Scale1]=OFF;
   }
   
   if(ProdOverWeightFlag[Scale1])
   {
      ProdOverWeightCnt[Scale1]++;
      if(Batch.Preset > 0)
         Batch.OverWeightCnt++;
      ProdLastTolerance[Scale1] = "+";
      ProdOverWeightFlag[Scale1]=OFF;
   }
   
   
   ProdTotalUnitsCnt[Scale1]++;
   if(Batch.Preset > 0)
   {
      Batch.UnitsDone++;
      Batch.Count++;
      if(BatchEndReq) 
      {
         BatchCancelled = ON;
      }
   }

   if(WP.CountAll)
   { 
      ProdAcceptUnitCnt[Scale1] =  ProdTotalUnitsCnt[Scale1];
      if(Batch.Preset > 0)
         Batch.UnitsAccepted = Batch.UnitsDone;
   } 
   else
   {           
      ProdAcceptUnitCnt[Scale1] =  ProdTotalUnitsCnt[Scale1] - (ProdUnderWeightCnt[Scale1] + ProdOverWeightCnt[Scale1]);
      if(Batch.Preset > 0)
         Batch.UnitsAccepted = Batch.UnitsDone - (Batch.UnderWeightCnt + Batch.OverWeightCnt);
   } 
   
   ProdLastChecked[Scale1] = CheckedWeight[Scale1];     
   
   if(Batch.Preset > 0)
   { 
      if(CheckedWeight[Scale1] < Batch.MinAccepted || Batch.MinAccepted == 0) 
         Batch.MinAccepted = CheckedWeight[Scale1];
      
      if(CheckedWeight[Scale1] > Batch.MaxAccepted || Batch.MaxAccepted == 0) 
         Batch.MaxAccepted = CheckedWeight[Scale1];
   }  
   
   if(Batch.Preset > 0 && WeighmentAborted) 
      Batch.Residue = Batch.Residue + CheckedWeight[Scale1];
   
   ProdStdDev[Scale1] = StdDev[Scale1];
   
   if(Batch.Preset > 0)
      Batch.StdDev = StdDev[Scale1];
   
   ProdAvgWeight[Scale1] = AvgWeight[Scale1];
   
   if(Batch.Preset > 0)
      Batch.Avg = AvgWeight[Scale1];
   
   ProdTotalWeighmentsLT[Scale1]++;
   ProdTotalUnitsLT[Scale1]++;
   
   //Correction of Cutoffs values
   if((WP.FeedAlgoType == FeedStd || WP.FeedAlgoType == FeedPLoop) && !WeighmentAborted && !SlowCycleOccured && !LowLevel[Scale1])
   {
    if(WeightSampled[Scale1])
    {
     if(MP.CutoffCorr > 0 && CorrWeight[Scale1] > (WP.Cutoff-CutoffCorrOffset[Scale1])) 
      {
       CutoffCorrection();
      } 
    }  
    else if(WP.DFTime > 0 && MP.DFWeightCorr > 0 && !BulkOnTime[Scale1])
    {
     BulkCutoffCorrection();
    }
   }
   
   if(WeightSampled[Scale1])
   {
      ProdTotalWeight[Scale1] = ProdTotalWeight[Scale1] + CheckedWeight[Scale1];
      if(Batch.Preset > 0)Batch.TotalWeight = Batch.TotalWeight + CheckedWeight[Scale1];
      ProdTotalWeightLT[Scale1] = ProdTotalWeightLT[Scale1] + CheckedWeight[Scale1];
      ProdLastWeight[Scale1] = ProdLastChecked[Scale1];
      ProdLastSampled[Scale1] = ">o<";
      weighingdata_weight = concat("c",adj(ProdLastWeight[Scale1],6,dp),WP.Unit);
   }
   else
   {
      ProdTotalWeight[Scale1] = ProdTotalWeight[Scale1] + TargetWeight[Scale1];
      if(Batch.Preset > 0)Batch.TotalWeight = Batch.TotalWeight + TargetWeight[Scale1];
      ProdTotalWeightLT[Scale1] = ProdTotalWeightLT[Scale1] + TargetWeight[Scale1];
      ProdLastWeight[Scale1] = TargetWeight[Scale1];
      ProdLastSampled[Scale1] = "---";
      weighingdata_weight = concat("u",adj(ProdLastWeight[Scale1],6,dp),WP.Unit);
   }
    
   if(WP.FeedAlgoType == FeediQSmart)
   {
      WP.BulkCutoff = FastCutoff;
      WP.Cutoff = SlowCutoff;
      RefreshWPScreen_No = SCREEN_RECIPE1;
   } 
   
   if(WP.FeedAlgoType == FeediQFlash)
   {
      WP.BulkCutoff = TargetWeight[Scale1]-FastCutoff;
      WP.Cutoff = 0;
      RefreshWPScreen_No = SCREEN_RECIPE1;
   }
   
   WeighingData_StringSource[Scale1] = concat(trim(ProdBulkDuration[Scale1]),"s",space(tab),trim(ProdDribbleDuration[Scale1]),"s",
                                              space(tab-3),weighingdata_weight,space(tab-1),TimeString);                                         
   RefreshWeighingDataPopup(1);
   
   WeightSampled[Scale1] = OFF;
   
   //Formating Unit Record String (Ticket Info)
       FileName_Date = getstddate();
       Standard_Date = concat("20",FileName_Date);
       
       Standard_Time = getstdtime();
       TicketInfo =  concat("<;", MC.SerialNbr, ";",trim(string(adj(TargetWeight[Scale1],7,2))), ";",WP.Unit, ";",
                           trim(string(adj(ProdLastWeight[Scale1],7,2))), ";",ProdLastSampled[Scale1], ";",                            
                           trim(string(adj(WP.UnderWeight,10,2))), ";",ProdLastTolerance[Scale1], ";",
                           trim(string(adj(WP.OverWeight,10,2))), ";",                                  
                           trim(string(adj(ProdTotalUnitsCnt[Scale1],10,0))), ";",Standard_Time, ";>", CRLF);
       
       fifo_write (FIFO_UNITREC_SHARED, TicketInfo, length (TicketInfo));
       fifo_write (FIFO_UNITREC_USB,    TicketInfo, length (TicketInfo));  
        
   RefreshReportScreenReq();
   RefreshAutoScreenReq();
   
   if(Batch.Preset > 0)
   {
      SaveBatchFlag = Execute;
      RefreshBatchScreenReq();
   } 
   
   PreSave_DataNoResetFlag = Execute;
   ProdDataReady = ON;
   ProdDataReadyPLC = ON;
   if(SlowCycleOccured)SlowCycleOccured = OFF;
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : IN_Refresh
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08/25
--  
--  Purpose           : Refresh group of physical Digital Inputs and 
--                      sets the IO_State 
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void IN_Refresh(void)
{
   int i;
   int InputRegTest1,InputRegTest2;

   if(MaxNumbOfIO > 31)
   {
      InputRegTest1 = pin(31,0); 
      InputRegTest2 = pin(MaxNumbOfIO,32);
   }
   else 
   {
      if(MaxNumbOfIO > 0 && MaxNumbOfIO <= 31)InputRegTest1 = pin(MaxNumbOfIO,0);
      InputRegTest2 = 0;
   }

   for(i=IO_IN_MIN;i<=IO_IN_MAX;i++)
   {
      if(i == 1)
      {
         InputRegTest2 = 0;
      }
      if(IO_Input[i]) //Inputs
      { 
         if (!IO_Enable[i]) IO_TestState[i] = OFF;
         else if(IO_Address[i] <=31) IO_TestState[i] = bit(IO_Address[i],InputRegTest1);
         else IO_TestState[i] = bit(IO_Address[i],InputRegTest2);
      }
   }
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : IN_Refresh
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08/25
--  
--  Purpose           : Refresh group of physical Digital Inputs
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void IN_Refresh2(void)
{
   if(MaxNumbOfIO > 31)
   {
      InputReg1 = pin(31,0); 
      InputReg2 = pin(MaxNumbOfIO,32);
   }
   else 
   {
      if(MaxNumbOfIO > 0 && MaxNumbOfIO <= 31)InputReg1 = pin(MaxNumbOfIO,0);
      InputReg2 = 0;
   }
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : Force_Out
--  Created by        : Jean-Pierre Doré
--  Last modified on  : 2015/08/25
--  
--  Purpose           : Outputs set to OFF when accessing IO Test Output page
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      :  
--                      
--------------------------------------------------------------------------*/
void Force_Out (void)
{
   int i;
   for(i=37;i<=MaxArrayIODbase;i++)
   {
      if(IO_Enable[i] && IO_Address[i] <= MaxNumbOfIO)
      {
         if (IO_Force[i])  bout((IO_Address[(i)]),1);      
         if (!IO_Force[i]) bout((IO_Address[(i)]),0);  
         IO_TestState[i]=out((IO_Address[(i)]));
      }
   }  
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : OUT_Refresh
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08/25
--  
--  Purpose           : Refresh physical Digital Outputs
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : Feeder Outputs will also be refreshed with 
--                      Feeding Instructions - RTCWin
--------------------------------------------------------------------------*/
void OUT_Refresh(void)
{
   bool Bit31;
   int i;
   int OutputRegTest1,OutputRegTest2;
   
   OutputRegTest1 = 0;
   OutputRegTest2 = 0;

   for(i=IO_OUT_MIN;i<=IO_OUT_MAX;i++)
   {
      if(!IO_Input[i]) //Outputs
      { 
         if (!IO_Enable[i]) IO_TestState[i] = OFF;
         else
         { 
            if (IO_Address[i]<=31)
            {
               if(IO_TestState[i])
               {
                  setbit(IO_Address[i],OutputRegTest1);
               }
               else clrbit(IO_Address[i],OutputRegTest1);
            }
            else
            {
               if(IO_TestState[i])
               {
                  setbit(IO_Address[i],OutputRegTest2);
               }
               else clrbit(IO_Address[i],OutputRegTest2);
            }
         }
      }
   }
   if(MaxNumbOfIO > 31) 
   {
      pout(OutputRegTest1,31,0);
      pout(OutputRegTest2,MaxNumbOfIO,32);
      Bit31 = bit(31,OutputRegTest1);
      bout(31,Bit31);
      if(MaxNumbOfIO == 63)
      {
         Bit31 = bit(31,OutputRegTest2);
         bout(63,Bit31);
      }
   }
   if(MaxNumbOfIO > 0 && MaxNumbOfIO <= 31) pout(OutputRegTest1,MaxNumbOfIO,0);
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : OUT_Refresh
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08/25
--  
--  Purpose           : Refresh physical Digital Outputs
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : Feeder Outputs will also be refreshed with 
--                      Feeding Instructions - RTCWin
--------------------------------------------------------------------------*/
void OUT_Refresh2(void)
{
   bool Bit31;
   //int TempOutputs,TempOutputs2;
   
   //OutputReg1 = 0;
   //OutputReg2 = 0;

   if(MaxNumbOfIO > 31) 
   {
      pout(OutputReg1,31,0);
      pout(OutputReg2,MaxNumbOfIO,32);
      Bit31 = bit(31,OutputReg1);
      bout(31,Bit31);
      if(MaxNumbOfIO == 63)
      {
         Bit31 = bit(31,OutputReg2);
         bout(63,Bit31);
      }
   }
   if(MaxNumbOfIO > 0 && MaxNumbOfIO <= 31) pout(OutputReg1,MaxNumbOfIO,0);
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : RefreshFeederOutputs
--  Created by        : Steve Santerre
--  Last modified on  : 2016/06/08
--  
--  Purpose           : Refresh Feeders Outputs array value
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : Feeder Outputs are controlled via 
                        Feeding Instructions - RTCWin
                        Their corresponding array need to be kept updated
--------------------------------------------------------------------------*/
void RefreshFeederOutputs(void)
{
   IO_State[O_DribbleFeed] = out(IO_Address[O_DribbleFeed]);
   if(IO_Enable[O_IntermFeed])       IO_State[O_IntermFeed] = out(IO_Address[O_IntermFeed]);
   if(IO_Enable[O_BulkFeed])         IO_State[O_BulkFeed] = out(IO_Address[O_BulkFeed]);
   if(IO_Enable[O_CatchGate])        IO_State[O_CatchGate] = out(IO_Address[O_CatchGate]);
   if(IO_Enable[O_CatchGateInterm]) IO_State[O_CatchGateInterm] = out(IO_Address[O_CatchGateInterm]);
}

/*------------------------------------------------------------------------------- 
--  Procedure Name    : SEQ_ModeManager
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08
--  
--  Purpose           : Mode Manager Sequence
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------------*/

void SEQ_ModeManager (void)
{
   int i;
   bool SampledInPauseMem;
   
   if(IO_Enable[I_Start] && !Input_State(I_Start)) 
     I_StartMem = OFF;
   if(IO_Enable[I_Stop] && !Input_State(I_Stop)) 
     I_StopMem = OFF;
   if(IO_Enable[I_ManDisch]&& !Input_State(I_ManDisch))
     I_ManDischMem = OFF;
   if(Fault_Type[FLT_GEN]!=0 || OffHMI) 
     {SeqModeManager = stp_OFF_MODE;OffHMI=OFF;}
   if((Fault_Type[FLT_GEN]!=0 ||(SeqModeManager!= stp_OFF_MODE && SeqModeManager!= stp_STOP_MODE && !CycleInHoldMem)) && StartHMI) 
     StartHMI=OFF;
   if((SeqModeManager!= stp_AUTO_MODE && SeqModeManager != stp_MANUAL_MODE) && StopHMI) 
     StopHMI=OFF;
   
   if(PauseHMI)
   {
    AutoPause = !AutoPause;
    PauseHMI = OFF;
    CurMode_Set();
   }
   
   if(SampledInPauseMem != SampledInPause)
   {
    if(SampledInPause)
     Output_ON(O_CycleInHold);
    else
     Output_OFF(O_CycleInHold); 
    SampledInPauseMem = SampledInPause;
   }
    
   SeqModeManagerNext = SeqModeManager;
   switch (SeqModeManager)
   {
   case stp_OFF_MODE:
      if(SeqModeManagerMEM!=SeqModeManager)
      {
         SeqModeManagerMEM = SeqModeManager;
         Start_PauseBtn = StartBtn;
         AutoPause = OFF;
         SampledInPause = OFF;
         KeyLockPTSZI(LAST,LAST,LAST,OFF,LAST);
         VarInitOffFlag = Execute;
         VarInitCycle();
         ChangeSMStateFlag = Execute;
         DischReqCheck();
      }
              
      if(Fault_Type[FLT_GEN]==0 && VarInitOffFlag != Execute)
      {    
         if(StepMsg!=lsub(TX_MSGWAITINGFORSTART) && T_Dn(T_FirstScan)) 
         {
            MessageDisplay(lsub(TX_MSGWAITINGFORSTART),Step_Message);
         }
         
         if(MC.CFGBagHolder && ((Input_State(I_EmptyBagReady) || Input_State(I_DischReq) && WP.FeedAlgoType != FeediQSim)))
         {
            // Manual bagger, a bag is still in the spout.
            // Allow the user to remove it manually
            Discharge_State = DischMan;
            
            if(DischargeHMI)
            {
               Output_OFF(O_BagHolder);
               Output_ON(O_BagRelease);
               Discharge_State = DischOff;
            }
         }
         else
         {
            Discharge_State = DischOff;
         }
         
         if((StartHMI || (IO_Enable[I_Start] && Input_State(I_Start) && !I_StartMem)) && T_Dn(T_FirstScan))
         {
            // Start was pressed on HMI, or Start input ON
            if(Current_Screen != SCREEN_AUTOMATIC)
            {
               Force_Screen_Req(SCREEN_AUTOMATIC, LARGE);
            }
            I_StartMem = ON;
            StartHMI=OFF;
                     
            if(Check_Scale_Cfg())
            {
               SeqModeManager = stp_PREP_MODE;
               MessageDisplay(lsub(TX_MSGPREPARATION),Step_Message);
               if(BatchEnded)
               {
                  Batch.Count = 0;
                  BatchEnded = OFF;
                  RefreshAutoScreenReq();
               }
            } 
         }
      }
      break;
                 
   case stp_STOP_MODE:
      if(SeqModeManagerMEM!=SeqModeManager)
      {
         SeqModeManagerMEM = SeqModeManager;
         Start_PauseBtn = StartBtn;
         KeyLockPTSZI(LAST,LAST,LAST,OFF,LAST);
         Hopper_State = HopperFill;
         
         if(CleanoutWeighment)
         {
          if(Batch.Preset > 0)
          {
           BatchEnded = ON;
           Output_ON(O_BatchEnded);
          } 
          CleanoutWeighment = OFF;
         } 
         
         Output_OFF(O_CycleInProgress);
         
         if(IO_Enable[O_ClosingCnv])
         {
            Output_OFF(O_ClosingCnv);
         }
         if(IO_Enable[O_DischargeDoor])
         {
            Output_OFF(O_DischargeDoor);
         }
         if(BatchEnded) 
         {
            MessageDisplay(lsub(TX_MSGENDOFBATCH),Step_Message);
         }
         else
         {
            MessageDisplay(lsub(TX_MSGWAITINGFORSTART),Step_Message);
         }
      }
      if(MC.CFGBagHolder && ((Input_State(I_EmptyBagReady) || Input_State(I_DischReq)&& WP.FeedAlgoType != FeediQSim)))
      {
         // Manual bagger, a bag is still in the spout.
         // Allow the user to remove it manually
         Discharge_State = DischMan;
         
         if(DischargeHMI)
         {
            Output_OFF(O_BagHolder);
            Output_ON(O_BagRelease);
            Discharge_State = DischOff;
         }
      }
      else
      {
         Discharge_State = DischOff;
      }
      
      if(StartHMI || (IO_Enable[I_Start] && Input_State(I_Start) && !I_StartMem))
      {
         // Start pressed or start input on
         if(Current_Screen != SCREEN_AUTOMATIC)
         {
            Force_Screen_Req(SCREEN_AUTOMATIC, LARGE);
         }
         I_StartMem = ON;
         StartHMI=OFF;
         SeqModeManager = stp_AUTO_MODE;
         Output_ON(O_CycleInProgress);
         if(BatchEnded)
         {
            Batch.Count = 0;
            BatchEnded = OFF;
            Output_OFF(O_BatchEnded);
            RefreshAutoScreenReq();
         } 
      }                            
      break;

   case stp_STOPPING_MODE:
      if(SeqModeManagerMEM != SeqModeManager)
      {
         SeqModeManagerMEM = SeqModeManager;
      }
      if(SeqMaster==stp_INIT && SeqKicker==stp_INIT)
      {
         SeqModeManager = stp_STOP_MODE;
      }
      break;
                            
   case stp_PREP_MODE:
      if(SeqModeManagerMEM != SeqModeManager)
      {
         SeqModeManagerMEM = SeqModeManager;
         VarInitOffFlag = Execute;
         ChangeSMStateFlag = Execute;
         CheckOutput = Execute;
         ResetLance = ON;
         RemoveLockingDevice = ON;
         ReleaseAcknowledge = ON;
         ForceZero = OFF;
         BagInPlace = OFF;
      }
         
      if(Fault_Type[FLT_STOP]==0)
      {
         if(CheckLowLevel())
         {
            VarInitCycle();
            if(VarInitOffFlag != Execute)
            {
               if(CheckOutputConfig())
               if(Actuator())
               if(KickerCheck())
               if(LanceCheck())
               if(LockingDeviceCheck())
               {
                  Output_ON(O_CycleInProgress);
                  SeqModeManager = stp_AUTO_MODE;
               }
            } 
         }
      }
      break;
             
   case stp_AUTO_MODE:
      if(SeqModeManagerMEM!=SeqModeManager)
      {
         SeqModeManagerMEM = SeqModeManager;
         Start_PauseBtn = PauseBtn;
         KeyLockPTSZI(LAST,LAST,LAST,ON,LAST);
         if(IO_Enable[O_ClosingCnv]) 
         {
            Output_ON(O_ClosingCnv);
         }
         if(IO_Enable[O_DischargeDoor])
         {
            Output_ON(O_DischargeDoor);
         }
      } 
          
      if(Fault_Type[FLT_STOP]!=0)
      {
         SeqModeManager = stp_STOPPING_MODE;
      }
      if(StopHMI || (IO_Enable[I_Stop] && Input_State(I_Stop) && !I_StopMem) || BatchEnded || CleanoutWeighment)
      {  
         SeqModeManager = stp_STOPPING_MODE;
         I_StopMem = ON;
         StopHMI=OFF;
      }
      break;    
   }

   if(SeqModeManager != mode_step || Current_Screen != Current_ScreenMEM)
   {
      RefreshGrayoutFlag = ON;
      
      if(SeqModeManager == stp_OFF_MODE)
      {
         if (SeqModeManager!= mode_step)
         {
            ScanMax = FillingTaskIntervalOff / 1000.0;
         }
         if(Current_Screen != Current_ScreenMEM && Current_Screen == SCREEN_AUTOMATIC)
         {
            OutResetFlag = Execute;
         }
      }
      else 
      {
         if(SeqModeManager!= mode_step)
         {
            ScanMax = FillingTaskIntervalAuto / 1000.0;
         }
      }   
      
      if(SeqModeManager!= mode_step) 
      {
         mode_step = SeqModeManager; 
         Clear_ButtonControlRefreshedFlag = ON;
      }
      Current_ScreenMEM = Current_Screen;
      CurMode_Set();
   }
   
   if( mode_step_MEM != mode_step)
   {
      CurMode_Set();          
   }        
}

/*------------------------------------------------------------------------------- 
--  Procedure Name    : SEQ_Master
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08
--  
--  Purpose           : Weigher Master Sequence
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------------*/

void SEQ_Master (void)
{
   bool MsgDownStreamSent,MsgWaitPauseSent,MsgBagReleaseDelaySent,EmptyBagOffMsg,BagNotDetectedMem;
   if(SeqModeManager == stp_OFF_MODE) SeqMaster = stp_INIT;
   if(IO_Enable[I_ResetOutofTolAlarm]&&!Input_State(I_ResetOutofTolAlarm))I_ResetOutofTolAlarmMem=OFF;
   if(IO_Enable[I_BatchEnd] && Input_State(I_BatchEnd) && AutoMode && Batch.Preset > 0) BatchEndReq = ON;
   
   if(IO_Enable[O_ExternalBagCounter] && T_Dn(T_ExternalBagCounter))
   {
      Output_OFF(O_ExternalBagCounter);
      T_EnOff(T_ExternalBagCounter);
   }

   SeqMasterNext = SeqMaster;
   switch (SeqMaster)
   {
   case stp_INIT:
      if(SeqMasterMEM != SeqMaster)
      {
         SeqMasterMEM = SeqMaster;
         if(SeqModeManager != stp_OFF_MODE) 
         {
            MessageDisplay(lsub(TX_MSGBLANK),Step_Message);
         }
         if(FeedInProcess) 
         {
            WeighmentAborted = ON;
         }
         TopUp = OFF;
         VarInitCycle();
         RefreshAutoScreenReq();
         EmptyBagOffMsg = OFF;
         BagNotDetectedMem = OFF;
      }
      if(SeqModeManager == stp_AUTO_MODE)
      {
         if(Batch.Preset > 0 && (Batch.Count >= Batch.Preset || BatchCancelled))
         {
            BatchEnded = ON;
            BatchEndReq = OFF;
            BatchCancelled = OFF;
            Output_ON(O_BatchEnded);
         }
         else
         {
            if(MC.CFGBagHolder)
            {
               // Manual bagger
               if((Input_State(I_EmptyBagReady) || Input_State(I_DischReq)) && WP.FeedAlgoType != FeediQSim)
               {
                  // There is a bag already, we need to get rid of it
                  WeighmentAborted = ON;
                  // We don't know if the feeding cycle was over or not,
                  // need to sample the bag
                  SeqMaster = stp_SAMPLE;
               }
               else
               {
                  Output_OFF(O_BagRelease);
                  Output_OFF(O_BagHolder);
                  Output_OFF(O_BagRequest);
                  if(T_EN[T_WPMDuration])
                  {
                     T_EnOff(T_WPMDuration);
                  }
                  T_EnOn(T_WPMDuration); // Start WPM timer
                  WPMDuration = 0;
                  if(((ZeroFreqCnt == 0 || (WP.FeedAlgoType != FeediQSim && (LowLevel[Scale1] || EmptyHopper))) && !AutoZeroOff) || ForceZero)
                  {
                     // Zero needed
                     if(CheckLowLevel())
                     {
                      SeqMaster = stp_ZERO;
                     } 
                  }
                  else
                  {
                     SeqMaster = stp_APPLYLOCKINGDEVICE;
                  }
               }
            }
            else
            {
               // Automatic bagger
               if(Input_State(I_DischReq)&& WP.FeedAlgoType != FeediQSim)
               {
                  // Discharge request is already ON. There is a bag present
                  // We need to get rid of it before starting the sequence
                  WeighmentAborted = ON;
                  // We don't know if the feeding cycle was over or not,
                  // need to sample the bag
                  SeqMaster = stp_SAMPLE;
               }
               else
               {
                  if(T_EN[T_WPMDuration])
                  {
                     T_EnOff(T_WPMDuration);
                  }
                  WPMDuration = 0;
                  T_EnOn(T_WPMDuration);  // Start WPM Timer
                  if(((ZeroFreqCnt == 0 || (WP.FeedAlgoType != FeediQSim && (LowLevel[Scale1] || EmptyHopper))) && !AutoZeroOff) || ForceZero)
                  {
                     // Zero needed
                     if(CheckLowLevel())
                     {
                      SeqMaster = stp_ZERO;
                     } 
                  }
                  else
                  {
                     SeqMaster = stp_APPLYLOCKINGDEVICE;
                  }
               }
            }
         }
      } 
      break;
            
   case stp_ZERO:
      if(SeqMasterMEM != SeqMaster) 
      {
         SeqMasterMEM = SeqMaster;
         T_EnOff(T_StabTimeLimit);
         if(IO_Enable[O_LockingDevice] && SeqLockingDevice != stp_LDOFF)
         {
            RemoveLockingDevice = ON;
         }
         if(IO_Enable[O_CleanoutCompleted] && IO_State[O_CleanoutCompleted])
         {
            Output_OFF(O_CleanoutCompleted);
         }
         SetZeroStatus[Scale1]= -1;
      }
      if(!IO_Enable[O_LockingDevice] || SeqLockingDevice == stp_LDOFF)
      {  
        if(SetZeroStatus[Scale1] != 0)
        { 
         if(!IsSettled[Scale1])
         {
            if(!T_EN[T_StabTimeLimit] && MP.StabTimeLimit != 0 && !FaultState(FltS_StabTooLong))
            {
               T_EnOn(T_StabTimeLimit);
               MessageDisplay(lsub(TX_MSGSTABILIZATION),Step_Message);
            }
            if(T_Dn(T_StabTimeLimit)) 
            {
               // Timeout, stabilisation too long
               FaultHandler(FltS_StabTooLong,""); 
               T_EnOff(T_StabTimeLimit);
               SeqMaster = stp_INIT;
            }
            break;
         }
         
         else
         {
            
               T_EnOff(T_StabTimeLimit);
               if(FaultState(FltS_StabTooLong))
               {
                  FaultResetReq();
               }
               if(WP.FeedAlgoType == FeediQSim) 
               {
                  iQSimSetWeight(SCALE_CHANNEL, SCALE_NUMBER,0.0);
                  SetZeroStatus[Scale1] = 0;
               } 
               else
               {
                  if(abs(NetWeight[Scale1]) <= (MaxWeight[Scale1] * ZeroLimit)&& SetZeroStatus[Scale1]==-1) 
                  {
                     SetZeroStatus[Scale1] = setzero(Scale1Adm);
                  }
                  else
                  {
                     SetZeroStatus[Scale1] = 99;
                  }
               }
         }
        }    
            switch (SetZeroStatus[Scale1]) 
            {
            case 99:
               if(!FaultState(FltS_ZeroOutOfRange))
               {
                  FaultHandler(FltS_ZeroOutOfRange,"");
                  SeqMaster = stp_INIT;
               }
               SetZeroStatus[Scale1]=-1;
               break;
            
            case 0:
               if(FaultState(FltS_ZeroOutOfRange))
               {
                  FaultResetReq();
               }
               MessageDisplay(lsub(TX_MSGZERODONE),Step_Message);
               if(CheckLowLevel())
               {
                  if(Actuator())
                  SeqMaster = stp_APPLYLOCKINGDEVICE;
               }
               break; 
                                              
            default:
               MessageDisplay(concat(lsub(TX_MSGSYSTEMWARNING),100 + SetZeroStatus[Scale1]),Step_Message);
               SetZeroStatus[Scale1]=-1;
               break;
            }
         }
      
      break; 
      
   case stp_APPLYLOCKINGDEVICE:
      if(SeqMasterMEM != SeqMaster) 
      {
         SeqMasterMEM = SeqMaster;
         if(IO_Enable[O_LockingDevice] && SeqLockingDevice != stp_LDON)
         {
            ApplyLockingDevice = ON;
         }
         else
         {
            if(MC.CFGBagHolder)
            {
               SeqMaster = stp_BAGREQUEST;
            }
            else
            {
               SeqMaster = stp_WAITFORDISCHREQ;
            }
         }
      }
      if(SeqLockingDevice == stp_LDON)
      {
         if(MC.CFGBagHolder)
         {
            SeqMaster = stp_BAGREQUEST;
         }
         else
         {
            SeqMaster = stp_WAITFORDISCHREQ;
         }
      }
      break;
      
   case stp_BAGREQUEST:
      if(SeqMasterMEM != SeqMaster) 
      {
         SeqMasterMEM = SeqMaster;
         Output_OFF(O_BagRelease);
         if(IO_Enable[O_BagRequest])
         {
            Output_ON(O_BagRequest);
         }
         
         if(IO_Enable[I_EmptyBagReady])
         {
            MessageDisplay(lsub(TX_MSGWAITINGFOREMPTYBAG),Step_Message);
            
         }
         else
         {
            // No empty bag ready input, wait for the discharge request signal
            SeqMaster = stp_WAITFORDISCHREQ;
         }
      }
      
      if(Input_State(I_EmptyBagReady))
      {
         Output_ON(O_BagHolder);
         Output_OFF(O_BagRequest);
         SeqMaster = stp_WAITFORDISCHREQ;
         T_EnOff(T_BagDetectionTime);
      }
      else
      {
         if(T_EN[T_WPMDuration])
         {
            // Stop Time here, the wait is not under our control
            WPMDuration += T_Acc(T_WPMDuration);
            T_EnOff(T_WPMDuration);
         }
      }
      
      
      break;
      
   case stp_WAITFORDISCHREQ:
      if(SeqMasterMEM != SeqMaster) 
      {
         SeqMasterMEM = SeqMaster;
         if(WP.FeedAlgoType == FeediQSim) T_EnOn(T_DischReqiQSim);
         if(!Input_State(I_DischReq))
         {
            MessageDisplay(lsub(TX_WAITINGFORDISCHREQ),Step_Message);
         }
         if(IO_Enable[O_BagRequest] && !MC.CFGBagHolder)
         {
            // Bag request on, only if we have an automatic bagger
            Output_ON(O_BagRequest);
         }
      }

      if(Input_State(I_DischReq) && (!MC.CFGBagHolder || !IO_Enable[I_EmptyBagReady] || !Input_State(I_EmptyBagReady)|| T_Dn(T_DischReqiQSim)))
      {
         // Bagger is asking for discharge, bag is in place
         DischReqMem = ON;
         BagInPlace = ON;
         T_EnOff(T_DischReqiQSim);
         Output_ON(O_BagHolder);
         Output_OFF(O_BagRequest);
         SeqMaster = stp_REMOVELOCKINGDEVICE;
         T_EnOn(T_WPMDuration);  // Restart WPM timer
         
         if(FaultState(FltS_BagNotDetected))
         {
            FaultResetReq();
         }
         
         if(WP.FeedAlgoType == FeediQSim) 
         {
            if(WP.FixedBagWeight)
            {
               iQSimSetWeight(SCALE_CHANNEL, SCALE_NUMBER,WP.MaxBagWeight);
            }
            else
            {
               iQSimSetWeight(SCALE_CHANNEL, SCALE_NUMBER,WP.MaxBagWeight / 2);
            }
         }
      }
      else
      {
         if(T_EN[T_WPMDuration])
         {
            // Stop Time here, the wait is not under our control
            WPMDuration += T_Acc(T_WPMDuration);
            T_EnOff(T_WPMDuration);
         }
         
         if(IO_Enable[I_EmptyBagReady] && !Input_State(I_DischReq))
         {
          
          if(BagNotDetectedMem && !FaultState(FltS_BagNotDetected) && !T_EN[T_BagDetectionTime])
          {
            BagNotDetectedMem = OFF;
            SeqMaster = stp_BAGREQUEST;
            break;
          } 
          
          if(MP.BagDetectionTime != 0 && !T_EN[T_BagDetectionTime] && !FaultState(FltS_BagNotDetected))
          {
           T_EnOn(T_BagDetectionTime);
          }
          
          if(T_Dn(T_BagDetectionTime))
          {
           if(!FaultState(FltS_BagNotDetected))
            {
             FaultHandler(FltS_BagNotDetected,"");
             Output_OFF(O_BagHolder);
             Output_ON(O_BagRelease);
            }
           else
            {  
             T_EnOff(T_BagDetectionTime);
             BagNotDetectedMem = ON;
            } 
          }
          
          
         } 
          
         if(Input_State(I_DischReq) && !EmptyBagOffMsg)
          {
           MessageDisplay(concat(lsub(TX_MSGWAITINGFOREMPTYBAG)," (OFF)"),Step_Message);
           EmptyBagOffMsg = ON;
          } 
      }
      break;
      
   case stp_REMOVELOCKINGDEVICE:
      if(SeqMasterMEM != SeqMaster) 
      {
         SeqMasterMEM = SeqMaster;
         if(IO_Enable[O_LockingDevice])
         {
            RemoveLockingDevice = ON;
         }
         else
         {
            SeqMaster = stp_TARE;
         }
      }
      if(SeqLockingDevice == stp_LDOFF)
      {
         SeqMaster = stp_TARE;
      }
      break;
      
   case stp_TARE:
      if(SeqMasterMEM != SeqMaster) 
      {
         SeqMasterMEM = SeqMaster;
         if(!T_EN[T_StabTimeLimit] && MP.StabTimeLimit != 0 && !FaultState(FltS_StabTooLong) && !WP.FixedBagWeight)
         {
            T_EnOn(T_StabTimeLimit);
         }
      }
      if(WP.FixedBagWeight && WP.FeedAlgoType != FeediQSim)
      {
         SetTareStatus[Scale1] = mantare(Scale1Adm,float(WP.MaxBagWeight));
      }
      else
      {
         if(IsSettled[Scale1])
         {
            T_EnOff(T_StabTimeLimit);
            
            if(WP.FeedAlgoType == FeediQSim)
            {
               iQSimSetWeight(SCALE_CHANNEL, SCALE_NUMBER,0.0);
               SetTareStatus[Scale1] = 0;
            }
            else
            {
               if(NetWeight[Scale1] <= WP.MaxBagWeight || WP.MaxBagWeight == 0)
               {
                  // autotare does not work when the weight is at 0
                  if(NetWeight[Scale1] > 0.00)
                  {
                     SetTareStatus[Scale1] = autotare(Scale1Adm);
                     ForceZero = OFF;
                  }
                  else
                  {
                     // Set zero to allow weighment
                     SetTareStatus[Scale1] = setzero(Scale1Adm);
                     // Force zero on next cycle
                     ForceZero = ON; 
                  }
               }
               else
               {
                  // Bag weight over the limit
                  SetTareStatus[Scale1] = 99;
               }
            }
         }
         else
         {
            if(T_Dn(T_StabTimeLimit)) 
            {
               // Timeout, stabilisation too long
               FaultHandler(FltS_StabTooLong,""); 
               T_EnOff(T_StabTimeLimit);
            }
            break;
         }
      }
      switch (SetTareStatus[Scale1]) 
      {
      case 99:
         if(!FaultState(FltS_EmptyBagTooHeavy))
         {
            FaultHandler(FltS_EmptyBagTooHeavy,"");
         }
         break;
      
      case 0:
         if(FaultState(FltS_EmptyBagTooHeavy))
         {
            FaultResetReq();
         }
         SeqMaster = stp_FEED;
         break; 
                                        
      default:
         MessageDisplay(concat(lsub(TX_MSGSYSTEMWARNING),100 + SetTareStatus[Scale1]),Step_Message);
         break;
      }
      break;
   
   case stp_FEED:
      if(SeqMasterMEM != SeqMaster) 
      {
         SeqMasterMEM = SeqMaster;
         MessageDisplay(lsub(TX_MSGFEEDING),Step_Message);
         FeedInProcess = ON;
         ZeroFreqCnt++;
      } 
                
      if(SeqFeeder == stp_FEEDDONE && !IsCleanoutDone)
      { 
         FeedInProcess = OFF;
         FirstWeighment = OFF;
         if (!FeedCycleCancelled)
         {        
            if((SampleFreqCnt>=0 && SampleFreqCnt<MP.SampleSize)||(WP.FeedAlgoType != FeediQSim && (LowLevel[Scale1] || EmptyHopper)) 
              || CheckAll  || iQFeedWaitSettled || CheckOnce || AutoPause)
            {
               if(T_Dn(T_FaultDisplay))
               DisplayAlarmDelay = OFF;
               
               if(!DisplayAlarmDelay)
               {
                  CheckOnce = OFF;
                  SampledInPause = AutoPause;
                  SeqMaster = stp_SAMPLE;
               }
            } 
            else 
            {
               WeighingDataRefreshFlag = ON;
               SeqMaster = stp_BAGFILLED;
            }
            WeighmentAborted = OFF; 
         }
         else  SeqMaster = stp_INIT;
      }    
      break;
   
   case stp_SAMPLE:
      if(SeqMasterMEM != SeqMaster) 
      {
         SeqMasterMEM = SeqMaster; 
         SampleBag = ON;     
      }
      if(!IsSettled[Scale1])
      {
         MessageDisplay(lsub(TX_MSGSTABILIZATION),Step_Message);                
         if(SeqMasterMEM != SeqMaster || (!T_EN[T_StabTimeLimit] && !FaultState(FltS_StabTooLong)))
         {
            T_EnOn(T_StabTimeLimit);
         }  
         else 
         {
            if(T_Dn(T_StabTimeLimit)) 
            {
               FaultHandler(FltS_StabTooLong,""); T_EnOff(T_StabTimeLimit);
            }
         }
      }
      else 
      {
         T_EnOff(T_StabTimeLimit);
         if(FaultState(FltS_StabTooLong))FaultResetReq();
         CheckedWeight[Scale1] = NetWeight[Scale1];
         WeightSampled[Scale1]=ON;
         if(!TopUp)
              {
               CorrWeight[Scale1] = CheckedWeight[Scale1];
              }
         
         if(WP.FeedAlgoType != FeediQSim && !SlowCycleOccured) 
         if(CheckedWeight[Scale1] < WP.UnderWeight && WP.TopUpONTime > 0 && !WeighmentAborted) 
         {
            TopUp = ON;
            SeqMaster = stp_TOPUP;
            break;
         }
      
         if(CheckedWeight[Scale1] < WP.UnderWeight || CheckedWeight[Scale1] > WP.OverWeight)
         { 
            ZeroFreqCnt = 0;
            SampleFreqCnt = 0;
            SeqMaster = stp_OUTOFTOL;
         } 
         else 
         {
            WeighingDataRefreshFlag = ON;
            SeqMaster = stp_BAGFILLED;
         }
      }
      break;
            
   case stp_TOPUP:
      if(SeqMasterMEM != SeqMaster) 
      {
         SeqMasterMEM = SeqMaster;
         MessageDisplay(lsub(TX_MSGFEEDTOPUP),Step_Message);
         if(WP.FeedAlgoType != FeedStd)
         {
            InitStandardFeed(OFF);
         }
         if(AOUTActive[AIO1])
         {
            if(MC.FeederType != FeederDS)
            {
               aout(AIO1,WP.MinFeeding);
            }
            else
            {
               aout(AIO1,0);
            }  
         }
         
         if(AOUTActive[AIO2])
         {
            if(MC.FeederType == FeederDS)
            {
               aout(AIO2,WP.MinFeeding);
            }
            else
            {
               aout(AIO2,WP.RegMinFeeding);
            } 
         } 
         
        if(IO_Enable[O_StreamRegulator] && MC.FeederType != FeederBHS)
         {
            if(WP.StreamRegConfig == StreamRegHigh) 
               StreamRegRaise();
            else 
               StreamRegLower();                                   
         }
      }  
      if(NetWeight[Scale1] < WP.UnderWeight && !SlowCycleOccured)
      {
         if(!T_EN[T_TopUpONTime])
         {
            T_EnOn(T_TopUpONTime);
            Output_ON(O_DribbleFeed);
            Output_ON(O_CatchGate);
            if(MC.FeederType == FeederBHS) Output_ON(O_StreamRegulator);
            Output_ON(O_CatchGateInterm);
         }
         if((T_Dn(T_TopUpONTime)&&!T_EN[T_TopUpOFFTime]))
         {
            T_EnOn(T_TopUpOFFTime);
            Output_OFF(O_DribbleFeed);
            Output_OFF(O_CatchGate);
            if(MC.FeederType == FeederBHS) Output_OFF(O_StreamRegulator);
            Output_OFF(O_CatchGateInterm);
         }
         if(T_Dn(T_TopUpOFFTime))
         {
            T_EnOff(T_TopUpONTime);
            T_EnOff(T_TopUpOFFTime);
         }
         break;   
      }                            
      else
      {
         if(!SlowCycleOccured || (SlowCycleOccured && !DisplayAlarmDelay))
         {
            T_EnOff(T_TopUpONTime);
            T_EnOff(T_TopUpOFFTime);
            Output_OFF(O_DribbleFeed);
            Output_OFF(O_CatchGate);
            if(MC.FeederType == FeederBHS) Output_OFF(O_StreamRegulator);
            Output_OFF(O_CatchGateInterm);
            InitiQSmart = ON;
            InitiQFlash = ON;
            InitPLoop   = ON;
            if(AOUTActive[AIO1]) aout(AIO1,0);
            if(AOUTActive[AIO2]) aout(AIO2,0); 
         }
         
         if(SlowCycleOccured && !DisplayAlarmDelay) 
            DisplayAlarmDelay = ON;
         if(T_Dn(T_FaultDisplay))
            DisplayAlarmDelay = OFF;
         
         if(!DisplayAlarmDelay)
         {
            SeqMaster = stp_SAMPLE;
         }
         break;
      }
            
   case stp_OUTOFTOL:                  
      if(SeqMasterMEM != SeqMaster || (!FaultState(FltB_UnderWeight) && !FaultState(FltB_OverWeight) && !CleanoutWeighment))
      {    
         if(MP.OutOfTolTime > 0) T_EnOn(T_OutOfTolTime);
         if(CheckedWeight[Scale1] < WP.UnderWeight)
         {
            if(!FaultState(FltB_UnderWeight) && !CleanoutWeighment) 
             FaultHandler(FltB_UnderWeight,"");
            if(SeqMasterMEM != SeqMaster)
            {
               if(!CleanoutWeighment)
               {
                Discharge_State = DischUnder;
                Output_ON(O_UnderWeightAlarm);
                Output_ON(O_OutOfTolerance);
               } 
               
               if(!OutOfTol[Scale1])
               {
                  ProdUnderWeightFlag[Scale1]=ON;
                  OutOfTol[Scale1] = ON;
                  WeighingDataRefreshFlag = ON;
               }
            }
         }
         else if(CheckedWeight[Scale1] > WP.OverWeight)
         {
            if(!FaultState(FltB_OverWeight) && !CleanoutWeighment)  
             FaultHandler(FltB_OverWeight,"");
            if(SeqMasterMEM != SeqMaster)
            {
               if(!CleanoutWeighment)
               {
                Discharge_State = DischOver;
                Output_ON(O_OverWeightAlarm);
                Output_ON(O_OutOfTolerance);
               }
               
               if(!OutOfTol[Scale1])
               {
                  ProdOverWeightFlag[Scale1]=ON;
                  OutOfTol[Scale1] = ON;
                  WeighingDataRefreshFlag = ON;
               } 
            }
         }
         if(SeqMasterMEM != SeqMaster) SeqMasterMEM = SeqMaster;
         break;
      }
                            
      if(DischargeHMI || CleanoutWeighment || (IO_Enable[I_ResetOutofTolAlarm]&&Input_State(I_ResetOutofTolAlarm)&&!I_ResetOutofTolAlarmMem) || T_Dn(T_OutOfTolTime)) 
      {  
         if(IO_Enable[I_ResetOutofTolAlarm]&&Input_State(I_ResetOutofTolAlarm))I_ResetOutofTolAlarmMem=ON; 
         T_EnOff(T_OutOfTolTime);
         Output_OFF(O_UnderWeightAlarm);
         Output_OFF(O_OverWeightAlarm);
         Output_OFF(O_OutOfTolerance);
         DischargeHMI=OFF;
         FaultResetReq();
         Discharge_State = DischOff;
         SeqMaster = stp_BAGFILLED;
      }  
      break;
      
   case stp_BAGFILLED:
      if(SeqMasterMEM != SeqMaster)
      {
         SeqMasterMEM = SeqMaster;
         
         if(IO_Enable[O_LockingDevice])
         {
            if((IO_Enable[O_LanceVacuum] && WP.EvacuationTime > 0)  || (IO_Enable[O_KnockHammer] && WP.KnockingTime > 0) || (IO_Enable[O_ControlledAspiration] && WP.AspirationTime > 0))
            {
               ApplyLockingDevice = ON;
            }   
            else
            {
               if(((ZeroFreqCnt == 0 || (WP.FeedAlgoType != FeediQSim && (LowLevel[Scale1] || EmptyHopper))) && !AutoZeroOff) || ForceZero)
               {
                  // Zero needed on next filling cycle, do not apply locking device
                  SeqMaster = stp_BAGRELEASEDELAY;
               }
               else
               {
                  // We do not need to do the zero on the next cycle, apply locking device right now
                  ApplyLockingDevice = ON;
               }
            }
         }
      }
      
      if(!IO_Enable[O_LockingDevice] || SeqLockingDevice == stp_LDON)
      {
         if(IO_Enable[O_LanceVacuum] && WP.EvacuationTime > 0)
         {
            SeqMaster = stp_LANCE;
         }
         else if(IO_Enable[O_KnockHammer] && WP.KnockingTime > 0)
         {
            SeqMaster = stp_KNOCKHAMMER;
         }
         else
         {
            SeqMaster = stp_BAGRELEASEDELAY;
         }
      }
      break;
      
   case stp_LANCE:
      if(SeqMasterMEM != SeqMaster)
      {
         SeqMasterMEM = SeqMaster;
         SeqLance = stp_LANCEDOWN;
      }
      if(SeqLance == stp_LANCEHIGH)
      {
         if(IO_Enable[O_KnockHammer] && WP.KnockingTime > 0)
         {
            SeqMaster = stp_KNOCKHAMMER;         
         }
         else
         {
            SeqMaster = stp_BAGRELEASEDELAY;
         }
      }
      break;
      
   case stp_KNOCKHAMMER:
      if(SeqMasterMEM != SeqMaster)
      {
         SeqMasterMEM = SeqMaster;
         if(T_EN[T_KnockingTime])
         {
            T_EnOff(T_KnockingTime);
         }
         T_EnOn(T_KnockingTime);
         Output_ON(O_KnockHammer);
         MessageDisplay(lsub(TX_MSGKNOCKINGTIME),Step_Message);
      }
      if(T_Dn(T_KnockingTime))
      {
         Output_OFF(O_KnockHammer);
         SeqMaster = stp_BAGRELEASEDELAY;
      }
      break;
      
   case stp_BAGRELEASEDELAY:
       if(SeqMasterMEM != SeqMaster)
        { 
         SeqMasterMEM = SeqMaster;
         MsgWaitPauseSent = OFF;
         MsgBagReleaseDelaySent = OFF;
         if(T_EN[T_DelayedBagRelease])
          {
           T_EnOff(T_DelayedBagRelease);
          }
        }
        if(AutoPause && SampledInPause)
        { 
           if(!MsgWaitPauseSent)  
             {
              MessageDisplay(lsub(TX_MSGWAITPAUSE),Step_Message);
              MsgWaitPauseSent = ON;
             }  
        }
        else
        {     
         SampledInPause = OFF;
         if(WP.BagReleaseDelay > 0 && !IO_Enable[I_OkToReleaseBag])
         {
           if(!T_EN[T_DelayedBagRelease])
           { 
            T_EnOn(T_DelayedBagRelease);
            MessageDisplay(lsub(TX_MSGBAGRELEASEDELAY),Step_Message);
            MsgBagReleaseDelaySent = ON;
            if(IO_Enable[O_ControlledAspiration])
            {
               Output_ON(O_ControlledAspiration);
            }
           } 
         }
         else
         {
            SeqMaster = stp_READYTORELEASEBAG;
         }
         
         if(T_Dn(T_DelayedBagRelease))
         {
          T_EnOff(T_DelayedBagRelease);
          SeqMaster = stp_READYTORELEASEBAG;
         }
        }
         
      

      break;
   
      
   case stp_READYTORELEASEBAG:
      if(SeqMasterMEM != SeqMaster)
      {
         SeqMasterMEM = SeqMaster;
         WPMDuration += T_Acc(T_WPMDuration);
         T_EnOff(T_WPMDuration);
         ProdCycleUnitsPerMin[Scale1] = 60 / WPMDuration;
         RefreshAutoScreenReq();
         Output_ON(O_ReadyToReleaseBag);
         MsgDownStreamSent = OFF;
      }
      if(IO_Enable[I_OkToReleaseBag])
      {
         if(Input_State(I_OkToReleaseBag))
         {
            if(ReleaseAcknowledge)
            {
               SeqMaster = stp_RELEASEBAG;
            }
            else
            {
               SeqMaster = stp_WAITFORRELEASEACKNOWLEDGE;
            }
         }
         else if(!MsgDownStreamSent)
         {
            MessageDisplay(lsub(TX_MSGWAITDOWNSTREAM),Step_Message);
            MsgDownStreamSent = ON;
         }
      }
      else
      {
         if(ReleaseAcknowledge)
         {
            SeqMaster = stp_RELEASEBAG;
         }
         else
         {
            SeqMaster = stp_WAITFORRELEASEACKNOWLEDGE;
         }
      }
      break;
      
   case stp_WAITFORRELEASEACKNOWLEDGE:
      if(SeqMasterMEM != SeqMaster)
      {
         SeqMasterMEM = SeqMaster;
         MessageDisplay(lsub(TX_MSGWAITFORRELEASEACKNOWLEDGE),Step_Message);
         // Stop timer, this wait is not under the weigher's control
         WPMDuration += T_Acc(T_WPMDuration);
         T_EnOff(T_WPMDuration);
      }
      if(ReleaseAcknowledge)
      {
         SeqMaster = stp_RELEASEBAG;
         T_EnOn(T_WPMDuration);
      }
      break;
            
   case stp_RELEASEBAG:
      if(SeqMasterMEM != SeqMaster)
      {
         SeqMasterMEM = SeqMaster;
         ReleaseAcknowledge = OFF;
         OutOfTol[Scale1] = OFF;
         Output_ON(O_BagRelease);
         Output_OFF(O_BagHolder);
         BagInPlace = OFF;
         SampleFreqCnt++;
         Output_OFF(O_ReadyToReleaseBag);
         if(WP.FeedAlgoType == FeediQSim) T_EnOn(T_DischReqiQSim);
         if (SampleFreqCnt == MP.SampleSize) 
         {
            LastCheckedWeight[Scale1] = ON;   
         }
      }
      
         //If Bag Holder, wait for weight to start decreasing(lost of Settled or Weight Close to Zero)
      if((!Input_State(I_DischReq) && (!MC.CFGBagHolder || !IsSettled[Scale1] ||(abs(NetWeight[Scale1]) <= (MaxWeight[Scale1] * ZeroLimit)))) 
          || T_Dn(T_DischReqiQSim)) 
      {
         SeqMaster = stp_ASPIRATION;
         Output_OFF(O_BagRelease);
         T_EnOff(T_DischReqiQSim);
         if(IO_Enable[O_ExternalBagCounter])
         {
            if(T_EN[T_ExternalBagCounter])
            {
               T_EnOff(T_ExternalBagCounter);
            }
            Output_ON(O_ExternalBagCounter);
            T_EnOn(T_ExternalBagCounter);
         }
         if(WP.FeedAlgoType == FeediQSim)
         {
            iQSimSetWeight(SCALE_CHANNEL, SCALE_NUMBER,0.0);
         }
      }
      else
      {
         MessageDisplay(lsub(TX_BAGRELEASED),Step_Message);
      }
      break; 
      
   case stp_ASPIRATION:
      if(SeqMasterMEM != SeqMaster)
      {
         SeqMasterMEM = SeqMaster;
         if(T_EN[T_AspirationTime])
         {
            T_EnOff(T_AspirationTime);
         }
         if(IO_Enable[O_ControlledAspiration] && WP.AspirationTime > 0)
         {
            T_EnOn(T_AspirationTime);
            MessageDisplay(lsub(TX_MSGASPIRATIONTIME),Step_Message);
         }
      }
      
      if(!T_EN[T_AspirationTime] || T_Dn(T_AspirationTime))
      {
         Output_OFF(O_ControlledAspiration);
         ProdCycleDuration[Scale1] = T_Acc(T_CycleDuration);
         T_EnOff(T_CycleDuration);
         T_EnOn(T_CycleDuration);
         RefreshAutoScreenReq();
         SeqMaster = stp_INIT;
      }
      break;
   }    
}

/*------------------------------------------------------------------------------- 
--  Procedure Name    : SEQ_Feeder
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08
--  
--  Purpose           : Feeder Sequence
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------------*/
void SEQ_Feeder(void)
{
   bool FeedingStarted,Aout1On,Aout2On;
   int scanmemFeeder[10],startcnt,startdelay,startdelay2;
   float DribbleSP;

   if(SeqModeManager == stp_OFF_MODE) SeqFeeder = stp_INIT;
   
   if(SeqFeeder != stp_INIT)
   {
    if(IO_State[O_BulkFeed] || IO_State[O_DribbleFeed] || IO_State[O_CatchGate])
    { 
     if(!T_EN[T_AbnormalFeed] && ProdBulkDuration[Scale1] > 0) 
     { 
      T_PRE[T_AbnormalFeed] = ProdBulkDuration[Scale1];
      T_EnOn(T_AbnormalFeed);
     } 
    
    if(T_Dn(T_AbnormalFeed) && (NetWeight[Scale1] < (WP.BulkCutoff *0.5)) && !CheckOnce) 
     CheckOnce = ON;
    } 
   }
   
   
   if(SeqMaster==stp_FEED && SeqFeeder==stp_INIT)
      { 
         switch(WP.FeedAlgoType)
         {
         case FeedStd:
            SeqFeeder = stp_FEEDSTD;
            break;
         case FeediQSim:
            SeqFeeder = stp_FEEDSTD;
            break;         
         case FeediQSmart:
            SeqFeeder = stp_FEEDSMART;
            break;     
         case FeediQFlash:
            SeqFeeder = stp_FEEDFLASH;
            break;
         case FeedPLoop:
           SeqFeeder = stp_FEEDPLOOP;
            break;         
         default:   
            SeqFeeder = stp_FEEDSTD;
            break;             
         }
      }

   SeqFeederNext = SeqFeeder;       
   switch (SeqFeeder)
   {
   case stp_INIT:
      if(SeqFeeder!=SeqFeederMEM) 
      {
         SeqFeederMEM = SeqFeeder;
         iQFeedDone = OFF;
         FeedingStarted = OFF;
         FeedCycleCancelled = OFF;
         EmptyBuffer();
         BulkFeedMem = OFF;
         DribbleFeedMem = OFF;
         T_EnOff(T_AbnormalFeed);
      }
      break;
            
   case stp_FEEDSTD:
      if(SeqFeederMEM != SeqFeeder)
      {
         SeqFeederMEM = SeqFeeder;
         if(WP.FeedAlgoType == FeediQSim) 
         {
            iQSimFeed();
         }
      }
      if(SeqFeedSTD == stp_FSDone)
      {
         if(WP.FeedAlgoType == FeediQSim) iQSimStopProgram (SCALE_CHANNEL, SCALE_NUMBER);
         if(!IsCleanoutDone)
         {
            SeqFeeder = stp_FEEDDONE;
         }
      }
      break;
                                                                                              
   case stp_FEEDSMART:
      if(SeqFeederMEM != SeqFeeder)
      {
         SeqFeederMEM = SeqFeeder;
         Aout1On = OFF;
         Aout2On = OFF;
         startcnt=0;
         startdelay=0;
         startdelay2=0;
         feeddelayduration = systemtimer_ms() - feeddelaystart;
         if(iQFeedSettledCnt < IQFEEDSETTLEDNBR || (SampleFreqCnt>=0 && SampleFreqCnt<MP.SampleSize) || CheckAll || LowLevel[Scale1] || EmptyHopper)
         iQFeedWaitSettled = ON;
         else
         iQFeedWaitSettled = OFF;
         iQFeedSettledCnt++;
         
         if(InitiQSmart || LearniQSmartParam || LearniQSmartParamMem) 
          InitStartiQSmart();
         else
          {
           iQOutActivate();
           if(InitCPSmSwitchTime)
            {
             SmSetControlParameters();
            }
          }    
      
         if(iQSmartLearnNbr < IQFEEDSETTLEDNBR)
         {
            MessageDisplay(concat(lsub(TX_MSGFEEDING),"...iQSmartLearn"),Step_Message);
         }
      
         if(LearniQSmartParam)
         {
            if(AutoTune_State != AutoTuneON)
            {
               AutoTune_State = AutoTuneON; 
               AutoTuneStateFlag = Execute;
            }
         } 
         else
         {
            if(AutoTune_State != AutoTuneOFF)
            {
               AutoTune_State = AutoTuneOFF; 
               AutoTuneStateFlag = Execute;
            }
         }
         scanmemFeeder[0] = systemtimer_ms();
      }           
            
      if(AOUTActive[AIO1]  && !Aout1On)
      {
         if(MC.FeederType == FeederGV)
         {
            if(MC.DribbleOutOnInBulk || out(IO_Address[O_BulkFeed]))
            {
               aout(AIO1,WP.MinFeeding);
               Aout1On = ON;
            }
         } 
         if(MC.FeederType == FeederDS)
         {
            aout(AIO1,WP.MaxFeeding);
            Aout1On = ON;
         }
      }
      if(AOUTActive[AIO2] && !Aout2On)
      {
         if(MC.FeederType == FeederDS)
         {
            if(MC.DribbleOutOnInBulk || out(IO_Address[O_BulkFeed]))
            {
               aout(AIO2,WP.MinFeeding);
               Aout1On = ON;
            }
         }
      }
            
      if(!FeedingStarted)
      {
         if(WP.StreamRegConfig != StreamRegLow && IO_Enable[O_StreamRegulator] && MC.FeederType != FeederBHS) 
         {
            StreamRegRaise();
         }
         
         if(T_Dn(T_StreamRegHigh) || WP.StreamRegConfig == StreamRegLow || WP.StreamRegConfig == StreamRegAutoRaiseAtRun || !IO_Enable[O_StreamRegulator] || MC.FeederType == FeederBHS)
         {
            scanmemFeeder[1] = systemtimer_ms();
            FeedingStarted = StartFeeding(iQFeedWaitSettled);
            
            if(!T_EN[T_iQFeedTimeout])T_EnOn(T_iQFeedTimeout);
            
            if(T_Dn(T_iQFeedTimeout)) 
            {
               SystemErrorCode = "133 iQ Smart start timeout";
               T_EnOff(T_iQFeedTimeout);
            }
            
            startcnt++; 
            scanmemFeeder[2] = systemtimer_ms();
            startdelay = scanmemFeeder[2] - scanmemFeeder[0];
         }
      } 
            
            
                  
      if (FeedingStarted)
      {
         if(T_EN[T_iQFeedTimeout])T_EnOff(T_iQFeedTimeout);
         if(IO_State[O_DribbleFeed] && startdelay2==0) startdelay2 = systemtimer_ms() - scanmemFeeder[2];
         
          if(MC.FeederType == FeederBHS)
            { 
             if(IO_State[O_CatchGate])
             {
              if(!IO_State[O_StreamRegulator])
                Output_ON(O_StreamRegulator);
             }   
             else
             {
              if(IO_State[O_StreamRegulator])
               Output_OFF(O_StreamRegulator);
             }  
            }

         if(!iQFeedDone)
         {
            scanmemFeeder[3] = systemtimer_ms();
            GetFillingData(OFF);
            scanmemFeeder[4] = systemtimer_ms();
         }   
         
          if((WP.StreamRegConfig == StreamRegAutoRaiseAtEnd || WP.StreamRegConfig == StreamRegAutoRaiseAtRun) && MC.FeederType != FeederBHS)
              {
               if(!BulkFeedMem)
               { 
                if((!FeederBWithAout && IO_Enable[O_BulkFeed] && out(IO_Address[O_BulkFeed])) || (FeederBWithAout && out(BULKADDRESS))) 
                 {
                   BulkFeedMem = ON;
                 }
               }  
               
               else
               {
                if(!DribbleFeedMem)
                {
                 if((!FeederBWithAout && IO_Enable[O_BulkFeed] && !out(IO_Address[O_BulkFeed])) || (FeederBWithAout && !out(BULKADDRESS))) 
                  {  
                  StreamRegLower();
                  DribbleFeedMem = ON;
                  }
                }
               }   
              }    

         if(iQFeedDone)
         {
            T_EnOff(T_iQFeedTimeout);
            iQSmartLearnNbr++;
            if(MC.FeederType == FeederBHS) Output_OFF(O_StreamRegulator);
            if(WP.StreamRegConfig == StreamRegAutoRaiseAtEnd && IO_Enable[O_StreamRegulator] && MC.FeederType != FeederBHS) 
                StreamRegRaise();
            if(!iQFeedStatusFinished && !SlowCycleMem)
            { 
               WeighmentAborted = ON;
            }
            ClearAnalogOuts();                          
            SeqFeeder = stp_FEEDDONE;
         }
      }
            
      if(SlowCycleMem)
      {
         if(DischargeHMI) 
         {
            FeedingStop();
            if(MC.FeederType == FeederBHS) Output_OFF(O_StreamRegulator);
            CheckOnce = ON;
            WeighmentAborted = ON;
            DischargeHMI = OFF;
         }
         if(!FeedingStarted) 
         {
            GetFillingData(OFF);
            WeighmentAborted = ON;
         } 
      }      
      break;
            
   case stp_FEEDFLASH:
      if(SeqFeederMEM != SeqFeeder)
      {
         SeqFeederMEM = SeqFeeder;
         startcnt=0;
         startdelay=0;
         startdelay2=0;
         feeddelayduration = systemtimer_ms() - feeddelaystart;               
         if(iQFeedSettledCnt < IQFEEDSETTLEDNBR || (SampleFreqCnt>=0 && SampleFreqCnt<MP.SampleSize) || CheckAll || LowLevel[Scale1] || EmptyHopper)
            iQFeedWaitSettled = ON;
         else
            iQFeedWaitSettled = OFF;
         iQFeedSettledCnt++;   
         
         if(InitiQFlash || LearniQFlashParam) InitStartiQFlash();
         else iQOutActivate();
            scanmemFeeder[0] = systemtimer_ms();
         if(iQFlashLearnNbr < IQFEEDSETTLEDNBR)
         {
            MessageDisplay(concat(lsub(TX_MSGFEEDING),"...iQFlashLearn"),Step_Message);
         }
      }           
      if(!FeedingStarted)
      {
         scanmemFeeder[1] = systemtimer_ms();
         FeedingStarted = StartFeeding(iQFeedWaitSettled);
         
         if(!T_EN[T_iQFeedTimeout])T_EnOn(T_iQFeedTimeout);
         if(T_Dn(T_iQFeedTimeout)) 
         {
            SystemErrorCode = "123 iQ Flash start timeout";
            T_EnOff(T_iQFeedTimeout);
         }
         
         startcnt++; 
         scanmemFeeder[2] = systemtimer_ms();
         startdelay = scanmemFeeder[2] - scanmemFeeder[0];              
      }
                            
      if (FeedingStarted)
      {
         if(T_EN[T_iQFeedTimeout])T_EnOff(T_iQFeedTimeout);
         if(IO_State[O_DribbleFeed] && startdelay2==0) startdelay2 = systemtimer_ms() - scanmemFeeder[2];
         if(!iQFeedDone)
         {
            scanmemFeeder[3] = systemtimer_ms();
            GetFillingData(OFF);
            scanmemFeeder[4] = systemtimer_ms();
         }   
         
         if(iQFeedDone)
         {
            T_EnOff(T_iQFeedTimeout);
            iQFlashLearnNbr++;
            if(!iQFeedStatusFinished  && !SlowCycleMem) 
            {
               WeighmentAborted = ON;
            } 
            if(LearniQFlashParam)
            {
               if(AutoTune_State != AutoTuneON)
               {
                  AutoTune_State = AutoTuneON; 
                  AutoTuneStateFlag = Execute;
               }
            } 
            else
            {
               if(AutoTune_State != AutoTuneOFF)
               {
                  AutoTune_State = AutoTuneOFF; 
                  AutoTuneStateFlag = Execute;
               }
            }
            SeqFeeder = stp_FEEDDONE;
         }
      }  
         if(SlowCycleMem)
         {
            if(DischargeHMI) 
            {
               FeedingStop();
               CheckOnce = ON;
               WeighmentAborted = ON;
               DischargeHMI = OFF;
            }
            if(!FeedingStarted) 
            {
               GetFillingData(OFF);
               WeighmentAborted = ON;
            }
         }   
      break;
      
   case stp_FEEDPLOOP:
            if(SeqFeederMEM != SeqFeeder)
               {
                 SeqFeederMEM = SeqFeeder;
                 if(MC.FeederType == FeederBSFHS)
                 {
                  if(WP.StreamRegConfig != StreamRegLow && IO_Enable[O_StreamRegulator]) 
                    StreamRegRaise();
                 }
                 if((SampleFreqCnt>=0 && SampleFreqCnt<MP.SampleSize) || CheckAll || LowLevel[Scale1] || EmptyHopper)
                    iQFeedWaitSettled = ON;
                 else
                    iQFeedWaitSettled = OFF;
                 
                 if(T_Dn(T_StreamRegHigh) || WP.StreamRegConfig == StreamRegLow || WP.StreamRegConfig == StreamRegAutoRaiseAtRun || !IO_Enable[O_StreamRegulator] || MC.FeederType == FeederBHS)
                  { 
                   InitStartPLoop();
                  } 
               }           
            
              if(!FeedingStarted)
              {
               FeedingStarted = StartFeeding(OFF);
               if(!T_EN[T_iQFeedTimeout])
                T_EnOn(T_iQFeedTimeout);
               if(T_Dn(T_iQFeedTimeout)) 
               {
                SystemErrorCode = "153 PLoop start timeout";
                T_EnOff(T_iQFeedTimeout);
               }
              }
              
            if (FeedingStarted)
            {
             if(T_EN[T_iQFeedTimeout])
              T_EnOff(T_iQFeedTimeout);

             if(MC.FeederType == FeederBSFHS)
             {
              if((WP.StreamRegConfig == StreamRegAutoRaiseAtEnd || WP.StreamRegConfig == StreamRegAutoRaiseAtRun) && (NetWeight[Scale1] > WP.BulkCutoff) && IO_State[O_StreamRegulator])
               StreamRegLower();
             } 
             
             if(!Scale1CmpDribbleActive)
             {
              DribbleSP = (WP.Cutoff-CutoffCorrOffset[Scale1])*UnitIQtoADM;
              setcomparator(SCALE_NUMBER,Scale1CmpDribble,IO_Address[O_DribbleFeed],DribbleSP,"<",1);
              Scale1CmpDribbleActive = ON;
             } 
             
             if(!Scale1CmpCatchGateActive)
             {
              DribbleSP = (WP.Cutoff-CutoffCorrOffset[Scale1])*UnitIQtoADM;
              setcomparator(SCALE_NUMBER,Scale1CmpCatchGate,IO_Address[O_CatchGate],DribbleSP,"<",1);
              Scale1CmpCatchGateActive = ON;
             } 
            
             if(MC.FeederType == FeederBHS)
             {
              if(!Scale1CmpCatchGateIntermActive)
              {
               DribbleSP = (WP.Cutoff-CutoffCorrOffset[Scale1])*UnitIQtoADM;
               setcomparator(SCALE_NUMBER,Scale1CmpCatchGateInterm,IO_Address[O_StreamRegulator],DribbleSP,"<",1);
               Scale1CmpCatchGateIntermActive = ON;
              }
             }   
             
              if(!iQFeedDone)
              { 
                GetFillingData(OFF);
              }   

              if(iQFeedDone)
               {
                ClearComparators();
                T_EnOff(T_iQFeedTimeout);
                if(WP.StreamRegConfig == StreamRegAutoRaiseAtEnd && IO_Enable[O_StreamRegulator] && MC.FeederType != FeederBHS) 
                 StreamRegRaise();
                SeqFeeder = stp_FEEDDONE;
               }
            }
             
              if(SlowCycleMem)
              {
               if(DischargeHMI) 
               {
                FeedingStop();
                ClearComparators();
                CheckOnce = ON;
                WeighmentAborted = ON;
                DischargeHMI = OFF;
               }
              if(!FeedingStarted) 
               {
               GetFillingData(OFF);
               WeighmentAborted = ON;
               }
                
              }   
            break;
         
   case stp_FEEDDONE:
      if(SeqFeederMEM != SeqFeeder)SeqFeederMEM = SeqFeeder;          
      if(SeqMaster!=stp_FEED) SeqFeeder = stp_INIT;
      break;         
   }                                               
}

/*------------------------------------------------------------------------------- 
--  Procedure Name    : SEQ_FeedSTD
--  Created by        : Steve Santerre
--  Last modified on  : 2015/08
--  
--  Purpose           : Standard Feeder Sequence
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------------*/
void SEQ_FeedSTD(void)
{
   bool SlowCycleDone,SlowCycleEndDribble, ResumeInDribble,FirstCycle,DribbleOnly;
   bool CmpBulkOn1,CmpBulkOn2,Scale1BulkWeightDirect,StandardFeedInitDone;
   int scanmemSTD[10],address;
   float BulkBlankLimit,DribbleBlankLimit,Aout1SwitchValue,DribbleSP,BulkSP;
   
   if(SeqModeManager == stp_OFF_MODE) 
   {
      SeqFeedSTD = stp_INIT;
      FirstCycle = ON;
      StandardFeedInitDone = OFF;
      if(T_EN[T_InitAlgo])T_EnOff(T_InitAlgo);
   } 

   if(WP.FeedAlgoType == FeedStd || WP.FeedAlgoType == FeediQSim)
   { 
      //Slow Cycle Alarm Managment
      if(SlowCycleMem || CycleInHoldMem)
      {
         if(!SlowCycleDone)
         if(DischargeHMI || WP.SlowCycleEnd == END_CYCLE) 
         {
           if(CycleInHoldMem)
            {
             UnHoldCycle();
            }
            SeqFeedSTD = stp_FSDribbleSetPoint;
            DribbleDone= ON;
            CheckOnce = ON;
            DischargeHMI = OFF;
            SlowCycleDone = ON;
            WeighmentAborted = ON;
         }
         
         if(WP.SlowCycleEnd == DRIBBLE_CYCLE && !SlowCycleEndDribble)
         {
          if(SeqFeedSTD != stp_FSDribbleSetPoint)
           {
            FeedSTDReset();
            SeqFeedSTD = stp_FSDribbleSetPoint;
           } 
          SlowCycleEndDribble = ON;
         } 
      }
      else  
      {
         if(SlowCycleDone) SlowCycleDone = OFF;
         if(SlowCycleEndDribble) SlowCycleEndDribble = OFF; 
      }           
    
      //Hold UnHold managment
      if(SeqFeedSTD != stp_INIT)
      {
         if(IO_Enable[I_HoldCycle] && Input_State(I_HoldCycle) && !HoldCycleMem)
         {
            if(SeqFeedSTD == stp_FSFeedDelay || SeqFeedSTD == stp_FSPostFilling)
            {
               // We are post-filling, just cancel the post-filling
               SeqFeedSTD = stp_INIT;
               Output_OFF(O_BulkFeed);
               Output_OFF(O_StreamRegulator);
               T_EnOff(T_PostFillingTime);
               T_EnOff(T_FeedDelay);
            }
            else
            {
               // Hold feeding cycle
               HoldCycleMem = ON;
               HoldCycleTgl = ON;
            }
         }
         
         if(WP.SlowCycleEnd == HOLD_CYCLE)
         {  
            if(SlowCycleMem)
            {
               if(!CycleInHoldMem) HoldCycleTgl = ON;
            } 
            else if(StartHMI && CycleInHoldMem)
            {
               HoldCycleTgl = ON;
            }
         }   

         if(HoldCycleTgl)
         {    
            if(!CycleInHoldMem)
            {
               HoldCycle();
               SeqFeedSTD = stp_FSHold;
               MessageDisplay(lsub(TX_MSGCYCLEINHOLD),Step_Message);
            }
            else
            {
               ResumeInDribble = UnHoldCycle();
               SeqFeedSTD = stp_INIT;  
            }
         }  
         if(IO_Enable[I_HoldCycle] && !Input_State(I_HoldCycle) && HoldCycleMem) HoldCycleMem = OFF;
      }
   } 
                
   SeqFeedSTDNext = SeqFeedSTD;
   switch (SeqFeedSTD)
   {
   case stp_INIT:
      if(SeqFeedSTDMEM != SeqFeedSTD)
      {
         CycleInHoldMem = OFF;
         HoldCycleTgl = OFF;
         BulkDone = OFF;
         IntermDone = OFF;
         DribbleDone = OFF;
         DribbleOnly = OFF;
         SeqFeedSTDMEM = SeqFeedSTD;
      } 
        
      if(SeqFeeder == stp_FEEDSTD)
      {
         if(FirstCycle)
         {
            InitStandardFeed(ON);
            T_EnOn(T_InitAlgo);
            FirstCycle = OFF;
         }          
          
      if(!StandardFeedInitDone)
       {
        if(T_Dn(T_InitAlgo)) //Allow time for weight filter to init when Setting Algorithm
          {
           StandardFeedInitDone = ON;
          }
       } 
             
      if(StandardFeedInitDone)
      {  
          if(((IO_Enable[I_DribbleFeedOnly] && Input_State(I_DribbleFeedOnly)) || ResumeInDribble ||
           (WP.BulkCutoff == 0 && !BulkOnTime[Scale1]) || (WP.BFTime <= 0 && BulkOnTime[Scale1])) && WP.FeedAlgoType != FeediQSim) //Dribble Feed Only Requested by External Input
         
         { 
            if(OK4DribbleFeed)
            {
               if(IO_Enable[O_CatchGate]) 
                Output_ON(O_CatchGate); 
               if(IO_Enable[O_CatchGateInterm])
                Output_ON(O_CatchGateInterm);
 
               if(AOUTActive[AIO1])
               {
                  if(MC.FeederType != FeederDS)
                  {
                     if(VibrationEnabled)
                     {
                        aout(AIO1,GetAoutVibration());
                     }
                     else
                     {
                        aout(AIO1,WP.MinFeeding);
                     }
                  }
               }
               if(AOUTActive[AIO2])
               {
                 if(MC.FeederType == FeederBHS || MC.FeederType == FeederBSFHS)
                  {
                     aout(AIO2,WP.RegMinFeeding);
                  }
                 if(MC.FeederType == FeederDS)
                  {
                     aout(AIO2,WP.MinFeeding);
                  }
               }         
               SeqFeedSTD = stp_FSDribbleBlank;
               T_EnOn(T_DribbleDuration);
               ResumeInDribble = OFF;
               DribbleOnly = ON;
               break;
            }
            break;  
         }
         else
         {  
            if(OK4BulkFeed)
            { 
               if(IO_Enable[O_CatchGate])
                Output_ON(O_CatchGate);
               if(IO_Enable[O_CatchGateInterm])
                Output_ON(O_CatchGateInterm); 
               SeqFeedSTD = stp_FSBulkBlank;
            }
            else 
               break;
         }
       }  
       else 
         break;
      }   
     else break;
 
   case stp_FSBulkBlank:
      if(SeqFeedSTDMEM != SeqFeedSTD) 
      {
         SeqFeedSTDMEM = SeqFeedSTD;
         MessageDisplay(lsub(TX_MSGBULKBLANK),Step_Message);
         T_EnOn(T_BulkDuration);
         if(BulkOnTime[Scale1]) 
         {
            T_PRE[T_BulkBlankT] = WP.BFTime;
         }
         else 
         {
            T_PRE[T_BulkBlankT] = MC.BulkBlankT;
         }
         T_EnOn(T_BulkBlankT);
         Output_ON(O_BulkFeed);
         if(AOUTActive[AIO1])
         {
            if(MC.FeederType == FeederGV)
            {
               if(MC.DribbleOutOnInBulk)
               {
                  aout(AIO1,WP.MinFeeding);
               }
            } 
            else
            {
               aout(AIO1,WP.MaxFeeding);
            }
         }
         if(AOUTActive[AIO2])
         {
            if(MC.FeederType == FeederBHS || MC.FeederType == FeederBSFHS)
            {
               aout(AIO2,WP.RegMaxFeeding);
            }
            if(MC.FeederType == FeederDS && MC.DribbleOutOnInBulk)
            {
               aout(AIO2,WP.MinFeeding);
            }
         }     
            
         if(MC.FeederType == FeederBHS)
         {
            if(!Scale1CmpCatchGateIntermActive)
            {
              if(WP.FeedAlgoType == FeediQSim)
              DribbleSP = TargetWeight[Scale1]*UnitIQtoADM;
              else
              DribbleSP = (WP.Cutoff-CutoffCorrOffset[Scale1])*UnitIQtoADM;
              setcomparator(SCALE_NUMBER,Scale1CmpCatchGateInterm,IO_Address[O_StreamRegulator],DribbleSP,"<",1);
              Scale1CmpCatchGateIntermActive = ON;
           }
         }
          else  
          {
            if(WP.StreamRegConfig != StreamRegLow && IO_Enable[O_StreamRegulator])
            StreamRegRaise();
          }  

         if(MC.DribbleOutOnInBulk)
         {
            Output_ON(O_DribbleFeed);
            if(IO_Enable[O_IntermFeed])
               Output_ON(O_IntermFeed);
         }
         break;    
      }
      
      if(T_Dn(T_BulkBlankT)|| T_PRE[T_BulkBlankT]==0 || WP.FeedAlgoType == FeediQSim)  
      {
         //if((IO_Enable[O_IntermFeed]&&OK4IntermFeed)||(!IO_Enable[O_IntermFeed]&&OK4DribbleFeed)||!BulkOnTime[Scale1])
         //{
            
            if(!BulkOnTime[Scale1])
            {
               SeqFeedSTD = stp_FSBulkSetPoint;
               T_EnOff(T_BulkBlankT);
            }
            else
            {
               if(T_EN[T_BulkDuration])
               {
                Output_OFF(O_BulkFeed);
                ProdBulkDuration[Scale1] = T_Acc(T_BulkDuration);
                T_EnOff(T_BulkDuration);
               }
               
               if(IO_Enable[O_IntermFeed] && OK4IntermFeed && !AOUTActive[AIO1])
               {
                SeqFeedSTD = stp_FSInterBlank;
                BulkDone=OFF;
                T_EnOn(T_SlowCycleTime);
                Output_OFF(O_CycleInHold);
                T_EnOff(T_BulkBlankT);
                T_EnOn(T_DribbleDuration);
               }
               else if((!IO_Enable[O_IntermFeed]||AOUTActive[AIO1]) && OK4DribbleFeed)
               {
                SeqFeedSTD = stp_FSDribbleBlank; 
                BulkDone=OFF;
                T_EnOn(T_SlowCycleTime);
                Output_OFF(O_CycleInHold);
                T_EnOff(T_BulkBlankT);
                T_EnOn(T_DribbleDuration);
                break;
                }
                else
                {
                 if(!IO_State[O_CycleInHold]) 
                 {
                  Output_OFF(O_DribbleFeed);
                  Output_OFF(O_IntermFeed);
                  T_EnOff(T_SlowCycleTime);
                  Output_ON(O_CycleInHold);
                  MessageDisplay(lsub(TX_MSGCYCLEINHOLD),Step_Message);
                 } 
                } 
                break;     
            }
         //}
      } 
      break;
       
   case stp_FSBulkSetPoint:
      if(SeqFeedSTDMEM != SeqFeedSTD)
      {
         SeqFeedSTDMEM = SeqFeedSTD;
         MessageDisplay(lsub(TX_MSGBULKFEED),Step_Message);
         T_EnOn(T_BulkDuration);
         scanmemSTD[0] = systemtimer_ms();
         if(AOUTActive[AIO1])
         {
            if(MC.FeederType != FeederGV)
            {
               if(MC.FeederType == FeederDS) Aout1SwitchValue = 0;
               else Aout1SwitchValue = WP.MinFeeding;
               setanalogcomparator (SCALE_NUMBER, Scale1CmpAout1, AIO1, WP.BulkCutoff*UnitIQtoADM, "<", 1, WP.MaxFeeding,Aout1SwitchValue ); //Bulk
            }
         }  
        
         if(AOUTActive[AIO2] && (MC.FeederType == FeederBHS || MC.FeederType == FeederBSFHS))
            {
             if(MC.FeederType == FeederBSFHS) Aout2SwitchVal = WP.IntermCutoff;
             if(MC.FeederType == FeederBHS)   Aout2SwitchVal = WP.BulkCutoff;
             setanalogcomparator (SCALE_NUMBER, Scale1CmpAout2, AIO2, Aout2SwitchVal*UnitIQtoADM, "<", 1, WP.RegMaxFeeding, WP.RegMinFeeding); //
            }

         if(IO_Enable[O_BulkFeed])
         {
            if(WP.FeedAlgoType == FeediQSim)
            BulkSP = BulkCO*UnitIQtoADM;
            else
            BulkSP = WP.BulkCutoff*UnitIQtoADM;
            setcomparator(SCALE_NUMBER,Scale1CmpBulk,IO_Address[O_BulkFeed],BulkSP,"<",1);
            Scale1CmpBulkActive = ON;
         }
         else
         {
            if(IO_Enable[O_CatchGate])
            {
               Scale1BulkWeightDirect = ON;
            }  
         }
         scanmemSTD[1] = systemtimer_ms();
         Output_OFF(O_BulkFeed);
         
         if(MC.DribbleOutOnInBulk)
         {                     
            Output_ON(O_DribbleFeed);
            if(IO_Enable[O_IntermFeed])Output_ON(O_IntermFeed);
         }
         break;
      } 
                           
      if(Scale1CmpBulkActive && !tstcomparator(SCALE_NUMBER,Scale1CmpBulk))
      {
         CmpBulkOn1 = ON;
      }
         
      if(Scale1BulkWeightDirect && NetWeight[Scale1] >= WP.BulkCutoff)
      {
         CmpBulkOn2 = ON;
      }
          
      if(CmpBulkOn1 || CmpBulkOn2)    
      {
         CmpBulkOn1 = OFF;
         CmpBulkOn2 = OFF;
         Scale1BulkWeightDirect = OFF;
         Scale1CmpBulkActive = OFF;  
         BulkDone=ON;
         clrcomparator(SCALE_NUMBER,Scale1CmpBulk);
      }
          
      if(BulkDone)
      {
         if(T_EN[T_BulkDuration])
         {
          ProdBulkDuration[Scale1] = T_Acc(T_BulkDuration);
          T_EnOff(T_BulkDuration);
         
          if(IO_Enable[O_CatchGateInterm])
           {
            setcomparator(SCALE_NUMBER,Scale1CmpCatchGateInterm,IO_Address[O_CatchGateInterm],(WP.IntermCutoff)*UnitIQtoADM,"<",1);
            Output_OFF(O_CatchGateInterm);
           }
         }
         
         if(IO_Enable[O_IntermFeed] && OK4IntermFeed && !AOUTActive[AIO1])
         {
            SeqFeedSTD = stp_FSInterBlank;
            BulkDone=OFF;
            T_EnOn(T_SlowCycleTime);
            Output_OFF(O_CycleInHold);
            T_EnOn(T_DribbleDuration);
         }
         else if((!IO_Enable[O_IntermFeed]||AOUTActive[AIO1]) && OK4DribbleFeed)
         {
            SeqFeedSTD = stp_FSDribbleBlank; 
            BulkDone=OFF;
            T_EnOn(T_SlowCycleTime);
            Output_OFF(O_CycleInHold);
            T_EnOn(T_DribbleDuration);
            break;
         }
         else
         {
          if(!IO_State[O_CycleInHold]) 
          {
           Output_OFF(O_DribbleFeed);
           Output_OFF(O_IntermFeed);
           T_EnOff(T_SlowCycleTime);
           Output_ON(O_CycleInHold);
           MessageDisplay(lsub(TX_MSGCYCLEINHOLD),Step_Message);
          } 
         } 
      }
      else 
         break;
               
  case stp_FSInterBlank:
      if(SeqFeedSTDMEM != SeqFeedSTD) 
      {
         SeqFeedSTDMEM = SeqFeedSTD;
         MessageDisplay(lsub(TX_MSGINTERMBLANK),Step_Message);
         T_EnOn(T_IntermBlankT);
         Output_ON(O_IntermFeed);
         if(MC.DribbleOutOnInBulk) Output_ON(O_DribbleFeed);
         break;
      }
      if(T_Dn(T_IntermBlankT)||T_PRE[T_IntermBlankT]==0 || WP.FeedAlgoType == FeediQSim )
      {
         T_EnOff(T_IntermBlankT);
         SeqFeedSTD = stp_FSInterSetPoint;
      }
      else 
         break;   
              
   case stp_FSInterSetPoint:
      if(SeqFeedSTDMEM != SeqFeedSTD)
      {
         SeqFeedSTDMEM = SeqFeedSTD;             
         MessageDisplay(lsub(TX_MSGINTERMFEED),Step_Message);                
         if(IO_Enable[O_IntermFeed])
         {
            setcomparator(SCALE_NUMBER,Scale1CmpInterm,IO_Address[O_IntermFeed],WP.IntermCutoff*UnitIQtoADM,"<",1);
            Scale1CmpIntermActive = ON;
         } 
         Output_OFF(O_IntermFeed);
         if(WP.StreamRegConfig != StreamRegHigh && MC.FeederType != FeederBHS) StreamRegLower();
         if(MC.DribbleOutOnInBulk)Output_ON(O_DribbleFeed);
         break;
      }   
      
      if(Scale1CmpIntermActive && !tstcomparator(SCALE_NUMBER,Scale1CmpInterm)) 
      {
         IntermDone=ON;
         Scale1CmpIntermActive = OFF;
         clrcomparator(SCALE_NUMBER,Scale1CmpInterm);
      }
      if(IntermDone && OK4DribbleFeed) 
      {
         SeqFeedSTD = stp_FSDribbleBlank;
         IntermDone = OFF;
      }
      else 
         break;
             
   case stp_FSDribbleBlank:
      if(SeqFeedSTDMEM != SeqFeedSTD) 
      {
         SeqFeedSTDMEM = SeqFeedSTD;
         MessageDisplay(lsub(TX_MSGDRIBBLEBLANK),Step_Message);
         T_EnOn(T_DribbleBlankT);
         Output_ON(O_DribbleFeed);
         if(AOUTActive[AIO1])
         {
            if(VibrationEnabled)
            {
               clrcomparator(SCALE_NUMBER,Scale1CmpAout1);
            }
            else
            {
               if(MC.FeederType == FeederGV && !MC.DribbleOutOnInBulk)
               {
                  aout(AIO1,WP.MinFeeding);
               }
               if(MC.FeederType == FeederDS)
               {
                  aout(AIO1,0);
               }  
            }
         }
         if(AOUTActive[AIO2])
         {
            if(MC.FeederType == FeederDS && !MC.DribbleOutOnInBulk)
            {
               aout(AIO2,WP.MinFeeding);
            }
         }     
         if(WP.StreamRegConfig != StreamRegHigh && MC.FeederType != FeederBHS) StreamRegLower();
        break;
      }
      if(VibrationEnabled)
      {
         aout(AIO1,GetAoutVibration());
      }
      if(T_Dn(T_DribbleBlankT)||T_PRE[T_DribbleBlankT]==0 || WP.FeedAlgoType == FeediQSim)
      {
         T_EnOff(T_DribbleBlankT);
         SeqFeedSTD = stp_FSDribbleSetPoint;
      }
      else 
         break;
         
   case stp_FSDribbleSetPoint:
      if(SeqFeedSTDMEM != SeqFeedSTD) 
      {
         SeqFeedSTDMEM = SeqFeedSTD;
         ResumeInDribble = OFF;
       
         if(WP.FeedAlgoType == FeediQSim)
          DribbleSP = TargetWeight[Scale1]*UnitIQtoADM;
         else
          DribbleSP = (WP.Cutoff-CutoffCorrOffset[Scale1])*UnitIQtoADM;
         
         if(!DribbleDone)
         {
            MessageDisplay(lsub(TX_MSGDRIBBLEFEED),Step_Message);
            if(IO_Enable[O_DribbleFeed])
            {
               setcomparator(SCALE_NUMBER,Scale1CmpDribble,IO_Address[O_DribbleFeed],DribbleSP,"<",1);
               Scale1CmpDribbleActive = ON;
            } 
            Output_OFF(O_DribbleFeed);
            
            if(IO_Enable[O_CatchGate])
            if(WP.CatchGateDelay == 0) 
            {
               if(IO_Enable[O_CatchGate])
               {
                  setcomparator(SCALE_NUMBER,Scale1CmpCatchGate,IO_Address[O_CatchGate],DribbleSP,"<",1);
                  Scale1CmpCatchGateActive = ON;
               } 
               Output_OFF(O_CatchGate);
            }
            
            if(SlowCycleEndDribble)
            {
               if(AOUTActive[AIO1])
               {
                  if(MC.FeederType == FeederDS)
                  {
                     aout(AIO1,0);
                  }
                  else
                  {
                     aout(AIO1,WP.MinFeeding);
                  }
               }
               if(AOUTActive[AIO2])
               {
                  if(MC.FeederType == FeederDS)
                  {
                     aout(AIO2,WP.MinFeeding);
                  }
                  if(MC.FeederType == FeederBHS || MC.FeederType == FeederBSFHS)
                  {
                     aout(AIO2,WP.RegMinFeeding);
                  } 
               }     
            }
         }  
      }
      if(VibrationEnabled)
      {
         aout(AIO1,GetAoutVibration());
      }
       
      if((Scale1CmpDribbleActive && !tstcomparator(SCALE_NUMBER,Scale1CmpDribble)) || (Scale1CmpCatchGateActive && !tstcomparator(SCALE_NUMBER,Scale1CmpCatchGate)))
      {
         if(!IO_Enable[O_CatchGate] || WP.CatchGateDelay != 0 || (IO_Enable[O_CatchGate] && Scale1CmpCatchGateActive && !tstcomparator(SCALE_NUMBER,Scale1CmpCatchGate)))
         {
            DribbleDone=ON;
            ProdDribbleDuration[Scale1] = T_Acc(T_DribbleDuration);
            T_EnOff(T_DribbleDuration);
            T_EnOff(T_VibrationPeriod);
            VibrationMem = OFF;
            if(IO_Enable[O_CatchGate]) 
            if(WP.CatchGateDelay == 0)
            {
               Scale1CmpCatchGateActive = OFF;
               clrcomparator(SCALE_NUMBER,Scale1CmpCatchGate);
            }
            else 
            {    
               T_EnOn(T_CatchGateDelay);
            }
            if(IO_Enable[O_CatchGateInterm])
            {
             clrcomparator(SCALE_NUMBER,Scale1CmpCatchGateInterm);
             Output_OFF(O_CatchGateInterm);
            }
            Scale1CmpDribbleActive = OFF;    
            clrcomparator(SCALE_NUMBER,Scale1CmpDribble);   
         }
      }      
        
      if(DribbleDone)
      {
         if(!IO_Enable[O_CatchGate] || WP.CatchGateDelay == 0 || (WP.CatchGateDelay < CATCHGATEDELAYMAX && (T_Dn(T_CatchGateDelay) || !T_EN[T_CatchGateDelay])) 
             ||  WP.CatchGateDelay >= CATCHGATEDELAYMAX)
         {
            T_EnOff(T_CatchGateDelay);
            FeedSTDReset();
            SeqFeedSTD = stp_FSDone;
         }
      }
      break;
           
   case stp_FSDone:
      if(SeqFeedSTDMEM != SeqFeedSTD)
      {
         SeqFeedSTDMEM = SeqFeedSTD;
         if(!BulkOnTime[Scale1] && !DribbleOnly)
         {
            BulkBlankLimit = T_PRE[T_BulkBlankT] + 0.15;
         
            if(ProdBulkDuration[Scale1] < BulkBlankLimit && WP.FeedAlgoType != FeediQSim)
            {
               if(!WeighmentAborted)
               {
                  FaultHandler(FltW_BulkBlankTooLong,"");
                  DisplayAlarmDelay = ON;
               } 
               CheckOnce = ON;
            }
         }
         
         DribbleBlankLimit = T_PRE[T_DribbleBlankT] + 0.15;
         if(ProdDribbleDuration[Scale1] < DribbleBlankLimit && WP.FeedAlgoType != FeediQSim) 
         {
            if(!WeighmentAborted)
            {
               FaultHandler(FltW_DribbleBlankTooLong,"");
               DisplayAlarmDelay = ON;
            } 
            CheckOnce = ON;
         }
        
        if(WP.StreamRegConfig == StreamRegAutoRaiseAtEnd && IO_Enable[O_StreamRegulator] && MC.FeederType != FeederBHS) 
          StreamRegRaise();
      }
      if(DribbleDone && SeqFeeder == stp_FEEDDONE)
      {
         DribbleDone=OFF;
         
         if(MC.FeederType == FeederB && WP.PostFillingTime != 0 && SeqModeManager == stp_AUTO_MODE && IO_Enable[O_CatchGate])
         {
            // Post-filling needed
            SeqFeedSTD = stp_FSFeedDelay;
         }
         else
         {
            SeqFeedSTD = stp_INIT;
         }
      }
      break;
      
   case stp_FSFeedDelay:
      if(SeqFeedSTDMEM != SeqFeedSTD)
      {
         SeqFeedSTDMEM = SeqFeedSTD;
         if(WP.FeedDelayTime != 0)
         {
            T_EnOn(T_FeedDelay);
         }
         else
         {
            SeqFeedSTD = stp_FSPostFilling;
         }
         Output_ON(O_StreamRegulator);
      }
      if(T_Dn(T_FeedDelay))
      {
         T_EnOff(T_FeedDelay);
         SeqFeedSTD = stp_FSPostFilling;
      }
      break;
      
   case stp_FSPostFilling:
      if(SeqFeedSTDMEM != SeqFeedSTD)
      {
         SeqFeedSTDMEM = SeqFeedSTD;
         T_EnOn(T_PostFillingTime);
         Output_ON(O_BulkFeed);
      }
      if(T_Dn(T_PostFillingTime))
      {
         Output_OFF(O_BulkFeed);
         Output_OFF(O_StreamRegulator);
         T_EnOff(T_PostFillingTime);
         SeqFeedSTD = stp_INIT;
      }
      break;
       
   case stp_FSHold:
      if(SeqFeedSTDMEM != SeqFeedSTD)SeqFeedSTDMEM = SeqFeedSTD;
         break;     
   }
}



/*------------------------------------------------------------------------- 
--  Procedure Name    : Kicker
--  Created by        : Steve Santerre
--  Last modified on  : 2017/02
--  
--  Purpose           : Kicker Sequence
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void SEQ_Kicker(void)
{
if(SeqModeManager == stp_OFF_MODE) SeqKicker = stp_INIT;

   SeqKickerNext = SeqKicker;
   switch (SeqKicker)
   {
   case stp_INIT:
      if(SeqKicker!=SeqKickerMEM)
      {
         SeqKickerMEM = SeqKicker;
         KickerRetractedOffMem=OFF;
      }
      if(IO_Enable[O_KickerActivate] && (SeqModeManager==stp_AUTO_MODE||SeqModeManager==stp_STOPPING_MODE))
      {
         if(Input_State(I_BagAtKicker) && !BagAtKickerMem)
         {
            BagAtKickerMem=ON;
            SeqKicker = stp_CENTERBAG;
         }
         else if(!Input_State(I_BagAtKicker) && BagAtKickerMem)
         {
            BagAtKickerMem=OFF;
            break;
         }
         else break;    
      }
      break;
               

   case stp_CENTERBAG:
      if(SeqKicker!=SeqKickerMEM)
      {
         SeqKickerMEM = SeqKicker;
      }
      if(!T_EN[T_KickerCenterTime] && T_PRE[T_KickerCenterTime] > 0 ) 
      {
         T_EnOn(T_KickerCenterTime);
         break;
      }
      
      if(T_Dn(T_KickerCenterTime) || T_PRE[T_KickerCenterTime] == 0  )
      {
         T_EnOff(T_KickerCenterTime);
         SeqKicker = stp_KICKBAG;
      }
      else break;
       
   case stp_KICKBAG:
      if(SeqKicker!=SeqKickerMEM)
      {
         SeqKickerMEM = SeqKicker;
         Output_ON(O_KickerActivate);
      }
      if(!T_EN[T_KickerActivateTime] && T_PRE[T_KickerActivateTime] > 0 ) 
      {
         T_EnOn(T_KickerActivateTime);
         break;
      }
      
      if(T_PRE[T_KickerActivateTime] > 0)
      {  
         if(T_Dn(T_KickerActivateTime))
         {
            Output_OFF(O_KickerActivate);
            T_EnOff(T_KickerActivateTime);
            SeqKicker = stp_INIT;
         }
      }
      else
      {
         if(Input_State(I_KickerRetracted) && KickerRetractedOffMem)
         {
            Output_OFF(O_KickerActivate);
            SeqKicker = stp_INIT;
         }
         if(!Input_State(I_KickerRetracted))
         {
            KickerRetractedOffMem=ON;
            break;
         }
      }
      break;         
   }   
}   

/*------------------------------------------------------------------------- 
--  Procedure Name    : SEQ_LockingDevice
--  Created by        : Frédéric Allard
--  Last modified on  : 2018/02
--  
--  Purpose           : Sequence for the locking device
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void SEQ_LockingDevice(void)
{
   switch(SeqLockingDevice)
   {
   case stp_INIT:
      SeqLockingDeviceMEM = SeqLockingDevice;
      
      if(IO_Enable[O_LockingDevice])
      {
         Output_OFF(O_LockingDevice);
         SeqLockingDevice = stp_LDREMOVING;
      }
      else
      {
         SeqLockingDevice = stp_LDOFF;
      }
      break;
      
   case stp_LDREMOVING:
      if(SeqLockingDevice != SeqLockingDeviceMEM)
      {
         SeqLockingDeviceMEM = SeqLockingDevice;
         if(T_EN[T_LockingDeviceTime])
         {
            T_EnOff(T_LockingDeviceTime);
         }
      }
      if(T_Dn(T_LockingDeviceTime) || MP.LockingDeviceTime == 0)
      {
         T_EnOff(T_LockingDeviceTime);
         SeqLockingDevice = stp_LDOFF;
      }
      else if(!T_EN[T_LockingDeviceTime])
      {
         T_EnOn(T_LockingDeviceTime);
      }
      else if(StepMsg != lsub(TX_MSGREMOVINGLOCKINGDEVICE) && (SeqModeManager == stp_PREP_MODE || SeqModeManager == stp_AUTO_MODE))
      {
         MessageDisplay(lsub(TX_MSGREMOVINGLOCKINGDEVICE),Step_Message);
      }
      break;
      
   case stp_LDOFF:
      if(SeqLockingDeviceMEM != SeqLockingDevice)
      {
         SeqLockingDeviceMEM = SeqLockingDevice;
      }
      if(RemoveLockingDevice)
      {
         // Already removed
         RemoveLockingDevice = OFF;
      }
      if(ApplyLockingDevice)
      {
         Output_ON(O_LockingDevice);
         SeqLockingDevice = stp_LDAPPLYING;
      }
      break;
      
   case stp_LDAPPLYING:
      if(SeqLockingDevice != SeqLockingDeviceMEM)
      {
         SeqLockingDeviceMEM = SeqLockingDevice;
         if(T_EN[T_LockingDeviceTime])
         {
            T_EnOff(T_LockingDeviceTime);
         }
      }
      if(T_Dn(T_LockingDeviceTime) || MP.LockingDeviceTime == 0)
      {
         T_EnOff(T_LockingDeviceTime);
         SeqLockingDevice = stp_LDON;
      }
      else if(!T_EN[T_LockingDeviceTime])
      {
         T_EnOn(T_LockingDeviceTime);
      }
      else if(StepMsg != lsub(TX_MSGAPPLYINGLOCKINGDEVICE) && (SeqModeManager == stp_AUTO_MODE || SeqModeManager == stp_PREP_MODE))
      {
         MessageDisplay(lsub(TX_MSGAPPLYINGLOCKINGDEVICE),Step_Message);
      }
      break;
      
   case stp_LDON:
      if(SeqLockingDeviceMEM != SeqLockingDevice)
      {
         SeqLockingDeviceMEM = SeqLockingDevice;
      }
      if(ApplyLockingDevice)
      {
         // Already applied
         ApplyLockingDevice = OFF;
      }
      if(RemoveLockingDevice)
      {
         Output_OFF(O_LockingDevice);
         SeqLockingDevice = stp_LDREMOVING;
      }
      break;
   }
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : SEQ_Lance
--  Created by        : Frédéric Allard
--  Last modified on  : 2018/02
--  
--  Purpose           : Sequence for the lance
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/
void SEQ_Lance(void)
{
   if(ResetLance)
   {
      SeqLance = stp_INIT;
   }
   
   switch(SeqLance)
   {
   case  stp_INIT:
      if(SeqLanceMEM != SeqLance)
      {
         SeqLanceMEM = SeqLance;
      }
      if(ResetLance)
      {
         ResetLance = OFF;
         Output_OFF(O_LanceVacuum);
         Output_OFF(O_BlowAir);
         Output_OFF(O_LowerEvacuationLance);
         Output_OFF(O_RaiseEvacuationLance);
         T_EnOff(T_EvacuationDelay);
         T_EnOff(T_EvacuationTime);
         T_EnOff(T_LanceBlowOffTime);
         SeqLance = stp_LANCERISING;
      }
      break;
      
   case stp_LANCERISING:
      if(SeqLanceMEM != SeqLance)
      {
         Output_OFF(O_LowerEvacuationLance);
         Output_ON(O_RaiseEvacuationLance);
         Output_OFF(O_LanceVacuum);
         Output_OFF(O_BlowAir);
         
         if(!IO_Enable[I_LanceHighPosition])
         {
            // No high position sensor, assume lance is high immediatly
            SeqLance = stp_LANCEHIGH;
         }
         else
         {
            if(Input_State(I_LanceHighPosition))
            {
               SeqLance = stp_LANCEHIGH;
            }
            else if(SeqModeManager == stp_AUTO_MODE)
            {
               MessageDisplay(lsub(TX_MSGLANCEUP),Step_Message);
            }
         }
         SeqLanceMEM = SeqLance;
      }
      if(Input_State(I_LanceHighPosition))
      {
         SeqLance = stp_LANCEHIGH;
      }
      break;
      
   case stp_LANCEDOWN:
      if(SeqLanceMEM != SeqLance)
      {
         SeqLanceMEM = SeqLance;
         Output_ON(O_LowerEvacuationLance);
         if(WP.EvacuationDelay > 0)
         {
            T_EnOn(T_EvacuationDelay);
            if(SeqModeManager == stp_AUTO_MODE)
            {
               MessageDisplay(lsub(TX_MSGLANCEDOWN),Step_Message);
            }
         }
         else
         {
            SeqLance = stp_LANCEVACUUM;
         }
      }
      if(T_EN[T_EvacuationDelay] && T_Dn(T_EvacuationDelay))
      {
         // Lance is down, start evacuation
         SeqLance = stp_LANCEVACUUM;
         T_EnOff(T_EvacuationDelay);
      }
      break;
   
   case stp_LANCEVACUUM:
      if(SeqLanceMEM != SeqLance)
      {
         SeqLanceMEM = SeqLance;
         Output_ON(O_LanceVacuum);
         if(WP.EvacuationTime > 0)
         {
            T_EnOn(T_EvacuationTime);
            if(SeqModeManager == stp_AUTO_MODE)
            {
               MessageDisplay(lsub(TX_MSGLANCEEVACUATION),Step_Message);
            }
         }
         else
         {
            SeqLance = stp_LANCEBLOWAIR;
         }
      }
      if(T_EN[T_EvacuationTime] && T_Dn(T_EvacuationTime))
      {
         SeqLance = stp_LANCEBLOWAIR;
         T_EnOff(T_EvacuationTime);
      }
      break;
      
   case stp_LANCEBLOWAIR:
      if(SeqLanceMEM != SeqLance)
      {
         SeqLanceMEM = SeqLance;
         Output_OFF(O_LanceVacuum);
         Output_ON(O_BlowAir);
         if(WP.LanceBlowOffTime > 0)
         {
            T_EnOn(T_LanceBlowOffTime);
            if(SeqModeManager == stp_AUTO_MODE)
            {
               MessageDisplay(lsub(TX_MSGLANCEBLOWAIR),Step_Message);
            }
         }
         else
         {
            SeqLance = stp_LANCERISING;
         }
      }
      if(T_EN[T_LanceBlowOffTime] && T_Dn(T_LanceBlowOffTime))
      {
         SeqLance = stp_LANCERISING;
         T_EnOff(T_LanceBlowOffTime);
      }
      break;

   case stp_LANCEHIGH:
      if(SeqLanceMEM != SeqLance)
      {
         SeqLanceMEM = SeqLance;
         if(IO_Enable[I_LanceHighPosition])
         {
            Output_OFF(O_RaiseEvacuationLance);
         }
      }
      break;
   }
}

/*------------------------------------------------------------------------- 
--  Procedure Name    : ReleaseAcknowledgeCheck
--  Created by        : Frédéric Allard
--  Last modified on  : 2018/02
--  
--  Purpose           : Called in filling task. Monitors the release acknowledge
--                      input and sets the ReleaseAcknowledge boolean when the
--                      downstream equipment has received the previous bag.
--                      
--  Value Parameters  :
--  Variable Params   : none
--  Side Effects      : 
--------------------------------------------------------------------------*/

void ReleaseAcknowledgeCheck(void)
{
   if(IO_Enable[I_ReleaseAcknowledge])
   {
      if(Input_State(I_ReleaseAcknowledge))
      {
         if(!ReleaseAcknowledgeInputMEM)
         {
            // Rising edge of release acknowledge input
            ReleaseAcknowledge = ON;
         }
         ReleaseAcknowledgeInputMEM = ON;
      }
      else
      {
         ReleaseAcknowledgeInputMEM = OFF;
      }
   }
   else
   {
      ReleaseAcknowledge = ON;
   }
}                      