#include "Sensor.h"
#include <iostream>

// Constructor
Sensor::Sensor(const std::string& name, const std::string& type, int dataframeIndex, int minValue, int maxValue, const std::string& units)
    : name(name), type(type), dataframeIndex(dataframeIndex), minValue(minValue), maxValue(maxValue), units(units) {}

// Getters
std::string Sensor::getName() const { return name; }
std::string Sensor::getType() const { return type; }
int Sensor::getDataframeIndex() const { return dataframeIndex; }
const std::vector<float>& Sensor::getValues() const { return values; }
int Sensor::getMinValue() const { return minValue; }
int Sensor::getMaxValue() const { return maxValue; }
std::string Sensor::getUnits() const { return units; }

// Setters
void Sensor::setName(const std::string& name) { this->name = name; }
void Sensor::setType(const std::string& type) { this->type = type; }
void Sensor::setDataframeIndex(int index) { dataframeIndex = index; }
void Sensor::addValue(float value) { values.push_back(value); }
void Sensor::setMinValue(int minValue) { this->minValue = minValue; }
void Sensor::setMaxValue(int maxValue) { this->maxValue = maxValue; }
void Sensor::setUnits(const std::string& units) { this->units = units; }

// Method to clear sensor values
void Sensor::clearValues() {
    values.clear();
}

// Print method for debugging
void Sensor::print() const {
    std::cout << "Name: " << name << "\n"
        << "  Type: " << type << "\n"
        << "  Dataframe Index: " << dataframeIndex << "\n"
        << "  Min Value: " << minValue << "\n"
        << "  Max Value: " << maxValue << "\n"
        << "  Units: " << units << "\n"
        << "  Values: ";
    for (const auto& value : values) {
        std::cout << value << " ";
    }
    std::cout << "\n";
}