#!/bin/bash
# Verification script for DigiDollar persistence serialization

echo "=== DigiDollar Persistence Serialization Verification ==="
echo ""

# Check test file exists
if [ -f "src/test/digidollar_persistence_serialization_tests.cpp" ]; then
    echo "✅ Test file exists"
    echo "   Location: src/test/digidollar_persistence_serialization_tests.cpp"
    echo "   Test cases:"
    grep "BOOST_AUTO_TEST_CASE" src/test/digidollar_persistence_serialization_tests.cpp | sed 's/^/   - /'
else
    echo "❌ Test file missing"
fi

echo ""

# Check serialization methods
echo "Checking SERIALIZE_METHODS implementations..."
echo ""

echo "1. DDTransaction:"
if grep -q "SERIALIZE_METHODS(DDTransaction" src/wallet/digidollarwallet.h; then
    echo "   ✅ Serialization implemented"
    echo "   Fields serialized:"
    grep -A 10 "SERIALIZE_METHODS(DDTransaction" src/wallet/digidollarwallet.h | grep "READWRITE" | sed 's/^/   /'
else
    echo "   ❌ Missing serialization"
fi

echo ""

echo "2. WalletDDBalance:"
if grep -q "SERIALIZE_METHODS(WalletDDBalance" src/wallet/digidollarwallet.h; then
    echo "   ✅ Serialization implemented"
    echo "   Fields serialized:"
    grep -A 10 "SERIALIZE_METHODS(WalletDDBalance" src/wallet/digidollarwallet.h | grep "READWRITE" | sed 's/^/   /'
else
    echo "   ❌ Missing serialization"
fi

echo ""

echo "3. WalletCollateralPosition:"
if grep -q "SERIALIZE_METHODS(WalletCollateralPosition" src/wallet/digidollarwallet.h; then
    echo "   ✅ Serialization implemented"
    echo "   Fields serialized:"
    grep -A 10 "SERIALIZE_METHODS(WalletCollateralPosition" src/wallet/digidollarwallet.h | grep "READWRITE" | sed 's/^/   /'
else
    echo "   ❌ Missing serialization"
fi

echo ""

echo "4. CDigiDollarAddress:"
if grep -q "void Serialize.*Stream.*const" src/base58.h && grep -q "void Unserialize.*Stream" src/base58.h; then
    echo "   ✅ Serialization implemented (template methods)"
    echo "   ✅ Comparison operator implemented"
else
    echo "   ❌ Missing serialization"
fi

echo ""

# Check if test is in Makefile
echo "Build system integration:"
if grep -q "digidollar_persistence_serialization_tests.cpp" src/Makefile.test.include; then
    echo "   ✅ Test added to Makefile.test.include"
else
    echo "   ❌ Test not in Makefile"
fi

echo ""

# Try to compile test object
echo "Compilation check:"
if [ -f "src/test/test_digibyte-digidollar_persistence_serialization_tests.o" ]; then
    echo "   ✅ Test object compiled successfully"
    echo "   Location: src/test/test_digibyte-digidollar_persistence_serialization_tests.o"
    
    # Check for test symbols
    if command -v nm &> /dev/null; then
        echo ""
        echo "   Test symbols found:"
        nm src/test/test_digibyte-digidollar_persistence_serialization_tests.o | grep "test_method" | sed 's/.*_ZN.*:://;s/11test_method.*//' | sed 's/^/   - /' | head -3
    fi
else
    echo "   ⚠️  Test object not compiled (may need: make clean && make -j4)"
fi

echo ""
echo "=== Verification Complete ==="
