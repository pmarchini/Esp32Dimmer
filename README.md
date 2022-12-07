# ESP32IDFDimmer

This library provides an API to control dimmer devices using the ESP32IDF. It supports both toggle and normal modes, and allows you to set the power levels of the dimmer. 

### Prerequisites
- ESP32 board with ESP-IDF v4.1 or higher
- A dimmable AC load 

### Installation
Clone the project from the repository and add the library to your project. 

### Usage
1. Include the library header in your program 
```
#include "esp32idfDimmer.h"
```
2. Instantiate the dimmers. 
```
dimmertyp *ptr_dimmer; 
dimmertyp *ptr_dimmer_2; 

ptr_dimmer = createDimmer(TRIAC_1_GPIO, ZEROCROSS_GPIO);
ptr_dimmer_2 = createDimmer(TRIAC_2_GPIO, ZEROCROSS_GPIO);
``` 
3. Start the dimmers. 
```
begin(ptr_dimmer, NORMAL_MODE, ON, _50Hz);
begin(ptr_dimmer_2, NORMAL_MODE, ON, _50Hz);
```
4. Set or get the power of the dimmers. 
```
// Set the power level to 50 
setPower(ptr_dimmer, 50); 

// Get the current power level 
int powerLevel = getPower(ptr_dimmer); 
``` 

## API

The library provides the following API methods:

* `createDimmer` - creates a new dimmer object
* `begin` - starts the dimmer
* `setPower` - sets the power level of the dimmer
* `getPower` - gets the current power level of the dimmer
* `setState` - sets the state of the dimmer (on/off)
* `getState` - gets the current state of the dimmer
* `changeState` - changes the state of the dimmer (on/off)
* `setMode` - sets the mode of the dimmer (toggle/normal)
* `getMode` - gets the current mode of the dimmer
* `toggleSettings` - sets the toggle range of the dimmer

## Example schematics

### Zero-crossing detector

![image](https://user-images.githubusercontent.com/49943249/194775323-f39d7d93-49cd-4882-aff1-6535ebe1c8b8.png)

### Triac command 

![image](https://user-images.githubusercontent.com/49943249/194775053-0badd3f8-0c23-4a86-8843-abe2f994f5b3.png)

## Contributing 

We welcome contributions to this library. Please open a pull request or an issue to get started. 

## License 

This library is released under the MIT License.
