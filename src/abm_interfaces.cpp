#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "abm_interfaces.h"
#include "sample.h"

DataList::DataList() : begin(0), step(0) {

}

DataList::DataList(std::ifstream &input,
        unsigned int begin, unsigned int step) : begin(begin), step(step) {
    std::string tmp;
    while(std::getline(input, tmp)) {
        if(tmp != "") {
            double val = atof(tmp.c_str());
            data.push_back(val);
        }
    }
}

void DataList::adjustData(double multiplicationFactor) {
    for(std::vector<double>::iterator it = data.begin(); it != data.end(); it++) {
        *it *= multiplicationFactor;
    }
}

DataList::~DataList() {

}

double DataList::lookup(int wavelength) const {
    unsigned int ind = (wavelength - begin) / (double)step;
    if(ind < 0) {
        std::cerr << "Warning, wavelength " << wavelength << " is below data point, clamping" << std::endl;
        ind = 0;
    } else if( ind >= data.size()) {
        std::cerr << "Warning, wavelength " << wavelength << " is above data point, clamping" << std::endl;
        ind = data.size() - 1;
    }
    return data[ind];
}

std::ostream& operator<<(std::ostream& os, const ABMInterface& x)
{
    os << "{" << std::endl;
    os << "name: " << x.name << std::endl;
    os << "nAbove: " << x.nAbove << std::endl;
    os << "nBelow: " << x.nBelow << std::endl;
    os << "perturbanceDownAbove: " << x.perturbanceDownAbove << std::endl;
    os << "perturbanceDownBelow: " << x.perturbanceDownBelow << std::endl;
    os << "perturbanceUpAbove: " << x.perturbanceUpAbove << std::endl;
    os << "perturbanceUpBelow: " << x.perturbanceUpBelow << std::endl;
    os << "aborptionAbove: " << x.absorptionAbove << std::endl;
    os << "aborptionBelow:" << x.absorptionBelow << std::endl;
    os << "thicknessAbove:" << x.thicknessAbove << std::endl;
    os << "thicknessBelow:" << x.thicknessBelow << std::endl;
    os << "}";

    return os;
}

ABMInterfaceListBuilder::ABMInterfaceListBuilder(const std::string &dataDirectory)  :
    dataDirectory(dataDirectory)
{
    readAllData(dataDirectory);
}

void ABMInterfaceListBuilder::readAllData(const std::string &dataDirectory) {
    const std::string carotenoidAbsorptionFilename("caro-PAS-400-2500.txt");
    const std::string celluloseAbsorptionFilename("cellulose400-2500.txt");
    const std::string chlorophyllAbsorptionFilename("chloAB-DFA-400-2500.txt");
    const std::string proteinAbsorptionFilename("protein400-2500.txt");
    const std::string waterAbsorptionFilename("sacwH400-2500.txt");
    const std::string mesophyllRefractiveIndexFilename("rmH400-2500.txt");
    const std::string cuticleRefractiveIndexFilename("rcH400-2500.txt"); 
    const std::string antidermalRefractiveIndexFilename("raH400-2500.txt"); 

    readData(dataDirectory + "/" + carotenoidAbsorptionFilename, carotenoidAbsorption);
    readData(dataDirectory + "/" + celluloseAbsorptionFilename, celluloseAbsorption);
    readData(dataDirectory + "/" + chlorophyllAbsorptionFilename, chlorophyllAbsorption);
    readData(dataDirectory + "/" + proteinAbsorptionFilename, proteinAbsorption);
    readData(dataDirectory + "/" + waterAbsorptionFilename, waterAbsorption);
    readData(dataDirectory + "/" + mesophyllRefractiveIndexFilename, mesophyllRefractiveIndex);
    readData(dataDirectory + "/" + cuticleRefractiveIndexFilename, cuticleRefractiveIndex);
    readData(dataDirectory + "/" + antidermalRefractiveIndexFilename, antidermalRefractiveIndex);

    carotenoidAbsorption.adjustData(1.0 / 10.0);
    celluloseAbsorption.adjustData(1.0 / 10.0);
    chlorophyllAbsorption.adjustData(1.0 / 10.0);
    proteinAbsorption.adjustData(1.0 / 10.0);
    waterAbsorption.adjustData(100);
}

void ABMInterfaceListBuilder::readData(std::string filename, DataList &dlist) {
    const double step  = 5;
    const double begin = 400;
    std::ifstream f(filename.c_str());
    if(f.is_open()) {
        std::cerr << "Reading in " << filename << std::endl;
        dlist = DataList(f, begin, step);
    } else {
        throw std::runtime_error("Could not find "  + filename);
    }

    f.close();
}

InterfaceList* ABMInterfaceListBuilder::buildInterfaces(const Sample &sample, int wavelength) {
   const double cmg_to_mkg = 1000; //g/cm^3 to kg/m^3 
   double proteinAbsorptionCoefficient     = sample.proteinConcentration * cmg_to_mkg * proteinAbsorption.lookup(wavelength);
   double chlorophyllAbsorptionCoefficient = (sample.chlorophyllAConcentration + sample.chlorophyllBConcentration)  
       * cmg_to_mkg * chlorophyllAbsorption.lookup(wavelength);
   double carotenoidAbsorptionCoefficient = sample.carotenoidConcentration * cmg_to_mkg * carotenoidAbsorption.lookup(wavelength);
   double celluloseAbsorptionCoefficient = sample.celluloseConcentration * cmg_to_mkg * celluloseAbsorption.lookup(wavelength);
   double linginAbsorptionCoefficient = sample.linginConcentration * cmg_to_mkg * celluloseAbsorption.lookup(wavelength); //Use cellulose for lingin due to lack of data
   double waterAbsorptionCoefficient = waterAbsorption.lookup(wavelength);
   double mesophyllAbsorption = proteinAbsorptionCoefficient + chlorophyllAbsorptionCoefficient + 
       carotenoidAbsorptionCoefficient + celluloseAbsorptionCoefficient + linginAbsorptionCoefficient +
       waterAbsorptionCoefficient;

   double cuticleRI = cuticleRefractiveIndex.lookup(wavelength);
   double mesophyllRI = mesophyllRefractiveIndex.lookup(wavelength);
   double antidermalRI = antidermalRefractiveIndex.lookup(wavelength);
   double airRI = 1.0;

   double mesophyllThickness = sample.mesophyllFraction * sample.wholeLeafThickness;

   return createInterfaceList(sample, airRI, cuticleRI, mesophyllRI, antidermalRI, mesophyllAbsorption, mesophyllThickness);
}

InterfaceList *ABMInterfaceListBuilder::createInterfaceList(const Sample &sample, double airRI,
                        double cuticleRI, double mesophyllRI, double antidermalRI,
                        double mesophyllAbsorption, double mesophyllThickness) {
    return NULL;
}
