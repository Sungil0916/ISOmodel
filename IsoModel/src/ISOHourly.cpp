/*
 * ISOHourly.cpp
 *
 *	Created on: Apr 28, 2014
 *			Author: craig
 */

// In comments, symbols and formulas from the standard are written in LaTeX
// markup to reduce ambiguity.

#include <isomodel/ISOHourly.hpp>
#include <isomodel/SimModel.hpp>
#include <numeric>
#include <algorithm>
#include <vector>
#include <functional>
#include <iterator>
#include <string>
#include <map>

namespace openstudio {
namespace isomodel {

// Utility functions.
std::vector<double> addVectors(std::vector<double> v1, std::vector<double> v2) {
	std::vector<double> v3;
	std::transform(v1.begin(), v1.end(), v2.begin(), std::back_inserter(v3), std::plus<double>());
	return v3;
}

std::vector<double> sumHoursByMonth(std::vector<double> hourlyData) {
	std::vector<double> monthlyData(12);
	std::vector<int> monthsInHours = { 0, 744, 1416, 2160, 2880, 3624, 4344, 5088, 5832, 6552, 7296,
		8016, 8760 };
	for (int month = 0; month < 12; ++month) {
		monthlyData[month] = std::accumulate(hourlyData.begin() + monthsInHours[month],
			hourlyData.begin() + monthsInHours[month + 1], 0.0);
	}
	return monthlyData;
}

ISOHourly::ISOHourly() {
	electInternalGains = 1;//SingleBldg.L51
	permLightPowerDensityWperM2 = 0;//SingleBldg.L50
	ventPreheatDegC = -50;//SingleBldg.Q40

	//XXX External Equipment usage Q56
	externalEquipment = 244000;
}

std::map<std::string, double> ISOHourly::calculateHour(int hourOfYear, int month, int dayOfWeek, int hourOfDay,
		double windMps, double temperature, double electPriceUSDperMWh,
		double solarRadiationS,	double solarRadiationE,
		double solarRadiationN, double solarRadiationW,
		double solarRadiationH, double& TMT1, double& tiHeatCool) {
	
	int scheduleOffset = (dayOfWeek % 7)==0 ? 7 : dayOfWeek % 7;//1
	// ExcelFunctions.printOut("E156",scheduleOffset,1);
	
	// Extract schedules to a function so that we can populate them based on
	// timeslice instead of fixed schedules.
	double fanEnabled = fanSchedule(hourOfYear,hourOfDay,scheduleOffset);//ExcelFunctions.OFFSET(CZ90,hourOfDay-1,E156-1);//0
	double ventExhaustM3phpm2 = ventilationSchedule(hourOfYear,hourOfDay,scheduleOffset);//ExcelFunctions.OFFSET(AB90,hourOfDay-1,E156-1);//1E-05
	double externalEquipmentEnabled = exteriorEquipmentSchedule(hourOfYear,hourOfDay,scheduleOffset);//ExcelFunctions.OFFSET(BV90,hourOfDay-1,E156-1);//0.05
	double internalEquipmentEnabled = interiorEquipmentSchedule(hourOfYear,hourOfDay,scheduleOffset);//ExcelFunctions.OFFSET(AK90,hourOfDay-1,E156-1);//0.3
	double exteriorLightingEnabled = exteriorLightingSchedule(hourOfYear,hourOfDay,scheduleOffset);//1
	double internalLightingEnabled = interiorLightingSchedule(hourOfYear,hourOfDay,scheduleOffset);//ExcelFunctions.OFFSET(BL90,hourOfDay-1,E156-1);//0.05
	double actualHeatingSetpoint = heatingSetpointSchedule(hourOfYear,hourOfDay,scheduleOffset);//15.6
	double actualCoolingSetpoint = coolingSetpointSchedule(hourOfYear,hourOfDay,scheduleOffset);//30

	// http://www.engineeringtoolbox.com/fans-efficiency-power-consumption-d_197.html eq. 3.
	double Qfan_tot = ventExhaustM3phpm2/3600*fanEnabled*fanDeltaPinPa/fanN;//0

	double externalEquipmentEnergyWperm2 = externalEquipmentEnabled*externalEquipment/structure->floorArea();//0.263382986063586
	
	// \Phi_{int,A}, ISO 13790 10.4.2.
	// Monthly name: phi_plug_occ and phi_plug_unocc.
	double phi_plug = internalEquipmentEnabled * building->electricApplianceHeatGainOccupied();

	double lightingContributionH = 53 / areaNaturallyLightedRatio * solarRadiationH * (naturalLightRatioH + shadingUsePerWPerM2 * naturalLightShadeRatioReductionH * std::min(shadingRatioWtoM2, solarRadiationH));//0
	// XXX BAA@20140626: I think this should be 'W' for west, not 'O'.
	double lightingContributionO = 53 / areaNaturallyLightedRatio * solarRadiationW * (naturalLightRatioW + shadingUsePerWPerM2 * naturalLightShadeRatioReductionW * std::min(shadingRatioWtoM2, solarRadiationW));//0
	double lightingContributionS = 53 / areaNaturallyLightedRatio * solarRadiationS * (naturalLightRatioS + shadingUsePerWPerM2 * naturalLightShadeRatioReductionS * std::min(shadingRatioWtoM2, solarRadiationS));//0
	double lightingContributionE = 53 / areaNaturallyLightedRatio * solarRadiationE * (naturalLightRatioE + shadingUsePerWPerM2 * naturalLightShadeRatioReductionE * std::min(shadingRatioWtoM2, solarRadiationE));//0
	double lightingContributionN = 53 / areaNaturallyLightedRatio * solarRadiationN * (naturalLightRatioN + shadingUsePerWPerM2 * naturalLightShadeRatioReductionN * std::min(shadingRatioWtoM2, solarRadiationN));//0
	double lightingLevel = (lightingContributionN+lightingContributionE+lightingContributionS+lightingContributionO+lightingContributionH);//0
	double electricForNaturalLightArea = std::max(0.0, maxRatioElectricLighting * (1 - lightingLevel / elightNatural));//1
	double electricForTotalLightArea = electricForNaturalLightArea * areaNaturallyLightedRatio + (1 - areaNaturallyLightedRatio) * maxRatioElectricLighting;//1
	// Heat produced by lighting.
	// \Phi_{int,L}, ISO 13790 10.4.3. 
	// Monthly name: phi_illum_occ, phi_illum_unocc
	double phi_illum = electricForTotalLightArea*lights->powerDensityOccupied()*internalLightingEnabled*electInternalGains;//0.538

	// XXX: permLightPowerDensityWperM2 is unused.
	double Q_illum_tot = electricForTotalLightArea * lights->powerDensityOccupied() * internalLightingEnabled;

	// \Phi_{int}, ISO 13790 10.2.2 eq. 35.
	// Monthly name: phi_int_wk_nt, phi_int_wke_day, phi_int_wke_nt.
	double phi_int = phi_plug + phi_illum;//1.753

	//TODO -- expand solar heat calculations to array format to include diagonals.
	// \Phi_{sol,k}, ISO 13790 11.3.2 eq. 43. 
	// Note: method of calculating A_{sol,k} with movable shading differs from
	// the method in the standard.
	double solarHeatGainH = solarRadiationH*(solarRatioH+solarShadeRatioReductionH*shadingUsePerWPerM2*std::min(solarRadiationH,shadingRatioWtoM2));//0
	double solarHeatGainW = solarRadiationW*(solarRatioW+solarShadeRatioReductionW*shadingUsePerWPerM2*std::min(solarRadiationW,shadingRatioWtoM2));//0
	double solarHeatGainS = solarRadiationS*(solarRatioS+solarShadeRatioReductionS*shadingUsePerWPerM2*std::min(solarRadiationS,shadingRatioWtoM2));//0
	double solarHeatGainE = solarRadiationE*(solarRatioE+solarShadeRatioReductionE*shadingUsePerWPerM2*std::min(solarRadiationE,shadingRatioWtoM2));//0
	double solarHeatGainN = solarRadiationN*(solarRatioN+solarShadeRatioReductionN*shadingUsePerWPerM2*std::min(solarRadiationN,shadingRatioWtoM2));//0
	// \Phi_{sol}, ISO 13790 11.2.2 eq. 41.
	double qSolarHeatGain = (solarHeatGainN+solarHeatGainE+solarHeatGainS+solarHeatGainW+solarHeatGainH);//0
	// \Phi_{ia}, ISO 13790 C.2 eq. C.1. 
	// (Note that solarPair = 0 and intPair = 0.5).
	double phii = solarPair*qSolarHeatGain+intPair*phi_int;//0.8765
	// \Phi_{ia10}, ISO 13790 C.4.2. 
	// Used to calculate \theta_{air,ac} when available heating or cooling power
	// is insufficient to achieve the setpoint. Adding 10 is equivalent to
	// applying 10 W/m^2 to the building because all the values in this
	// implementation are expressed per area (so as to get final results in EUI).
	double phii10 = phii+10;//10.8765

	// Ventilation from wind. ISO 15242.
	double qSupplyBySystem = ventExhaustM3phpm2*windImpactSupplyRatio;//1E-05
	double exhaustSupply = -(qSupplyBySystem-ventExhaustM3phpm2); // ISO 15242 q_{v-diff}. 0
	double tAfterExchange = (1-ventilation->heatRecoveryEfficiency())*temperature+ventilation->heatRecoveryEfficiency()*20;//-1
	double tSuppliedAir = std::max(ventPreheatDegC,tAfterExchange);//-1
	// ISO 15242 6.7.1 Step 1.
	double qWind = 0.0769*q4Pa*std::pow((ventDcpWindImpact*windMps*windMps),0.667);//0.341700355961478
	double qStackPrevIntTemp = 0.0146*q4Pa*std::pow((0.5*windImpactHz*(std::max(0.00001,std::abs(temperature-tiHeatCool)))),0.667);//1.21396172390701
	// ISO 15242 6.7.1 Step 2.
	double qExfiltration = std::max(0.0,std::max(qStackPrevIntTemp,qWind)-std::abs(exhaustSupply)*(0.5*qStackPrevIntTemp+0.667*(qWind)/(qStackPrevIntTemp+qWind)));//1.21396172390701
	double qEnvelope = std::max(0.0,exhaustSupply)+qExfiltration;//1.21396172390701
	// ISO 15242 6.7.2.
	double qEnteringTotal = qEnvelope+qSupplyBySystem;//1.21397172390701

	// \theta_{sup} ISO 13790 9.3.
	double tEnteringAndSupplied = (temperature*qEnvelope+tSuppliedAir*qSupplyBySystem)/qEnteringTotal;//-1
	// I think hei is H_{ve,adj} or H_{ve} ISO 13790 9.3.1 eq. 21. I'm not sure
	// what the 0.34 is.
	double hei = 0.34*qEnteringTotal;//0.412750386128385
	// H_{tr,1}, ISO 13790 C.3 eq. C.6.
	double h1 = 1/(1/hei+1/his);//0.402051964790249
	// H_{tr,2}, ISO 13790 C.3 eq. C.7.
	double h2 = h1+hwindowWperkm2;//0.726440377838674
	//ExcelFunctions.printOut("h2",h2,0.726440377838674);

	// Subscript '0' indicates the free-floating condition and sub '10' indicates
	// the the condition after applying 10 W/m^s. This procedure is outlined in
	// ISO 13790 C.4.2 and is used to calculate the temperature when insuficient
	// heating or cooling power is available to get the temp between the heating
	// and cooling setpoints.
	
	// \Phi_{st}, ISO 13790 C.2 eq. C.3 
	// In generalized form from Georgia Tech spreadsheet.
	double phisPhi0 = prsSolar*qSolarHeatGain+prsInterior*phi_int;//-0.00694325870664089
	// \Phi_{m}, ISO 13790 C.2 eq. C.2.
	// In generalized form from Georgia Tech spreadsheet.
	double phimPhi0 = prmSolar*qSolarHeatGain+prmInterior*phi_int;//0.8765
	// H_{tr,3}, ISO 13790 C.3 eq. C.9.
	double h3 = 1/(1/h2+1/hms);//0.713778173058944
	// \Phi_{mtot}, ISO 13790 C.3 eq. C.5.
	double phimTotalPhi10 = phimPhi0+hem*temperature+h3*(phisPhi0+hwindowWperkm2*temperature+h1*(phii10/hei+tEnteringAndSupplied))/h2;//10.4245918723996
	double phimTotalPhi0	= phimPhi0+hem*temperature+h3*(phisPhi0+hwindowWperkm2*temperature+h1*(phii/hei+tEnteringAndSupplied))/h2;//0.853577061315981
	// \theta_{m,t10}, ISO 13790 C.3 eq. C.4.
	double tmt1Phi10 = (TMT1*(Cm/3.6-0.5*(h3+hem))+phimTotalPhi10)/(Cm/3.6+0.5*(h3+hem));//19.9760054473834
	double tmPhi10 = 0.5*(TMT1+tmt1Phi10);//19.9880027236917
	double tsPhi10 = (hms*tmPhi10+phisPhi0+hwindowWperkm2*temperature+h1*(tEnteringAndSupplied+phii10/hei))/(hms+hwindowWperkm2+h1);//19.8762155145252
	//ExcelFunctions.printOut("BA156",tsPhi10,19.8762155145252);
	double tiPhi10 = (his*tsPhi10+hei*tEnteringAndSupplied+phii10)/(his+hei);//20.0181282126061
	// \theta_{m,t}, ISO 13790 C.3 eq. C.4.
	double tmt1Phi0 = (TMT1*(Cm/3.6-0.5*(h3+hem))+phimTotalPhi0)/(Cm/3.6+0.5*(h3+hem));//19.9416027398399
	double tmPhi0 = 0.5*(TMT1+tmt1Phi0);//19.9708013699199
	double tsPhi0 = (hms*tmPhi0+phisPhi0+hwindowWperkm2*temperature+h1*(tEnteringAndSupplied+phii/hei))/(hms+hwindowWperkm2+h1);//19.6255895732024
	double tiPhi0 = (his*tsPhi0+hei*tEnteringAndSupplied+phii)/(his+hei);//19.1460200317084
	double phiCooling = 10*(actualCoolingSetpoint-tiPhi0)/(tiPhi10-tiPhi0);//124.45680714884
	double phiHeating = 10*(actualHeatingSetpoint-tiPhi0)/(tiPhi10-tiPhi0);//-40.6603229894958
	double phiActual = std::max(0.0,phiHeating)+std::min(phiCooling,0.0);//0
	double Qneed_cl = std::max(0.0,-phiActual);// Raw need. Not adjusted for efficiency.
	double Qneed_ht = std::max(0.0,phiActual); // Raw need. Not adjusted for efficiency.

	double Q_illum_ext_tot =lights->exteriorEnergy()*exteriorLightingEnabled/structure->floorArea();//0.0539503346043362
	//ExcelFunctions.printOut("CS156",exteriorLightingEnergyWperm2,0.0539503346043362);

	double Q_dhw=0;//XXX no DHW calculations

	std::map<std::string, double> results;
	results["Qneed_ht"] = Qneed_ht;
	results["Qneed_cl"] = Qneed_cl;
	results["Q_illum_tot"] = Q_illum_tot;
	results["Q_illum_ext_tot"] = Q_illum_ext_tot;
	results["Qfan_tot"] = Qfan_tot;
	results["phi_plug"] = phi_plug;
	results["externalEquipmentEnergyWperm2"] = externalEquipmentEnergyWperm2;
	results["Q_dhw"] = Q_dhw;

	// SPEED TEST vector vs map
	std::vector<double> resultsVectorTest;
	resultsVectorTest.push_back(Qneed_ht);
	resultsVectorTest.push_back(Qneed_cl);
	resultsVectorTest.push_back(Q_illum_tot);
	resultsVectorTest.push_back(Q_illum_ext_tot);
	resultsVectorTest.push_back(Qfan_tot);
	resultsVectorTest.push_back(phi_plug);
	resultsVectorTest.push_back(externalEquipmentEnergyWperm2);
	resultsVectorTest.push_back(Q_dhw);

	// Update tiHeatCool & TMT1 for next hour. tiHeatCool and TMT1 are passed by
	// reference to the function, allowing this information to pass from hour to
	// hour.
	double phiiHeatCool = phiActual + phii;
	// \Phi_{mtot} ISO 13790 C.3 eq. C.5
	double phimHeatCoolTotal = phimPhi0+hem*temperature+h3*(phisPhi0+hwindowWperkm2*temperature+h1*(phiiHeatCool/hei+tEnteringAndSupplied))/h2;//0.853577061315981
	// Set tmt to this hour's \theta_{m,t-1}.
	double tmt = TMT1;
	// \theta_{m,t}, ISO 13790 C.3 eq. C.4.
	// Set TMT1 to next hour's \theta_{m,t-1} (this hour's \theta_{m,t}).
	TMT1 = (TMT1*(Cm/3.6-0.5*(h3+hem))+phimHeatCoolTotal)/(Cm/3.6+0.5*(h3+hem));//19.9416027398399
	// \theta_{m}, ISO 13790 C.3 eq. C.9.
	double tmHeatCool = 0.5*(TMT1+tmt);//19.9708013699199
	// \theta_{s}, ISO 13790 C.3 eq. C.10.
	double tsHeatCool = (hms*tmHeatCool+phisPhi0+hwindowWperkm2*temperature+h1*(tEnteringAndSupplied+phiiHeatCool/hei))/(hms+hwindowWperkm2+h1);//19.6255895732024
	// \theta_{air}, ISO 13790, C.3 eq. C.11.
	tiHeatCool = (his*tsHeatCool + hei * tEnteringAndSupplied + phiiHeatCool)/(his + hei);//19.1460200317084

	return results;
}

ISOHourly::~ISOHourly() {

}

const int ISOHourly::SOUTH = 0;
const int ISOHourly::SOUTHEAST = 1;
const int ISOHourly::EAST = 2;
const int ISOHourly::NORTHEAST = 3;
const int ISOHourly::NORTH = 4;
const int ISOHourly::NORTHWEST = 5;
const int ISOHourly::WEST = 6;
const int ISOHourly::SOUTHWEST = 7;
const int ISOHourly::ROOF = 8;

void ISOHourly::initialize() {
	//XXX where do all these static numbers come from?
	fanDeltaPinPa = 800;//800
	fanN = 0.8;//0.8
	provisionalCFlowad = 1;//1
	solarPair = 0;//0
	intPair = 0.5;//0.5
	presenceSensorAd = 0.6;//0.6
	automaticAd = 0.8;//0.8
	presenceAutoAd = 0.6;//0.6
	manualSwitchAd = 1;//1
	presenceSensorLux = 500;//500
	automaticLux = 300;//300
	presenceAutoLux = 300;//300
	manualSwitchLux = 500;//500
	shadingRatioWtoM2 = 500;//500
	shadingMaximumUseRatio = 0.5;//0.5
	ventDcpWindImpact = 0.75;//0.75
	AtPerAFloor = 4.5;//4.5
	hci = 2.5;//2.5
	hri = 5.5;//5.5

	switch((int)building->lightingOccupancySensor()){
		case 2:
			maxRatioElectricLighting = presenceSensorAd;
			elightNatural = presenceSensorLux;
			break;
		case 3:
			maxRatioElectricLighting = automaticAd;
			elightNatural = automaticLux;
			break;
		case 4:
			maxRatioElectricLighting = presenceAutoAd;
			elightNatural = presenceAutoLux;
			break;
		default:
			maxRatioElectricLighting = manualSwitchAd;
			elightNatural = manualSwitchLux;
			break;
	}
	double lightedNaturalAream2 = 0;//XXX SingleBuilding.L53
	areaNaturallyLighted = std::max(0.0001,lightedNaturalAream2);//0.0001
	areaNaturallyLightedRatio = areaNaturallyLighted/structure->floorArea();//2.15887693494743E-09
	for(int i = 0;i<9;i++){
		this->structureCalculations(structure->windowShadingDevice(),
				structure->wallArea()[i],structure->windowArea()[i],
				structure->wallUniform()[i],structure->windowUniform()[i],
				structure->wallSolarAbsorbtion()[i],
				structure->windowShadingCorrectionFactor()[i],
				structure->windowNormalIncidenceSolarEnergyTransmittance()[i],
				i);
	}

	shadingUsePerWPerM2 = shadingMaximumUseRatio/shadingRatioWtoM2;//0.001
	naturalLightRatioH = nla[ROOF]/structure->floorArea();//0
	naturalLightShadeRatioReductionH = nlaWMovableShadingH-naturalLightRatioH;//0
	nlaWMovableShadingW = nlams[WEST]/structure->floorArea();//0.00885843163506495
	naturalLightRatioW = nla[WEST]/structure->floorArea();//0.00885843163506495
	naturalLightShadeRatioReductionW = nlaWMovableShadingW-naturalLightRatioW;//0
	nlaWMovableShadingS = nlams[SOUTH]/structure->floorArea();//0.0132876952208514
	naturalLightRatioS = nla[SOUTH]/structure->floorArea();//0.0132876952208514
	naturalLightShadeRatioReductionS = nlaWMovableShadingS-naturalLightRatioS;//0
	nlaWMovableShadingE = nlams[EAST]/structure->floorArea();//0.00885843163506495
	naturalLightRatioE = nla[EAST]/structure->floorArea();//0.00885843163506495
	naturalLightShadeRatioReductionE = nlaWMovableShadingE-naturalLightRatioE;//0
	nlaWMovableShadingN = nlams[NORTH]/structure->floorArea();//0.0132876952208514
	naturalLightRatioN = nla[NORTH]/structure->floorArea();//0.0132876952208514
	naturalLightShadeRatioReductionN = nlaWMovableShadingN-naturalLightRatioN;//0
	saWMovableShadingH = sams[ROOF]/structure->floorArea();//0.00082174001745161
	solarRatioH = sa[ROOF]/structure->floorArea();//0.00082174001745161
	solarShadeRatioReductionH = saWMovableShadingH-solarRatioH;//0
	saWMovableShadingW = sams[WEST]/structure->floorArea();//0.00891699584502545
	solarRatioW = sa[WEST]/structure->floorArea();//0.0129205467658081
	solarShadeRatioReductionW = saWMovableShadingW-solarRatioW;//-0.0040035509207826
	saWMovableShadingS = sams[SOUTH]/structure->floorArea();//0.0133755339312847
	solarRatioS = sa[SOUTH]/structure->floorArea();//0.0193808819012279
	solarShadeRatioReductionS = saWMovableShadingS-solarRatioS;//-0.00600534796994325
	saWMovableShadingE = sams[EAST]/structure->floorArea();//0.00891699584502545
	solarRatioE = sa[EAST]/structure->floorArea();//0.0129205467658081
	solarShadeRatioReductionE = saWMovableShadingE-solarRatioE;//-0.0040035509207826
	saWMovableShadingN = sams[NORTH]/structure->floorArea();//0.0133755339312847
	solarRatioN = sa[NORTH]/structure->floorArea();//0.0193808819012279
	solarShadeRatioReductionN = saWMovableShadingN-solarRatioN;//-0.00600534796994325

	// ISO 15242 Air leakage values.
	// Air leakage at 50 Pa in air-changes/hr. (Such as from blower door test).
	double n50 = 2; // SingleBldg.V4
	// Total air leakage at 4Pa in m3/hr. ISO 15242 Annex D Table D.1.
	double buildingv8 = 0.19 * (n50 *	(structure->floorArea() * structure->buildingHeight()));
	// Air leakage per area at 4Pa (m3/hr/m2).
	q4Pa = std::max(0.000001,buildingv8/structure->floorArea()); // 1.5048

	P96 = hri*1.2; // 6.6
	// ISO 13790 12.2.2
	P97 = hci+P96; // 9.1 XXX Does it make sense to do P97=hci+hri*1.2 and eliminate P96?
	// ISO 13790 7.2.2.2
	P98 = 1/hci-1/P97; // 0.29010989010989 (or 1/3.45).
	
	// ISO 13790 7.2.2.2 eq. 9 
	//
	// Eq in ISO is H_{tr,is} = h_{is} * A_{tot}
	// where A_{tot} = \Lambda_{at} * A_{f}
	//
	// P98 is 1/h_{is} (not sure why its done this way).
	// Eq here is H_{tr,is} = \Lambda_{at} / (1/h_{is})
	//						H_{tr,is} = \Lambda_{at} * h_{is}
	//
	// This is the "per floor area" version of eq. 9.
	his = AtPerAFloor/P98; //15.5113636363636

	// Calculate Cm from the data in the .ism file.
	// Units seem to need to be in KJ, so divide by 1000.
	double Cm_int = structure->interiorHeatCapacity() / 1000.0;
	// Convert env Cm to per floor area.
	double Cm_env = (structure->wallHeatCapacity()*sum(structure->wallArea())/structure->floorArea()) / 1000.0;
	Cm = Cm_int + Cm_env;

	// Calculate Am based the Cm value and the default values in ISO 13790 12.3.1.2 Table 12.
	if (Cm > 370.0) {
		Am = 3.5;
	} else if (Cm > 260.0) {
		// Am = 3.0 @ Cm = 260; Am = 3.5 @ Cm = 370.
		Am = 3.0 + 0.5 * ((Cm - 260) / 110);
	} else if (Cm > 165.0) {
		// Am = 2.5 @ Cm = 165; Am = 3.0 @ Cm = 260.
		Am = 2.5 + 0.5 * ((Cm - 165) / 95);
	}
	else {
		Am = 2.5;
	}

	double hWind = 0, hWall = 0;
	for(int i = 0;i<ROOF+1;i++){
		hWind += hWindow[i];
		hWall += htot[i] - hWindow[i];
	}
	hwindowWperkm2 = hWind/structure->floorArea();//0.324388413048425
	
	// Constant portion of \Phi_{st}, i.e. without multiplying by
	// (.5*\Phi_{int} + \Phi_{sol}).	ISO 13790 C.2 eq. C.3.
	prs = (AtPerAFloor-Am-hwindowWperkm2/P97)/AtPerAFloor;//-0.0079215729682155
	// intPair = 0.5, this ends up providing the ".5" in ".5*\Phi_{int}" in
	// eq. C.3. When used in phisPhi0.
	prsInterior = (1-intPair)*prs;//-0.00396078648410775
	prsSolar = (1-solarPair)*prs;//-0.0079215729682155

	// Constant portion of \Phi_{m}, i.e. without multiplying by
	// (.5*\Phi_{int} + \Phi_{sol}).	ISO 13790 C.2 eq. C.2.
	prm = Am/AtPerAFloor;//1
	prmInterior = (1-intPair)*prm;//0.5
	prmSolar = (1-solarPair)*prm;//1

	// ISO 13790 12.2.2 eq. 64
	hms = P97*Am;//40.95

	hOpaqueWperkm2 = std::max(hWall/structure->floorArea(),0.000001);//0.14073662888776

	// ISO 13790 12.2.2 eq. 63
	hem = 1/(1/hOpaqueWperkm2-1/hms);//0.141221979444827

	double hzone = 39;//XXX SingleBuilding.N6
	windImpactHz = std::max(0.1,hzone);//39
	windImpactSupplyRatio = std::max(0.00001,ventilation->fanControlFactor());//XXX ventSupplyExhaustRatio = SingleBuilding.P40 ?

}

void ISOHourly::populateSchedules() {

	int dayStart = (int)pop->daysStart();
	int dayEnd = (int)pop->daysEnd();
	int hourStart = (int)pop->hoursStart();
	int hourEnd = (int)pop->hoursEnd();

	bool hoccupied, doccupied, popoccupied;
	for(int h = 0;h<24;h++){
		hoccupied = h >= hourStart && h < hourEnd;
		for(int d = 0;d<7;d++){
			doccupied = (d >= dayStart && d < dayEnd);
			popoccupied = hoccupied && doccupied;
			fixedVentilationSchedule[h][d] = hoccupied ? ventilation->supplyRate() : 0.0;
			fixedFanSchedule[h][d] = hoccupied ? 1 : 0.0;
			fixedExteriorEquipmentSchedule[h][d] = hoccupied ? 0.3 : 0.12;
			fixedInteriorEquipmentSchedule[h][d] = popoccupied ? 0.9 : 0.3;
			fixedExteriorLightingSchedule[h][d] = !hoccupied ? 1.0 : 0.0;
			fixedInteriorLightingSchedule[h][d] = popoccupied ? 0.9 : 0.05;
			fixedActualHeatingSetpoint[h][d] = popoccupied ? heating->temperatureSetPointOccupied() : heating->temperatureSetPointUnoccupied();
			fixedActualCoolingSetpoint[h][d] = popoccupied ? cooling->temperatureSetPointOccupied() : cooling->temperatureSetPointUnoccupied();
		}
	}

}
void printMatrix(const char* matName, double* mat,unsigned int dim1,unsigned int dim2){
	if(DEBUG_ISO_MODEL_SIMULATION)
	{
		std::cout << matName << "("<< dim1 <<", " << dim2 <<	"): " << std::endl << "\t";
		for(unsigned int j = 0;j< dim2; j++){
			std::cout << "," << j;
		}
		std::cout << std::endl; 
		for(unsigned int i = 0;i<dim1 ;i++){
	std::cout << "\t" << i;
			for(unsigned int j = 0;j< dim2; j++){
	std::cout << "," << mat[i*dim2+j] ;
			}		
			std::cout << std::endl; 
		}
	}
}

ISOResults ISOHourly::calculateHourly() {
	populateSchedules();
	printMatrix("Cooling Setpoint",(double*)this->fixedActualCoolingSetpoint,24,7);
	printMatrix("Heating Setpoint",(double*)this->fixedActualHeatingSetpoint,24,7);
	printMatrix("Exterior Equipment",(double*)this->fixedExteriorEquipmentSchedule,24,7);
	printMatrix("Exterior Lighting",(double*)this->fixedExteriorLightingSchedule,24,7);
	printMatrix("Fan",(double*)this->fixedFanSchedule,24,7);
	printMatrix("Interior Equipment",(double*)this->fixedInteriorEquipmentSchedule,24,7);
	printMatrix("Interior Lighting",(double*)this->fixedInteriorLightingSchedule,24,7);
	printMatrix("Ventilation",(double*)this->fixedVentilationSchedule,24,7);
	initialize();
	int hourOfDay = 1;
	int dayOfWeek = 1;
	int month = 1;
	TimeFrame frame;
	double TMT1,tiHeatCool;
	TMT1 = tiHeatCool = 20;
	std::vector<double > wind = weatherData->data()[WSPD];
	std::vector<double > temp = weatherData->data()[DBT];
	SolarRadiation pos(&frame, weatherData.get());
	pos.Calculate();
	std::vector<std::vector<double > > radiation = pos.eglobe();
	electPriceUSDperMWh[0] = 24;

	// Container to hold results.
	// Q_illum_tot, Q_illum_ext_tot, Qneed_ht, Qneed_cl, phi_plug, externalEquipmentEnergyWperm2, 
	// Qfan_tot, Q_dhw

	// TIMING TEST
	LARGE_INTEGER li = { 0 }, li2 = { 0 };
	QueryPerformanceFrequency(&li);
	__int64 freq = li.QuadPart;
	__int64 ticks;

	QueryPerformanceCounter(&li);
	// run your app here...

	std::map < std::string, std::vector<double> > rawResults;
	for(int i = 0;i<TIMESLICES;i++){
		electPriceUSDperMWh[i] = 24;
		month = frame.Month[i];
		if(hourOfDay==25){
			hourOfDay = 1;
			dayOfWeek = (dayOfWeek==7) ? 1 : dayOfWeek+1;
		}
		/*
		 * int , int month, int , int ,
			double ,	double , double ,
			double solarRadiationN, double solarRadiationE,
			double solarRadiationS, double solarRadiationW,
			double solarRadiationH,
			double& , double&
		 */
		std::map<std::string, double> hourResults = calculateHour(i+1, //hourOfYear
				month, //month
				dayOfWeek, //dayOfWeek
				hourOfDay,//hourOfDay
				wind[i], //windMps
				temp[i], //temperature
				electPriceUSDperMWh[i],//electPriceUSDperMWh
				radiation[i][0],
				radiation[i][2],
				radiation[i][4],
				radiation[i][6],
				0,//radiation[i][8],//roof is 0 for some reason
				TMT1,//TMT1
				tiHeatCool);//tiHeatCool
		// Store each result type in its own vector.
		for (auto const & kv : hourResults) {
			rawResults[kv.first].push_back(kv.second);
		}
	}

	QueryPerformanceCounter(&li2);

	ticks = li2.QuadPart - li.QuadPart;
	std::cout << "Core calcs of hourly model ran in " << ticks << " ticks" << " (" << format_elapsed((double)ticks / (double)freq) << ")" << std::endl;

	// Factor the raw need results by the distribution efficiencies.
	double a_ht_loss = heating->hvacLossFactor();
	double a_cl_loss = cooling->hvacLossFactor();
	double f_waste = heating->hotcoldWasteFactor();
	double cop = cooling->cop();
	double efficiency_ht = heating->efficiency();

	// Calculate the yearly totals.
	double Qneed_ht_yr = std::accumulate(rawResults["Qneed_ht"].begin(), rawResults["Qneed_ht"].end(), 0.0);
	double Qneed_cl_yr = std::accumulate(rawResults["Qneed_cl"].begin(), rawResults["Qneed_cl"].end(), 0.0);

	double f_dem_ht = std::max(Qneed_ht_yr / (Qneed_cl_yr + Qneed_ht_yr), 0.1);
	double f_dem_cl = std::max((1.0 - f_dem_ht), 0.1);
	
	double eta_dist_ht = 1.0 / (1.0 + a_ht_loss + f_waste/f_dem_ht);
	double eta_dist_cl = 1.0 / (1.0 + a_cl_loss + f_waste/f_dem_cl);

	// Create unary functions to factor the heating and cooling values for use
	// with transform().
	auto factorHeating = [=](double need){return need / eta_dist_ht / efficiency_ht; };
	auto factorCooling = [=](double need){return need / eta_dist_cl / cop; };
	
	// Create containers for factored heating and cooling values.
	std::vector<double> v_Qht_sys(rawResults["Qneed_ht"].size());
	std::vector<double> v_Qcl_sys(rawResults["Qneed_cl"].size());

	// Factor the heating and cooling values.
	std::transform(rawResults["Qneed_ht"].begin(), rawResults["Qneed_ht"].end(), v_Qht_sys.begin(), factorHeating);
	std::transform(rawResults["Qneed_cl"].begin(), rawResults["Qneed_cl"].end(), v_Qcl_sys.begin(), factorCooling);

	// Store the factored results and rename them to match the monthly result names.
	std::map < std::string, std::vector<double> > results;
	std::vector<double> zeroes (rawResults["Qneed_ht"].size(), 0.0);
	
	results["Eelec_ht"] = (heating->energyType() == 1) ? v_Qht_sys : zeroes; // If electric.
	results["Eelec_cl"] = v_Qcl_sys;
	results["Eelec_int_lt"] = rawResults["Q_illum_tot"];
	results["Eelec_ext_lt"] = rawResults["Q_illum_ext_tot"];
	results["Eelec_fan"] = rawResults["Qfan_tot"];
	results["Eelec_pump"] = zeroes;
	results["Eelec_int_plug"] = rawResults["phi_plug"];
	results["Eelec_ext_plug"] = rawResults["externalEquipmentEnergyWperm2"];
	results["Eelec_dhw"] = rawResults["Q_dhw"];
	results["Egas_ht"] = (heating->energyType() != 1) ? v_Qht_sys : zeroes; // If not electric.
	results["Egas_cl"] = zeroes;
	results["Egas_plug"] = zeroes;
	results["Egas_dhw"] = zeroes;

	// Convert to EUI in kWh/m^2
	auto wattsPerHourTokWh = [](double watts){return watts / 1000.0; };

	for (auto & kv : results) {
		std::transform(kv.second.begin(), kv.second.end(), kv.second.begin(), wattsPerHourTokWh);
	}

	// Calculate monthly results
	std::map<std::string, std::vector<double>> monthlyResults;
	for (auto const & kv : results){
		monthlyResults[kv.first] = sumHoursByMonth(kv.second);
	}

	// TODO: Make a flag for the CLI to output hourly by hour or hourly by month.
	//// Output the hourly results.
	//for(int i = 0; i < TIMESLICES; ++i){
	//	std::cout << "Hour: " << i << ", ";
	//	std::cout 
	//		<< results["Qneed_ht"][i] << ", "
	//		<< results["Qneed_cl"][i] << ", "
	//		<< results["Q_illum_tot"][i] << ", "
	//		<< results["Q_illum_ext_tot"][i] << ", "
	//		<< results["Qfan_tot"][i] << ", "
	//		<< results["phi_plug"][i] << ", "
	//		<< results["externalEquipmentEnergyWperm2"][i] << ", "
	//		<< results["Q_dhw"][i];
	//	std::cout << std::endl;
	//}

	//// Output the monthly results.
	//std::cout << "Hourly results by month:" << std::endl;
	//std::cout << "Month, Heat, Cool, IntLights, ExtLights, Fans, IntEquip, ExtEquip, DHW" << std::endl;
	//for (int i = 0; i < 12; ++i){
	//	std::cout << i + 1 << ", ";
	//	std::cout 
	//		<< monthlyResults["Qneed_ht"][i] << ", "
	//		<< monthlyResults["Qneed_cl"][i] << ", "
	//		<< monthlyResults["Q_illum_tot"][i] << ", "
	//		<< monthlyResults["Q_illum_ext_tot"][i] << ", "
	//		<< monthlyResults["Qfan_tot"][i] << ", "
	//		<< monthlyResults["phi_plug"][i] << ", "
	//		<< monthlyResults["externalEquipmentEnergyWperm2"][i] << ", "
	//		<< monthlyResults["Q_dhw"][i];
	//	std::cout << std::endl;
	//}

	// TODO Fix this! Hardcoded values of '0' for things not being calculated is not ideal.
	ISOResults allResults;
	EndUses hourlyEndUsesByMonth[12];
	for (int i = 0; i<12; i++){
#ifdef _OPENSTUDIOS
		hourlyEndUsesByMonth[i].addEndUse(monthlyResults["Eelec_ht"][i], EndUseFuelType::Electricity, EndUseCategoryType::Heating);
		hourlyEndUsesByMonth[i].addEndUse(monthlyResults["Eelec_cl"][i], EndUseFuelType::Electricity, EndUseCategoryType::Cooling);
		hourlyEndUsesByMonth[i].addEndUse(monthlyResults["Eelec_int_lt"][i], EndUseFuelType::Electricity, EndUseCategoryType::InteriorLights);
		hourlyEndUsesByMonth[i].addEndUse(monthlyResults["Eelec_ext_lt"][i], EndUseFuelType::Electricity, EndUseCategoryType::ExteriorLights);
		hourlyEndUsesByMonth[i].addEndUse(monthlyResults["Eelec_fan"][i], EndUseFuelType::Electricity, EndUseCategoryType::Fans);
		hourlyEndUsesByMonth[i].addEndUse(monthlyResults["Eelec_pump"][i], EndUseFuelType::Electricity, EndUseCategoryType::Pumps);
		hourlyEndUsesByMonth[i].addEndUse(monthlyResults["Eelec_int_plug"][i], EndUseFuelType::Electricity, EndUseCategoryType::InteriorEquipment);
		hourlyEndUsesByMonth[i].addEndUse(monthlyResults["Eelec_ext_plug"][i], EndUseFuelType::Electricity, EndUseCategoryType::ExteriorEquipment);
		hourlyEndUsesByMonth[i].addEndUse(monthlyResults["Eelec_dhw"][i], EndUseFuelType::Electricity, EndUseCategoryType::WaterSystems);

		hourlyEndUsesByMonth[i].addEndUse(monthlyResults["Egas_ht"][i], EndUseFuelType::Gas, EndUseCategoryType::Heating);
		hourlyEndUsesByMonth[i].addEndUse(monthlyResults["Egas_cl"][i], EndUseFuelType::Gas, EndUseCategoryType::Cooling);
		hourlyEndUsesByMonth[i].addEndUse(monthlyResults["Egas_plug"][i], EndUseFuelType::Gas, EndUseCategoryType::InteriorEquipment);
		hourlyEndUsesByMonth[i].addEndUse(monthlyResults["Egas_dhw"][i], EndUseFuelType::Gas, EndUseCategoryType::WaterSystems);
#else
		int euse = 0;
		hourlyEndUsesByMonth[i].addEndUse(euse++, monthlyResults["Eelec_ht"][i]);
		hourlyEndUsesByMonth[i].addEndUse(euse++, monthlyResults["Eelec_cl"][i]);
		hourlyEndUsesByMonth[i].addEndUse(euse++, monthlyResults["Eelec_int_lt"][i]);
		hourlyEndUsesByMonth[i].addEndUse(euse++, monthlyResults["Eelec_ext_lt"][i]);
		hourlyEndUsesByMonth[i].addEndUse(euse++, monthlyResults["Eelec_fan"][i]);
		hourlyEndUsesByMonth[i].addEndUse(euse++, monthlyResults["Eelec_pump"][i]);
		hourlyEndUsesByMonth[i].addEndUse(euse++, monthlyResults["Eelec_int_plug"][i]);
		hourlyEndUsesByMonth[i].addEndUse(euse++, monthlyResults["Eelec_ext_plug"][i]);
		hourlyEndUsesByMonth[i].addEndUse(euse++, monthlyResults["Eelec_dhw"][i]);
		hourlyEndUsesByMonth[i].addEndUse(euse++, monthlyResults["Egas_ht"][i]);
		hourlyEndUsesByMonth[i].addEndUse(euse++, monthlyResults["Egas_cl"][i]);
		hourlyEndUsesByMonth[i].addEndUse(euse++, monthlyResults["Egas_plug"][i]);
		hourlyEndUsesByMonth[i].addEndUse(euse++, monthlyResults["Egas_dhw"][i]);
#endif
		allResults.hourlyResultsByMonth.push_back(hourlyEndUsesByMonth[i]);
	}
	return allResults;

}

}
}
