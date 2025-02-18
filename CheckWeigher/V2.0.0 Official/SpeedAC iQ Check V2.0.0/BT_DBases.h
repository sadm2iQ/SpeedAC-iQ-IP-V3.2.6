﻿
//Database Names
#define TX_DBWEIGHPROGRAM         "Weigh Program"
#define TX_DBMACHINECONFIG        "Machine Configuration"
#define TX_DBMACHINEPARAM         "Machine Parameters"


//Weigh Program Data base
dbase TX_DBWEIGHPROGRAM WPdb[WPdbSIZE]
{
      int       No;                    selectmode=input;
      string    Name[WP_NAME_LENGTH];
      decimal   NominalWeight%6.2;
      decimal   TolUnderAccWeight%6.2;
      decimal   TolUnderRejWeight%6.2;
      decimal   TolOverAccWeight%6.2;
      decimal   TolOverRejWeight%6.2;
      decimal   TolUnderAccCnt%10.0;
      decimal   TolUnderRejCnt%10.0;
      decimal   TolOverAccCnt%10.0;
      decimal   TolOverRejCnt%10.0;
      decimal   TolGoodCnt%10.0;
      decimal   TolInvalidCnt%10.0;
      decimal   TolTotalCnt%10.0;
      decimal   TolUnderRejPct%5.1;
      decimal   TolUnderAccPct%5.1;
      decimal   TolGoodPct%5.1;
      decimal   TolOverAccPct%5.1;
      decimal   TolOverRejPct%5.1;
      decimal   TolInvalidPct%5.1;
      decimal   TolTotalPct%5.1;
      string    TotalWeight[14];
      string    Unit[4];        //enum list kg-lb
      decimal   FlexSP1%6.2;
      decimal   FlexSP2%6.2;
      decimal   FlexT1%5.2;
      decimal   FlexT2%5.2;
      decimal   FlexCounter%6.0;
      float     BagLength;      //mm
      decimal   TareWeight%4.2;
      int       RejectCfg;
      decimal   RejectUnitDelay%4.2;
      decimal   RejectUnitTime%4.2;
      int       PrimaryFilter;
      int       EvaluationFilter;
      int       ZeroFilter;
      decimal   DynamicOffset%5.3;
}

/**************************************************************************** 
  WP Name list
*****************************************************************************/
dbase  WPNameList[MAX_WP]
{
    int       No;                    selectmode=input;
    string    Name[WP_NAME_LENGTH];
    decimal   NominalWeight%6.2;
    string    Unit[4];
}

/*********************************************** */
/*                                               */
/*  Text definitions moved to Language.h file    */
/*                                               */
/* ********************************************* */


//Machine Config Data base
dbase TX_DBMACHINECONFIG MCdb[MAX_MC]
{
      int     No;                    selectmode=input; 
      string  Name[24];      
      int     NumberOfWP;
      bool    OIML;                       //enum list yes-no   
      int     Language1;                  
      string  Lang1Text[24];                  
      int     Language2;                  
      string  Lang2Text[24];
      int     Language3;                  
      string  Lang3Text[24];
      int     PortCOM2Cfg;    
      int     PortUSBCfg;
      int     PortETHCfg;
      string  PrinterIP[15];              //IP address of printer xxx.xxx.xxx.xxx
      string  SerialNbr[24];
      string  PlantLineName[24];
      string  CustomerName[24];
      string  CustomerLocation[24];      
      string  SupportPhoneNbr[24];
      string  SupportFaxNbr[24];
      string  SupportEmail[24];
      string  PartsPhoneNbr[24];
      string  PartsFaxNbr[24];
      string  PartsEmail[24];
      string  iQVersion[30];
      int     TrialDays;
      bool    System; //Metric(0)-Imperial(1)
}



//Machine Param Data base
dbase TX_DBMACHINEPARAM MPdb[MAX_MP]
{
      int     No;                  selectmode=input;
      string  Name[24];    
      decimal FlexCounter%6.0;
      decimal FlexSP1%6.2;
      decimal FlexSP2%6.2;
      decimal FlexT1%5.2;
      decimal FlexT2%5.2;
      int     ScreenSaverTime;
      float   PhotocellsDistance;
      float   BeltSpeed;
      decimal PCEntryTimeout%5.2;
      decimal PCExitTimeout%5.2;
      bool    AutoTuneCfg;
}

