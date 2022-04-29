// EnergyPlus, Copyright (c) 1996-2022, The Board of Trustees of the University of Illinois,
// The Regents of the University of California, through Lawrence Berkeley National Laboratory
// (subject to receipt of any required approvals from the U.S. Dept. of Energy), Oak Ridge
// National Laboratory, managed by UT-Battelle, Alliance for Sustainable Energy, LLC, and other
// contributors. All rights reserved.
//
// NOTICE: This Software was developed under funding from the U.S. Department of Energy and the
// U.S. Government consequently retains certain rights. As such, the U.S. Government has been
// granted for itself and others acting on its behalf a paid-up, nonexclusive, irrevocable,
// worldwide license in the Software to reproduce, distribute copies to the public, prepare
// derivative works, and perform publicly and display publicly, and to permit others to do so.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice, this list of
//     conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright notice, this list of
//     conditions and the following disclaimer in the documentation and/or other materials
//     provided with the distribution.
//
// (3) Neither the name of the University of California, Lawrence Berkeley National Laboratory,
//     the University of Illinois, U.S. Dept. of Energy nor the names of its contributors may be
//     used to endorse or promote products derived from this software without specific prior
//     written permission.
//
// (4) Use of EnergyPlus(TM) Name. If Licensee (i) distributes the software in stand-alone form
//     without changes from the version obtained under this License, or (ii) Licensee makes a
//     reference solely to the software portion of its product, Licensee must refer to the
//     software as "EnergyPlus version X" software, where "X" is the version number Licensee
//     obtained under this License and may not use a different name for the software. Except as
//     specifically required in this Section (4), Licensee shall not use in a company name, a
//     product name, in advertising, publicity, or other promotional activities any name, trade
//     name, trademark, logo, or other designation of "EnergyPlus", "E+", "e+" or confusingly
//     similar designation, without the U.S. Department of Energy's prior written consent.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// C++ Headers

// ObjexxFCL Headers
#include <ObjexxFCL/Array.functions.hh>
#include <ObjexxFCL/Optional.hh>

// EnergyPlus Headers
#include <EnergyPlus/Autosizing/Base.hh>
#include <EnergyPlus/BranchNodeConnections.hh>
#include <EnergyPlus/CurveManager.hh>
#include <EnergyPlus/DXCoils.hh>
#include <EnergyPlus/Data/EnergyPlusData.hh>
#include <EnergyPlus/DataAirSystems.hh>
#include <EnergyPlus/DataContaminantBalance.hh>
#include <EnergyPlus/DataEnvironment.hh>
#include <EnergyPlus/DataHVACGlobals.hh>
#include <EnergyPlus/DataHeatBalance.hh>
#include <EnergyPlus/DataSizing.hh>
#include <EnergyPlus/DataWater.hh>
#include <EnergyPlus/Fans.hh>
#include <EnergyPlus/FluidProperties.hh>
#include <EnergyPlus/General.hh>
#include <EnergyPlus/GeneralRoutines.hh>
#include <EnergyPlus/GlobalNames.hh>
#include <EnergyPlus/HVACFan.hh>
#include <EnergyPlus/InputProcessing/InputProcessor.hh>
#include <EnergyPlus/NodeInputManager.hh>
#include <EnergyPlus/OutAirNodeManager.hh>
#include <EnergyPlus/OutputProcessor.hh>
#include <EnergyPlus/OutputReportPredefined.hh>
#include <EnergyPlus/PlantUtilities.hh>
#include <EnergyPlus/Psychrometrics.hh>
#include <EnergyPlus/ReportCoilSelection.hh>
#include <EnergyPlus/ScheduleManager.hh>
#include <EnergyPlus/UnitarySystem.hh>
#include <EnergyPlus/VariableSpeedCoils.hh>
#include <EnergyPlus/WaterManager.hh>

namespace EnergyPlus {

namespace VariableSpeedCoils {

    // Using/Aliasing
    using namespace DataLoopNode;
    using namespace Psychrometrics;
    using namespace DataSizing;
    using namespace DataHVACGlobals;

    using DXCoils::AdjustCBF;
    using DXCoils::CalcCBF;

    Real64 constexpr RatedInletAirTemp = 26.6667;       // 26.6667C or 80F
    Real64 constexpr RatedInletWetBulbTemp = 19.4444;   // 19.44 or 67F, cooling mode
    Real64 constexpr RatedInletAirHumRat = 0.0111847;   // Humidity ratio corresponding to 80F dry bulb/67F wet bulb
    Real64 constexpr RatedInletWaterTemp = 29.4444;     // 85 F cooling mode
    Real64 constexpr RatedAmbAirTemp = 35.0;            // 95 F cooling mode
    Real64 constexpr RatedInletAirTempHeat = 21.1111;   // 21.11C or 70F, heating mode
    Real64 constexpr RatedInletWaterTempHeat = 21.1111; // 21.11C or 70F, heating mode
    Real64 constexpr RatedAmbAirTempHeat = 8.3333;      // 8.33 or 47F, heating mode
    Real64 constexpr RatedAmbAirWBHeat = 6.1111;        // 8.33 or 43F, heating mode, rated wet bulb temperature
                                                        // Water Systems
    int constexpr CondensateDiscarded = 1001;           // default mode where water is "lost"
    int constexpr CondensateToTank = 1002;              // collect coil condensate from air and store in water storage tank

    int constexpr WaterSupplyFromMains = 101;
    int constexpr WaterSupplyFromTank = 102;

    // Defrost strategy (heat pump only)
    int constexpr ReverseCycle = 1; // uses reverse cycle defrost strategy
    int constexpr Resistive = 2;    // uses electric resistance heater for defrost
                                    // Defrost control  (heat pump only)
    int constexpr Timed = 1;        // defrost cycle is timed
    int constexpr OnDemand = 2;     // defrost cycle occurs only when required

    void SimVariableSpeedCoils(EnergyPlusData &state,
                               std::string_view CompName,              // Coil Name
                               int &CompIndex,                         // Index for Component name
                               int const CyclingScheme,                // Continuous fan OR cycling compressor
                               Real64 &MaxONOFFCyclesperHour,          // Maximum cycling rate of heat pump [cycles/hr]
                               Real64 &HPTimeConstant,                 // Heat pump time constant [s]
                               Real64 &FanDelayTime,                   // Fan delay time, time delay for the HP's fan to
                               CompressorOperation const CompressorOp, // compressor on/off. 0 = off; 1= on
                               Real64 const PartLoadFrac,
                               int const SpeedNum,                    // compressor speed number
                               Real64 const SpeedRatio,               // compressor speed ratio
                               Real64 const SensLoad,                 // Sensible demand load [W]
                               Real64 const LatentLoad,               // Latent demand load [W]
                               Optional<Real64 const> OnOffAirFlowRat // ratio of comp on to comp off air flow rate
    )
    {

        //       AUTHOR         Bo Shen, ORNL
        //       DATE WRITTEN   March 2012
        //       MODIFIED       Bo Shen, 12/2014, add variable-speed HPWH
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine manages variable-speed Water to Air Heat Pump component simulation.

        // Using/Aliasing
        using FluidProperties::FindGlycol;
        using General::SolveRoot;

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int DXCoilNum;            // The WatertoAirHP that you are currently loading input into
        Real64 OnOffAirFlowRatio; // ratio of comp on to comp off air flow rate
        Real64 RuntimeFrac;       // run time fraction
        int SpeedCal;             // variable for error proof speed input

        // Obtains and Allocates WatertoAirHP related parameters from input file
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        if (CompIndex == 0) {
            DXCoilNum = UtilityRoutines::FindItemInList(CompName, state.dataVariableSpeedCoils->VarSpeedCoil);
            if (DXCoilNum == 0) {
                ShowFatalError(state, "WaterToAirHPVSWEquationFit not found=" + std::string{CompName});
            }
            CompIndex = DXCoilNum;
        } else {
            DXCoilNum = CompIndex;
            if (DXCoilNum > state.dataVariableSpeedCoils->NumVarSpeedCoils || DXCoilNum < 1) {
                ShowFatalError(state,
                               format("SimVariableSpeedCoils: Invalid CompIndex passed={}, Number of Water to Air HPs={}, WaterToAir HP name={}",
                                      DXCoilNum,
                                      state.dataVariableSpeedCoils->NumVarSpeedCoils,
                                      CompName));
            }
            if (!CompName.empty() && CompName != state.dataVariableSpeedCoils->VarSpeedCoil(DXCoilNum).Name) {
                ShowFatalError(
                    state,
                    format("SimVariableSpeedCoils: Invalid CompIndex passed={}, WaterToAir HP name={}, stored WaterToAir HP Name for that index={}",
                           DXCoilNum,
                           CompName,
                           state.dataVariableSpeedCoils->VarSpeedCoil(DXCoilNum).Name));
            }
        }

        if (present(OnOffAirFlowRat)) {
            OnOffAirFlowRatio = OnOffAirFlowRat;
        } else {
            OnOffAirFlowRatio = 1.0;
        }

        auto &VarSpeedCoil = state.dataVariableSpeedCoils->VarSpeedCoil(DXCoilNum);

        // ERROR PROOF
        if (SpeedNum < 1) {
            SpeedCal = 1;
        } else {
            SpeedCal = SpeedNum;
        }

        if ((VarSpeedCoil.VSCoilTypeOfNum == DataHVACGlobals::Coil_CoolingWaterToAirHPVSEquationFit) ||
            (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed)) {
            // Cooling mode
            InitVarSpeedCoil(state,
                             DXCoilNum,
                             MaxONOFFCyclesperHour,
                             HPTimeConstant,
                             FanDelayTime,
                             SensLoad,
                             LatentLoad,
                             CyclingScheme,
                             OnOffAirFlowRatio,
                             SpeedRatio,
                             SpeedCal);
            CalcVarSpeedCoilCooling(state,
                                    DXCoilNum,
                                    CyclingScheme,
                                    RuntimeFrac,
                                    SensLoad,
                                    LatentLoad,
                                    CompressorOp,
                                    PartLoadFrac,
                                    OnOffAirFlowRatio,
                                    SpeedRatio,
                                    SpeedCal);
            UpdateVarSpeedCoil(state, VarSpeedCoil);
        } else if ((VarSpeedCoil.VSCoilTypeOfNum ==
                    DataHVACGlobals::Coil_HeatingWaterToAirHPVSEquationFit) ||
                   (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed)) {
            // Heating mode
            InitVarSpeedCoil(state,
                             DXCoilNum,
                             MaxONOFFCyclesperHour,
                             HPTimeConstant,
                             FanDelayTime,
                             SensLoad,
                             LatentLoad,
                             CyclingScheme,
                             OnOffAirFlowRatio,
                             SpeedRatio,
                             SpeedCal);
            CalcVarSpeedCoilHeating(
                state, VarSpeedCoil, CyclingScheme, RuntimeFrac, SensLoad, CompressorOp, PartLoadFrac, OnOffAirFlowRatio, SpeedRatio, SpeedCal);
            UpdateVarSpeedCoil(state, VarSpeedCoil);
        } else if (VarSpeedCoil.VSCoilTypeOfNum == CoilDX_HeatPumpWaterHeaterVariableSpeed) {
            // Heating mode
            InitVarSpeedCoil(state,
                             DXCoilNum,
                             MaxONOFFCyclesperHour,
                             HPTimeConstant,
                             FanDelayTime,
                             SensLoad,
                             LatentLoad,
                             CyclingScheme,
                             OnOffAirFlowRatio,
                             SpeedRatio,
                             SpeedCal);
            CalcVarSpeedHPWH(state, VarSpeedCoil, RuntimeFrac, PartLoadFrac, SpeedRatio, SpeedNum, CyclingScheme);
            UpdateVarSpeedCoil(state, VarSpeedCoil);
        } else {
            ShowFatalError(state, "SimVariableSpeedCoils: WatertoAir heatpump not in either HEATING or COOLING mode");
        }

        // two additional output variables
        VarSpeedCoil.SpeedNumReport = SpeedCal;
        VarSpeedCoil.SpeedRatioReport = SpeedRatio;
    }

    void GetVarSpeedCoilInput(EnergyPlusData &state)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Bo Shen
        //       DATE WRITTEN   March, 2012
        //       MODIFIED       Bo Shen, 12/2014, add variable-speed HPWH
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // Obtains input data for HPs and stores it in HP data structures

        // METHODOLOGY EMPLOYED:
        // Uses "Get" routines to read in data.

        // Using/Aliasing
        using namespace NodeInputManager;
        using BranchNodeConnections::TestCompSet;
        using GlobalNames::VerifyUniqueCoilName;
        using namespace OutputReportPredefined;
        using CurveManager::CurveValue;
        using CurveManager::GetCurveIndex;
        using CurveManager::SetCurveOutputMinMaxValues;

        using OutAirNodeManager::CheckOutAirNodeNumber;
        using ScheduleManager::GetScheduleIndex;
        using WaterManager::SetupTankDemandComponent;
        using WaterManager::SetupTankSupplyComponent;

        // SUBROUTINE PARAMETER DEFINITIONS:
        static constexpr std::string_view RoutineName("GetVarSpeedCoilInput: "); // include trailing blank space

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int DXCoilNum;         // The Water to Air HP that you are currently loading input into
        int NumCool;           // Counter for cooling coil, water source
        int NumCoolAS;         // Counter for cooling coil, air source
        int NumHeat;           // Counter for heating coil, water source
        int NumHeatAS;         // Counter for heating coil, air source
        int NumHPWHAirToWater; // counter for air source HPWH
        int CoilCounter;       // Counter
        int I;                 // Loop index increment
        int NumAlphas;         // Number of variables in String format
        int NumNums;           // Number of variables in Numeric format
        int NumParams;         // Total number of input fields
        int MaxNums(0);        // Maximum number of numeric input fields
        int MaxAlphas(0);      // Maximum number of alpha input fields
        int IOStat;
        int AlfaFieldIncre;              // increment number of Alfa field
        bool ErrorsFound(false);         // If errors detected in input
        Real64 CurveVal;                 // Used to verify modifier curves equal 1 at rated conditions
        Real64 WHInletAirTemp;           // Used to pass proper inlet air temp to HPWH DX coil performance curves
        Real64 WHInletWaterTemp;         // Used to pass proper inlet water temp to HPWH DX coil performance curves
        std::string CurrentModuleObject; // for ease in getting objects
        Array1D_string AlphArray;        // Alpha input items for object
        Array1D_string cAlphaFields;     // Alpha field names
        Array1D_string cNumericFields;   // Numeric field names
        Array1D<Real64> NumArray;        // Numeric input items for object
        Array1D_bool lAlphaBlanks;       // Logical array, alpha field input BLANK = .TRUE.
        Array1D_bool lNumericBlanks;     // Logical array, numeric field input BLANK = .TRUE.

        NumCool = state.dataInputProcessing->inputProcessor->getNumObjectsFound(state, "COIL:COOLING:WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT");
        NumHeat = state.dataInputProcessing->inputProcessor->getNumObjectsFound(state, "COIL:HEATING:WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT");
        NumCoolAS = state.dataInputProcessing->inputProcessor->getNumObjectsFound(state, "COIL:COOLING:DX:VARIABLESPEED");
        NumHeatAS = state.dataInputProcessing->inputProcessor->getNumObjectsFound(state, "COIL:HEATING:DX:VARIABLESPEED");
        NumHPWHAirToWater =
            state.dataInputProcessing->inputProcessor->getNumObjectsFound(state, "COIL:WATERHEATING:AIRTOWATERHEATPUMP:VARIABLESPEED");
        state.dataVariableSpeedCoils->NumVarSpeedCoils = NumCool + NumHeat + NumCoolAS + NumHeatAS + NumHPWHAirToWater;
        DXCoilNum = 0;

        if (state.dataVariableSpeedCoils->NumVarSpeedCoils <= 0) {
            ShowSevereError(state, "No Equipment found in GetVarSpeedCoilInput");
            ErrorsFound = true;
        }

        // Allocate Arrays
        if (state.dataVariableSpeedCoils->NumVarSpeedCoils > 0) {
            state.dataVariableSpeedCoils->VarSpeedCoil.allocate(state.dataVariableSpeedCoils->NumVarSpeedCoils);
            state.dataHeatBal->HeatReclaimVS_DXCoil.allocate(state.dataVariableSpeedCoils->NumVarSpeedCoils);
        }

        state.dataInputProcessing->inputProcessor->getObjectDefMaxArgs(
            state, "COIL:COOLING:WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT", NumParams, NumAlphas, NumNums);
        MaxNums = max(MaxNums, NumNums);
        MaxAlphas = max(MaxAlphas, NumAlphas);
        state.dataInputProcessing->inputProcessor->getObjectDefMaxArgs(
            state, "COIL:HEATING:WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT", NumParams, NumAlphas, NumNums);
        MaxNums = max(MaxNums, NumNums);
        MaxAlphas = max(MaxAlphas, NumAlphas);

        state.dataInputProcessing->inputProcessor->getObjectDefMaxArgs(state, "COIL:COOLING:DX:VARIABLESPEED", NumParams, NumAlphas, NumNums);
        MaxNums = max(MaxNums, NumNums);
        MaxAlphas = max(MaxAlphas, NumAlphas);
        state.dataInputProcessing->inputProcessor->getObjectDefMaxArgs(state, "COIL:HEATING:DX:VARIABLESPEED", NumParams, NumAlphas, NumNums);
        MaxNums = max(MaxNums, NumNums);
        MaxAlphas = max(MaxAlphas, NumAlphas);

        // variable speed air-source HPWH
        state.dataInputProcessing->inputProcessor->getObjectDefMaxArgs(
            state, "COIL:WATERHEATING:AIRTOWATERHEATPUMP:VARIABLESPEED", NumParams, NumAlphas, NumNums);
        MaxNums = max(MaxNums, NumNums);
        MaxAlphas = max(MaxAlphas, NumAlphas);

        AlphArray.allocate(MaxAlphas);
        cAlphaFields.allocate(MaxAlphas);
        lAlphaBlanks.dimension(MaxAlphas, true);
        cNumericFields.allocate(MaxNums);
        lNumericBlanks.dimension(MaxNums, true);
        NumArray.dimension(MaxNums, 0.0);

        // Get the data for cooling coil, WATER SOURCE
        CurrentModuleObject = "Coil:Cooling:WaterToAirHeatPump:VariableSpeedEquationFit"; // for reporting

        for (CoilCounter = 1; CoilCounter <= NumCool; ++CoilCounter) {

            ++DXCoilNum;
            AlfaFieldIncre = 1;

            state.dataInputProcessing->inputProcessor->getObjectItem(state,
                                                                     CurrentModuleObject,
                                                                     CoilCounter,
                                                                     AlphArray,
                                                                     NumAlphas,
                                                                     NumArray,
                                                                     NumNums,
                                                                     IOStat,
                                                                     lNumericBlanks,
                                                                     lAlphaBlanks,
                                                                     cAlphaFields,
                                                                     cNumericFields);

            // ErrorsFound will be set to True if problem was found, left untouched otherwise
            VerifyUniqueCoilName(state, CurrentModuleObject, AlphArray(1), ErrorsFound, CurrentModuleObject + " Name");

            auto &VarSpeedCoil = state.dataVariableSpeedCoils->VarSpeedCoil(DXCoilNum);
            VarSpeedCoil.bIsDesuperheater = false;
            VarSpeedCoil.Name = AlphArray(1);
            VarSpeedCoil.CoolHeatType = "COOLING";
            VarSpeedCoil.VSCoilTypeOfNum = DataHVACGlobals::Coil_CoolingWaterToAirHPVSEquationFit;
            VarSpeedCoil.VarSpeedCoilType =
                DataHVACGlobals::cAllCoilTypes(VarSpeedCoil.VSCoilTypeOfNum);
            VarSpeedCoil.NumOfSpeeds = int(NumArray(1));
            VarSpeedCoil.NormSpedLevel = int(NumArray(2));
            VarSpeedCoil.RatedCapCoolTotal = NumArray(3);
            VarSpeedCoil.RatedAirVolFlowRate = NumArray(4);
            VarSpeedCoil.RatedWaterVolFlowRate = NumArray(5);
            VarSpeedCoil.Twet_Rated = NumArray(6);
            VarSpeedCoil.Gamma_Rated = NumArray(7);
            VarSpeedCoil.HOTGASREHEATFLG = int(NumArray(8));
            VarSpeedCoil.CondenserType = DataHeatBalance::RefrigCondenserType::Water;

            VarSpeedCoil.WaterInletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(2),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilCoolingWaterToAirHeatPumpVariableSpeedEquationFit,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Water,
                                  DataLoopNode::ConnectionType::Inlet,
                                  NodeInputManager::CompFluidStream::Secondary,
                                  ObjectIsNotParent);
            VarSpeedCoil.WaterOutletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(3),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilCoolingWaterToAirHeatPumpVariableSpeedEquationFit,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Water,
                                  DataLoopNode::ConnectionType::Outlet,
                                  NodeInputManager::CompFluidStream::Secondary,
                                  ObjectIsNotParent);
            VarSpeedCoil.AirInletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(4),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilCoolingWaterToAirHeatPumpVariableSpeedEquationFit,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Air,
                                  DataLoopNode::ConnectionType::Inlet,
                                  NodeInputManager::CompFluidStream::Primary,
                                  ObjectIsNotParent);
            VarSpeedCoil.AirOutletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(5),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilCoolingWaterToAirHeatPumpVariableSpeedEquationFit,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Air,
                                  DataLoopNode::ConnectionType::Outlet,
                                  NodeInputManager::CompFluidStream::Primary,
                                  ObjectIsNotParent);

            TestCompSet(state, CurrentModuleObject, AlphArray(1), AlphArray(2), AlphArray(3), "Water Nodes");
            TestCompSet(state, CurrentModuleObject, AlphArray(1), AlphArray(4), AlphArray(5), "Air Nodes");

            //   If (VarSpeedCoil(DXCoilNum)%NumOfSpeeds .LT. 2) Then
            if (VarSpeedCoil.NumOfSpeeds < 1) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be >= 1. entered number is {:.0T}", cNumericFields(1), NumArray(1)));
                ErrorsFound = true;
            }

            if (VarSpeedCoil.NormSpedLevel >
                VarSpeedCoil.NumOfSpeeds) {
                VarSpeedCoil.NormSpedLevel =
                    VarSpeedCoil.NumOfSpeeds;
            }

            if ((VarSpeedCoil.NormSpedLevel >
                 VarSpeedCoil.NumOfSpeeds) ||
                (VarSpeedCoil.NormSpedLevel <= 0)) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be valid speed level entered number is {:.0T}", cNumericFields(2), NumArray(2)));
                ErrorsFound = true;
            }

            // part load curve
            VarSpeedCoil.PLFFPLR = GetCurveIndex(state, AlphArray(6)); // convert curve name to number
            if (VarSpeedCoil.PLFFPLR == 0) {
                if (lAlphaBlanks(6)) {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", missing");
                    ShowContinueError(state, "...required " + cAlphaFields(6) + " is blank.");
                } else {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", invalid");
                    ShowContinueError(state, "...not found " + cAlphaFields(6) + "=\"" + AlphArray(6) + "\".");
                }
                ErrorsFound = true;
            } else {
                CurveVal = CurveValue(state, VarSpeedCoil.PLFFPLR, 1.0);
                if (CurveVal > 1.10 || CurveVal < 0.90) {
                    ShowWarningError(state,
                                     std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                         VarSpeedCoil.Name + "\", curve values");
                    ShowContinueError(state, "..." + cAlphaFields(6) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                    ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                }
            }

            for (I = 1; I <= VarSpeedCoil.NumOfSpeeds; ++I) {
                VarSpeedCoil.MSRatedTotCap(I) = NumArray(9 + (I - 1) * 6);
                VarSpeedCoil.MSRatedSHR(I) = NumArray(10 + (I - 1) * 6);
                VarSpeedCoil.MSRatedCOP(I) = NumArray(11 + (I - 1) * 6);
                VarSpeedCoil.MSRatedAirVolFlowRate(I) = NumArray(12 + (I - 1) * 6);
                VarSpeedCoil.MSRatedWaterVolFlowRate(I) = NumArray(13 + (I - 1) * 6);
                VarSpeedCoil.MSWasteHeatFrac(I) = NumArray(14 + (I - 1) * 6);

                AlfaFieldIncre = 7 + (I - 1) * 7;
                VarSpeedCoil.MSCCapFTemp(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSCCapFTemp(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is BiQuadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSCCapFTemp(I), // Curve index
                                                                {2},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(
                            state, VarSpeedCoil.MSCCapFTemp(I), RatedInletWetBulbTemp, RatedInletWaterTemp);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 8 + (I - 1) * 7;
                VarSpeedCoil.MSCCapAirFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSCCapAirFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |=
                        CurveManager::CheckCurveDims(state,
                                                     VarSpeedCoil.MSCCapAirFFlow(I), // Curve index
                                                     {1},                                                                     // Valid dimensions
                                                     RoutineName,                                                             // Routine name
                                                     CurrentModuleObject,                                                     // Object Type
                                                     VarSpeedCoil.Name,              // Object Name
                                                     cAlphaFields(AlfaFieldIncre));                                           // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSCCapAirFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 9 + (I - 1) * 7;
                VarSpeedCoil.MSCCapWaterFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSCCapWaterFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |=
                        CurveManager::CheckCurveDims(state,
                                                     VarSpeedCoil.MSCCapWaterFFlow(I), // Curve index
                                                     {1},                                                                       // Valid dimensions
                                                     RoutineName,                                                               // Routine name
                                                     CurrentModuleObject,                                                       // Object Type
                                                     VarSpeedCoil.Name,                // Object Name
                                                     cAlphaFields(AlfaFieldIncre));                                             // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSCCapWaterFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 10 + (I - 1) * 7;
                VarSpeedCoil.MSEIRFTemp(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSEIRFTemp(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is BiQuadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSEIRFTemp(I), // Curve index
                                                                {2},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(
                            state, VarSpeedCoil.MSEIRFTemp(I), RatedInletWetBulbTemp, RatedInletWaterTemp);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 11 + (I - 1) * 7;
                VarSpeedCoil.MSEIRAirFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSEIRAirFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSEIRAirFFlow(I), // Curve index
                                                                {1},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 12 + (I - 1) * 7;
                VarSpeedCoil.MSEIRWaterFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSEIRWaterFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |=
                        CurveManager::CheckCurveDims(state,
                                                     VarSpeedCoil.MSEIRWaterFFlow(I), // Curve index
                                                     {1},                                                                      // Valid dimensions
                                                     RoutineName,                                                              // Routine name
                                                     CurrentModuleObject,                                                      // Object Type
                                                     VarSpeedCoil.Name,               // Object Name
                                                     cAlphaFields(AlfaFieldIncre));                                            // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSEIRWaterFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 13 + (I - 1) * 7;
                // Read waste heat modifier curve name
                VarSpeedCoil.MSWasteHeat(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSWasteHeat(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal types are BiQuadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSWasteHeat(I), // Curve index
                                                                {2},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(
                            state, VarSpeedCoil.MSWasteHeat(I), RatedInletWaterTemp, RatedInletAirTemp);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }
            }

            for (I = 1; I <= VarSpeedCoil.NumOfSpeeds; ++I) {
                VarSpeedCoil.MSRatedPercentTotCap(I) =
                    VarSpeedCoil.MSRatedTotCap(I) /
                    VarSpeedCoil.MSRatedTotCap(
                        VarSpeedCoil.NumOfSpeeds);
                VarSpeedCoil.MSRatedAirVolFlowPerRatedTotCap(I) =
                    VarSpeedCoil.MSRatedAirVolFlowRate(I) /
                    VarSpeedCoil.MSRatedTotCap(I);
                VarSpeedCoil.MSRatedWaterVolFlowPerRatedTotCap(I) =
                    VarSpeedCoil.MSRatedWaterVolFlowRate(I) /
                    VarSpeedCoil.MSRatedTotCap(I);
            }

            // CurrentModuleObject = "Coil:Cooling:WaterToAirHeatPump:VariableSpeedEquationFit"
            SetupOutputVariable(state,
                                "Cooling Coil Electricity Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.Energy,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name,
                                _,
                                "Electricity",
                                "Cooling",
                                _,
                                "System");
            SetupOutputVariable(state,
                                "Cooling Coil Total Cooling Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergyLoadTotal,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name,
                                _,
                                "ENERGYTRANSFER",
                                "COOLINGCOILS",
                                _,
                                "System");
            SetupOutputVariable(state,
                                "Cooling Coil Sensible Cooling Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergySensible,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name);
            SetupOutputVariable(state,
                                "Cooling Coil Latent Cooling Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergyLatent,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name);
            SetupOutputVariable(state,
                                "Cooling Coil Source Side Heat Transfer Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergySource,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name,
                                _,
                                "PLANTLOOPCOOLINGDEMAND",
                                "COOLINGCOILS",
                                _,
                                "System");

            // for table output, being consistent with outher water-to-air coils
            //        IF (VarSpeedCoil(DXCoilNum)%RatedCapCoolTotal /= AutoSize) THEN
            //            VarSpeedCoil(DXCoilNum)%RatedCapCoolSens = VarSpeedCoil(DXCoilNum)%RatedCapCoolTotal &
            //                *VarSpeedCoil(DXCoilNum)%MSRatedSHR(VarSpeedCoil(DXCoilNum)%NormSpedLevel)
            //        ELSE
            //            VarSpeedCoil(DXCoilNum)%RatedCapCoolSens = AUTOSIZE
            //        END IF

            VarSpeedCoil.RatedCapCoolSens =
                AutoSize; // always auto-sized, to be determined in the sizing calculation

            // BAN Sept 30 2103, CR9322, commented out, now it is redundant, it is reported from sizing routine
            // create predefined report entries
            // PreDefTableEntry(state,  pdchCoolCoilType, VarSpeedCoil( DXCoilNum ).Name, CurrentModuleObject );
            // PreDefTableEntry(state,  pdchCoolCoilTotCap, VarSpeedCoil( DXCoilNum ).Name, VarSpeedCoil( DXCoilNum ).RatedCapCoolTotal );
            // PreDefTableEntry(state,  pdchCoolCoilSensCap, VarSpeedCoil( DXCoilNum ).Name, VarSpeedCoil( DXCoilNum ).RatedCapCoolSens );
            // PreDefTableEntry(state,  pdchCoolCoilLatCap, VarSpeedCoil( DXCoilNum ).Name, VarSpeedCoil( DXCoilNum ).RatedCapCoolTotal -
            // VarSpeedCoil( DXCoilNum ).RatedCapCoolSens );  PreDefTableEntry(state,  pdchCoolCoilSHR, VarSpeedCoil( DXCoilNum ).Name, VarSpeedCoil(
            // DXCoilNum
            // ).RatedCapCoolSens / VarSpeedCoil( DXCoilNum ).RatedCapCoolTotal );  PreDefTableEntry(state,  pdchCoolCoilNomEff, VarSpeedCoil(
            // DXCoilNum
            // ).Name, VarSpeedCoil( DXCoilNum ).MSRatedCOP( VarSpeedCoil( DXCoilNum ).NormSpedLevel ) );
        }

        //-------------------------AIR SOURCE, COOLING---BEGIN
        // Get the data for cooling coil, AIR SOURCE
        CurrentModuleObject = "Coil:Cooling:DX:VariableSpeed"; // for reporting

        for (CoilCounter = 1; CoilCounter <= NumCoolAS; ++CoilCounter) {

            ++DXCoilNum;
            AlfaFieldIncre = 1;

            state.dataInputProcessing->inputProcessor->getObjectItem(state,
                                                                     CurrentModuleObject,
                                                                     CoilCounter,
                                                                     AlphArray,
                                                                     NumAlphas,
                                                                     NumArray,
                                                                     NumNums,
                                                                     IOStat,
                                                                     lNumericBlanks,
                                                                     lAlphaBlanks,
                                                                     cAlphaFields,
                                                                     cNumericFields);
            // ErrorsFound will be set to True if problem was found, left untouched otherwise
            VerifyUniqueCoilName(state, CurrentModuleObject, AlphArray(1), ErrorsFound, CurrentModuleObject + " Name");

            auto &VarSpeedCoil = state.dataVariableSpeedCoils->VarSpeedCoil(DXCoilNum);
            VarSpeedCoil.bIsDesuperheater = false;
            VarSpeedCoil.Name = AlphArray(1);
            // Initialize DataHeatBalance heat reclaim variable name for use by heat reclaim coils
            state.dataHeatBal->HeatReclaimVS_DXCoil(DXCoilNum).Name = VarSpeedCoil.Name;
            state.dataHeatBal->HeatReclaimVS_DXCoil(DXCoilNum).SourceType = CurrentModuleObject;
            VarSpeedCoil.CoolHeatType = "COOLING";
            VarSpeedCoil.VSCoilTypeOfNum = Coil_CoolingAirToAirVariableSpeed;
            VarSpeedCoil.VarSpeedCoilType =
                DataHVACGlobals::cAllCoilTypes(Coil_CoolingAirToAirVariableSpeed);
            VarSpeedCoil.NumOfSpeeds = int(NumArray(1));
            VarSpeedCoil.NormSpedLevel = int(NumArray(2));
            VarSpeedCoil.RatedCapCoolTotal = NumArray(3);
            VarSpeedCoil.RatedAirVolFlowRate = NumArray(4);
            VarSpeedCoil.Twet_Rated = NumArray(5);
            VarSpeedCoil.Gamma_Rated = NumArray(6);

            VarSpeedCoil.AirInletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(2),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilCoolingDXVariableSpeed,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Air,
                                  DataLoopNode::ConnectionType::Inlet,
                                  NodeInputManager::CompFluidStream::Primary,
                                  ObjectIsNotParent);
            VarSpeedCoil.AirOutletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(3),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilCoolingDXVariableSpeed,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Air,
                                  DataLoopNode::ConnectionType::Outlet,
                                  NodeInputManager::CompFluidStream::Primary,
                                  ObjectIsNotParent);

            TestCompSet(state, CurrentModuleObject, AlphArray(1), AlphArray(2), AlphArray(3), "Air Nodes");

            if (VarSpeedCoil.NumOfSpeeds < 1) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be >= 1. entered number is {:.0T}", cNumericFields(1), NumArray(1)));
                ErrorsFound = true;
            }

            if (VarSpeedCoil.NormSpedLevel >
                VarSpeedCoil.NumOfSpeeds) {
                VarSpeedCoil.NormSpedLevel =
                    VarSpeedCoil.NumOfSpeeds;
            }

            if ((VarSpeedCoil.NormSpedLevel >
                 VarSpeedCoil.NumOfSpeeds) ||
                (VarSpeedCoil.NormSpedLevel <= 0)) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be valid speed level entered number is {:.0T}", cNumericFields(2), NumArray(2)));
                ErrorsFound = true;
            }

            // part load curve
            VarSpeedCoil.PLFFPLR = GetCurveIndex(state, AlphArray(4)); // convert curve name to number
            if (VarSpeedCoil.PLFFPLR == 0) {
                if (lAlphaBlanks(4)) {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", missing");
                    ShowContinueError(state, "...required " + cAlphaFields(6) + " is blank.");
                } else {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", invalid");
                    ShowContinueError(state, "...not found " + cAlphaFields(4) + "=\"" + AlphArray(4) + "\".");
                }
                ErrorsFound = true;
            } else {
                CurveVal = CurveValue(state, VarSpeedCoil.PLFFPLR, 1.0);
                if (CurveVal > 1.10 || CurveVal < 0.90) {
                    ShowWarningError(state,
                                     std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                         VarSpeedCoil.Name + "\", curve values");
                    ShowContinueError(state, "..." + cAlphaFields(4) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                    ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                }
            }

            // outdoor condenser node
            if (lAlphaBlanks(5)) {
                VarSpeedCoil.CondenserInletNodeNum = 0;
            } else {
                VarSpeedCoil.CondenserInletNodeNum =
                    GetOnlySingleNode(state,
                                      AlphArray(5),
                                      ErrorsFound,
                                      DataLoopNode::ConnectionObjectType::CoilCoolingDXVariableSpeed,
                                      VarSpeedCoil.Name,
                                      DataLoopNode::NodeFluidType::Air,
                                      DataLoopNode::ConnectionType::OutsideAirReference,
                                      NodeInputManager::CompFluidStream::Primary,
                                      ObjectIsNotParent);

                if (!CheckOutAirNodeNumber(state, VarSpeedCoil.CondenserInletNodeNum)) {
                    ShowWarningError(state,
                                     std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                         VarSpeedCoil.Name + "\", may be invalid");
                    ShowContinueError(state,
                                      cAlphaFields(10) + "=\"" + AlphArray(5) +
                                          "\", node does not appear in an OutdoorAir:NodeList or as an OutdoorAir:Node.");
                    ShowContinueError(
                        state, "This node needs to be included in an air system or the coil model will not be valid, and the simulation continues");
                }
            }

            if ((UtilityRoutines::SameString(AlphArray(6), "AirCooled")) || lAlphaBlanks(6)) {
                VarSpeedCoil.CondenserType = DataHeatBalance::RefrigCondenserType::Air;
            } else if (UtilityRoutines::SameString(AlphArray(6), "EvaporativelyCooled")) {
                VarSpeedCoil.CondenserType = DataHeatBalance::RefrigCondenserType::Evap;
                VarSpeedCoil.ReportEvapCondVars = true;
            } else {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, "..." + cAlphaFields(6) + "=\"" + AlphArray(6) + "\":");
                ShowContinueError(state, "...must be AirCooled or EvaporativelyCooled.");
                ErrorsFound = true;
            }

            VarSpeedCoil.EvapCondPumpElecNomPower = NumArray(7);

            if (VarSpeedCoil.EvapCondPumpElecNomPower < 0.0) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, "..." + cNumericFields(7) + " cannot be < 0.0.");
                ShowContinueError(state, format("...entered value=[{:.2T}].", NumArray(7)));
                ErrorsFound = true;
            }

            // Set crankcase heater capacity
            VarSpeedCoil.CrankcaseHeaterCapacity = NumArray(8);
            if (VarSpeedCoil.CrankcaseHeaterCapacity < 0.0) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, "..." + cNumericFields(8) + " cannot be < 0.0.");
                ShowContinueError(state, format("...entered value=[{:.2T}].", NumArray(8)));
                ErrorsFound = true;
            }

            // Set crankcase heater cutout temperature
            VarSpeedCoil.MaxOATCrankcaseHeater = NumArray(9);

            // Set crankcase heater cutout temperature
            VarSpeedCoil.MinOATCompressor = NumArray(10);

            // Get Water System tank connections
            //  A7, \field Name of Water Storage Tank for Supply
            VarSpeedCoil.EvapWaterSupplyName = AlphArray(7);
            if (lAlphaBlanks(7)) {
                VarSpeedCoil.EvapWaterSupplyMode = WaterSupplyFromMains;
            } else {
                VarSpeedCoil.EvapWaterSupplyMode = WaterSupplyFromTank;
                SetupTankDemandComponent(state,
                                         VarSpeedCoil.Name,
                                         CurrentModuleObject,
                                         VarSpeedCoil.EvapWaterSupplyName,
                                         ErrorsFound,
                                         VarSpeedCoil.EvapWaterSupTankID,
                                         VarSpeedCoil.EvapWaterTankDemandARRID);
            }

            // A8; \field Name of Water Storage Tank for Condensate Collection
            VarSpeedCoil.CondensateCollectName = AlphArray(8);
            if (lAlphaBlanks(8)) {
                VarSpeedCoil.CondensateCollectMode = CondensateDiscarded;
            } else {
                VarSpeedCoil.CondensateCollectMode = CondensateToTank;
                SetupTankSupplyComponent(state,
                                         VarSpeedCoil.Name,
                                         CurrentModuleObject,
                                         VarSpeedCoil.CondensateCollectName,
                                         ErrorsFound,
                                         VarSpeedCoil.CondensateTankID,
                                         VarSpeedCoil.CondensateTankSupplyARRID);
            }

            //   Basin heater power as a function of temperature must be greater than or equal to 0
            VarSpeedCoil.BasinHeaterPowerFTempDiff = NumArray(11);
            if (NumArray(11) < 0.0) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, "..." + cNumericFields(11) + " must be >= 0.0.");
                ShowContinueError(state, format("...entered value=[{:.2T}].", NumArray(11)));
                ErrorsFound = true;
            }

            VarSpeedCoil.BasinHeaterSetPointTemp = NumArray(12);
            if (VarSpeedCoil.BasinHeaterPowerFTempDiff > 0.0) {
                if (VarSpeedCoil.BasinHeaterSetPointTemp < 2.0) {
                    ShowWarningError(state,
                                     std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                         VarSpeedCoil.Name + "\", freeze possible");
                    ShowContinueError(state, "..." + cNumericFields(12) + " is < 2 {C}. Freezing could occur.");
                    ShowContinueError(state, format("...entered value=[{:.2T}].", NumArray(12)));
                }
            }

            if (!lAlphaBlanks(9)) {
                VarSpeedCoil.BasinHeaterSchedulePtr = GetScheduleIndex(state, AlphArray(9));
                if (VarSpeedCoil.BasinHeaterSchedulePtr == 0) {
                    ShowWarningError(state,
                                     std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                         VarSpeedCoil.Name + "\", invalid");
                    ShowContinueError(state, "...not found " + cAlphaFields(14) + "=\"" + AlphArray(9) + "\".");
                    ShowContinueError(state, "Basin heater will be available to operate throughout the simulation.");
                }
            }

            for (I = 1; I <= VarSpeedCoil.NumOfSpeeds; ++I) {
                VarSpeedCoil.MSRatedTotCap(I) = NumArray(13 + (I - 1) * 6);
                VarSpeedCoil.MSRatedSHR(I) = NumArray(14 + (I - 1) * 6);
                VarSpeedCoil.MSRatedCOP(I) = NumArray(15 + (I - 1) * 6);
                VarSpeedCoil.MSRatedAirVolFlowRate(I) = NumArray(16 + (I - 1) * 6);
                VarSpeedCoil.EvapCondAirFlow(I) = NumArray(17 + (I - 1) * 6);

                VarSpeedCoil.EvapCondEffect(I) = NumArray(18 + (I - 1) * 6);
                if (VarSpeedCoil.EvapCondEffect(I) < 0.0 ||
                    VarSpeedCoil.EvapCondEffect(I) > 1.0) {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", invalid");
                    ShowContinueError(state, "..." + cNumericFields(18 + (I - 1) * 6) + " cannot be < 0.0 or > 1.0.");
                    ShowContinueError(state, format("...entered value=[{:.2T}].", NumArray(18 + (I - 1) * 6)));
                    ErrorsFound = true;
                }

                AlfaFieldIncre = 10 + (I - 1) * 4;
                VarSpeedCoil.MSCCapFTemp(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSCCapFTemp(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is BiQuadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSCCapFTemp(I), // Curve index
                                                                {2},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(
                            state, VarSpeedCoil.MSCCapFTemp(I), RatedInletWetBulbTemp, RatedAmbAirTemp);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 11 + (I - 1) * 4;
                VarSpeedCoil.MSCCapAirFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSCCapAirFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |=
                        CurveManager::CheckCurveDims(state,
                                                     VarSpeedCoil.MSCCapAirFFlow(I), // Curve index
                                                     {1},                                                                     // Valid dimensions
                                                     RoutineName,                                                             // Routine name
                                                     CurrentModuleObject,                                                     // Object Type
                                                     VarSpeedCoil.Name,              // Object Name
                                                     cAlphaFields(AlfaFieldIncre));                                           // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSCCapAirFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 12 + (I - 1) * 4;
                VarSpeedCoil.MSEIRFTemp(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSEIRFTemp(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is BiQuadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSEIRFTemp(I), // Curve index
                                                                {2},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(
                            state, VarSpeedCoil.MSEIRFTemp(I), RatedInletWetBulbTemp, RatedAmbAirTemp);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 13 + (I - 1) * 4;
                VarSpeedCoil.MSEIRAirFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSEIRAirFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSEIRAirFFlow(I), // Curve index
                                                                {1},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }
            }

            for (I = 1; I <= VarSpeedCoil.NumOfSpeeds; ++I) {
                VarSpeedCoil.MSRatedPercentTotCap(I) =
                    VarSpeedCoil.MSRatedTotCap(I) /
                    VarSpeedCoil.MSRatedTotCap(
                        VarSpeedCoil.NumOfSpeeds);
                VarSpeedCoil.MSRatedAirVolFlowPerRatedTotCap(I) =
                    VarSpeedCoil.MSRatedAirVolFlowRate(I) /
                    VarSpeedCoil.MSRatedTotCap(I);
                VarSpeedCoil.MSRatedEvapCondVolFlowPerRatedTotCap(I) =
                    VarSpeedCoil.EvapCondAirFlow(I) /
                    VarSpeedCoil.MSRatedTotCap(I);
            }

            // CurrentModuleObject = "Coil:Cooling:DX:VariableSpeed"
            SetupOutputVariable(state,
                                "Cooling Coil Electricity Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.Energy,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name,
                                _,
                                "Electricity",
                                "Cooling",
                                _,
                                "System");
            SetupOutputVariable(state,
                                "Cooling Coil Total Cooling Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergyLoadTotal,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name,
                                _,
                                "ENERGYTRANSFER",
                                "COOLINGCOILS",
                                _,
                                "System");
            SetupOutputVariable(state,
                                "Cooling Coil Sensible Cooling Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergySensible,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name);
            SetupOutputVariable(state,
                                "Cooling Coil Latent Cooling Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergyLatent,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name);
            SetupOutputVariable(state,
                                "Cooling Coil Source Side Heat Transfer Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergySource,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name);

            VarSpeedCoil.RatedCapCoolSens =
                AutoSize; // always auto-sized, to be determined in the sizing calculation

            // BAN Sept 30 2103, CR9322, commented out, now it is redundant, it is reported from sizing routine
            // create predefined report entries
            // PreDefTableEntry(state,  pdchCoolCoilType, VarSpeedCoil( DXCoilNum ).Name, CurrentModuleObject );
            // PreDefTableEntry(state,  pdchCoolCoilTotCap, VarSpeedCoil( DXCoilNum ).Name, VarSpeedCoil( DXCoilNum ).RatedCapCoolTotal );
            // PreDefTableEntry(state,  pdchCoolCoilSensCap, VarSpeedCoil( DXCoilNum ).Name, VarSpeedCoil( DXCoilNum ).RatedCapCoolSens );
            // PreDefTableEntry(state,  pdchCoolCoilLatCap, VarSpeedCoil( DXCoilNum ).Name, VarSpeedCoil( DXCoilNum ).RatedCapCoolTotal -
            // VarSpeedCoil( DXCoilNum ).RatedCapCoolSens );  PreDefTableEntry(state,  pdchCoolCoilSHR, VarSpeedCoil( DXCoilNum ).Name, VarSpeedCoil(
            // DXCoilNum
            // ).RatedCapCoolSens / VarSpeedCoil( DXCoilNum ).RatedCapCoolTotal );  PreDefTableEntry(state,  pdchCoolCoilNomEff, VarSpeedCoil(
            // DXCoilNum
            // ).Name, VarSpeedCoil( DXCoilNum ).MSRatedCOP( VarSpeedCoil( DXCoilNum ).NormSpedLevel ) );
        }

        //-------------------------AIR SOURCE COOLING---END

        // Get the data for heating coil, WATER SOURCE
        CurrentModuleObject = "Coil:Heating:WaterToAirHeatPump:VariableSpeedEquationFit";

        for (CoilCounter = 1; CoilCounter <= NumHeat; ++CoilCounter) {

            ++DXCoilNum;

            state.dataInputProcessing->inputProcessor->getObjectItem(state,
                                                                     CurrentModuleObject,
                                                                     CoilCounter,
                                                                     AlphArray,
                                                                     NumAlphas,
                                                                     NumArray,
                                                                     NumNums,
                                                                     IOStat,
                                                                     lNumericBlanks,
                                                                     lAlphaBlanks,
                                                                     cAlphaFields,
                                                                     cNumericFields);
            // ErrorsFound will be set to True if problem was found, left untouched otherwise
            VerifyUniqueCoilName(state, CurrentModuleObject, AlphArray(1), ErrorsFound, CurrentModuleObject + " Name");

            auto &VarSpeedCoil = state.dataVariableSpeedCoils->VarSpeedCoil(DXCoilNum);
            VarSpeedCoil.bIsDesuperheater = false;
            VarSpeedCoil.Name = AlphArray(1);
            VarSpeedCoil.CoolHeatType = "HEATING";
            VarSpeedCoil.VSCoilTypeOfNum = Coil_HeatingWaterToAirHPVSEquationFit; // fix coil type

            VarSpeedCoil.VarSpeedCoilType =
                DataHVACGlobals::cAllCoilTypes(VarSpeedCoil.VSCoilTypeOfNum);
            VarSpeedCoil.NumOfSpeeds = int(NumArray(1));
            VarSpeedCoil.NormSpedLevel = int(NumArray(2));
            VarSpeedCoil.RatedCapHeat = NumArray(3);
            VarSpeedCoil.RatedAirVolFlowRate = NumArray(4);
            VarSpeedCoil.RatedWaterVolFlowRate = NumArray(5);
            VarSpeedCoil.CondenserType = DataHeatBalance::RefrigCondenserType::Water;

            VarSpeedCoil.WaterInletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(2),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilHeatingWaterToAirHeatPumpVariableSpeedEquationFit,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Water,
                                  DataLoopNode::ConnectionType::Inlet,
                                  NodeInputManager::CompFluidStream::Secondary,
                                  ObjectIsNotParent);
            VarSpeedCoil.WaterOutletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(3),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilHeatingWaterToAirHeatPumpVariableSpeedEquationFit,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Water,
                                  DataLoopNode::ConnectionType::Outlet,
                                  NodeInputManager::CompFluidStream::Secondary,
                                  ObjectIsNotParent);
            VarSpeedCoil.AirInletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(4),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilHeatingWaterToAirHeatPumpVariableSpeedEquationFit,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Air,
                                  DataLoopNode::ConnectionType::Inlet,
                                  NodeInputManager::CompFluidStream::Primary,
                                  ObjectIsNotParent);
            VarSpeedCoil.AirOutletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(5),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilHeatingWaterToAirHeatPumpVariableSpeedEquationFit,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Air,
                                  DataLoopNode::ConnectionType::Outlet,
                                  NodeInputManager::CompFluidStream::Primary,
                                  ObjectIsNotParent);

            TestCompSet(state, CurrentModuleObject, AlphArray(1), AlphArray(2), AlphArray(3), "Water Nodes");
            TestCompSet(state, CurrentModuleObject, AlphArray(1), AlphArray(4), AlphArray(5), "Air Nodes");

            //       If (VarSpeedCoil(DXCoilNum)%NumOfSpeeds .LT. 2) Then
            if (VarSpeedCoil.NumOfSpeeds < 1) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be >= 1. entered number is {:.0T}", cNumericFields(1), NumArray(1)));
                ErrorsFound = true;
            }

            if (VarSpeedCoil.NormSpedLevel >
                VarSpeedCoil.NumOfSpeeds) {
                VarSpeedCoil.NormSpedLevel =
                    VarSpeedCoil.NumOfSpeeds;
            }

            if ((VarSpeedCoil.NormSpedLevel >
                 VarSpeedCoil.NumOfSpeeds) ||
                (VarSpeedCoil.NormSpedLevel <= 0)) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be valid speed level entered number is {:.0T}", cNumericFields(2), NumArray(2)));
                ErrorsFound = true;
            }

            // part load curve
            VarSpeedCoil.PLFFPLR = GetCurveIndex(state, AlphArray(6)); // convert curve name to number
            if (VarSpeedCoil.PLFFPLR == 0) {
                if (lAlphaBlanks(6)) {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", missing");
                    ShowContinueError(state, "...required " + cAlphaFields(6) + " is blank.");
                } else {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", invalid");
                    ShowContinueError(state, "...not found " + cAlphaFields(6) + "=\"" + AlphArray(6) + "\".");
                }
                ErrorsFound = true;
            } else {
                CurveVal = CurveValue(state, VarSpeedCoil.PLFFPLR, 1.0);
                if (CurveVal > 1.10 || CurveVal < 0.90) {
                    ShowWarningError(state,
                                     std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                         VarSpeedCoil.Name + "\", curve values");
                    ShowContinueError(state, "..." + cAlphaFields(6) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                    ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                }
            }

            for (I = 1; I <= VarSpeedCoil.NumOfSpeeds; ++I) {
                VarSpeedCoil.MSRatedTotCap(I) = NumArray(6 + (I - 1) * 5);
                VarSpeedCoil.MSRatedCOP(I) = NumArray(7 + (I - 1) * 5);
                VarSpeedCoil.MSRatedAirVolFlowRate(I) = NumArray(8 + (I - 1) * 5);
                VarSpeedCoil.MSRatedWaterVolFlowRate(I) = NumArray(9 + (I - 1) * 5);
                VarSpeedCoil.MSWasteHeatFrac(I) = NumArray(10 + (I - 1) * 5);

                AlfaFieldIncre = 7 + (I - 1) * 7;
                VarSpeedCoil.MSCCapFTemp(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSCCapFTemp(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is BiQuadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSCCapFTemp(I), // Curve index
                                                                {2},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state,
                                              VarSpeedCoil.MSCCapFTemp(I),
                                              RatedInletAirTempHeat,
                                              RatedInletWaterTempHeat);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 8 + (I - 1) * 7;
                VarSpeedCoil.MSCCapAirFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSCCapAirFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |=
                        CurveManager::CheckCurveDims(state,
                                                     VarSpeedCoil.MSCCapAirFFlow(I), // Curve index
                                                     {1},                                                                     // Valid dimensions
                                                     RoutineName,                                                             // Routine name
                                                     CurrentModuleObject,                                                     // Object Type
                                                     VarSpeedCoil.Name,              // Object Name
                                                     cAlphaFields(AlfaFieldIncre));                                           // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSCCapAirFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 9 + (I - 1) * 7;
                VarSpeedCoil.MSCCapWaterFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSCCapWaterFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(14 + (I - 1) * 6) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |=
                        CurveManager::CheckCurveDims(state,
                                                     VarSpeedCoil.MSCCapWaterFFlow(I), // Curve index
                                                     {1},                                                                       // Valid dimensions
                                                     RoutineName,                                                               // Routine name
                                                     CurrentModuleObject,                                                       // Object Type
                                                     VarSpeedCoil.Name,                // Object Name
                                                     cAlphaFields(AlfaFieldIncre));                                             // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSCCapWaterFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 10 + (I - 1) * 7;
                VarSpeedCoil.MSEIRFTemp(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSEIRFTemp(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is BiQuadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSEIRFTemp(I), // Curve index
                                                                {2},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state,
                                              VarSpeedCoil.MSEIRFTemp(I),
                                              RatedInletAirTempHeat,
                                              RatedInletWaterTempHeat);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 11 + (I - 1) * 7;
                VarSpeedCoil.MSEIRAirFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSEIRAirFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(16 + (I - 1) * 6) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSEIRAirFFlow(I), // Curve index
                                                                {1},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 12 + (I - 1) * 7;
                VarSpeedCoil.MSEIRWaterFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSEIRWaterFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |=
                        CurveManager::CheckCurveDims(state,
                                                     VarSpeedCoil.MSEIRWaterFFlow(I), // Curve index
                                                     {1},                                                                      // Valid dimensions
                                                     RoutineName,                                                              // Routine name
                                                     CurrentModuleObject,                                                      // Object Type
                                                     VarSpeedCoil.Name,               // Object Name
                                                     cAlphaFields(AlfaFieldIncre));                                            // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSEIRWaterFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 13 + (I - 1) * 7;
                // Read waste heat modifier curve name
                VarSpeedCoil.MSWasteHeat(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSWasteHeat(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal types are BiQuadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSWasteHeat(I), // Curve index
                                                                {2},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state,
                                              VarSpeedCoil.MSWasteHeat(I),
                                              RatedInletAirTempHeat,
                                              RatedInletWaterTempHeat);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }
            }

            for (I = 1; I <= VarSpeedCoil.NumOfSpeeds; ++I) {
                VarSpeedCoil.MSRatedPercentTotCap(I) =
                    VarSpeedCoil.MSRatedTotCap(I) /
                    VarSpeedCoil.MSRatedTotCap(
                        VarSpeedCoil.NumOfSpeeds);
                VarSpeedCoil.MSRatedAirVolFlowPerRatedTotCap(I) =
                    VarSpeedCoil.MSRatedAirVolFlowRate(I) /
                    VarSpeedCoil.MSRatedTotCap(I);
                VarSpeedCoil.MSRatedWaterVolFlowPerRatedTotCap(I) =
                    VarSpeedCoil.MSRatedWaterVolFlowRate(I) /
                    VarSpeedCoil.MSRatedTotCap(I);
            }

            // CurrentModuleObject = "Coil:Heating:WaterToAirHeatPump:VariableSpeedEquationFit"
            SetupOutputVariable(state,
                                "Heating Coil Electricity Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.Energy,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name,
                                _,
                                "Electricity",
                                "Heating",
                                _,
                                "System");
            SetupOutputVariable(state,
                                "Heating Coil Heating Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergyLoadTotal,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name,
                                _,
                                "ENERGYTRANSFER",
                                "HEATINGCOILS",
                                _,
                                "System");
            SetupOutputVariable(state,
                                "Heating Coil Source Side Heat Transfer Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergySource,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name,
                                _,
                                "PLANTLOOPHEATINGDEMAND",
                                "HEATINGCOILS",
                                _,
                                "System");

            // create predefined report entries
            PreDefTableEntry(
                state, state.dataOutRptPredefined->pdchHeatCoilType, VarSpeedCoil.Name, CurrentModuleObject);
            PreDefTableEntry(state,
                             state.dataOutRptPredefined->pdchHeatCoilNomCap,
                             VarSpeedCoil.Name,
                             VarSpeedCoil.RatedCapHeat);
            PreDefTableEntry(state,
                             state.dataOutRptPredefined->pdchHeatCoilNomEff,
                             VarSpeedCoil.Name,
                             VarSpeedCoil.MSRatedCOP(
                                 VarSpeedCoil.NormSpedLevel));
        }

        //-------------------------AIR SOURCE, HEATING---BEGIN
        // Get the data for heating coil, AIR SOURCE
        CurrentModuleObject = "COIL:HEATING:DX:VARIABLESPEED";

        for (CoilCounter = 1; CoilCounter <= NumHeatAS; ++CoilCounter) {

            ++DXCoilNum;

            state.dataInputProcessing->inputProcessor->getObjectItem(state,
                                                                     CurrentModuleObject,
                                                                     CoilCounter,
                                                                     AlphArray,
                                                                     NumAlphas,
                                                                     NumArray,
                                                                     NumNums,
                                                                     IOStat,
                                                                     lNumericBlanks,
                                                                     lAlphaBlanks,
                                                                     cAlphaFields,
                                                                     cNumericFields);
            // ErrorsFound will be set to True if problem was found, left untouched otherwise
            VerifyUniqueCoilName(state, CurrentModuleObject, AlphArray(1), ErrorsFound, CurrentModuleObject + " Name");

            auto &VarSpeedCoil = state.dataVariableSpeedCoils->VarSpeedCoil(DXCoilNum);
            VarSpeedCoil.bIsDesuperheater = false;
            VarSpeedCoil.Name = AlphArray(1);
            VarSpeedCoil.CoolHeatType = "HEATING";
            VarSpeedCoil.VSCoilTypeOfNum = Coil_HeatingAirToAirVariableSpeed;
            VarSpeedCoil.VarSpeedCoilType =
                DataHVACGlobals::cAllCoilTypes(Coil_HeatingAirToAirVariableSpeed);
            VarSpeedCoil.NumOfSpeeds = int(NumArray(1));
            VarSpeedCoil.NormSpedLevel = int(NumArray(2));
            VarSpeedCoil.RatedCapHeat = NumArray(3);
            VarSpeedCoil.RatedAirVolFlowRate = NumArray(4);

            VarSpeedCoil.AirInletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(2),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilHeatingDXVariableSpeed,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Air,
                                  DataLoopNode::ConnectionType::Inlet,
                                  NodeInputManager::CompFluidStream::Primary,
                                  ObjectIsNotParent);
            VarSpeedCoil.AirOutletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(3),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilHeatingDXVariableSpeed,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Air,
                                  DataLoopNode::ConnectionType::Outlet,
                                  NodeInputManager::CompFluidStream::Primary,
                                  ObjectIsNotParent);

            TestCompSet(state, CurrentModuleObject, AlphArray(1), AlphArray(2), AlphArray(3), "Air Nodes");

            if (VarSpeedCoil.NumOfSpeeds < 1) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be >= 1. entered number is {:.0T}", cNumericFields(1), NumArray(1)));
                ErrorsFound = true;
            }

            if (VarSpeedCoil.NormSpedLevel >
                VarSpeedCoil.NumOfSpeeds) {
                VarSpeedCoil.NormSpedLevel =
                    VarSpeedCoil.NumOfSpeeds;
            }

            if ((VarSpeedCoil.NormSpedLevel >
                 VarSpeedCoil.NumOfSpeeds) ||
                (VarSpeedCoil.NormSpedLevel <= 0)) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be valid speed level entered number is {:.0T}", cNumericFields(2), NumArray(2)));
                ErrorsFound = true;
            }

            // part load curve
            VarSpeedCoil.PLFFPLR = GetCurveIndex(state, AlphArray(4)); // convert curve name to number
            if (VarSpeedCoil.PLFFPLR == 0) {
                if (lAlphaBlanks(4)) {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", missing");
                    ShowContinueError(state, "...required " + cAlphaFields(4) + " is blank.");
                } else {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", invalid");
                    ShowContinueError(state, "...not found " + cAlphaFields(4) + "=\"" + AlphArray(4) + "\".");
                }
                ErrorsFound = true;
            } else {
                CurveVal = CurveValue(state, VarSpeedCoil.PLFFPLR, 1.0);
                if (CurveVal > 1.10 || CurveVal < 0.90) {
                    ShowWarningError(state,
                                     std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                         VarSpeedCoil.Name + "\", curve values");
                    ShowContinueError(state, "..." + cAlphaFields(4) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                    ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                }
            }

            VarSpeedCoil.DefrostEIRFT = GetCurveIndex(state, AlphArray(5)); // convert curve name to number

            if (UtilityRoutines::SameString(AlphArray(6), "ReverseCycle")) {
                if (VarSpeedCoil.DefrostEIRFT == 0) {
                    if (lAlphaBlanks(5)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(5) + " is blank.");
                        ShowContinueError(state, "...field is required because " + cAlphaFields(6) + " is \"ReverseCycle\".");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(5) + "=\"" + AlphArray(5) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is BiQuadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.DefrostEIRFT, // Curve index
                                                                {2},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name
                }
            }

            if (UtilityRoutines::SameString(AlphArray(6), "ReverseCycle"))
                VarSpeedCoil.DefrostStrategy = ReverseCycle;
            if (UtilityRoutines::SameString(AlphArray(6), "Resistive"))
                VarSpeedCoil.DefrostStrategy = Resistive;
            if (VarSpeedCoil.DefrostStrategy == 0) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, "...illegal " + cAlphaFields(6) + "=\"" + AlphArray(6) + "\".");
                ShowContinueError(state, "...valid values for this field are ReverseCycle or Resistive.");
                ErrorsFound = true;
            }

            if (UtilityRoutines::SameString(AlphArray(7), "Timed")) VarSpeedCoil.DefrostControl = Timed;
            if (UtilityRoutines::SameString(AlphArray(7), "OnDemand"))
                VarSpeedCoil.DefrostControl = OnDemand;
            if (VarSpeedCoil.DefrostControl == 0) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, "...illegal " + cAlphaFields(7) + "=\"" + AlphArray(7) + "\".");
                ShowContinueError(state, "...valid values for this field are Timed or OnDemand.");
                ErrorsFound = true;
            }

            // Set minimum OAT for heat pump compressor operation
            VarSpeedCoil.MinOATCompressor = NumArray(5);

            // reserved for HSPF calculation
            VarSpeedCoil.OATempCompressorOn = NumArray(6);

            // Set maximum outdoor temp for defrost to occur
            VarSpeedCoil.MaxOATDefrost = NumArray(7);

            // Set crankcase heater capacity
            VarSpeedCoil.CrankcaseHeaterCapacity = NumArray(8);
            if (VarSpeedCoil.CrankcaseHeaterCapacity < 0.0) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, "..." + cNumericFields(9) + " cannot be < 0.0.");
                ShowContinueError(state, format("...entered value=[{:.2T}].", NumArray(9)));
                ErrorsFound = true;
            }

            // Set crankcase heater cutout temperature
            VarSpeedCoil.MaxOATCrankcaseHeater = NumArray(9);

            // Set defrost time period
            VarSpeedCoil.DefrostTime = NumArray(10);
            if (VarSpeedCoil.DefrostTime == 0.0 &&
                VarSpeedCoil.DefrostControl == 1) {
                ShowWarningError(state,
                                 std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                     "\", ");
                ShowContinueError(state, "..." + cNumericFields(5) + " = 0.0 for defrost control = TIMED.");
            }

            // Set defrost capacity (for resistive defrost)
            VarSpeedCoil.DefrostCapacity = NumArray(11);
            if (VarSpeedCoil.DefrostCapacity == 0.0 &&
                VarSpeedCoil.DefrostStrategy == 2) {
                ShowWarningError(state,
                                 std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                     "\", ");
                ShowContinueError(state, "..." + cNumericFields(6) + " = 0.0 for defrost strategy = RESISTIVE.");
            }

            for (I = 1; I <= VarSpeedCoil.NumOfSpeeds; ++I) {
                VarSpeedCoil.MSRatedTotCap(I) = NumArray(12 + (I - 1) * 3);
                VarSpeedCoil.MSRatedCOP(I) = NumArray(13 + (I - 1) * 3);
                VarSpeedCoil.MSRatedAirVolFlowRate(I) = NumArray(14 + (I - 1) * 3);

                if (VarSpeedCoil.MSRatedTotCap(I) < 1.e-10) {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", invalid value");
                    ShowContinueError(state,
                                      format("...too small {}=[{:.2R}].",
                                             cNumericFields(12 + (I - 1) * 3),
                                             VarSpeedCoil.MSRatedTotCap(I)));
                    ErrorsFound = true;
                }

                AlfaFieldIncre = 8 + (I - 1) * 4;
                VarSpeedCoil.MSCCapFTemp(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSCCapFTemp(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is BiQuadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSCCapFTemp(I), // Curve index
                                                                {2},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(
                            state, VarSpeedCoil.MSCCapFTemp(I), RatedInletAirTempHeat, RatedAmbAirTempHeat);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 9 + (I - 1) * 4;
                VarSpeedCoil.MSCCapAirFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSCCapAirFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |=
                        CurveManager::CheckCurveDims(state,
                                                     VarSpeedCoil.MSCCapAirFFlow(I), // Curve index
                                                     {1},                                                                     // Valid dimensions
                                                     RoutineName,                                                             // Routine name
                                                     CurrentModuleObject,                                                     // Object Type
                                                     VarSpeedCoil.Name,              // Object Name
                                                     cAlphaFields(AlfaFieldIncre));                                           // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSCCapAirFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 10 + (I - 1) * 4;
                VarSpeedCoil.MSEIRFTemp(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSEIRFTemp(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is BiQuadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSEIRFTemp(I), // Curve index
                                                                {2},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(
                            state, VarSpeedCoil.MSEIRFTemp(I), RatedInletAirTempHeat, RatedAmbAirTempHeat);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 11 + (I - 1) * 4;
                VarSpeedCoil.MSEIRAirFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSEIRAirFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSEIRAirFFlow(I), // Curve index
                                                                {1},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) + " output is not equal to 1.0 (+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }
            }

            if (ErrorsFound) continue;

            for (I = 1; I <= VarSpeedCoil.NumOfSpeeds; ++I) {
                VarSpeedCoil.MSRatedPercentTotCap(I) =
                    VarSpeedCoil.MSRatedTotCap(I) /
                    VarSpeedCoil.MSRatedTotCap(
                        VarSpeedCoil.NumOfSpeeds);
                VarSpeedCoil.MSRatedAirVolFlowPerRatedTotCap(I) =
                    VarSpeedCoil.MSRatedAirVolFlowRate(I) /
                    VarSpeedCoil.MSRatedTotCap(I);
            }

            // CurrentModuleObject = "Coil:Heating:DX:Variablespeed "
            SetupOutputVariable(state,
                                "Heating Coil Electricity Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.Energy,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name,
                                _,
                                "Electricity",
                                "Heating",
                                _,
                                "System");
            SetupOutputVariable(state,
                                "Heating Coil Heating Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergyLoadTotal,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name,
                                _,
                                "ENERGYTRANSFER",
                                "HEATINGCOILS",
                                _,
                                "System");
            SetupOutputVariable(state,
                                "Heating Coil Source Side Heat Transfer Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergySource,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name);

            // create predefined report entries
            PreDefTableEntry(
                state, state.dataOutRptPredefined->pdchHeatCoilType, VarSpeedCoil.Name, CurrentModuleObject);
            PreDefTableEntry(state,
                             state.dataOutRptPredefined->pdchHeatCoilNomCap,
                             VarSpeedCoil.Name,
                             VarSpeedCoil.RatedCapHeat);
            PreDefTableEntry(state,
                             state.dataOutRptPredefined->pdchHeatCoilNomEff,
                             VarSpeedCoil.Name,
                             VarSpeedCoil.MSRatedCOP(
                                 VarSpeedCoil.NormSpedLevel));
        }

        //-------------------------AIR SOURCE HEATING---END

        //------------------------VARIABLE-SPEED AIR SOURCE HPWH---BEGIN
        CurrentModuleObject = "COIL:WATERHEATING:AIRTOWATERHEATPUMP:VARIABLESPEED"; // for reporting

        for (CoilCounter = 1; CoilCounter <= NumHPWHAirToWater; ++CoilCounter) {

            ++DXCoilNum;
            AlfaFieldIncre = 1;

            state.dataInputProcessing->inputProcessor->getObjectItem(state,
                                                                     CurrentModuleObject,
                                                                     CoilCounter,
                                                                     AlphArray,
                                                                     NumAlphas,
                                                                     NumArray,
                                                                     NumNums,
                                                                     IOStat,
                                                                     lNumericBlanks,
                                                                     lAlphaBlanks,
                                                                     cAlphaFields,
                                                                     cNumericFields);
            // ErrorsFound will be set to True if problem was found, left untouched otherwise
            VerifyUniqueCoilName(state, CurrentModuleObject, AlphArray(1), ErrorsFound, CurrentModuleObject + " Name");

            auto &VarSpeedCoil = state.dataVariableSpeedCoils->VarSpeedCoil(DXCoilNum);
            VarSpeedCoil.bIsDesuperheater = false;
            VarSpeedCoil.CondenserType = DataHeatBalance::RefrigCondenserType::WaterHeater;
            VarSpeedCoil.CoolHeatType = "WATERHEATING";
            VarSpeedCoil.VSCoilTypeOfNum = CoilDX_HeatPumpWaterHeaterVariableSpeed;
            VarSpeedCoil.VarSpeedCoilType = cAllCoilTypes(CoilDX_HeatPumpWaterHeaterVariableSpeed);

            VarSpeedCoil.Name = AlphArray(1);
            VarSpeedCoil.NumOfSpeeds = int(NumArray(1));
            VarSpeedCoil.NormSpedLevel = int(NumArray(2));

            if (VarSpeedCoil.NumOfSpeeds < 1) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be >= 1. entered number is {:.0T}", cNumericFields(1), NumArray(1)));
                ErrorsFound = true;
            }

            if (VarSpeedCoil.NormSpedLevel >
                VarSpeedCoil.NumOfSpeeds) {
                VarSpeedCoil.NormSpedLevel =
                    VarSpeedCoil.NumOfSpeeds;
            }

            if ((VarSpeedCoil.NormSpedLevel >
                 VarSpeedCoil.NumOfSpeeds) ||
                (VarSpeedCoil.NormSpedLevel <= 0)) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be valid speed level entered number is {:.0T}", cNumericFields(2), NumArray(2)));
                ErrorsFound = true;
            }

            VarSpeedCoil.RatedCapWH = NumArray(3);
            if (VarSpeedCoil.RatedCapWH <= 0.0) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be > 0.0, entered value=[{:.2T}].", cNumericFields(3), NumArray(3)));
                ErrorsFound = true;
            }

            VarSpeedCoil.WHRatedInletDBTemp = NumArray(4);
            VarSpeedCoil.WHRatedInletWBTemp = NumArray(5);
            VarSpeedCoil.WHRatedInletWaterTemp = NumArray(6);

            VarSpeedCoil.RatedAirVolFlowRate = NumArray(7);
            VarSpeedCoil.RatedWaterVolFlowRate = NumArray(8);

            if (VarSpeedCoil.RatedAirVolFlowRate != DataGlobalConstants::AutoCalculate) {
                if (VarSpeedCoil.RatedAirVolFlowRate <= 0.0) {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", invalid");
                    ShowContinueError(state, format("...{} must be > 0.0.  entered value=[{:.3T}].", cNumericFields(7), NumArray(7)));
                    ErrorsFound = true;
                }
            }

            if (VarSpeedCoil.RatedWaterVolFlowRate != DataGlobalConstants::AutoCalculate) {
                if (VarSpeedCoil.RatedWaterVolFlowRate <= 0.0) {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", invalid");
                    ShowContinueError(state, format("...{} must be > 0.0  entered value=[{:.3T}].", cNumericFields(8), NumArray(8)));
                    ErrorsFound = true;
                }
            }

            if (UtilityRoutines::SameString(AlphArray(2), "Yes") || UtilityRoutines::SameString(AlphArray(2), "No")) {
                //  initialized to TRUE on allocate
                if (UtilityRoutines::SameString(AlphArray(2), "No"))
                    VarSpeedCoil.FanPowerIncludedInCOP = false;
                else
                    VarSpeedCoil.FanPowerIncludedInCOP = true;
            } else {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, ",,,invalid choice for " + cAlphaFields(2) + ".  Entered choice = " + AlphArray(2));
                ShowContinueError(state, "Valid choices are Yes or No.");
                ErrorsFound = true;
            }

            if (UtilityRoutines::SameString(AlphArray(3), "Yes") || UtilityRoutines::SameString(AlphArray(3), "No")) {
                //  initialized to FALSE on allocate
                if (UtilityRoutines::SameString(AlphArray(3), "Yes"))
                    VarSpeedCoil.CondPumpPowerInCOP = true;
                else
                    VarSpeedCoil.CondPumpPowerInCOP = false;
            } else {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, ",,,invalid choice for " + cAlphaFields(3) + ".  Entered choice = " + AlphArray(3));
                ShowContinueError(state, "Valid choices are Yes or No.");
                ErrorsFound = true;
            }

            if (UtilityRoutines::SameString(AlphArray(4), "Yes") || UtilityRoutines::SameString(AlphArray(4), "No")) {
                //  initialized to FALSE on allocate
                if (UtilityRoutines::SameString(AlphArray(4), "Yes"))
                    VarSpeedCoil.CondPumpHeatInCapacity = true;
                else
                    VarSpeedCoil.CondPumpHeatInCapacity = false;
            } else {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, ",,,invalid choice for " + cAlphaFields(4) + ".  Entered choice = " + AlphArray(4));
                ShowContinueError(state, "Valid choices are Yes or No.");
                ErrorsFound = true;
            }

            VarSpeedCoil.HPWHCondPumpFracToWater = NumArray(9);
            if (VarSpeedCoil.HPWHCondPumpFracToWater <= 0.0 ||
                VarSpeedCoil.HPWHCondPumpFracToWater > 1.0) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be >= 0 and <= 1.  entered value=[{:.3T}].", cNumericFields(10), NumArray(9)));
                ErrorsFound = true;
            }

            if (!VarSpeedCoil.CondPumpHeatInCapacity) {
                VarSpeedCoil.HPWHCondPumpFracToWater = 0.0;
            }

            // Air nodes
            VarSpeedCoil.AirInletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(5),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilWaterHeatingAirToWaterHeatPumpVariableSpeed,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Air,
                                  DataLoopNode::ConnectionType::Inlet,
                                  NodeInputManager::CompFluidStream::Primary,
                                  ObjectIsNotParent);

            VarSpeedCoil.AirOutletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(6),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilWaterHeatingAirToWaterHeatPumpVariableSpeed,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Air,
                                  DataLoopNode::ConnectionType::Outlet,
                                  NodeInputManager::CompFluidStream::Primary,
                                  ObjectIsNotParent);

            TestCompSet(state, CurrentModuleObject, AlphArray(1), AlphArray(5), AlphArray(6), "Air Nodes");

            // Check if the air inlet node is OA node, to justify whether the coil is placed in zone or not
            VarSpeedCoil.IsDXCoilInZone =
                !CheckOutAirNodeNumber(state, VarSpeedCoil.AirInletNodeNum);

            // Water nodes
            VarSpeedCoil.WaterInletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(7),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilWaterHeatingAirToWaterHeatPumpVariableSpeed,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Water,
                                  DataLoopNode::ConnectionType::Inlet,
                                  NodeInputManager::CompFluidStream::Secondary,
                                  ObjectIsNotParent);

            VarSpeedCoil.WaterOutletNodeNum =
                GetOnlySingleNode(state,
                                  AlphArray(8),
                                  ErrorsFound,
                                  DataLoopNode::ConnectionObjectType::CoilWaterHeatingAirToWaterHeatPumpVariableSpeed,
                                  AlphArray(1),
                                  DataLoopNode::NodeFluidType::Water,
                                  DataLoopNode::ConnectionType::Outlet,
                                  NodeInputManager::CompFluidStream::Secondary,
                                  ObjectIsNotParent);

            TestCompSet(state, CurrentModuleObject, AlphArray(1), AlphArray(7), AlphArray(8), "Water Nodes");

            VarSpeedCoil.CrankcaseHeaterCapacity = NumArray(10);
            if (VarSpeedCoil.CrankcaseHeaterCapacity < 0.0) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be >= 0.0  entered value=[{:.1T}].", cNumericFields(10), NumArray(10)));
                ErrorsFound = true;
            }

            VarSpeedCoil.MaxOATCrankcaseHeater = NumArray(11);
            if (VarSpeedCoil.MaxOATCrankcaseHeater < 0.0) {
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, format("...{} must be >= 0 {{C}}.  entered value=[{:.1T}].", cNumericFields(11), NumArray(11)));
                ErrorsFound = true;
            }

            if (UtilityRoutines::SameString(AlphArray(9), "DryBulbTemperature")) {
                VarSpeedCoil.InletAirTemperatureType = DryBulbIndicator;
            } else if (UtilityRoutines::SameString(AlphArray(9), "WetBulbTemperature")) {
                VarSpeedCoil.InletAirTemperatureType = WetBulbIndicator;
            } else {
                //   wrong temperature type selection
                ShowSevereError(state,
                                std::string{RoutineName} + CurrentModuleObject + "=\"" + VarSpeedCoil.Name +
                                    "\", invalid");
                ShowContinueError(state, "..." + cAlphaFields(9) + " must be DryBulbTemperature or WetBulbTemperature.");
                ShowContinueError(state, "...entered value=\"" + AlphArray(9) + "\".");
                ErrorsFound = true;
            }

            // set rated inlet air temperature for curve object verification
            if (VarSpeedCoil.InletAirTemperatureType == WetBulbIndicator) {
                WHInletAirTemp = VarSpeedCoil.WHRatedInletWBTemp;
            } else {
                WHInletAirTemp = VarSpeedCoil.WHRatedInletDBTemp;
            }
            // set rated water temperature for curve object verification
            WHInletWaterTemp = VarSpeedCoil.WHRatedInletWaterTemp;

            // part load curve
            VarSpeedCoil.PLFFPLR = GetCurveIndex(state, AlphArray(10)); // convert curve name to number
            if (VarSpeedCoil.PLFFPLR == 0) {
                if (lAlphaBlanks(10)) {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", missing");
                    ShowContinueError(state, "...required " + cAlphaFields(10) + " is blank.");
                } else {
                    ShowSevereError(state,
                                    std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                        VarSpeedCoil.Name + "\", invalid");
                    ShowContinueError(state, "...not found " + cAlphaFields(10) + "=\"" + AlphArray(10) + "\".");
                }
                ErrorsFound = true;
            } else {
                CurveVal = CurveValue(state, VarSpeedCoil.PLFFPLR, 1.0);
                if (CurveVal > 1.10 || CurveVal < 0.90) {
                    ShowWarningError(state,
                                     std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                         VarSpeedCoil.Name + "\", curve values");
                    ShowContinueError(state,
                                      "..." + cAlphaFields(10) +
                                          " output is not equal to 1.0 "
                                          "(+ or - 10%) at rated conditions.");
                    ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                }
            }

            for (I = 1; I <= VarSpeedCoil.NumOfSpeeds; ++I) {
                VarSpeedCoil.MSRatedTotCap(I) = NumArray(12 + (I - 1) * 6);
                VarSpeedCoil.MSRatedCOP(I) = NumArray(13 + (I - 1) * 6);
                VarSpeedCoil.MSRatedSHR(I) = NumArray(14 + (I - 1) * 6);
                VarSpeedCoil.MSRatedAirVolFlowRate(I) = NumArray(15 + (I - 1) * 6);
                VarSpeedCoil.MSRatedWaterVolFlowRate(I) = NumArray(16 + (I - 1) * 6);
                VarSpeedCoil.MSWHPumpPower(I) = NumArray(17 + (I - 1) * 6);

                AlfaFieldIncre = 11 + (I - 1) * 6;
                VarSpeedCoil.MSCCapFTemp(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSCCapFTemp(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is BiQuadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSCCapFTemp(I), // Curve index
                                                                {2},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal =
                            CurveValue(state, VarSpeedCoil.MSCCapFTemp(I), WHInletAirTemp, WHInletWaterTemp);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) +
                                                  " output is not equal to 1.0 "
                                                  "(+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 12 + (I - 1) * 6;
                VarSpeedCoil.MSCCapAirFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSCCapAirFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |=
                        CurveManager::CheckCurveDims(state,
                                                     VarSpeedCoil.MSCCapAirFFlow(I), // Curve index
                                                     {1},                                                                     // Valid dimensions
                                                     RoutineName,                                                             // Routine name
                                                     CurrentModuleObject,                                                     // Object Type
                                                     VarSpeedCoil.Name,              // Object Name
                                                     cAlphaFields(AlfaFieldIncre));                                           // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSCCapAirFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) +
                                                  " output is not equal to 1.0 "
                                                  "(+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 13 + (I - 1) * 6;
                VarSpeedCoil.MSCCapWaterFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSCCapWaterFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |=
                        CurveManager::CheckCurveDims(state,
                                                     VarSpeedCoil.MSCCapWaterFFlow(I), // Curve index
                                                     {1},                                                                       // Valid dimensions
                                                     RoutineName,                                                               // Routine name
                                                     CurrentModuleObject,                                                       // Object Type
                                                     VarSpeedCoil.Name,                // Object Name
                                                     cAlphaFields(AlfaFieldIncre));                                             // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSCCapWaterFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) +
                                                  " output is not equal to 1.0 "
                                                  "(+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 14 + (I - 1) * 6;
                VarSpeedCoil.MSEIRFTemp(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSEIRFTemp(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is BiQuadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSEIRFTemp(I), // Curve index
                                                                {2},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal =
                            CurveValue(state, VarSpeedCoil.MSEIRFTemp(I), WHInletAirTemp, WHInletWaterTemp);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) +
                                                  " output is not equal to 1.0 "
                                                  "(+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 15 + (I - 1) * 6;
                VarSpeedCoil.MSEIRAirFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSEIRAirFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |= CurveManager::CheckCurveDims(state,
                                                                VarSpeedCoil.MSEIRAirFFlow(I), // Curve index
                                                                {1},                                                        // Valid dimensions
                                                                RoutineName,                                                // Routine name
                                                                CurrentModuleObject,                                        // Object Type
                                                                VarSpeedCoil.Name, // Object Name
                                                                cAlphaFields(AlfaFieldIncre));                              // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) +
                                                  " output is not equal to 1.0 "
                                                  "(+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }

                AlfaFieldIncre = 16 + (I - 1) * 6;
                VarSpeedCoil.MSEIRWaterFFlow(I) =
                    GetCurveIndex(state, AlphArray(AlfaFieldIncre)); // convert curve name to number
                if (VarSpeedCoil.MSEIRWaterFFlow(I) == 0) {
                    if (lAlphaBlanks(AlfaFieldIncre)) {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", missing");
                        ShowContinueError(state, "...required " + cAlphaFields(AlfaFieldIncre) + " is blank.");
                    } else {
                        ShowSevereError(state,
                                        std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                            VarSpeedCoil.Name + "\", invalid");
                        ShowContinueError(state, "...not found " + cAlphaFields(AlfaFieldIncre) + "=\"" + AlphArray(AlfaFieldIncre) + "\".");
                    }
                    ErrorsFound = true;
                } else {
                    // Verify Curve Object, only legal type is Quadratic
                    ErrorsFound |=
                        CurveManager::CheckCurveDims(state,
                                                     VarSpeedCoil.MSEIRWaterFFlow(I), // Curve index
                                                     {1},                                                                      // Valid dimensions
                                                     RoutineName,                                                              // Routine name
                                                     CurrentModuleObject,                                                      // Object Type
                                                     VarSpeedCoil.Name,               // Object Name
                                                     cAlphaFields(AlfaFieldIncre));                                            // Field Name

                    if (!ErrorsFound) {
                        CurveVal = CurveValue(state, VarSpeedCoil.MSEIRWaterFFlow(I), 1.0);
                        if (CurveVal > 1.10 || CurveVal < 0.90) {
                            ShowWarningError(state,
                                             std::string{RoutineName} + CurrentModuleObject + "=\"" +
                                                 VarSpeedCoil.Name + "\", curve values");
                            ShowContinueError(state,
                                              "..." + cAlphaFields(AlfaFieldIncre) +
                                                  " output is not equal to 1.0 "
                                                  "(+ or - 10%) at rated conditions.");
                            ShowContinueError(state, format("...Curve output at rated conditions = {:.3T}", CurveVal));
                        }
                    }
                }
            }

            // get scale values
            for (I = 1; I <= VarSpeedCoil.NumOfSpeeds; ++I) {
                VarSpeedCoil.MSRatedPercentTotCap(I) =
                    VarSpeedCoil.MSRatedTotCap(I) /
                    VarSpeedCoil.MSRatedTotCap(
                        VarSpeedCoil.NumOfSpeeds);
                VarSpeedCoil.MSRatedAirVolFlowPerRatedTotCap(I) =
                    VarSpeedCoil.MSRatedAirVolFlowRate(I) /
                    VarSpeedCoil.MSRatedTotCap(I);
                VarSpeedCoil.MSRatedWaterVolFlowPerRatedTotCap(I) =
                    VarSpeedCoil.MSRatedWaterVolFlowRate(I) /
                    VarSpeedCoil.MSRatedTotCap(I);
                VarSpeedCoil.MSWHPumpPowerPerRatedTotCap(I) =
                    VarSpeedCoil.MSWHPumpPower(I) /
                    VarSpeedCoil.MSRatedTotCap(I);
            }

            // CurrentModuleObject = "Coil:Waterheating:Airtowaterheatpump:Variablespeed"
            SetupOutputVariable(state,
                                "Cooling Coil Electricity Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.Energy,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name,
                                _,
                                "Electricity",
                                "Heating",
                                _,
                                "System");
            SetupOutputVariable(state,
                                "Cooling Coil Sensible Cooling Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergySensible,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name);
            SetupOutputVariable(state,
                                "Cooling Coil Latent Cooling Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergyLatent,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name);
            SetupOutputVariable(state,
                                "Cooling Coil Water Side Heat Transfer Energy",
                                OutputProcessor::Unit::J,
                                VarSpeedCoil.EnergySource,
                                OutputProcessor::SOVTimeStepType::System,
                                OutputProcessor::SOVStoreType::Summed,
                                VarSpeedCoil.Name,
                                _,
                                "PLANTLOOPHEATINGDEMAND",
                                "HEATINGCOILS",
                                _,
                                "System");

            if (VarSpeedCoil.IsDXCoilInZone) {
                SetupOutputVariable(state,
                                    "Cooling Coil Cooling Energy",
                                    OutputProcessor::Unit::J,
                                    VarSpeedCoil.EnergyLoadTotal,
                                    OutputProcessor::SOVTimeStepType::System,
                                    OutputProcessor::SOVStoreType::Summed,
                                    VarSpeedCoil.Name,
                                    _,
                                    "ENERGYTRANSFER",
                                    "COOLINGCOILS",
                                    _,
                                    "System");
            } else {
                SetupOutputVariable(state,
                                    "Cooling Coil Cooling Energy",
                                    OutputProcessor::Unit::J,
                                    VarSpeedCoil.EnergyLoadTotal,
                                    OutputProcessor::SOVTimeStepType::System,
                                    OutputProcessor::SOVStoreType::Summed,
                                    VarSpeedCoil.Name);
            }

            VarSpeedCoil.RatedCapCoolSens =
                AutoSize; // always auto-sized, to be determined in the sizing calculation
        }
        //---------------------------VARIABLE-SPEED AIR SOURCE HPWH END --------------

        AlphArray.deallocate();
        cAlphaFields.deallocate();
        lAlphaBlanks.deallocate();
        cNumericFields.deallocate();
        lNumericBlanks.deallocate();
        NumArray.deallocate();

        if (ErrorsFound) {
            ShowFatalError(state, std::string{RoutineName} + "Errors found getting input. Program terminates.");
        }

        for (DXCoilNum = 1; DXCoilNum <= state.dataVariableSpeedCoils->NumVarSpeedCoils; ++DXCoilNum) {
            auto &VarSpeedCoil = state.dataVariableSpeedCoils->VarSpeedCoil(DXCoilNum);
            if ((VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) ||
                (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed)) {
                // Setup Report variables for the Heat Pump

                // cooling and heating coils separately
                if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) {
                    // air source cooling coils
                    SetupOutputVariable(state,
                                        "Cooling Coil Air Mass Flow Rate",
                                        OutputProcessor::Unit::kg_s,
                                        VarSpeedCoil.AirMassFlowRate,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Air Inlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.InletAirDBTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Air Inlet Humidity Ratio",
                                        OutputProcessor::Unit::kgWater_kgDryAir,
                                        VarSpeedCoil.InletAirHumRat,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Latent Cooling Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QLatent,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Air Outlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.OutletAirDBTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Air Outlet Humidity Ratio",
                                        OutputProcessor::Unit::kgWater_kgDryAir,
                                        VarSpeedCoil.OutletAirHumRat,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Sensible Cooling Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QSensible,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Total Cooling Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QLoadTotal,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Part Load Ratio",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.PartLoadRatio,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Electricity Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.Power,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Runtime Fraction",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.RunFrac,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Source Side Heat Transfer Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QSource,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Upper Speed Level",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.SpeedNumReport,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Neighboring Speed Levels Ratio",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.SpeedRatioReport,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);

                    if (VarSpeedCoil.CondensateCollectMode == CondensateToTank) {
                        SetupOutputVariable(state,
                                            "Cooling Coil Condensate Volume Flow Rate",
                                            OutputProcessor::Unit::m3_s,
                                            VarSpeedCoil.CondensateVdot,
                                            OutputProcessor::SOVTimeStepType::System,
                                            OutputProcessor::SOVStoreType::Average,
                                            VarSpeedCoil.Name);
                        SetupOutputVariable(state,
                                            "Cooling Coil Condensate Volume",
                                            OutputProcessor::Unit::m3,
                                            VarSpeedCoil.CondensateVol,
                                            OutputProcessor::SOVTimeStepType::System,
                                            OutputProcessor::SOVStoreType::Summed,
                                            VarSpeedCoil.Name,
                                            _,
                                            "OnSiteWater",
                                            "Condensate",
                                            _,
                                            "System");
                    }

                    if (VarSpeedCoil.ReportEvapCondVars) {
                        SetupOutputVariable(state,
                                            "Cooling Coil Condenser Inlet Temperature",
                                            OutputProcessor::Unit::C,
                                            VarSpeedCoil.CondInletTemp,
                                            OutputProcessor::SOVTimeStepType::System,
                                            OutputProcessor::SOVStoreType::Average,
                                            VarSpeedCoil.Name);
                        SetupOutputVariable(state,
                                            "Cooling Coil Evaporative Condenser Water Volume",
                                            OutputProcessor::Unit::m3,
                                            VarSpeedCoil.EvapWaterConsump,
                                            OutputProcessor::SOVTimeStepType::System,
                                            OutputProcessor::SOVStoreType::Summed,
                                            VarSpeedCoil.Name,
                                            _,
                                            "Water",
                                            "Cooling",
                                            _,
                                            "System");
                        SetupOutputVariable(state,
                                            "Cooling Coil Evaporative Condenser Mains Water Volume",
                                            OutputProcessor::Unit::m3,
                                            VarSpeedCoil.EvapWaterConsump,
                                            OutputProcessor::SOVTimeStepType::System,
                                            OutputProcessor::SOVStoreType::Summed,
                                            VarSpeedCoil.Name,
                                            _,
                                            "MainsWater",
                                            "Cooling",
                                            _,
                                            "System");
                        SetupOutputVariable(state,
                                            "Cooling Coil Evaporative Condenser Pump Electricity Rate",
                                            OutputProcessor::Unit::W,
                                            VarSpeedCoil.EvapCondPumpElecPower,
                                            OutputProcessor::SOVTimeStepType::System,
                                            OutputProcessor::SOVStoreType::Average,
                                            VarSpeedCoil.Name);
                        SetupOutputVariable(state,
                                            "Cooling Coil Evaporative Condenser Pump Electricity Energy",
                                            OutputProcessor::Unit::J,
                                            VarSpeedCoil.EvapCondPumpElecConsumption,
                                            OutputProcessor::SOVTimeStepType::System,
                                            OutputProcessor::SOVStoreType::Summed,
                                            VarSpeedCoil.Name,
                                            _,
                                            "Electricity",
                                            "COOLING",
                                            _,
                                            "System");
                        if (VarSpeedCoil.BasinHeaterPowerFTempDiff > 0.0) {
                            SetupOutputVariable(state,
                                                "Cooling Coil Basin Heater Electricity Rate",
                                                OutputProcessor::Unit::W,
                                                VarSpeedCoil.BasinHeaterPower,
                                                OutputProcessor::SOVTimeStepType::System,
                                                OutputProcessor::SOVStoreType::Average,
                                                VarSpeedCoil.Name);
                            SetupOutputVariable(state,
                                                "Cooling Coil Basin Heater Electricity Energy",
                                                OutputProcessor::Unit::J,
                                                VarSpeedCoil.BasinHeaterConsumption,
                                                OutputProcessor::SOVTimeStepType::System,
                                                OutputProcessor::SOVStoreType::Summed,
                                                VarSpeedCoil.Name,
                                                _,
                                                "Electricity",
                                                "COOLING",
                                                _,
                                                "System");
                        }
                    }

                    SetupOutputVariable(state,
                                        "Cooling Coil Crankcase Heater Electricity Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.CrankcaseHeaterPower,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Crankcase Heater Electricity Energy",
                                        OutputProcessor::Unit::J,
                                        VarSpeedCoil.CrankcaseHeaterConsumption,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Summed,
                                        VarSpeedCoil.Name,
                                        _,
                                        "Electricity",
                                        "COOLING",
                                        _,
                                        "System");
                } else {
                    // air source heating coils
                    SetupOutputVariable(state,
                                        "Heating Coil Air Mass Flow Rate",
                                        OutputProcessor::Unit::kg_s,
                                        VarSpeedCoil.AirMassFlowRate,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Air Inlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.InletAirDBTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Air Inlet Humidity Ratio",
                                        OutputProcessor::Unit::kgWater_kgDryAir,
                                        VarSpeedCoil.InletAirHumRat,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Air Outlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.OutletAirDBTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Air Outlet Humidity Ratio",
                                        OutputProcessor::Unit::kgWater_kgDryAir,
                                        VarSpeedCoil.OutletAirHumRat,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Sensible Heating Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QSensible,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Heating Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QLoadTotal,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Part Load Ratio",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.PartLoadRatio,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Electricity Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.Power,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Runtime Fraction",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.RunFrac,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);

                    SetupOutputVariable(state,
                                        "Heating Coil Source Side Heat Transfer Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QSource,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Upper Speed Level",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.SpeedNumReport,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Neighboring Speed Levels Ratio",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.SpeedRatioReport,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);

                    SetupOutputVariable(state,
                                        "Heating Coil Defrost Electricity Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.DefrostPower,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Defrost Electricity Energy",
                                        OutputProcessor::Unit::J,
                                        VarSpeedCoil.DefrostConsumption,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Summed,
                                        VarSpeedCoil.Name,
                                        _,
                                        "Electricity",
                                        "HEATING",
                                        _,
                                        "System");
                    SetupOutputVariable(state,
                                        "Heating Coil Crankcase Heater Electricity Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.CrankcaseHeaterPower,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Crankcase Heater Electricity Energy",
                                        OutputProcessor::Unit::J,
                                        VarSpeedCoil.CrankcaseHeaterConsumption,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Summed,
                                        VarSpeedCoil.Name,
                                        _,
                                        "Electricity",
                                        "HEATING",
                                        _,
                                        "System");
                }
            } else {

                if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingWaterToAirHPVSEquationFit) { // fix coil type
                    // cooling WAHP coil
                    // Setup Report variables for water source Heat Pump
                    SetupOutputVariable(state,
                                        "Cooling Coil Electricity Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.Power,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Total Cooling Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QLoadTotal,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Sensible Cooling Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QSensible,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Latent Cooling Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QLatent,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Source Side Heat Transfer Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QSource,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Part Load Ratio",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.PartLoadRatio,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Runtime Fraction",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.RunFrac,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);

                    SetupOutputVariable(state,
                                        "Cooling Coil Air Mass Flow Rate",
                                        OutputProcessor::Unit::kg_s,
                                        VarSpeedCoil.AirMassFlowRate,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Air Inlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.InletAirDBTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Air Inlet Humidity Ratio",
                                        OutputProcessor::Unit::kgWater_kgDryAir,
                                        VarSpeedCoil.InletAirHumRat,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Air Outlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.OutletAirDBTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Air Outlet Humidity Ratio",
                                        OutputProcessor::Unit::kgWater_kgDryAir,
                                        VarSpeedCoil.OutletAirHumRat,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Source Side Mass Flow Rate",
                                        OutputProcessor::Unit::kg_s,
                                        VarSpeedCoil.WaterMassFlowRate,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Source Side Inlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.InletWaterTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Source Side Outlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.OutletWaterTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);

                    SetupOutputVariable(state,
                                        "Cooling Coil Upper Speed Level",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.SpeedNumReport,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Neighboring Speed Levels Ratio",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.SpeedRatioReport,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Recoverable Heat Transfer Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QWasteHeat,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                } else if (VarSpeedCoil.VSCoilTypeOfNum ==
                           Coil_HeatingWaterToAirHPVSEquationFit) { // fix coil type
                    // heating WAHP coil
                    // Setup Report variables for water source Heat Pump
                    SetupOutputVariable(state,
                                        "Heating Coil Electricity Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.Power,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Heating Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QLoadTotal,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Sensible Heating Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QSensible,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);

                    SetupOutputVariable(state,
                                        "Heating Coil Source Side Heat Transfer Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QSource,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Part Load Ratio",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.PartLoadRatio,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Runtime Fraction",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.RunFrac,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);

                    SetupOutputVariable(state,
                                        "Heating Coil Air Mass Flow Rate",
                                        OutputProcessor::Unit::kg_s,
                                        VarSpeedCoil.AirMassFlowRate,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Air Inlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.InletAirDBTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Air Inlet Humidity Ratio",
                                        OutputProcessor::Unit::kgWater_kgDryAir,
                                        VarSpeedCoil.InletAirHumRat,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Air Outlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.OutletAirDBTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Air Outlet Humidity Ratio",
                                        OutputProcessor::Unit::kgWater_kgDryAir,
                                        VarSpeedCoil.OutletAirHumRat,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Source Side Mass Flow Rate",
                                        OutputProcessor::Unit::kg_s,
                                        VarSpeedCoil.WaterMassFlowRate,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Source Side Inlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.InletWaterTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Source Side Outlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.OutletWaterTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);

                    SetupOutputVariable(state,
                                        "Heating Coil Upper Speed Level",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.SpeedNumReport,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Neighboring Speed Levels Ratio",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.SpeedRatioReport,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Heating Coil Recoverable Heat Transfer Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QWasteHeat,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                } else if (VarSpeedCoil.VSCoilTypeOfNum == CoilDX_HeatPumpWaterHeaterVariableSpeed) {
                    // air source water heating coil
                    SetupOutputVariable(state,
                                        "Cooling Coil Water Heating Electricity Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.Power,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Total Cooling Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QLoadTotal,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Sensible Cooling Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QSensible,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Latent Cooling Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.QLatent,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Total Water Heating Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.TotalHeatingEnergyRate,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Part Load Ratio",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.PartLoadRatio,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Runtime Fraction",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.RunFrac,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);

                    SetupOutputVariable(state,
                                        "Cooling Coil Air Mass Flow Rate",
                                        OutputProcessor::Unit::kg_s,
                                        VarSpeedCoil.AirMassFlowRate,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Air Inlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.InletAirDBTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Air Inlet Humidity Ratio",
                                        OutputProcessor::Unit::kgWater_kgDryAir,
                                        VarSpeedCoil.InletAirHumRat,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Air Outlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.OutletAirDBTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Air Outlet Humidity Ratio",
                                        OutputProcessor::Unit::kgWater_kgDryAir,
                                        VarSpeedCoil.OutletAirHumRat,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Water Mass Flow Rate",
                                        OutputProcessor::Unit::kg_s,
                                        VarSpeedCoil.WaterMassFlowRate,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Water Inlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.InletWaterTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Water Outlet Temperature",
                                        OutputProcessor::Unit::C,
                                        VarSpeedCoil.OutletWaterTemp,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);

                    SetupOutputVariable(state,
                                        "Cooling Coil Crankcase Heater Electricity Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.CrankcaseHeaterPower,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Crankcase Heater Electricity Energy",
                                        OutputProcessor::Unit::J,
                                        VarSpeedCoil.CrankcaseHeaterConsumption,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Summed,
                                        VarSpeedCoil.Name,
                                        _,
                                        "Electricity",
                                        "HEATING",
                                        _,
                                        "System");

                    SetupOutputVariable(state,
                                        "Cooling Coil Upper Speed Level",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.SpeedNumReport,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Neighboring Speed Levels Ratio",
                                        OutputProcessor::Unit::None,
                                        VarSpeedCoil.SpeedRatioReport,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);

                    SetupOutputVariable(state,
                                        "Cooling Coil Water Heating Pump Electricity Rate",
                                        OutputProcessor::Unit::W,
                                        VarSpeedCoil.HPWHCondPumpElecNomPower,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Average,
                                        VarSpeedCoil.Name);
                    SetupOutputVariable(state,
                                        "Cooling Coil Water Heating Pump Electricity Energy",
                                        OutputProcessor::Unit::J,
                                        VarSpeedCoil.EvapCondPumpElecConsumption,
                                        OutputProcessor::SOVTimeStepType::System,
                                        OutputProcessor::SOVStoreType::Summed,
                                        VarSpeedCoil.Name,
                                        _,
                                        "Electricity",
                                        "HEATING",
                                        _,
                                        "System");
                }
            }
        }

        if (ErrorsFound) {
            ShowFatalError(state,
                           std::string{RoutineName} + "Errors found in getting " + CurrentModuleObject +
                               " input.  Preceding condition(s) causes termination.");
        }
    }

    // Beginning Initialization Section of the Module
    //******************************************************************************

    void InitVarSpeedCoil(EnergyPlusData &state,
                          int const DXCoilNum,                             // Current DXCoilNum under simulation
                          Real64 const MaxONOFFCyclesperHour,              // Maximum cycling rate of heat pump [cycles/hr]
                          Real64 const HPTimeConstant,                     // Heat pump time constant [s]
                          Real64 const FanDelayTime,                       // Fan delay time, time delay for the HP's fan to
                          Real64 const SensLoad,                           // Control zone sensible load[W]
                          Real64 const LatentLoad,                         // Control zone latent load[W]
                          int const CyclingScheme,                         // fan operating mode
                          [[maybe_unused]] Real64 const OnOffAirFlowRatio, // ratio of compressor on flow to average flow over time step
                          Real64 const SpeedRatio,                         // compressor speed ratio
                          int const SpeedNum                               // compressor speed number
    )
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Bo Shen, based on  MODULE WaterToAirHeatPumpSimple:InitSimpleWatertoAirHP
        //       DATE WRITTEN   March, 2012
        //       MODIFIED       Bo Shen, 12/2014, add variable-speed HPWH
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for initializations of the variable speed Water to Air HP Components.

        // METHODOLOGY EMPLOYED:
        // Uses the status flags to trigger initializations.

        // Using/Aliasing
        using FluidProperties::GetDensityGlycol;
        using FluidProperties::GetSpecificHeatGlycol;

        using PlantUtilities::InitComponentNodes;
        using PlantUtilities::ScanPlantLoopsForObject;
        using PlantUtilities::SetComponentFlowRate;
        using Psychrometrics::PsyRhoAirFnPbTdbW;

        // Locals
        // SUBROUTINE ARGUMENT DEFINITIONS:

        // shut off after compressor cycle off  [s]

        // SUBROUTINE PARAMETER DEFINITIONS:
        static constexpr std::string_view RoutineNameSimpleWatertoAirHP("InitSimpleWatertoAirHP");

        // INTERFACE BLOCK SPECIFICATIONS
        // na

        // DERIVED TYPE DEFINITIONS
        // na

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int AirInletNode;                  // Node Number of the air inlet
        int WaterInletNode;                // Node Number of the Water inlet
        Real64 rho;                        // local fluid density
        Real64 Cp;                         // local fluid specific heat
        int SpeedCal;                      // calculated speed level
        bool ErrorsFound(false);           // TRUE when errors found, air loop initialization error
        Real64 RatedVolFlowPerRatedTotCap; // Rated Air Volume Flow Rate divided by Rated Total Capacity [m3/s-W)
        int Mode;                          // Performance mode for MultiMode DX coil; Always 1 for other coil types
        Real64 RatedHeatPumpIndoorAirTemp; // Indoor dry-bulb temperature to heat pump evaporator at rated conditions [C]
        Real64 RatedHeatPumpIndoorHumRat;  // Inlet humidity ratio to heat pump evaporator at rated conditions [kg/kg]
        Real64 WaterFlowScale;             // water flow scaling factor match rated flow rate

        // SUBROUTINE PARAMETER DEFINITIONS:
        static constexpr std::string_view RoutineName("InitVarSpeedCoil");

        if (state.dataVariableSpeedCoils->MyOneTimeFlag) {
            // initialize the environment and sizing flags
            state.dataVariableSpeedCoils->MySizeFlag.allocate(state.dataVariableSpeedCoils->NumVarSpeedCoils);
            state.dataVariableSpeedCoils->MyEnvrnFlag.allocate(state.dataVariableSpeedCoils->NumVarSpeedCoils);
            state.dataVariableSpeedCoils->MyPlantScanFlag.allocate(state.dataVariableSpeedCoils->NumVarSpeedCoils);
            state.dataVariableSpeedCoils->MySizeFlag = true;
            state.dataVariableSpeedCoils->MyEnvrnFlag = true;
            state.dataVariableSpeedCoils->MyPlantScanFlag = true;
            state.dataVariableSpeedCoils->MyOneTimeFlag = false;
        }

        state.dataHVACGlobal->DXCT = 1; // hard-code to non-DOAS sizing routine for cfm/ton until .ISHundredPercentDOASDXCoil member from DXcoils.cc
                                        // is added to VarSpeedCoil object

        auto &VarSpeedCoil = state.dataVariableSpeedCoils->VarSpeedCoil(DXCoilNum);

        // variable-speed heat pump water heating, begin
        if (VarSpeedCoil.VSCoilTypeOfNum == CoilDX_HeatPumpWaterHeaterVariableSpeed &&
            state.dataVariableSpeedCoils->MySizeFlag(DXCoilNum)) {

            ErrorsFound = false;
            SizeVarSpeedCoil(state, VarSpeedCoil, ErrorsFound);
            if (ErrorsFound) {
                ShowFatalError(state, format("{}: Failed to size variable speed coil.", RoutineName));
            }

            //   get rated coil bypass factor excluding fan heat

            state.dataVariableSpeedCoils->MySizeFlag(DXCoilNum) = false;
        }
        // variable-speed heat pump water heating, end

        // water source
        if ((VarSpeedCoil.VSCoilTypeOfNum == DataHVACGlobals::Coil_CoolingWaterToAirHPVSEquationFit) ||
            (VarSpeedCoil.VSCoilTypeOfNum ==
             DataHVACGlobals::Coil_HeatingWaterToAirHPVSEquationFit)) { // fix coil type
            if (state.dataVariableSpeedCoils->MyPlantScanFlag(DXCoilNum) && allocated(state.dataPlnt->PlantLoop)) {
                // switch from coil type numbers in DataHVACGlobals, to coil type numbers in plant.
                DataPlant::PlantEquipmentType CoilVSWAHPType(DataPlant::PlantEquipmentType::Invalid);
                if (VarSpeedCoil.VSCoilTypeOfNum == DataHVACGlobals::Coil_CoolingWaterToAirHPVSEquationFit) {
                    CoilVSWAHPType = DataPlant::PlantEquipmentType::CoilVSWAHPCoolingEquationFit;
                } else if (VarSpeedCoil.VSCoilTypeOfNum ==
                           DataHVACGlobals::Coil_HeatingWaterToAirHPVSEquationFit) {
                    CoilVSWAHPType = DataPlant::PlantEquipmentType::CoilVSWAHPHeatingEquationFit;
                }
                ErrorsFound = false;
                ScanPlantLoopsForObject(state,
                                        VarSpeedCoil.Name,
                                        CoilVSWAHPType,
                                        VarSpeedCoil.plantLoc,
                                        ErrorsFound,
                                        _,
                                        _,
                                        _,
                                        _,
                                        _);
                if (ErrorsFound) {
                    ShowFatalError(state, "InitVarSpeedCoil: Program terminated for previous conditions.");
                }
                state.dataVariableSpeedCoils->MyPlantScanFlag(DXCoilNum) = false;
            }
        } else {
            state.dataVariableSpeedCoils->MyPlantScanFlag(DXCoilNum) = false;
        }

        if (!state.dataGlobal->SysSizingCalc && state.dataVariableSpeedCoils->MySizeFlag(DXCoilNum) &&
            !state.dataVariableSpeedCoils->MyPlantScanFlag(DXCoilNum)) {
            // for each furnace, do the sizing once.
            ErrorsFound = false;
            SizeVarSpeedCoil(state, VarSpeedCoil, ErrorsFound);
            if (ErrorsFound) {
                ShowFatalError(state, format("{}: Failed to size variable speed coil.", RoutineName));
            }

            state.dataVariableSpeedCoils->MySizeFlag(DXCoilNum) = false;

            // Multispeed Cooling
            if ((VarSpeedCoil.VSCoilTypeOfNum == DataHVACGlobals::Coil_CoolingWaterToAirHPVSEquationFit) ||
                (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed)) {
                for (Mode = 1; Mode <= VarSpeedCoil.NumOfSpeeds; ++Mode) {
                    // Check for zero capacity or zero max flow rate
                    if (VarSpeedCoil.MSRatedTotCap(Mode) <= 0.0) {
                        ShowSevereError(state,
                                        format("Sizing: {} {} has zero rated total capacity at speed {}",
                                               VarSpeedCoil.VarSpeedCoilType,
                                               VarSpeedCoil.Name,
                                               Mode));
                        ErrorsFound = true;
                    }
                    if (VarSpeedCoil.MSRatedAirVolFlowRate(Mode) <= 0.0) {
                        ShowSevereError(state,
                                        format("Sizing: {} {} has zero rated air flow rate at speed {}",
                                               VarSpeedCoil.VarSpeedCoilType,
                                               VarSpeedCoil.Name,
                                               Mode));
                        ErrorsFound = true;
                    }
                    if (ErrorsFound) {
                        ShowFatalError(state, "Preceding condition causes termination.");
                    }
                    // Check for valid range of (Rated Air Volume Flow Rate / Rated Total Capacity)
                    RatedVolFlowPerRatedTotCap = VarSpeedCoil.MSRatedAirVolFlowRate(Mode) /
                                                 VarSpeedCoil.MSRatedTotCap(Mode);
                    // note: variable-speed HP can exceed the flow rate restrictions at low speed levels
                    //        IF (((MinRatedAirVolFlowPerRatedTotCap - RatedVolFlowPerRatedTotCap) > SmallDifferenceTest).OR. &
                    //           ((RatedVolFlowPerRatedTotCap - MaxRatedAirVolFlowPerRatedTotCap) > SmallDifferenceTest)) THEN
                    //          CALL ShowSevereError(state,'Sizing: '//TRIM(VarSpeedCoil(DXCoilNum)%VarSpeedCoilType) &
                    //           // ' "'//TRIM(VarSpeedCoil(DXCoilNum)%Name)//  &
                    //                '": Rated air volume flow rate per watt of rated total '// &
                    //                'cooling capacity is out of range at speed '//TRIM(TrimSigDigits(Mode)))
                    //          CALL ShowContinueError &
                    //           ('Min Rated Vol Flow Per Watt=['//TRIM(TrimSigDigits(MinRatedAirVolFlowPerRatedTotCap,3))//'], '// &
                    //           'Rated Vol Flow Per Watt=['//TRIM(TrimSigDigits(RatedVolFlowPerRatedTotCap,3))//'],  &
                    //           Max Rated Vol Flow Per Watt=['// &
                    //           TRIM(TrimSigDigits(MaxRatedAirVolFlowPerRatedTotCap,3))//']. See Input-Output Reference Manual for valid range.')
                    //        END IF
                    //        VarSpeedCoil(DXCoilNum)%MSRatedAirMassFlowRate(Mode) = VarSpeedCoil(DXCoilNum)%MSRatedAirVolFlowRate(Mode)* &
                    //          PsyRhoAirFnPbTdbW(state, OutBaroPress,RatedInletAirTemp,RatedInletAirHumRat,RoutineName)
                    //        ! get high speed rated coil bypass factor
                    //        VarSpeedCoil(DXCoilNum)%MSRatedCBF(Mode) = CalcCBF(VarSpeedCoil(DXCoilNum)%VarSpeedCoilType, &
                    //               VarSpeedCoil(DXCoilNum)%Name,&
                    //                                           RatedInletAirTemp,RatedInletAirHumRat,VarSpeedCoil(DXCoilNum)%MSRatedTotCap(Mode),&
                    //                                           VarSpeedCoil(DXCoilNum)%MSRatedAirMassFlowRate(Mode), &
                    //                           VarSpeedCoil(DXCoilNum)%MSRatedSHR(Mode))
                }
                // call coil model with everthing set at rating point
                VarSpeedCoil.InletAirDBTemp = RatedInletAirTemp;
                VarSpeedCoil.InletAirHumRat =
                    Psychrometrics::PsyWFnTdbTwbPb(state, RatedInletAirTemp, RatedInletWetBulbTemp, DataEnvironment::StdPressureSeaLevel);
                VarSpeedCoil.InletAirEnthalpy =
                    Psychrometrics::PsyHFnTdbW(RatedInletAirTemp, VarSpeedCoil.InletAirHumRat);
                VarSpeedCoil.InletAirPressure = DataEnvironment::StdPressureSeaLevel;

                VarSpeedCoil.AirMassFlowRate =
                    VarSpeedCoil.RatedAirVolFlowRate *
                    Psychrometrics::PsyRhoAirFnPbTdbW(state,
                                                      DataEnvironment::StdPressureSeaLevel,
                                                      RatedInletAirTemp,
                                                      VarSpeedCoil.InletAirHumRat);
                // store environment data fill back in after rating point calc is over
                Real64 holdOutDryBulbTemp = state.dataEnvrn->OutDryBulbTemp;
                Real64 holdOutHumRat = state.dataEnvrn->OutHumRat;
                Real64 holdOutWetBulb = state.dataEnvrn->OutWetBulbTemp;
                Real64 holdOutBaroPress = state.dataEnvrn->OutBaroPress;
                Real64 ratedOutdoorAirWetBulb = 23.9; // from I/O ref. more precise value?

                state.dataEnvrn->OutDryBulbTemp = RatedAmbAirTemp;
                state.dataEnvrn->OutWetBulbTemp = ratedOutdoorAirWetBulb;
                state.dataEnvrn->OutBaroPress = DataEnvironment::StdPressureSeaLevel; // assume rating is for sea level.
                state.dataEnvrn->OutHumRat =
                    Psychrometrics::PsyWFnTdbTwbPb(state, RatedAmbAirTemp, ratedOutdoorAirWetBulb, DataEnvironment::StdPressureSeaLevel, RoutineName);
                if (VarSpeedCoil.CondenserInletNodeNum > 0) {
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).Temp = RatedAmbAirTemp;
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).HumRat =
                        state.dataEnvrn->OutHumRat;
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).Press =
                        DataEnvironment::StdPressureSeaLevel;
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).OutAirWetBulb =
                        ratedOutdoorAirWetBulb;
                }
                if (VarSpeedCoil.VSCoilTypeOfNum ==
                    DataHVACGlobals::Coil_CoolingWaterToAirHPVSEquationFit) { // need to set water info for WSHP
                    VarSpeedCoil.WaterMassFlowRate =
                        VarSpeedCoil.MSRatedWaterMassFlowRate(
                            VarSpeedCoil.NumOfSpeeds);
                    VarSpeedCoil.InletWaterTemp = RatedInletWaterTemp; // 85 F cooling mode
                    Real64 CpSource = GetSpecificHeatGlycol(
                        state,
                        state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidName,
                        state.dataVariableSpeedCoils->SourceSideInletTemp,
                        state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidIndex,
                        RoutineName);
                    VarSpeedCoil.InletWaterEnthalpy =
                        VarSpeedCoil.InletWaterTemp * CpSource;
                }

                // calculate coil model at rating point
                Real64 runtimeFrac(1.0);
                VarSpeedCoil.DesignAirMassFlowRate =
                    VarSpeedCoil.MSRatedAirMassFlowRate(
                        VarSpeedCoil.NumOfSpeeds);
                VarSpeedCoil.DesignAirVolFlowRate =
                    VarSpeedCoil.MSRatedAirVolFlowRate(
                        VarSpeedCoil.NumOfSpeeds);
                VarSpeedCoil.DesignWaterMassFlowRate =
                    VarSpeedCoil.MSRatedWaterMassFlowRate(
                        VarSpeedCoil.NumOfSpeeds);
                VarSpeedCoil.DesignWaterVolFlowRate =
                    VarSpeedCoil.MSRatedWaterVolFlowRate(
                        VarSpeedCoil.NumOfSpeeds);

                CalcVarSpeedCoilCooling(state,
                                        DXCoilNum,
                                        2.0,
                                        runtimeFrac,
                                        SensLoad,
                                        LatentLoad,
                                        CompressorOperation::On,
                                        1.0,
                                        1.0,
                                        1.0,
                                        VarSpeedCoil.NumOfSpeeds);
                // coil outlets
                Real64 RatedOutletWetBulb(0.0);
                RatedOutletWetBulb = Psychrometrics::PsyTwbFnTdbWPb(state,
                                                                    VarSpeedCoil.OutletAirDBTemp,
                                                                    VarSpeedCoil.OutletAirHumRat,
                                                                    DataEnvironment::StdPressureSeaLevel,
                                                                    RoutineName);
                state.dataRptCoilSelection->coilSelectionReportObj->setRatedCoilConditions(
                    state,
                    VarSpeedCoil.Name,
                    VarSpeedCoil.VarSpeedCoilType,
                    VarSpeedCoil.QLoadTotal, // this is the report variable
                    VarSpeedCoil.QSensible,  // this is the report variable
                    VarSpeedCoil.AirMassFlowRate,
                    VarSpeedCoil.InletAirDBTemp,
                    VarSpeedCoil.InletAirHumRat,
                    RatedInletWetBulbTemp,
                    VarSpeedCoil.OutletAirDBTemp,
                    VarSpeedCoil.OutletAirHumRat,
                    RatedOutletWetBulb,
                    RatedAmbAirTemp,
                    ratedOutdoorAirWetBulb,
                    VarSpeedCoil.MSRatedCBF(
                        VarSpeedCoil.NumOfSpeeds),
                    -999.0); // coil effectiveness not define for DX

                // now replace the outdoor air conditions set above for one time rating point calc
                state.dataEnvrn->OutDryBulbTemp = holdOutDryBulbTemp;
                state.dataEnvrn->OutHumRat = holdOutHumRat;
                state.dataEnvrn->OutWetBulbTemp = holdOutWetBulb;
                state.dataEnvrn->OutBaroPress = holdOutBaroPress;
            }

            // Multispeed Heating
            if ((VarSpeedCoil.VSCoilTypeOfNum == DataHVACGlobals::Coil_HeatingWaterToAirHPVSEquationFit) ||
                (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed)) {
                RatedHeatPumpIndoorAirTemp = 21.11;  // 21.11C or 70F
                RatedHeatPumpIndoorHumRat = 0.00881; // Humidity ratio corresponding to 70F dry bulb/60F wet bulb
                for (Mode = 1; Mode <= VarSpeedCoil.NumOfSpeeds; ++Mode) {

                    VarSpeedCoil.MSRatedAirMassFlowRate(Mode) =
                        VarSpeedCoil.MSRatedAirVolFlowRate(Mode) *
                        PsyRhoAirFnPbTdbW(state, state.dataEnvrn->OutBaroPress, RatedHeatPumpIndoorAirTemp, RatedHeatPumpIndoorHumRat, RoutineName);
                    // Check for valid range of (Rated Air Volume Flow Rate / Rated Total Capacity)
                    RatedVolFlowPerRatedTotCap = VarSpeedCoil.MSRatedAirVolFlowRate(Mode) /
                                                 VarSpeedCoil.MSRatedTotCap(Mode);
                    // note: variable-speed HP can exceed the flow rate restrictions at low speed levels
                    //        IF (((MinRatedAirVolFlowPerRatedTotCap - RatedVolFlowPerRatedTotCap) > SmallDifferenceTest).OR. &
                    //            ((RatedVolFlowperRatedTotCap - MaxRatedAirVolFlowPerRatedTotCap) > SmallDifferenceTest)) THEN
                    //          CALL ShowSevereError(state,'Coil:Heating:DX:MultiSpeed '//TRIM(VarSpeedCoil(DXCoilNum)%Name)//  &
                    //                              ': Rated air volume flow rate per watt of rated total '// &
                    //                'heating capacity is out of range at speed '//TRIM(TrimSigDigits(Mode)))
                    //          CALL ShowContinueError(state, 'Min Rated Vol Flow Per Watt=['//TRIM(TrimSigDigits &
                    //           (MinRatedAirVolFlowPerRatedTotCap,3))//'], '// &
                    //           'Rated Vol Flow Per Watt=['//TRIM(TrimSigDigits(RatedVolFlowPerRatedTotCap,3))//'],  &
                    //               Max Rated Vol Flow Per Watt=['// &
                    //           TRIM(TrimSigDigits(MaxRatedAirVolFlowPerRatedTotCap,3))//']. See Input-Output Reference  &
                    //                Manual for valid range.')
                    //        END IF
                }
                // call coil model with everthing set at rating point
                VarSpeedCoil.InletAirDBTemp = RatedInletAirTempHeat;
                VarSpeedCoil.InletAirHumRat =
                    Psychrometrics::PsyWFnTdbTwbPb(state, RatedInletAirTempHeat, RatedInletWetBulbTemp, DataEnvironment::StdPressureSeaLevel);
                VarSpeedCoil.InletAirEnthalpy =
                    Psychrometrics::PsyHFnTdbW(RatedInletAirTempHeat, VarSpeedCoil.InletAirHumRat);
                VarSpeedCoil.InletAirPressure = DataEnvironment::StdPressureSeaLevel;

                VarSpeedCoil.AirMassFlowRate =
                    VarSpeedCoil.RatedAirVolFlowRate *
                    Psychrometrics::PsyRhoAirFnPbTdbW(state,
                                                      DataEnvironment::StdPressureSeaLevel,
                                                      RatedInletAirTempHeat,
                                                      VarSpeedCoil.InletAirHumRat);
                // store environment data fill back in after rating point calc is over
                Real64 holdOutDryBulbTemp = state.dataEnvrn->OutDryBulbTemp;
                Real64 holdOutHumRat = state.dataEnvrn->OutHumRat;
                Real64 holdOutWetBulb = state.dataEnvrn->OutWetBulbTemp;
                Real64 holdOutBaroPress = state.dataEnvrn->OutBaroPress;

                state.dataEnvrn->OutDryBulbTemp = RatedAmbAirTempHeat;
                state.dataEnvrn->OutWetBulbTemp = RatedAmbAirWBHeat;
                state.dataEnvrn->OutBaroPress = DataEnvironment::StdPressureSeaLevel; // assume rating is for sea level.
                state.dataEnvrn->OutHumRat =
                    Psychrometrics::PsyWFnTdbTwbPb(state, RatedAmbAirTempHeat, RatedAmbAirWBHeat, DataEnvironment::StdPressureSeaLevel, RoutineName);
                if (VarSpeedCoil.CondenserInletNodeNum > 0) {
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).Temp = RatedAmbAirTempHeat;
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).HumRat =
                        state.dataEnvrn->OutHumRat;
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).Press =
                        DataEnvironment::StdPressureSeaLevel;
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).OutAirWetBulb =
                        RatedAmbAirWBHeat;
                }

                if (VarSpeedCoil.VSCoilTypeOfNum ==
                    DataHVACGlobals::Coil_HeatingWaterToAirHPVSEquationFit) { // need to set water info for WSHP
                    VarSpeedCoil.WaterMassFlowRate =
                        VarSpeedCoil.MSRatedWaterMassFlowRate(
                            VarSpeedCoil.NumOfSpeeds);
                    VarSpeedCoil.InletWaterTemp = RatedInletWaterTempHeat; // 21.11C or 70F, heating mode
                    Real64 CpSource = GetSpecificHeatGlycol(
                        state,
                        state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidName,
                        state.dataVariableSpeedCoils->SourceSideInletTemp,
                        state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidIndex,
                        RoutineName);
                    VarSpeedCoil.InletWaterEnthalpy =
                        VarSpeedCoil.InletWaterTemp * CpSource;
                }

                // calculate coil model at rating point
                Real64 runtimeFrac(1.0);
                VarSpeedCoil.DesignAirMassFlowRate =
                    VarSpeedCoil.MSRatedAirMassFlowRate(
                        VarSpeedCoil.NumOfSpeeds);
                VarSpeedCoil.DesignAirVolFlowRate =
                    VarSpeedCoil.MSRatedAirVolFlowRate(
                        VarSpeedCoil.NumOfSpeeds);
                VarSpeedCoil.DesignWaterMassFlowRate =
                    VarSpeedCoil.MSRatedWaterMassFlowRate(
                        VarSpeedCoil.NumOfSpeeds);
                VarSpeedCoil.DesignWaterVolFlowRate =
                    VarSpeedCoil.MSRatedWaterVolFlowRate(
                        VarSpeedCoil.NumOfSpeeds);
                CalcVarSpeedCoilHeating(state,
                                        VarSpeedCoil,
                                        2.0,
                                        runtimeFrac,
                                        SensLoad,
                                        CompressorOperation::On,
                                        1.0,
                                        1.0,
                                        1.0,
                                        VarSpeedCoil.NumOfSpeeds);
                // coil outlets
                Real64 RatedOutletWetBulb(0.0);
                RatedOutletWetBulb = Psychrometrics::PsyTwbFnTdbWPb(state,
                                                                    VarSpeedCoil.OutletAirDBTemp,
                                                                    VarSpeedCoil.OutletAirHumRat,
                                                                    DataEnvironment::StdPressureSeaLevel,
                                                                    RoutineName);
                state.dataRptCoilSelection->coilSelectionReportObj->setRatedCoilConditions(
                    state,
                    VarSpeedCoil.Name,
                    VarSpeedCoil.VarSpeedCoilType,
                    VarSpeedCoil.QLoadTotal, // this is the report variable
                    VarSpeedCoil.QSensible,  // this is the report variable
                    VarSpeedCoil.AirMassFlowRate,
                    VarSpeedCoil.InletAirDBTemp,
                    VarSpeedCoil.InletAirHumRat,
                    RatedInletWetBulbTemp,
                    VarSpeedCoil.OutletAirDBTemp,
                    VarSpeedCoil.OutletAirHumRat,
                    RatedOutletWetBulb,
                    RatedAmbAirTempHeat,
                    RatedAmbAirWBHeat,
                    VarSpeedCoil.MSRatedCBF(
                        VarSpeedCoil.NumOfSpeeds),
                    -999.0); // coil effectiveness not define for DX

                // now replace the outdoor air conditions set above for one time rating point calc
                state.dataEnvrn->OutDryBulbTemp = holdOutDryBulbTemp;
                state.dataEnvrn->OutHumRat = holdOutHumRat;
                state.dataEnvrn->OutWetBulbTemp = holdOutWetBulb;
                state.dataEnvrn->OutBaroPress = holdOutBaroPress;
            }

            // store fan info for coil
            if (VarSpeedCoil.SupplyFan_TypeNum == DataHVACGlobals::FanType_SystemModelObject) {
                if (VarSpeedCoil.SupplyFanIndex > -1) {
                    state.dataRptCoilSelection->coilSelectionReportObj->setCoilSupplyFanInfo(
                        state,
                        VarSpeedCoil.Name,
                        VarSpeedCoil.VarSpeedCoilType,
                        VarSpeedCoil.SupplyFanName,
                        DataAirSystems::ObjectVectorOOFanSystemModel,
                        VarSpeedCoil.SupplyFanIndex);
                }

            } else {
                if (VarSpeedCoil.SupplyFanIndex > 0) {
                    state.dataRptCoilSelection->coilSelectionReportObj->setCoilSupplyFanInfo(
                        state,
                        VarSpeedCoil.Name,
                        VarSpeedCoil.VarSpeedCoilType,
                        VarSpeedCoil.SupplyFanName,
                        DataAirSystems::StructArrayLegacyFanModels,
                        VarSpeedCoil.SupplyFanIndex);
                }
            }
        }

        if (SpeedNum > VarSpeedCoil.NumOfSpeeds) {
            SpeedCal = VarSpeedCoil.NumOfSpeeds;
        } else if (SpeedNum < 1) {
            SpeedCal = 1;
        } else {
            SpeedCal = SpeedNum;
        }

        if ((SpeedNum <= 1) || (SpeedNum > VarSpeedCoil.NumOfSpeeds)) {
            VarSpeedCoil.DesignAirMassFlowRate =
                VarSpeedCoil.MSRatedAirMassFlowRate(SpeedCal);
            VarSpeedCoil.DesignAirVolFlowRate =
                VarSpeedCoil.MSRatedAirVolFlowRate(SpeedCal);
            VarSpeedCoil.DesignWaterMassFlowRate =
                VarSpeedCoil.MSRatedWaterMassFlowRate(SpeedCal);
            VarSpeedCoil.DesignWaterVolFlowRate =
                VarSpeedCoil.MSRatedWaterVolFlowRate(SpeedCal);
        } else {
            VarSpeedCoil.DesignAirMassFlowRate =
                VarSpeedCoil.MSRatedAirMassFlowRate(SpeedCal) * SpeedRatio +
                (1.0 - SpeedRatio) * VarSpeedCoil.MSRatedAirMassFlowRate(SpeedCal - 1);
            VarSpeedCoil.DesignAirVolFlowRate =
                VarSpeedCoil.MSRatedAirVolFlowRate(SpeedCal) * SpeedRatio +
                (1.0 - SpeedRatio) * VarSpeedCoil.MSRatedAirVolFlowRate(SpeedCal - 1);
            VarSpeedCoil.DesignWaterMassFlowRate =
                VarSpeedCoil.MSRatedWaterMassFlowRate(SpeedCal) * SpeedRatio +
                (1.0 - SpeedRatio) * VarSpeedCoil.MSRatedWaterMassFlowRate(SpeedCal - 1);
            VarSpeedCoil.DesignWaterVolFlowRate =
                VarSpeedCoil.MSRatedWaterVolFlowRate(SpeedCal) * SpeedRatio +
                (1.0 - SpeedRatio) * VarSpeedCoil.MSRatedWaterVolFlowRate(SpeedCal - 1);
        }

        // Do the Begin Environment initializations
        if (state.dataGlobal->BeginEnvrnFlag && state.dataVariableSpeedCoils->MyEnvrnFlag(DXCoilNum) &&
            !state.dataVariableSpeedCoils->MyPlantScanFlag(DXCoilNum)) {
            // Do the initializations to start simulation

            AirInletNode = VarSpeedCoil.AirInletNodeNum;

            // Initialize all report variables to a known state at beginning of simulation
            VarSpeedCoil.AirVolFlowRate = 0.0;
            VarSpeedCoil.InletAirDBTemp = 0.0;
            VarSpeedCoil.InletAirHumRat = 0.0;
            VarSpeedCoil.OutletAirDBTemp = 0.0;
            VarSpeedCoil.OutletAirHumRat = 0.0;
            VarSpeedCoil.WaterVolFlowRate = 0.0;
            VarSpeedCoil.WaterMassFlowRate = 0.0;
            VarSpeedCoil.InletWaterTemp = 0.0;
            VarSpeedCoil.InletWaterEnthalpy = 0.0;
            VarSpeedCoil.OutletWaterEnthalpy = 0.0;
            VarSpeedCoil.OutletWaterTemp = 0.0;
            VarSpeedCoil.Power = 0.0;
            VarSpeedCoil.QLoadTotal = 0.0;
            VarSpeedCoil.QSensible = 0.0;
            VarSpeedCoil.QLatent = 0.0;
            VarSpeedCoil.QSource = 0.0;
            VarSpeedCoil.Energy = 0.0;
            VarSpeedCoil.EnergyLoadTotal = 0.0;
            VarSpeedCoil.EnergySensible = 0.0;
            VarSpeedCoil.EnergyLatent = 0.0;
            VarSpeedCoil.EnergySource = 0.0;
            VarSpeedCoil.COP = 0.0;
            VarSpeedCoil.RunFrac = 0.0;
            VarSpeedCoil.PartLoadRatio = 0.0;

            VarSpeedCoil.MaxONOFFCyclesperHour = MaxONOFFCyclesperHour;
            VarSpeedCoil.HPTimeConstant = HPTimeConstant;
            VarSpeedCoil.FanDelayTime = FanDelayTime;

            if ((VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingWaterToAirHPVSEquationFit) ||
                (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingWaterToAirHPVSEquationFit)) {
                WaterInletNode = VarSpeedCoil.WaterInletNodeNum;

                rho = GetDensityGlycol(state,
                                       state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidName,
                                       DataGlobalConstants::CWInitConvTemp,
                                       state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidIndex,
                                       RoutineNameSimpleWatertoAirHP);
                Cp = GetSpecificHeatGlycol(
                    state,
                    state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidName,
                    DataGlobalConstants::CWInitConvTemp,
                    state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidIndex,
                    RoutineNameSimpleWatertoAirHP);

                //    VarSpeedCoil(DXCoilNum)%DesignWaterMassFlowRate= &
                //                             rho * VarSpeedCoil(DXCoilNum)%RatedWaterVolFlowRate

                InitComponentNodes(state,
                                   0.0,
                                   VarSpeedCoil.DesignWaterMassFlowRate,
                                   VarSpeedCoil.WaterInletNodeNum,
                                   VarSpeedCoil.WaterOutletNodeNum);

                state.dataLoopNodes->Node(WaterInletNode).Temp = 5.0;
                state.dataLoopNodes->Node(WaterInletNode).Enthalpy = Cp * state.dataLoopNodes->Node(WaterInletNode).Temp;
                state.dataLoopNodes->Node(WaterInletNode).Quality = 0.0;
                state.dataLoopNodes->Node(WaterInletNode).Press = 0.0;
                state.dataLoopNodes->Node(WaterInletNode).HumRat = 0.0;

                state.dataLoopNodes->Node(VarSpeedCoil.WaterOutletNodeNum).Temp = 5.0;
                state.dataLoopNodes->Node(VarSpeedCoil.WaterOutletNodeNum).Enthalpy =
                    Cp * state.dataLoopNodes->Node(WaterInletNode).Temp;
                state.dataLoopNodes->Node(VarSpeedCoil.WaterOutletNodeNum).Quality = 0.0;
                state.dataLoopNodes->Node(VarSpeedCoil.WaterOutletNodeNum).Press = 0.0;
                state.dataLoopNodes->Node(VarSpeedCoil.WaterOutletNodeNum).HumRat = 0.0;
            }

            VarSpeedCoil.SimFlag = true;
            state.dataHeatBal->HeatReclaimVS_DXCoil(DXCoilNum).AvailCapacity = 0.0;

            state.dataVariableSpeedCoils->MyEnvrnFlag(DXCoilNum) = false;

        } // End If for the Begin Environment initializations

        if (!state.dataGlobal->BeginEnvrnFlag) {
            state.dataVariableSpeedCoils->MyEnvrnFlag(DXCoilNum) = true;
        }

        // Do the following initializations (every time step): This should be the info from
        // the previous components outlets or the node data in this section.
        // First set the conditions for the air into the heat pump model

        // Set water and air inlet nodes

        AirInletNode = VarSpeedCoil.AirInletNodeNum;
        WaterInletNode = VarSpeedCoil.WaterInletNodeNum;

        if ((SensLoad != 0.0 || LatentLoad != 0.0) && (state.dataLoopNodes->Node(AirInletNode).MassFlowRate > 0.0)) {

            if (VarSpeedCoil.MSRatedWaterMassFlowRate(
                    VarSpeedCoil.NormSpedLevel) > 0.0) {
                WaterFlowScale = VarSpeedCoil.RatedWaterMassFlowRate /
                                 VarSpeedCoil.MSRatedWaterMassFlowRate(
                                     VarSpeedCoil.NormSpedLevel);
                VarSpeedCoil.WaterMassFlowRate =
                    VarSpeedCoil.DesignWaterMassFlowRate * WaterFlowScale;
            } else {
                VarSpeedCoil.WaterMassFlowRate = 0.0;
            }

            if (CyclingScheme == ContFanCycCoil) {
                // continuous fan, cycling compressor
                VarSpeedCoil.AirMassFlowRate = state.dataLoopNodes->Node(AirInletNode).MassFlowRate;
                //    VarSpeedCoil(DXCoilNum)%AirMassFlowRate   = VarSpeedCoil(DXCoilNum)%DesignAirVolFlowRate*  &
                //             PsyRhoAirFnPbTdbW(state, OutBaroPress,Node(AirInletNode)%Temp,Node(AirInletNode)%HumRat)
                // If air flow is less than 25% rated flow. Then set air flow to the 25% of rated conditions
                if (VarSpeedCoil.AirMassFlowRate <
                    0.25 * VarSpeedCoil.DesignAirVolFlowRate *
                        PsyRhoAirFnPbTdbW(state,
                                          state.dataEnvrn->OutBaroPress,
                                          state.dataLoopNodes->Node(AirInletNode).Temp,
                                          state.dataLoopNodes->Node(AirInletNode).HumRat)) {
                    VarSpeedCoil.AirMassFlowRate =
                        0.25 * VarSpeedCoil.DesignAirVolFlowRate *
                        PsyRhoAirFnPbTdbW(state,
                                          state.dataEnvrn->OutBaroPress,
                                          state.dataLoopNodes->Node(AirInletNode).Temp,
                                          state.dataLoopNodes->Node(AirInletNode).HumRat);
                }
            } else { // CYCLIC FAN, NOT CORRECTION, WILL BE PROCESSED IN THE FOLLOWING SUBROUTINES
                VarSpeedCoil.AirMassFlowRate = state.dataLoopNodes->Node(AirInletNode).MassFlowRate;
            }

        } else { // heat pump is off
            VarSpeedCoil.WaterMassFlowRate = 0.0;
            VarSpeedCoil.AirMassFlowRate = 0.0;
        }

        if ((VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingWaterToAirHPVSEquationFit) ||
            (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingWaterToAirHPVSEquationFit)) {
            SetComponentFlowRate(state,
                                 VarSpeedCoil.WaterMassFlowRate,
                                 VarSpeedCoil.WaterInletNodeNum,
                                 VarSpeedCoil.WaterOutletNodeNum,
                                 VarSpeedCoil.plantLoc);

            VarSpeedCoil.InletWaterTemp = state.dataLoopNodes->Node(WaterInletNode).Temp;
            VarSpeedCoil.InletWaterEnthalpy = state.dataLoopNodes->Node(WaterInletNode).Enthalpy;
        } else {
            VarSpeedCoil.InletWaterTemp = 0.0;
            VarSpeedCoil.InletWaterEnthalpy = 0.0;
        }

        if (VarSpeedCoil.VSCoilTypeOfNum == CoilDX_HeatPumpWaterHeaterVariableSpeed) {
            VarSpeedCoil.InletWaterTemp = state.dataLoopNodes->Node(WaterInletNode).Temp;
            VarSpeedCoil.InletWaterEnthalpy = state.dataLoopNodes->Node(WaterInletNode).Enthalpy;
        };

        VarSpeedCoil.InletAirDBTemp = state.dataLoopNodes->Node(AirInletNode).Temp;
        VarSpeedCoil.InletAirHumRat = state.dataLoopNodes->Node(AirInletNode).HumRat;
        VarSpeedCoil.InletAirEnthalpy = state.dataLoopNodes->Node(AirInletNode).Enthalpy;

        VarSpeedCoil.MaxONOFFCyclesperHour = MaxONOFFCyclesperHour;
        VarSpeedCoil.HPTimeConstant = HPTimeConstant;
        VarSpeedCoil.FanDelayTime = FanDelayTime;

        VarSpeedCoil.InletAirPressure = state.dataEnvrn->OutBaroPress; // temporary
        // Outlet variables
        VarSpeedCoil.Power = 0.0;
        VarSpeedCoil.QLoadTotal = 0.0;
        VarSpeedCoil.QSensible = 0.0;
        VarSpeedCoil.QLatent = 0.0;
        VarSpeedCoil.QSource = 0.0;
        VarSpeedCoil.QWasteHeat = 0.0;
        VarSpeedCoil.Energy = 0.0;
        VarSpeedCoil.EnergyLoadTotal = 0.0;
        VarSpeedCoil.EnergySensible = 0.0;
        VarSpeedCoil.EnergyLatent = 0.0;
        VarSpeedCoil.EnergySource = 0.0;
        VarSpeedCoil.COP = 0.0;

        VarSpeedCoil.OutletAirDBTemp = 0.0;
        VarSpeedCoil.OutletWaterTemp = 0.0;
        VarSpeedCoil.OutletAirHumRat = 0.0;
        VarSpeedCoil.OutletAirEnthalpy = 0.0;
        VarSpeedCoil.OutletWaterEnthalpy = 0.0;

        // bug fix, must set zeros to the variables below, otherwise can't pass switch DD test
        VarSpeedCoil.CrankcaseHeaterConsumption = 0.0;
        VarSpeedCoil.EvapWaterConsump = 0.0;
        VarSpeedCoil.BasinHeaterConsumption = 0.0;
        VarSpeedCoil.EvapCondPumpElecConsumption = 0.0;
        VarSpeedCoil.CrankcaseHeaterPower = 0.0;
        VarSpeedCoil.DefrostConsumption = 0.0;
        VarSpeedCoil.CondensateVdot = 0.0;
        VarSpeedCoil.CondensateVol = 0.0;
        VarSpeedCoil.QWasteHeat = 0.0;

        // clear zeros to HPWH variables
        VarSpeedCoil.ElecWaterHeatingPower =
            0.0; // Total electric power consumed by compressor and condenser pump [W]
        VarSpeedCoil.ElecWaterHeatingConsumption =
            0.0; // Total electric consumption by compressor and condenser pump [J]
        VarSpeedCoil.TotalHeatingEnergy = 0.0;       // total water heating energy
        VarSpeedCoil.TotalHeatingEnergyRate = 0.0;   // total WH energy rate
        VarSpeedCoil.HPWHCondPumpElecNomPower = 0.0; // power power

        state.dataVariableSpeedCoils->VSHPWHHeatingCapacity = 0.0; // Used by Heat Pump:Water Heater object as total water heating capacity [W]
        state.dataVariableSpeedCoils->VSHPWHHeatingCOP = 0.0;      // Used by Heat Pump:Water Heater object as water heating COP [W/W]
        VarSpeedCoil.OutletWaterTemp = VarSpeedCoil.InletWaterTemp;
        state.dataHeatBal->HeatReclaimVS_DXCoil(DXCoilNum).AvailCapacity = 0.0;
    }

    void SizeVarSpeedCoil(EnergyPlusData &state, VariableSpeedCoilData &VarSpeedCoil, bool &ErrorsFound)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Bo Shen, based on WaterToAirHeatPumpSimple:SizeHVACWaterToAir
        //       DATE WRITTEN   March, 2012
        //       MODIFIED       August 2013 Daeho Kang, add component sizing table entries
        //       MODIFIED       Bo Shen, 12/2014, add variable-speed HPWH
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for sizing WSHP Components for which nominal capacities
        // and flow rates have not been specified in the input

        // METHODOLOGY EMPLOYED:
        // Obtains heating capacities and flow rates from the zone or system sizing arrays.
        // NOTE: For WSHP's we are sizing the heating capacity to be
        // equal to the cooling capacity.  Thus the cooling and
        // and heating capacities of a DX heat pump system will be identical. In real life the ARI
        // heating and cooling capacities are close but not identical.

        // Using/Aliasing
        using namespace Psychrometrics;
        using DataHVACGlobals::SmallAirVolFlow;
        using DataHVACGlobals::SmallLoad;
        using PlantUtilities::MyPlantSizingIndex;
        using PlantUtilities::RegisterPlantCompDesignFlow;
        using namespace OutputReportPredefined;
        using CurveManager::CurveValue;
        using FluidProperties::GetDensityGlycol;
        using FluidProperties::GetSpecificHeatGlycol;

        // Locals
        Real64 QLoadTotal; // placeholder for calculating SHR

        // SUBROUTINE PARAMETER DEFINITIONS:
        static constexpr std::string_view RoutineName("SizeVarSpeedCoil");
        static constexpr std::string_view RoutineNameAlt("SizeHVACWaterToAir");

        auto &ZoneEqSizing(state.dataSize->ZoneEqSizing);

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        Real64 rhoair;
        Real64 MixTemp = -999.0;
        Real64 MixHumRat = -999.0;
        Real64 MixEnth = -999.0;
        Real64 MixWetBulb = -999.0;
        Real64 SupTemp = -999.0;
        Real64 SupHumRat = -999.0;
        Real64 SupEnth = -999.0;
        Real64 OutTemp = -999.0;
        Real64 OutAirFrac = -999.0;
        Real64 VolFlowRate = -999.0;
        Real64 CoolCapAtPeak = -999.0;
        Real64 TotCapTempModFac = -999.0;
        int TimeStepNumAtMax;
        int DDNum;
        int PltSizNum;
        bool RatedCapCoolTotalAutoSized;
        bool RatedCapCoolSensAutoSized;
        bool RatedCapHeatAutoSized;
        bool RatedAirFlowAutoSized;
        bool RatedWaterFlowAutoSized;
        Real64 SystemCapacity;
        Real64 rho;
        Real64 cp;
        int NormSpeed;                       // norminal speed level
        int UpperSpeed;                      // highest speed level
        int Mode;                            // speed level
        Real64 rhoW;                         // water density
        Real64 rhoA;                         // air density
        Real64 SHR;                          // sensible heat transfer ratio
        Real64 RatedAirMassFlowRate;         // rated air mass flow rate
        Real64 CBFRated;                     // bypass factor at the rated condition, considering difference in flow rates
        Real64 RatedInletEnth;               // rated inlet air enthalpy
        Real64 QLoadTotal1;                  // placeholder for calculating SHR
        Real64 QLoadTotal2;                  // placeholder for calculating SHR
        Real64 AirMassFlowRatio;             // air mass flow ratio
        Real64 WaterMassFlowRatio;           // water mass flow rate
        Real64 RatedSourceTempCool;          // rated source temperature, space cooling mode
        std::string CurrentObjSubfix;        // Object subfix type for printing
        bool IsAutoSize;                     // Indicator to autosize
        bool HardSizeNoDesRun;               // Indicator to hardsize withouth sizing runs
        bool HardSizeNoDesRunAirFlow;        // Indicator to hardsize withouth sizing runs for rated air flow field
        Real64 RatedAirVolFlowRateDes;       // Autosized rated air flow for reporting
        Real64 RatedAirVolFlowRateUser;      // Hardsized rated air flow for reporting
        Real64 RatedCapCoolTotalDes;         // Autosized rated total cooling capacity for reporting
        Real64 RatedCapCoolTotalUser;        // Hardsized rated total cooling capacity for reporting
        Real64 RatedCapHeatDes;              // Autosized rated heating capacity for reporting
        Real64 RatedCapHeatUser;             // Hardsized rated heating capacity for reporting
        Real64 RatedWaterVolFlowRateDes;     // Autosized rated water flow for reporting
        Real64 RatedWaterVolFlowRateUser;    // Hardsized rated water flow for reporting
        Real64 RatedCapCoolSensDes;          // Autosized rated sensible cooling capacity for reporting
        Real64 RatedCapCoolSensUser;         // Hardsized rated sensible cooling capacity for reporting
        Real64 EvapCondPumpElecNomPowerDes;  // Autosized evaporative condenser pump power for reporting
        Real64 EvapCondPumpElecNomPowerUser; // Hardsized evaporative condenser pump power for reporting
        Real64 DefrostCapacityDes;           // Autosized resistive defrost heater capacity for reporting
        Real64 DefrostCapacityUser;          // Hardsized resistive defrost heater capacity for reporting
        bool SizingDesRunThisAirSys;         // true if a particular air system had a Sizing:System object and system sizing done
        bool SizingDesRunThisZone;           // true if a particular zone had a Sizing:Zone object and zone sizing was done
        Real64 HPInletAirHumRat;             // Rated inlet air humidity ratio for heat pump water heater [kgWater/kgDryAir]
        Real64 HPWHCoolCapacity;             // estimate cooling capacity in HPWH

        UpperSpeed = VarSpeedCoil.NumOfSpeeds;
        NormSpeed = VarSpeedCoil.NormSpedLevel;
        PltSizNum = 0;
        RatedAirFlowAutoSized = false;
        RatedWaterFlowAutoSized = false;
        RatedCapHeatAutoSized = false;
        IsAutoSize = false;
        if (state.dataSize->SysSizingRunDone || state.dataSize->ZoneSizingRunDone) {
            HardSizeNoDesRun = false;
        } else {
            HardSizeNoDesRun = true;
        }
        if (state.dataSize->CurSysNum > 0) {
            CheckThisAirSystemForSizing(state, state.dataSize->CurSysNum, SizingDesRunThisAirSys);
        } else {
            SizingDesRunThisAirSys = false;
        }
        if (state.dataSize->CurZoneEqNum > 0) {
            CheckThisZoneForSizing(state, state.dataSize->CurZoneEqNum, SizingDesRunThisZone);
        } else {
            SizingDesRunThisZone = false;
        }
        HardSizeNoDesRunAirFlow = false;
        RatedAirVolFlowRateDes = 0.0;
        RatedAirVolFlowRateUser = 0.0;
        RatedCapCoolTotalDes = 0.0;
        RatedCapCoolTotalUser = 0.0;
        RatedCapHeatDes = 0.0;
        RatedCapHeatUser = 0.0;
        RatedWaterVolFlowRateDes = 0.0;
        RatedWaterVolFlowRateUser = 0.0;
        RatedCapCoolSensDes = 0.0;
        RatedCapCoolSensUser = 0.0;
        EvapCondPumpElecNomPowerDes = 0.0;
        EvapCondPumpElecNomPowerUser = 0.0;
        DefrostCapacityDes = 0.0;
        DefrostCapacityUser = 0.0;

        if (VarSpeedCoil.VSCoilTypeOfNum == DataHVACGlobals::Coil_CoolingWaterToAirHPVSEquationFit ||
            VarSpeedCoil.VSCoilTypeOfNum == DataHVACGlobals::Coil_HeatingWaterToAirHPVSEquationFit) {
            CurrentObjSubfix = ":WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT";
        } else if (VarSpeedCoil.VSCoilTypeOfNum == CoilDX_HeatPumpWaterHeaterVariableSpeed) {
            CurrentObjSubfix = ":WATERHEATING:AIRTOWATERHEATPUMP:VARIABLESPEED";
        } else {
            CurrentObjSubfix = ":DX:VARIABLESPEED";
        }

        if (VarSpeedCoil.CoolHeatType == "WATERHEATING") {
            if (VarSpeedCoil.RatedAirVolFlowRate == DataGlobalConstants::AutoCalculate) {
                VarSpeedCoil.RatedAirVolFlowRate =
                    VarSpeedCoil.RatedCapWH *
                    VarSpeedCoil.MSRatedAirVolFlowRate(NormSpeed) /
                    VarSpeedCoil.MSRatedTotCap(NormSpeed); // 0.00005035;
                VarSpeedCoil.AirVolFlowAutoSized = true;
            }
            state.dataRptCoilSelection->coilSelectionReportObj->setCoilAirFlow(
                state,
                VarSpeedCoil.Name,
                VarSpeedCoil.VarSpeedCoilType,
                VarSpeedCoil.RatedAirVolFlowRate,
                VarSpeedCoil.AirVolFlowAutoSized);

            if (VarSpeedCoil.RatedWaterVolFlowRate == DataGlobalConstants::AutoCalculate) {
                VarSpeedCoil.RatedHPWHCondWaterFlow =
                    VarSpeedCoil.RatedCapWH *
                    VarSpeedCoil.MSRatedWaterVolFlowRate(NormSpeed) /
                    VarSpeedCoil.MSRatedTotCap(NormSpeed); // 0.00000004487;
                VarSpeedCoil.RatedWaterVolFlowRate =
                    VarSpeedCoil.RatedHPWHCondWaterFlow;
                VarSpeedCoil.WaterVolFlowAutoSized = true;
            }
            state.dataRptCoilSelection->coilSelectionReportObj->setCoilWaterFlowPltSizNum(
                state,
                VarSpeedCoil.Name,
                VarSpeedCoil.VarSpeedCoilType,
                VarSpeedCoil.RatedWaterVolFlowRate,
                VarSpeedCoil.WaterVolFlowAutoSized,
                -999,
                VarSpeedCoil.plantLoc.loopNum);
        }

        if (VarSpeedCoil.RatedAirVolFlowRate == AutoSize) {
            RatedAirFlowAutoSized = true;
        }

        if (state.dataSize->CurSysNum > 0) {
            if (!RatedAirFlowAutoSized && !SizingDesRunThisAirSys) { // Simulation continue
                HardSizeNoDesRunAirFlow = true;
                if (VarSpeedCoil.RatedAirVolFlowRate > 0.0) {
                    BaseSizer::reportSizerOutput(state,
                                                 "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                                 VarSpeedCoil.Name,
                                                 "User-Specified Rated Air Flow Rate [m3/s]",
                                                 VarSpeedCoil.RatedAirVolFlowRate);
                }
            } else {
                CheckSysSizing(state,
                               "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                               VarSpeedCoil.Name);
                if (state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).DesMainVolFlow >= SmallAirVolFlow) {
                    RatedAirVolFlowRateDes = state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).DesMainVolFlow;
                } else {
                    RatedAirVolFlowRateDes = 0.0;
                }
            }
        }

        if (state.dataSize->CurZoneEqNum > 0) {
            if (!RatedAirFlowAutoSized && !SizingDesRunThisZone) { // Simulation continue
                HardSizeNoDesRunAirFlow = true;
                if (VarSpeedCoil.RatedAirVolFlowRate > 0.0) {
                    BaseSizer::reportSizerOutput(state,
                                                 "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                                 VarSpeedCoil.Name,
                                                 "User-Specified Rated Air Flow Rate [m3/s]",
                                                 VarSpeedCoil.RatedAirVolFlowRate);
                }
            } else {
                CheckZoneSizing(state,
                                "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                VarSpeedCoil.Name);
                RatedAirVolFlowRateDes = max(state.dataSize->FinalZoneSizing(state.dataSize->CurZoneEqNum).DesCoolVolFlow,
                                             state.dataSize->FinalZoneSizing(state.dataSize->CurZoneEqNum).DesHeatVolFlow);
                if (RatedAirVolFlowRateDes < SmallAirVolFlow) {
                    RatedAirVolFlowRateDes = 0.0;
                }
            }
        }

        if (RatedAirFlowAutoSized) VarSpeedCoil.RatedAirVolFlowRate = RatedAirVolFlowRateDes;
        //    CALL BaseSizer::reportSizerOutput('COIL:'//TRIM(VarSpeedCoil(DXCoilNum)%CoolHeatType)//TRIM(CurrentObjSubfix), &
        //                             VarSpeedCoil(DXCoilNum)%Name, &
        //                            'Rated Air Flow Rate [m3/s]', &
        //                             VarSpeedCoil(DXCoilNum)%RatedAirVolFlowRate)

        // RatedAirFlowAutoSized = .TRUE.

        RatedCapCoolTotalAutoSized = false;
        RatedCapCoolSensAutoSized = false;

        // size rated total cooling capacity
        IsAutoSize = false;
        if (VarSpeedCoil.RatedCapCoolTotal == AutoSize &&
            VarSpeedCoil.CoolHeatType == "COOLING") {
            RatedCapCoolTotalAutoSized = true;
        }
        if (SizingDesRunThisZone || SizingDesRunThisAirSys) HardSizeNoDesRun = false;
        if (state.dataSize->CurSysNum > 0) {
            if (!RatedCapCoolTotalAutoSized && !SizingDesRunThisAirSys) { // Simulation continue
                HardSizeNoDesRun = true;
                if (VarSpeedCoil.RatedCapCoolTotal > 0.0) {
                    BaseSizer::reportSizerOutput(state,
                                                 "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                                 VarSpeedCoil.Name,
                                                 "User-Specified Rated Total Cooling Capacity [W]",
                                                 VarSpeedCoil.RatedCapCoolTotal);
                }
            } else {
                CheckSysSizing(state,
                               "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                               VarSpeedCoil.Name);
                VolFlowRate = VarSpeedCoil.RatedAirVolFlowRate;
                if (VolFlowRate >= SmallAirVolFlow) {
                    if (state.dataSize->CurOASysNum > 0) { // coil is in the OA stream
                        MixTemp = state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).OutTempAtCoolPeak;
                        MixHumRat = state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).OutHumRatAtCoolPeak;
                        SupTemp = state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).PrecoolTemp;
                        SupHumRat = state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).PrecoolHumRat;
                    } else { // coil is on the main air loop
                        SupTemp = state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).CoolSupTemp;
                        SupHumRat = state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).CoolSupHumRat;
                        if (state.dataAirSystemsData->PrimaryAirSystems(state.dataSize->CurSysNum).NumOACoolCoils ==
                            0) { // there is no precooling of the OA stream
                            MixTemp = state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).MixTempAtCoolPeak;
                            MixHumRat = state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).MixHumRatAtCoolPeak;
                        } else { // there is precooling of OA stream
                            if (VolFlowRate > 0.0) {
                                OutAirFrac = state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).DesOutAirVolFlow / VolFlowRate;
                            } else {
                                OutAirFrac = 1.0;
                            }
                            OutAirFrac = min(1.0, max(0.0, OutAirFrac));
                            MixTemp = OutAirFrac * state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).PrecoolTemp +
                                      (1.0 - OutAirFrac) * state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).RetTempAtCoolPeak;
                            MixHumRat = OutAirFrac * state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).PrecoolHumRat +
                                        (1.0 - OutAirFrac) * state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).RetHumRatAtCoolPeak;
                        }
                    }
                    OutTemp = state.dataSize->FinalSysSizing(state.dataSize->CurSysNum).OutTempAtCoolPeak;
                    rhoair = PsyRhoAirFnPbTdbW(state, state.dataEnvrn->OutBaroPress, MixTemp, MixHumRat, RoutineName);
                    MixEnth = PsyHFnTdbW(MixTemp, MixHumRat);
                    SupEnth = PsyHFnTdbW(SupTemp, SupHumRat);

                    // design fan heat will be added to coil load
                    Real64 FanCoolLoad =
                        DataAirSystems::calcFanDesignHeatGain(state, state.dataSize->DataFanEnumType, state.dataSize->DataFanIndex, VolFlowRate);
                    // inlet/outlet temp is adjusted after enthalpy is calculcated so fan heat is not double counted
                    Real64 CpAir = PsyCpAirFnW(MixHumRat);
                    if (state.dataAirSystemsData->PrimaryAirSystems(state.dataSize->CurSysNum).supFanLocation ==
                        DataAirSystems::FanPlacement::BlowThru) {
                        MixTemp += FanCoolLoad / (CpAir * rhoair * VolFlowRate);
                    } else if (state.dataAirSystemsData->PrimaryAirSystems(state.dataSize->CurSysNum).supFanLocation ==
                               DataAirSystems::FanPlacement::DrawThru) {
                        SupTemp -= FanCoolLoad / (CpAir * rhoair * VolFlowRate);
                    }
                    MixWetBulb = PsyTwbFnTdbWPb(state, MixTemp, MixHumRat, state.dataEnvrn->OutBaroPress, RoutineName);
                    // need to use OutTemp for air-cooled and RatedInletWaterTemp for water-cooled
                    if (VarSpeedCoil.CondenserInletNodeNum != 0) {
                        RatedSourceTempCool = RatedInletWaterTemp;
                    } else {
                        RatedSourceTempCool = OutTemp;
                    }
                    TotCapTempModFac = CurveValue(state,
                                                  VarSpeedCoil.MSCCapFTemp(
                                                      VarSpeedCoil.NormSpedLevel),
                                                  MixWetBulb,
                                                  RatedSourceTempCool);

                    //       The mixed air temp for zone equipment without an OA mixer is 0.
                    //       This test avoids a negative capacity until a solution can be found.
                    if (MixEnth > SupEnth) {
                        CoolCapAtPeak = (rhoair * VolFlowRate * (MixEnth - SupEnth)) + FanCoolLoad;
                    } else {
                        CoolCapAtPeak = (rhoair * VolFlowRate * (48000.0 - SupEnth)) + FanCoolLoad;
                    }
                    CoolCapAtPeak = max(0.0, CoolCapAtPeak);
                    if (TotCapTempModFac > 0.0) {
                        RatedCapCoolTotalDes = CoolCapAtPeak / TotCapTempModFac;
                    } else {
                        RatedCapCoolTotalDes = CoolCapAtPeak;
                    }
                } else {
                    RatedCapCoolTotalDes = 0.0;
                }
            }

        } else if (state.dataSize->CurZoneEqNum > 0) {
            if (!RatedCapCoolTotalAutoSized && !SizingDesRunThisZone) { // Simulation continue
                HardSizeNoDesRun = true;
                if (VarSpeedCoil.RatedCapCoolTotal > 0.0) {
                    BaseSizer::reportSizerOutput(state,
                                                 "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                                 VarSpeedCoil.Name,
                                                 "User-Specified Rated Total Cooling Capacity [W]",
                                                 VarSpeedCoil.RatedCapCoolTotal);
                }
            } else {
                CheckZoneSizing(state,
                                "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                VarSpeedCoil.Name);
                VolFlowRate = VarSpeedCoil.RatedAirVolFlowRate;
                if (VolFlowRate >= SmallAirVolFlow) {
                    if (state.dataSize->ZoneEqDXCoil) {
                        if (ZoneEqSizing(state.dataSize->CurZoneEqNum).OAVolFlow > 0.0) {
                            MixTemp = state.dataSize->FinalZoneSizing(state.dataSize->CurZoneEqNum).DesCoolCoilInTemp;
                            MixHumRat = state.dataSize->FinalZoneSizing(state.dataSize->CurZoneEqNum).DesCoolCoilInHumRat;
                        } else {
                            MixTemp = state.dataSize->FinalZoneSizing(state.dataSize->CurZoneEqNum).ZoneRetTempAtCoolPeak;
                            MixHumRat = state.dataSize->FinalZoneSizing(state.dataSize->CurZoneEqNum).ZoneHumRatAtCoolPeak;
                        }
                    } else {
                        MixTemp = state.dataSize->FinalZoneSizing(state.dataSize->CurZoneEqNum).DesCoolCoilInTemp;
                        MixHumRat = state.dataSize->FinalZoneSizing(state.dataSize->CurZoneEqNum).DesCoolCoilInHumRat;
                    }
                    SupTemp = state.dataSize->FinalZoneSizing(state.dataSize->CurZoneEqNum).CoolDesTemp;
                    SupHumRat = state.dataSize->FinalZoneSizing(state.dataSize->CurZoneEqNum).CoolDesHumRat;
                    TimeStepNumAtMax = state.dataSize->FinalZoneSizing(state.dataSize->CurZoneEqNum).TimeStepNumAtCoolMax;
                    DDNum = state.dataSize->FinalZoneSizing(state.dataSize->CurZoneEqNum).CoolDDNum;
                    if (DDNum > 0 && TimeStepNumAtMax > 0) {
                        OutTemp = state.dataSize->DesDayWeath(DDNum).Temp(TimeStepNumAtMax);
                    } else {
                        OutTemp = 0.0;
                    }
                    rhoair = PsyRhoAirFnPbTdbW(state, state.dataEnvrn->OutBaroPress, MixTemp, MixHumRat, RoutineName);
                    MixEnth = PsyHFnTdbW(MixTemp, MixHumRat);
                    SupEnth = PsyHFnTdbW(SupTemp, SupHumRat);

                    // design fan heat will be added to coil load
                    Real64 FanCoolLoad =
                        DataAirSystems::calcFanDesignHeatGain(state, state.dataSize->DataFanEnumType, state.dataSize->DataFanIndex, VolFlowRate);
                    // inlet/outlet temp is adjusted after enthalpy is calculcated so fan heat is not double counted
                    Real64 CpAir = PsyCpAirFnW(MixHumRat);

                    if (state.dataSize->DataFanPlacement == DataSizing::ZoneFanPlacement::BlowThru) {
                        MixTemp += FanCoolLoad / (CpAir * rhoair * VolFlowRate);
                    } else {
                        SupTemp -= FanCoolLoad / (CpAir * rhoair * VolFlowRate);
                    }

                    MixWetBulb = PsyTwbFnTdbWPb(state, MixTemp, MixHumRat, state.dataEnvrn->OutBaroPress, RoutineName);
                    // need to use OutTemp for air-cooled and RatedInletWaterTemp for water-cooled
                    if (VarSpeedCoil.CondenserInletNodeNum != 0) {
                        RatedSourceTempCool = RatedInletWaterTemp;
                    } else {
                        RatedSourceTempCool = OutTemp;
                    }
                    TotCapTempModFac = CurveValue(state,
                                                  VarSpeedCoil.MSCCapFTemp(
                                                      VarSpeedCoil.NormSpedLevel),
                                                  MixWetBulb,
                                                  RatedSourceTempCool);

                    //       The mixed air temp for zone equipment without an OA mixer is 0.
                    //       This test avoids a negative capacity until a solution can be found.
                    if (MixEnth > SupEnth) {
                        CoolCapAtPeak = (rhoair * VolFlowRate * (MixEnth - SupEnth)) + FanCoolLoad;
                    } else {
                        CoolCapAtPeak = (rhoair * VolFlowRate * (48000.0 - SupEnth)) + FanCoolLoad;
                    }
                    CoolCapAtPeak = max(0.0, CoolCapAtPeak);
                    if (TotCapTempModFac > 0.0) {
                        RatedCapCoolTotalDes = CoolCapAtPeak / TotCapTempModFac;
                    } else {
                        RatedCapCoolTotalDes = CoolCapAtPeak;
                    }
                } else {
                    RatedCapCoolTotalDes = 0.0;
                }
            }
        }
        if (RatedCapCoolTotalDes < SmallLoad) {
            RatedCapCoolTotalDes = 0.0;
        }
        if (!HardSizeNoDesRun) {
            if (RatedCapCoolTotalAutoSized) {
                VarSpeedCoil.RatedCapCoolTotal = RatedCapCoolTotalDes;
                BaseSizer::reportSizerOutput(state,
                                             "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                             VarSpeedCoil.Name,
                                             "Design Size Rated Total Cooling Capacity [W]",
                                             VarSpeedCoil.RatedCapCoolTotal);
                PreDefTableEntry(state,
                                 state.dataOutRptPredefined->pdchCoolCoilTotCap,
                                 VarSpeedCoil.Name,
                                 VarSpeedCoil.RatedCapCoolTotal);
                PreDefTableEntry(state,
                                 state.dataOutRptPredefined->pdchCoolCoilLatCap,
                                 VarSpeedCoil.Name,
                                 VarSpeedCoil.RatedCapCoolTotal -
                                     VarSpeedCoil.RatedCapCoolSens);
                if (VarSpeedCoil.RatedCapCoolTotal != 0.0) {
                    PreDefTableEntry(state,
                                     state.dataOutRptPredefined->pdchCoolCoilSHR,
                                     VarSpeedCoil.Name,
                                     VarSpeedCoil.RatedCapCoolSens /
                                         VarSpeedCoil.RatedCapCoolTotal);
                    PreDefTableEntry(state,
                                     state.dataOutRptPredefined->pdchCoolCoilNomEff,
                                     VarSpeedCoil.Name,
                                     VarSpeedCoil.MSRatedCOP(NormSpeed));
                } else {
                    PreDefTableEntry(
                        state, state.dataOutRptPredefined->pdchCoolCoilSHR, VarSpeedCoil.Name, 0.0);
                    PreDefTableEntry(
                        state, state.dataOutRptPredefined->pdchCoolCoilNomEff, VarSpeedCoil.Name, 0.0);
                }
                addFootNoteSubTable(
                    state,
                    state.dataOutRptPredefined->pdstCoolCoil,
                    "Nominal values are gross at rated conditions, i.e., the supply air fan heat and electric power NOT accounted for.");
            } else {
                if (VarSpeedCoil.RatedCapCoolTotal > 0.0 && RatedCapCoolTotalDes > 0.0) {
                    RatedCapCoolTotalUser = VarSpeedCoil.RatedCapCoolTotal;
                    BaseSizer::reportSizerOutput(state,
                                                 "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                                 VarSpeedCoil.Name,
                                                 "Design Size Rated Total Cooling Capacity [W]",
                                                 RatedCapCoolTotalDes,
                                                 "User-Specified Rated Total Cooling Capacity [W]",
                                                 RatedCapCoolTotalUser);
                    if (state.dataGlobal->DisplayExtraWarnings) {
                        if ((std::abs(RatedCapCoolTotalDes - RatedCapCoolTotalUser) / RatedCapCoolTotalUser) >
                            state.dataSize->AutoVsHardSizingThreshold) {
                            ShowMessage(state,
                                        "SizeVarSpeedCoil: Potential issue with equipment sizing for " +
                                            VarSpeedCoil.CoolHeatType + ' ' + CurrentObjSubfix);
                            ShowContinueError(state, "Coil Name =" + VarSpeedCoil.Name);
                            ShowContinueError(state, format("User-Specified Rated Total Cooling Capacity of {:.2R} [W]", RatedCapCoolTotalUser));
                            ShowContinueError(state,
                                              format("differs from Design Size Rated Total Cooling Capacity of {:.2R} [W]", RatedCapCoolTotalDes));
                            ShowContinueError(state, "This may, or may not, indicate mismatched component sizes.");
                            ShowContinueError(state, "Verify that the value entered is intended and is consistent with other components.");
                        }
                    }
                }
            }

            state.dataRptCoilSelection->coilSelectionReportObj->setCoilEntAirTemp(
                state,
                VarSpeedCoil.Name,
                VarSpeedCoil.VarSpeedCoilType,
                MixTemp,
                state.dataSize->CurSysNum,
                state.dataSize->CurZoneEqNum);
            state.dataRptCoilSelection->coilSelectionReportObj->setCoilEntAirHumRat(
                state,
                VarSpeedCoil.Name,
                VarSpeedCoil.VarSpeedCoilType,
                MixHumRat);
            state.dataRptCoilSelection->coilSelectionReportObj->setCoilLvgAirTemp(
                state,
                VarSpeedCoil.Name,
                VarSpeedCoil.VarSpeedCoilType,
                SupTemp);
            state.dataRptCoilSelection->coilSelectionReportObj->setCoilLvgAirHumRat(
                state,
                VarSpeedCoil.Name,
                VarSpeedCoil.VarSpeedCoilType,
                SupHumRat);
            state.dataRptCoilSelection->coilSelectionReportObj->setCoilCoolingCapacity(
                state,
                VarSpeedCoil.Name,
                VarSpeedCoil.VarSpeedCoilType,
                RatedCapCoolTotalDes,
                RatedCapCoolTotalAutoSized,
                state.dataSize->CurSysNum,
                state.dataSize->CurZoneEqNum,
                state.dataSize->CurOASysNum,
                0.0, // no fan load included in sizing
                TotCapTempModFac,
                -999.0,
                -999.0); // VS model doesn't limit, double check
        }

        // Set the global DX cooling coil capacity variable for use by other objects
        if (VarSpeedCoil.CoolHeatType == "COOLING") {
            state.dataSize->DXCoolCap = VarSpeedCoil.RatedCapCoolTotal;
        }

        // size rated heating capacity
        if (VarSpeedCoil.RatedCapHeat == AutoSize &&
            VarSpeedCoil.CoolHeatType == "HEATING") {
            RatedCapHeatAutoSized = true;
        }
        //   simply set heating capacity equal to the cooling capacity
        // VarSpeedCoil(DXCoilNum)%RatedCapHeat = DXCoolCap
        if (VarSpeedCoil.CoolHeatType == "HEATING") {
            if (VarSpeedCoil.CompanionCoolingCoilNum > 0) {
                RatedCapHeatDes =
                    state.dataVariableSpeedCoils->VarSpeedCoil(VarSpeedCoil.CompanionCoolingCoilNum)
                        .RatedCapCoolTotal;
                VarSpeedCoil.RatedCapCoolTotal = RatedCapHeatDes; // AVOID BEING ZERO
            } else {
                RatedCapHeatDes = state.dataSize->DXCoolCap; // previous code, can be risky
            }
            // END IF
            if (RatedCapHeatAutoSized) {
                if (RatedCapHeatDes == AutoSize) {
                    ShowWarningError(state,
                                     "COIL:" + VarSpeedCoil.CoolHeatType +
                                         ":WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT \"" +
                                         VarSpeedCoil.Name + "\"");
                    ShowContinueError(
                        state, std::string{RoutineName} + ": Heating coil could not be autosized since cooling coil was not previously sized.");
                    ShowContinueError(state, "... Cooling coil must be upstream of heating coil.");
                    ShowContinueError(state, "... Manually sizing this heating coil will be required.");
                }
            }
            if (RatedCapHeatDes < SmallLoad) {
                RatedCapHeatDes = 0.0;
            }
            state.dataRptCoilSelection->coilSelectionReportObj->setCoilHeatingCapacity(
                state,
                VarSpeedCoil.Name,
                VarSpeedCoil.VarSpeedCoilType,
                RatedCapHeatDes,
                RatedCapHeatAutoSized,
                state.dataSize->CurSysNum,
                state.dataSize->CurZoneEqNum,
                state.dataSize->CurOASysNum,
                0.0,
                1.0,
                -999.0,
                -999.0);
        }
        if (RatedCapHeatAutoSized) {
            VarSpeedCoil.RatedCapHeat = RatedCapHeatDes;
            BaseSizer::reportSizerOutput(state,
                                         "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                         VarSpeedCoil.Name,
                                         "Design Size Nominal Heating Capacity [W]",
                                         RatedCapHeatDes);
            PreDefTableEntry(state,
                             state.dataOutRptPredefined->pdchHeatCoilNomCap,
                             VarSpeedCoil.Name,
                             VarSpeedCoil.RatedCapHeat);
            if (VarSpeedCoil.RatedCapHeat != 0.0) {
                PreDefTableEntry(state,
                                 state.dataOutRptPredefined->pdchHeatCoilNomEff,
                                 VarSpeedCoil.Name,
                                 VarSpeedCoil.MSRatedCOP(NormSpeed));
            } else {
                PreDefTableEntry(
                    state, state.dataOutRptPredefined->pdchHeatCoilNomEff, VarSpeedCoil.Name, 0.0);
            }
            addFootNoteSubTable(state,
                                state.dataOutRptPredefined->pdstHeatCoil,
                                "Nominal values are gross at rated conditions, i.e., the supply air fan heat and electric power NOT accounted for.");
        } else {
            if (VarSpeedCoil.RatedCapHeat > 0.0 && RatedCapHeatDes > 0.0) {
                RatedCapHeatUser = VarSpeedCoil.RatedCapHeat;
                BaseSizer::reportSizerOutput(state,
                                             "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                             VarSpeedCoil.Name,
                                             "Design Size Nominal Heating Capacity [W]",
                                             RatedCapHeatDes,
                                             "User-Specified Nominal Heating Capacity [W]",
                                             RatedCapHeatUser);
                if (state.dataGlobal->DisplayExtraWarnings) {
                    if ((std::abs(RatedCapHeatDes - RatedCapHeatUser) / RatedCapHeatUser) > state.dataSize->AutoVsHardSizingThreshold) {
                        ShowMessage(state,
                                    "SizeVarSpeedCoil: Potential issue with equipment sizing for " +
                                        VarSpeedCoil.CoolHeatType + ' ' + CurrentObjSubfix);
                        ShowContinueError(state, "Coil Name =" + VarSpeedCoil.Name);
                        ShowContinueError(state, format("User-Specified Rated Total Heating Capacity of {:.2R} [W]", RatedCapHeatUser));
                        ShowContinueError(state, format("differs from Design Size Rated Total Heating Capacity of {:.2R} [W]", RatedCapHeatDes));
                        ShowContinueError(state, "This may, or may not, indicate mismatched component sizes.");
                        ShowContinueError(state, "Verify that the value entered is intended and is consistent with other components.");
                    }
                }
            }
        }

        // FORCE BACK TO THE RATED AIR FLOW RATE WITH THE SAME RATIO DEFINED BY THE CATALOG DATA
        if (!HardSizeNoDesRunAirFlow) {
            if ((RatedCapCoolTotalAutoSized) && (RatedAirFlowAutoSized)) {
                RatedAirVolFlowRateDes = VarSpeedCoil.RatedCapCoolTotal *
                                         VarSpeedCoil.MSRatedAirVolFlowPerRatedTotCap(NormSpeed);
            } else if ((RatedCapHeatAutoSized) && (RatedAirFlowAutoSized)) {
                RatedAirVolFlowRateDes = VarSpeedCoil.RatedCapHeat *
                                         VarSpeedCoil.MSRatedAirVolFlowPerRatedTotCap(NormSpeed);
            }

            // write the air flow sizing output
            if (RatedAirFlowAutoSized) {
                VarSpeedCoil.RatedAirVolFlowRate = RatedAirVolFlowRateDes;
                BaseSizer::reportSizerOutput(state,
                                             "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                             VarSpeedCoil.Name,
                                             "Design Size Rated Air Flow Rate [m3/s]",
                                             RatedAirVolFlowRateDes);
            } else {
                if (VarSpeedCoil.RatedAirVolFlowRate > 0.0 && RatedAirVolFlowRateDes > 0.0) {
                    RatedAirVolFlowRateUser = VarSpeedCoil.RatedAirVolFlowRate;
                    BaseSizer::reportSizerOutput(state,
                                                 "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                                 VarSpeedCoil.Name,
                                                 "Design Size Rated Air Flow Rate [m3/s]",
                                                 RatedAirVolFlowRateDes,
                                                 "User-Specified Rated Air Flow Rate [m3/s]",
                                                 RatedAirVolFlowRateUser);
                    if (state.dataGlobal->DisplayExtraWarnings) {
                        if ((std::abs(RatedAirVolFlowRateDes - RatedAirVolFlowRateUser) / RatedAirVolFlowRateUser) >
                            state.dataSize->AutoVsHardSizingThreshold) {
                            ShowMessage(state,
                                        "SizeVarSpeedCoil: Potential issue with equipment sizing for" +
                                            VarSpeedCoil.CoolHeatType + ' ' + CurrentObjSubfix);
                            ShowContinueError(state, "Coil Name =" + VarSpeedCoil.Name);
                            ShowContinueError(state, format("User-Specified Rated Air Flow Rate of {:.5R} [m3/s]", RatedAirVolFlowRateUser));
                            ShowContinueError(state, format("differs from Design Size Rated Air Flow Rate of {:.5R} [m3/s]", RatedAirVolFlowRateDes));
                            ShowContinueError(state, "This may, or may not, indicate mismatched component sizes.");
                            ShowContinueError(state, "Verify that the value entered is intended and is consistent with other components.");
                        }
                    }
                }
            }
            state.dataRptCoilSelection->coilSelectionReportObj->setCoilAirFlow(state,
                                                                               VarSpeedCoil.Name,
                                                                               VarSpeedCoil.VarSpeedCoilType,
                                                                               RatedAirVolFlowRateDes,
                                                                               RatedAirFlowAutoSized);
        }

        // Check that heat pump heating capacity is within 20% of cooling capacity. Check only for heating coil and report both.
        if (VarSpeedCoil.CoolHeatType == "HEATING" &&
            VarSpeedCoil.CompanionCoolingCoilNum > 0) {

            if (state.dataVariableSpeedCoils->VarSpeedCoil(VarSpeedCoil.CompanionCoolingCoilNum)
                    .RatedCapCoolTotal > 0.0) {

                if (std::abs(state.dataVariableSpeedCoils->VarSpeedCoil(VarSpeedCoil.CompanionCoolingCoilNum)
                                 .RatedCapCoolTotal -
                             VarSpeedCoil.RatedCapHeat) /
                        state.dataVariableSpeedCoils->VarSpeedCoil(VarSpeedCoil.CompanionCoolingCoilNum)
                            .RatedCapCoolTotal >
                    0.2) {

                    ShowWarningError(state,
                                     "COIL:" + VarSpeedCoil.CoolHeatType +
                                         ":WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT \"" +
                                         VarSpeedCoil.Name + "\"");
                    ShowContinueError(
                        state,
                        "...used with COIL:" +
                            state.dataVariableSpeedCoils->VarSpeedCoil(VarSpeedCoil.CompanionCoolingCoilNum)
                                .CoolHeatType +
                            ":WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT \"" +
                            state.dataVariableSpeedCoils->VarSpeedCoil(VarSpeedCoil.CompanionCoolingCoilNum)
                                .Name +
                            "\"");
                    ShowContinueError(state, "...heating capacity is disproportionate (> 20% different) to total cooling capacity");
                    ShowContinueError(state,
                                      format("...heating capacity = {:.3T} W", VarSpeedCoil.RatedCapHeat));
                    ShowContinueError(state,
                                      format("...cooling capacity = {:.3T} W",
                                             state.dataVariableSpeedCoils
                                                 ->VarSpeedCoil(VarSpeedCoil.CompanionCoolingCoilNum)
                                                 .RatedCapCoolTotal));
                }
            }
        }

        // ASSIGN CAPACITY
        if (VarSpeedCoil.CoolHeatType == "COOLING") {
            VarSpeedCoil.MSRatedTotCap(UpperSpeed) =
                VarSpeedCoil.RatedCapCoolTotal /
                VarSpeedCoil.MSRatedPercentTotCap(NormSpeed);
        } else if (VarSpeedCoil.CoolHeatType == "HEATING") {
            VarSpeedCoil.MSRatedTotCap(UpperSpeed) =
                VarSpeedCoil.RatedCapHeat /
                VarSpeedCoil.MSRatedPercentTotCap(NormSpeed);
        } else if (VarSpeedCoil.CoolHeatType == "WATERHEATING") {
            VarSpeedCoil.MSRatedTotCap(UpperSpeed) =
                VarSpeedCoil.RatedCapWH /
                VarSpeedCoil.MSRatedPercentTotCap(NormSpeed);
        }

        if (VarSpeedCoil.CoolHeatType == "WATERHEATING") {
            HPInletAirHumRat = PsyWFnTdbTwbPb(state,
                                              VarSpeedCoil.WHRatedInletDBTemp,
                                              VarSpeedCoil.WHRatedInletWBTemp,
                                              state.dataEnvrn->StdBaroPress,
                                              RoutineName);
            rhoA = PsyRhoAirFnPbTdbW(state,
                                     state.dataEnvrn->OutBaroPress,
                                     VarSpeedCoil.WHRatedInletDBTemp,
                                     HPInletAirHumRat,
                                     RoutineName);

            for (Mode = VarSpeedCoil.NumOfSpeeds; Mode >= 1; --Mode) {
                VarSpeedCoil.MSRatedTotCap(Mode) =
                    VarSpeedCoil.MSRatedTotCap(UpperSpeed) *
                    VarSpeedCoil.MSRatedPercentTotCap(Mode);
                VarSpeedCoil.MSRatedAirVolFlowRate(Mode) =
                    VarSpeedCoil.MSRatedTotCap(Mode) *
                    VarSpeedCoil.MSRatedAirVolFlowPerRatedTotCap(Mode);
                VarSpeedCoil.MSRatedAirMassFlowRate(Mode) =
                    VarSpeedCoil.MSRatedAirVolFlowRate(Mode) * rhoA;
                // EVAPORATIVE PRECOOLING CONDENSER AIR FLOW RATE
                VarSpeedCoil.EvapCondAirFlow(Mode) = 0.0;
            }
        } else {
            rhoA = PsyRhoAirFnPbTdbW(state, state.dataEnvrn->OutBaroPress, RatedInletAirTemp, RatedInletAirHumRat, RoutineName);
            // HPWH, the mass flow rate will be updated by a revised entering air density

            if (VarSpeedCoil.MSHPDesignSpecIndex > -1 &&
                state.dataUnitarySystems->designSpecMSHP.size() > 0) {
                if (VarSpeedCoil.CoolHeatType == "COOLING") {
                    if (state.dataUnitarySystems->designSpecMSHP[VarSpeedCoil.MSHPDesignSpecIndex]
                            .numOfSpeedCooling != VarSpeedCoil.NumOfSpeeds) {
                        ShowFatalError(state,
                                       "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix + " = " +
                                           VarSpeedCoil.Name +
                                           " number of speeds not equal to number of speed specified in UnitarySystemPerformance:Multispeed object.");
                    } else {
                        for (Mode = VarSpeedCoil.NumOfSpeeds; Mode >= 1; --Mode) {
                            VarSpeedCoil.MSRatedAirVolFlowRate(Mode) =
                                VarSpeedCoil.RatedAirVolFlowRate *
                                state.dataUnitarySystems->designSpecMSHP[VarSpeedCoil.MSHPDesignSpecIndex]
                                    .coolingVolFlowRatio[Mode - 1];
                            VarSpeedCoil.MSRatedTotCap(Mode) =
                                VarSpeedCoil.MSRatedAirVolFlowRate(Mode) /
                                VarSpeedCoil.MSRatedAirVolFlowPerRatedTotCap(Mode);
                            VarSpeedCoil.MSRatedAirMassFlowRate(Mode) =
                                VarSpeedCoil.MSRatedAirVolFlowRate(Mode) * rhoA;
                            // EVAPORATIVE PRECOOLING CONDENSER AIR FLOW RATE
                            VarSpeedCoil.EvapCondAirFlow(Mode) =
                                VarSpeedCoil.MSRatedTotCap(Mode) *
                                VarSpeedCoil.MSRatedEvapCondVolFlowPerRatedTotCap(Mode);
                        }
                    }
                } else if (VarSpeedCoil.CoolHeatType == "HEATING") {
                    if (state.dataUnitarySystems->designSpecMSHP[VarSpeedCoil.MSHPDesignSpecIndex]
                            .numOfSpeedHeating != VarSpeedCoil.NumOfSpeeds) {
                        ShowFatalError(state,
                                       "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix + " = " +
                                           VarSpeedCoil.Name +
                                           " number of speeds not equal to number of speed specified in UnitarySystemPerformance:Multispeed object.");
                    } else {
                        for (Mode = VarSpeedCoil.NumOfSpeeds; Mode >= 1; --Mode) {
                            VarSpeedCoil.MSRatedAirVolFlowRate(Mode) =
                                VarSpeedCoil.RatedAirVolFlowRate *
                                state.dataUnitarySystems->designSpecMSHP[VarSpeedCoil.MSHPDesignSpecIndex]
                                    .heatingVolFlowRatio[Mode - 1];
                            VarSpeedCoil.MSRatedTotCap(Mode) =
                                VarSpeedCoil.MSRatedAirVolFlowRate(Mode) /
                                VarSpeedCoil.MSRatedAirVolFlowPerRatedTotCap(Mode);
                            VarSpeedCoil.MSRatedAirMassFlowRate(Mode) =
                                VarSpeedCoil.MSRatedAirVolFlowRate(Mode) * rhoA;
                            // EVAPORATIVE PRECOOLING CONDENSER AIR FLOW RATE
                            VarSpeedCoil.EvapCondAirFlow(Mode) =
                                VarSpeedCoil.MSRatedTotCap(Mode) *
                                VarSpeedCoil.MSRatedEvapCondVolFlowPerRatedTotCap(Mode);
                        }
                    }
                }
            } else {
                for (Mode = VarSpeedCoil.NumOfSpeeds; Mode >= 1; --Mode) {
                    VarSpeedCoil.MSRatedTotCap(Mode) =
                        VarSpeedCoil.MSRatedTotCap(UpperSpeed) *
                        VarSpeedCoil.MSRatedPercentTotCap(Mode);
                    VarSpeedCoil.MSRatedAirVolFlowRate(Mode) =
                        VarSpeedCoil.MSRatedTotCap(Mode) *
                        VarSpeedCoil.MSRatedAirVolFlowPerRatedTotCap(Mode);
                    VarSpeedCoil.MSRatedAirMassFlowRate(Mode) =
                        VarSpeedCoil.MSRatedAirVolFlowRate(Mode) * rhoA;
                    // EVAPORATIVE PRECOOLING CONDENSER AIR FLOW RATE
                    VarSpeedCoil.EvapCondAirFlow(Mode) =
                        VarSpeedCoil.MSRatedTotCap(Mode) *
                        VarSpeedCoil.MSRatedEvapCondVolFlowPerRatedTotCap(Mode);
                }
            }
        }

        // size rated power
        if (VarSpeedCoil.CoolHeatType == "COOLING") {

            VarSpeedCoil.RatedCOPCool =
                VarSpeedCoil.MSRatedCOP(VarSpeedCoil.NormSpedLevel);
            VarSpeedCoil.RatedPowerCool =
                VarSpeedCoil.RatedCapCoolTotal /
                VarSpeedCoil.RatedCOPCool;

        } else if (VarSpeedCoil.CoolHeatType == "HEATING") {
            VarSpeedCoil.RatedCOPHeat =
                VarSpeedCoil.MSRatedCOP(VarSpeedCoil.NormSpedLevel);
            VarSpeedCoil.RatedPowerHeat =
                VarSpeedCoil.RatedCapHeat /
                VarSpeedCoil.RatedCOPHeat;
            VarSpeedCoil.RatedCapCoolTotal =
                VarSpeedCoil.RatedCapHeat;
        } else if (VarSpeedCoil.CoolHeatType == "WATERHEATING") {
            VarSpeedCoil.RatedCOPHeat =
                VarSpeedCoil.MSRatedCOP(VarSpeedCoil.NormSpedLevel);
            VarSpeedCoil.RatedPowerHeat =
                VarSpeedCoil.RatedCapWH / VarSpeedCoil.RatedCOPHeat;
            VarSpeedCoil.RatedCapCoolTotal =
                VarSpeedCoil.RatedCapWH *
                (1.0 - 1.0 / VarSpeedCoil.RatedCOPHeat);
        }

        // Size water volumetric flow rate
        if ((VarSpeedCoil.RatedWaterVolFlowRate == AutoSize) &&
            (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingWaterToAirHPVSEquationFit ||
             VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingWaterToAirHPVSEquationFit)) {
            RatedWaterFlowAutoSized = true;
        }

        //!   if not found on a plant loop, check condenser loop and warn user if not found
        //    IF(PltSizNum == 0) THEN
        //      PltSizNum = &
        //          MyCondPlantSizingIndex('COIL:'//TRIM(VarSpeedCoil(DXCoilNum)%CoolHeatType)// &
        //               TRIM(CurrentObjSubfix), &
        //                                 VarSpeedCoil(DXCoilNum)%Name, &
        //                                 VarSpeedCoil(DXCoilNum)%WaterInletNodeNum, &
        //                                 VarSpeedCoil(DXCoilNum)%WaterOutletNodeNum, ErrorsFound)
        //    END IF

        //   WSHP condenser can be on either a plant loop or condenser loop. Test each to find plant sizing number.
        //   first check to see if coil is connected to a plant loop, no warning on this CALL
        if (RatedWaterFlowAutoSized) {
            if (VarSpeedCoil.CondenserType == DataHeatBalance::RefrigCondenserType::Water)
                PltSizNum = MyPlantSizingIndex(state,
                                               "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                               VarSpeedCoil.Name,
                                               VarSpeedCoil.WaterInletNodeNum,
                                               VarSpeedCoil.WaterOutletNodeNum,
                                               ErrorsFound,
                                               false);

            if (PltSizNum > 0) {
                rho = GetDensityGlycol(state,
                                       state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidName,
                                       state.dataSize->PlantSizData(PltSizNum).ExitTemp,
                                       state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidIndex,
                                       RoutineNameAlt);
                cp = GetSpecificHeatGlycol(
                    state,
                    state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidName,
                    state.dataSize->PlantSizData(PltSizNum).ExitTemp,
                    state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidIndex,
                    RoutineNameAlt);

                if (VarSpeedCoil.CoolHeatType == "HEATING") {

                    RatedWaterVolFlowRateDes = VarSpeedCoil.RatedCapHeat /
                                               (state.dataSize->PlantSizData(PltSizNum).DeltaT * cp * rho);

                    //        CALL BaseSizer::reportSizerOutput('COIL:'//TRIM(VarSpeedCoil(DXCoilNum)%CoolHeatType)//&
                    //                                TRIM(CurrentObjSubfix), &
                    //                                  VarSpeedCoil(DXCoilNum)%Name, &
                    //                                  'Rated Water Flow Rate [m3/s]', VarSpeedCoil(DXCoilNum)%RatedWaterVolFlowRate)
                    state.dataRptCoilSelection->coilSelectionReportObj->setCoilLvgWaterTemp(
                        state,
                        VarSpeedCoil.Name,
                        VarSpeedCoil.VarSpeedCoilType,
                        state.dataSize->PlantSizData(PltSizNum).ExitTemp +
                            state.dataSize->PlantSizData(PltSizNum).DeltaT); // TRACE 3D Plus coil selection report

                } else if (VarSpeedCoil.CoolHeatType == "COOLING") {

                    //       use companion heating coil capacity to calculate volumetric flow rate
                    if (VarSpeedCoil.CompanionCoolingCoilNum > 0) {
                        SystemCapacity =
                            state.dataVariableSpeedCoils->VarSpeedCoil(VarSpeedCoil.CompanionCoolingCoilNum)
                                .RatedCapHeat;
                    } else {
                        SystemCapacity = VarSpeedCoil.RatedCapCoolTotal;
                    }

                    RatedWaterVolFlowRateDes = SystemCapacity / (state.dataSize->PlantSizData(PltSizNum).DeltaT * cp * rho);

                    //        CALL BaseSizer::reportSizerOutput('COIL:'//TRIM(VarSpeedCoil(DXCoilNum)%CoolHeatType)&
                    //                                //TRIM(CurrentObjSubfix), &
                    //                                  VarSpeedCoil(DXCoilNum)%Name, &
                    //                                  'Rated Water Flow Rate [m3/s]', VarSpeedCoil(DXCoilNum)%RatedWaterVolFlowRate)
                    state.dataRptCoilSelection->coilSelectionReportObj->setCoilLvgWaterTemp(
                        state,
                        VarSpeedCoil.Name,
                        VarSpeedCoil.VarSpeedCoilType,
                        state.dataSize->PlantSizData(PltSizNum).ExitTemp -
                            state.dataSize->PlantSizData(PltSizNum).DeltaT); // TRACE 3D Plus coil selection report
                }

                state.dataRptCoilSelection->coilSelectionReportObj->setCoilEntWaterTemp(
                    state,
                    VarSpeedCoil.Name,
                    VarSpeedCoil.VarSpeedCoilType,
                    state.dataSize->PlantSizData(PltSizNum).ExitTemp); // TRACE 3D Plus coil selection report

                state.dataRptCoilSelection->coilSelectionReportObj->setCoilWaterDeltaT(
                    state,
                    VarSpeedCoil.Name,
                    VarSpeedCoil.VarSpeedCoilType,
                    state.dataSize->PlantSizData(PltSizNum).DeltaT); // TRACE 3D Plus coil selection report
            } else {
                ShowSevereError(state, "Autosizing of water flow requires a loop Sizing:Plant object");
                ShowContinueError(state, "Autosizing also requires physical connection to a plant or condenser loop.");
                ShowContinueError(state,
                                  "Occurs in COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix +
                                      " Object=" + VarSpeedCoil.Name);
                ErrorsFound = true;
            }
        }

        // WRITE THE WATER SIZING OUTPUT
        if (RatedWaterFlowAutoSized) {
            // FORCE BACK TO THE RATED WATER FLOW RATE WITH THE SAME RATIO DEFINED BY THE CATLOG DATA
            if (RatedCapCoolTotalAutoSized) {
                RatedWaterVolFlowRateDes = VarSpeedCoil.RatedCapCoolTotal *
                                           VarSpeedCoil.MSRatedWaterVolFlowPerRatedTotCap(NormSpeed);
            } else if (RatedCapHeatAutoSized) {
                RatedWaterVolFlowRateDes = VarSpeedCoil.RatedCapHeat *
                                           VarSpeedCoil.MSRatedWaterVolFlowPerRatedTotCap(NormSpeed);
            }
            state.dataRptCoilSelection->coilSelectionReportObj->setCoilWaterFlowNodeNums(
                state,
                VarSpeedCoil.Name,
                VarSpeedCoil.VarSpeedCoilType,
                RatedWaterVolFlowRateDes,
                RatedWaterFlowAutoSized,
                VarSpeedCoil.WaterInletNodeNum,
                VarSpeedCoil.WaterOutletNodeNum,
                VarSpeedCoil.plantLoc.loopNum);
            VarSpeedCoil.RatedWaterVolFlowRate = RatedWaterVolFlowRateDes;
            BaseSizer::reportSizerOutput(state,
                                         "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                         VarSpeedCoil.Name,
                                         "Design Size Rated Water Flow Rate [m3/s]",
                                         RatedWaterVolFlowRateDes);
            // Ensure water flow rate at lower speed must be lower or
            // equal to the flow rate at higher speed. Otherwise, a severe error is isssued.
            for (Mode = 1; Mode <= VarSpeedCoil.NumOfSpeeds - 1; ++Mode) {
                if (VarSpeedCoil.MSRatedWaterVolFlowRate(Mode) >
                    VarSpeedCoil.MSRatedWaterVolFlowRate(Mode + 1) * 1.05) {
                    ShowWarningError(
                        state,
                        format("SizeDXCoil: {} {}, Speed {} Rated Air Flow Rate must be less than or equal to Speed {} Rated Air Flow Rate.",
                               VarSpeedCoil.VarSpeedCoilType,
                               VarSpeedCoil.Name,
                               Mode,
                               Mode + 1));
                    ShowContinueError(state,
                                      format("Instead, {:.2R} > {:.2R}",
                                             VarSpeedCoil.MSRatedAirVolFlowRate(Mode),
                                             VarSpeedCoil.MSRatedAirVolFlowRate(Mode + 1)));
                    ShowFatalError(state, "Preceding conditions cause termination.");
                }
            }
        } else {
            if (VarSpeedCoil.RatedWaterVolFlowRate > 0.0 && RatedWaterVolFlowRateDes > 0.0) {
                RatedWaterVolFlowRateUser = VarSpeedCoil.RatedWaterVolFlowRate;
                BaseSizer::reportSizerOutput(state,
                                             "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                             VarSpeedCoil.Name,
                                             "Design Size Rated Water Flow Rate [m3/s]",
                                             RatedWaterVolFlowRateDes,
                                             "User-Specified Rated Water Flow Rate [m3/s]",
                                             RatedWaterVolFlowRateUser);
                if (state.dataGlobal->DisplayExtraWarnings) {
                    if ((std::abs(RatedWaterVolFlowRateDes - RatedWaterVolFlowRateUser) / RatedWaterVolFlowRateUser) >
                        state.dataSize->AutoVsHardSizingThreshold) {
                        ShowMessage(state,
                                    "SizeVarSpeedCoil: Potential issue with equipment sizing for" +
                                        VarSpeedCoil.CoolHeatType + ' ' + CurrentObjSubfix);
                        ShowContinueError(state, "Coil Name =" + VarSpeedCoil.Name);
                        ShowContinueError(state, format("User-Specified Rated Water Flow Rate of {:.5R} [m3/s]", RatedWaterVolFlowRateUser));
                        ShowContinueError(state, format("differs from Design Size Rated Water Flow Rate of {:.5R} [m3/s]", RatedWaterVolFlowRateDes));
                        ShowContinueError(state, "This may, or may not, indicate mismatched component sizes.");
                        ShowContinueError(state, "Verify that the value entered is intended and is consistent with other components.");
                    }
                }
            }
        }

        // Save component design water volumetric flow rate.
        if (VarSpeedCoil.RatedWaterVolFlowRate > 0.0 &&
            VarSpeedCoil.CoolHeatType == "WATERHEATING") {
            RegisterPlantCompDesignFlow(state,
                                        VarSpeedCoil.WaterInletNodeNum,
                                        VarSpeedCoil.RatedWaterVolFlowRate);
        }
        // Use 1/2 flow since both cooling and heating coil will save flow yet only 1 will operate at a time
        else if (VarSpeedCoil.RatedWaterVolFlowRate > 0.0) {
            RegisterPlantCompDesignFlow(state,
                                        VarSpeedCoil.WaterInletNodeNum,
                                        0.5 * VarSpeedCoil.RatedWaterVolFlowRate);
        }

        if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingWaterToAirHPVSEquationFit ||
            VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingWaterToAirHPVSEquationFit) {

            if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingWaterToAirHPVSEquationFit) {
                RatedSourceTempCool = RatedInletWaterTemp;
            } else {
                RatedSourceTempCool = RatedInletWaterTempHeat;
            }

            if (PltSizNum > 0) {
                rhoW = rho;
            } else {
                rhoW = GetDensityGlycol(state,
                                        state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidName,
                                        RatedSourceTempCool,
                                        state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidIndex,
                                        RoutineName);
            }

            VarSpeedCoil.RatedWaterMassFlowRate =
                VarSpeedCoil.RatedWaterVolFlowRate * rhoW;
            for (Mode = VarSpeedCoil.NumOfSpeeds; Mode >= 1; --Mode) {
                VarSpeedCoil.MSRatedWaterVolFlowRate(Mode) =
                    VarSpeedCoil.MSRatedTotCap(Mode) *
                    VarSpeedCoil.MSRatedWaterVolFlowPerRatedTotCap(Mode);
                VarSpeedCoil.MSRatedWaterMassFlowRate(Mode) =
                    VarSpeedCoil.MSRatedWaterVolFlowRate(Mode) * rhoW;
            }
        } else if (VarSpeedCoil.CoolHeatType == "WATERHEATING") {
            RatedSourceTempCool = VarSpeedCoil.WHRatedInletWaterTemp;
            rhoW = RhoH2O(RatedSourceTempCool);
            VarSpeedCoil.RatedWaterMassFlowRate =
                VarSpeedCoil.RatedWaterVolFlowRate * rhoW;
            for (Mode = VarSpeedCoil.NumOfSpeeds; Mode >= 1; --Mode) {
                VarSpeedCoil.MSRatedWaterVolFlowRate(Mode) =
                    VarSpeedCoil.MSRatedTotCap(Mode) *
                    VarSpeedCoil.MSRatedWaterVolFlowPerRatedTotCap(Mode);
                VarSpeedCoil.MSWHPumpPower(Mode) =
                    VarSpeedCoil.MSRatedTotCap(Mode) *
                    VarSpeedCoil.MSWHPumpPowerPerRatedTotCap(Mode);
                VarSpeedCoil.MSRatedWaterMassFlowRate(Mode) =
                    VarSpeedCoil.MSRatedWaterVolFlowRate(Mode) * rhoW;
            }
        } else {
            RatedSourceTempCool = RatedAmbAirTemp;
        }

        // Ensure air flow rate at lower speed must be lower or
        // equal to the flow rate at higher speed. Otherwise, a severe error is isssued.
        for (Mode = 1; Mode <= VarSpeedCoil.NumOfSpeeds - 1; ++Mode) {
            if (VarSpeedCoil.MSRatedAirVolFlowRate(Mode) >
                VarSpeedCoil.MSRatedAirVolFlowRate(Mode + 1)) {
                ShowWarningError(state,
                                 format("SizeDXCoil: {} {}, Speed {} Rated Air Flow Rate must be less than or equal to Speed {} Rated Air Flow Rate.",
                                        VarSpeedCoil.VarSpeedCoilType,
                                        VarSpeedCoil.Name,
                                        Mode,
                                        Mode + 1));
                ShowContinueError(state,
                                  format("Instead, {:.2R} > {:.2R}",
                                         VarSpeedCoil.MSRatedAirVolFlowRate(Mode),
                                         VarSpeedCoil.MSRatedAirVolFlowRate(Mode + 1)));
                ShowFatalError(state, "Preceding conditions cause termination.");
            }
        }

        // Ensure capacity at lower speed must be lower or equal to the capacity at higher speed.
        for (Mode = 1; Mode <= VarSpeedCoil.NumOfSpeeds - 1; ++Mode) {
            if (VarSpeedCoil.MSRatedTotCap(Mode) >
                VarSpeedCoil.MSRatedTotCap(Mode + 1)) {
                ShowWarningError(state,
                                 format("SizeDXCoil: {} {}, Speed {} Rated Total Cooling Capacity must be less than or equal to Speed {} Rated Total "
                                        "Cooling Capacity.",
                                        VarSpeedCoil.VarSpeedCoilType,
                                        VarSpeedCoil.Name,
                                        Mode,
                                        Mode + 1));
                ShowContinueError(state,
                                  format("Instead, {:.2R} > {:.2R}",
                                         VarSpeedCoil.MSRatedTotCap(Mode),
                                         VarSpeedCoil.MSRatedTotCap(Mode + 1)));
                ShowFatalError(state, "Preceding conditions cause termination.");
            }
        }

        // convert SHR to rated Bypass factor and effective air side surface area
        if (VarSpeedCoil.CoolHeatType == "COOLING") {
            for (Mode = 1; Mode <= VarSpeedCoil.NumOfSpeeds; ++Mode) {
                VarSpeedCoil.MSRatedCBF(Mode) =
                    CalcCBF(state,
                            VarSpeedCoil.VarSpeedCoilType,
                            VarSpeedCoil.Name,
                            RatedInletAirTemp,
                            RatedInletAirHumRat,
                            VarSpeedCoil.MSRatedTotCap(Mode),
                            VarSpeedCoil.MSRatedAirVolFlowRate(Mode),
                            VarSpeedCoil.MSRatedSHR(Mode),
                            true);
                if (VarSpeedCoil.MSRatedCBF(Mode) > 0.0) {
                    VarSpeedCoil.MSEffectiveAo(Mode) =
                        -std::log(VarSpeedCoil.MSRatedCBF(Mode)) *
                        VarSpeedCoil.MSRatedAirMassFlowRate(Mode);
                } else {
                    VarSpeedCoil.MSEffectiveAo(Mode) = 0.0;
                }
            }
        } else if (VarSpeedCoil.CoolHeatType == "WATERHEATING") {
            state.dataHVACGlobal->HPWHInletDBTemp = VarSpeedCoil.WHRatedInletDBTemp;
            state.dataHVACGlobal->HPWHInletWBTemp = VarSpeedCoil.WHRatedInletWBTemp;

            rhoA = PsyRhoAirFnPbTdbW(state, state.dataEnvrn->StdBaroPress, state.dataHVACGlobal->HPWHInletDBTemp, HPInletAirHumRat, RoutineName);
            for (Mode = 1; Mode <= VarSpeedCoil.NumOfSpeeds; ++Mode) {
                VarSpeedCoil.MSRatedAirMassFlowRate(Mode) =
                    VarSpeedCoil.MSRatedAirVolFlowRate(Mode) * rhoA;
                // Check for valid range of (Rated Air Volume Flow Rate / Rated Total Capacity)
                // RatedVolFlowPerRatedTotCap = VarSpeedCoil(DXCoilNum).MSRatedAirVolFlowRate(Mode) /
                //    VarSpeedCoil(DXCoilNum).MSRatedTotCap(Mode);//prepared for checking ratios, not used here
            }

            for (Mode = 1; Mode <= VarSpeedCoil.NumOfSpeeds; ++Mode) {
                // get cooling capacity, without fan power, i.e. total coil cooling
                if (VarSpeedCoil.CondPumpPowerInCOP)
                    HPWHCoolCapacity = VarSpeedCoil.MSRatedTotCap(Mode) *
                                           (1.0 - 1.0 / VarSpeedCoil.MSRatedCOP(Mode)) +
                                       VarSpeedCoil.MSWHPumpPower(Mode) -
                                       VarSpeedCoil.MSWHPumpPower(Mode) *
                                           VarSpeedCoil.HPWHCondPumpFracToWater;
                else
                    HPWHCoolCapacity = VarSpeedCoil.MSRatedTotCap(Mode) *
                                           (1.0 - 1.0 / VarSpeedCoil.MSRatedCOP(Mode)) -
                                       VarSpeedCoil.MSWHPumpPower(Mode) *
                                           VarSpeedCoil.HPWHCondPumpFracToWater;

                VarSpeedCoil.MSRatedCBF(Mode) =
                    CalcCBF(state,
                            VarSpeedCoil.VarSpeedCoilType,
                            VarSpeedCoil.Name,
                            state.dataHVACGlobal->HPWHInletDBTemp,
                            HPInletAirHumRat,
                            HPWHCoolCapacity,
                            VarSpeedCoil.MSRatedAirVolFlowRate(Mode),
                            VarSpeedCoil.MSRatedSHR(Mode),
                            true);
                if (VarSpeedCoil.MSRatedCBF(Mode) > 0.0) {
                    VarSpeedCoil.MSEffectiveAo(Mode) =
                        -std::log(VarSpeedCoil.MSRatedCBF(Mode)) *
                        VarSpeedCoil.MSRatedAirMassFlowRate(Mode);
                } else {
                    VarSpeedCoil.MSEffectiveAo(Mode) = 0.0;
                }
            }

            // update VarSpeedCoil(DXCoilNum).RatedCapCoolTotal
            Mode = VarSpeedCoil.NormSpedLevel;
            if (VarSpeedCoil.CondPumpPowerInCOP) {
                VarSpeedCoil.RatedCapCoolTotal =
                    VarSpeedCoil.MSRatedTotCap(Mode) *
                        (1.0 - 1.0 / VarSpeedCoil.MSRatedCOP(Mode)) +
                    VarSpeedCoil.MSWHPumpPower(Mode) -
                    VarSpeedCoil.MSWHPumpPower(Mode) *
                        VarSpeedCoil.HPWHCondPumpFracToWater;
            } else {
                VarSpeedCoil.RatedCapCoolTotal =
                    VarSpeedCoil.MSRatedTotCap(Mode) *
                        (1.0 - 1.0 / VarSpeedCoil.MSRatedCOP(Mode)) -
                    VarSpeedCoil.MSWHPumpPower(Mode) *
                        VarSpeedCoil.HPWHCondPumpFracToWater;
            }
        }

        // size rated sensible cooling capacity
        RatedCapCoolSensAutoSized = true; // always do that

        if (VarSpeedCoil.RatedAirVolFlowRate >= SmallAirVolFlow &&
            VarSpeedCoil.CoolHeatType == "COOLING") {
            RatedAirMassFlowRate = VarSpeedCoil.RatedAirVolFlowRate *
                                   PsyRhoAirFnPbTdbW(state, state.dataEnvrn->StdBaroPress, RatedInletAirTemp, RatedInletAirHumRat, RoutineName);
            RatedInletEnth = PsyHFnTdbW(RatedInletAirTemp, RatedInletAirHumRat);
            CBFRated = AdjustCBF(VarSpeedCoil.MSRatedCBF(NormSpeed),
                                 VarSpeedCoil.MSRatedAirMassFlowRate(NormSpeed),
                                 RatedAirMassFlowRate);
            if (CBFRated > 0.999) CBFRated = 0.999;
            AirMassFlowRatio = RatedAirMassFlowRate / VarSpeedCoil.MSRatedAirMassFlowRate(NormSpeed);

            if (VarSpeedCoil.MSRatedWaterVolFlowRate(NormSpeed) > 1.0e-10) {
                WaterMassFlowRatio = VarSpeedCoil.RatedWaterVolFlowRate /
                                     VarSpeedCoil.MSRatedWaterVolFlowRate(NormSpeed);
            } else {
                WaterMassFlowRatio = 1.0;
            }

            Real64 TempInletWetBulb = RatedInletWetBulbTemp;
            CalcTotCapSHR_VSWSHP(state,
                                 RatedInletAirTemp,
                                 RatedInletAirHumRat,
                                 RatedInletEnth,
                                 TempInletWetBulb,
                                 AirMassFlowRatio,
                                 WaterMassFlowRatio,
                                 RatedAirMassFlowRate,
                                 CBFRated,
                                 VarSpeedCoil.MSRatedTotCap(NormSpeed),
                                 VarSpeedCoil.MSCCapFTemp(NormSpeed),
                                 VarSpeedCoil.MSCCapAirFFlow(NormSpeed),
                                 VarSpeedCoil.MSCCapWaterFFlow(NormSpeed),
                                 0.0,
                                 0,
                                 0,
                                 0,
                                 QLoadTotal1,
                                 QLoadTotal2,
                                 QLoadTotal,
                                 SHR,
                                 RatedSourceTempCool,
                                 state.dataEnvrn->StdBaroPress,
                                 0.0,
                                 1,
                                 VarSpeedCoil.capModFacTotal);

            RatedCapCoolSensDes = VarSpeedCoil.RatedCapCoolTotal * SHR;
        } else if (VarSpeedCoil.RatedAirVolFlowRate >= SmallAirVolFlow &&
                   VarSpeedCoil.CoolHeatType == "WATERHEATING") {
            SHR = VarSpeedCoil.MSRatedSHR(NormSpeed);
            RatedCapCoolSensDes = VarSpeedCoil.RatedCapCoolTotal * SHR;
        } else {
            RatedCapCoolSensDes = 0.0;
        }

        if (RatedCapCoolSensDes < SmallLoad) {
            RatedCapCoolSensDes = 0.0;
        }

        if (VarSpeedCoil.CoolHeatType == "COOLING") { // always report for cooling mode
            if (RatedCapCoolTotalAutoSized) {
                VarSpeedCoil.RatedCapCoolSens = RatedCapCoolSensDes;
                BaseSizer::reportSizerOutput(state,
                                             "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                             VarSpeedCoil.Name,
                                             "Design Size Rated Sensible Cooling Capacity [W]",
                                             VarSpeedCoil.RatedCapCoolSens);

            } else {
                // sensible capacity does not have an input field
                if (RatedCapCoolSensDes > 0.0) {
                    VarSpeedCoil.RatedCapCoolSens = RatedCapCoolSensDes;
                    BaseSizer::reportSizerOutput(state,
                                                 "COIL:" + VarSpeedCoil.CoolHeatType + CurrentObjSubfix,
                                                 VarSpeedCoil.Name,
                                                 "Design Size Rated Sensible Cooling Capacity [W]",
                                                 RatedCapCoolSensDes); //, &
                }
            }
            PreDefTableEntry(state,
                             state.dataOutRptPredefined->pdchCoolCoilTotCap,
                             VarSpeedCoil.Name,
                             VarSpeedCoil.RatedCapCoolTotal);
            PreDefTableEntry(state,
                             state.dataOutRptPredefined->pdchCoolCoilSensCap,
                             VarSpeedCoil.Name,
                             VarSpeedCoil.RatedCapCoolSens);
            PreDefTableEntry(state,
                             state.dataOutRptPredefined->pdchCoolCoilLatCap,
                             VarSpeedCoil.Name,
                             VarSpeedCoil.RatedCapCoolTotal -
                                 VarSpeedCoil.RatedCapCoolSens);
            if (VarSpeedCoil.RatedCapCoolTotal != 0.0) {
                PreDefTableEntry(state,
                                 state.dataOutRptPredefined->pdchCoolCoilSHR,
                                 VarSpeedCoil.Name,
                                 VarSpeedCoil.RatedCapCoolSens /
                                     VarSpeedCoil.RatedCapCoolTotal);
            } else {
                PreDefTableEntry(state, state.dataOutRptPredefined->pdchCoolCoilSHR, VarSpeedCoil.Name, 0.0);
            }
            PreDefTableEntry(state,
                             state.dataOutRptPredefined->pdchCoolCoilNomEff,
                             VarSpeedCoil.Name,
                             VarSpeedCoil.MSRatedCOP(
                                 VarSpeedCoil.NormSpedLevel));
            addFootNoteSubTable(state,
                                state.dataOutRptPredefined->pdstCoolCoil,
                                "Nominal values are gross at rated conditions, i.e., the supply air fan heat and electric power NOT accounted for.");
        }

        // START SIZING EVAP PRECOOLING PUMP POWER
        IsAutoSize = false;
        if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) {
            if (VarSpeedCoil.EvapCondPumpElecNomPower == AutoSize) {
                IsAutoSize = true;
            }
            //     Auto size high speed evap condenser pump power to Total Capacity * 0.004266 w/w (15 w/ton)
            EvapCondPumpElecNomPowerDes = VarSpeedCoil.RatedCapCoolTotal * 0.004266;
            if (IsAutoSize) {
                VarSpeedCoil.EvapCondPumpElecNomPower = EvapCondPumpElecNomPowerDes;
                BaseSizer::reportSizerOutput(state,
                                             "AS VS COOLING COIL",
                                             VarSpeedCoil.Name,
                                             "Design Size Evaporative Condenser Pump Rated Power Consumption [W]",
                                             EvapCondPumpElecNomPowerDes);
            } else {
                if (VarSpeedCoil.EvapCondPumpElecNomPower > 0.0 && EvapCondPumpElecNomPowerDes > 0.0) {
                    EvapCondPumpElecNomPowerUser = VarSpeedCoil.EvapCondPumpElecNomPower;
                    BaseSizer::reportSizerOutput(state,
                                                 "AS VS COOLING COIL",
                                                 VarSpeedCoil.Name,
                                                 "Design Size Evaporative Condenser Pump Rated Power Consumption [W]",
                                                 EvapCondPumpElecNomPowerDes,
                                                 "User-Specified Evaporative Condenser Pump Rated Power Consumption [W]",
                                                 EvapCondPumpElecNomPowerUser);
                    if (state.dataGlobal->DisplayExtraWarnings) {
                        if ((std::abs(EvapCondPumpElecNomPowerDes - EvapCondPumpElecNomPowerUser) / EvapCondPumpElecNomPowerUser) >
                            state.dataSize->AutoVsHardSizingThreshold) {
                            ShowMessage(state,
                                        "SizeVarSpeedCoil: Potential issue with equipment sizing for " +
                                            VarSpeedCoil.CoolHeatType + ' ' + CurrentObjSubfix);
                            ShowContinueError(state, "Coil Name =" + VarSpeedCoil.Name);
                            ShowContinueError(state,
                                              format("User-Specified Evaporative Condenser Pump Rated Power Consumption of {:.2R} [W]",
                                                     EvapCondPumpElecNomPowerUser));
                            ShowContinueError(state,
                                              format("differs from Design Size Evaporative Condenser Pump Rated Power Consumption of {:.2R} [W]",
                                                     EvapCondPumpElecNomPowerDes));
                            ShowContinueError(state, "This may, or may not, indicate mismatched component sizes.");
                            ShowContinueError(state, "Verify that the value entered is intended and is consistent with other components.");
                        }
                    }
                }
            }
        }
        // END SIZING EVAP PRE-COOLING PUMP POWER

        // SIZE DEFROST HEATER

        // Resistive Defrost Heater Capacity = capacity at the first stage
        IsAutoSize = false;
        if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
            if (VarSpeedCoil.DefrostCapacity == AutoSize) {
                IsAutoSize = true;
            }
            if (VarSpeedCoil.DefrostStrategy == Resistive) {
                DefrostCapacityDes = VarSpeedCoil.RatedCapHeat;
            } else {
                DefrostCapacityDes = 0.0;
            }
            if (IsAutoSize) {
                VarSpeedCoil.DefrostCapacity = DefrostCapacityDes;
                BaseSizer::reportSizerOutput(state,
                                             "AS VS HEATING COIL",
                                             VarSpeedCoil.Name,
                                             "Design Size Resistive Defrost Heater Capacity [W]",
                                             DefrostCapacityDes);
            } else {
                if (VarSpeedCoil.DefrostCapacity > 0.0 && DefrostCapacityDes > 0.0 && !HardSizeNoDesRun) {
                    DefrostCapacityUser = VarSpeedCoil.DefrostCapacity;
                    BaseSizer::reportSizerOutput(state,
                                                 "AS VS HEATING COIL",
                                                 VarSpeedCoil.Name,
                                                 "Design Size Resistive Defrost Heater Capacity [W]",
                                                 DefrostCapacityDes,
                                                 "User-Specified Resistive Defrost Heater Capacity [W]",
                                                 DefrostCapacityUser);
                    if (state.dataGlobal->DisplayExtraWarnings) {
                        if ((std::abs(DefrostCapacityDes - DefrostCapacityUser) / DefrostCapacityUser) > state.dataSize->AutoVsHardSizingThreshold) {
                            ShowMessage(state,
                                        "SizeVarSpeedCoil: Potential issue with equipment sizing for " +
                                            VarSpeedCoil.CoolHeatType + ' ' + CurrentObjSubfix);
                            ShowContinueError(state, "Coil Name =" + VarSpeedCoil.Name);
                            ShowContinueError(state, format("User-Specified Resistive Defrost Heater Capacity of {:.2R} [W]", DefrostCapacityUser));
                            ShowContinueError(state,
                                              format("differs from Design Size Resistive Defrost Heater Capacity of {:.2R} [W]", DefrostCapacityDes));
                            ShowContinueError(state, "This may, or may not, indicate mismatched component sizes.");
                            ShowContinueError(state, "Verify that the value entered is intended and is consistent with other components.");
                        }
                    }
                }
            }
        }
        // END SIZING DEFROST HEATER

        // test autosized sensible and total cooling capacity for total > sensible
        if (RatedCapCoolSensAutoSized && RatedCapCoolTotalAutoSized) {
            if (VarSpeedCoil.RatedCapCoolSens >
                VarSpeedCoil.RatedCapCoolTotal) {
                ShowWarningError(state,
                                 "COIL:" + VarSpeedCoil.CoolHeatType +
                                     ":WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT \"" + VarSpeedCoil.Name +
                                     "\"");
                ShowContinueError(state, std::string{RoutineName} + ": Rated Sensible Cooling Capacity > Rated Total Cooling Capacity");
                ShowContinueError(state, "Each of these capacity inputs have been autosized.");
                ShowContinueError(
                    state,
                    format("Rated Sensible Cooling Capacity = {:.2T} W", VarSpeedCoil.RatedCapCoolSens));
                ShowContinueError(
                    state,
                    format("Rated Total Cooling Capacity    = {:.2T} W", VarSpeedCoil.RatedCapCoolTotal));
                ShowContinueError(state, "See eio file for further details.");
                ShowContinueError(state, "Check Total and Sensible Cooling Capacity Coefficients to ensure they are accurate.");
                ShowContinueError(state, "Check Zone and System Sizing objects to verify sizing inputs.");
                ShowContinueError(state, "Sizing statistics:");
                ShowContinueError(state, format("Entering Air Dry-Bulb Temperature = {:.3T} C", MixTemp));
                ShowContinueError(state, format("Entering Air Wet-Bulb Temperature = {:.3T} C", MixWetBulb));
                ShowContinueError(state, "Entering Condenser Water Temperature used = 24.4444 C");
                ShowContinueError(state, "Used design air and water flow rates (i.e., used 1 for ratioVL and ratioVS)");
                ShowContinueError(state, format("ratioTDB = {:.3T}", ((MixTemp + 283.15) / 273.15)));
                ShowContinueError(state, format("ratioTWB = {:.3T}", ((MixWetBulb + 283.15) / 273.15)));
                ShowContinueError(state, format("ratioTS  = {:.3T}", ((85.0 + 283.15) / 273.15)));
                ShowContinueError(state, "Rated Sensible Cooling Capacity = Rated Total Cooling Capacity * Sensible Heat Ratio");
                ShowContinueError(state, format("Total Cooling Capacity Modifier = {:.5T}", TotCapTempModFac));
                ShowContinueError(state, "...Rated Total Cooling Capacity = Total Design Load / Total Cooling Capacity Modifier");
                ShowContinueError(state, "Carefully review the Load Side Total, Sensible, and Latent heat transfer rates");
                ShowContinueError(state, "... to ensure they meet the expected manufacturers performance specifications.");
            }
        } else if (RatedCapCoolTotalAutoSized) {
            if (VarSpeedCoil.RatedCapCoolSens >
                VarSpeedCoil.RatedCapCoolTotal) {
                ShowWarningError(state,
                                 "COIL:" + VarSpeedCoil.CoolHeatType +
                                     ":WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT \"" + VarSpeedCoil.Name +
                                     "\"");
                ShowContinueError(state, std::string{RoutineName} + ": Rated Sensible Cooling Capacity > Rated Total Cooling Capacity");
                ShowContinueError(state, "Only the rated total capacity input is autosized, consider autosizing both inputs.");
                ShowContinueError(
                    state,
                    format("Rated Sensible Cooling Capacity = {:.2T} W", VarSpeedCoil.RatedCapCoolSens));
                ShowContinueError(
                    state,
                    format("Rated Total Cooling Capacity    = {:.2T} W", VarSpeedCoil.RatedCapCoolTotal));
                ShowContinueError(state, "See eio file for further details.");
                ShowContinueError(state, "Check Total and Sensible Cooling Capacity Coefficients to ensure they are accurate.");
                ShowContinueError(state, "Check Zone and System Sizing objects to verify sizing inputs.");
                ShowContinueError(state, "Sizing statistics for Total Cooling Capacity:");
                ShowContinueError(state, format("Entering Air Wet-Bulb Temperature = {:.3T} C", MixWetBulb));
                ShowContinueError(state, "Entering Condenser Water Temperature used = 24.4444 C");
                ShowContinueError(state, "Used design air and water flow rates (i.e., used 1 for ratioVL and ratioVS)");
                ShowContinueError(state, format("ratioTWB = {:.3T}", ((MixWetBulb + 283.15) / 273.15)));
                ShowContinueError(state, format("ratioTS  = {:.3T}", ((85.0 + 283.15) / 273.15)));
                ShowContinueError(state, "Rated Sensible Cooling Capacity = Rated Total Cooling Capacity * Sensible Heat Ratio");
                ShowContinueError(state, "Carefully review the Load Side Total, Sensible, and Latent heat transfer rates");
                ShowContinueError(state, "... to ensure they meet the expected manufacturers performance specifications.");
            }
        }
    }

    void CalcVarSpeedCoilCooling(EnergyPlusData &state,
                                 int const DXCoilNum,                        // Heat Pump Number
                                 int const CyclingScheme,                    // Fan/Compressor cycling scheme indicator
                                 Real64 &RuntimeFrac,                        // Runtime Fraction of compressor or percent on time (on-time/cycle time)
                                 [[maybe_unused]] Real64 const SensDemand,   // Cooling Sensible Demand [W] !unused1208
                                 [[maybe_unused]] Real64 const LatentDemand, // Cooling Latent Demand [W]
                                 CompressorOperation const CompressorOp,     // compressor operation flag
                                 Real64 const PartLoadRatio,                 // compressor part load ratio
                                 [[maybe_unused]] Real64 const OnOffAirFlowRatio, // ratio of compressor on flow to average flow over time step
                                 Real64 const SpeedRatio, // SpeedRatio varies between 1.0 (higher speed) and 0.0 (lower speed)
                                 int const SpeedNum       // Speed number, high bound
    )
    {

        //       AUTHOR         Bo Shen, based on WaterToAirHeatPumpSimple:CalcHPCoolingSimple
        //       DATE WRITTEN   March 2012
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for simulating the cooling mode of the Variable-Speed Water to Air HP Simple

        // METHODOLOGY EMPLOYED:
        // Simulate the heat pump performance using the coefficients and rated conditions, interpolating between speed levels
        // If the LatDegradModelSimFlag is enabled, the coil will be simulated twice:
        // (1)first simulation at the rated conditions (2) second simulation at the
        // actual operating conditions. Then call CalcEffectiveSHR and the effective SHR
        // is adjusted.
        // If the LatDegradModelSimFlag is disabled, the cooling coil is only simulated
        // once at the actual operating conditions.
        // Finally, adjust the heat pump outlet conditions based on the PartLoadRatio
        // and RuntimeFrac.

        // Using/Aliasing
        using CurveManager::CurveValue;
        auto &TimeStepSys = state.dataHVACGlobal->TimeStepSys;
        using FluidProperties::GetSpecificHeatGlycol;
        using Psychrometrics::PsyCpAirFnW;
        using Psychrometrics::PsyHFnTdbW;
        using Psychrometrics::PsyRhoAirFnPbTdbW;
        using Psychrometrics::PsyTdbFnHW;
        using Psychrometrics::PsyTwbFnTdbWPb;
        using Psychrometrics::PsyWFnTdbH;
        using Psychrometrics::PsyWFnTdbTwbPb;

        // Locals
        // SUBROUTINE ARGUMENT DEFINITIONS:

        // SUBROUTINE PARAMETER DEFINITIONS:
        static constexpr std::string_view RoutineName("CalcVarSpeedCoilCooling");
        static constexpr std::string_view RoutineNameSourceSideInletTemp("CalcVarSpeedCoilCooling:SourceSideInletTemp");

        // INTERFACE BLOCK SPECIFICATIONS
        // na

        // DERIVED TYPE DEFINITIONS
        // na

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:

        Real64 Twet_Rated;  // Twet at rated conditions (coil air flow rate and air temperatures), sec
        Real64 Gamma_Rated; // Gamma at rated conditions (coil air flow rate and air temperatures)

        Real64 SHRss;    // Sensible heat ratio at steady state
        Real64 SHReff;   // Effective sensible heat ratio at part-load condition
        Real64 CpSource; // Specific heat of water [J/kg_C]
        Real64 CpAir;    // Specific heat of air [J/kg_C]
        Real64 ReportingConstant;

        bool LatDegradModelSimFlag;      // Latent degradation model simulation flag
        int NumIteration;                // Iteration Counter
        Real64 LoadSideInletDBTemp_Unit; // calc conditions for unit
        Real64 LoadSideInletWBTemp_Unit; // calc conditions for unit
        Real64 LoadSideInletHumRat_Unit; // calc conditions for unit
        Real64 LoadSideInletEnth_Unit;   // calc conditions for unit
        Real64 CpAir_Unit;               // calc conditions for unit
        Real64 AirMassFlowRatio;         // airflow ratio at low speed
        Real64 WaterMassFlowRatio;       // airflow ratio at high speed
        Real64 EIRAirFFModFac;           // air flow fraction modification
        Real64 EIRWaterFFModFac;         // water flow fraction modification
        Real64 EIRTempModFac;            // total capacity temperature correctio fraction
        Real64 CBFSpeed;                 // total capacity temperature correctio fraction
        Real64 SHR;                      // total capacity temperature correctio fraction
        Real64 EIR;                      // total capacity temperature correctio fraction
        int MaxSpeed;                    // maximum speed level
        int SpeedCal;                    // calculated speed level
        Real64 AoEff;                    // effective air side surface area
        Real64 QLoadTotal1;              // total capacity at low speed
        Real64 QLoadTotal2;              // total capacity at high speed
        Real64 Winput1;                  // power consumption at low speed
        Real64 Winput2;                  // power consumption at high speed
        Real64 QWasteHeat;               // recoverable waste heat
        Real64 QWasteHeat1;              // recoverable waste heat at low speed
        Real64 QWasteHeat2;              // recoverable waste heat at high speed
        Real64 PLF;                      // part-load function
        Real64 MaxHumRat;                // max possible humidity
        Real64 MaxOutletEnth;            // max possible outlet enthalpy

        // ADDED VARIABLES FOR air source coil
        Real64 CondInletTemp; // Condenser inlet temperature (C). Outdoor dry-bulb temp for air-cooled condenser.
        // Outdoor Wetbulb +(1 - effectiveness)*(outdoor drybulb - outdoor wetbulb) for evap condenser.
        Real64 CondInletHumRat; // Condenser inlet humidity ratio (kg/kg). Zero for air-cooled condenser.
        // For evap condenser, its the humidity ratio of the air leaving the evap cooling pads.
        Real64 CondAirMassFlow;    // Condenser air mass flow rate [kg/s]
        Real64 RhoSourceAir;       // Density of air [kg/m3]
        Real64 RhoEvapCondWater;   // Density of water used for evaporative condenser [kg/m3]
        Real64 EvapCondEffectSped; // condenser evaporative effectiveness at the speed level
        Real64 RhoWater;           // condensed water density
        Real64 SpecHumIn;          // inlet air specific humidity
        Real64 SpecHumOut;         // outlet air specific humidity
        Real64 rhoair(0);          // entering air density

        if (state.dataVariableSpeedCoils->firstTime) {
            // Set indoor air conditions to the rated condition
            state.dataVariableSpeedCoils->LoadSideInletDBTemp_Init = 26.7;
            state.dataVariableSpeedCoils->LoadSideInletHumRat_Init = 0.0111;
            state.dataVariableSpeedCoils->LoadSideInletEnth_Init =
                PsyHFnTdbW(state.dataVariableSpeedCoils->LoadSideInletDBTemp_Init, state.dataVariableSpeedCoils->LoadSideInletHumRat_Init);
            state.dataVariableSpeedCoils->CpAir_Init = PsyCpAirFnW(state.dataVariableSpeedCoils->LoadSideInletHumRat_Init);
            state.dataVariableSpeedCoils->firstTime = false;
        }
        state.dataVariableSpeedCoils->LoadSideInletWBTemp_Init = PsyTwbFnTdbWPb(state,
                                                                                state.dataVariableSpeedCoils->LoadSideInletDBTemp_Init,
                                                                                state.dataVariableSpeedCoils->LoadSideInletHumRat_Init,
                                                                                state.dataEnvrn->OutBaroPress,
                                                                                RoutineName);

        auto &VarSpeedCoil = state.dataVariableSpeedCoils->VarSpeedCoil(DXCoilNum);

        MaxSpeed = VarSpeedCoil.NumOfSpeeds;

        // must be placed inside the loop, otherwise cause bug in release mode, need to be present at two places
        if (SpeedNum > MaxSpeed) {
            SpeedCal = MaxSpeed;
        } else {
            SpeedCal = SpeedNum;
        }

        //  LOAD LOCAL VARIABLES FROM DATA STRUCTURE (for code readability)
        if (!(CyclingScheme == ContFanCycCoil) && PartLoadRatio > 0.0) {
            VarSpeedCoil.AirMassFlowRate =
                state.dataLoopNodes->Node(VarSpeedCoil.AirInletNodeNum).MassFlowRate / PartLoadRatio;
        }

        Twet_Rated = VarSpeedCoil.Twet_Rated;
        Gamma_Rated = VarSpeedCoil.Gamma_Rated;

        state.dataVariableSpeedCoils->LoadSideMassFlowRate = VarSpeedCoil.AirMassFlowRate;

        if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) {
            // Get condenser outdoor node info from DX COOLING Coil
            if (VarSpeedCoil.CondenserInletNodeNum != 0) {
                state.dataVariableSpeedCoils->OutdoorDryBulb_CalcVarSpeedCoilCooling =
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).Temp;
                state.dataVariableSpeedCoils->OutdoorHumRat_CalcVarSpeedCoilCooling =
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).HumRat;
                state.dataVariableSpeedCoils->OutdoorPressure_CalcVarSpeedCoilCooling =
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).Press;
                state.dataVariableSpeedCoils->OutdoorWetBulb_CalcVarSpeedCoilCooling =
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).OutAirWetBulb;
            } else {
                state.dataVariableSpeedCoils->OutdoorDryBulb_CalcVarSpeedCoilCooling = state.dataEnvrn->OutDryBulbTemp;
                state.dataVariableSpeedCoils->OutdoorHumRat_CalcVarSpeedCoilCooling = state.dataEnvrn->OutHumRat;
                state.dataVariableSpeedCoils->OutdoorPressure_CalcVarSpeedCoilCooling = state.dataEnvrn->OutBaroPress;
                state.dataVariableSpeedCoils->OutdoorWetBulb_CalcVarSpeedCoilCooling = state.dataEnvrn->OutWetBulbTemp;
            }

            RhoSourceAir = PsyRhoAirFnPbTdbW(state,
                                             state.dataVariableSpeedCoils->OutdoorPressure_CalcVarSpeedCoilCooling,
                                             state.dataVariableSpeedCoils->OutdoorDryBulb_CalcVarSpeedCoilCooling,
                                             state.dataVariableSpeedCoils->OutdoorHumRat_CalcVarSpeedCoilCooling);

            if ((SpeedNum == 1) || (SpeedNum > MaxSpeed) || (SpeedRatio == 1.0)) {
                CondAirMassFlow = RhoSourceAir * VarSpeedCoil.EvapCondAirFlow(SpeedCal);
            } else {
                CondAirMassFlow =
                    RhoSourceAir * (VarSpeedCoil.EvapCondAirFlow(SpeedCal) * SpeedRatio +
                                    (1.0 - SpeedRatio) * VarSpeedCoil.EvapCondAirFlow(SpeedCal - 1));
            }

            // AIR COOL OR EVAP COOLED CONDENSER
            if (VarSpeedCoil.CondenserType == DataHeatBalance::RefrigCondenserType::Evap) {
                if ((SpeedNum == 1) || (SpeedNum > MaxSpeed) || (SpeedRatio == 1.0)) {
                    EvapCondEffectSped = VarSpeedCoil.EvapCondEffect(SpeedCal);
                } else {
                    EvapCondEffectSped = VarSpeedCoil.EvapCondEffect(SpeedCal) * SpeedRatio +
                                         (1.0 - SpeedRatio) * VarSpeedCoil.EvapCondEffect(SpeedCal - 1);
                }
                // (Outdoor wet-bulb temp from DataEnvironment) + (1.0-EvapCondEffectiveness) * (drybulb - wetbulb)
                CondInletTemp = state.dataVariableSpeedCoils->OutdoorWetBulb_CalcVarSpeedCoilCooling +
                                (state.dataVariableSpeedCoils->OutdoorDryBulb_CalcVarSpeedCoilCooling -
                                 state.dataVariableSpeedCoils->OutdoorWetBulb_CalcVarSpeedCoilCooling) *
                                    (1.0 - EvapCondEffectSped);
                CondInletHumRat = PsyWFnTdbTwbPb(state,
                                                 CondInletTemp,
                                                 state.dataVariableSpeedCoils->OutdoorWetBulb_CalcVarSpeedCoilCooling,
                                                 state.dataVariableSpeedCoils->OutdoorPressure_CalcVarSpeedCoilCooling);
                state.dataVariableSpeedCoils->CompAmbTemp_CalcVarSpeedCoilCooling = CondInletTemp;
            } else {                                                                                  // AIR COOLED CONDENSER
                CondInletTemp = state.dataVariableSpeedCoils->OutdoorDryBulb_CalcVarSpeedCoilCooling; // Outdoor dry-bulb temp
                state.dataVariableSpeedCoils->CompAmbTemp_CalcVarSpeedCoilCooling =
                    state.dataVariableSpeedCoils->OutdoorDryBulb_CalcVarSpeedCoilCooling;
                CondInletHumRat = state.dataEnvrn->OutHumRat;
            }

            state.dataVariableSpeedCoils->SourceSideMassFlowRate = CondAirMassFlow;
            state.dataVariableSpeedCoils->SourceSideInletTemp = CondInletTemp;
            state.dataVariableSpeedCoils->SourceSideInletEnth = PsyHFnTdbW(CondInletTemp, CondInletHumRat);
            CpSource = PsyCpAirFnW(CondInletHumRat);
            VarSpeedCoil.CondInletTemp = CondInletTemp;

            // If used in a heat pump, the value of MaxOAT in the heating coil overrides that in the cooling coil (in GetInput)
            // Initialize crankcase heater, operates below OAT defined in input deck for HP DX heating coil
            if (state.dataVariableSpeedCoils->OutdoorDryBulb_CalcVarSpeedCoilCooling <
                VarSpeedCoil.MaxOATCrankcaseHeater) {
                state.dataVariableSpeedCoils->CrankcaseHeatingPower_CalcVarSpeedCoilCooling =
                    VarSpeedCoil.CrankcaseHeaterCapacity;
            } else {
                state.dataVariableSpeedCoils->CrankcaseHeatingPower_CalcVarSpeedCoilCooling = 0.0;
            }
        } else {
            state.dataVariableSpeedCoils->SourceSideMassFlowRate = VarSpeedCoil.WaterMassFlowRate;
            state.dataVariableSpeedCoils->SourceSideInletTemp = VarSpeedCoil.InletWaterTemp;
            state.dataVariableSpeedCoils->SourceSideInletEnth = VarSpeedCoil.InletWaterEnthalpy;
            CpSource =
                GetSpecificHeatGlycol(state,
                                      state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidName,
                                      state.dataVariableSpeedCoils->SourceSideInletTemp,
                                      state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidIndex,
                                      RoutineNameSourceSideInletTemp);
        }

        // Check for flows, do not perform simulation if no flow in load side or source side.
        if (state.dataVariableSpeedCoils->SourceSideMassFlowRate <= 0.0 || state.dataVariableSpeedCoils->LoadSideMassFlowRate <= 0.0) {

            if ((VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) &&
                (VarSpeedCoil.CondenserType == DataHeatBalance::RefrigCondenserType::Air) &&
                (state.dataVariableSpeedCoils->LoadSideMassFlowRate > 0.0)) {
                // ALLOW SIMULATION IF AIR-COOLED CONDENSER COIL
                VarSpeedCoil.SimFlag = true;
            } else {
                VarSpeedCoil.SimFlag = false;
                return;
            }
        } else {
            VarSpeedCoil.SimFlag = true;
        }

        if (CompressorOp == CompressorOperation::Off) {
            VarSpeedCoil.SimFlag = false;
            return;
        }

        if ((VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) &&
            (CondInletTemp < VarSpeedCoil.MinOATCompressor)) {
            VarSpeedCoil.SimFlag = false;
            return;
        }

        // Loop the calculation at least once depending whether the latent degradation model
        // is enabled. 1st iteration to calculate the QLatent(rated) at (TDB,TWB)indoorair=(26.7C,19.4C)
        // and 2nd iteration to calculate the  QLatent(actual)
        if ((PartLoadRatio < 1e-10) || (Twet_Rated <= 0.0) || (Gamma_Rated <= 0.0) || (SpeedNum > 1.0)) {
            LatDegradModelSimFlag = false;
            // Set NumIteration=1 so that latent model would quit after 1 simulation with the actual condition
            NumIteration = 1;
        } else {
            LatDegradModelSimFlag = true;
            // Set NumIteration=0 so that latent model would simulate twice with rated and actual condition
            NumIteration = 0;
        }

        // Set indoor air conditions to the actual condition
        LoadSideInletDBTemp_Unit = VarSpeedCoil.InletAirDBTemp;
        LoadSideInletHumRat_Unit = VarSpeedCoil.InletAirHumRat;
        LoadSideInletWBTemp_Unit =
            PsyTwbFnTdbWPb(state, LoadSideInletDBTemp_Unit, LoadSideInletHumRat_Unit, state.dataEnvrn->OutBaroPress, RoutineName);
        LoadSideInletEnth_Unit = VarSpeedCoil.InletAirEnthalpy;
        CpAir_Unit = PsyCpAirFnW(LoadSideInletHumRat_Unit);

        RuntimeFrac = 1.0;
        state.dataHVACGlobal->OnOffFanPartLoadFraction = 1.0;
        VarSpeedCoil.RunFrac = 1.0;
        if ((SpeedNum == 1) && (PartLoadRatio < 1.0)) {
            PLF = CurveValue(state, VarSpeedCoil.PLFFPLR, PartLoadRatio);
            if (PLF < 0.7) {
                PLF = 0.7;
            }
            if (CyclingScheme == CycFanCycCoil)
                state.dataHVACGlobal->OnOffFanPartLoadFraction =
                    PLF; // save PLF for fan model, don't change fan power for constant fan mode if coil is off
            // calculate the run time fraction
            VarSpeedCoil.RunFrac = PartLoadRatio / PLF;
            VarSpeedCoil.PartLoadRatio = PartLoadRatio;

            if (VarSpeedCoil.RunFrac > 1.0) {
                VarSpeedCoil.RunFrac = 1.0; // Reset coil runtime fraction to 1.0
            } else if (VarSpeedCoil.RunFrac < 0.0) {
                VarSpeedCoil.RunFrac = 0.0;
            }

            RuntimeFrac = VarSpeedCoil.RunFrac;
        }

        while (true) {
            ++NumIteration;
            if (NumIteration == 1) {
                // Set indoor air conditions to the rated conditions
                state.dataVariableSpeedCoils->LoadSideInletDBTemp = state.dataVariableSpeedCoils->LoadSideInletDBTemp_Init;
                state.dataVariableSpeedCoils->LoadSideInletHumRat = state.dataVariableSpeedCoils->LoadSideInletHumRat_Init;
                state.dataVariableSpeedCoils->LoadSideInletWBTemp = state.dataVariableSpeedCoils->LoadSideInletWBTemp_Init;
                state.dataVariableSpeedCoils->LoadSideInletEnth = state.dataVariableSpeedCoils->LoadSideInletEnth_Init;
                CpAir = state.dataVariableSpeedCoils->CpAir_Init;
            } else {
                // Set indoor air conditions to the actual condition
                state.dataVariableSpeedCoils->LoadSideInletDBTemp = LoadSideInletDBTemp_Unit;
                state.dataVariableSpeedCoils->LoadSideInletHumRat = LoadSideInletHumRat_Unit;
                state.dataVariableSpeedCoils->LoadSideInletWBTemp = LoadSideInletWBTemp_Unit;
                state.dataVariableSpeedCoils->LoadSideInletEnth = LoadSideInletEnth_Unit;
                CpAir = CpAir_Unit;
            }

            // must be placed inside the loop, otherwise cause bug in release mode
            if (SpeedNum > MaxSpeed) {
                SpeedCal = MaxSpeed;
            } else {
                SpeedCal = SpeedNum;
            }

            if ((SpeedNum == 1) || (SpeedNum > MaxSpeed) || (SpeedRatio == 1.0)) {
                AirMassFlowRatio =
                    state.dataVariableSpeedCoils->LoadSideMassFlowRate / VarSpeedCoil.DesignAirMassFlowRate;

                if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) {
                    WaterMassFlowRatio = 1.0;
                } else {
                    WaterMassFlowRatio = state.dataVariableSpeedCoils->SourceSideMassFlowRate /
                                         VarSpeedCoil.DesignWaterMassFlowRate;
                }

                CBFSpeed = AdjustCBF(VarSpeedCoil.MSRatedCBF(SpeedCal),
                                     VarSpeedCoil.MSRatedAirMassFlowRate(SpeedCal),
                                     state.dataVariableSpeedCoils->LoadSideMassFlowRate);

                if (CBFSpeed > 0.999) CBFSpeed = 0.999;

                CalcTotCapSHR_VSWSHP(state,
                                     state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                     state.dataVariableSpeedCoils->LoadSideInletHumRat,
                                     state.dataVariableSpeedCoils->LoadSideInletEnth,
                                     state.dataVariableSpeedCoils->LoadSideInletWBTemp,
                                     AirMassFlowRatio,
                                     WaterMassFlowRatio,
                                     state.dataVariableSpeedCoils->LoadSideMassFlowRate,
                                     CBFSpeed,
                                     VarSpeedCoil.MSRatedTotCap(SpeedCal),
                                     VarSpeedCoil.MSCCapFTemp(SpeedCal),
                                     VarSpeedCoil.MSCCapAirFFlow(SpeedCal),
                                     VarSpeedCoil.MSCCapWaterFFlow(SpeedCal),
                                     0.0,
                                     0,
                                     0,
                                     0,
                                     QLoadTotal1,
                                     QLoadTotal2,
                                     state.dataVariableSpeedCoils->QLoadTotal,
                                     SHR,
                                     state.dataVariableSpeedCoils->SourceSideInletTemp,
                                     VarSpeedCoil.InletAirPressure,
                                     0.0,
                                     1,
                                     VarSpeedCoil.capModFacTotal);

                EIRTempModFac = CurveValue(state,
                                           VarSpeedCoil.MSEIRFTemp(SpeedCal),
                                           state.dataVariableSpeedCoils->LoadSideInletWBTemp,
                                           state.dataVariableSpeedCoils->SourceSideInletTemp);
                EIRAirFFModFac = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(SpeedCal), AirMassFlowRatio);

                if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) {
                    EIRWaterFFModFac = 1.0;
                } else {
                    EIRWaterFFModFac =
                        CurveValue(state, VarSpeedCoil.MSEIRWaterFFlow(SpeedCal), WaterMassFlowRatio);
                }

                EIR = (1.0 / VarSpeedCoil.MSRatedCOP(SpeedCal)) * EIRTempModFac * EIRAirFFModFac *
                      EIRWaterFFModFac;

                CBFSpeed = AdjustCBF(VarSpeedCoil.MSRatedCBF(SpeedCal),
                                     VarSpeedCoil.MSRatedAirMassFlowRate(SpeedCal),
                                     state.dataVariableSpeedCoils->LoadSideMassFlowRate);

                if (CBFSpeed > 0.999) CBFSpeed = 0.999;

                CalcTotCapSHR_VSWSHP(state,
                                     state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                     state.dataVariableSpeedCoils->LoadSideInletHumRat,
                                     state.dataVariableSpeedCoils->LoadSideInletEnth,
                                     state.dataVariableSpeedCoils->LoadSideInletWBTemp,
                                     AirMassFlowRatio,
                                     WaterMassFlowRatio,
                                     state.dataVariableSpeedCoils->LoadSideMassFlowRate,
                                     CBFSpeed,
                                     VarSpeedCoil.MSRatedTotCap(SpeedCal),
                                     VarSpeedCoil.MSCCapFTemp(SpeedCal),
                                     VarSpeedCoil.MSCCapAirFFlow(SpeedCal),
                                     VarSpeedCoil.MSCCapWaterFFlow(SpeedCal),
                                     0.0,
                                     0,
                                     0,
                                     0,
                                     QLoadTotal1,
                                     QLoadTotal2,
                                     state.dataVariableSpeedCoils->QLoadTotal,
                                     SHR,
                                     state.dataVariableSpeedCoils->SourceSideInletTemp,
                                     VarSpeedCoil.InletAirPressure,
                                     0.0,
                                     1,
                                     VarSpeedCoil.capModFacTotal);

                state.dataVariableSpeedCoils->Winput = state.dataVariableSpeedCoils->QLoadTotal * EIR;

                if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) {
                    QWasteHeat = 0.0;
                } else {
                    QWasteHeat =
                        state.dataVariableSpeedCoils->Winput * VarSpeedCoil.MSWasteHeatFrac(SpeedCal);
                    QWasteHeat *= CurveValue(state,
                                             VarSpeedCoil.MSWasteHeat(SpeedCal),
                                             state.dataVariableSpeedCoils->LoadSideInletWBTemp,
                                             state.dataVariableSpeedCoils->SourceSideInletTemp);
                }
            } else {
                AirMassFlowRatio =
                    state.dataVariableSpeedCoils->LoadSideMassFlowRate / VarSpeedCoil.DesignAirMassFlowRate;

                if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) {
                    WaterMassFlowRatio = 1.0;
                } else {
                    WaterMassFlowRatio = state.dataVariableSpeedCoils->SourceSideMassFlowRate /
                                         VarSpeedCoil.DesignWaterMassFlowRate;
                }

                AoEff = VarSpeedCoil.MSEffectiveAo(SpeedCal) * SpeedRatio +
                        (1.0 - SpeedRatio) * VarSpeedCoil.MSEffectiveAo(SpeedCal - 1);

                CBFSpeed = std::exp(-AoEff / state.dataVariableSpeedCoils->LoadSideMassFlowRate);

                if (CBFSpeed > 0.999) CBFSpeed = 0.999;

                CalcTotCapSHR_VSWSHP(state,
                                     state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                     state.dataVariableSpeedCoils->LoadSideInletHumRat,
                                     state.dataVariableSpeedCoils->LoadSideInletEnth,
                                     state.dataVariableSpeedCoils->LoadSideInletWBTemp,
                                     AirMassFlowRatio,
                                     WaterMassFlowRatio,
                                     state.dataVariableSpeedCoils->LoadSideMassFlowRate,
                                     CBFSpeed,
                                     VarSpeedCoil.MSRatedTotCap(SpeedCal - 1),
                                     VarSpeedCoil.MSCCapFTemp(SpeedCal - 1),
                                     VarSpeedCoil.MSCCapAirFFlow(SpeedCal - 1),
                                     VarSpeedCoil.MSCCapWaterFFlow(SpeedCal - 1),
                                     VarSpeedCoil.MSRatedTotCap(SpeedCal),
                                     VarSpeedCoil.MSCCapFTemp(SpeedCal),
                                     VarSpeedCoil.MSCCapAirFFlow(SpeedCal),
                                     VarSpeedCoil.MSCCapWaterFFlow(SpeedCal),
                                     QLoadTotal1,
                                     QLoadTotal2,
                                     state.dataVariableSpeedCoils->QLoadTotal,
                                     SHR,
                                     state.dataVariableSpeedCoils->SourceSideInletTemp,
                                     VarSpeedCoil.InletAirPressure,
                                     SpeedRatio,
                                     2,
                                     VarSpeedCoil.capModFacTotal);

                SpeedCal = SpeedNum - 1;
                EIRTempModFac = CurveValue(state,
                                           VarSpeedCoil.MSEIRFTemp(SpeedCal),
                                           state.dataVariableSpeedCoils->LoadSideInletWBTemp,
                                           state.dataVariableSpeedCoils->SourceSideInletTemp);
                EIRAirFFModFac = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(SpeedCal), AirMassFlowRatio);

                if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) {
                    EIRWaterFFModFac = 1.0;
                } else {
                    EIRWaterFFModFac =
                        CurveValue(state, VarSpeedCoil.MSEIRWaterFFlow(SpeedCal), WaterMassFlowRatio);
                }

                EIR = (1.0 / VarSpeedCoil.MSRatedCOP(SpeedCal)) * EIRTempModFac * EIRAirFFModFac *
                      EIRWaterFFModFac;
                Winput1 = QLoadTotal1 * EIR;

                if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) {
                    QWasteHeat1 = 0.0;
                } else {
                    QWasteHeat1 = Winput1 * VarSpeedCoil.MSWasteHeatFrac(SpeedCal);
                    QWasteHeat1 *= CurveValue(state,
                                              VarSpeedCoil.MSWasteHeat(SpeedCal),
                                              state.dataVariableSpeedCoils->LoadSideInletWBTemp,
                                              state.dataVariableSpeedCoils->SourceSideInletTemp);
                }

                SpeedCal = SpeedNum;
                EIRTempModFac = CurveValue(state,
                                           VarSpeedCoil.MSEIRFTemp(SpeedCal),
                                           state.dataVariableSpeedCoils->LoadSideInletWBTemp,
                                           state.dataVariableSpeedCoils->SourceSideInletTemp);
                EIRAirFFModFac = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(SpeedCal), AirMassFlowRatio);

                if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) {
                    EIRWaterFFModFac = 1.0;
                } else {
                    EIRWaterFFModFac =
                        CurveValue(state, VarSpeedCoil.MSEIRWaterFFlow(SpeedCal), WaterMassFlowRatio);
                }

                EIR = (1.0 / VarSpeedCoil.MSRatedCOP(SpeedCal)) * EIRTempModFac * EIRAirFFModFac *
                      EIRWaterFFModFac;
                Winput2 = QLoadTotal2 * EIR;

                if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) {
                    QWasteHeat2 = 0.0;
                } else {
                    QWasteHeat2 = Winput2 * VarSpeedCoil.MSWasteHeatFrac(SpeedCal);
                    QWasteHeat2 *= CurveValue(state,
                                              VarSpeedCoil.MSWasteHeat(SpeedCal),
                                              state.dataVariableSpeedCoils->LoadSideInletWBTemp,
                                              state.dataVariableSpeedCoils->SourceSideInletTemp);
                }

                state.dataVariableSpeedCoils->Winput = Winput2 * SpeedRatio + (1.0 - SpeedRatio) * Winput1;
                QWasteHeat = QWasteHeat2 * SpeedRatio + (1.0 - SpeedRatio) * QWasteHeat1;
            }

            state.dataVariableSpeedCoils->QSensible = state.dataVariableSpeedCoils->QLoadTotal * SHR;

            state.dataVariableSpeedCoils->QSource = state.dataVariableSpeedCoils->QLoadTotal + state.dataVariableSpeedCoils->Winput - QWasteHeat;

            if (state.dataVariableSpeedCoils->QSource < 0) {
                state.dataVariableSpeedCoils->QSource = 0.0;
                QWasteHeat = state.dataVariableSpeedCoils->QLoadTotal + state.dataVariableSpeedCoils->Winput;
            }

            // Check if the Sensible Load is greater than the Total Cooling Load
            if (state.dataVariableSpeedCoils->QSensible > state.dataVariableSpeedCoils->QLoadTotal) {
                state.dataVariableSpeedCoils->QSensible = state.dataVariableSpeedCoils->QLoadTotal;
            }

            if (LatDegradModelSimFlag) {
                // Calculate for SHReff using the Latent Degradation Model
                if (NumIteration == 1) {
                    state.dataVariableSpeedCoils->QLatRated = state.dataVariableSpeedCoils->QLoadTotal - state.dataVariableSpeedCoils->QSensible;
                } else if (NumIteration == 2) {
                    state.dataVariableSpeedCoils->QLatActual = state.dataVariableSpeedCoils->QLoadTotal - state.dataVariableSpeedCoils->QSensible;
                    SHRss = state.dataVariableSpeedCoils->QSensible / state.dataVariableSpeedCoils->QLoadTotal;
                    SHReff = CalcEffectiveSHR(VarSpeedCoil,
                                              SHRss,
                                              CyclingScheme,
                                              RuntimeFrac,
                                              state.dataVariableSpeedCoils->QLatRated,
                                              state.dataVariableSpeedCoils->QLatActual,
                                              state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                              state.dataVariableSpeedCoils->LoadSideInletWBTemp);
                    //       Update sensible capacity based on effective SHR
                    state.dataVariableSpeedCoils->QSensible = state.dataVariableSpeedCoils->QLoadTotal * SHReff;
                    goto LOOP_exit;
                }
            } else {
                // Assume SHReff=SHRss
                SHReff = state.dataVariableSpeedCoils->QSensible / state.dataVariableSpeedCoils->QLoadTotal;
                goto LOOP_exit;
            }
        }
    LOOP_exit:;

        // considering hot gas reheat here
        if (VarSpeedCoil.HOTGASREHEATFLG > 0) {
            state.dataVariableSpeedCoils->QLoadTotal -= QWasteHeat;
            state.dataVariableSpeedCoils->QSensible -= QWasteHeat;
            SHReff = state.dataVariableSpeedCoils->QSensible / state.dataVariableSpeedCoils->QLoadTotal;
        }

        VarSpeedCoil.BasinHeaterPower = 0.0;
        VarSpeedCoil.CrankcaseHeaterPower = 0.0;

        if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) {
            if (VarSpeedCoil.CondenserType == DataHeatBalance::RefrigCondenserType::Evap) {
                //******************
                //             WATER CONSUMPTION IN m3 OF WATER FOR DIRECT
                //             H2O [m3/s] = Delta W[kgWater/kgDryAir]*Mass Flow Air[kgDryAir/s]
                //                                /RhoWater [kgWater/m3]
                //******************
                RhoEvapCondWater = RhoH2O(state.dataVariableSpeedCoils->OutdoorDryBulb_CalcVarSpeedCoilCooling);
                VarSpeedCoil.EvapWaterConsumpRate =
                    (CondInletHumRat - state.dataVariableSpeedCoils->OutdoorHumRat_CalcVarSpeedCoilCooling) * CondAirMassFlow / RhoEvapCondWater *
                    RuntimeFrac;
                VarSpeedCoil.EvapCondPumpElecPower =
                    VarSpeedCoil.EvapCondPumpElecNomPower * RuntimeFrac;
                // Calculate basin heater power
                CalcBasinHeaterPower(state,
                                     VarSpeedCoil.BasinHeaterPowerFTempDiff,
                                     VarSpeedCoil.BasinHeaterSchedulePtr,
                                     VarSpeedCoil.BasinHeaterSetPointTemp,
                                     VarSpeedCoil.BasinHeaterPower);
                VarSpeedCoil.BasinHeaterPower *= (1.0 - RuntimeFrac);
            }

            VarSpeedCoil.CrankcaseHeaterPower =
                state.dataVariableSpeedCoils->CrankcaseHeatingPower_CalcVarSpeedCoilCooling * (1.0 - RuntimeFrac);

            // set water system demand request (if needed)
            if (VarSpeedCoil.EvapWaterSupplyMode == WaterSupplyFromTank) {
                state.dataWaterData->WaterStorage(VarSpeedCoil.EvapWaterSupTankID)
                    .VdotRequestDemand(VarSpeedCoil.EvapWaterTankDemandARRID) =
                    VarSpeedCoil.EvapWaterConsumpRate;
            }
        }

        if ((PartLoadRatio > 0.0 && CyclingScheme == ContFanCycCoil) || (CyclingScheme == CycFanCycCoil)) {
            // calculate coil outlet state variables
            state.dataVariableSpeedCoils->LoadSideOutletEnth =
                state.dataVariableSpeedCoils->LoadSideInletEnth -
                state.dataVariableSpeedCoils->QLoadTotal / state.dataVariableSpeedCoils->LoadSideMassFlowRate;
            state.dataVariableSpeedCoils->LoadSideOutletDBTemp =
                state.dataVariableSpeedCoils->LoadSideInletDBTemp -
                state.dataVariableSpeedCoils->QSensible / (state.dataVariableSpeedCoils->LoadSideMassFlowRate * CpAir);

            MaxHumRat = PsyWFnTdbRhPb(state,
                                      state.dataVariableSpeedCoils->LoadSideOutletDBTemp,
                                      0.9999,
                                      VarSpeedCoil.InletAirPressure,
                                      RoutineName);
            MaxOutletEnth = PsyHFnTdbW(state.dataVariableSpeedCoils->LoadSideOutletDBTemp, MaxHumRat);
            if (state.dataVariableSpeedCoils->LoadSideOutletEnth > MaxOutletEnth) {
                state.dataVariableSpeedCoils->LoadSideOutletEnth = MaxOutletEnth;
                // QLoadTotal = LoadSideMassFlowRate * (LoadSideInletEnth - LoadSideOutletEnth)
            }
            state.dataVariableSpeedCoils->LoadSideOutletHumRat =
                PsyWFnTdbH(state, state.dataVariableSpeedCoils->LoadSideOutletDBTemp, state.dataVariableSpeedCoils->LoadSideOutletEnth, RoutineName);
            if (state.dataVariableSpeedCoils->LoadSideOutletHumRat > MaxHumRat) {
                state.dataVariableSpeedCoils->LoadSideOutletHumRat = MaxHumRat;
            }
        }

        // Actual outlet conditions are "average" for time step
        if (CyclingScheme == ContFanCycCoil) {
            // continuous fan, cycling compressor
            VarSpeedCoil.OutletAirEnthalpy =
                PartLoadRatio * state.dataVariableSpeedCoils->LoadSideOutletEnth +
                (1.0 - PartLoadRatio) * state.dataVariableSpeedCoils->LoadSideInletEnth;
            VarSpeedCoil.OutletAirHumRat =
                PartLoadRatio * state.dataVariableSpeedCoils->LoadSideOutletHumRat +
                (1.0 - PartLoadRatio) * state.dataVariableSpeedCoils->LoadSideInletHumRat;
            VarSpeedCoil.OutletAirDBTemp =
                PsyTdbFnHW(VarSpeedCoil.OutletAirEnthalpy,
                           VarSpeedCoil.OutletAirHumRat);
            state.dataVariableSpeedCoils->PLRCorrLoadSideMdot = state.dataVariableSpeedCoils->LoadSideMassFlowRate;
        } else {
            // default to cycling fan, cycling compressor
            VarSpeedCoil.OutletAirEnthalpy = state.dataVariableSpeedCoils->LoadSideOutletEnth;
            VarSpeedCoil.OutletAirHumRat = state.dataVariableSpeedCoils->LoadSideOutletHumRat;
            VarSpeedCoil.OutletAirDBTemp = state.dataVariableSpeedCoils->LoadSideOutletDBTemp;
            state.dataVariableSpeedCoils->PLRCorrLoadSideMdot = state.dataVariableSpeedCoils->LoadSideMassFlowRate * PartLoadRatio;
        }

        // scale heat transfer rates to PLR and power to RTF
        state.dataVariableSpeedCoils->QLoadTotal *= PartLoadRatio;
        state.dataVariableSpeedCoils->QSensible *= PartLoadRatio;
        // count the powr separately
        state.dataVariableSpeedCoils->Winput *= RuntimeFrac; //+ VarSpeedCoil(DXCoilNum)%CrankcaseHeaterPower &
        //+ VarSpeedCoil(DXCoilNum)%BasinHeaterPower + VarSpeedCoil(DXCoilNum)%EvapCondPumpElecPower
        state.dataVariableSpeedCoils->QSource *= PartLoadRatio;
        QWasteHeat *= PartLoadRatio;

        //  Add power to global variable so power can be summed by parent object
        state.dataHVACGlobal->DXElecCoolingPower = state.dataVariableSpeedCoils->Winput;

        ReportingConstant = TimeStepSys * DataGlobalConstants::SecInHour;
        // Update heat pump data structure
        VarSpeedCoil.Power = state.dataVariableSpeedCoils->Winput;
        VarSpeedCoil.QLoadTotal = state.dataVariableSpeedCoils->QLoadTotal;
        VarSpeedCoil.QSensible = state.dataVariableSpeedCoils->QSensible;
        VarSpeedCoil.QLatent =
            state.dataVariableSpeedCoils->QLoadTotal - state.dataVariableSpeedCoils->QSensible;
        VarSpeedCoil.QSource = state.dataVariableSpeedCoils->QSource;
        VarSpeedCoil.Energy = state.dataVariableSpeedCoils->Winput * ReportingConstant;
        VarSpeedCoil.EnergyLoadTotal = state.dataVariableSpeedCoils->QLoadTotal * ReportingConstant;
        VarSpeedCoil.EnergySensible = state.dataVariableSpeedCoils->QSensible * ReportingConstant;
        VarSpeedCoil.EnergyLatent =
            (state.dataVariableSpeedCoils->QLoadTotal - state.dataVariableSpeedCoils->QSensible) * ReportingConstant;
        VarSpeedCoil.EnergySource = state.dataVariableSpeedCoils->QSource * ReportingConstant;
        VarSpeedCoil.CrankcaseHeaterConsumption =
            VarSpeedCoil.CrankcaseHeaterPower * ReportingConstant;
        VarSpeedCoil.EvapWaterConsump =
            VarSpeedCoil.EvapWaterConsumpRate * ReportingConstant;
        VarSpeedCoil.BasinHeaterConsumption =
            VarSpeedCoil.BasinHeaterPower * ReportingConstant;
        VarSpeedCoil.EvapCondPumpElecConsumption =
            VarSpeedCoil.EvapCondPumpElecPower * ReportingConstant;
        if (RuntimeFrac == 0.0) {
            VarSpeedCoil.COP = 0.0;
        } else {
            VarSpeedCoil.COP =
                state.dataVariableSpeedCoils->QLoadTotal / state.dataVariableSpeedCoils->Winput;
        }
        VarSpeedCoil.RunFrac = RuntimeFrac;
        VarSpeedCoil.PartLoadRatio = PartLoadRatio;
        VarSpeedCoil.AirMassFlowRate = state.dataVariableSpeedCoils->PLRCorrLoadSideMdot;
        rhoair = PsyRhoAirFnPbTdbW(state,
                                   state.dataEnvrn->OutBaroPress,
                                   state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                   state.dataVariableSpeedCoils->LoadSideInletHumRat,
                                   RoutineName);
        VarSpeedCoil.AirVolFlowRate =
            VarSpeedCoil.AirMassFlowRate / rhoair;

        if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) {
            VarSpeedCoil.WaterMassFlowRate = 0.0;
            VarSpeedCoil.OutletWaterTemp = 0.0;
            VarSpeedCoil.OutletWaterEnthalpy = 0.0;
            state.dataHeatBal->HeatReclaimVS_DXCoil(DXCoilNum).AvailCapacity = state.dataVariableSpeedCoils->QSource;
        } else {
            VarSpeedCoil.WaterMassFlowRate = state.dataVariableSpeedCoils->SourceSideMassFlowRate;
            VarSpeedCoil.OutletWaterTemp =
                state.dataVariableSpeedCoils->SourceSideInletTemp +
                state.dataVariableSpeedCoils->QSource / (state.dataVariableSpeedCoils->SourceSideMassFlowRate * CpSource);
            VarSpeedCoil.OutletWaterEnthalpy =
                state.dataVariableSpeedCoils->SourceSideInletEnth +
                state.dataVariableSpeedCoils->QSource / state.dataVariableSpeedCoils->SourceSideMassFlowRate;
        }

        VarSpeedCoil.QWasteHeat = QWasteHeat;

        if (VarSpeedCoil.CondensateCollectMode == CondensateToTank) {
            // calculate and report condensation rates  (how much water extracted from the air stream)
            // water flow of water in m3/s for water system interactions
            RhoWater = RhoH2O((VarSpeedCoil.InletAirDBTemp +
                               VarSpeedCoil.OutletAirDBTemp) /
                              2.0);
            //     CR9155 Remove specific humidity calculations
            SpecHumIn = state.dataVariableSpeedCoils->LoadSideInletHumRat;
            SpecHumOut = state.dataVariableSpeedCoils->LoadSideOutletHumRat;
            //  mdot * del HumRat / rho water
            VarSpeedCoil.CondensateVdot =
                max(0.0, (state.dataVariableSpeedCoils->LoadSideMassFlowRate * (SpecHumIn - SpecHumOut) / RhoWater));
            VarSpeedCoil.CondensateVol =
                VarSpeedCoil.CondensateVdot * ReportingConstant;
        }
    }

    void CalcVarSpeedHPWH(EnergyPlusData &state,
                          VariableSpeedCoilData &VarSpeedCoil,        // the number of the DX coil to be simulated
                          Real64 &RuntimeFrac,        // Runtime Fraction of compressor or percent on time (on-time/cycle time)
                          Real64 const PartLoadRatio, // sensible water heating load / full load sensible water heating capacity
                          Real64 const SpeedRatio,    // SpeedRatio varies between 1.0 (higher speed) and 0.0 (lower speed)
                          int const SpeedNum,         // Speed number, high bound
                          int const CyclingScheme     // Continuous fan OR cycling compressor
    )
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Bo Shen, ORNL
        //       DATE WRITTEN   12/2014

        // PURPOSE OF THIS SUBROUTINE:
        // Calculates the gross cooling capacity of a variable-speed heat pump water heater evaporator and
        // heating capacity of the condenser coil given the rated heating capacity and COP.

        // METHODOLOGY EMPLOYED:
        // The routine requires the user to enter the total heating capacity and COP for the
        // heat pump water heater along with logicals defining if fan and condenser pump are included at numerous speed levels.
        // Since manufacturer's can rate their HPWH equipment with or without including condenser
        // pump heat, this information is required to accurately determine the condenser's leaving
        // water temperature. In addition, knowledge of the fan heat is required to back into
        // a compressor COP.

        // Using/Aliasing
        using CurveManager::CurveValue;

        // SUBROUTINE PARAMETER DEFINITIONS:
        static constexpr std::string_view RoutineName("CalcVarSpeedHPWH");

        // INTERFACE BLOCK SPECIFICATIONS
        // na

        // DERIVED TYPE DEFINITIONS
        // na

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        Real64 OperatingHeatingCapacity; // Water heating operating capacity including the impact of capacity and COP curves (W)
        Real64 OperatingHeatingCOP;      // Water heating operating COP including the impact of capacity and COP curves (W/W)
        Real64 OperatingHeatingPower;    // Water heating operating Power (W)
        Real64 CompressorPower;          // Power consumed by compressor only (W)

        Real64 TotalTankHeatingCapacity; // Water heating capacity corrected for condenser water pump heat (W)
        Real64 TankHeatingCOP;           // Water heating COP corrected for fan and condenser water pump power (W/W)
        // (these previous 2 variables also include the impact of capacity and COP curves)
        Real64 EvapCoolingCapacity;   // Air cooling capacity corrected for evap fan and cond water pump heat (W)
        Real64 InletWaterTemp;        // Condenser water inlet temperature (C)
        Real64 OutletWaterTemp;       // Condenser water outlet temperature (C)
        Real64 EvapInletMassFlowRate; // Evaporator air inlet mass flow rate (m3/s)
        Real64 CondInletMassFlowRate; // Condenser water inlet mass flow rate (m3/s)
        Real64 CpWater;               // Specific heat of condenser inlet water (J/Kg/k)
        Real64 InletAirTemp;          // HPWH inlet air temperature (dry-bulb or wet-bulb) (C)
        Real64 AirMassFlowRatio;      // Ratio of evaporator inlet air mass flow rate to rated mass flow rate
        Real64 WaterMassFlowRatio;    // Ratio of evaporator inlet water mass flow rate to rated mass flow rate
        Real64 PumpHeatToWater;       // Amount of pump heat attributed to heating water
        Real64 HPRTF;                 // Heat pump run time fraction
        Real64 PLF;                   // part-load function
        Real64 CBFSpeed;              // bypass factor as individual speed level
        Real64 COPAirFFModFac;        // air flow fraction modification
        Real64 COPWaterFFModFac;      // water flow fraction modification
        Real64 COPTempModFac;         // total capacity temperature correctio fraction
        Real64 TOTCAPAirFFModFac;     // air flow fraction modification
        Real64 TOTCAPWaterFFModFac;   // water flow fraction modification
        Real64 TOTCAPTempModFac;      // total capacity temperature correctio fraction
        Real64 SHR;                   // total capacity temperature correctio fraction
        Real64 COP;                   // total capacity temperature correctio fraction
        Real64 AoEff;                 // effective air side surface area
        Real64 Winput1;               // power consumption at low speed
        Real64 Winput2;               // power consumption at high speed
        Real64 LoadPressure;          // evaporator inlet pressure
        Real64 CrankcaseHeatingPower; // power due to crankcase heater
        Real64 hDelta;                // Change in air enthalpy across the cooling coil [J/kg]
        Real64 hADP;                  // Apparatus dew point enthalpy [J/kg]
        Real64 tADP;                  // Apparatus dew point temperature [C]
        Real64 wADP;                  // Apparatus dew point humidity ratio [kg/kg]
        Real64 hTinwADP;              // Enthalpy at inlet dry-bulb and wADP [J/kg]
        Real64 WHCAP1;                // total heating capacity at low speed [W]
        Real64 WHCAP2;                // total heating capacity at high speed [W]
        Real64 CpAir;                 // Specific heat of air [J/kg_C]
        Real64 MaxHumRat;             // max possible humidity
        Real64 MaxOutletEnth;         // max possible outlet enthalpy
        Real64 ReportingConstant;
        int EvapInletNode;    // Evaporator air inlet node number
        int EvapOutletNode;   // Evaporator air outlet node number
        int CondInletNode;    // Condenser water inlet node number
        int CondOutletNode;   // Condenser water outlet node number
        int MaxSpeed;         // maximum speed level
        int SpeedCal;         // calculated speed level
        Real64 rhoair(0.0);   // entering air density
        Real64 RhoWater(0.0); // water density

        // note: load side is the evaporator side, and source side is the condenser side

        CondInletNode = VarSpeedCoil.WaterInletNodeNum;
        CondOutletNode = VarSpeedCoil.WaterOutletNodeNum;
        // If heat pump water heater is OFF, set outlet to inlet and RETURN
        if (PartLoadRatio == 0.0) {
            state.dataLoopNodes->Node(CondOutletNode) = state.dataLoopNodes->Node(CondInletNode);
            return;
        } else {
            EvapInletNode = VarSpeedCoil.AirInletNodeNum;
            EvapOutletNode = VarSpeedCoil.AirOutletNodeNum;
            InletWaterTemp = state.dataLoopNodes->Node(CondInletNode).Temp;
            CondInletMassFlowRate = state.dataLoopNodes->Node(CondInletNode).MassFlowRate;
            EvapInletMassFlowRate = state.dataLoopNodes->Node(EvapInletNode).MassFlowRate;
            CpWater = CPHW(InletWaterTemp);
            CompressorPower = 0.0;
            OperatingHeatingPower = 0.0;
            TankHeatingCOP = 0.0;
        }

        //  LOAD LOCAL VARIABLES FROM DATA STRUCTURE (for code readability)
        if (!(CyclingScheme == ContFanCycCoil) && PartLoadRatio > 0.0) {
            CondInletMassFlowRate = CondInletMassFlowRate / PartLoadRatio;
            EvapInletMassFlowRate = EvapInletMassFlowRate / PartLoadRatio;
        }

        VarSpeedCoil.AirMassFlowRate = EvapInletMassFlowRate;
        VarSpeedCoil.WaterMassFlowRate = CondInletMassFlowRate;

        // determine inlet air temperature type for curve objects
        if (VarSpeedCoil.InletAirTemperatureType == WetBulbIndicator) {
            InletAirTemp = state.dataHVACGlobal->HPWHInletWBTemp;
        } else {
            InletAirTemp = state.dataHVACGlobal->HPWHInletDBTemp;
        }

        // check if indoor evaporator or outdoor evaporator
        CrankcaseHeatingPower = 0.0;
        if (EvapInletNode != 0) {
            state.dataVariableSpeedCoils->LoadSideInletDBTemp = state.dataLoopNodes->Node(EvapInletNode).Temp;
            state.dataVariableSpeedCoils->LoadSideInletHumRat = state.dataLoopNodes->Node(EvapInletNode).HumRat;
            LoadPressure = state.dataLoopNodes->Node(EvapInletNode).Press;
            // prevent the air pressure not given
            if (LoadPressure < 10.0) LoadPressure = state.dataEnvrn->OutBaroPress;

            state.dataVariableSpeedCoils->LoadSideInletWBTemp = state.dataLoopNodes->Node(EvapInletNode).OutAirWetBulb;
            state.dataVariableSpeedCoils->LoadSideInletEnth = state.dataLoopNodes->Node(EvapInletNode).Enthalpy;
        } else {
            state.dataVariableSpeedCoils->LoadSideInletDBTemp = state.dataEnvrn->OutDryBulbTemp;
            state.dataVariableSpeedCoils->LoadSideInletHumRat = state.dataEnvrn->OutHumRat;
            LoadPressure = state.dataEnvrn->OutBaroPress;
            state.dataVariableSpeedCoils->LoadSideInletWBTemp = state.dataEnvrn->OutWetBulbTemp;
            state.dataVariableSpeedCoils->LoadSideInletEnth = state.dataEnvrn->OutEnthalpy;

            // Initialize crankcase heater, operates below OAT defined in input deck for HP DX heating coil
            if (state.dataEnvrn->OutDryBulbTemp < VarSpeedCoil.MaxOATCrankcaseHeater) {
                CrankcaseHeatingPower = VarSpeedCoil.CrankcaseHeaterCapacity;
            };
        }

        state.dataVariableSpeedCoils->LoadSideMassFlowRate = EvapInletMassFlowRate;
        state.dataVariableSpeedCoils->SourceSideMassFlowRate = CondInletMassFlowRate;
        state.dataVariableSpeedCoils->SourceSideInletTemp = InletWaterTemp;
        state.dataVariableSpeedCoils->SourceSideInletEnth = state.dataLoopNodes->Node(CondInletNode).Enthalpy;
        VarSpeedCoil.InletWaterEnthalpy = state.dataVariableSpeedCoils->SourceSideInletEnth;

        // Check for flows, do not perform simulation if no flow in load side or source side.
        if ((state.dataVariableSpeedCoils->SourceSideMassFlowRate <= 0.0) || (state.dataVariableSpeedCoils->LoadSideMassFlowRate <= 0.0)) {
            VarSpeedCoil.SimFlag = false;
            return;
        } else {
            VarSpeedCoil.SimFlag = true;
        }

        MaxSpeed = VarSpeedCoil.NumOfSpeeds;

        // must be placed inside the loop, otherwise cause bug in release mode, need to be present at two places
        if (SpeedNum > MaxSpeed) {
            SpeedCal = MaxSpeed;
        } else {
            SpeedCal = SpeedNum;
        }

        // part-load calculation
        RuntimeFrac = 1.0;
        state.dataHVACGlobal->OnOffFanPartLoadFraction = 1.0;
        VarSpeedCoil.RunFrac = 1.0;
        if ((SpeedNum == 1) && (PartLoadRatio < 1.0)) {
            PLF = CurveValue(state, VarSpeedCoil.PLFFPLR, PartLoadRatio);
            if (PLF < 0.7) {
                PLF = 0.7;
            }
            if (CyclingScheme == CycFanCycCoil)
                state.dataHVACGlobal->OnOffFanPartLoadFraction =
                    PLF; // save PLF for fan model, don't change fan power for constant fan mode if coil is off
            // calculate the run time fraction
            VarSpeedCoil.RunFrac = PartLoadRatio / PLF;
            VarSpeedCoil.PartLoadRatio = PartLoadRatio;

            if (VarSpeedCoil.RunFrac > 1.0) {
                VarSpeedCoil.RunFrac = 1.0; // Reset coil runtime fraction to 1.0
            } else if (VarSpeedCoil.RunFrac < 0.0) {
                VarSpeedCoil.RunFrac = 0.0;
            }

            RuntimeFrac = VarSpeedCoil.RunFrac;
        }

        // interpolate between speeds
        // must be placed inside the loop, otherwise cause bug in release mode
        if (SpeedNum > MaxSpeed) {
            SpeedCal = MaxSpeed;
        } else {
            SpeedCal = SpeedNum;
        }

        Real64 locFanElecPower = 0.0; // local for fan electric power
        if (VarSpeedCoil.SupplyFan_TypeNum == DataHVACGlobals::FanType_SystemModelObject) {
            if (VarSpeedCoil.SupplyFanIndex > -1) {
                locFanElecPower = state.dataHVACFan->fanObjs[VarSpeedCoil.SupplyFanIndex]->fanPower();
            }
        } else {
            if (VarSpeedCoil.SupplyFanIndex > 0) {
                locFanElecPower = Fans::GetFanPower(state, VarSpeedCoil.SupplyFanIndex);
            }
        }

        if ((SpeedNum == 1) || (SpeedNum > MaxSpeed) || (SpeedRatio == 1.0)) {
            AirMassFlowRatio =
                state.dataVariableSpeedCoils->LoadSideMassFlowRate / VarSpeedCoil.DesignAirMassFlowRate;
            WaterMassFlowRatio =
                state.dataVariableSpeedCoils->SourceSideMassFlowRate / VarSpeedCoil.DesignWaterMassFlowRate;
            VarSpeedCoil.HPWHCondPumpElecNomPower =
                VarSpeedCoil.MSWHPumpPower(SpeedCal);

            COPTempModFac = CurveValue(state,
                                       VarSpeedCoil.MSEIRFTemp(SpeedCal),
                                       InletAirTemp,
                                       state.dataVariableSpeedCoils->SourceSideInletTemp);
            COPAirFFModFac = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(SpeedCal), AirMassFlowRatio);
            COPWaterFFModFac = CurveValue(state, VarSpeedCoil.MSEIRWaterFFlow(SpeedCal), WaterMassFlowRatio);

            COP = VarSpeedCoil.MSRatedCOP(SpeedCal) * COPTempModFac * COPAirFFModFac * COPWaterFFModFac;

            TOTCAPTempModFac = CurveValue(state,
                                          VarSpeedCoil.MSCCapFTemp(SpeedCal),
                                          InletAirTemp,
                                          state.dataVariableSpeedCoils->SourceSideInletTemp);
            //   Get capacity modifying factor (function of mass flow) for off-rated conditions
            TOTCAPAirFFModFac = CurveValue(state, VarSpeedCoil.MSCCapAirFFlow(SpeedCal), AirMassFlowRatio);
            // Get capacity modifying factor (function of mass flow) for off-rated conditions
            TOTCAPWaterFFModFac =
                CurveValue(state, VarSpeedCoil.MSCCapWaterFFlow(SpeedCal), WaterMassFlowRatio);

            OperatingHeatingCapacity = VarSpeedCoil.MSRatedTotCap(SpeedCal) * TOTCAPTempModFac *
                                       TOTCAPAirFFModFac * TOTCAPWaterFFModFac;

            state.dataVariableSpeedCoils->Winput = OperatingHeatingCapacity / COP;
            OperatingHeatingPower = state.dataVariableSpeedCoils->Winput;

            OperatingHeatingCOP = COP;
            PumpHeatToWater = VarSpeedCoil.HPWHCondPumpElecNomPower *
                              VarSpeedCoil.HPWHCondPumpFracToWater;
            TankHeatingCOP = OperatingHeatingCOP;

            // account for pump heat if not included in total water heating capacity
            if (VarSpeedCoil.CondPumpHeatInCapacity) {
                TotalTankHeatingCapacity = OperatingHeatingCapacity;
            } else {
                TotalTankHeatingCapacity = OperatingHeatingCapacity + PumpHeatToWater;
            }

            HPRTF = RuntimeFrac;
            // calculate evaporator total cooling capacity
            if (VarSpeedCoil.FanPowerIncludedInCOP) {
                if (VarSpeedCoil.CondPumpPowerInCOP) {
                    //       make sure fan power is full load fan power, it isn't though,
                    CompressorPower = OperatingHeatingPower - locFanElecPower / HPRTF -
                                      VarSpeedCoil.HPWHCondPumpElecNomPower;
                    if (OperatingHeatingPower > 0.0) TankHeatingCOP = TotalTankHeatingCapacity / OperatingHeatingPower;
                } else {
                    CompressorPower = OperatingHeatingPower - locFanElecPower / HPRTF;
                    if ((OperatingHeatingPower + VarSpeedCoil.HPWHCondPumpElecNomPower) > 0.0)
                        TankHeatingCOP = TotalTankHeatingCapacity /
                                         (OperatingHeatingPower + VarSpeedCoil.HPWHCondPumpElecNomPower);
                }
            } else {
                if (VarSpeedCoil.CondPumpPowerInCOP) {
                    //       make sure fan power is full load fan power
                    CompressorPower = OperatingHeatingPower - VarSpeedCoil.HPWHCondPumpElecNomPower;
                    if ((OperatingHeatingPower + locFanElecPower / HPRTF) > 0.0)
                        TankHeatingCOP = TotalTankHeatingCapacity / (OperatingHeatingPower + locFanElecPower / HPRTF);
                } else {
                    CompressorPower = OperatingHeatingPower;
                    if ((OperatingHeatingPower + locFanElecPower / HPRTF +
                         VarSpeedCoil.HPWHCondPumpElecNomPower) > 0.0)
                        TankHeatingCOP = TotalTankHeatingCapacity / (OperatingHeatingPower + locFanElecPower / HPRTF +
                                                                     VarSpeedCoil.HPWHCondPumpElecNomPower);
                }
            }

            if (VarSpeedCoil.CondPumpHeatInCapacity) {
                EvapCoolingCapacity = TotalTankHeatingCapacity - PumpHeatToWater - CompressorPower;
            } else {
                EvapCoolingCapacity = TotalTankHeatingCapacity - CompressorPower;
            }

            CBFSpeed = AdjustCBF(VarSpeedCoil.MSRatedCBF(SpeedCal),
                                 VarSpeedCoil.MSRatedAirMassFlowRate(SpeedCal),
                                 state.dataVariableSpeedCoils->LoadSideMassFlowRate);

        } else {
            AirMassFlowRatio =
                state.dataVariableSpeedCoils->LoadSideMassFlowRate / VarSpeedCoil.DesignAirMassFlowRate;
            WaterMassFlowRatio =
                state.dataVariableSpeedCoils->SourceSideMassFlowRate / VarSpeedCoil.DesignWaterMassFlowRate;
            AoEff = VarSpeedCoil.MSEffectiveAo(SpeedCal) * SpeedRatio +
                    (1.0 - SpeedRatio) * VarSpeedCoil.MSEffectiveAo(SpeedCal - 1);
            CBFSpeed = std::exp(-AoEff / state.dataVariableSpeedCoils->LoadSideMassFlowRate);

            // calculate low speed
            SpeedCal = SpeedNum - 1;

            VarSpeedCoil.HPWHCondPumpElecNomPower =
                VarSpeedCoil.MSWHPumpPower(SpeedCal);
            COPTempModFac = CurveValue(state,
                                       VarSpeedCoil.MSEIRFTemp(SpeedCal),
                                       InletAirTemp,
                                       state.dataVariableSpeedCoils->SourceSideInletTemp);
            COPAirFFModFac = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(SpeedCal), AirMassFlowRatio);
            COPWaterFFModFac = CurveValue(state, VarSpeedCoil.MSEIRWaterFFlow(SpeedCal), WaterMassFlowRatio);

            COP = VarSpeedCoil.MSRatedCOP(SpeedCal) * COPTempModFac * COPAirFFModFac * COPWaterFFModFac;

            TOTCAPTempModFac = CurveValue(state,
                                          VarSpeedCoil.MSCCapFTemp(SpeedCal),
                                          InletAirTemp,
                                          state.dataVariableSpeedCoils->SourceSideInletTemp);
            //   Get capacity modifying factor (function of mass flow) for off-rated conditions
            TOTCAPAirFFModFac = CurveValue(state, VarSpeedCoil.MSCCapAirFFlow(SpeedCal), AirMassFlowRatio);
            // Get capacity modifying factor (function of mass flow) for off-rated conditions
            TOTCAPWaterFFModFac =
                CurveValue(state, VarSpeedCoil.MSCCapWaterFFlow(SpeedCal), WaterMassFlowRatio);

            OperatingHeatingCapacity = VarSpeedCoil.MSRatedTotCap(SpeedCal) * TOTCAPTempModFac *
                                       TOTCAPAirFFModFac * TOTCAPWaterFFModFac;

            state.dataVariableSpeedCoils->Winput = OperatingHeatingCapacity / COP;
            OperatingHeatingPower = state.dataVariableSpeedCoils->Winput;
            Winput1 = state.dataVariableSpeedCoils->Winput;
            WHCAP1 = OperatingHeatingCapacity;

            // calculate upper speed
            SpeedCal = SpeedNum;

            VarSpeedCoil.HPWHCondPumpElecNomPower =
                VarSpeedCoil.MSWHPumpPower(SpeedCal);
            COPTempModFac = CurveValue(state,
                                       VarSpeedCoil.MSEIRFTemp(SpeedCal),
                                       InletAirTemp,
                                       state.dataVariableSpeedCoils->SourceSideInletTemp);
            COPAirFFModFac = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(SpeedCal), AirMassFlowRatio);
            COPWaterFFModFac = CurveValue(state, VarSpeedCoil.MSEIRWaterFFlow(SpeedCal), WaterMassFlowRatio);

            COP = VarSpeedCoil.MSRatedCOP(SpeedCal) * COPTempModFac * COPAirFFModFac * COPWaterFFModFac;

            TOTCAPTempModFac = CurveValue(state,
                                          VarSpeedCoil.MSCCapFTemp(SpeedCal),
                                          InletAirTemp,
                                          state.dataVariableSpeedCoils->SourceSideInletTemp);
            //   Get capacity modifying factor (function of mass flow) for off-rated conditions
            TOTCAPAirFFModFac = CurveValue(state, VarSpeedCoil.MSCCapAirFFlow(SpeedCal), AirMassFlowRatio);
            // Get capacity modifying factor (function of mass flow) for off-rated conditions
            TOTCAPWaterFFModFac =
                CurveValue(state, VarSpeedCoil.MSCCapWaterFFlow(SpeedCal), WaterMassFlowRatio);

            OperatingHeatingCapacity = VarSpeedCoil.MSRatedTotCap(SpeedCal) * TOTCAPTempModFac *
                                       TOTCAPAirFFModFac * TOTCAPWaterFFModFac;

            state.dataVariableSpeedCoils->Winput = OperatingHeatingCapacity / COP;
            OperatingHeatingPower = state.dataVariableSpeedCoils->Winput;

            Winput2 = state.dataVariableSpeedCoils->Winput;
            WHCAP2 = OperatingHeatingCapacity;

            // interpolation
            state.dataVariableSpeedCoils->Winput = Winput2 * SpeedRatio + (1.0 - SpeedRatio) * Winput1;
            OperatingHeatingPower = state.dataVariableSpeedCoils->Winput;
            OperatingHeatingCapacity = WHCAP2 * SpeedRatio + (1.0 - SpeedRatio) * WHCAP1;
            VarSpeedCoil.HPWHCondPumpElecNomPower =
                VarSpeedCoil.MSWHPumpPower(SpeedNum) * SpeedRatio +
                (1.0 - SpeedRatio) * VarSpeedCoil.MSWHPumpPower(SpeedNum - 1);

            OperatingHeatingCOP = OperatingHeatingCapacity / OperatingHeatingPower;
            TankHeatingCOP = OperatingHeatingCOP;

            PumpHeatToWater = VarSpeedCoil.HPWHCondPumpElecNomPower *
                              VarSpeedCoil.HPWHCondPumpFracToWater;

            // account for pump heat if not included in total water heating capacity
            if (VarSpeedCoil.CondPumpHeatInCapacity) {
                TotalTankHeatingCapacity = OperatingHeatingCapacity;
            } else {
                TotalTankHeatingCapacity = OperatingHeatingCapacity + PumpHeatToWater;
            }

            HPRTF = RuntimeFrac;
            // calculate evaporator total cooling capacity
            if (VarSpeedCoil.FanPowerIncludedInCOP) {
                if (VarSpeedCoil.CondPumpPowerInCOP) {
                    //       make sure fan power is full load fan power
                    CompressorPower = OperatingHeatingPower - locFanElecPower / HPRTF -
                                      VarSpeedCoil.HPWHCondPumpElecNomPower;
                    if (OperatingHeatingPower > 0.0) TankHeatingCOP = TotalTankHeatingCapacity / OperatingHeatingPower;
                } else {
                    CompressorPower = OperatingHeatingPower - locFanElecPower / HPRTF;
                    if ((OperatingHeatingPower + VarSpeedCoil.HPWHCondPumpElecNomPower) > 0.0)
                        TankHeatingCOP = TotalTankHeatingCapacity /
                                         (OperatingHeatingPower + VarSpeedCoil.HPWHCondPumpElecNomPower);
                }
            } else {
                if (VarSpeedCoil.CondPumpPowerInCOP) {
                    //       make sure fan power is full load fan power
                    CompressorPower = OperatingHeatingPower - VarSpeedCoil.HPWHCondPumpElecNomPower;
                    if ((OperatingHeatingPower + locFanElecPower / HPRTF) > 0.0)
                        TankHeatingCOP = TotalTankHeatingCapacity / (OperatingHeatingPower + locFanElecPower / HPRTF);
                } else {
                    CompressorPower = OperatingHeatingPower;
                    if ((OperatingHeatingPower + locFanElecPower / HPRTF +
                         VarSpeedCoil.HPWHCondPumpElecNomPower) > 0.0)
                        TankHeatingCOP = TotalTankHeatingCapacity / (OperatingHeatingPower + locFanElecPower / HPRTF +
                                                                     VarSpeedCoil.HPWHCondPumpElecNomPower);
                }
            }

            if (VarSpeedCoil.CondPumpHeatInCapacity) {
                EvapCoolingCapacity = TotalTankHeatingCapacity - PumpHeatToWater - CompressorPower;
            } else {
                EvapCoolingCapacity = TotalTankHeatingCapacity - CompressorPower;
            }
        }

        state.dataVariableSpeedCoils->QSource = TotalTankHeatingCapacity;
        state.dataVariableSpeedCoils->QLoadTotal = EvapCoolingCapacity;
        state.dataHVACGlobal->DXCoilTotalCapacity = EvapCoolingCapacity; // for standard rating calculation
        SHR = 1.0;
        // if indoor, calculate SHR
        if (EvapInletNode != 0) {
            if (CBFSpeed > 0.999) CBFSpeed = 0.999;

            if (CBFSpeed < 0.001) {
                SHR = 1.0;
            } else {
                hDelta = state.dataVariableSpeedCoils->QLoadTotal / state.dataVariableSpeedCoils->LoadSideMassFlowRate;
                hADP = state.dataVariableSpeedCoils->LoadSideInletEnth - hDelta / (1.0 - CBFSpeed);
                tADP = PsyTsatFnHPb(state, hADP, LoadPressure, RoutineName);
                wADP = PsyWFnTdbH(state, tADP, hADP, RoutineName);
                hTinwADP = PsyHFnTdbW(state.dataVariableSpeedCoils->LoadSideInletDBTemp, wADP);
                if ((state.dataVariableSpeedCoils->LoadSideInletEnth - hADP) > 1.e-10) {
                    SHR = min((hTinwADP - hADP) / (state.dataVariableSpeedCoils->LoadSideInletEnth - hADP), 1.0);
                } else {
                    SHR = 1.0;
                }
            }
        }

        state.dataVariableSpeedCoils->QSensible = state.dataVariableSpeedCoils->QLoadTotal * SHR;

        // determine condenser water inlet/outlet condition at full capacity
        if (CondInletMassFlowRate == 0.0) {
            OutletWaterTemp = InletWaterTemp;
        } else {
            OutletWaterTemp = InletWaterTemp + TotalTankHeatingCapacity / (CpWater * CondInletMassFlowRate);
        }

        state.dataLoopNodes->Node(CondOutletNode).Temp = OutletWaterTemp;

        state.dataLoopNodes->Node(CondOutletNode).MassFlowRate = state.dataLoopNodes->Node(CondInletNode).MassFlowRate;

        // send heating capacity and COP to water heater module for standards rating calculation
        // total heating capacity including condenser pump
        state.dataVariableSpeedCoils->VSHPWHHeatingCapacity = TotalTankHeatingCapacity;
        // total heating COP including compressor, fan, and condenser pump
        state.dataVariableSpeedCoils->VSHPWHHeatingCOP = TankHeatingCOP;

        VarSpeedCoil.TotalHeatingEnergyRate = TotalTankHeatingCapacity * PartLoadRatio;
        // calculate total compressor plus condenser pump power, fan power reported in fan module
        VarSpeedCoil.ElecWaterHeatingPower =
            (CompressorPower + VarSpeedCoil.HPWHCondPumpElecNomPower) * HPRTF;

        // pass the outputs for the cooling coil section
        VarSpeedCoil.BasinHeaterPower = 0.0;
        VarSpeedCoil.CrankcaseHeaterPower = CrankcaseHeatingPower * (1.0 - RuntimeFrac);

        // calculate coil outlet state variables
        state.dataVariableSpeedCoils->LoadSideOutletEnth =
            state.dataVariableSpeedCoils->LoadSideInletEnth -
            state.dataVariableSpeedCoils->QLoadTotal / state.dataVariableSpeedCoils->LoadSideMassFlowRate;
        CpAir = PsyCpAirFnW(state.dataVariableSpeedCoils->LoadSideInletHumRat);
        state.dataVariableSpeedCoils->LoadSideOutletDBTemp =
            state.dataVariableSpeedCoils->LoadSideInletDBTemp -
            state.dataVariableSpeedCoils->QSensible / (state.dataVariableSpeedCoils->LoadSideMassFlowRate * CpAir);

        MaxHumRat = PsyWFnTdbRhPb(state,
                                  state.dataVariableSpeedCoils->LoadSideOutletDBTemp,
                                  0.9999,
                                  VarSpeedCoil.InletAirPressure,
                                  RoutineName);
        MaxOutletEnth = PsyHFnTdbW(state.dataVariableSpeedCoils->LoadSideOutletDBTemp, MaxHumRat);
        if (state.dataVariableSpeedCoils->LoadSideOutletEnth > MaxOutletEnth) {
            state.dataVariableSpeedCoils->LoadSideOutletEnth = MaxOutletEnth;
        }
        state.dataVariableSpeedCoils->LoadSideOutletHumRat =
            PsyWFnTdbH(state, state.dataVariableSpeedCoils->LoadSideOutletDBTemp, state.dataVariableSpeedCoils->LoadSideOutletEnth, RoutineName);
        if (state.dataVariableSpeedCoils->LoadSideOutletHumRat > MaxHumRat) {
            state.dataVariableSpeedCoils->LoadSideOutletHumRat = MaxHumRat;
        }

        // Actual outlet conditions are "average" for time step
        if (CyclingScheme == ContFanCycCoil) {
            // continuous fan, cycling compressor
            VarSpeedCoil.OutletAirEnthalpy =
                PartLoadRatio * state.dataVariableSpeedCoils->LoadSideOutletEnth +
                (1.0 - PartLoadRatio) * state.dataVariableSpeedCoils->LoadSideInletEnth;
            VarSpeedCoil.OutletAirHumRat =
                PartLoadRatio * state.dataVariableSpeedCoils->LoadSideOutletHumRat +
                (1.0 - PartLoadRatio) * state.dataVariableSpeedCoils->LoadSideInletHumRat;
            VarSpeedCoil.OutletAirDBTemp =
                PsyTdbFnHW(VarSpeedCoil.OutletAirEnthalpy,
                           VarSpeedCoil.OutletAirHumRat);
            state.dataVariableSpeedCoils->PLRCorrLoadSideMdot = state.dataVariableSpeedCoils->LoadSideMassFlowRate;
        } else {
            VarSpeedCoil.OutletAirEnthalpy = state.dataVariableSpeedCoils->LoadSideOutletEnth;
            VarSpeedCoil.OutletAirHumRat = state.dataVariableSpeedCoils->LoadSideOutletHumRat;
            VarSpeedCoil.OutletAirDBTemp = state.dataVariableSpeedCoils->LoadSideOutletDBTemp;
            state.dataVariableSpeedCoils->PLRCorrLoadSideMdot = state.dataVariableSpeedCoils->LoadSideMassFlowRate * PartLoadRatio;
        }

        // scale heat transfer rates to PLR and power to RTF
        state.dataVariableSpeedCoils->QLoadTotal *= PartLoadRatio;
        state.dataVariableSpeedCoils->QSensible *= PartLoadRatio;
        // count the powr separately
        state.dataVariableSpeedCoils->Winput *= RuntimeFrac; //+ VarSpeedCoil(DXCoilNum)%CrankcaseHeaterPower &
        //+ VarSpeedCoil(DXCoilNum)%BasinHeaterPower + VarSpeedCoil(DXCoilNum)%EvapCondPumpElecPower
        state.dataVariableSpeedCoils->QSource *= PartLoadRatio;

        //  Add power to global variable so power can be summed by parent object
        state.dataHVACGlobal->DXElecCoolingPower = state.dataVariableSpeedCoils->Winput;

        ReportingConstant = state.dataHVACGlobal->TimeStepSys * DataGlobalConstants::SecInHour;
        // Update heat pump data structure
        VarSpeedCoil.HPWHCondPumpElecNomPower =
            VarSpeedCoil.HPWHCondPumpElecNomPower * RuntimeFrac; // water heating pump power
        VarSpeedCoil.Power = state.dataVariableSpeedCoils->Winput;
        VarSpeedCoil.QLoadTotal = state.dataVariableSpeedCoils->QLoadTotal;
        VarSpeedCoil.QSensible = state.dataVariableSpeedCoils->QSensible;
        VarSpeedCoil.QLatent =
            state.dataVariableSpeedCoils->QLoadTotal - state.dataVariableSpeedCoils->QSensible;
        VarSpeedCoil.QSource = state.dataVariableSpeedCoils->QSource;
        VarSpeedCoil.Energy = state.dataVariableSpeedCoils->Winput * ReportingConstant;
        VarSpeedCoil.EnergyLoadTotal = state.dataVariableSpeedCoils->QLoadTotal * ReportingConstant;
        VarSpeedCoil.EnergySensible = state.dataVariableSpeedCoils->QSensible * ReportingConstant;
        VarSpeedCoil.EnergyLatent =
            (state.dataVariableSpeedCoils->QLoadTotal - state.dataVariableSpeedCoils->QSensible) * ReportingConstant;
        VarSpeedCoil.EnergySource = state.dataVariableSpeedCoils->QSource * ReportingConstant;
        VarSpeedCoil.CrankcaseHeaterConsumption =
            VarSpeedCoil.CrankcaseHeaterPower * ReportingConstant;
        VarSpeedCoil.EvapWaterConsump = 0.0;
        VarSpeedCoil.BasinHeaterConsumption = 0.0;
        // re-use EvapCondPumpElecConsumption to store WH pump energy consumption
        VarSpeedCoil.EvapCondPumpElecConsumption =
            VarSpeedCoil.HPWHCondPumpElecNomPower * ReportingConstant;
        if (RuntimeFrac == 0.0) {
            VarSpeedCoil.COP = 0.0;
        } else {
            VarSpeedCoil.COP =
                state.dataVariableSpeedCoils->QLoadTotal / state.dataVariableSpeedCoils->Winput;
        }
        VarSpeedCoil.RunFrac = RuntimeFrac;
        VarSpeedCoil.PartLoadRatio = PartLoadRatio;
        VarSpeedCoil.AirMassFlowRate = state.dataVariableSpeedCoils->PLRCorrLoadSideMdot;
        rhoair = PsyRhoAirFnPbTdbW(state,
                                   state.dataEnvrn->OutBaroPress,
                                   state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                   state.dataVariableSpeedCoils->LoadSideInletHumRat,
                                   RoutineName);
        VarSpeedCoil.AirVolFlowRate =
            VarSpeedCoil.AirMassFlowRate / rhoair;
        VarSpeedCoil.WaterMassFlowRate = state.dataVariableSpeedCoils->SourceSideMassFlowRate;
        RhoWater = RhoH2O(InletWaterTemp); // initialize
        VarSpeedCoil.WaterVolFlowRate =
            VarSpeedCoil.WaterMassFlowRate / RhoWater;

        VarSpeedCoil.OutletWaterTemp =
            state.dataVariableSpeedCoils->SourceSideInletTemp +
            state.dataVariableSpeedCoils->QSource / (state.dataVariableSpeedCoils->SourceSideMassFlowRate * CpWater);
        VarSpeedCoil.OutletWaterEnthalpy =
            state.dataVariableSpeedCoils->SourceSideInletEnth +
            state.dataVariableSpeedCoils->QSource / state.dataVariableSpeedCoils->SourceSideMassFlowRate;
        VarSpeedCoil.QWasteHeat = 0.0;

        if (VarSpeedCoil.bIsDesuperheater) // desuperheater doesn't save power and cooling energy variables
        {
            // source side is the water side; load side is the air side
            VarSpeedCoil.Power = 0.0;
            VarSpeedCoil.QLoadTotal = 0.0;
            VarSpeedCoil.QSensible = 0.0;
            VarSpeedCoil.QLatent = 0.0;
            VarSpeedCoil.Energy = 0.0;
            VarSpeedCoil.EnergyLoadTotal = 0.0;
            VarSpeedCoil.EnergySensible = 0.0;
            VarSpeedCoil.EnergyLatent = 0.0;
            VarSpeedCoil.CrankcaseHeaterConsumption = 0.0;
        }
    }

    void setVarSpeedHPWHFanTypeNum(EnergyPlusData &state, int const dXCoilNum, int const fanTypeNum)
    {
        state.dataVariableSpeedCoils->VarSpeedCoil(dXCoilNum).SupplyFan_TypeNum = fanTypeNum;
    }

    void setVarSpeedHPWHFanIndex(EnergyPlusData &state, int const dXCoilNum, int const fanIndex)
    {
        state.dataVariableSpeedCoils->VarSpeedCoil(dXCoilNum).SupplyFanIndex = fanIndex;
    }

    void setVarSpeedFanInfo(EnergyPlusData &state, int const dXCoilNum, std::string const fanName, int const fanIndex, int const fanTypeNum)
    {
        state.dataVariableSpeedCoils->VarSpeedCoil(dXCoilNum).SupplyFanIndex = fanIndex;
        state.dataVariableSpeedCoils->VarSpeedCoil(dXCoilNum).SupplyFan_TypeNum = fanTypeNum;
        state.dataVariableSpeedCoils->VarSpeedCoil(dXCoilNum).SupplyFanName = fanName;
    }

    void getCoilTypeAndName(EnergyPlusData &state, int const CoilIndex, std::string &CoilType, std::string &CoilName, bool &ErrorsFound)
    {
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        if (CoilIndex == 0) {
            ShowSevereError(state, "getCoilTypeAndName: Could not find Coil");
            ErrorsFound = true;
        } else {
            CoilName = state.dataVariableSpeedCoils->VarSpeedCoil(CoilIndex).Name;
            CoilType = state.dataVariableSpeedCoils->VarSpeedCoil(CoilIndex).VarSpeedCoilType;
        }
    }

    void CalcVarSpeedCoilHeating(EnergyPlusData &state,
                                 VariableSpeedCoilData &VarSpeedCoil,      // Heat Pump Number
                                 int const CyclingScheme,                  // Fan/Compressor cycling scheme indicator
                                 Real64 &RuntimeFrac,                      // Runtime Fraction of compressor or percent on time (on-time/cycle time)
                                 [[maybe_unused]] Real64 const SensDemand, // Cooling Sensible Demand [W] !unused1208
                                 const CompressorOperation CompressorOp,   // compressor operation flag
                                 Real64 const PartLoadRatio,               // compressor part load ratio
                                 [[maybe_unused]] Real64 const OnOffAirFlowRatio, // ratio of compressor on flow to average flow over time step
                                 Real64 const SpeedRatio, // SpeedRatio varies between 1.0 (higher speed) and 0.0 (lower speed)
                                 int const SpeedNum       // Speed number, high bound, i.e. SpeedNum - 1 is the other side
    )
    {

        //       AUTHOR         Bo Shen, based on WaterToAirHeatPumpSimple:CalcHPHeatingSimple
        //       DATE WRITTEN   March 2012
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for simulating the heating mode of the Variable Speed Water to Air HP Simple

        // METHODOLOGY EMPLOYED:
        // Simulate the heat pump performance using the coefficients and rated conditions
        // Finally, adjust the heat pump outlet conditions based on the PartLoadRatio
        // and RuntimeFrac.

        // Using/Aliasing
        using CurveManager::CurveValue;
        auto &TimeStepSys = state.dataHVACGlobal->TimeStepSys;
        using FluidProperties::GetSpecificHeatGlycol;
        using Psychrometrics::PsyCpAirFnW;
        using Psychrometrics::PsyHFnTdbW;
        using Psychrometrics::PsyRhoAirFnPbTdbW;
        using Psychrometrics::PsyTdbFnHW;
        using Psychrometrics::PsyTwbFnTdbWPb;
        using Psychrometrics::PsyWFnTdbH;
        using Psychrometrics::PsyWFnTdbTwbPb;

        // Locals
        // SUBROUTINE ARGUMENT DEFINITIONS:

        // SUBROUTINE PARAMETER DEFINITIONS:
        static constexpr std::string_view RoutineName("CalcVarSpeedCoilHeating");
        static constexpr std::string_view RoutineNameSourceSideInletTemp("CalcVarSpeedCoilHeating:SourceSideInletTemp");

        // INTERFACE BLOCK SPECIFICATIONS
        // na

        // DERIVED TYPE DEFINITIONS
        // na

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        Real64 CpSource;            // Specific heat of water [J/kg_C]
        Real64 CpAir;               // Specific heat of air [J/kg_C]
        Real64 AirMassFlowRatio;    // airflow ratio at low speed
        Real64 WaterMassFlowRatio;  // airflow ratio at high speed
        Real64 TotCapAirFFModFac;   // air flow fraction modification
        Real64 TotCapWaterFFModFac; // water flow fraction modification
        Real64 TotCapTempModFac;    // total capacity temperature correction fraction
        Real64 EIRAirFFModFac;      // air flow fraction modification
        Real64 EIRWaterFFModFac;    // water flow fraction modification
        Real64 EIRTempModFac;       // total capacity temperature correction fraction
        Real64 EIR;                 // total capacity temperature correction fraction
        int MaxSpeed;               // maximum speed level
        int SpeedCal;               // calculated speed level
        Real64 QLoadTotal1;         // heating capacity at low speed
        Real64 QLoadTotal2;         // heating capacity at high speed
        Real64 Winput1;             // power consumption at low speed
        Real64 Winput2;             // power consumption at high speed
        Real64 QWasteHeat;          // recoverable waste heat
        Real64 QWasteHeat1;         // recoverable waste heat at low speed
        Real64 QWasteHeat2;         // recoverable waste heat at high speed
        Real64 PLF;                 // part-load function
        Real64 ReportingConstant;
        Real64 rhoair(0.0); // entering air density

        // ADDED VARIABLES FOR air source coil

        MaxSpeed = VarSpeedCoil.NumOfSpeeds;

        //  LOAD LOCAL VARIABLES FROM DATA STRUCTURE (for code readability)
        if (!(CyclingScheme == ContFanCycCoil) && PartLoadRatio > 0.0) {
            VarSpeedCoil.AirMassFlowRate =
                state.dataLoopNodes->Node(VarSpeedCoil.AirInletNodeNum).MassFlowRate / PartLoadRatio;
        }

        state.dataVariableSpeedCoils->LoadSideMassFlowRate = VarSpeedCoil.AirMassFlowRate;
        state.dataVariableSpeedCoils->LoadSideInletDBTemp = VarSpeedCoil.InletAirDBTemp;
        state.dataVariableSpeedCoils->LoadSideInletHumRat = VarSpeedCoil.InletAirHumRat;

        state.dataVariableSpeedCoils->LoadSideInletWBTemp = PsyTwbFnTdbWPb(state,
                                                                           state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                                                           state.dataVariableSpeedCoils->LoadSideInletHumRat,
                                                                           state.dataEnvrn->OutBaroPress,
                                                                           RoutineName);
        state.dataVariableSpeedCoils->LoadSideInletEnth = VarSpeedCoil.InletAirEnthalpy;
        CpAir = PsyCpAirFnW(state.dataVariableSpeedCoils->LoadSideInletHumRat);

        if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
            // Get condenser outdoor node info from DX Heating Coil
            if (VarSpeedCoil.CondenserInletNodeNum != 0) {
                state.dataVariableSpeedCoils->OutdoorDryBulb =
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).Temp;
                state.dataVariableSpeedCoils->OutdoorHumRat =
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).HumRat;
                state.dataVariableSpeedCoils->OutdoorPressure =
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).Press;
                state.dataVariableSpeedCoils->OutdoorWetBulb =
                    state.dataLoopNodes->Node(VarSpeedCoil.CondenserInletNodeNum).OutAirWetBulb;
            } else {
                state.dataVariableSpeedCoils->OutdoorDryBulb = state.dataEnvrn->OutDryBulbTemp;
                state.dataVariableSpeedCoils->OutdoorHumRat = state.dataEnvrn->OutHumRat;
                state.dataVariableSpeedCoils->OutdoorPressure = state.dataEnvrn->OutBaroPress;
                state.dataVariableSpeedCoils->OutdoorWetBulb = state.dataEnvrn->OutWetBulbTemp;
            }
            state.dataVariableSpeedCoils->SourceSideMassFlowRate = 1.0; // not used and avoid divided by zero
            state.dataVariableSpeedCoils->SourceSideInletTemp = state.dataVariableSpeedCoils->OutdoorDryBulb;
            state.dataVariableSpeedCoils->SourceSideInletEnth =
                PsyHFnTdbW(state.dataVariableSpeedCoils->OutdoorDryBulb, state.dataVariableSpeedCoils->OutdoorHumRat);
            CpSource = PsyCpAirFnW(state.dataEnvrn->OutHumRat);

            // Initialize crankcase heater, operates below OAT defined in input deck for HP DX heating coil
            if (state.dataVariableSpeedCoils->OutdoorDryBulb < VarSpeedCoil.MaxOATCrankcaseHeater) {
                state.dataVariableSpeedCoils->CrankcaseHeatingPower = VarSpeedCoil.CrankcaseHeaterCapacity;
            } else {
                state.dataVariableSpeedCoils->CrankcaseHeatingPower = 0.0;
            }
        } else {
            state.dataVariableSpeedCoils->SourceSideMassFlowRate = VarSpeedCoil.WaterMassFlowRate;
            state.dataVariableSpeedCoils->SourceSideInletTemp = VarSpeedCoil.InletWaterTemp;
            state.dataVariableSpeedCoils->SourceSideInletEnth = VarSpeedCoil.InletWaterEnthalpy;
            CpSource =
                GetSpecificHeatGlycol(state,
                                      state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidName,
                                      state.dataVariableSpeedCoils->SourceSideInletTemp,
                                      state.dataPlnt->PlantLoop(VarSpeedCoil.plantLoc.loopNum).FluidIndex,
                                      RoutineNameSourceSideInletTemp);
        }

        // Check for flows, do not perform simulation if no flow in load side or source side.
        if ((state.dataVariableSpeedCoils->SourceSideMassFlowRate <= 0.0) || (state.dataVariableSpeedCoils->LoadSideMassFlowRate <= 0.0)) {
            VarSpeedCoil.SimFlag = false;
            return;
        } else {
            VarSpeedCoil.SimFlag = true;
        }

        if ((VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) &&
            (state.dataVariableSpeedCoils->OutdoorDryBulb < VarSpeedCoil.MinOATCompressor)) {
            VarSpeedCoil.SimFlag = false;
            return;
        }

        if (CompressorOp == CompressorOperation::Off) {
            VarSpeedCoil.SimFlag = false;
            return;
        }

        if (SpeedNum > MaxSpeed) {
            SpeedCal = MaxSpeed;
        } else {
            SpeedCal = SpeedNum;
        }

        RuntimeFrac = 1.0;
        VarSpeedCoil.RunFrac = 1.0;
        state.dataHVACGlobal->OnOffFanPartLoadFraction = 1.0;
        if ((SpeedNum == 1) && (PartLoadRatio < 1.0)) {
            PLF = CurveValue(state, VarSpeedCoil.PLFFPLR, PartLoadRatio);
            if (PLF < 0.7) {
                PLF = 0.7;
            }
            if (CyclingScheme == CycFanCycCoil)
                state.dataHVACGlobal->OnOffFanPartLoadFraction =
                    PLF; // save PLF for fan model, don't change fan power for constant fan mode if coil is off
            // calculate the run time fraction
            VarSpeedCoil.RunFrac = PartLoadRatio / PLF;
            VarSpeedCoil.PartLoadRatio = PartLoadRatio;

            if (VarSpeedCoil.RunFrac > 1.0) {
                VarSpeedCoil.RunFrac = 1.0; // Reset coil runtime fraction to 1.0
            } else if (VarSpeedCoil.RunFrac < 0.0) {
                VarSpeedCoil.RunFrac = 0.0;
            }

            RuntimeFrac = VarSpeedCoil.RunFrac;
        }

        if ((SpeedNum == 1) || (SpeedNum > MaxSpeed) || (SpeedRatio == 1.0)) {
            AirMassFlowRatio =
                state.dataVariableSpeedCoils->LoadSideMassFlowRate / VarSpeedCoil.DesignAirMassFlowRate;

            if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
                WaterMassFlowRatio = 1.0;
            } else {
                WaterMassFlowRatio = state.dataVariableSpeedCoils->SourceSideMassFlowRate /
                                     VarSpeedCoil.DesignWaterMassFlowRate;
            }

            TotCapTempModFac = CurveValue(state,
                                          VarSpeedCoil.MSCCapFTemp(SpeedCal),
                                          state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                          state.dataVariableSpeedCoils->SourceSideInletTemp);
            TotCapAirFFModFac = CurveValue(state, VarSpeedCoil.MSCCapAirFFlow(SpeedCal), AirMassFlowRatio);

            if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
                TotCapWaterFFModFac = 1.0;
            } else {
                TotCapWaterFFModFac =
                    CurveValue(state, VarSpeedCoil.MSCCapWaterFFlow(SpeedCal), WaterMassFlowRatio);
            }

            state.dataVariableSpeedCoils->QLoadTotal = VarSpeedCoil.MSRatedTotCap(SpeedCal) *
                                                       TotCapTempModFac * TotCapAirFFModFac * TotCapWaterFFModFac;
            VarSpeedCoil.capModFacTotal = TotCapTempModFac * TotCapAirFFModFac * TotCapWaterFFModFac;
            state.dataVariableSpeedCoils->TotRatedCapacity =
                VarSpeedCoil.MSRatedTotCap(SpeedCal); // for defrosting power cal

            EIRTempModFac = CurveValue(state,
                                       VarSpeedCoil.MSEIRFTemp(SpeedCal),
                                       state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                       state.dataVariableSpeedCoils->SourceSideInletTemp);
            EIRAirFFModFac = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(SpeedCal), AirMassFlowRatio);

            if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
                EIRWaterFFModFac = 1.0;
            } else {
                EIRWaterFFModFac =
                    CurveValue(state, VarSpeedCoil.MSEIRWaterFFlow(SpeedCal), WaterMassFlowRatio);
            }

            EIR = (1.0 / VarSpeedCoil.MSRatedCOP(SpeedCal)) * EIRTempModFac * EIRAirFFModFac *
                  EIRWaterFFModFac;
            state.dataVariableSpeedCoils->Winput = state.dataVariableSpeedCoils->QLoadTotal * EIR;

            if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
                QWasteHeat = 0.0;
            } else {
                QWasteHeat = state.dataVariableSpeedCoils->Winput * VarSpeedCoil.MSWasteHeatFrac(SpeedCal);
                QWasteHeat *= CurveValue(state,
                                         VarSpeedCoil.MSWasteHeat(SpeedCal),
                                         state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                         state.dataVariableSpeedCoils->SourceSideInletTemp);
            }

        } else {
            AirMassFlowRatio =
                state.dataVariableSpeedCoils->LoadSideMassFlowRate / VarSpeedCoil.DesignAirMassFlowRate;

            if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
                WaterMassFlowRatio = 1.0;
            } else {
                WaterMassFlowRatio = state.dataVariableSpeedCoils->SourceSideMassFlowRate /
                                     VarSpeedCoil.DesignWaterMassFlowRate;
            }

            SpeedCal = SpeedNum - 1;
            TotCapTempModFac = CurveValue(state,
                                          VarSpeedCoil.MSCCapFTemp(SpeedCal),
                                          state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                          state.dataVariableSpeedCoils->SourceSideInletTemp);
            TotCapAirFFModFac = CurveValue(state, VarSpeedCoil.MSCCapAirFFlow(SpeedCal), AirMassFlowRatio);

            if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
                TotCapWaterFFModFac = 1.0;
            } else {
                TotCapWaterFFModFac =
                    CurveValue(state, VarSpeedCoil.MSCCapWaterFFlow(SpeedCal), WaterMassFlowRatio);
            }

            QLoadTotal1 = VarSpeedCoil.MSRatedTotCap(SpeedCal) * TotCapTempModFac * TotCapAirFFModFac *
                          TotCapWaterFFModFac;

            EIRTempModFac = CurveValue(state,
                                       VarSpeedCoil.MSEIRFTemp(SpeedCal),
                                       state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                       state.dataVariableSpeedCoils->SourceSideInletTemp);
            EIRAirFFModFac = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(SpeedCal), AirMassFlowRatio);

            if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
                EIRWaterFFModFac = 1.0;
            } else {
                EIRWaterFFModFac =
                    CurveValue(state, VarSpeedCoil.MSEIRWaterFFlow(SpeedCal), WaterMassFlowRatio);
            }

            EIR = (1.0 / VarSpeedCoil.MSRatedCOP(SpeedCal)) * EIRTempModFac * EIRAirFFModFac *
                  EIRWaterFFModFac;
            Winput1 = QLoadTotal1 * EIR;

            if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
                QWasteHeat1 = 0.0;
            } else {
                QWasteHeat1 = Winput1 * VarSpeedCoil.MSWasteHeatFrac(SpeedCal);
                QWasteHeat1 *= CurveValue(state,
                                          VarSpeedCoil.MSWasteHeat(SpeedCal),
                                          state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                          state.dataVariableSpeedCoils->SourceSideInletTemp);
            }

            SpeedCal = SpeedNum;
            TotCapTempModFac = CurveValue(state,
                                          VarSpeedCoil.MSCCapFTemp(SpeedCal),
                                          state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                          state.dataVariableSpeedCoils->SourceSideInletTemp);
            TotCapAirFFModFac = CurveValue(state, VarSpeedCoil.MSCCapAirFFlow(SpeedCal), AirMassFlowRatio);

            if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
                TotCapWaterFFModFac = 1.0;
            } else {
                TotCapWaterFFModFac =
                    CurveValue(state, VarSpeedCoil.MSCCapWaterFFlow(SpeedCal), WaterMassFlowRatio);
            }

            QLoadTotal2 = VarSpeedCoil.MSRatedTotCap(SpeedCal) * TotCapTempModFac * TotCapAirFFModFac *
                          TotCapWaterFFModFac;

            EIRTempModFac = CurveValue(state,
                                       VarSpeedCoil.MSEIRFTemp(SpeedCal),
                                       state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                       state.dataVariableSpeedCoils->SourceSideInletTemp);
            EIRAirFFModFac = CurveValue(state, VarSpeedCoil.MSEIRAirFFlow(SpeedCal), AirMassFlowRatio);

            if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
                EIRWaterFFModFac = 1.0;
            } else {
                EIRWaterFFModFac =
                    CurveValue(state, VarSpeedCoil.MSEIRWaterFFlow(SpeedCal), WaterMassFlowRatio);
            }

            EIR = (1.0 / VarSpeedCoil.MSRatedCOP(SpeedCal)) * EIRTempModFac * EIRAirFFModFac *
                  EIRWaterFFModFac;
            Winput2 = QLoadTotal2 * EIR;

            if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
                QWasteHeat2 = 0.0;
            } else {
                QWasteHeat2 = Winput2 * VarSpeedCoil.MSWasteHeatFrac(SpeedCal);
                QWasteHeat2 *= CurveValue(state,
                                          VarSpeedCoil.MSWasteHeat(SpeedCal),
                                          state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                          state.dataVariableSpeedCoils->SourceSideInletTemp);
            }

            state.dataVariableSpeedCoils->QLoadTotal = QLoadTotal2 * SpeedRatio + (1.0 - SpeedRatio) * QLoadTotal1;
            state.dataVariableSpeedCoils->Winput = Winput2 * SpeedRatio + (1.0 - SpeedRatio) * Winput1;
            QWasteHeat = QWasteHeat2 * SpeedRatio + (1.0 - SpeedRatio) * QWasteHeat1;
            state.dataVariableSpeedCoils->TotRatedCapacity =
                VarSpeedCoil.MSRatedTotCap(SpeedCal) * SpeedRatio +
                (1.0 - SpeedRatio) * VarSpeedCoil.MSRatedTotCap(SpeedCal - 1);
        }

        VarSpeedCoil.CrankcaseHeaterPower = 0.0; // necessary to clear zero for water source coils
        VarSpeedCoil.DefrostPower = 0.0;         // clear the defrost power
        if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
            // Calculating adjustment factors for defrost
            // Calculate delta w through outdoor coil by assuming a coil temp of 0.82*DBT-9.7(F) per DOE2.1E
            state.dataVariableSpeedCoils->OutdoorCoilT = 0.82 * state.dataVariableSpeedCoils->OutdoorDryBulb - 8.589;
            state.dataVariableSpeedCoils->OutdoorCoildw =
                max(1.0e-6,
                    (state.dataVariableSpeedCoils->OutdoorHumRat -
                     PsyWFnTdpPb(state, state.dataVariableSpeedCoils->OutdoorCoilT, state.dataVariableSpeedCoils->OutdoorPressure)));

            // Initializing defrost adjustment factors
            state.dataVariableSpeedCoils->LoadDueToDefrost = 0.0;
            state.dataVariableSpeedCoils->HeatingCapacityMultiplier = 1.0;
            state.dataVariableSpeedCoils->FractionalDefrostTime = 0.0;
            state.dataVariableSpeedCoils->InputPowerMultiplier = 1.0;
            // Check outdoor temperature to determine of defrost is active
            if (state.dataVariableSpeedCoils->OutdoorDryBulb <= VarSpeedCoil.MaxOATDefrost) {
                // Calculate defrost adjustment factors depending on defrost control type
                if (VarSpeedCoil.DefrostControl == Timed) {
                    state.dataVariableSpeedCoils->FractionalDefrostTime = VarSpeedCoil.DefrostTime;
                    state.dataVariableSpeedCoils->HeatingCapacityMultiplier = 0.909 - 107.33 * state.dataVariableSpeedCoils->OutdoorCoildw;
                    state.dataVariableSpeedCoils->InputPowerMultiplier = 0.90 - 36.45 * state.dataVariableSpeedCoils->OutdoorCoildw;
                } else { // else defrost control is on-demand
                    state.dataVariableSpeedCoils->FractionalDefrostTime = 1.0 / (1.0 + 0.01446 / state.dataVariableSpeedCoils->OutdoorCoildw);
                    state.dataVariableSpeedCoils->HeatingCapacityMultiplier = 0.875 * (1.0 - state.dataVariableSpeedCoils->FractionalDefrostTime);
                    state.dataVariableSpeedCoils->InputPowerMultiplier = 0.954 * (1.0 - state.dataVariableSpeedCoils->FractionalDefrostTime);
                }
                // correction fractional defrost time shorten by runtime fraction
                state.dataVariableSpeedCoils->FractionalDefrostTime *= RuntimeFrac;

                if (state.dataVariableSpeedCoils->FractionalDefrostTime > 0.0) {
                    // Calculate defrost adjustment factors depending on defrost control strategy
                    if (VarSpeedCoil.DefrostStrategy == ReverseCycle) {
                        state.dataVariableSpeedCoils->LoadDueToDefrost = (0.01 * state.dataVariableSpeedCoils->FractionalDefrostTime) *
                                                                         (7.222 - state.dataVariableSpeedCoils->OutdoorDryBulb) *
                                                                         (state.dataVariableSpeedCoils->TotRatedCapacity / 1.01667);
                        state.dataVariableSpeedCoils->DefrostEIRTempModFac =
                            CurveValue(state,
                                       VarSpeedCoil.DefrostEIRFT,
                                       max(15.555, state.dataVariableSpeedCoils->LoadSideInletWBTemp),
                                       max(15.555, state.dataVariableSpeedCoils->OutdoorDryBulb));
                        VarSpeedCoil.DefrostPower =
                            state.dataVariableSpeedCoils->DefrostEIRTempModFac * (state.dataVariableSpeedCoils->TotRatedCapacity / 1.01667) *
                            state.dataVariableSpeedCoils->FractionalDefrostTime;
                    } else { // Defrost strategy is resistive
                        VarSpeedCoil.DefrostPower =
                            VarSpeedCoil.DefrostCapacity *
                            state.dataVariableSpeedCoils->FractionalDefrostTime;
                    }
                } else { // Defrost is not active because (OutDryBulbTemp > VarSpeedCoil(DXCoilNum).MaxOATDefrost)
                    VarSpeedCoil.DefrostPower = 0.0;
                }
            }

            VarSpeedCoil.CrankcaseHeaterPower =
                state.dataVariableSpeedCoils->CrankcaseHeatingPower * (1.0 - RuntimeFrac);
            //! Modify total heating capacity based on defrost heating capacity multiplier
            //! MaxHeatCap passed from parent object VRF Condenser and is used to limit capacity of TU's to that available from condenser
            //  IF(PRESENT(MaxHeatCap))THEN
            //    TotCap = MIN(MaxHeatCap,TotCap * HeatingCapacityMultiplier)
            //  ELSE
            //    TotCap = TotCap * HeatingCapacityMultiplier
            //  END IF
            state.dataVariableSpeedCoils->QLoadTotal =
                state.dataVariableSpeedCoils->QLoadTotal * state.dataVariableSpeedCoils->HeatingCapacityMultiplier -
                state.dataVariableSpeedCoils->LoadDueToDefrost;
            // count the powr separately
            state.dataVariableSpeedCoils->Winput *= state.dataVariableSpeedCoils->InputPowerMultiplier; //+ VarSpeedCoil(DXCoilNum)%DefrostPower
        }

        state.dataVariableSpeedCoils->QSource = state.dataVariableSpeedCoils->QLoadTotal + QWasteHeat - state.dataVariableSpeedCoils->Winput;
        state.dataVariableSpeedCoils->QSensible = state.dataVariableSpeedCoils->QLoadTotal;

        if (state.dataVariableSpeedCoils->QSource < 0) {
            state.dataVariableSpeedCoils->QSource = 0.0;
            QWasteHeat = state.dataVariableSpeedCoils->Winput - state.dataVariableSpeedCoils->QLoadTotal;
        }

        // calculate coil outlet state variables
        state.dataVariableSpeedCoils->LoadSideOutletEnth =
            state.dataVariableSpeedCoils->LoadSideInletEnth +
            state.dataVariableSpeedCoils->QLoadTotal / state.dataVariableSpeedCoils->LoadSideMassFlowRate;
        state.dataVariableSpeedCoils->LoadSideOutletDBTemp =
            state.dataVariableSpeedCoils->LoadSideInletDBTemp +
            state.dataVariableSpeedCoils->QSensible / (state.dataVariableSpeedCoils->LoadSideMassFlowRate * CpAir);
        state.dataVariableSpeedCoils->LoadSideOutletHumRat =
            PsyWFnTdbH(state, state.dataVariableSpeedCoils->LoadSideOutletDBTemp, state.dataVariableSpeedCoils->LoadSideOutletEnth, RoutineName);

        // Actual outlet conditions are "average" for time step
        if (CyclingScheme == ContFanCycCoil) {
            // continuous fan, cycling compressor
            VarSpeedCoil.OutletAirEnthalpy =
                PartLoadRatio * state.dataVariableSpeedCoils->LoadSideOutletEnth +
                (1.0 - PartLoadRatio) * state.dataVariableSpeedCoils->LoadSideInletEnth;
            VarSpeedCoil.OutletAirHumRat =
                PartLoadRatio * state.dataVariableSpeedCoils->LoadSideOutletHumRat +
                (1.0 - PartLoadRatio) * state.dataVariableSpeedCoils->LoadSideInletHumRat;
            VarSpeedCoil.OutletAirDBTemp =
                PsyTdbFnHW(VarSpeedCoil.OutletAirEnthalpy,
                           VarSpeedCoil.OutletAirHumRat);
            state.dataVariableSpeedCoils->PLRCorrLoadSideMdot = state.dataVariableSpeedCoils->LoadSideMassFlowRate;
        } else {
            // default to cycling fan, cycling compressor
            VarSpeedCoil.OutletAirEnthalpy = state.dataVariableSpeedCoils->LoadSideOutletEnth;
            VarSpeedCoil.OutletAirHumRat = state.dataVariableSpeedCoils->LoadSideOutletHumRat;
            VarSpeedCoil.OutletAirDBTemp = state.dataVariableSpeedCoils->LoadSideOutletDBTemp;
            state.dataVariableSpeedCoils->PLRCorrLoadSideMdot = state.dataVariableSpeedCoils->LoadSideMassFlowRate * PartLoadRatio;
        }

        // scale heat transfer rates to PLR and power to RTF
        state.dataVariableSpeedCoils->QLoadTotal *= PartLoadRatio;
        state.dataVariableSpeedCoils->QSensible *= PartLoadRatio;
        // count the powr separately
        state.dataVariableSpeedCoils->Winput *= RuntimeFrac; //+ VarSpeedCoil(DXCoilNum)%CrankcaseHeaterPower
        state.dataVariableSpeedCoils->QSource *= PartLoadRatio;
        QWasteHeat *= PartLoadRatio;

        //  Add power to global variable so power can be summed by parent object
        state.dataHVACGlobal->DXElecHeatingPower = state.dataVariableSpeedCoils->Winput;

        ReportingConstant = TimeStepSys * DataGlobalConstants::SecInHour;
        // Update heat pump data structure
        VarSpeedCoil.Power = state.dataVariableSpeedCoils->Winput;
        VarSpeedCoil.QLoadTotal = state.dataVariableSpeedCoils->QLoadTotal;
        VarSpeedCoil.QSensible = state.dataVariableSpeedCoils->QSensible;
        VarSpeedCoil.QSource = state.dataVariableSpeedCoils->QSource;
        VarSpeedCoil.Energy = state.dataVariableSpeedCoils->Winput * ReportingConstant;
        VarSpeedCoil.EnergyLoadTotal = state.dataVariableSpeedCoils->QLoadTotal * ReportingConstant;
        VarSpeedCoil.EnergySensible = state.dataVariableSpeedCoils->QSensible * ReportingConstant;
        VarSpeedCoil.EnergyLatent = 0.0;
        VarSpeedCoil.EnergySource = state.dataVariableSpeedCoils->QSource * ReportingConstant;
        VarSpeedCoil.CrankcaseHeaterConsumption =
            VarSpeedCoil.CrankcaseHeaterPower * ReportingConstant;
        VarSpeedCoil.DefrostConsumption =
            VarSpeedCoil.DefrostPower * ReportingConstant;
        if (RuntimeFrac == 0.0) {
            VarSpeedCoil.COP = 0.0;
        } else {
            VarSpeedCoil.COP =
                state.dataVariableSpeedCoils->QLoadTotal / state.dataVariableSpeedCoils->Winput;
        }
        VarSpeedCoil.RunFrac = RuntimeFrac;
        VarSpeedCoil.PartLoadRatio = PartLoadRatio;
        VarSpeedCoil.AirMassFlowRate = state.dataVariableSpeedCoils->PLRCorrLoadSideMdot;
        rhoair = PsyRhoAirFnPbTdbW(state,
                                   state.dataEnvrn->OutBaroPress,
                                   state.dataVariableSpeedCoils->LoadSideInletDBTemp,
                                   state.dataVariableSpeedCoils->LoadSideInletHumRat,
                                   RoutineName);
        VarSpeedCoil.AirVolFlowRate =
            VarSpeedCoil.AirMassFlowRate / rhoair;

        if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingAirToAirVariableSpeed) {
            VarSpeedCoil.WaterMassFlowRate = 0.0;
            VarSpeedCoil.OutletWaterTemp = 0.0;
            VarSpeedCoil.OutletWaterEnthalpy = 0.0;
        } else {
            VarSpeedCoil.WaterMassFlowRate = state.dataVariableSpeedCoils->SourceSideMassFlowRate;
            VarSpeedCoil.OutletWaterTemp =
                state.dataVariableSpeedCoils->SourceSideInletTemp -
                state.dataVariableSpeedCoils->QSource / (state.dataVariableSpeedCoils->SourceSideMassFlowRate * CpSource);
            VarSpeedCoil.OutletWaterEnthalpy =
                state.dataVariableSpeedCoils->SourceSideInletEnth -
                state.dataVariableSpeedCoils->QSource / state.dataVariableSpeedCoils->SourceSideMassFlowRate;
        }

        VarSpeedCoil.QWasteHeat = QWasteHeat;
    }

    Real64 GetCoilCapacityVariableSpeed(EnergyPlusData &state,
                                        std::string const &CoilType, // must match coil types in this module
                                        std::string const &CoilName, // must match coil names for the coil type
                                        bool &ErrorsFound            // set to true if problem
    )
    {

        // FUNCTION INFORMATION:
        //       AUTHOR         Bo Shen, based on WaterToAirHeatPumpSimple:GetCoilCapacity
        //       DATE WRITTEN   March 2012
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS FUNCTION:
        // This function looks up the rated coil capacity at the nominal speed level for the given coil and returns it.  If
        // incorrect coil type or name is given, ErrorsFound is returned as true and capacity is returned
        // as negative.

        // Using/Aliasing
        using FluidProperties::FindGlycol;

        // Return value
        Real64 CoilCapacity; // returned capacity of matched coil

        // FUNCTION LOCAL VARIABLE DECLARATIONS:
        int WhichCoil;

        // Obtains and Allocates WatertoAirHP related parameters from input file
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            //    WaterIndex=FindGlycol('WATER') !Initialize the WaterIndex once
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        if (UtilityRoutines::SameString(CoilType, "COIL:COOLING:WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT") ||
            UtilityRoutines::SameString(CoilType, "COIL:HEATING:WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT") ||
            UtilityRoutines::SameString(CoilType, "COIL:COOLING:DX:VARIABLESPEED") ||
            UtilityRoutines::SameString(CoilType, "COIL:HEATING:DX:VARIABLESPEED") ||
            UtilityRoutines::SameString(CoilType, "COIL:WATERHEATING:AIRTOWATERHEATPUMP:VARIABLESPEED")) {
            WhichCoil = UtilityRoutines::FindItemInList(CoilName, state.dataVariableSpeedCoils->VarSpeedCoil);
            if (WhichCoil != 0) {
                if (UtilityRoutines::SameString(CoilType, "COIL:HEATING:WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT") ||
                    UtilityRoutines::SameString(CoilType, "COIL:HEATING:DX:VARIABLESPEED")) {
                    CoilCapacity = state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).RatedCapHeat;
                } else if (UtilityRoutines::SameString(CoilType, "COIL:WATERHEATING:AIRTOWATERHEATPUMP:VARIABLESPEED")) {
                    CoilCapacity = state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).RatedCapWH;
                } else {
                    CoilCapacity = state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).RatedCapCoolTotal;
                }
            }
        } else {
            WhichCoil = 0;
        }

        if (WhichCoil == 0) {
            ShowSevereError(state, "GetCoilCapacityVariableSpeed: Could not find CoilType=\"" + CoilType + "\" with Name=\"" + CoilName + "\"");
            ErrorsFound = true;
            CoilCapacity = -1000.0;
        }

        return CoilCapacity;
    }

    int GetCoilIndexVariableSpeed(EnergyPlusData &state,
                                  std::string const &CoilType, // must match coil types in this module
                                  std::string const &CoilName, // must match coil names for the coil type
                                  bool &ErrorsFound            // set to true if problem
    )
    {

        // FUNCTION INFORMATION:
        //       AUTHOR         Bo Shen, based on WaterToAirHeatPumpSimple:GetCoilIndex
        //       DATE WRITTEN   March 2012
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS FUNCTION:
        // This function looks up the coil index for the given coil and returns it.  If
        // incorrect coil type or name is given, ErrorsFound is returned as true and index is returned
        // as zero.

        // Using/Aliasing
        using FluidProperties::FindGlycol;

        // Return value
        int IndexNum; // returned index of matched coil

        // Obtains and Allocates WatertoAirHP related parameters from input file
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            //    WaterIndex=FindGlycol('WATER') !Initialize the WaterIndex once
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        IndexNum = UtilityRoutines::FindItemInList(CoilName, state.dataVariableSpeedCoils->VarSpeedCoil);

        if (IndexNum == 0) {
            ShowSevereError(state, "GetCoilIndexVariableSpeed: Could not find CoilType=\"" + CoilType + "\" with Name=\"" + CoilName + "\"");
            ErrorsFound = true;
        }

        return IndexNum;
    }

    Real64 GetCoilAirFlowRateVariableSpeed(EnergyPlusData &state,
                                           std::string const &CoilType, // must match coil types in this module
                                           std::string const &CoilName, // must match coil names for the coil type
                                           bool &ErrorsFound            // set to true if problem
    )
    {

        // FUNCTION INFORMATION:
        //       AUTHOR         Bo Shen, based on WaterToAirHeatPumpSimple:GetCoilAirFlowRate
        //       DATE WRITTEN   March 2012
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS FUNCTION:
        // This function looks up the max coil air flow rate for the given coil and returns it.  If
        // incorrect coil type or name is given, ErrorsFound is returned as true and capacity is returned
        // as negative.

        // Return value
        Real64 CoilAirFlowRate; // returned air volume flow rate of matched coil

        // FUNCTION LOCAL VARIABLE DECLARATIONS:
        int WhichCoil;

        // Obtains and Allocates WatertoAirHP related parameters from input file
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            //    WaterIndex=FindGlycol('WATER') !Initialize the WaterIndex once
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        if (UtilityRoutines::SameString(CoilType, "COIL:COOLING:WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT") ||
            UtilityRoutines::SameString(CoilType, "COIL:HEATING:WATERTOAIRHEATPUMP:VARIABLESPEEDEQUATIONFIT") ||
            UtilityRoutines::SameString(CoilType, "COIL:COOLING:DX:VARIABLESPEED") ||
            UtilityRoutines::SameString(CoilType, "COIL:HEATING:DX:VARIABLESPEED") ||
            UtilityRoutines::SameString(CoilType, "COIL:WATERHEATING:AIRTOWATERHEATPUMP:VARIABLESPEED")) {
            WhichCoil = UtilityRoutines::FindItemInList(CoilName, state.dataVariableSpeedCoils->VarSpeedCoil);
            if (WhichCoil != 0) {
                // CoilAirFlowRate=VarSpeedCoil(WhichCoil)%RatedAirVolFlowRate
                if (state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).RatedAirVolFlowRate == AutoSize) { // means autosize
                    CoilAirFlowRate = state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).RatedAirVolFlowRate;
                } else {
                    CoilAirFlowRate = state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).MSRatedAirVolFlowRate(
                                          state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).NumOfSpeeds) /
                                      state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).MSRatedAirVolFlowRate(
                                          state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).NormSpedLevel) *
                                      state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).RatedAirVolFlowRate;
                } // use largest air flow rate
            }
        } else {
            WhichCoil = 0;
        }

        if (WhichCoil == 0) {
            ShowSevereError(state, "GetCoilAirFlowRateVariableSpeed: Could not find CoilType=\"" + CoilType + "\" with Name=\"" + CoilName + "\"");
            ErrorsFound = true;
            CoilAirFlowRate = -1000.0;
        }

        return CoilAirFlowRate;
    }

    int GetVSCoilPLFFPLR(EnergyPlusData &state,
                         std::string const &CoilType, // must match coil types in this module
                         std::string const &CoilName, // must match coil names for the coil type
                         bool &ErrorsFound            // set to true if problem
    )
    {

        // FUNCTION INFORMATION:
        //       AUTHOR         Bo Shen
        //       DATE WRITTEN   12/2014
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS FUNCTION:
        // This function looks up the given coil and returns PLR curve index.  If
        // incorrect coil type or name is given, ErrorsFound is returned as true and value is returned
        // as zero.

        // Return value
        int PLRNumber; // returned outlet node of matched coil

        // FUNCTION LOCAL VARIABLE DECLARATIONS:
        int WhichCoil;

        // Obtains and Allocates WatertoAirHP related parameters from input file
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            //    WaterIndex=FindGlycol('WATER') !Initialize the WaterIndex once
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        WhichCoil = UtilityRoutines::FindItemInList(CoilName, state.dataVariableSpeedCoils->VarSpeedCoil);
        if (WhichCoil != 0) {
            PLRNumber = state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).PLFFPLR;
        }

        if (WhichCoil == 0) {
            ShowSevereError(state, "GetVSCoilPLFFPLR: Could not find CoilType=\"" + CoilType + "\" with Name=\"" + CoilName + "\"");
            ErrorsFound = true;
            PLRNumber = 0;
        }

        return PLRNumber;
    }

    int GetVSCoilCapFTCurveIndex(EnergyPlusData &state,
                                 int const CoilIndex, // must match coil names for the coil type
                                 bool &ErrorsFound    // set to true if problem
    )
    {

        // FUNCTION INFORMATION:
        //       AUTHOR         Richard Raustad
        //       DATE WRITTEN   7/2017

        // PURPOSE OF THIS FUNCTION:
        // This function looks up the given coil and returns CapFT curve index.  If
        // incorrect coil index is given, ErrorsFound is returned as true and value is returned
        // as zero.

        // Return value
        int CapFTIndex; // returned CapFT curve index of matched coil

        // Obtains and Allocates WatertoAirHP related parameters from input file
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        if (CoilIndex == 0) {
            ShowSevereError(state, "GetVSCoilCapFTCurveIndex: Could not find Coil");
            ErrorsFound = true;
            CapFTIndex = 0;
        } else {
            CapFTIndex =
                state.dataVariableSpeedCoils->VarSpeedCoil(CoilIndex).MSCCapFTemp(state.dataVariableSpeedCoils->VarSpeedCoil(CoilIndex).NumOfSpeeds);
        }

        return CapFTIndex;
    }

    int GetCoilInletNodeVariableSpeed(EnergyPlusData &state,
                                      std::string const &CoilType, // must match coil types in this module
                                      std::string const &CoilName, // must match coil names for the coil type
                                      bool &ErrorsFound            // set to true if problem
    )
    {

        // FUNCTION INFORMATION:
        //       AUTHOR         Bo Shen, based on WaterToAirHeatPumpSimple:GetCoilInletNode
        //       DATE WRITTEN   March 2012
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS FUNCTION:
        // This function looks up the given coil and returns the inlet node.  If
        // incorrect coil type or name is given, ErrorsFound is returned as true and value is returned
        // as zero.

        // Using/Aliasing
        using FluidProperties::FindGlycol;

        // Return value
        int NodeNumber; // returned outlet node of matched coil

        // FUNCTION LOCAL VARIABLE DECLARATIONS:
        int WhichCoil;

        // Obtains and Allocates WatertoAirHP related parameters from input file
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            //    WaterIndex=FindGlycol('WATER') !Initialize the WaterIndex once
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        WhichCoil = UtilityRoutines::FindItemInList(CoilName, state.dataVariableSpeedCoils->VarSpeedCoil);
        if (WhichCoil != 0) {
            NodeNumber = state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).AirInletNodeNum;
        }

        if (WhichCoil == 0) {
            ShowSevereError(state, "GetCoilInletNodeVariableSpeed: Could not find CoilType=\"" + CoilType + "\" with Name=\"" + CoilName + "\"");
            ErrorsFound = true;
            NodeNumber = 0;
        }

        return NodeNumber;
    }

    int GetCoilOutletNodeVariableSpeed(EnergyPlusData &state,
                                       std::string const &CoilType, // must match coil types in this module
                                       std::string const &CoilName, // must match coil names for the coil type
                                       bool &ErrorsFound            // set to true if problem
    )
    {

        // FUNCTION INFORMATION:
        //       AUTHOR         Bo Shen, based on WaterToAirHeatPumpSimple:GetCoilOutletNode
        //       DATE WRITTEN   March 2012
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS FUNCTION:
        // This function looks up the given coil and returns the outlet node.  If
        // incorrect coil type or name is given, ErrorsFound is returned as true and value is returned
        // as zero.

        // Using/Aliasing
        using FluidProperties::FindGlycol;

        // Return value
        int NodeNumber; // returned outlet node of matched coil

        // FUNCTION LOCAL VARIABLE DECLARATIONS:
        int WhichCoil;

        // Obtains and Allocates WatertoAirHP related parameters from input file
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            //    WaterIndex=FindGlycol('WATER') !Initialize the WaterIndex once
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        WhichCoil = UtilityRoutines::FindItemInList(CoilName, state.dataVariableSpeedCoils->VarSpeedCoil);
        if (WhichCoil != 0) {
            NodeNumber = state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).AirOutletNodeNum;
        }

        if (WhichCoil == 0) {
            ShowSevereError(state, "GetCoilOutletNodeVariableSpeed: Could not find CoilType=\"" + CoilType + "\" with Name=\"" + CoilName + "\"");
            ErrorsFound = true;
            NodeNumber = 0;
        }

        return NodeNumber;
    }

    int GetVSCoilCondenserInletNode(EnergyPlusData &state,
                                    std::string const &CoilName, // must match coil names for the coil type
                                    bool &ErrorsFound            // set to true if problem
    )
    {

        // FUNCTION INFORMATION:
        //       AUTHOR         Bo Shen, based on DXCoil:GetCoilCondenserInletNode
        //       DATE WRITTEN   July 2012
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS FUNCTION:
        // This function looks up the given coil and returns the condenser inlet node.  If
        // incorrect coil  name is given, ErrorsFound is returned as true.

        // Return value
        int CondNode; // returned condenser node number of matched coil

        // FUNCTION LOCAL VARIABLE DECLARATIONS:
        int WhichCoil;

        // Obtains and Allocates WatertoAirHP related parameters from input file
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            //    WaterIndex=FindGlycol('WATER') !Initialize the WaterIndex once
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        WhichCoil = UtilityRoutines::FindItemInList(CoilName, state.dataVariableSpeedCoils->VarSpeedCoil);
        if (WhichCoil != 0) {
            CondNode = state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).CondenserInletNodeNum;
        } else {
            ShowSevereError(state, "GetCoilCondenserInletNode: Invalid VS DX Coil, Type= VS DX Cooling Name=\"" + CoilName + "\"");
            ErrorsFound = true;
            CondNode = 0;
        }

        return CondNode;
    }

    Real64 GetVSCoilMinOATCompressor(EnergyPlusData &state,
                                     std::string const &CoilName, // must match coil names for the coil type
                                     bool &ErrorsFound            // set to true if problem
    )
    {

        // FUNCTION INFORMATION:
        //       AUTHOR         Bo Shen
        //       DATE WRITTEN   July 2012
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS FUNCTION:
        // This function looks up the given coil and returns min OAT for compressor operation.  If
        // incorrect coil  name is given, ErrorsFound is returned as true.

        // Return value
        Real64 MinOAT; // returned min OAT for compressor operation

        // FUNCTION LOCAL VARIABLE DECLARATIONS:
        int WhichCoil;

        // Obtains and Allocates WatertoAirHP related parameters from input file
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            //    WaterIndex=FindGlycol('WATER') !Initialize the WaterIndex once
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        WhichCoil = UtilityRoutines::FindItemInList(CoilName, state.dataVariableSpeedCoils->VarSpeedCoil);
        if (WhichCoil != 0) {
            MinOAT = state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).MinOATCompressor;
        } else {
            ShowSevereError(state, "GetVSCoilMinOATCompressor: Invalid VS DX Coil, Type= VS DX Coil Name=\"" + CoilName + "\"");
            ErrorsFound = true;
            MinOAT = -1000.0;
        }

        return MinOAT;
    }

    Real64 GetVSCoilMinOATCompressorUsingIndex(EnergyPlusData &state,
                                               int const CoilIndex, // index to cooling coil
                                               bool &ErrorsFound    // set to true if problem
    )
    {

        // FUNCTION INFORMATION:
        //       AUTHOR         R. Raustad
        //       DATE WRITTEN   August 2019

        // PURPOSE OF THIS FUNCTION:
        // This function looks up the the min oat for the cooling coil compressor and returns it.  If
        // incorrect coil index is given, ErrorsFound is returned as true and value is returned
        // as negative 1000.

        // Return value
        Real64 MinOAT; // returned min oa temperature of matched coil

        // Obtains and Allocates WatertoAirHP related parameters from input file
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        if (CoilIndex == 0) {

            ShowSevereError(state, "GetVSCoilMinOATCompressorUsingIndex: Index passed = 0");
            ShowContinueError(state, "... returning Min OAT as -1000.");
            ErrorsFound = true;
            MinOAT = -1000.0;

        } else {

            MinOAT = state.dataVariableSpeedCoils->VarSpeedCoil(CoilIndex).MinOATCompressor;
        }

        return MinOAT;
    }

    int GetVSCoilNumOfSpeeds(EnergyPlusData &state,
                             std::string const &CoilName, // must match coil names for the coil type
                             bool &ErrorsFound            // set to true if problem
    )
    {

        // FUNCTION INFORMATION:
        //       AUTHOR         Richard Raustad, FSEC
        //       DATE WRITTEN   March 2013
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS FUNCTION:
        // This function looks up the given coil and returns number of speeds.  If
        // incorrect coil name is given, ErrorsFound is returned as true.

        // Return value
        int Speeds; // returned number of speeds

        // FUNCTION LOCAL VARIABLE DECLARATIONS:
        int WhichCoil;

        // Obtains and Allocates WatertoAirHP related parameters from input file
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        WhichCoil = UtilityRoutines::FindItemInList(CoilName, state.dataVariableSpeedCoils->VarSpeedCoil);
        if (WhichCoil != 0) {
            Speeds = state.dataVariableSpeedCoils->VarSpeedCoil(WhichCoil).NumOfSpeeds;
        } else {
            ShowSevereError(state, "GetVSCoilNumOfSpeeds: Invalid VS DX Coil, Type= VS DX Coil Name=\"" + CoilName + "\"");
            ErrorsFound = true;
            Speeds = 0;
        }

        return Speeds;
    }

    void SetVarSpeedCoilData(EnergyPlusData &state,
                             int const WSHPNum,                    // Number of OA Controller
                             bool &ErrorsFound,                    // Set to true if certain errors found
                             Optional_int CompanionCoolingCoilNum, // Index to cooling coil for heating coil = SimpleWSHPNum
                             Optional_int CompanionHeatingCoilNum, // Index to heating coil for cooling coil = SimpleWSHPNum
                             Optional_int MSHPDesignSpecIndex      // index to UnitarySystemPerformance:Multispeed object
    )
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Bo Shen, based on WaterToAirHeatPumpSimple:SetWSHPData
        //       DATE WRITTEN   March 2012
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This routine was designed to "push" information from a parent object to
        // this WSHP coil object.

        // Using/Aliasing
        using FluidProperties::FindGlycol;

        // Obtains and Allocates WatertoAirHP related parameters from input file
        if (state.dataVariableSpeedCoils->GetCoilsInputFlag) { // First time subroutine has been entered
            GetVarSpeedCoilInput(state);
            //    WaterIndex=FindGlycol('WATER') !Initialize the WaterIndex once
            state.dataVariableSpeedCoils->GetCoilsInputFlag = false;
        }

        if (WSHPNum <= 0 || WSHPNum > state.dataVariableSpeedCoils->NumVarSpeedCoils) {
            ShowSevereError(state,
                            format("SetVarSpeedCoilData: called with VS WSHP Coil Number out of range={} should be >0 and <{}",
                                   WSHPNum,
                                   state.dataVariableSpeedCoils->NumVarSpeedCoils));
            ErrorsFound = true;
            return;
        }

        if (present(CompanionCoolingCoilNum)) {
            state.dataVariableSpeedCoils->VarSpeedCoil(WSHPNum).CompanionCoolingCoilNum = CompanionCoolingCoilNum;
            state.dataVariableSpeedCoils->VarSpeedCoil(WSHPNum).FindCompanionUpStreamCoil = true;
            state.dataVariableSpeedCoils->VarSpeedCoil(CompanionCoolingCoilNum).CompanionHeatingCoilNum = WSHPNum;
        }

        if (present(CompanionHeatingCoilNum)) {
            state.dataVariableSpeedCoils->VarSpeedCoil(WSHPNum).CompanionHeatingCoilNum = CompanionHeatingCoilNum;
            state.dataVariableSpeedCoils->VarSpeedCoil(CompanionHeatingCoilNum).CompanionCoolingCoilNum = WSHPNum;
        }

        if (present(MSHPDesignSpecIndex)) {
            state.dataVariableSpeedCoils->VarSpeedCoil(WSHPNum).MSHPDesignSpecIndex = MSHPDesignSpecIndex;
        }
    }

    void UpdateVarSpeedCoil(EnergyPlusData &state, VariableSpeedCoilData &VarSpeedCoil)
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR         Bo Shen, based on WaterToAirHeatPumpSimple:UpdateSimpleWSHP
        //       DATE WRITTEN   March 2012
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine updates the Water to Air Heat Pump outlet nodes.

        // METHODOLOGY EMPLOYED:
        // Data is moved from the HP data structure to the HP outlet nodes.

        // Using/Aliasing
        auto &TimeStepSys = state.dataHVACGlobal->TimeStepSys;
        using PlantUtilities::SafeCopyPlantNode;

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int AirInletNode;
        int WaterInletNode;
        int AirOutletNode;
        int WaterOutletNode;
        Real64 ReportingConstant;

        // WatertoAirHP(DXCoilNum)%SimFlag=.FALSE.
        if (!VarSpeedCoil.SimFlag) {
            // Heatpump is off; just pass through conditions
            VarSpeedCoil.Power = 0.0;
            VarSpeedCoil.QLoadTotal = 0.0;
            VarSpeedCoil.QSensible = 0.0;
            VarSpeedCoil.QLatent = 0.0;
            VarSpeedCoil.QSource = 0.0;
            VarSpeedCoil.Energy = 0.0;
            VarSpeedCoil.EnergyLoadTotal = 0.0;
            VarSpeedCoil.EnergySensible = 0.0;
            VarSpeedCoil.EnergyLatent = 0.0;
            VarSpeedCoil.EnergySource = 0.0;
            VarSpeedCoil.COP = 0.0;
            VarSpeedCoil.RunFrac = 0.0;
            VarSpeedCoil.PartLoadRatio = 0.0;

            VarSpeedCoil.OutletAirDBTemp =
                VarSpeedCoil.InletAirDBTemp;
            VarSpeedCoil.OutletAirHumRat =
                VarSpeedCoil.InletAirHumRat;
            VarSpeedCoil.OutletAirEnthalpy =
                VarSpeedCoil.InletAirEnthalpy;
            VarSpeedCoil.OutletWaterTemp =
                VarSpeedCoil.InletWaterTemp;
            VarSpeedCoil.OutletWaterEnthalpy =
                VarSpeedCoil.InletWaterEnthalpy;
        }

        AirInletNode = VarSpeedCoil.AirInletNodeNum;
        WaterInletNode = VarSpeedCoil.WaterInletNodeNum;
        AirOutletNode = VarSpeedCoil.AirOutletNodeNum;
        WaterOutletNode = VarSpeedCoil.WaterOutletNodeNum;

        // Set the air outlet  nodes of the WatertoAirHPSimple
        state.dataLoopNodes->Node(AirOutletNode).MassFlowRate = state.dataLoopNodes->Node(AirInletNode).MassFlowRate; // LoadSideMassFlowRate
        state.dataLoopNodes->Node(AirOutletNode).Temp = VarSpeedCoil.OutletAirDBTemp;
        state.dataLoopNodes->Node(AirOutletNode).HumRat = VarSpeedCoil.OutletAirHumRat;
        state.dataLoopNodes->Node(AirOutletNode).Enthalpy = VarSpeedCoil.OutletAirEnthalpy;

        // Set the air outlet nodes for properties that just pass through & not used
        state.dataLoopNodes->Node(AirOutletNode).Quality = state.dataLoopNodes->Node(AirInletNode).Quality;
        state.dataLoopNodes->Node(AirOutletNode).Press = state.dataLoopNodes->Node(AirInletNode).Press;
        state.dataLoopNodes->Node(AirOutletNode).MassFlowRateMin = state.dataLoopNodes->Node(AirInletNode).MassFlowRateMin;
        state.dataLoopNodes->Node(AirOutletNode).MassFlowRateMax = state.dataLoopNodes->Node(AirInletNode).MassFlowRateMax; // LoadSideMassFlowRate
        state.dataLoopNodes->Node(AirOutletNode).MassFlowRateMinAvail = state.dataLoopNodes->Node(AirInletNode).MassFlowRateMinAvail;
        state.dataLoopNodes->Node(AirOutletNode).MassFlowRateMaxAvail =
            state.dataLoopNodes->Node(AirInletNode).MassFlowRateMaxAvail; // LoadSideMassFlowRate

        // Set the water outlet node of the WatertoAirHPSimple
        // Set the water outlet nodes for properties that just pass through & not used
        if (WaterInletNode != 0 && WaterOutletNode != 0) {
            SafeCopyPlantNode(state, WaterInletNode, WaterOutletNode);
            state.dataLoopNodes->Node(WaterOutletNode).Temp = VarSpeedCoil.OutletWaterTemp;
            state.dataLoopNodes->Node(WaterOutletNode).Enthalpy = VarSpeedCoil.OutletWaterEnthalpy;
        }

        ReportingConstant = TimeStepSys * DataGlobalConstants::SecInHour;
        VarSpeedCoil.Energy =
            VarSpeedCoil.Power * ReportingConstant;
        VarSpeedCoil.EnergyLoadTotal =
            VarSpeedCoil.QLoadTotal * ReportingConstant;
        VarSpeedCoil.EnergySensible =
            VarSpeedCoil.QSensible * ReportingConstant;
        VarSpeedCoil.EnergyLatent =
            VarSpeedCoil.QLatent * ReportingConstant;
        VarSpeedCoil.EnergySource =
            VarSpeedCoil.QSource * ReportingConstant;

        if (state.dataContaminantBalance->Contaminant.CO2Simulation) {
            state.dataLoopNodes->Node(AirOutletNode).CO2 = state.dataLoopNodes->Node(AirInletNode).CO2;
        }

        if (VarSpeedCoil.reportCoilFinalSizes) {
            if (!state.dataGlobal->WarmupFlag && !state.dataGlobal->DoingHVACSizingSimulations && !state.dataGlobal->DoingSizing) {
                if (VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingWaterToAirHPVSEquationFit ||
                    VarSpeedCoil.VSCoilTypeOfNum == Coil_CoolingAirToAirVariableSpeed) { // cooling coil
                    state.dataRptCoilSelection->coilSelectionReportObj->setCoilFinalSizes(
                        state,
                        VarSpeedCoil.Name,
                        VarSpeedCoil.VarSpeedCoilType,
                        VarSpeedCoil.RatedCapCoolTotal,
                        VarSpeedCoil.RatedCapCoolSens,
                        VarSpeedCoil.RatedAirVolFlowRate,
                        VarSpeedCoil.RatedWaterMassFlowRate);
                } else if (VarSpeedCoil.VSCoilTypeOfNum == Coil_HeatingWaterToAirHPVSEquationFit ||
                           VarSpeedCoil.VSCoilTypeOfNum ==
                               Coil_HeatingAirToAirVariableSpeed) { // heating coil
                    state.dataRptCoilSelection->coilSelectionReportObj->setCoilFinalSizes(
                        state,
                        VarSpeedCoil.Name,
                        VarSpeedCoil.VarSpeedCoilType,
                        VarSpeedCoil.RatedCapHeat,
                        VarSpeedCoil.RatedCapHeat,
                        VarSpeedCoil.RatedAirVolFlowRate,
                        VarSpeedCoil.RatedWaterMassFlowRate);
                }
                VarSpeedCoil.reportCoilFinalSizes = false;
            }
        }
    }

    Real64 CalcEffectiveSHR(VariableSpeedCoilData &VarSpeedCoil,     // Index number for cooling coil
                            Real64 const SHRss,      // Steady-state sensible heat ratio
                            int const CyclingScheme, // Fan/compressor cycling scheme indicator
                            Real64 const RTF,        // Compressor run-time fraction
                            Real64 const QLatRated,  // Rated latent capacity
                            Real64 const QLatActual, // Actual latent capacity
                            Real64 const EnteringDB, // Entering air dry-bulb temperature
                            Real64 const EnteringWB  // Entering air wet-bulb temperature
    )
    {

        // FUNCTION INFORMATION:
        //    AUTHOR         Bo Shen, based on WaterToAirHeatPumpSimple:CalcEffectiveSHR
        //    DATE WRITTEN   March 2012

        // PURPOSE OF THIS FUNCTION:
        //    Adjust sensible heat ratio to account for degradation of DX coil latent
        //    capacity at part-load (cycling) conditions.

        // METHODOLOGY EMPLOYED:
        //    With model parameters entered by the user, the part-load latent performance
        //    of a DX cooling coil is determined for a constant air flow system with
        //    a cooling coil that cycles on/off. The model calculates the time
        //    required for condensate to begin falling from the cooling coil.
        //    Runtimes greater than this are integrated to a "part-load" latent
        //    capacity which is used to determine the "part-load" sensible heat ratio.
        //    See reference below for additional details (linear decay model, Eq. 8b).

        //    For cycling fan operation, a modified version of Henderson and Rengarajan (1996)
        //    model is used by ultilizing the fan delay time as the time-off (or time duration
        //    for the re-evaporation of moisture from time coil). Refer to Tang, C.C. (2005)

        // REFERENCES:
        // na

        // Return value
        Real64 SHReff; // Effective sensible heat ratio, includes degradation due to cycling effects

        // FUNCTION LOCAL VARIABLE DECLARATIONS:
        Real64 Twet; // Nominal time for condensate to begin leaving the coil's condensate drain line
        // at the current operating conditions (sec)
        Real64 Gamma; // Initial moisture evaporation rate divided by steady-state AC latent capacity
        // at the current operating conditions
        Real64 Twet_Rated;            // Twet at rated conditions (coil air flow rate and air temperatures), sec
        Real64 Gamma_Rated;           // Gamma at rated conditions (coil air flow rate and air temperatures)
        Real64 Twet_max;              // Maximum allowed value for Twet
        Real64 MaxONOFFCyclesperHour; // Maximum cycling rate of heat pump [cycles/hr]
        Real64 HPTimeConstant;        // Heat pump time constant [s]
        Real64 FanDelayTime;          // Fan delay time, time delay for the HP's fan to
        // shut off after compressor cycle off  [s]
        Real64 Ton;     // Coil on time (sec)
        Real64 Toff;    // Coil off time (sec)
        Real64 Toffa;   // Actual coil off time (sec). Equations valid for Toff <= (2.0 * Twet/Gamma)
        Real64 aa;      // Intermediate variable
        Real64 To1;     // Intermediate variable (first guess at To). To = time to the start of moisture removal
        Real64 To2;     // Intermediate variable (second guess at To). To = time to the start of moisture removal
        Real64 Error;   // Error for iteration (DO) loop
        Real64 LHRmult; // Latent Heat Ratio (LHR) multiplier. The effective latent heat ratio LHR = (1-SHRss)*LHRmult

        Twet_Rated = VarSpeedCoil.Twet_Rated;
        Gamma_Rated = VarSpeedCoil.Gamma_Rated;
        MaxONOFFCyclesperHour = VarSpeedCoil.MaxONOFFCyclesperHour;
        HPTimeConstant = VarSpeedCoil.HPTimeConstant;
        FanDelayTime = VarSpeedCoil.FanDelayTime;

        //  No moisture evaporation (latent degradation) occurs for runtime fraction of 1.0
        //  All latent degradation model parameters cause divide by 0.0 if not greater than 0.0
        //  Latent degradation model parameters initialize to 0.0 meaning no evaporation model used.
        if ((RTF >= 1.0) || (QLatRated == 0.0) || (QLatActual == 0.0) || (Twet_Rated <= 0.0) || (Gamma_Rated <= 0.0) ||
            (MaxONOFFCyclesperHour <= 0.0) || (HPTimeConstant <= 0.0) || (RTF <= 0.0)) {
            SHReff = SHRss;
            return SHReff;
        }

        Twet_max = 9999.0; // high limit for Twet

        //  Calculate the model parameters at the actual operating conditions
        Twet = min(Twet_Rated * QLatRated / (QLatActual + 1.e-10), Twet_max);
        Gamma = Gamma_Rated * QLatRated * (EnteringDB - EnteringWB) / ((26.7 - 19.4) * QLatActual + 1.e-10);

        //  Calculate the compressor on and off times using a converntional thermostat curve
        Ton = 3600.0 / (4.0 * MaxONOFFCyclesperHour * (1.0 - RTF)); // duration of cooling coil on-cycle (sec)

        if ((CyclingScheme == CycFanCycCoil) && (FanDelayTime != 0.0)) {
            // For CycFanCycCoil, moisture is evaporated from the cooling coil back to the air stream
            // until the fan cycle off. Assume no evaporation from the coil after the fan shuts off.
            Toff = FanDelayTime;
        } else {
            // For ContFanCycCoil, moisture is evaporated from the cooling coil back to the air stream
            // for the entire heat pump off-cycle.
            Toff = 3600.0 / (4.0 * MaxONOFFCyclesperHour * RTF); // duration of cooling coil off-cycle (sec)
        }

        //  Cap Toff to meet the equation restriction
        if (Gamma > 0.0) {
            Toffa = min(Toff, 2.0 * Twet / Gamma);
        } else {
            Toffa = Toff;
        }

        //  Use sucessive substitution to solve for To
        aa = (Gamma * Toffa) - (0.25 / Twet) * pow_2(Gamma) * pow_2(Toffa);

        To1 = aa + HPTimeConstant;
        Error = 1.0;
        while (Error > 0.001) {
            To2 = aa - HPTimeConstant * (std::exp(-To1 / HPTimeConstant) - 1.0);
            Error = std::abs((To2 - To1) / To1);
            To1 = To2;
        }

        //  Adjust Sensible Heat Ratio (SHR) using Latent Heat Ratio (LHR) multiplier
        //  Floating underflow errors occur when -Ton/HPTimeConstant is a large negative number.
        //  Cap lower limit at -700 to avoid the underflow errors.
        aa = std::exp(max(-700.0, -Ton / HPTimeConstant));
        //  Calculate latent heat ratio multiplier
        LHRmult = max(((Ton - To2) / (Ton + HPTimeConstant * (aa - 1.0))), 0.0);

        //  Calculate part-load or "effective" sensible heat ratio
        SHReff = 1.0 - (1.0 - SHRss) * LHRmult;

        if (SHReff < SHRss) SHReff = SHRss; // Effective SHR can be less than the steady-state SHR
        if (SHReff > 1.0) SHReff = 1.0;     // Effective sensible heat ratio can't be greater than 1.0

        return SHReff;
    }

    void CalcTotCapSHR_VSWSHP(EnergyPlusData &state,
                              Real64 const InletDryBulb,       // inlet air dry bulb temperature [C]
                              Real64 const InletHumRat,        // inlet air humidity ratio [kg water / kg dry air]
                              Real64 const InletEnthalpy,      // inlet air specific enthalpy [J/kg]
                              Real64 &InletWetBulb,            // inlet air wet bulb temperature [C]
                              Real64 const AirMassFlowRatio,   // Ratio of actual air mass flow to nominal air mass flow
                              Real64 const WaterMassFlowRatio, // Ratio of actual water mass flow to nominal water mass flow
                              Real64 const AirMassFlow,        // actual mass flow for capacity and SHR calculation
                              Real64 const CBF,                // coil bypass factor
                              Real64 const TotCapNom1,         // nominal total capacity at low speed [W]
                              int const CCapFTemp1,            // capacity modifier curve index, function of entering wetbulb at low speed
                              int const CCapAirFFlow1,         // capacity modifier curve, function of actual air flow vs rated flow at low speed
                              int const CCapWaterFFlow1,       // capacity modifier curve, function of actual water flow vs rated flow at low speed
                              Real64 const TotCapNom2,         // nominal total capacity at high speed [W]
                              int const CCapFTemp2,            // capacity modifier curve index, function of entering wetbulb at high speed
                              int const CCapAirFFlow2,         // capacity modifier curve, function of actual air flow vs rated flow at high speed
                              int const CCapWaterFFlow2,       // capacity modifier curve, function of actual water flow vs rated flow at high speed
                              Real64 &TotCap1,                 // total capacity at the given conditions [W] at low speed
                              Real64 &TotCap2,                 // total capacity at the given conditions [W] at high speed
                              Real64 &TotCapSpeed,             // integrated total capacity corresponding to the speed ratio
                              Real64 &SHR,                     // sensible heat ratio at the given conditions
                              Real64 const CondInletTemp,      // Condenser inlet temperature [C]
                              Real64 const Pressure,           // air pressure [Pa]
                              Real64 const SpeedRatio,         // from 0.0 to 1.0
                              int const NumSpeeds,             // number of speeds for input
                              Real64 &TotCapModFac             // capacity modification factor, func of temp and func of flow
    )
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Bo Shen, , based on DX:CalcTotCapSHR, introducing two speed levels
        //       DATE WRITTEN   March 2012

        // PURPOSE OF THIS SUBROUTINE:
        // Calculates total capacity and sensible heat ratio of a DX coil at the specified conditions

        // METHODOLOGY EMPLOYED:
        // With the rated performance data entered by the user, the model employs some of the
        // DOE-2.1E curve fits to adjust the capacity and SHR of the unit as a function
        // of entering air temperatures and supply air flow rate (actual vs rated flow). The model
        // does NOT employ the exact same methodology to calculate performance as DOE-2, although
        // some of the DOE-2 curve fits are employed by this model.

        // The model checks for coil dryout conditions, and adjusts the calculated performance
        // appropriately.

        // REFERENCES:
        // ASHRAE HVAC 2 Toolkit page 4-81.
        // Henderson, H.I. Jr., K. Rengarajan and D.B. Shirey, III. 1992.The impact of comfort
        // control on air conditioner energy use in humid climates. ASHRAE Transactions 98(2):
        // 104-113.
        // Henderson, H.I. Jr., Danny Parker and Y.J. Huang. 2000.Improving DOE-2's RESYS routine:
        // User Defined Functions to Provide More Accurate Part Load Energy Use and Humidity
        // Predictions. Proceedings of ACEEE Conference.

        // Using/Aliasing
        using CurveManager::CurveValue;

        // SUBROUTINE PARAMETER DEFINITIONS:
        static constexpr std::string_view RoutineName("CalcTotCapSHR_VSWSHP");
        constexpr int MaxIter(30);        // Maximum number of iterations for dry evaporator calculations
        constexpr Real64 Tolerance(0.01); // Error tolerance for dry evaporator iterations

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        Real64 TotCapWaterFlowModFac1; // Total capacity modifier (function of actual supply water flow vs nominal flow) at low speed
        Real64 TotCapTempModFac2;      // Total capacity modifier (function of entering wetbulb, outside water inlet temp) at high speed
        Real64 TotCapAirFlowModFac2;   // Total capacity modifier (function of actual supply air flow vs nominal flow) at high speed
        Real64 TotCapWaterFlowModFac2; // Total capacity modifier (function of actual supply water flow vs nominal flow) at high speed
        Real64 TotCapCalc;             // temporary calculated value of total capacity [W]
        Real64 TotCapCalc1;            // temporary calculated value of total capacity [W] at low speed
        Real64 TotCapCalc2;            // temporary calculated value of total capacity [W] at high speed

        int Counter = 0;                        // Error tolerance for dry evaporator iterations
        Real64 RF = 0.4;                        // Relaxation factor for dry evaporator iterations
        Real64 werror = 0.0;                    // Deviation of humidity ratio in dry evaporator iteration loop
        Real64 SHRCalc = SHR;                   // initialize temporary calculated value of SHR
        Real64 InletWetBulbCalc = InletWetBulb; // calculated inlet wetbulb temperature used for finding dry coil point [C]
        Real64 InletHumRatCalc = InletHumRat;   // calculated inlet humidity ratio used for finding dry coil point [kg water / kg dry air]
        bool LoopOn = true;                     // flag to control the loop iteration

        //  LOOP WHILE (ABS(werror) .gt. Tolerance .OR. Counter == 0)
        while (LoopOn) {
            //   Get capacity modifying factor (function of inlet wetbulb & condenser inlet temp) for off-rated conditions
            Real64 TotCapTempModFac1 = CurveValue(state, CCapFTemp1, InletWetBulbCalc, CondInletTemp);
            //   Get capacity modifying factor (function of mass flow) for off-rated conditions
            Real64 TotCapAirFlowModFac1 = CurveValue(state, CCapAirFFlow1, AirMassFlowRatio);
            // Get capacity modifying factor (function of mass flow) for off-rated conditions
            if (CCapWaterFFlow1 == 0) {
                TotCapWaterFlowModFac1 = 1.0;
            } else {
                TotCapWaterFlowModFac1 = CurveValue(state, CCapWaterFFlow1, WaterMassFlowRatio);
            }

            //   Get total capacity
            if (NumSpeeds < 2) { // ONLY ONE SPEED
                TotCapCalc = TotCapNom1 * TotCapAirFlowModFac1 * TotCapWaterFlowModFac1 * TotCapTempModFac1;
                TotCapCalc1 = TotCapCalc;
                TotCapCalc2 = 0.0;
                TotCapModFac = TotCapAirFlowModFac1 * TotCapWaterFlowModFac1 * TotCapTempModFac1;
            } else {
                TotCapTempModFac2 = CurveValue(state, CCapFTemp2, InletWetBulbCalc, CondInletTemp);
                TotCapAirFlowModFac2 = CurveValue(state, CCapAirFFlow2, AirMassFlowRatio);

                if (CCapWaterFFlow2 == 0) {
                    TotCapWaterFlowModFac2 = 1.0;
                } else {
                    TotCapWaterFlowModFac2 = CurveValue(state, CCapWaterFFlow2, WaterMassFlowRatio);
                }

                TotCapCalc1 = TotCapNom1 * TotCapAirFlowModFac1 * TotCapWaterFlowModFac1 * TotCapTempModFac1;
                TotCapCalc2 = TotCapNom2 * TotCapAirFlowModFac2 * TotCapWaterFlowModFac2 * TotCapTempModFac2;

                TotCapCalc = TotCapCalc2 * SpeedRatio + (1.0 - SpeedRatio) * TotCapCalc1;
                TotCapModFac = (TotCapAirFlowModFac2 * TotCapWaterFlowModFac2 * TotCapTempModFac2) * SpeedRatio +
                               (1.0 - SpeedRatio) * (TotCapAirFlowModFac1 * TotCapWaterFlowModFac1 * TotCapTempModFac1);
            }

            Real64 localCBF = max(0.0, CBF); // negative coil bypass factor is physically impossible

            //   Calculate apparatus dew point conditions using TotCap and CBF
            Real64 hDelta = TotCapCalc / AirMassFlow;                       // Change in air enthalpy across the cooling coil [J/kg]
            Real64 hADP = InletEnthalpy - hDelta / (1.0 - localCBF);        // Apparatus dew point enthalpy [J/kg]
            Real64 tADP = PsyTsatFnHPb(state, hADP, Pressure, RoutineName); // Apparatus dew point temperature [C]
            Real64 wADP = PsyWFnTdbH(state, tADP, hADP, RoutineName);       // Apparatus dew point humidity ratio [kg/kg]
            Real64 hTinwADP = PsyHFnTdbW(InletDryBulb, wADP);               // Enthalpy at inlet dry-bulb and wADP [J/kg]
            SHRCalc = min((hTinwADP - hADP) / (InletEnthalpy - hADP), 1.0); // temporary calculated value of SHR

            //   Check for dry evaporator conditions (win < wadp)
            if (wADP > InletHumRatCalc || (Counter >= 1 && Counter < MaxIter)) {
                if (InletHumRatCalc == 0.0) InletHumRatCalc = 0.00001;
                werror = (InletHumRatCalc - wADP) / InletHumRatCalc;
                //     Increase InletHumRatCalc at constant inlet air temp to find coil dry-out point. Then use the
                //     capacity at the dry-out point to determine exiting conditions from coil. This is required
                //     since the TotCapTempModFac doesn't work properly with dry-coil conditions.
                InletHumRatCalc = RF * wADP + (1.0 - RF) * InletHumRatCalc;
                InletWetBulbCalc = PsyTwbFnTdbWPb(state, InletDryBulb, InletHumRatCalc, Pressure);
                ++Counter;
                if (std::abs(werror) > Tolerance) {
                    LoopOn = true; // Recalculate with modified inlet conditions
                } else {
                    LoopOn = false;
                }
            } else {
                LoopOn = false;
            }
        } // END LOOP

        //  Calculate full load output conditions
        if (SHRCalc > 1.0 || Counter > 0) SHRCalc = 1.0; // if Counter > 0 means a dry coil so SHR = 1

        SHR = SHRCalc;
        TotCap1 = TotCapCalc1;
        TotCap2 = TotCapCalc2;
        TotCapSpeed = TotCapCalc;
        InletWetBulb = InletWetBulbCalc;
    }

    Real64 getVarSpeedPartLoadRatio(EnergyPlusData &state, int const DXCoilNum)
    {
        return state.dataVariableSpeedCoils->VarSpeedCoil(DXCoilNum).PartLoadRatio;
    }

} // namespace VariableSpeedCoils

} // namespace EnergyPlus
