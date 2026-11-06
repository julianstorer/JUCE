/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-9-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

OSCArgument::OSCArgument (int32 v)              : type (OSCTypes::int32),   intValue (v) {}
OSCArgument::OSCArgument (int64 v)              : type (OSCTypes::int64),   int64Value (v) {}
OSCArgument::OSCArgument (float v)              : type (OSCTypes::float32), floatValue (v) {}
OSCArgument::OSCArgument (double v)             : type (OSCTypes::double64),doubleValue (v) {}
OSCArgument::OSCArgument (const String& s)      : type (OSCTypes::string),  stringValue (s) {}
OSCArgument::OSCArgument (MemoryBlock b)        : type (OSCTypes::blob),    blob (std::move (b)) {}
OSCArgument::OSCArgument (OSCColour c)          : type (OSCTypes::colour),  intValue ((int32) c.toInt32()) {}
OSCArgument::OSCArgument (bool b)               : type (b ? OSCTypes::T : OSCTypes::F) {}
OSCArgument::OSCArgument (OSCType t)            : type (t) {} //for nil and impulse


//==============================================================================
String OSCArgument::getString() const noexcept
{
    if (isString())
        return stringValue;

    jassertfalse; // you must check the type of an argument before attempting to get its value!
    return {};
}

int32 OSCArgument::getInt32() const noexcept
{
    if (isInt32())
        return intValue;

    jassertfalse; // you must check the type of an argument before attempting to get its value!
    return 0;
}

int64 OSCArgument::getInt64() const noexcept
{
	if (isInt64())
		return int64Value;

	jassertfalse; // you must check the type of an argument before attempting to get its value!
	return 0;
}

float OSCArgument::getFloat32() const noexcept
{
    if (isFloat32())
        return floatValue;

    jassertfalse; // you must check the type of an argument before attempting to get its value!
    return 0.0f;
}

double OSCArgument::getDouble() const noexcept
{
	if (isDouble())
		return doubleValue;

	jassertfalse; // you must check the type of an argument before attempting to get its value!
	return 0.0;
}

const MemoryBlock& OSCArgument::getBlob() const noexcept
{
    // you must check the type of an argument before attempting to get its value!
    jassert (isBlob());

    return blob;
}

OSCColour OSCArgument::getColour() const noexcept
{
    if (isColour())
        return OSCColour::fromInt32 ((uint32) intValue);

    jassertfalse; // you must check the type of an argument before attempting to get its value!
    return { 0, 0, 0, 0 };
}

bool OSCArgument::getBool() const noexcept
{
	if (isBool())
		return type == OSCTypes::T;

	jassertfalse; // you must check the type of an argument before attempting to get its value!
	return false;
}


//==============================================================================
//==============================================================================
#if JUCE_UNIT_TESTS

class OSCArgumentTests final : public UnitTest
{
public:
    OSCArgumentTests()
         : UnitTest ("OSCArgument class", UnitTestCategories::osc)
    {}


    MemoryBlock getMemoryBlockWithRandomData (size_t numBytes)
    {
        MemoryBlock block (numBytes);

        Random rng = getRandom();

        for (size_t i = 0; i < numBytes; ++i)
            block[i] = (char) rng.nextInt (256);

        return block;
    }

    void runTest() override
    {
        runTestInitialisation();
    }

    void runTestInitialisation()
    {
        beginTest ("Int32");
        {
            int value = 123456789;

            OSCArgument arg (value);

            expect (arg.getType() == OSCTypes::int32);
            expect (arg.isInt32());
            expect (! arg.isInt64());
            expect (! arg.isFloat32());
            expect (! arg.isDouble());
            expect (! arg.isString());
            expect (! arg.isBlob());
            expect (! arg.isColour());
            expect (! arg.isNil());
            expect (! arg.isImpulse());
            expect (! arg.isBool());

            expect (arg.getInt32() == value);
        }

        beginTest("Int64");
        {
            int64 value = 1234567890123456789;

            OSCArgument arg(value);

            expect(arg.getType() == OSCTypes::int64);
            expect (! arg.isInt32());
            expect (arg.isInt64());
            expect (! arg.isFloat32());
            expect (! arg.isDouble());
            expect (! arg.isString());
            expect (! arg.isBlob());
            expect (! arg.isColour());
            expect (! arg.isNil());
            expect (! arg.isImpulse());
            expect (! arg.isBool());

            expect(arg.getInt64() == value);
        }

        beginTest ("Float32");
        {
            float value = 12345.6789f;

            OSCArgument arg (value);

            expect (arg.getType() == OSCTypes::float32);
            expect (! arg.isInt32());
            expect (! arg.isInt64());
            expect (arg.isFloat32());
            expect (! arg.isDouble());
            expect (! arg.isString());
            expect (! arg.isBlob());
            expect (! arg.isColour());
            expect (! arg.isNil());
            expect (! arg.isImpulse());
            expect (! arg.isBool());

            expectEquals (arg.getFloat32(), value);
        }

        beginTest("Double");
		{
			double value = 12345.6789;

			OSCArgument arg(value);

			expect(arg.getType() == OSCTypes::double64);
            expect (! arg.isInt32());
            expect (! arg.isInt64());
            expect (! arg.isFloat32());
            expect (arg.isDouble());
            expect (! arg.isString());
            expect (! arg.isBlob());
            expect (! arg.isColour());
            expect (! arg.isNil());
            expect (! arg.isImpulse());
            expect (! arg.isBool());

			expectEquals(arg.getDouble(), value);
		}

        beginTest ("String");
        {
            String value = "Hello, World!";
            OSCArgument arg (value);

            expect (arg.getType() == OSCTypes::string);
            expect (! arg.isInt32());
            expect (! arg.isInt64());
            expect (! arg.isFloat32());
            expect (! arg.isDouble());
            expect (arg.isString());
            expect (! arg.isBlob());
            expect (! arg.isColour());
            expect (! arg.isNil());
            expect (! arg.isImpulse());
            expect (! arg.isBool());

            expect (arg.getString() == value);
        }

        beginTest ("String (from C string)");
        {
            OSCArgument arg ("Hello, World!");

            expect (arg.getType() == OSCTypes::string);
            expect (! arg.isInt32());
            expect (! arg.isInt64());
            expect (! arg.isFloat32());
            expect (! arg.isDouble());
            expect (arg.isString());
            expect (! arg.isBlob());
            expect (! arg.isColour());
            expect (! arg.isNil());
            expect (! arg.isImpulse());
            expect (! arg.isBool());

            expect (arg.getString() == "Hello, World!");
        }

        beginTest ("Blob");
        {
            auto blob = getMemoryBlockWithRandomData (413);
            OSCArgument arg (blob);

            expect (arg.getType() == OSCTypes::blob);
            expect (! arg.isInt32());
            expect (! arg.isInt64());
            expect (! arg.isFloat32());
            expect (! arg.isDouble());
            expect (! arg.isString());
            expect (arg.isBlob());
            expect (! arg.isColour());
            expect (! arg.isNil());
            expect (! arg.isImpulse());
            expect (! arg.isBool());

            expect (arg.getBlob() == blob);
        }

        beginTest ("Colour");
        {
            Random rng = getRandom();

            for (int i = 100; --i >= 0;)
            {
                OSCColour col = { (uint8) rng.nextInt (256),
                                  (uint8) rng.nextInt (256),
                                  (uint8) rng.nextInt (256),
                                  (uint8) rng.nextInt (256) };

                OSCArgument arg (col);

                expect (arg.getType() == OSCTypes::colour);
                expect (! arg.isInt32());
                expect (! arg.isInt64());
                expect (! arg.isFloat32());
                expect (! arg.isDouble());
                expect (! arg.isString());
                expect (! arg.isBlob());
                expect (arg.isColour());
                expect (! arg.isNil());
                expect (! arg.isImpulse());
                expect (! arg.isBool());

                expect (arg.getColour().toInt32() == col.toInt32());
            }
        }

        beginTest ("Nil");
		{
			OSCArgument arg (OSCTypes::nil);

			expect (arg.getType() == OSCTypes::nil);
            expect (! arg.isInt32());
            expect (! arg.isInt64());
            expect (! arg.isFloat32());
            expect (! arg.isDouble());
            expect (! arg.isString());
            expect (! arg.isBlob());
            expect (! arg.isColour());
            expect (arg.isNil());
            expect (! arg.isImpulse());
            expect (! arg.isBool());
		}

        beginTest("Impulse");
        {
            OSCArgument arg(OSCTypes::impulse);

			expect(arg.getType() == OSCTypes::impulse);
            expect (! arg.isInt32());
            expect (! arg.isInt64());
            expect (! arg.isFloat32());
            expect (! arg.isDouble());
            expect (! arg.isString());
            expect (! arg.isBlob());
            expect (! arg.isColour());
            expect (! arg.isNil());
            expect (arg.isImpulse());
            expect (! arg.isBool());
		}

        beginTest("True");
        {
            OSCArgument arg(OSCTypes::T);
            expect (! arg.isInt32());
            expect (! arg.isInt64());
            expect (! arg.isFloat32());
            expect (! arg.isDouble());
            expect (! arg.isString());
            expect (! arg.isBlob());
            expect (! arg.isColour());
            expect (! arg.isNil());
            expect (! arg.isImpulse());
            expect (arg.isBool());
        }

        beginTest("False");
		{
			OSCArgument arg(OSCTypes::F);
			expect (! arg.isInt32());
			expect (! arg.isInt64());
			expect (! arg.isFloat32());
			expect (! arg.isDouble());
			expect (! arg.isString());
			expect (! arg.isBlob());
			expect (! arg.isColour());
			expect (! arg.isNil());
			expect (! arg.isImpulse());
			expect (arg.isBool());
		}


        beginTest ("Copy, move and assignment");
        {
            {
                int value = -42;
                OSCArgument arg (value);

                OSCArgument copy = arg;
                expect (copy.getType() == OSCTypes::int32);
                expect (copy.getInt32() == value);

                OSCArgument assignment ("this will be overwritten!");
                assignment = copy;
                expect (assignment.getType() == OSCTypes::int32);
                expect (assignment.getInt32() == value);
           }
           {
                const size_t numBytes = 412;
                MemoryBlock blob = getMemoryBlockWithRandomData (numBytes);
                OSCArgument arg (blob);

                OSCArgument copy = arg;
                expect (copy.getType() == OSCTypes::blob);
                expect (copy.getBlob() == blob);

                OSCArgument assignment ("this will be overwritten!");
                assignment = copy;
                expect (assignment.getType() == OSCTypes::blob);
                expect (assignment.getBlob() == blob);
           }
        }
    }
};

static OSCArgumentTests OSCArgumentUnitTests;

#endif

} // namespace juce
