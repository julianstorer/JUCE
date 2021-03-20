/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2017 - ROLI Ltd.

   JUCE is an open source library subject to commercial or open-source
   licensing.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

//==============================================================================
/**
    Represents a Network Interface.

    @tags{Core}
*/
class JUCE_API  NetworkInterface  final
{
public:

	NetworkInterface() noexcept;
	NetworkInterface(StringRef device, StringRef friendly, int interfaceIndex) noexcept;
	~NetworkInterface();

	/** Creates a copy of another interface. */
	NetworkInterface(const NetworkInterface&) noexcept;

	/** Creates a copy of another interface. */
	NetworkInterface& operator= (const NetworkInterface&) noexcept;

	bool operator== (const NetworkInterface&) const noexcept;
	bool operator!= (const NetworkInterface&) const noexcept;


	String toString() const;

	/** Retrieves the "friendly" name of the network-interface.  Only on Windows the friendly-name differs from the device-name */
	String getFriendlyName() const;

	/** Retrieves the technical name of the network interface */
	String getDeviceName() const;

	/** Retrieves the EUI-48 address for this interface.  If none is available, this will return a "null"-MAC */
	MACAddress getMACAddress() const;

	/** Retrieves the index of the interface as being used by Bonjour for example. If less than 1 no index could be retrieved */
	int getInterfaceIndex();

	/** Retrieves the maximum transmission unit in bytes this interface supports */
	int getMtuSize();

	/** Retrieves the operating-state of this interfaces */
	bool isUp();

	/** Retrieves the receive-speed this interface is working with.  -1 if this information could not be retrieved */
	int64 getRxSpeed();

	/** Retrieves the transmit-speed this interface is working with.  -1 if this information could not be retrieved */
	int64 getTxSpeed();

	/** Retrieve the IPDAdresses used on this interface 

		@param includeIPv6    if this is specified as true, both IPv4 & IPv6 addresses will be returned
	*/
	Array<IPAddress> getIPAddresses(bool includeIPv6 = false) const;

	// setters
	void setMACAddress(MACAddress &mac);
	void addIPAddress(IPAddress &addr);
	void addIPAddresses(Array<IPAddress> &addrs);
	void setMtuSize( int mtu );
	void setInterfaceUp( bool isUp );
	void setRxSpeed(int64 bps);
	void setTxSpeed(int64 bps);

    //==============================================================================
    /** Populates a list of all the Network interface that this machine is using. */
    static void findAllInterfaces (Array<NetworkInterface>& results );

    /** Populates a list of all the Network interaces that this machine is using. */
    static Array<NetworkInterface> getAllInterfaces ();

private:
	String deviceName;
	String friendlyName;
	Array<IPAddress> allIpAddresses;
	Array<IPAddress> ipv4Addresses;
	MACAddress macAddress;
	int index;
	int64 rxSpeed;
	int64 txSpeed;
	int mtuSize;
	bool interfaceUp;
};

} // namespace juce
