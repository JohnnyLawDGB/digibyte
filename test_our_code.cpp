#include "src/consensus/digidollar_transaction_validation.h"
#include "src/consensus/digidollar.h"
#include <iostream>

int main() {
    std::cout << "Testing DigiDollar Transaction Validation (GREEN phase)..." << std::endl;

    // Test mint amount validation
    CAmount validAmount = 1000 * DigiDollar::CENT;
    CAmount tooSmall = 50 * DigiDollar::CENT;
    CAmount tooLarge = 200000 * DigiDollar::CENT;

    std::cout << "Mint amount validation:" << std::endl;
    std::cout << "  Valid amount ($1000): " << (ValidateMintAmount(validAmount) ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Too small ($50): " << (!ValidateMintAmount(tooSmall) ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Too large ($200k): " << (!ValidateMintAmount(tooLarge) ? "PASS" : "FAIL") << std::endl;

    // Test oracle price validation
    std::cout << "\nOracle price validation:" << std::endl;
    std::cout << "  Valid price (5000): " << (ValidateOraclePrice(5000) ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Zero price: " << (!ValidateOraclePrice(0) ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Negative price: " << (!ValidateOraclePrice(-100) ? "PASS" : "FAIL") << std::endl;

    // Test DD conservation
    std::cout << "\nDD conservation validation:" << std::endl;
    std::cout << "  Balanced (1000 = 900 + 100): " << (ValidateDDConservation(1000, 900, 100) ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Insufficient inputs: " << (!ValidateDDConservation(800, 900, 100) ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Excessive inputs: " << (!ValidateDDConservation(1200, 900, 100) ? "PASS" : "FAIL") << std::endl;

    // Test address validation
    std::cout << "\nDD address validation:" << std::endl;
    std::cout << "  Valid DD address: " << (ValidateDDAddress("dd1qw508d6qejxtdg4y5r3zarvary0c5xw7k3k4k4k") ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Invalid address: " << (!ValidateDDAddress("invalid_address") ? "PASS" : "FAIL") << std::endl;

    // Test ERR activation
    std::cout << "\nERR activation:" << std::endl;
    std::cout << "  Low collateral (90%): " << (ShouldActivateERR(90) ? "PASS" : "FAIL") << std::endl;
    std::cout << "  High collateral (150%): " << (!ShouldActivateERR(150) ? "PASS" : "FAIL") << std::endl;

    std::cout << "\nGREEN phase validation complete!" << std::endl;
    return 0;
}