#ifndef SENSOR_H
#define SENSOR_H

#include <string>
#include <vector>

class Sensor {
public:
    // Constructor
    Sensor(const std::string& name, const std::string& type, int dataframeIndex, int minValue, int maxValue, const std::string& units);

    // Getters
    std::string getName() const;
    std::string getType() const;
    int getDataframeIndex() const;
    const std::vector<float>& getValues() const;
    int getMinValue() const;
    int getMaxValue() const;
    std::string getUnits() const;

    // Setters
    void setName(const std::string& name);
    void setType(const std::string& type);
    void setDataframeIndex(int index);
    void addValue(float value);
    void setMinValue(int minValue);
    void setMaxValue(int maxValue);
    void setUnits(const std::string& units);

    // Method to clear sensor values
    void clearValues();

    // Print method for debugging
    void print() const;

private:
    std::string name;
    std::string type;
    int dataframeIndex;
    int minValue;
    int maxValue;
    std::string units;
    std::vector<float> values;
};

#endif // SENSOR_H