/**********************************************************************
 *  Copyright (c) 2008-2013, Alliance for Sustainable Energy.
 *  All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 **********************************************************************/

#include "UserModel.hpp"

using namespace std;
namespace openstudio {
namespace isomodel {

UserModel::UserModel() :
    _weather(new WeatherData()), _edata(new EpwData())
{
}

UserModel::~UserModel()
{
}

ISOHourly UserModel::toHourlyModel() const
{
  ISOHourly sim = ISOHourly();
  if (!_valid) {
    return *((ISOHourly*) NULL);
  }
  std::shared_ptr<Population> pop(new Population);
  pop->setDaysStart(_buildingOccupancyFrom);
  pop->setDaysEnd(_buildingOccupancyTo);
  pop->setHoursEnd(_equivFullLoadOccupancyTo);
  pop->setHoursStart(_equivFullLoadOccupancyFrom);
  pop->setDensityOccupied(_peopleDensityOccupied);
  pop->setDensityUnoccupied(_peopleDensityUnoccupied);
  pop->setHeatGainPerPerson(_heatGainPerPerson);
  sim.setPop(pop);

  std::shared_ptr<Building> building(new Building);
  building->setElectricApplianceHeatGainOccupied(_elecPowerAppliancesOccupied);
  building->setElectricApplianceHeatGainUnoccupied(_elecPowerAppliancesUnoccupied);
  building->setLightingOccupancySensor(_lightingOccupancySensorSystem);
  sim.setBuilding(building);

  std::shared_ptr<Cooling> cooling(new Cooling);
  cooling->setCOP(_coolingSystemCOP);
  cooling->setHvacLossFactor(_hvacCoolingLossFactor);
  cooling->setTemperatureSetPointOccupied(_coolingOccupiedSetpoint);
  cooling->setTemperatureSetPointUnoccupied(_coolingUnoccupiedSetpoint);
  sim.setCooling(cooling);

  std::shared_ptr<Heating> heating(new Heating);
  heating->setEfficiency(_heatingSystemEfficiency);
  heating->setHvacLossFactor(_hvacHeatingLossFactor);
  heating->setHotcoldWasteFactor(_hvacWasteFactor);
  heating->setTemperatureSetPointOccupied(_heatingOccupiedSetpoint);
  heating->setTemperatureSetPointUnoccupied(_heatingUnoccupiedSetpoint);
  sim.setHeating(heating);

  std::shared_ptr<Lighting> lighting(new Lighting);
  lighting->setExteriorEnergy(_exteriorLightingPower);
  lighting->setPowerDensityOccupied(_lightingPowerIntensityOccupied);
  lighting->setPowerDensityUnoccupied(_lightingPowerIntensityUnoccupied);
  sim.setLights(lighting);

  std::shared_ptr<Structure> structure(new Structure);
  structure->setFloorArea(_floorArea);
  structure->setBuildingHeight(_buildingHeight);
  structure->setInfiltrationRate(_buildingAirLeakage);
  structure->setInteriorHeatCapacity(_interiorHeatCapacity);
  //directions in the order [S, SE, E, NE, N, NW, W, SW, roof/skylight]
  Vector wallArea(9);
  wallArea[0] = _wallAreaS;
  wallArea[1] = _wallAreaSE;
  wallArea[2] = _wallAreaE;
  wallArea[3] = _wallAreaNE;
  wallArea[4] = _wallAreaN;
  wallArea[5] = _wallAreaNW;
  wallArea[6] = _wallAreaW;
  wallArea[7] = _wallAreaSW;
  wallArea[8] = _roofArea;
  structure->setWallArea(wallArea); //vector
  structure->setWallHeatCapacity(_exteriorHeatCapacity); //??

  Vector wallSolar(9);
  wallSolar[0] = _wallSolarAbsorptionS;
  wallSolar[1] = _wallSolarAbsorptionSE;
  wallSolar[2] = _wallSolarAbsorptionE;
  wallSolar[3] = _wallSolarAbsorptionNE;
  wallSolar[4] = _wallSolarAbsorptionN;
  wallSolar[5] = _wallSolarAbsorptionNW;
  wallSolar[6] = _wallSolarAbsorptionW;
  wallSolar[7] = _wallSolarAbsorptionSW;
  wallSolar[8] = _roofSolarAbsorption;
  structure->setWallSolarAbsorbtion(wallSolar); //vector

  Vector wallTherm(9);
  wallTherm[0] = _wallThermalEmissivityS;
  wallTherm[1] = _wallThermalEmissivitySE;
  wallTherm[2] = _wallThermalEmissivityE;
  wallTherm[3] = _wallThermalEmissivityNE;
  wallTherm[4] = _wallThermalEmissivityN;
  wallTherm[5] = _wallThermalEmissivityNW;
  wallTherm[6] = _wallThermalEmissivityW;
  wallTherm[7] = _wallThermalEmissivitySW;
  wallTherm[8] = _roofThermalEmissivity;
  structure->setWallThermalEmissivity(wallTherm); //vector

  Vector wallU(9);
  wallU[0] = _wallUvalueS;
  wallU[1] = _wallUvalueSE;
  wallU[2] = _wallUvalueE;
  wallU[3] = _wallUvalueNE;
  wallU[4] = _wallUvalueN;
  wallU[5] = _wallUvalueNW;
  wallU[6] = _wallUvalueW;
  wallU[7] = _wallUvalueSW;
  wallU[8] = _roofUValue;
  structure->setWallUniform(wallU); //vector

  Vector windowArea(9);
  windowArea[0] = _windowAreaS;
  windowArea[1] = _windowAreaSE;
  windowArea[2] = _windowAreaE;
  windowArea[3] = _windowAreaNE;
  windowArea[4] = _windowAreaN;
  windowArea[5] = _windowAreaNW;
  windowArea[6] = _windowAreaW;
  windowArea[7] = _windowAreaSW;
  windowArea[8] = _skylightArea;
  structure->setWindowArea(windowArea); //vector

  Vector winSHGC(9);
  winSHGC[0] = _windowSHGCS;
  winSHGC[1] = _windowSHGCSE;
  winSHGC[2] = _windowSHGCE;
  winSHGC[3] = _windowSHGCNE;
  winSHGC[4] = _windowSHGCN;
  winSHGC[5] = _windowSHGCNW;
  winSHGC[6] = _windowSHGCW;
  winSHGC[7] = _windowSHGCSW;
  winSHGC[8] = _skylightSHGC;
  structure->setWindowNormalIncidenceSolarEnergyTransmittance(winSHGC); //vector

  Vector winSCF(9);
  winSCF[0] = _windowSCFS;
  winSCF[1] = _windowSCFSE;
  winSCF[2] = _windowSCFE;
  winSCF[3] = _windowSCFNE;
  winSCF[4] = _windowSCFN;
  winSCF[5] = _windowSCFNW;
  winSCF[6] = _windowSCFW;
  winSCF[7] = _windowSCFSW;
  winSCF[8] = _windowSCFN;
  structure->setWindowShadingCorrectionFactor(winSCF); //vector
  structure->setWindowShadingDevice(_windowSDFN);

  Vector winU(9);
  winU[0] = _windowUvalueS;
  winU[1] = _windowUvalueSE;
  winU[2] = _windowUvalueE;
  winU[3] = _windowUvalueNE;
  winU[4] = _windowUvalueN;
  winU[5] = _windowUvalueNW;
  winU[6] = _windowUvalueW;
  winU[7] = _windowUvalueSW;
  winU[8] = _skylightUvalue;
  structure->setWindowUniform(winU); //vector
  sim.setStructure(structure);

  std::shared_ptr<Ventilation> ventilation(new Ventilation);
  ventilation->setExhaustAirRecirculated(_exhaustAirRecirclation);
  ventilation->setFanControlFactor(_fanFlowControlFactor);
  ventilation->setFanPower(_specificFanPower);
  ventilation->setHeatRecoveryEfficiency(_heatRecovery);
  ventilation->setSupplyDifference(_supplyExhaustRate);
  ventilation->setSupplyRate(_freshAirFlowRate);
  sim.setVentilation(ventilation);
  sim.setWeatherData(_edata);

  return sim;
}

SimModel UserModel::toSimModel() const
{

  SimModel sim;

  if (!valid()) {
    std::cout << "Invalid" << std::endl;
    return *((SimModel*) NULL);
  }

  std::shared_ptr<Population> pop(new Population());
  pop->setDaysStart(_buildingOccupancyFrom);
  pop->setDaysEnd(_buildingOccupancyTo);
  pop->setHoursEnd(_equivFullLoadOccupancyTo);
  pop->setHoursStart(_equivFullLoadOccupancyFrom);
  pop->setDensityOccupied(_peopleDensityOccupied);
  pop->setDensityUnoccupied(_peopleDensityUnoccupied);
  pop->setHeatGainPerPerson(_heatGainPerPerson);
  sim.setPop(pop);

  std::shared_ptr<Location> loc(new Location);
  loc->setTerrain(_terrainClass);
  loc->setWeatherData(_weather);
  sim.setLocation(loc);

  std::shared_ptr<Building> building(new Building);
  building->setBuildingEnergyManagement(_bemType);
  building->setConstantIllumination(_constantIlluminationControl);
  building->setElectricApplianceHeatGainOccupied(_elecPowerAppliancesOccupied);
  building->setElectricApplianceHeatGainUnoccupied(_elecPowerAppliancesUnoccupied);
  building->setGasApplianceHeatGainOccupied(_gasPowerAppliancesOccupied);
  building->setGasApplianceHeatGainUnoccupied(_gasPowerAppliancesUnoccupied);
  building->setLightingOccupancySensor(_lightingOccupancySensorSystem);
  sim.setBuilding(building);

  std::shared_ptr<Cooling> cooling(new Cooling);
  cooling->setCOP(_coolingSystemCOP);
  cooling->setHvacLossFactor(_hvacCoolingLossFactor);
  cooling->setPartialLoadValue(_coolingSystemIPLVToCOPRatio);
  cooling->setPumpControlReduction(_coolingPumpControl);
  cooling->setTemperatureSetPointOccupied(_coolingOccupiedSetpoint);
  cooling->setTemperatureSetPointUnoccupied(_coolingUnoccupiedSetpoint);
  sim.setCooling(cooling);

  std::shared_ptr<Heating> heating(new Heating);
  heating->setEfficiency(_heatingSystemEfficiency);
  heating->setEnergyType(_heatingEnergyCarrier);
  heating->setHotcoldWasteFactor(_hvacWasteFactor); // Used in hvac distribution efficiency.
  heating->setHotWaterDemand(_dhwDemand);
  heating->setHotWaterDistributionEfficiency(_dhwDistributionEfficiency);
  heating->setHotWaterEnergyType(_dhwEnergyCarrier);
  heating->setHotWaterSystemEfficiency(_dhwEfficiency);
  heating->setHvacLossFactor(_hvacHeatingLossFactor);
  heating->setPumpControlReduction(_heatingPumpControl);
  heating->setTemperatureSetPointOccupied(_heatingOccupiedSetpoint);
  heating->setTemperatureSetPointUnoccupied(_heatingUnoccupiedSetpoint);
  sim.setHeating(heating);

  std::shared_ptr<Lighting> lighting(new Lighting);
  lighting->setDimmingFraction(_daylightSensorSystem);
  lighting->setExteriorEnergy(_exteriorLightingPower);
  lighting->setPowerDensityOccupied(_lightingPowerIntensityOccupied);
  lighting->setPowerDensityUnoccupied(_lightingPowerIntensityUnoccupied);
  sim.setLights(lighting);

  std::shared_ptr<Structure> structure(new Structure);
  structure->setFloorArea(_floorArea);
  structure->setBuildingHeight(_buildingHeight);
  structure->setInfiltrationRate(_buildingAirLeakage);
  structure->setInteriorHeatCapacity(_interiorHeatCapacity);
  //directions in the order [S, SE, E, NE, N, NW, W, SW, roof/skylight]
  Vector wallArea(9);
  wallArea[0] = _wallAreaS;
  wallArea[1] = _wallAreaSE;
  wallArea[2] = _wallAreaE;
  wallArea[3] = _wallAreaNE;
  wallArea[4] = _wallAreaN;
  wallArea[5] = _wallAreaNW;
  wallArea[6] = _wallAreaW;
  wallArea[7] = _wallAreaSW;
  wallArea[8] = _roofArea;
  structure->setWallArea(wallArea); //vector
  structure->setWallHeatCapacity(_exteriorHeatCapacity); //??

  Vector wallSolar(9);
  wallSolar[0] = _wallSolarAbsorptionS;
  wallSolar[1] = _wallSolarAbsorptionSE;
  wallSolar[2] = _wallSolarAbsorptionE;
  wallSolar[3] = _wallSolarAbsorptionNE;
  wallSolar[4] = _wallSolarAbsorptionN;
  wallSolar[5] = _wallSolarAbsorptionNW;
  wallSolar[6] = _wallSolarAbsorptionW;
  wallSolar[7] = _wallSolarAbsorptionSW;
  wallSolar[8] = _roofSolarAbsorption;
  structure->setWallSolarAbsorbtion(wallSolar); //vector

  Vector wallTherm(9);
  wallTherm[0] = _wallThermalEmissivityS;
  wallTherm[1] = _wallThermalEmissivitySE;
  wallTherm[2] = _wallThermalEmissivityE;
  wallTherm[3] = _wallThermalEmissivityNE;
  wallTherm[4] = _wallThermalEmissivityN;
  wallTherm[5] = _wallThermalEmissivityNW;
  wallTherm[6] = _wallThermalEmissivityW;
  wallTherm[7] = _wallThermalEmissivitySW;
  wallTherm[8] = _roofThermalEmissivity;
  structure->setWallThermalEmissivity(wallTherm); //vector

  Vector wallU(9);
  wallU[0] = _wallUvalueS;
  wallU[1] = _wallUvalueSE;
  wallU[2] = _wallUvalueE;
  wallU[3] = _wallUvalueNE;
  wallU[4] = _wallUvalueN;
  wallU[5] = _wallUvalueNW;
  wallU[6] = _wallUvalueW;
  wallU[7] = _wallUvalueSW;
  wallU[8] = _roofUValue;
  structure->setWallUniform(wallU); //vector

  Vector windowArea(9);
  windowArea[0] = _windowAreaS;
  windowArea[1] = _windowAreaSE;
  windowArea[2] = _windowAreaE;
  windowArea[3] = _windowAreaNE;
  windowArea[4] = _windowAreaN;
  windowArea[5] = _windowAreaNW;
  windowArea[6] = _windowAreaW;
  windowArea[7] = _windowAreaSW;
  windowArea[8] = _skylightArea;
  structure->setWindowArea(windowArea); //vector

  Vector winSHGC(9);
  winSHGC[0] = _windowSHGCS;
  winSHGC[1] = _windowSHGCSE;
  winSHGC[2] = _windowSHGCE;
  winSHGC[3] = _windowSHGCNE;
  winSHGC[4] = _windowSHGCN;
  winSHGC[5] = _windowSHGCNW;
  winSHGC[6] = _windowSHGCW;
  winSHGC[7] = _windowSHGCSW;
  winSHGC[8] = _skylightSHGC;
  structure->setWindowNormalIncidenceSolarEnergyTransmittance(winSHGC); //vector

  Vector winSCF(9);
  winSCF[0] = _windowSCFS;
  winSCF[1] = _windowSCFSE;
  winSCF[2] = _windowSCFE;
  winSCF[3] = _windowSCFNE;
  winSCF[4] = _windowSCFN;
  winSCF[5] = _windowSCFNW;
  winSCF[6] = _windowSCFW;
  winSCF[7] = _windowSCFSW;
  winSCF[8] = _windowSCFN;
  structure->setWindowShadingCorrectionFactor(winSCF); //vector
  structure->setWindowShadingDevice(_windowSDFN);

  Vector winU(9);
  winU[0] = _windowUvalueS;
  winU[1] = _windowUvalueSE;
  winU[2] = _windowUvalueE;
  winU[3] = _windowUvalueNE;
  winU[4] = _windowUvalueN;
  winU[5] = _windowUvalueNW;
  winU[6] = _windowUvalueW;
  winU[7] = _windowUvalueSW;
  winU[8] = _skylightUvalue;
  structure->setWindowUniform(winU); //vector
  sim.setStructure(structure);

  std::shared_ptr<Ventilation> ventilation(new Ventilation);
  ventilation->setExhaustAirRecirculated(_exhaustAirRecirclation);
  ventilation->setFanControlFactor(_fanFlowControlFactor);
  ventilation->setFanPower(_specificFanPower);
  ventilation->setHeatRecoveryEfficiency(_heatRecovery);
  ventilation->setSupplyDifference(_supplyExhaustRate);
  ventilation->setSupplyRate(_freshAirFlowRate);
  ventilation->setType(_ventilationType);
  ventilation->setWasteFactor(_hvacWasteFactor); //??
  sim.setVentilation(ventilation);
  return sim;
}
//http://stackoverflow.com/questions/10051679/c-tokenize-string
std::vector<std::string> inline stringSplit(const std::string &source, char delimiter = ' ', bool keepEmpty = false)
{
  std::vector<std::string> results;

  size_t prev = 0;
  size_t next = 0;
  if (source.size() == 0)
    return results;
  while ((next = source.find_first_of(delimiter, prev)) != std::string::npos) {
    if (keepEmpty || (next - prev != 0)) {
      results.push_back(source.substr(prev, next - prev));
    }
    prev = next + 1;
  }

  if (prev < source.size()) {
    results.push_back(source.substr(prev));
  }

  return results;
}

// trim from front and back ends
static inline std::string trim(std::string &s)
{
  const char* cstr = s.c_str();
  int start = 0, end = s.size();
  while (*cstr == ' ') {
    cstr++;
    start++;
  }
  cstr += (end - start - 1);
  while (*cstr == ' ') {
    cstr--;
    end--;
  }
  return s.substr(start, end - start);
}
// trim from front and back ends
static inline std::string lcase(std::string &s)
{
  for (unsigned int i = 0; i < s.size(); i++) {
    if (s.at(i) < 91) {
      s.at(i) = s.at(i) + 32;
    }
  }
  return s;
}

void UserModel::initializeStructure(const Properties& buildingParams)
{
  double attributeValue = buildingParams.getPropertyAsDouble("windowuvaluen");
  _windowUvalueN = attributeValue;

  attributeValue = buildingParams.getPropertyAsDouble("windowshgcn");
  _windowSHGCN = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowscfn");
  _windowSCFN = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowsdfn");
  _windowSDFN = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("walluvaluen");
  _wallUvalueN = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallsolarabsorptionn");
  _wallSolarAbsorptionN = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallthermalemissivityn");
  _wallThermalEmissivityN = attributeValue;

  attributeValue = buildingParams.getPropertyAsDouble("windowuvaluene");
  _windowUvalueNE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowshgcne");
  _windowSHGCNE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowscfne");
  _windowSCFNE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowsdfne");
  _windowSDFNE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("walluvaluene");
  _wallUvalueNE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallsolarabsorptionne");
  _wallSolarAbsorptionNE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallthermalemissivityne");
  _wallThermalEmissivityNE = attributeValue;

  attributeValue = buildingParams.getPropertyAsDouble("windowuvaluee");
  _windowUvalueE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowshgce");
  _windowSHGCE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowscfe");
  _windowSCFE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowsdfe");
  _windowSDFE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("walluvaluee");
  _wallUvalueE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallsolarabsorptione");
  _wallSolarAbsorptionE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallthermalemissivitye");
  _wallThermalEmissivityE = attributeValue;

  attributeValue = buildingParams.getPropertyAsDouble("windowuvaluese");
  _windowUvalueSE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowshgcse");
  _windowSHGCSE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowscfse");
  _windowSCFSE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowsdfse");
  _windowSDFSE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("walluvaluese");
  _wallUvalueSE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallsolarabsorptionse");
  _wallSolarAbsorptionSE = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallthermalemissivityse");
  _wallThermalEmissivitySE = attributeValue;

  attributeValue = buildingParams.getPropertyAsDouble("windowuvalues");
  _windowUvalueS = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowshgcs");
  _windowSHGCS = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowscfs");
  _windowSCFS = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowsdfs");
  _windowSDFS = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("walluvalues");
  _wallUvalueS = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallsolarabsorptions");
  _wallSolarAbsorptionS = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallthermalemissivitys");
  _wallThermalEmissivityS = attributeValue;

  attributeValue = buildingParams.getPropertyAsDouble("windowuvaluesw");
  _windowUvalueSW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowshgcsw");
  _windowSHGCSW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowscfsw");
  _windowSCFSW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowsdfsw");
  _windowSDFSW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("walluvaluesw");
  _wallUvalueSW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallsolarabsorptionsw");
  _wallSolarAbsorptionSW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallthermalemissivitysw");
  _wallThermalEmissivitySW = attributeValue;

  attributeValue = buildingParams.getPropertyAsDouble("windowuvaluew");
  _windowUvalueW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowshgcw");
  _windowSHGCW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowscfw");
  _windowSCFW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowsdfw");
  _windowSDFW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("walluvaluew");
  _wallUvalueW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallsolarabsorptionw");
  _wallSolarAbsorptionW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallthermalemissivityw");
  _wallThermalEmissivityW = attributeValue;

  attributeValue = buildingParams.getPropertyAsDouble("windowuvaluenw");
  _windowUvalueNW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowshgcnw");
  _windowSHGCNW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowscfnw");
  _windowSCFNW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("windowsdfnw");
  _windowSDFNW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("walluvaluenw");
  _wallUvalueNW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallsolarabsorptionnw");
  _wallSolarAbsorptionNW = attributeValue;
  attributeValue = buildingParams.getPropertyAsDouble("wallthermalemissivitynw");
  _wallThermalEmissivityNW = attributeValue;
}

void UserModel::initializeParameters(const Properties& buildingParams)
{
  double attributeValue = buildingParams.getPropertyAsDouble("terrainclass");
  setTerrainClass(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("buildingheight");
  setBuildingHeight(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("floorarea");
  setFloorArea(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("occupancydaystart");
  setBuildingOccupancyFrom(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("occupancydayend");
  setBuildingOccupancyTo(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("occupancyhourstart");
  setEquivFullLoadOccupancyFrom(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("occupancyhourend");
  setEquivFullLoadOccupancyTo(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("peopledensityoccupied");
  setPeopleDensityOccupied(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("peopledensityunoccupied");
  setPeopleDensityUnoccupied(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("lightingpowerdensityoccupied");
  setLightingPowerIntensityOccupied(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("lightingpowerdensityunoccupied");
  setLightingPowerIntensityUnoccupied(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("electricappliancepowerdensityoccupied");
  setElecPowerAppliancesOccupied(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("electricappliancepowerdensityunoccupied");
  setElecPowerAppliancesUnoccupied(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("gasappliancepowerdensityoccupied");
  setGasPowerAppliancesOccupied(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("gasappliancepowerdensityunoccupied");
  setGasPowerAppliancesUnoccupied(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("exteriorlightingpower");
  setExteriorLightingPower(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("hvacwastefactor");
  setHvacWasteFactor(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("hvacheatinglossfactor");
  setHvacHeatingLossFactor(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("hvaccoolinglossfactor");
  setHvacCoolingLossFactor(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("daylightsensordimmingfraction");
  setDaylightSensorSystem(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("lightingoccupancysensordimmingfraction");
  setLightingOccupancySensorSystem(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("constantilluminationcontrolmultiplier");	//constantilluminaitoncontrol
  setConstantIlluminationControl(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("coolingsystemcop");
  setCoolingSystemCOP(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("coolingsystemiplvtocopratio");
  setCoolingSystemIPLVToCOPRatio(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("heatingfueltype");
  setHeatingEnergyCarrier(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("heatingsystemefficiency");
  setHeatingSystemEfficiency(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("ventilationtype");
  setVentilationType(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("ventilationintakerateoccupied");
  setFreshAirFlowRate(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("ventilationExhaustRateOccupied");
  setSupplyExhaustRate(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("heatrecovery");
  setHeatRecovery(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("exhaustairrecirculation");
  setExhaustAirRecirclation(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("infiltrationrateoccupied");
  // set both of these to infiltration. Prior version set only the
  // buildingAirLeakage from the infiltration and the _infiltration var
  // wasn't used. Its still not used, but does have a getter and setter
  // so we set it
  setBuildingAirLeakage(attributeValue);
  setInfiltration(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("dhwdemand");
  setDhwDemand(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("dhwsystemefficiency");
  setDhwEfficiency(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("dhwdistributionefficiency");
  setDhwDistributionEfficiency(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("dhwfueltype");
  setDhwEnergyCarrier(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("bemtype");
  setBemType(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("interiorheatcapacity");
  setInteriorHeatCapacity(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("exteriorheatcapacity");
  setExteriorHeatCapacity(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("heatingpumpcontrol");
  setHeatingPumpControl(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("coolingpumpcontrol");
  setCoolingPumpControl(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("heatgainperperson");
  setHeatGainPerPerson(attributeValue);
  //specificFanPower
  attributeValue = buildingParams.getPropertyAsDouble("specificfanpower");
  setSpecificFanPower(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("fanflowcontrolfactor");
  setFanFlowControlFactor(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("roofuvalue");
  setRoofUValue(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("roofsolarabsorption");
  setRoofSolarAbsorption(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("roofthermalemissivity");
  setRoofThermalEmissivity(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("skylightuvalue");
  setSkylightUvalue(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("skylightshgc");
  setSkylightSHGC(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("wallareas");
  setWallAreaS(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("wallarease");
  setWallAreaSE(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("wallareae");
  setWallAreaE(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("wallareane");
  setWallAreaNE(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("wallarean");
  setWallAreaN(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("wallareanw");
  setWallAreaNW(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("wallareaw");
  setWallAreaW(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("wallareasw");
  setWallAreaSW(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("roofarea");
  setRoofArea(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("windowareas");
  setWindowAreaS(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("windowarease");
  setWindowAreaSE(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("windowareae");
  setWindowAreaE(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("windowareane");
  setWindowAreaNE(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("windowarean");
  setWindowAreaN(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("windowareanw");
  setWindowAreaNW(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("windowareaw");
  setWindowAreaW(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("windowareasw");
  setWindowAreaSW(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("skylightarea");
  setSkylightArea(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("coolingsetpointoccupied");
  setCoolingOccupiedSetpoint(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("coolingsetpointunoccupied");
  setCoolingUnoccupiedSetpoint(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("heatingsetpointoccupied");
  setHeatingOccupiedSetpoint(attributeValue);
  attributeValue = buildingParams.getPropertyAsDouble("heatingsetpointunoccupied");	//weatherFilePath
  setHeatingUnoccupiedSetpoint(attributeValue);

  std::string weatherFilePath = buildingParams.getProperty("weatherfilepath");
  if (weatherFilePath.length() == 0) throw invalid_argument("weatherFilePath building parameter is missing");

  setWeatherFilePath(weatherFilePath);
}

void UserModel::loadBuilding(std::string buildingFile)
{
  Properties buildingParams(buildingFile);
  initializeParameters(buildingParams);
  initializeStructure(buildingParams);
}

int UserModel::weatherState(std::string header)
{
  if (!header.compare("solar"))
    return 1;
  else if (!header.compare("hdbt"))
    return 2;
  else if (!header.compare("hEgh"))
    return 3;
  else if (!header.compare("mEgh"))
    return 4;
  else if (!header.compare("mdbt"))
    return 5;
  else if (!header.compare("mwind"))
    return 6;
  else
    return -1;
}
std::string UserModel::resolveFilename(std::string baseFile, std::string relativeFile)
{
  unsigned int lastSeparator = 0;
  unsigned int i = 0;
  const char separatorChar = '/';
  const char winSeparatorChar = '\\';
  std::string result;
  for (; i < baseFile.length(); i++) {
    result += (baseFile[i] == winSeparatorChar) ? separatorChar : baseFile[i];
    if (result[i] == separatorChar) {
      lastSeparator = i;
    }
  }
  result = result.substr(0, lastSeparator + 1);
  unsigned int j = 0;
  if (relativeFile.length() > 0) {
    //if first char is a separator, skip it
    if (relativeFile[0] == separatorChar || relativeFile[0] == winSeparatorChar)
      j++;
  }
  for (; j < relativeFile.length(); j++, i++) {
    result += (relativeFile[j] == winSeparatorChar) ? separatorChar : relativeFile[j];
  }
  return result;
}

void UserModel::loadWeather()
{
  std::string weatherFilename;
  //see if weather file path is absolute path
  //if so, use it, else assemble relative path
  if (boost::filesystem::exists(_weatherFilePath)) {
    weatherFilename = _weatherFilePath;
  } else {
    weatherFilename = resolveFilename(this->dataFile, _weatherFilePath);
    if (!boost::filesystem::exists(weatherFilename)) {
      std::cout << "Weather File Not Found: " << _weatherFilePath << std::endl;
      _valid = false;
    }
  }

  _edata->loadData(weatherFilename);
  initializeSolar();
}

void UserModel::loadAndSetWeather()
{
  loadWeather();
  _valid = true;
}

void UserModel::loadWeather(int block_size, double* weather_data)
{
  _edata->loadData(block_size, weather_data);
  initializeSolar();
  _valid = true;
}

void UserModel::initializeSolar()
{

  int state = 0, row = 0;
  Matrix _msolar(12, 8);
  Matrix _mhdbt(12, 24);
  Matrix _mhEgh(12, 24);
  Vector _mEgh(12);
  Vector _mdbt(12);
  Vector _mwind(12);

  string line;
  std::vector<std::string> linesplit;

  std::stringstream inputFile(_edata->toISOData());

  while (inputFile.good()) {
    getline(inputFile, line);
    if (line.size() > 0 && line[0] == '#')
      continue;
    linesplit = stringSplit(line, ',', true);
    if (linesplit.size() == 0) {
      continue;
    } else if (linesplit.size() == 1) {
      state = weatherState(linesplit[0]);
      row = 0;
    } else if (row < 12) {
      switch (state) {
      case 1: //solar = [12 x 8] mean monthly total solar radiation (W/m2) on a vertical surface for each of the 8 cardinal directions
        for (unsigned int c = 1; c < linesplit.size() && c < 9; c++) {
          _msolar(row, c - 1) = atof(linesplit[c].c_str());
        }
        break;
      case 2: //hdbt = [12 x 24] mean monthly dry bulb temp for each of the 24 hours of the day (C)
        for (unsigned int c = 1; c < linesplit.size() && c < 25; c++) {
          _mhdbt(row, c - 1) = atof(linesplit[c].c_str());
        }
        break;
      case 3: //hEgh =[12 x 24] mean monthly Global Horizontal Radiation for each of the 24 hours of the day (W/m2)
        for (unsigned int c = 1; c < linesplit.size() && c < 25; c++) {
          _mhEgh(row, c - 1) = atof(linesplit[c].c_str());
        }
        break;
      case 4:  //megh = [12 x 1] mean monthly Global Horizontal Radiation (W/m2)
        _mEgh[row] = atof(linesplit[1].c_str());
        break;
      case 5:    //mdbt = [12 x 1] mean monthly dry bulb temp (C)
        _mdbt[row] = atof(linesplit[1].c_str());
        break;
      case 6:    //mwind = [12 x 1] mean monthly wind speed; (m/s) 
        _mwind[row] = atof(linesplit[1].c_str());
        break;
      default:
        break;
      }
      row++;
    }
  }
  _weather->setMdbt(_mdbt);
  _weather->setMEgh(_mEgh);
  _weather->setMhdbt(_mhdbt);
  _weather->setMhEgh(_mhEgh);
  _weather->setMsolar(_msolar);
  _weather->setMwind(_mwind);
}

void UserModel::load(std::string buildingFile)
{
  this->dataFile = buildingFile;
  _valid = true;
  if (!boost::filesystem::exists(buildingFile)) {
    std::cout << "ISO Model File Not Found: " << buildingFile << std::endl;
    _valid = false;
    return;
  }
  if (DEBUG_ISO_MODEL_SIMULATION)
    std::cout << "Loading Building File: " << buildingFile << std::endl;
  loadBuilding(buildingFile);
  if (DEBUG_ISO_MODEL_SIMULATION)
    std::cout << "Loading Weather File: " << this->weatherFilePath() << std::endl;
  loadWeather();
  if (DEBUG_ISO_MODEL_SIMULATION)
    std::cout << "Weather File Loaded" << std::endl;
}

} // isomodel
} // openstudio

