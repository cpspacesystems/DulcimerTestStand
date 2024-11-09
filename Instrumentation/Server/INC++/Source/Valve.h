#ifndef VALVE_H
#define VALVE_H

#include <string>
#include <vector>

class Valve {
public:
    // Constructor
    Valve(const std::string& name, int dataframeIndex, int relayPin, bool enabled);

    // Getters
    std::string getName() const;
    int getDataframeIndex() const;
    int getRelayPin() const;
    bool isEnabled() const;
    const std::vector<bool>& getValues() const;

    // Setters
    void setName(const std::string& name);
    void setDataframeIndex(int index);
    void setRelayPin(int pin);
    void setEnabled(bool state);
    void addValue(bool value);

    // Method to clear values
    void clearValues();

    // Print method for debugging
    void print() const;

private:
    std::string name;
    int dataframeIndex;
    int relayPin;
    bool enabled;
    std::vector<bool> values;
};

#endif // VALVE_H