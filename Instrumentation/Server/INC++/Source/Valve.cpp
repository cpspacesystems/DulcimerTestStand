#include "Valve.h"
#include <iostream>
#include <vector>

// Constructor
Valve::Valve(const std::string& name, int dataframeIndex, int relayPin, bool enabled)
    : name(name), dataframeIndex(dataframeIndex), relayPin(relayPin), enabled(enabled) {}

// Getters
std::string Valve::getName() const { return name; }
int Valve::getDataframeIndex() const { return dataframeIndex; }
int Valve::getRelayPin() const { return relayPin; }
bool Valve::isEnabled() const { return enabled; }
const std::vector<bool>& Valve::getValues() const { return values; }

// Setters
void Valve::setName(const std::string& name) { this->name = name; }
void Valve::setDataframeIndex(int index) { dataframeIndex = index; }
void Valve::setRelayPin(int pin) { relayPin = pin; }
void Valve::setEnabled(bool state) { enabled = state; }
void Valve::addValue(bool value) { values.push_back(value); }

// Method to clear values
void Valve::clearValues() { values.clear(); }

// Print method for debugging
void Valve::print() const {
    std::cout << "Label: " << name << "\n"
        << "  Dataframe Index: " << dataframeIndex << "\n"
        << "  Relay Pin: " << relayPin << "\n"
        << "  Enabled: " << (enabled ? "true" : "false") << "\n"
        << "  Values: ";
    for (const auto& value : values) {
        std::cout << (value ? "true" : "false") << " ";
    }
    std::cout << "\n";
}