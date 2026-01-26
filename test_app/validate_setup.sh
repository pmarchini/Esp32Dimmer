#!/bin/bash

# ESP32 Dimmer Test Setup Validation Script
# This script checks if the environment is properly configured for running tests

set -e

echo "========================================="
echo "ESP32 Dimmer Test Setup Validator"
echo "========================================="
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

errors=0
warnings=0

# Function to print status
check_pass() {
    echo -e "${GREEN}✓${NC} $1"
}

check_fail() {
    echo -e "${RED}✗${NC} $1"
    ((errors++))
}

check_warn() {
    echo -e "${YELLOW}⚠${NC} $1"
    ((warnings++))
}

echo "Checking prerequisites..."
echo ""

# Check Python
if command -v python3 &> /dev/null; then
    PYTHON_VERSION=$(python3 --version | cut -d' ' -f2)
    check_pass "Python 3 found: $PYTHON_VERSION"
else
    check_fail "Python 3 not found"
fi

# Check IDF_PATH
if [ -n "$IDF_PATH" ]; then
    check_pass "IDF_PATH is set: $IDF_PATH"
    
    # Check if IDF_PATH exists
    if [ -d "$IDF_PATH" ]; then
        check_pass "ESP-IDF directory exists"
        
        # Check for idf.py
        if [ -f "$IDF_PATH/tools/idf.py" ]; then
            check_pass "idf.py found"
        else
            check_fail "idf.py not found in IDF_PATH"
        fi
    else
        check_fail "IDF_PATH directory does not exist"
    fi
else
    check_fail "IDF_PATH not set"
    echo "  Run: . \$HOME/esp/esp-idf/export.sh"
fi

# Check for idf.py in PATH
if command -v idf.py &> /dev/null; then
    check_pass "idf.py is in PATH"
else
    check_warn "idf.py not in PATH (you may need to run export.sh)"
fi

# Check CMake
if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    check_pass "CMake found: $CMAKE_VERSION"
else
    check_fail "CMake not found"
fi

# Check for test_app directory
if [ -d "test_app" ]; then
    check_pass "test_app directory found"
    
    # Check for required files
    if [ -f "test_app/CMakeLists.txt" ]; then
        check_pass "test_app/CMakeLists.txt found"
    else
        check_fail "test_app/CMakeLists.txt not found"
    fi
    
    if [ -f "test_app/main/test_main.c" ]; then
        check_pass "test_app/main/test_main.c found"
    else
        check_fail "test_app/main/test_main.c not found"
    fi
else
    check_fail "test_app directory not found (are you in the repository root?)"
fi

# Check for component directory
if [ -d "src/components/esp32-triac-dimmer-driver" ]; then
    check_pass "Component directory found"
    
    if [ -f "src/components/esp32-triac-dimmer-driver/esp32-triac-dimmer-driver.c" ]; then
        check_pass "Component source file found"
    else
        check_fail "Component source file not found"
    fi
else
    check_fail "Component directory not found"
fi

# Check serial ports
echo ""
echo "Available serial ports:"
if ls /dev/ttyUSB* 1> /dev/null 2>&1; then
    for port in /dev/ttyUSB*; do
        if [ -r "$port" ] && [ -w "$port" ]; then
            check_pass "$(basename $port) (read/write access)"
        else
            check_warn "$(basename $port) (no read/write access - check permissions)"
        fi
    done
elif ls /dev/ttyACM* 1> /dev/null 2>&1; then
    for port in /dev/ttyACM*; do
        if [ -r "$port" ] && [ -w "$port" ]; then
            check_pass "$(basename $port) (read/write access)"
        else
            check_warn "$(basename $port) (no read/write access - check permissions)"
        fi
    done
elif ls /dev/cu.* 1> /dev/null 2>&1; then
    for port in /dev/cu.usbserial-*; do
        if [ -e "$port" ]; then
            check_pass "$(basename $port)"
        fi
    done
else
    check_warn "No serial ports found (ESP32 may not be connected)"
fi

# Summary
echo ""
echo "========================================="
echo "Summary"
echo "========================================="
echo "Errors: $errors"
echo "Warnings: $warnings"
echo ""

if [ $errors -eq 0 ]; then
    if [ $warnings -eq 0 ]; then
        echo -e "${GREEN}✓ All checks passed!${NC}"
        echo ""
        echo "You can now run tests:"
        echo "  cd test_app"
        echo "  idf.py set-target esp32"
        echo "  idf.py build"
        echo "  idf.py -p /dev/ttyUSB0 flash monitor"
    else
        echo -e "${YELLOW}⚠ Setup is OK but has warnings${NC}"
        echo "Check warnings above and resolve if needed"
    fi
    exit 0
else
    echo -e "${RED}✗ Setup has errors${NC}"
    echo "Please resolve the errors above before running tests"
    exit 1
fi
